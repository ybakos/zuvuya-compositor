#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

static const int COMPOSITOR_VERSION = 4;
struct wl_compositor *compositor;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s, id %d, version %d\n", interface, name, version);
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface, COMPOSITOR_VERSION);
  }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
    uint32_t name) {
  printf("Got a registry.remove event for id %d\n", name);
}

static const struct wl_registry_listener registry_listener = {
  .global = registry_handle_global,
  .global_remove = registry_handle_global_remove
};

int main(int argc, char** argv) {
  struct wl_display* display = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  if (compositor == NULL) {
    fprintf(stderr, "no wl_compositor obtained\n");
    return EXIT_FAILURE;
  }

  wl_compositor_destroy(compositor);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  return 0;
}
