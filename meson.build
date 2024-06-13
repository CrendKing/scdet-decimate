project('scd_decimate', 'cpp',
    version: '1.0',
    default_options: ['cpp_std=c++23'],
)

sources = files(
    'src' / 'vs_filter.cpp',
)

vapoursynth_dep = dependency('vapoursynth', version: '>= 56')

shared_library(
    'scd_decimate',
    sources,
    dependencies: vapoursynth_dep,
    cpp_pch: 'pch/pch.h',
)
