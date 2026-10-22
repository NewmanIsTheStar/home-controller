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
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables



int picofs_read(int fd, char *ptr, int len)
{
    int i;

    //picofs_printf("read start offset = %d\n", custom_fds[fd].data_offset);

    for(i=0; i<len; i++)
    {       
        if ((custom_fds[fd].data_offset + i) < custom_fds[fd].data_len)
        {
            ptr[i] = custom_fds[fd].data[custom_fds[fd].data_offset + i];
        }
        else
        {
            //picofs_printf("truncated read @ offset = %d i = %d\n", custom_fds[fd].data_offset, i);
            break;
        }
    }
    
    //hex_dump(ptr, i);
    
    custom_fds[fd].data_offset += i;

    //picofs_printf("new offset = %d\n", custom_fds[fd].data_offset);

    return(i);
}

