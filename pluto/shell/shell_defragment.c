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
extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_defragment(void)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_DEFRAGMENT;

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);


    return(0);
}

/*!
 * \brief cleanup and defragment the file system
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_defragment(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        return(-1);
    }

    picofs_consolidate_all_files_in_flash();
    
    return(0);
}
