/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include "utility.h"
#include "watchdog.h"
#include "pluto.h"
#include "picofs.h"
#include "shell.h"
#include "pluto_swload.h"
#include "system_web.h"

/*WARNING
    In this file we have a combination of newlib stanard file operations and lwip socket operations.  LWIP uses a macro for close so that it may be used for network sockets.
    This macro breaks any standard calls to close() intended to close a file.  
    
    To avoid this conflict all calls to close for file operations are placed in parentheses to prevent macro substituion like this: (close)(fd);
*/


int http_parse_header(char *buffer, int buflen, int *filelen, char **filestart);
int parse_url(const char *url, char *host, size_t host_size, char *uri, size_t uri_size);
int pluto_write_uf2(int fd, char *ptr, int len);
int pluto_compare_uf2(char *file_data, char *server_data, int len);
int pluto_download_process_chunk(int dst_fd, char *dst_address, char *ptr, int len, int *num_differences, DOWNLOAD_MODE_T mode);
int pluto_initialize_uf2_buffer(void);

#define HTTPC_CLIENT_AGENT "MonkeyBuntBob"

/* GET request basic */
#define HTTPC_REQ_11 "GET %s HTTP/1.1\r\n" /* URI */\
    "User-Agent: %s\r\n" /* User-Agent */ \
    "Accept: */*\r\n" \
    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
    "\r\n"
#define HTTPC_REQ_11_FORMAT(uri) HTTPC_REQ_11, uri, HTTPC_CLIENT_AGENT

/* GET request with host */
#define HTTPC_REQ_11_HOST "GET %s HTTP/1.1\r\n" /* URI */\
    "User-Agent: %s\r\n" /* User-Agent */ \
    "Accept: */*\r\n" \
    "Host: %s\r\n" /* server name */ \
    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
    "\r\n"
#define HTTPC_REQ_11_HOST_FORMAT(uri, srv_name) HTTPC_REQ_11_HOST, uri, HTTPC_CLIENT_AGENT, srv_name

/* GET request with proxy */
#define HTTPC_REQ_11_PROXY "GET http://%s%s HTTP/1.1\r\n" /* HOST, URI */\
    "User-Agent: %s\r\n" /* User-Agent */ \
    "Accept: */*\r\n" \
    "Host: %s\r\n" /* server name */ \
    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
    "\r\n"
#define HTTPC_REQ_11_PROXY_FORMAT(host, uri, srv_name) HTTPC_REQ_11_PROXY, host, uri, HTTPC_CLIENT_AGENT, srv_name

/* GET request with proxy (non-default server port) */
#define HTTPC_REQ_11_PROXY_PORT "GET http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */\
    "User-Agent: %s\r\n" /* User-Agent */ \
    "Accept: */*\r\n" \
    "Host: %s\r\n" /* server name */ \
    "Connection: Close\r\n" /* we don't support persistent connections, yet */ \
    "\r\n"
#define HTTPC_REQ_11_PROXY_PORT_FORMAT(host, host_port, uri, srv_name) HTTPC_REQ_11_PROXY_PORT, host, host_port, uri, HTTPC_CLIENT_AGENT, srv_name

// extern variables
extern SYS_WEB_VARIABLES_T sys_web;

// static variables
static int download_position = 0;
static char uf2_block[512];
static int dst_index = 0;
static int uf2_blocks_processed = 0;
static int uf2_blocks_written = 0;

