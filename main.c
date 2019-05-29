#include <wlr/util/log.h>
#include <wayland-server.h>

int main(int argc, char** argv) {
  wlr_log_init(WLR_DEBUG, NULL);
  struct wl_display* display = wl_display_create();

  wl_display_destroy_clients(display);
  wl_display_destroy(display);
  wlr_log(WLR_DEBUG, "Exiting...");
  return 0;
}
