project('zuvuya-compositor', 'c')

wlroots_project = subproject('wlroots')
lib_wlroots = wlroots_project.get_variable('wlroots')

# Required by wlroots.
add_project_arguments([
  '-DWLR_USE_UNSTABLE',
], language: 'c')

# We'll include wayland-server.h, which includes some deprecated stuff that we
# should ignore.
add_project_arguments('-DWL_HIDE_DEPRECATED', language: 'c')

executable('main',
  'main.c',
  dependencies: [lib_wlroots]
)
