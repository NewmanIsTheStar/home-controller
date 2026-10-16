/*
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
#include "swload_task.h"

extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_download(char *url)
{
    SM_GENERIC_URL_T msg;

    msg.message_id = SHELL_MSG_DOWNLOAD_FILE;
    STRNCPY(msg.url, url, sizeof(msg.url));

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing url
 * \return nothing
 */
int shell_do_download(SM_GENERIC_URL_T *msg)
{
    int err = -1;    

    if (!msg || !(msg->url[0]))
    {
        // error no filename
        return(-1);
    }

    if (strstr(msg->url, ".uf2"))
    {
        if (err = download_file_uf2(msg->url))
        {
            shell_printf("wget: failed to download %s\n", msg->url);
        }
    }
    else
    {
        if (err = download_file(msg->url))
        {
            shell_printf("wget: failed to download %s\n", msg->url);
        }
    }
    
    return(err);
}

int shell_queue_verify(char *url)
{
    SM_GENERIC_URL_T msg;

    msg.message_id = SHELL_MSG_VERIFY_FILE;
    STRNCPY(msg.url, url, sizeof(msg.url));

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing url
 * \return nothing
 */
int shell_do_verify(SM_GENERIC_URL_T *msg)
{
    int err = -1;    

    if (!msg || !(msg->url[0]))
    {
        // error no filename
        return(-1);
    }

    if (msg->url[0])
    {
        //TODO: add support for UF2 file verification
        if (err = verify_file(msg->url))
        {
            shell_printf("wget: failed to download %s\n", msg->url);
        }
    }
    else
    {
        shell_printf("wget: error: no URL provided\n");
    }

    return(err);
}