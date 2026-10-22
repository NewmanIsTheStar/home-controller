/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include "picofs.h"


//prototypes


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables

 

int picofs_unlink(const char *name, uint8_t fid) 
{
    int err = -1;
    int i;
    int fd;

    fd = picofs_find_available_fd();   // NB we are bypassing the wrappers so have to manage file descriptors directly in this function
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    if (picofs_open_file(fd, name, O_WRONLY | O_TRUNC, fid, true, 0))   
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;

    custom_fds[fd].file_status |= STS_DELETED;  // mark for deletion
    custom_fds[fd].data_len = 0;                // empty file
 
    picofs_close_file(fd, true);
    
    picofs_release_fd(fd);

    return 0;
}
