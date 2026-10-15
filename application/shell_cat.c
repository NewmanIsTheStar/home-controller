/**
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"

int shell_queue_cat(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_CAT_FILE;
    STRNCPY(msg.filename, filename, sizeof(msg.filename));

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_cat(SM_GENERIC_FILE_T *msg)
{
    FILE *filePointer;
    char buffer[128];

    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        return(-1);
    }

    // open the file in read mode ("r")
    filePointer = fopen(msg->filename, "r");

    // check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf("cat: %s: No such file\n", msg->filename);
        return 1; 
    }

    // read and print the file line-by-line
    while (fgets(buffer, sizeof(buffer), filePointer) != NULL)
    {
        shell_printf("%s", buffer); 
    }

    // close the file
    fclose(filePointer);

    return(0);
}
