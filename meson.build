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

lib_wayland = dependency('wayland-client')
# For shm
rt = meson.get_compiler('c').find_library('rt')

cmd = run_command('pkg-config', 'wayland-protocols', '--variable=pkgdatadir')
if (cmd.returncode() != 0)
  message('pkg-config could not find a path to wayland-scanner')
  error(cmd.stderr())
endif
XDG_SHELL_PROTOCOL_XML_PATH = cmd.stdout().strip() + '/stable/xdg-shell/xdg-shell.xml'

cmd = run_command('pkg-config', '--variable=wayland_scanner', 'wayland-scanner')
if (cmd.returncode() != 0)
  message('pkg-config could not find a path to wayland-scanner')
  error(cmd.stderr())
endif
WAYLAND_SCANNER = cmd.stdout().strip()

cmd = run_command(WAYLAND_SCANNER,
  'client-header',
  XDG_SHELL_PROTOCOL_XML_PATH,
  'xdg-shell-client-protocol.h'
)
if (cmd.returncode() != 0)
  message('Wayland scanner could not generate the xdg shell header file.')
  error(cmd.stderr())
endif

cmd = run_command(WAYLAND_SCANNER,
  'server-header',
  XDG_SHELL_PROTOCOL_XML_PATH,
  'xdg-shell-protocol.h'
)
if (cmd.returncode() != 0)
  message('Wayland scanner could not generate the xdg shell header file.')
  error(cmd.stderr())
endif

cmd = run_command(WAYLAND_SCANNER, 'private-code', XDG_SHELL_PROTOCOL_XML_PATH, 'xdg-shell-protocol.c')
if (cmd.returncode() != 0)
  message('Wayland scanner could not generate the xdg shell source file.')
  error(cmd.stderr())
endif

executable('main',
  ['main.c', 'xdg-shell-protocol.c'],
  dependencies: [lib_wlroots]
)

executable('client',
  ['client.c', 'shm.c', 'xdg-shell-client-protocol.c'],
  dependencies: [lib_wayland, rt]
)
