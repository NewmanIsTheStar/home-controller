/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdbool.h>
#include <fcntl.h>
#include "picofs.h"



//prototypes


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];

//static variables



/*!
 * \brief resize file that is open for write
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_ftruncate(int fd, off_t length)
{
    int err = -1;
    size_t cache_size = 0;
    char *new_cache = NULL;

    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS) && (custom_fds[fd].flags & (O_WRONLY | O_RDWR)))
    {
        // abort if no change requested
        if (length == custom_fds[fd].data_len)
        {
            return(0);
        }

         err = picofs_cache_realloc(fd, length, 0x00);   // 0x00 is technically correct, OxFF avoids unnecessary erase cycles when resizing a file

        if (!err)
        {        
            // ensure length remains within the allocated cache 
            CLIP(length, 0, custom_fds[fd].cache_len);

            // finalize the data length
            custom_fds[fd].data_len = length;        
        }
    }

    return(err);
}


