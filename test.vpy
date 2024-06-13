import vapoursynth
vapoursynth.core.std.LoadPlugin(r'build\debug\scd_decimate.dll')

output_clip = video_in.scdd.SCDDecimate(0.0005, 0.15, 2)
output_clip.text.FrameProps().set_output()
