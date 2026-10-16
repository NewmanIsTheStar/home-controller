/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #define _GNU_SOURCE 

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"
#include "pluto.h"

extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_boot(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_BOOT;
    STRNCPY(msg.filename, filename, sizeof(msg.filename));

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_boot(SM_GENERIC_FILE_T *msg)
{
    int err = -1;
    int exe_fd = -1;
    char *map;    

    if (msg->filename[0])
    {
        // open the file with for read
        exe_fd = open(msg->filename, O_RDONLY, 0644);
        if (exe_fd == -1) 
        {
            perror("shell_do_boot: Error opening/creating file");
            return EXIT_FAILURE;
        }

        // map the file so that tasks can access it directly as memory (rather than using file i/o)
        map = picofs_mmap(NULL, 0, PROT_READ, MAP_SHARED, exe_fd, 0);
        if (map == MAP_FAILED) 
        {
            perror("shell_do_boot: Error mapping the file");
            close(exe_fd);
            exe_fd = -1;
            return EXIT_FAILURE;
        }

        picofs_munmap(map, 0);

        close(exe_fd);
        exe_fd = -1;

        // for now we assume executable was linked to be loaded at the standard location (XIP_BASE)
        pluto_boot_setup((u_int32_t)map - FS_BASE, (char *)XIP_BASE);

        shell_printf("boot file set to: %s @ %p\n", msg->filename, map);
    }
    else
    {
        shell_printf("boot: error: no file provided\n");
    }



    return(err);
}