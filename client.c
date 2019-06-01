#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <shm.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

static const int WIDTH = 300;
static const int HEIGHT = 300;
static const int MEMORY_SIZE = 4 * WIDTH * HEIGHT;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s id %d version %d\n", interface, name, version);
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
  int fd = create_shm_file(MEMORY_SIZE);
  if (fd < 0) {
    fprintf(stderr, "creating a buffer file of %d B failed: %m\n", MEMORY_SIZE);
    return EXIT_FAILURE;
  }
  void* canvas = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (canvas == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %m\n");
    close(fd);
    return EXIT_FAILURE;
  }
  uint32_t *pixel = canvas;
  for (int i = 0; i < WIDTH * HEIGHT; ++i) {
    *pixel++ = 0x6600ff;
  }

  struct wl_display* display = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  close(fd);
  return EXIT_SUCCESS;
}
