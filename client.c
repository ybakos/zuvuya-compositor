#include <stdio.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s id %d\n", interface, name);
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

  wl_display_disconnect(display);
  return 0;
}
