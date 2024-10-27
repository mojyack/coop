project('coop', 'cpp', version : '0.0.1', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])
add_project_arguments('-Wfatal-errors', language: 'cpp')
add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')

executable('example', ['example/main.cpp'], include_directories : [include_directories('include')])
