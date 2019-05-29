#include <wayland-server.h>
#include <wlr/util/log.h>
#include <wlr/backend.h>

int main(int argc, char** argv) {
  wlr_log_init(WLR_DEBUG, NULL);
  struct wl_display* display = wl_display_create();
  struct wlr_backend* backend = wlr_backend_autocreate(display, NULL);

  wlr_backend_start(backend);
  wl_display_run(display);

  wlr_backend_destroy(backend);
  wl_display_destroy_clients(display);
  wl_display_destroy(display);
  wlr_log(WLR_DEBUG, "Exiting...");
  return 0;
}
