#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <shm.h>

static const int WIDTH = 300;
static const int HEIGHT = 300;

static const int WL_SHM_INTERFACE_VERSION = 1;
struct wl_shm *shm;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s, id %d, version %d\n", interface, name, version);
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, WL_SHM_INTERFACE_VERSION);
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

int stride() {
  return WIDTH * 4;
}

int shm_size() {
  return stride() * HEIGHT;
}

int main(int argc, char** argv) {
  struct wl_display* display = wl_display_connect(NULL);
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  int fd = create_shm_file(shm_size());
  void * shm_data = mmap(NULL, shm_size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  uint32_t *pixel = shm_data;
  for (int i = 0; i < WIDTH * HEIGHT; ++i) {
    *pixel++ = 0x6600ff;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, shm_size());
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT,
    stride(), WL_SHM_FORMAT_ARGB8888);

  wl_shm_pool_destroy(pool);
  wl_buffer_destroy(buffer);
  wl_display_disconnect(display);
  return 0;
}
