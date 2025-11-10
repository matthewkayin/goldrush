#include "core/logger.h"

int main() {
    logger_init();

    log_error("uh oh");
    log_warn("oops");
    log_info("hey");
    log_debug("okay");

    logger_quit();

    return 0;
}