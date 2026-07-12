/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vox/vox_accel.h"

#include <stdio.h>

typedef struct vox_hash_vector {
    const char *bytes;
    vox_u32 length;
    vox_u32 expected;
} vox_hash_vector;

static int check_known_vectors(void)
{
    static const vox_hash_vector vectors[] = {
        {"", 0U, 0x811c9dc5U},
        {"a", 1U, 0xe40c292cU},
        {"foobar", 6U, 0xbf9cf968U},
        {"hello", 5U, 0x4f9f2cabU}
    };
    vox_u32 i;

    for (i = 0U; i < (vox_u32)(sizeof(vectors) / sizeof(vectors[0])); ++i) {
        vox_u32 scalar;
        vox_u32 selected;

        scalar = vox_fnv1a32_scalar(vectors[i].bytes, vectors[i].length);
        selected = vox_fnv1a32(vectors[i].bytes, vectors[i].length);
        if (scalar != vectors[i].expected || selected != scalar) {
            fprintf(stderr,
                    "FNV-1a vector %lu failed: expected %08lx, scalar %08lx, selected %08lx\n",
                    (unsigned long)i,
                    (unsigned long)vectors[i].expected,
                    (unsigned long)scalar,
                    (unsigned long)selected);
            return 0;
        }
    }
    return 1;
}

static int check_deterministic_parity(void)
{
    vox_u8 bytes[1024];
    vox_u32 state;
    vox_u32 i;
    vox_u32 length;

    state = 0x5eed1234U;
    for (i = 0U; i < (vox_u32)sizeof(bytes); ++i) {
        state = state * 1664525U + 1013904223U;
        bytes[i] = (vox_u8)(state >> 24);
    }

    for (length = 0U; length <= (vox_u32)sizeof(bytes); ++length) {
        vox_u32 scalar;
        vox_u32 selected;

        scalar = vox_fnv1a32_scalar(bytes, length);
        selected = vox_fnv1a32(bytes, length);
        if (selected != scalar) {
            fprintf(stderr,
                    "FNV-1a parity failed at length %lu: scalar %08lx, selected %08lx\n",
                    (unsigned long)length,
                    (unsigned long)scalar,
                    (unsigned long)selected);
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    if (!check_known_vectors() || !check_deterministic_parity()) {
        return 1;
    }
    if (vox_fnv1a32_scalar(0, 1U) != 0U ||
        vox_fnv1a32(0, 1U) != 0U) {
        fprintf(stderr, "FNV-1a invalid-input contract failed\n");
        return 1;
    }

    printf("VOX acceleration contract passed (%s backend)\n",
           vox_accel_nasm_enabled() ? "NASM x86-64" : "C89 scalar");
    return 0;
}
