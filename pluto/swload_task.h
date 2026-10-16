/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SWLOAD_TASK_H
#define SWLOAD_TASK_H

void swload_task(__unused void *params);
int download_file_uf2(char *url);
int download_file(char *url);
int verify_file(char *url);

#endif
