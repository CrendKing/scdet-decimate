struct Context {
    VSNode *clipWithStats;
    double lowThreshold;
    double highThreshold;
    int detectionRange;
    int cumulativeSkips;
};

auto VS_CC filterGetFrame(int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) -> const VSFrame * {
    auto *ctx = static_cast<Context *>(instanceData);

    if (activationReason == arInitial) {
        for (int i = 0; i < ctx->detectionRange; ++i) {
            vsapi->requestFrameFilter(n + ctx->cumulativeSkips + i, ctx->clipWithStats, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrame *frame = nullptr;
        VSMap *props = nullptr;
        double statsDiff = 0;
        int skippedDurationNum = 0;

        for (int skips = 0; skips < ctx->detectionRange; ++skips) {
            frame = vsapi->getFrameFilter(n + ctx->cumulativeSkips + skips, ctx->clipWithStats, frameCtx);
            props = vsapi->getFramePropertiesRW(const_cast<VSFrame *>(frame));
            statsDiff = vsapi->mapGetFloat(props, "PlaneStatsDiff", 0, nullptr);
            const int currDurationNum = vsapi->mapGetInt(props, "_DurationNum", 0, nullptr);

            if (statsDiff < ctx->lowThreshold) {
                skippedDurationNum += currDurationNum;
                vsapi->freeFrame(frame);
            } else {
                if (skips > 0) {
                    vsapi->mapSetInt(props, "_DurationNum", currDurationNum + skippedDurationNum, maReplace);
                    ctx->cumulativeSkips += skips;
                }

                break;
            }
        }

        vsapi->mapSetInt(props, "_SceneChangeNext", static_cast<int>(statsDiff >= ctx->highThreshold), maReplace);
        return frame;
    }

    return nullptr;
}

auto VS_CC filterFree(void *instanceData, VSCore *core, const VSAPI *vsapi) -> void {
    const auto *ctx = static_cast<Context *>(instanceData);
    vsapi->freeNode(ctx->clipWithStats);
    delete ctx;
}

auto VS_CC filterCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) -> void {
    auto ctx = new Context;

    int propGetError = peSuccess;
    ctx->highThreshold = std::clamp(vsapi->mapGetFloat(in, "high", 0, &propGetError), std::numeric_limits<double>::epsilon(), 1.0);
    ctx->lowThreshold = std::clamp(vsapi->mapGetFloat(in, "low", 0, &propGetError), 0.0, ctx->highThreshold);
    ctx->detectionRange = std::clamp(static_cast<int>(vsapi->mapGetInt(in, "range", 0, &propGetError)), 0, 5) + 1;
    ctx->cumulativeSkips = 0;

    VSPlugin *stdPlugin = vsapi->getPluginByID(VSH_STD_PLUGIN_ID, core);
    VSNode *inputClip = vsapi->mapGetNode(in, "clip", 0, &propGetError);

    VSMap *trimParams = vsapi->createMap();
    vsapi->mapSetNode(trimParams, "clip", inputClip, maReplace);
    vsapi->mapSetInt(trimParams, "first", 1, maReplace);
    VSMap *trimOutputMap = vsapi->invoke(stdPlugin, "Trim", trimParams);
    VSNode *trimOutputNode = vsapi->mapGetNode(trimOutputMap, "clip", 0, nullptr);

    VSMap *planeStatsParams = vsapi->createMap();
    vsapi->mapSetNode(planeStatsParams, "clipa", inputClip, maReplace);
    vsapi->mapSetNode(planeStatsParams, "clipb", trimOutputNode, maReplace);
    VSMap *planeStatsOutputMap = vsapi->invoke(stdPlugin, "PlaneStats", planeStatsParams);
    ctx->clipWithStats = vsapi->mapGetNode(planeStatsOutputMap, "clip", 0, nullptr);

    vsapi->freeNode(inputClip);
    vsapi->freeMap(trimParams);
    vsapi->freeMap(trimOutputMap);
    vsapi->freeMap(planeStatsParams);
    vsapi->freeMap(planeStatsOutputMap);

    vsapi->createVideoFilter(out, "SCDDecimate", vsapi->getVideoInfo(inputClip), filterGetFrame, filterFree, fmFrameState, nullptr, 0, ctx, core);
}

VS_EXTERNAL_API(auto) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) -> void {
    vspapi->configPlugin("com.crendking.vs_filters", "scdd", "Scene Change Detection + Frame Deduplication", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("SCDDecimate", "clip:vnode;low:float:opt;high:float;range:int:opt", "clip:vnode;", filterCreate, nullptr, plugin);
}
