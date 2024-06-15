import vapoursynth
vapoursynth.core.std.LoadPlugin(r'build\debug\scdet_decimate.dll')

clip_with_stats = video_in.std.PlaneStats(clipb=video_in.std.Trim(first=1))
scdd_clip = clip_with_stats.scdd.SCDetDecimate(high=0.15, low=0.0005)
scdd_clip.text.FrameProps().set_output()
