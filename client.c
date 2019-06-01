#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <shm.h>

static const int WIDTH = 300;
static const int HEIGHT = 300;
static const int MEMORY_SIZE = 4 * WIDTH * HEIGHT;

int main(int argc, char** argv) {
  int fd = create_shm_file(MEMORY_SIZE);
  void* canvas = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  uint32_t *pixel = canvas;
  for (int i = 0; i < WIDTH * HEIGHT; ++i) {
    *pixel++ = 0x6600ff;
  }
  return EXIT_SUCCESS;
}
