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
#include "basic.h"

extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_basic_kill(void)
{
    SM_GENERIC_FILE_T msg;


    msg.message_id = SHELL_MSG_BASIC_KILL;

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);


    return(0);
}

/*!
 * \brief kill the running basic interpreter
 *
 * \return nothing
 */
int shell_do_basic_kill(SM_GENERIC_FILE_T *msg)
{
    basic_Kill();

    return(0);
}
