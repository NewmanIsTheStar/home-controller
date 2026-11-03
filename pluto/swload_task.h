/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SWLOAD_TASK_H
#define SWLOAD_TASK_H

typedef enum
{
    DOWNLOAD_SAVE_BIN,
    DOWNLOAD_SAVE_UF2,
    DOWNLOAD_VERIFY_BIN,
    DOWNLOAD_VERIFY_UF2
} DOWNLOAD_MODE_T;

void swload_task(__unused void *params);
int download_file_uf2(char *url);
int download_file(char *url, DOWNLOAD_MODE_T mode);
int verify_file(char *url);
int verify_file_uf2(char *url);
int image_compare(char *ptr1, char *ptr2, int len);

#endif
