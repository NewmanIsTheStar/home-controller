/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include <limits.h>


// configuration conversion info
typedef struct
{
    int version;
    size_t size;
    size_t version_offset;
    size_t crc_offset;
    void (*upgrade_function)(void *previous_config);
} CONFIG_CONVERSION_T;


// prototypes
int config_read(void);
int config_write(void);
void config_changed(void);
bool config_dirty(bool clear_flag);
int config_mmap(char *filename, void **configuration_buffer, size_t config_size);
int config_sync_file(void *configuration_buffer, int configuration_len);
void *config_get_flash_location(char *filename);
bool config_compare_flash_ram(char *filename, void *configuration_buffer, bool stop_at_first_difference, bool print_differences);
int config_validate(char *filename, CONFIG_CONVERSION_T conversion_table[], int conversion_table_rows, void **configuration_buffer);
int config_get_conversion_row(CONFIG_CONVERSION_T conversion_table[], int conversion_table_rows, void *configuration_buffer);

#endif