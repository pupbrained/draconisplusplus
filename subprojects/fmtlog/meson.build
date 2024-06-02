project('fmtlog', 'cpp', version: '2.2.2')

incdir = include_directories(
  'include',
  is_system: true # Ignores warnings from include dir
)

libdir = meson.current_source_dir()
fmtlog_dep = declare_dependency(
    include_directories: incdir,
    link_args: [libdir + '/libfmtlog-static.a']
)

install_headers('include/fmtlog.h', subdir: 'fmtlog')

