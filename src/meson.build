main_sources = [
  'reddit.c',
  'curl_wrappers.c',
  'memory.c',
  'rofi_reddit.c',
  'history.c',
]

pluginsdir = rofi_dependency.get_variable('pluginsdir')

rofi_reddit_shared_lib = shared_module(
  'reddit',
  main_sources,
  dependencies: deps,
  include_directories: extra_inc,
  install_dir: pluginsdir,
  name_prefix: '',
  install: install,
  link_args: link_args,
)
