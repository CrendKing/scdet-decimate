struct Context {
    const VSAPI *vsapi;

    VSNode *clip;
    double highThreshold;
    double lowThreshold;
    int backBufferSize;

    std::deque<const VSFrame *> frameQueue;
    int queueBase;

    int processedFrameNo;
    int maxGetFrameNo;

    int frameDurationNumSkipped;

    std::thread requestFramesThread;
    std::mutex mutex;
    std::condition_variable producerCv;
    std::condition_variable consumerCv;
    bool isDraining;

    // FILE *logFile;
};

auto VS_CC filterGetFrame(int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) -> const VSFrame * {
    auto *ctx = static_cast<Context *>(instanceData);

    {
        const std::unique_lock<std::mutex> lock(ctx->mutex);
        ctx->maxGetFrameNo = std::max(ctx->maxGetFrameNo, n);
    }
    ctx->producerCv.notify_one();

    std::unique_lock<std::mutex> lock(ctx->mutex);

    while (n >= ctx->queueBase + static_cast<int>(ctx->frameQueue.size()) && !ctx->isDraining) {
        ctx->consumerCv.wait(lock);
    }

    const VSFrame *frame = ctx->isDraining ? ctx->frameQueue.back() : ctx->frameQueue[n - ctx->queueBase];
    vsapi->addFrameRef(frame);
    return frame;
}

auto VS_CC filterFree(void *instanceData, VSCore *core, const VSAPI *vsapi) -> void {
    auto *ctx = static_cast<Context *>(instanceData);

    {
        const std::unique_lock<std::mutex> lock(ctx->mutex);
        ctx->isDraining = true;
    }
    ctx->producerCv.notify_one();
    // filterFree() is only called after all filterGetFrame() has finished, so no need to notify consumer cv
    ctx->requestFramesThread.join();

    for (auto &frame : ctx->frameQueue) {
        vsapi->freeFrame(frame);
    }
    vsapi->freeNode(ctx->clip);

    delete ctx;
}

auto VS_CC frameDone(void *instanceData, const VSFrame *frame, int n, VSNode *node, const char *errorMsg) -> void {
    auto *ctx = static_cast<Context *>(instanceData);

    if (errorMsg != nullptr) {
        {
            const std::unique_lock<std::mutex> lock(ctx->mutex);
            ctx->isDraining = true;
        }
        ctx->producerCv.notify_one();
        ctx->consumerCv.notify_all();

        return;
    }

    VSMap *props = ctx->vsapi->getFramePropertiesRW(const_cast<VSFrame *>(frame));
    const double statsDiff = ctx->vsapi->mapGetFloat(props, "PlaneStatsDiff", 0, nullptr);
    const int frameDurationNum = static_cast<int>(ctx->vsapi->mapGetInt(props, "_DurationNum", 0, nullptr));

    if (statsDiff < ctx->lowThreshold) {
        ctx->frameDurationNumSkipped += frameDurationNum;
        ctx->vsapi->freeFrame(frame);
    } else {
        ctx->vsapi->mapSetInt(props, "_SceneChangeNext", static_cast<int>(statsDiff >= ctx->highThreshold), maReplace);

        if (ctx->frameDurationNumSkipped > 0) {
            ctx->vsapi->mapSetInt(props, "_DurationNum", frameDurationNum + ctx->frameDurationNumSkipped, maReplace);
            ctx->frameDurationNumSkipped = 0;
        }

        {
            const std::unique_lock<std::mutex> lock(ctx->mutex);

            ctx->frameQueue.emplace_back(frame);

            const int numFramesToRemove = std::max(ctx->maxGetFrameNo - ctx->backBufferSize - ctx->queueBase, 0);
            for (int i = 0; i < numFramesToRemove; ++i) {
                ctx->vsapi->freeFrame(ctx->frameQueue.front());
                ctx->frameQueue.pop_front();
            }
            ctx->queueBase += numFramesToRemove;
        }
        ctx->consumerCv.notify_all();
    }

    {
        const std::unique_lock<std::mutex> lock(ctx->mutex);
        ctx->processedFrameNo = n;
    }
    ctx->producerCv.notify_one();
}

auto requestFrames(Context *ctx) -> void {
    int requestFrameNo = 0;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(ctx->mutex);

            while (ctx->maxGetFrameNo < ctx->queueBase + static_cast<int>(ctx->frameQueue.size()) || ctx->processedFrameNo < requestFrameNo - 1) {
                if (ctx->isDraining) {
                    return;
                }

                ctx->producerCv.wait(lock);
            }
        }

        ctx->vsapi->getFrameAsync(requestFrameNo, ctx->clip, frameDone, ctx);
        requestFrameNo += 1;
    }
}

auto VS_CC filterCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) -> void {
    auto *ctx = new Context;
    ctx->vsapi = vsapi;

    int propGetError = peSuccess;
    ctx->clip = vsapi->mapGetNode(in, "clip", 0, &propGetError);
    ctx->highThreshold = std::clamp(vsapi->mapGetFloat(in, "high", 0, &propGetError), std::numeric_limits<double>::epsilon(), 1.0);
    ctx->lowThreshold = std::clamp(vsapi->mapGetFloat(in, "low", 0, &propGetError), 0.0, ctx->highThreshold);
    ctx->backBufferSize = std::max(static_cast<int>(vsapi->mapGetInt(in, "buffer", 0, &propGetError)), 30);

    ctx->queueBase = 0;

    ctx->processedFrameNo = -1;
    ctx->maxGetFrameNo = -1;

    ctx->frameDurationNumSkipped = 0;

    ctx->isDraining = false;

    // ctx->logFile = _fsopen("Log.log", "w", _SH_DENYNO);

    const std::array deps = {
        VSFilterDependency { .source = ctx->clip, .requestPattern = rpNoFrameReuse }
    };
    vsapi->createVideoFilter(out, "SCDetDecimate", vsapi->getVideoInfo(ctx->clip), filterGetFrame, filterFree, fmUnordered, deps.data(), 1, ctx, core);

    ctx->requestFramesThread = std::thread(requestFrames, ctx);
}

VS_EXTERNAL_API(auto) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) -> void {
    vspapi->configPlugin("com.crendking.vs_filters", "scdd", "Scene Change Detection + Frame Deduplication", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("SCDetDecimate", "clip:vnode;high:float;low:float:opt;buffer:int:opt", "clip:vnode;", filterCreate, nullptr, plugin);
}