/*!
 * \brief Monitor weather and control relay based on conditions and time of day
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void swload_task(__unused void *params)
{
    int err = 0;
    int ret;
    int wrote_bytes;
    int read_bytes;
    static int msg_to_snd;
    int retry;
    fd_set readset;
    struct timeval tv;  
    char buffer[1600];
    static int web_socket = -1;    
    int file_len;
    char *file_start;
    int total_read = 0;
    int total_expected = 0;
    int compare;
    int file_offset = 0;
    int total_bad_compares = 0;


    for(;;)
    {
        // // (re)establish socket connection
        // if (web_socket < 0) web_socket = establish_socket("psycho.badnet", 80, SOCK_STREAM);
        
        // if(web_socket >= 0)
        // {
        //     // create request
        //     snprintf(buffer, sizeof(buffer), HTTPC_REQ_11_HOST_FORMAT("/wombat", "fileserver.psycho"));

        //     // send a request
        //     wrote_bytes = send(web_socket, buffer, strlen(buffer), 0);

        //     if (wrote_bytes > 0) 
        //     {
        //         printf("read software file\n");                    
        //         total_expected = 0;
        //         for (retry=0; retry<5; retry++)
        //         {
        //             FD_ZERO(&readset);
        //             FD_SET(web_socket, &readset);
        //             tv.tv_sec = 5;
        //             tv.tv_usec = 500;

        //             ret = select(web_socket + 1, &readset, NULL, NULL, &tv);

        //             if ((ret > 0) && FD_ISSET(web_socket, &readset))
        //             {
        //                 read_bytes = recv(web_socket, buffer, sizeof(buffer), 0);
        //                 if (read_bytes > 0)
        //                 {
        //                     // reset retry counter
        //                     retry = 0;

        //                     //hex_dump(buffer, read_bytes);
                            
        //                     // attempt to find http header -- only works if header is completely contained in a buffer
        //                     if (!total_expected && !http_parse_header(buffer, read_bytes, &file_len, &file_start))
        //                     {
        //                         file_offset = (int)(file_start - buffer);  // offset from start of received byte stream to file start
        //                         total_expected = file_offset + file_len;

                               
        //                         // compare bytes of file in the header with flash
        //                         // compare = image_compare((char *)XIP_BASE, file_start, read_bytes - file_offset);
        //                         // total_bad_compares += compare;                                

        //                         //printf("MEM COMPARE beginning at file byte %d returned %d\n", read_bytes - file_offset, compare);
        //                     }
        //                     else
        //                     {
        //                         compare = image_compare((char *)XIP_BASE + total_read - file_offset, buffer, read_bytes);
                                
        //                         if((total_read - file_offset + read_bytes) < 1024*1024)
        //                         {
        //                             total_bad_compares += compare;
        //                         }

        //                         //printf("MEM COMPARE beginning at file byte %d returned %d\n", total_read - file_offset, compare);                             
        //                     }


        //                     // accumulate total bytes received
        //                     total_read += read_bytes;

        //                     //printf("TOTAL_READ = %d TOTAL_EXPECTED = %d\n", total_read, total_expected);
        //                     if (total_expected && (total_read >= total_expected)) break;
        //                 }
        //                 else {
        //                     perror("READ ERROR = ");
        //                     printf("read returned %d  ||| retry = %d\n", read_bytes, retry);
        //                     err = -1;
        //                 }
        //             }
        //             else
        //             {
        //                 printf("select returned %d  and FD_ISSET was not set  ||| retry = %d\n", ret, retry);
        //                 err = -1;
        //             }  

        //         }         
        //     }
        //     else
        //     {
        //         printf("wrote_bytes = %d\n", wrote_bytes);

        //         // close socket
        //         lwip_close(web_socket);
        //         web_socket = -1;
        //         err = -1;
        //     }
        // }

        // printf("TOTAL_READ = %d TOTAL_EXPECTED = %d TOTAL_BAD_COMPARES %d\n", total_read, total_expected, total_bad_compares);        

        SLEEP_MS(60000);

        watchdog_pulse((int *)params);
    }

}



int http_parse_header(char *buffer, int buflen, int *filelen, char **filestart)
{
    char *eol = NULL;
    char *eoh = NULL;
    char *status = NULL;
    char *content = NULL;
    char http_version[4];
    char http_status[5];
    char content_len[16];
    int i;
    int err = -1;


    // find end of line
    eol = strnstr(buffer, "\r\n", buflen);
    if (eol != NULL)
    {
        if ((strncmp(buffer, "HTTP/", 5) == 0))
        {
            STRNCPY(http_version, buffer+5, 4);
            //printf("HTTP VERSION = %s\n", http_version);

            // status appears after first space
            status = strnstr(buffer, " ", buflen);
            if (status != NULL)
            {
                status++;

                //copy status
                for (i=0; i<3 && status[i] != ' '; i++)
                {
                    http_status[i] = status[i];
                }
                http_status[i] = 0;
                //printf("HTTP STATUS = %s\n", http_status);

                // find end of header
                eoh = strnstr(buffer, "\r\n\r\n", buflen);
                if (eoh != NULL)
                {
                    content = strnstr(buffer, "Content-Length: ", buflen);
                    if (content != NULL)
                    {
                        content+=strlen("Content-Length: ");
                        
                        //copy content length
                        for (i=0; i<15 && content[i] != ' ' && content[i] != '\r' && content[i] != '\n'; i++)
                        {
                            content_len[i] = content[i];
                            //printf("CONTENT_LEN_CHARARCTER = %02x\n", content[i]);
                        }
                        //printf("LOOP TERMINATED with i = %d content[i] = %02x\n", i, content[i]);
                        content_len[i] = 0;
                        //printf("HTTP CONTENT LENGTH = %s\n", content_len);

                        *filelen = atoi(content_len);
                        *filestart = eoh+strlen("\r\n\r\n");

                        // printf("FILE portion in buffer with header: ");
                        // for (i=0; i<*filelen && i<buflen; i++)
                        // {
                        //     if (i%16 == 0) printf("\n");
                        //     printf("%02x ", (*filestart)[i]);                            
                        // }
                        // printf("\nEND FILE portion in buffer with header\n");

                        err = 0;
                    }
                }
            } 
        }        
    }

    return(err);
}

int image_compare(char *ptr1, char *ptr2, int len)
{
    int i;
    int num_different_bytes = 0;

    for (i=0; i< len; i++)
    {
        if (ptr1[i] != ptr2[i])
        {
            //if ((ptr1[i] != 0xFF) && (ptr2[i]!= 0x00))
            {
                num_different_bytes++;
                printf("@%08x %02x vs %02x\n", ptr1+i, ptr1[i], ptr2[i]);                
            }

        }
    }

    // if (!num_different_bytes)
    // {
    //     printf("%08x to %08x OK\n", ptr1, ptr1+i-1);
    // }

    return(num_different_bytes);
}

/*!
 * \brief download file from server, large files are assumed executable and placed at 64K boundary for address translation
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
int download_file(char *url, DOWNLOAD_MODE_T mode)
{
    int err = 0;
    int file_error = 0;
    int ret;
    int wrote_bytes;
    int read_bytes;
    static int msg_to_snd;
    int retry;
    fd_set readset;
    struct timeval tv;  
    char buffer[1600];
    int web_socket = -1;    
    int server_file_len = 0;
    int local_file_len = 0;
    char *file_start;
    int total_read = 0;
    int total_expected = 0;
    int compare;
    int file_offset = 0;
    int total_bad_compares = 0;
    char host[256];
    char uri[256];
    char filename[16];
    int fd = -1;
    int i = 0;
    int j = 0;
    int total_processed = 0;
    char *mode_string = NULL;
    bool verify_only = false;
    char *flash_data = NULL;
    int differences_found = 0;

    switch(mode)
    {
    case DOWNLOAD_SAVE_BIN:
        mode_string = "save::binary";
        break;
    case DOWNLOAD_SAVE_UF2:
        mode_string = "save::uf2";
        break;
    case DOWNLOAD_VERIFY_BIN:
        mode_string = "verify::binary";
        verify_only = true;
        break;
    case DOWNLOAD_VERIFY_UF2:
        mode_string = "verify::uf2";
        verify_only = true;
        break;
    default:
        mode_string = "unknown mode";
        break;    
    }
    
    if (parse_url(url, host, sizeof(host), uri, sizeof(uri)) == 0) 
    {
        // generate filename from uri
        for(i=0; i < sizeof(uri); i++)
        {
            if (isalpha(uri[i]) || isdigit(uri[i]) || (uri[i] == '.') || uri[i] == 0)
            {
                filename[j++] = uri[i];
                if ((j >= 16) || (uri[i] == 0)) 
                {
                    // force zero termination
                    filename[15] = 0;
                    break;
                }
            }
        }

        shell_printf("Host:               %s\n", host);
        shell_printf("URI:                %s\n", uri);
        shell_printf("Filename:           %s\n", filename);
        shell_printf("Mode:               %s\n", mode_string);                      
    } 
    else 
    {
        shell_printf("download_file: URL parsing failed.\n");
        return (-1);
    }
    


    // establish socket connection
    if (web_socket < 0) web_socket = establish_socket(host, 80, SOCK_STREAM);
    
    if(web_socket >= 0)
    {
        // create request
        snprintf(buffer, sizeof(buffer), HTTPC_REQ_11_HOST_FORMAT(uri, host));

        //hex_dump(buffer, strlen(buffer));

        // send a request
        wrote_bytes = send(web_socket, buffer, strlen(buffer), 0);

        if (wrote_bytes == strlen(buffer))    //TODO: handle short write by sending rest of the buffer
        {                 
            total_expected = 0;
            download_position = 0;
            file_error = 0;

            // read file from server with 5 retries until the end of file or a fatal error occurs
            for (retry=0; (retry<5) && !file_error; retry++)
            {
                FD_ZERO(&readset);
                FD_SET(web_socket, &readset);
                tv.tv_sec = 5;
                tv.tv_usec = 500;

                ret = select(web_socket + 1, &readset, NULL, NULL, &tv);

                if ((ret > 0) && FD_ISSET(web_socket, &readset))
                {
                    read_bytes = recv(web_socket, buffer, sizeof(buffer), 0);
                    if (read_bytes > 0)
                    {
                        //hex_dump(buffer, read_bytes);

                        // reset retry counter
                        retry = 0;
                        
                        // attempt to find http header -- only works if header is completely contained in a buffer
                        if (!total_expected && !http_parse_header(buffer, read_bytes, &server_file_len, &file_start))
                        {
                            shell_printf("Server file size:   %d\n", server_file_len);
                                       
                            if (fd < 0)
                            {
                                // open file
                                if (verify_only)
                                {
                                    shell_printf("Downloading file ...\n"); 
                                    fd = open(filename, O_RDONLY);
                                }
                                else
                                {
                                    if (mode == DOWNLOAD_SAVE_UF2)
                                    {
                                        // uf2 files have roughly 50% overhead
                                        local_file_len = (server_file_len/2) + FS_PAGE_SIZE;
                                        shell_printf("Local file size:    %d\n", local_file_len);
                                        pluto_initialize_uf2_buffer(); 
                                    }
                                    else
                                    {
                                        local_file_len = server_file_len;
                                    }

                                    shell_printf("Downloading file ...\n");   
                                    fd = picofs_open_download_file(filename, local_file_len);
                                }
                                
                                // check if the file exists and opened successfully
                                if (fd < 0) 
                                {
                                    shell_printf("download_file: failed to open local file %s\n", filename);
                                    lwip_close(web_socket);
                                    web_socket = -1;                                    
                                    return 1; 
                                }

                                // mmap file for direct memory access when verifying file
                                if (verify_only)
                                {
                                    flash_data = picofs_mmap(NULL, server_file_len, PROT_READ, MAP_SHARED, fd, 0);
                                    if (flash_data == MAP_FAILED) 
                                    {
                                        perror("download_file: Error mapping the file for direct memory access\n");
                                        (close)(fd);
                                        fd = -1;
                                        lwip_close(web_socket);
                                        web_socket = -1;                                        
                                        return EXIT_FAILURE;
                                    }
                                }                                
                            }

                            file_offset = (int)(file_start - buffer);  // offset from start of received byte stream to file start
                            total_expected = file_offset + server_file_len;
                            
                            if (((read_bytes - file_offset) + total_processed) < total_expected)
                            {
                                // write all data after the http header to file
                                file_error = pluto_download_process_chunk(fd, flash_data, buffer + file_offset, (read_bytes - file_offset), &differences_found, mode);  
                                
                                // printf("First DATA\n");
                                // hex_dump(buffer + file_offset, (read_bytes - file_offset));

                                total_processed += (read_bytes - file_offset);
                            }
                            else if (total_processed < total_expected)
                            {
                                // truncate at total_expected -- remainder of buffer is http
                                file_error = pluto_download_process_chunk(fd, flash_data, buffer + file_offset, total_expected - total_processed, &differences_found, mode);
                                total_processed += (total_expected - total_processed);
                            }                            
                        }
                        else if (server_file_len)
                        {
                            if ((read_bytes + total_processed) < total_expected)
                            {
                                // write all data to file
                                file_error = pluto_download_process_chunk(fd, flash_data, buffer, read_bytes, &differences_found, mode);    
                                total_processed += read_bytes;
                            }
                            else if (total_processed < total_expected)
                            {
                                // truncate at total_expected -- remainder of buffer is http
                                file_error = pluto_download_process_chunk(fd, flash_data, buffer, total_expected - total_processed, &differences_found, mode);
                                total_processed += (total_expected - total_processed);
                            }
                        }
                        else
                        {
                            shell_printf("ABORT: missed header\n");
                            err = -7;
                        }
                        
                        //printf("total processed = %d\n", total_processed);

                        // accumulate total bytes received
                        total_read += read_bytes;

                        //printf("TOTAL_READ = %d TOTAL_EXPECTED = %d\n", total_read, total_expected);
                        if (total_expected && (total_read >= total_expected))
                        {
                            shell_printf("Download complete\n");
                            break;
                        } 
                    }
                    else {
                        perror("READ ERROR = ");
                        printf("read returned %d  ||| retry = %d\n", read_bytes, retry);
                        err = -1;
                    }
                }
                else
                {
                    printf("select returned %d  and FD_ISSET was not set  ||| retry = %d\n", ret, retry);
                    err = -1;
                }  

            }         
        }
        else
        {
            shell_printf("download_file: error failed to send HTTP GET :: bytes sent = %d [expected %d]\n", wrote_bytes, strlen(buffer));
            
            //printf("wrote_bytes = %d\n", wrote_bytes);

            // close socket
            lwip_close(web_socket);
            web_socket = -1;
            err = -1;
        }
    }

    //printf("TOTAL_READ = %d TOTAL_EXPECTED = %d\n", total_read, total_expected);        

    if (web_socket >= 0)
    {
        lwip_close(web_socket);
        web_socket = -1;
    } 

    if (verify_only)
    {
        // close and unmap file
        (close)(fd);

        if (flash_data)
        {
            picofs_munmap(flash_data, server_file_len);
        }

        // print verification result
        if (err || differences_found || (total_read != total_expected))
        {
            shell_printf("verification failed\n");
            shell_printf("differences found: %d\n", differences_found);
        }
        else
        {
            shell_printf("verification succeeded: local file matches remote file\n");
        }
    }
    else
    {
        if (file_error)
        {
            shell_printf("downlaod aborted : file error\n");
        }        
        // close the file
        picofs_close(fd);       // directly call picofs_close() bypassing the wrapper since we used picofs_open_download_file() rather than a standard open()
        picofs_release_fd(fd);  // as we are bypassing the wrapper me must release the file descriptor

        //shell_printf("UF2: blocks processed = %d blocked written = %d\n", uf2_blocks_processed, uf2_blocks_written);
    }

    return(err);
}



int parse_url(const char *url, char *host, size_t host_size, char *uri, size_t uri_size) 
{
    const char *p = url;
    
    // Skip protocol (http:// or https://) if present
    const char *scheme_end = strstr(url, "://");
    if (scheme_end != NULL) {
        p = scheme_end + 3;
    }
    
    // Find the first '/' after the scheme which marks the start of the URI/path
    const char *path_start = strchr(p, '/');
    
    if (path_start != NULL) {
        // Copy hostname part
        size_t h_len = (size_t)(path_start - p);
        if (h_len >= host_size) return -1; // Buffer too small
        strncpy(host, p, h_len);
        host[h_len] = '\0';
        
        // Copy URI part (including the leading '/')
        if (strlen(path_start) >= uri_size) return -1; // Buffer too small
        strncpy(uri, path_start, uri_size);
    } else {
        // No path found, entire remaining string is the host, URI is "/"
        if (strlen(p) >= host_size) return -1;
        strncpy(host, p, host_size);
        
        if (uri_size > 1) {
            strcpy(uri, "/");
        } else {
            return -1;
        }
    }
    return 0;
}

int pluto_initialize_uf2_buffer(void)
{
    dst_index = 0;
    memset(uf2_block, FS_ERASED_CELL_VALUE, 512);
    uf2_blocks_processed = 0;
    uf2_blocks_written = 0;

    return(0);
}

int pluto_write_uf2(int fd, char *ptr, int len)
{
    int err = 0;
    int src_index = 0;

    while (src_index < len)
    {
        // copy as much data as possible into the block buffer
        for( ; (src_index < len) && (dst_index < 512); dst_index++)
        {
            uf2_block[dst_index] = ptr[src_index++];
        }

        // check if we have received an entire block
        if (dst_index >= 512)
        {
            err = pluto_write_uf2_block(fd, (const uf2_block_t *)uf2_block);
            
            if (!err)
            {
                uf2_blocks_written++;
            }

            uf2_blocks_processed++;

            // clear block buffer
            dst_index = 0;
            memset(uf2_block, FS_ERASED_CELL_VALUE, 512);
        }
    }

    return(err);
}


int pluto_compare_uf2(char *file_data, char *server_data, int len)
{
    int err = 0;
    int num_bytes = 0;
    static char uf2_block[512];
    static int dst_index = 0;
    int src_index = 0;

    while (src_index < len)
    {
        // copy as much data as possible into the block buffer
        for( ; (src_index < len) && (dst_index < 512); dst_index++)
        {
            uf2_block[dst_index] = server_data[src_index++];
        }

        // check if we have received an entire block
        if (dst_index >= 512)
        {
            num_bytes = pluto_compare_uf2_block(file_data, (const uf2_block_t *)uf2_block);

            if (num_bytes)
            {
                err = -1;
            }
                        
            // clear block buffer
            dst_index = 0;
            memset(uf2_block, FS_ERASED_CELL_VALUE, 512);
        }
    }

    return(num_bytes);
}


/*!
 * \brief process a chunk of downloaded file
 *
 * \param dst_fd destination file descriptor
 * \param dst_address destination address
 * \param ptr received chunk of file
 * \param len length of chunk
 * \param num_differences number of byte differences found
 * \param mode type of processing to perform: write binary, write uf2, compare binary, compare uf2
 * 
 * \return nothing
 */
