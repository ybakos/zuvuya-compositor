#include <wayland-client.h>
#include <wayland-client-protocol.h>

int main(int argc, char** argv) {
  struct wl_display* display = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display);


  wl_display_disconnect(display);
  return 0;
}
