project('coop', 'cpp', version : '0.0.1', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])
add_project_arguments('-Wfatal-errors', language: 'cpp')
add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')

deps = []
if host_machine.system() == 'windows'
  cpp = meson.get_compiler('cpp')
  deps += [
    cpp.find_library('ws2_32'),
  ]
endif

if get_option('examples')
  executable('example', ['example/main.cpp'], dependencies : deps, include_directories : [include_directories('include')])
  executable('pipe-test', ['example/pipe-test.cpp'], dependencies : deps, include_directories : [include_directories('include')])
endif

install_subdir('include/coop', install_dir : 'include')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  description: 'C++ asynchronous coroutine runtime',
  name : 'coop',
)
