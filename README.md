# SCDetDecimate

Scene Change Detection + Frame Deduplication

```
scdd.SCDetDecimate(vnode clip, float high, float low=0, buffer=30)
```

Requires the input clip to have the `PlaneStatsDiff` frame property, which is usually obtained with
```
input = clip.std.PlaneStats(clipb=clip.std.Trim(first=1))
```

Set the `_SceneChangeNext` property to 1 if `PlaneStatsDiff >= high`. 0 otherwise. Same behavior as [SCDetect](https://github.com/vapoursynth/vs-miscfilters-obsolete/blob/master/src/miscfilters.cpp).

If the frame's `PlaneStatsDiff < low`, omit the current input frame from the output clip, and add its duration to the next frame. Repeat this step until a unique frame is encountered. Equivalent to ffmpeg's [mpdecimate](https://ffmpeg.org/ffmpeg-filters.html#mpdecimate) filter.

A back buffer of frames is maintained to support random access from downstream.

This filter makes sure `high` is always in range of `(0, 1]`, `low` in range of `[0, high]`. Minimum `buffer` is 30. Out-of-range values are clamped.

### Examples

```
clip.scdd.SCDetDecimate(high=0.15, low=0.0005)
```
Remove frames less than 0.05% different their next frame, repeat at most twice. Mark frames more than 15% different to their next frame as scene change.

```
clip.scdd.SCDetDecimate(high=0.15)
```
Effectively disable frame removal due to `low=0`. Scene change detection still happens.
