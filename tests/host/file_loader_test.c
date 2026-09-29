#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "file_loader.h"

static void create_test_file(
    const char *path,
    size_t size
) {
    FILE *file =
        fopen(
            path,
            "wb"
        );

    assert(file != NULL);

    for (
        size_t i = 0;
        i < size;
        i++
    ) {
        const uint8_t value =
            (uint8_t)i;

        assert(
            fwrite(
                &value,
                1,
                1,
                file
            ) == 1
        );
    }

    fclose(file);
}

int main(void)
{
    assert(
        system(
            "mkdir -p build/host"
        ) == 0
    );

    static const char *valid_path =
        "build/host/test-rom.bin";

    static const char *short_path =
        "build/host/test-rom-short.bin";

    create_test_file(
        valid_path,
        4096
    );

    create_test_file(
        short_path,
        4095
    );

    uint8_t data[4096];

    assert(
        host_load_exact_file(
            valid_path,
            data,
            sizeof(data)
        )
    );

    assert(data[0] == 0x00);
    assert(data[1] == 0x01);
    assert(data[255] == 0xff);
    assert(data[256] == 0x00);
    assert(data[4095] == 0xff);

    assert(
        !host_load_exact_file(
            short_path,
            data,
            sizeof(data)
        )
    );

    assert(
        !host_load_exact_file(
            "build/host/does-not-exist.bin",
            data,
            sizeof(data)
        )
    );

    remove(valid_path);
    remove(short_path);

    puts("PASS host exact-size file loader");

    return 0;
}
