/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SWLOAD_TASK_H
#define SWLOAD_TASK_H

void swload_task(__unused void *params);
int download_file_uf2(char *url);
int download_file_bin(char *url);
int verify_file_bin(char *url);
int verify_file_uf2(char *url);
int image_compare(char *ptr1, char *ptr2, int len);

#endif
