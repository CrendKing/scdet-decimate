project('scdet_decimate', 'cpp',
    version: '1.0',
    default_options: ['cpp_std=c++23'],
)

sources = files(
    'src' / 'vs_filter.cpp',
)

incdir = include_directories(
    run_command(
        get_option('vapoursynth_dir') / '.venv/Scripts/python',
        '-m', 'vapoursynth', 'get-include',
        check: true,
    ).stdout().strip(),
)

shared_library(
    'scdet_decimate',
    sources,
    include_directories: incdir,
    cpp_pch: 'pch/pch.h',
)
