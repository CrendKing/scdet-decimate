# SCDDecimate

```
scdd.SCDDecimate(vnode clip, float low=0, float high, range)
```

Apply `std.PlaneStats()` on the input clip, then do the following based on the value of each frame's `PlaneStatsDiff` property:

1. If the frame's `PlaneStatsDiff < low`, remove the current frame from the clip, and add its duration to the next frame. Repeat this step for at most `range` times. If `range` is 0, this step doesn't occur.
2. Set the `_SceneChangeNext` property to 1 if `PlaneStatsDiff >= high`. 0 otherwise.

This filter makes sure `high` is always in range of `(0, 1]`, `low` in range of `[0, high]`, `range` in range of `[0, 5]`. Out-of-range values are clamped.

### Examples

```
clip.scdd.SCDDecimate(low=0.0005, high=0.15, range=2)
```
Remove frames less than 0.05% different their next frame, repeat at most twice. Mark frames more than 15% different to their next frame as scene change.

```
clip.scdd.SCDDecimate(low=0.05, high=0.15)
```
Disable frame removal due to `range=0`, regardless of the `low` value. Scene change detection still happens.
