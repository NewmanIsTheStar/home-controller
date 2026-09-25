
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

// #include "weather.h"
// #include "flash.h"
// #include "calendar.h"
// #include "utility.h"
// #include "config.h"
#include "watchdog.h"
#include "pluto.h"


int http_parse_haeader(char *buffer, int buflen, int *filelen, char **filestart);
int image_compare(char *ptr1, char *ptr2, int len);

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
        // (re)establish socket connection
        if (web_socket < 0) web_socket = establish_socket("psycho.badnet", 80, SOCK_STREAM);
        
        if(web_socket >= 0)
        {
            // create request
            snprintf(buffer, sizeof(buffer), HTTPC_REQ_11_HOST_FORMAT("/pluto.bin", "fileserver.psycho"));

            // send a request
            wrote_bytes = send(web_socket, buffer, strlen(buffer), 0);

            if (wrote_bytes > 0) 
            {
                printf("read software file\n");                    
                total_expected = 0;
                for (retry=0; retry<5; retry++)
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
                            // reset retry counter
                            retry = 0;

                            //hex_dump(buffer, read_bytes);
                            
                            // attempt to find http header -- only works if header is completely contained in a buffer
                            if (!total_expected && !http_parse_haeader(buffer, read_bytes, &file_len, &file_start))
                            {
                                file_offset = (int)(file_start - buffer);  // offset from start of received byte stream to file start
                                total_expected = file_offset + file_len;

                                // compare bytes of file in the header with flash
                                compare = image_compare((char *)XIP_BASE, file_start, read_bytes - file_offset);
                                total_bad_compares += compare;                                

                                //printf("MEM COMPARE beginning at file byte %d returned %d\n", read_bytes - file_offset, compare);
                            }
                            else
                            {
                                compare = image_compare((char *)XIP_BASE + total_read - file_offset, buffer, read_bytes);
                                
                                if((total_read - file_offset + read_bytes) < 1024*1024)
                                {
                                    total_bad_compares += compare;
                                }

                                //printf("MEM COMPARE beginning at file byte %d returned %d\n", total_read - file_offset, compare);                             
                            }


                            // accumulate total bytes received
                            total_read += read_bytes;

                            //printf("TOTAL_READ = %d TOTAL_EXPECTED = %d\n", total_read, total_expected);
                            if (total_expected && (total_read >= total_expected)) break;
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
                printf("wrote_bytes = %d\n", wrote_bytes);

                // close socket
                lwip_close(web_socket);
                web_socket = -1;
                err = -1;
            }
        }

        printf("TOTAL_READ = %d TOTAL_EXPECTED = %d TOTAL_BAD_COMPARES %d\n", total_read, total_expected, total_bad_compares);        

        SLEEP_MS(60000);

        watchdog_pulse((int *)params);
    }

}



int http_parse_haeader(char *buffer, int buflen, int *filelen, char **filestart)
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
            printf("HTTP VERSION = %s\n", http_version);

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
                printf("HTTP STATUS = %s\n", http_status);

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
                            printf("CONTENT_LEN_CHARARCTER = %02x\n", content[i]);
                        }
                        printf("LOOP TERMINATED with i = %d content[i] = %02x\n", i, content[i]);
                        content_len[i] = 0;
                        printf("HTTP CONTENT LENGTH = %s\n", content_len);

                        *filelen = atoi(content_len);
                        *filestart = eoh+strlen("\r\n\r\n");

                        printf("FILE portion in buffer with header: ");
                        for (i=0; i<*filelen && i<buflen; i++)
                        {
                            if (i%16 == 0) printf("\n");
                            printf("%02x ", (*filestart)[i]);                            
                        }
                        printf("\nEND FILE portion in buffer with header\n");

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

    // for (i=0; i< len; i++)
    // {
    //     if (ptr1[i] != ptr2[i])
    //     {
    //         if ((ptr1[i] != 0xFF) && (ptr2[i]!= 0x00))
    //         {
    //             num_different_bytes++;
    //             printf("@%08x %02x vs %02x\n", ptr1 - (char *)XIP_BASE+i, ptr1[i], ptr2[i]);                
    //         }

    //     }
    // }

    return(num_different_bytes);
}