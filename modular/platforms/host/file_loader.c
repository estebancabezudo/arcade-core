#include "file_loader.h"

#include <stdio.h>

bool host_load_exact_file(
    const char *path,
    uint8_t *destination,
    size_t expected_size
) {
    if (
        path == NULL ||
        destination == NULL
    ) {
        return false;
    }

    FILE *file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return false;
    }

    if (
        fseek(
            file,
            0,
            SEEK_END
        ) != 0
    ) {
        fclose(file);
        return false;
    }

    const long file_size =
        ftell(file);

    if (
        file_size < 0 ||
        (size_t)file_size != expected_size
    ) {
        fclose(file);
        return false;
    }

    rewind(file);

    const size_t bytes_read =
        fread(
            destination,
            1,
            expected_size,
            file
        );

    const bool read_ok =
        bytes_read == expected_size &&
        ferror(file) == 0;

    fclose(file);

    return read_ok;
}
