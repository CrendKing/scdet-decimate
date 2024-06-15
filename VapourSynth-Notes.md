Due to how VapourSynth works, making this filter compatible with `fmParallel` mode is not simple.

VapourSynth internally maintains a thread pool. All `GetFrame` calls are actually "tasks" that pushed to a task queue, and get picked by active threads.

In `fmFrameState` and `fmUnordered` modes, VapourSynth only uses 1 thread to call the `GetFrame` function. In the `fmParallel*` modes, VapourSynth uses ALL the threads to concurrently call `GetFrame`. There is no reservation. If the filter's `GetFrame` callback is blocked for some reason (e.g. waiting for condition variable), that thread is not going back to the pool.

We use a custom thread to sequentially get frames from source clip, analyze each frame's properties and store frame information. If downstream requests a frame too far in the future, our `GetFrame` callback must be blocked until that frame is processed by the custom thread, because returning nullptr from `GetFrame` is fatal error.

What usually happens is, with the `fmParallel*` modes, VapourSynth spawns as many `GetFrame` requests as the thread pool allows, leaving no thread for our custom thread to call `GetFrame` on upstream. Since our custom thread can't get frame, it can't unblock the `GetFrame` callbacks. It effectively becomes dead locked.

VapourSynth [did warn](http://www.vapoursynth.com/doc/api/vapoursynth4.h.html#getframe) user to not call `GetFrame()` inside a `GetFrame` callback, because they consider filters by default in the `fmParallel` mode. What they didn't say is that user must not call `GetFrame()` anywhere inside a `fmParallel` filter, not just the callback. They also didn't tell user that we can call `GetFrame()` anywhere inside a non-`fmParallel*` filter, as long as the [thread count](http://www.vapoursynth.com/doc/api/vapoursynth4.h.html#setthreadcount) is greater than 1.

By the way, `getFrame()` is just a wrapper with the standard condvar-wait logic on top of `getFrameAsync()`. Both are vulnerable to the issue above.
