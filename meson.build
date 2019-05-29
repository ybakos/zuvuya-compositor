project('zuvuya-compositor', 'c')

wlroots_project = subproject('wlroots')
lib_wlroots = wlroots_project.get_variable('wlroots')

# Required by wlroots.
add_project_arguments([
  '-DWLR_USE_UNSTABLE',
], language: 'c')

executable('main',
  'main.c',
  dependencies: [lib_wlroots]
)
