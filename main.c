#include <wlr/util/log.h>

int main(int argc, char** argv) {
  wlr_log_init(WLR_DEBUG, NULL);

  wlr_log(WLR_DEBUG, "Exiting...");
  return 0;
}
