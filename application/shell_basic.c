/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"
#include "basic.h"

extern MessageBufferHandle_t xMessageBuffer;
// Application RAM Storage variables

extern char ascii_ram_buffer[];
extern size_t current_buffer_index;
extern char *basic_program;
extern char basic_program_buffer[];


int shell_queue_basic_file(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_BASIC_FILE;
    STRNCPY(msg.filename, filename, sizeof(msg.filename));
    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief execute a basic script from a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_basic_file(SM_GENERIC_FILE_T *msg)
{
    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        return(-1);
    }

    basic_Interpreter(IM_MMAP_FILE, NULL, msg->filename, NULL, 0, false); 
    
    return(0);
}
                   
int shell_queue_basic_script(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_BASIC_SCRIPT;
    STRNCPY(msg.filename, filename, sizeof(msg.filename));
    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief execute a basic script from RAM buffer
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_basic_script(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        return(-1);
    }

    basic_program[current_buffer_index++] = 0;
    basic_Interpreter(IM_EXECUTE_IN_PASSED_RAM_BUFFER, NULL, NULL, basic_program, current_buffer_index, false); 
    
    return(0);
}

int shell_queue_basic_interactive(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_BASIC_FILE;
    STRNCPY(msg.filename, filename, sizeof(msg.filename));
    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief execute a basic script from a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_basic_interactive(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        return(-1);
    }

    basic_Interpreter(IM_INTERACTIVE, NULL, NULL, basic_program_buffer, strlen(basic_program_buffer), false);
    
    return(0);
}