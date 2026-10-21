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

int shell_queue_hexdump(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_CAT_FILE;
    
    if (filename)
    {
        STRNCPY(msg.filename, filename, sizeof(msg.filename));
    }
    else
    {
        msg.filename[0] = 0;
    }

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);
    

    return(0);
}


/*!
 * \brief print the contents of a binary file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_hexdump(SM_GENERIC_FILE_T *msg)
{
    FILE *filePointer;
    unsigned char buffer[16];
    size_t bytes_read = 0;
    char output_line[160];
    char output_byte[8];
    int i;

    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        return(-1);
    }

    printf("hexdump %s\n", msg->filename);

    // open the file in read binary mode ("r")
    filePointer = fopen(msg->filename, "rb");

    // check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf("hd: %s: No such file\n", msg->filename);
        return 1; 
    }

    // read and print the file 16 bytes at a time
    while (bytes_read = fread(buffer, 1, sizeof(buffer), filePointer))
    {
        if(bytes_read)
        {
            output_line[0] = 0;

            for (i = 0; i < bytes_read;i++) 
            {
                snprintf(output_byte, sizeof(output_byte), "%02x ", buffer[i]);
                STRAPPEND(output_line, output_byte);
            }            

             // add extra padding if less than 16 bytes (i.e. last line)
            for (; i<16; i++)
            {
                STRAPPEND(output_line, "   ");
            }

            STRAPPEND(output_line, " ");  
            for (i = 0; i < bytes_read; i++) 
            {
                if (isprint(buffer[i]))
                {
                   snprintf(output_byte, sizeof(output_byte), "%c", buffer[i]);
                   STRAPPEND(output_line, output_byte);
                }
                else
                {
                    snprintf(output_byte, sizeof(output_byte), "-");
                    STRAPPEND(output_line, output_byte);
                }
            }

            STRAPPEND(output_line, "\n");
            shell_printf("%s", output_line);                
        }        
    }

    // close the file
    fclose(filePointer);

    return(0);
}
