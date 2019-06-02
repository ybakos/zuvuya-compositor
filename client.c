#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <shm.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <xdg-shell-client-protocol.h>

static const int WIDTH = 300;
static const int HEIGHT = 300;
static const int STRIDE = 4 * WIDTH;
static const int MEMORY_SIZE = STRIDE * HEIGHT;

static const int WL_SHM_INTERFACE_VERSION = 1;
struct wl_shm *shm;
static const int WL_COMPOSITOR_INTERFACE_VERSION = 4;
struct wl_compositor *compositor;
static const int XDG_WM_BASE_INTERFACE_VERSION = 2;
struct xdg_wm_base *xdg_wm_base;

static void registry_handle_global(void *data, struct wl_registry *registry,
    uint32_t name, const char *interface, uint32_t version) {
  printf("Got a registry.global event for %s id %d version %d\n", interface, name, version);
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = wl_registry_bind(registry, name, &wl_shm_interface, WL_SHM_INTERFACE_VERSION);
  } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
      WL_COMPOSITOR_INTERFACE_VERSION);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
      XDG_WM_BASE_INTERFACE_VERSION);
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
  if (display == NULL) {
    fprintf(stderr, "failed to create display\n");
    return EXIT_FAILURE;
  }

  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  if (shm == NULL) {
    fprintf(stderr, "no wl_shm support\n");
    return EXIT_FAILURE;
  }
  struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, MEMORY_SIZE);
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, WIDTH, HEIGHT,
    STRIDE, WL_SHM_FORMAT_ARGB8888);
  if (buffer == NULL) {
    fprintf(stderr, "could not obtain a buffer\n");
    return EXIT_FAILURE;
  }

  if (compositor == NULL) {
    fprintf(stderr, "no compositor support\n");
    return EXIT_FAILURE;
  }

  struct wl_surface* surface = wl_compositor_create_surface(compositor);
  wl_surface_attach(surface, buffer, 0, 0);

  wl_surface_destroy(surface);
  wl_compositor_destroy(compositor);
  wl_buffer_destroy(buffer);
  wl_shm_pool_destroy(pool);
  wl_shm_destroy(shm);
  wl_registry_destroy(registry);
  wl_display_disconnect(display);
  close(fd);
  return EXIT_SUCCESS;
}
