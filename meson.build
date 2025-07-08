project('WPEBackend-android', 'cpp',
  meson_version: '>=1.0',
  license: 'LGPL-2.1',
  default_options: [
    'b_ndebug=if-release',
    'cpp_std=c++11',
    'cpp_rtti=false',
    'cpp_eh=none',
  ],
)

cpp_compiler = meson.get_compiler('cpp')

wpebackend_android_lib = shared_module('WPEBackend-android',
  'src/android.cpp',
  'src/ipc.cpp',
  'src/renderer-backend-egl.cpp',
  'src/renderer-host.cpp',
  'src/view-backend.cpp',
  include_directories: ['include'],
  gnu_symbol_visibility: 'inlineshidden',
  cpp_args: ['-DG_LOG_DOMAIN="WPEBackend-Android"'],
  install_dir: get_option('libdir'),
  dependencies: [
    dependency('wpe-1.0'),
    dependency('gio-2.0'),
    cpp_compiler.find_library('EGL'),
    cpp_compiler.find_library('GLESv2'),
    cpp_compiler.find_library('android'),
    cpp_compiler.find_library('log'),
  ],
)

install_headers(
  'include/wpe-android/view-backend.h',
  subdir: 'wpe-android',
)
