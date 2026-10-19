/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"
#include "system_web.h"

extern SYS_WEB_VARIABLES_T sys_web; 
extern MessageBufferHandle_t xMessageBuffer;
extern const char *editor_text;

int shell_queue_save_text_file(void)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_SAVE_TEXT_FILE;

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}

/*!
 * \brief save text file that is currently in the ascii buffer
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_save_text_file(SM_GENERIC_FILE_T *msg)
{
    FILE *file_ptr = NULL;
    char filename[16];
    int automation_number = 0;
    char *automation_name;

    if (!msg)
    {
        return(-1);
    }
   
    // get the filename
    // ideally the web browser would pass the file name, but we just remember what it was when we last opened the page
    STRNCPY(filename, sys_web.edit_text_filename, sizeof(filename));  

    file_ptr = fopen(filename, "w");

    if (file_ptr == NULL) 
    {
        printf("ERROR opening file\n");
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    if (fputs((char *)editor_text, file_ptr) == EOF) 
    {
        printf("ERRO writing file\n");
        perror("Error writing to file");
        fclose(file_ptr);
        return EXIT_FAILURE;
    }

    fclose(file_ptr);
    

    return EXIT_SUCCESS;
}
