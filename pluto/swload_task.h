#ifndef SWLOAD_TASK_H
#define SWLOAD_TASK_H

void swload_task(__unused void *params);
int download_file(char *url);
int verify_file(char *url);

#endif
