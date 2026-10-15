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

int shell_queue_page_map(SHELL_MSG_T msg_id)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = msg_id;

    return(0);
}

/*!
 * \brief display a details of a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_page_map(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        return(-1);
    }

    switch(msg->message_id)
    {
    case SHELL_MSG_PAGE_MAP:
        picofs_find_page_status(PFS_DISPLAY_SHELL_PAGE_MAP);
        break;
    case SHELL_MSG_PAGE_NUMBERS:
        picofs_find_page_status(PFS_DISPLAY_SHELL_PAGE_NUMBERS);
        break;
    default:
        break;
    }
    
    return(0);
}
