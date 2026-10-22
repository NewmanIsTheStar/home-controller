/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fcntl.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "picofs.h"


//prototypes


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif
extern SemaphoreHandle_t picofs_mutex;

//static variables

 
/*!
 * \brief copy file 
 *
 * \param fd     file descriptor 
 * \return 0 on success
 */
int picofs_rename(const char *dst, const char *src)
{
    int err = -1;

    err = picofs_copy(dst, src);   // NB picofs_copy takes care of locking internally
    if (err)
    {
        return(err);
    }

    if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {  
        err = picofs_unlink_by_name(src);

        xSemaphoreGive(picofs_mutex);
    }    
    
    return(err);
}

