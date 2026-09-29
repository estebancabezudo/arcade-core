#ifndef ARCADE_CORE_HOST_FILE_LOADER_H
#define ARCADE_CORE_HOST_FILE_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Load a file whose size must match expected_size exactly.
 *
 * This belongs exclusively to the host platform layer.
 * Emulator devices, boards and machines must not depend on it.
 */
bool host_load_exact_file(
    const char *path,
    uint8_t *destination,
    size_t expected_size
);

#endif
