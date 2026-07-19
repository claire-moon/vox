/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "digs_miner_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIGS_MINER_ICON_EXPECTED_FNV 0xEE849C02U

static int digs_read_file(const char *path, vox_u8 **bytes, size_t *size)
{
    FILE *input;
    long length;
    vox_u8 *data;
    input = fopen(path, "rb");
    if (input == 0 || fseek(input, 0L, SEEK_END) != 0) {
        if (input != 0) fclose(input);
        return 0;
    }
    length = ftell(input);
    if (length <= 0L || fseek(input, 0L, SEEK_SET) != 0) {
        fclose(input);
        return 0;
    }
    data = (vox_u8 *)malloc((size_t)length + 1U);
    if (data == 0 || fread(data, 1U, (size_t)length, input) !=
        (size_t)length) {
        free(data);
        fclose(input);
        return 0;
    }
    data[length] = 0U;
    fclose(input);
    *bytes = data;
    *size = (size_t)length;
    return 1;
}

static vox_u32 digs_hash_bytes(const vox_u8 *bytes, size_t size)
{
    vox_u32 hash = 2166136261U;
    size_t index;
    for (index = 0U; index < size; ++index) {
        hash ^= (vox_u32)bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

int main(void)
{
    const char *first_path = "digs-miner-art-a.xpm";
    const char *second_path = "digs-miner-art-b.xpm";
    vox_u8 *first = 0;
    vox_u8 *second = 0;
    size_t first_size = 0U;
    size_t second_size = 0U;
    vox_u32 hash;
    int result = 1;
    if (digs_miner_write_icon_xpm(first_path) != VOX_OK ||
        digs_miner_write_icon_xpm(second_path) != VOX_OK ||
        !digs_read_file(first_path, &first, &first_size) ||
        !digs_read_file(second_path, &second, &second_size)) {
        fprintf(stderr, "could not generate canonical miner icon\n");
        goto cleanup;
    }
    if (first_size != second_size ||
        memcmp(first, second, first_size) != 0) {
        fprintf(stderr, "miner icon output is not deterministic\n");
        goto cleanup;
    }
    if (strstr((const char *)first, "\"256 256 ") == 0 ||
        strstr((const char *)first, "\".. c None\"") == 0 ||
        strstr((const char *)first, "#") == 0) {
        fprintf(stderr, "miner icon is not a transparent 256px XPM\n");
        goto cleanup;
    }
    hash = digs_hash_bytes(first, first_size);
    if (hash != DIGS_MINER_ICON_EXPECTED_FNV) {
        fprintf(stderr, "canonical miner icon hash changed: %08lx\n",
                (unsigned long)hash);
        goto cleanup;
    }
    printf("DIGS canonical miner icon bytes=%lu fnv=%08lx\n",
           (unsigned long)first_size, (unsigned long)hash);
    result = 0;

cleanup:
    free(second);
    free(first);
    (void)remove(second_path);
    (void)remove(first_path);
    return result;
}