int pluto_download_process_chunk(int dst_fd, char *dst_address, char *ptr, int len, int *num_differences, DOWNLOAD_MODE_T mode)
{
    int err = 0;
    int num_bytes = 0;

    switch(mode)
    {
    default:
        break;
    case DOWNLOAD_SAVE_BIN:
        num_bytes = picofs_write(dst_fd, ptr, len);

        if (num_bytes < 0)
        {
            err = num_bytes;
        }
        break;
    case DOWNLOAD_SAVE_UF2:
        err = pluto_write_uf2(dst_fd, ptr, len);    
        break;
    case DOWNLOAD_VERIFY_BIN:
        num_bytes = image_compare((dst_address+download_position), ptr, len);
        download_position += len;

        if (num_bytes)
        {
            err = -1;
            *num_differences += num_bytes;
        }
        break;
    case DOWNLOAD_VERIFY_UF2:
        num_bytes = pluto_compare_uf2(dst_address, ptr, len);

        if (num_bytes)
        {
            err = -1;
            *num_differences += num_bytes;
        }        
        break;
    }

    // progress indicator
    //shell_printf("."); 

    return(err);
}


int pluto_download_software(void)
{
    int err = -1;
    DOWNLOAD_MODE_T mode;

    if (sys_web.software_download_now && sys_web.software_url[0])
    {
        sys_web.software_download_now = 0;

        urldecode(sys_web.software_url, sys_web.software_url);

        if (strstr(sys_web.software_url, ".uf2"))
        {
            mode = DOWNLOAD_SAVE_UF2;
        }
        else
        {
            mode = DOWNLOAD_SAVE_BIN;
        }

        if (err = download_file(sys_web.software_url, mode))
        {
            printf("pluto_download_software: failed to download %s\n", sys_web.software_url);
        }
        else
        {
            pluto_application_restart(REBOOT_SOFTWARE_UPGRADE);
        }
    }

    return(err);
}