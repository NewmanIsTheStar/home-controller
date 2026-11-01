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
#include <hardware/flash.h>
#include "picofs.h"


extern MessageBufferHandle_t xMessageBuffer;

// prototype
int shell_hexdump_buffer(char *buffer, int len);



int shell_queue_hexdump(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_HEXDUMP_FILE;
    
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
 * \brief print the contents of a binary file OR raw flash memory address to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_hexdump(SM_GENERIC_FILE_T *msg)
{
    FILE *file;
    unsigned char buffer[16];
    size_t bytes_read = 0;
    char output_line[160];
    char output_byte[8];
    int i;
    int address = 0;

    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        return(-1);
    }

    printf("hexdump %s\n", msg->filename);

    // open the file in read binary mode ("r")
    file = fopen(msg->filename, "rb");

    // check if the file exists and opened successfully
    if (file == NULL) 
    {

        // check if valid address
        if (sscanf(msg->filename, "%x", &address) == 1)
        {
            if (((char *)address >= FLASH_SCAN_START) && ((char *)address < FLASH_SCAN_END))
            {
                shell_hexdump_buffer((char *)address, FS_PAGE_SIZE); 
            }

            return(0);
        }

        shell_printf("hd: %s: No such file\n", msg->filename);
        return 1; 
    }

    // read and print the file 16 bytes at a time
    while (bytes_read = fread(buffer, 1, sizeof(buffer), file))
    {
        shell_hexdump_buffer(buffer, bytes_read);       
    }

    // close the file
    fclose(file);

    return(0);
}

/*!
 * \brief print the hexadecimal contents of a line buffer
 *
 * \param[in]  buffer data
 * \param[in]  len    length 
 * \return nothing
 */
int shell_hexdump_buffer(char *buffer, int len)
{
    char output_line[160];
    char output_byte[8];
    int i, j;
    int bytes_on_line = 0;


    if(len)
    {
        for(j=0; j < len; j+=16)
        {
            output_line[0] = 0;
            bytes_on_line = (len-j)>=16?16:(len-j);

            for (i = 0; i < bytes_on_line;i++) 
            {
                snprintf(output_byte, sizeof(output_byte), "%02x ", buffer[j+i]);
                STRAPPEND(output_line, output_byte);
            }            

            // add extra padding if less than 16 bytes (i.e. last line)
            for (; i<16; i++)
            {
                STRAPPEND(output_line, "   ");
            }

            STRAPPEND(output_line, " ");  
            for (i = 0; i < bytes_on_line; i++) 
            {
                if (isprint(buffer[j+i]))
                {
                    snprintf(output_byte, sizeof(output_byte), "%c", buffer[j+i]);
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

    return(0);
}