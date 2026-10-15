/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
// #include "pico/cyw43_arch.h"
// #include "pico/stdlib.h"
// #include "pico/util/datetime.h"
//#include "hardware/rtc.h"
// #include "hardware/watchdog.h"
// #include <hardware/flash.h>

// #include "lwip/netif.h"
// #include "lwip/ip4_addr.h"
// #include "lwip/apps/lwiperf.h"
// #include "lwip/opt.h"
// #include "lwip/sockets.h"
#include "lwip/netdb.h"
// #include "lwip/sys.h"
// #include <lwip/dns.h>

// #include "FreeRTOS.h"
// #include "FreeRTOSConfig.h"
// #include "task.h"


// #include "stdarg.h"

//#include "weather.h"

// #include "calendar.h"
#include "utility.h"
// #include "config.h"
// #include "system_config.h"
#include "application_config.h"
// #include "watchdog.h"
#include "pluto.h"
#include "web.h"


//prototype

// external variables
extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;

//global


/*!
 * \brief print hex dump of buffer to string
 *
 * \return nothing
 */
void hex_dump_to_string(const uint8_t *bptr, uint32_t len, char *out_string, int out_len) {
    unsigned int i = 0;
    unsigned int num_chars = 0;

    for (i=0; i<len; i++)
    {
        sprintf(out_string+num_chars, "%02x ", bptr[i]);
        
        num_chars+=3;

        if (num_chars > (out_len-4)) break;
    }
    //sprintf(out_string+num_chars, "\n");
}


// /*!
//  * \brief print hex dump of buffer
//  *
//  * \return nothing
//  */
// void hex_dump(const uint8_t *bptr, uint32_t len) {
//     unsigned int i = 0;

//     printf("dump_bytes %lu", len);
//     for (i = 0; i < len;i++) {
//         if ((i & 0x0f) == 0) {
//             printf("\n");
//         } else if ((i & 0x07) == 0) {
//             printf(" ");
//         }
//         printf("%02x ", bptr[i]);
//     }
//     printf("\n");

//     printf("dump_chars %d", len);
//     for (i = 0; i < len;i++) {
//         if ((i & 0x0f) == 0) {
//             printf("\n");
//         } else if ((i & 0x07) == 0) {
//             printf(" ");
//         }
//         if (isprint(bptr[i]))
//         {
//             printf("%c", bptr[i]);
//         }
//         else
//         {
//             printf("-", bptr[i]);
//         }
//     }
//     printf("\n");
// }

#define HEX_DUMP_BYTE_PER_LINE (32)
/*!
 * \brief print hex dump of buffer
 *
 * \return nothing
 */
void hex_dump(const uint8_t *bptr, uint32_t len)
{
    size_t bytes_remaining = 0;
    size_t bytes_on_line = 0;
    char output_line[160];
    char output_byte[8];
    int i = 0;
    int line = 0;

    // ignore requests to dump NULL pointers
    if (!bptr)
    {
        return;
    }

    printf("hexdump address = %08p length = %08x\n", bptr, len);

    for(line = 0; line < ((len+(HEX_DUMP_BYTE_PER_LINE-1))/HEX_DUMP_BYTE_PER_LINE); line++)
    {
        bytes_remaining = len - line*HEX_DUMP_BYTE_PER_LINE;

        if(bytes_remaining > HEX_DUMP_BYTE_PER_LINE)
        {
            bytes_on_line = HEX_DUMP_BYTE_PER_LINE;
        }
        else
        {
            bytes_on_line = bytes_remaining;
        }        

        if(bytes_remaining)
        {
            output_line[0] = 0;

            for (i = 0; i < bytes_on_line;i++) 
            {
                snprintf(output_byte, sizeof(output_byte), "%02x ", bptr[line*HEX_DUMP_BYTE_PER_LINE+i]);
                STRAPPEND(output_line, output_byte);
            }            

             // add extra padding if less than HEX_DUMP_BYTE_PER_LINE bytes left(i.e. last line)
            for (; i<HEX_DUMP_BYTE_PER_LINE; i++)
            {
                STRAPPEND(output_line, "   ");
            }

            STRAPPEND(output_line, " ");  
            for (i = 0; i < bytes_on_line; i++) 
            {
                if (isprint(bptr[line*HEX_DUMP_BYTE_PER_LINE+i]))
                {
                   snprintf(output_byte, sizeof(output_byte), "%c", bptr[line*HEX_DUMP_BYTE_PER_LINE+i]);
                   STRAPPEND(output_line, output_byte);
                }
                else
                {
                    snprintf(output_byte, sizeof(output_byte), "-");
                    STRAPPEND(output_line, output_byte);
                }
            }

            STRAPPEND(output_line, "\n");
            printf("%s", output_line);                
        }        
    }
}







/*!
 * \brief Replace plus with space in string
 * 
 * \return number of evil plus signs destroyed
 */
int deplus_string(char *string, int max_len)
{
    int num_plus = 0;
    int i = 0;

    while ((i < max_len) && (string[i] != 0))
    {
        if (string[i] == '+')
        {
            string[i] = ' ';
            num_plus++;
        }

        i++;
    }

    return(num_plus);
}


/*!
 * \brief print printable text
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return number of characters printed
 */
int print_printable_text(char *contaminated_string)  
{
    unsigned int i = 0;
    int num_spaces = 0;

    if (contaminated_string != NULL)
    {
        for(i=0; i<2000; i++)
        {
            if (contaminated_string[i] == 0) break;

            if(contaminated_string[i] == '{')
            {
                printf("\n");  
                indent(num_spaces);                                  
                printf("{\n");
                num_spaces++;            
                indent(num_spaces);
            }   
            else if(contaminated_string[i] == '}')
            {
                num_spaces--;
                printf("\n");
                indent(num_spaces);            
                printf("}\n");
                indent(num_spaces);              
            }              
            else if (isprint(contaminated_string[i]))
            {
                printf("%c", contaminated_string[i]);
            }
            else if (contaminated_string[i] == '\r')
            {
                printf("[carriage return]");
            }
            else if (contaminated_string[i] == '\n')
            {
                printf("[newline]\n");
                indent(num_spaces);
            }
            
            

        }
        printf("\n");
    }
    return(i);
}

/*!
 * \brief print specfied number of spaces
 *
 * \param[in]   num_spaces  
 * 
 * \return 0
 */
int indent(int num_spaces)  
{
    while(num_spaces > 0)
    {
        printf(" ");
        num_spaces--;
    }

    return(0);
}



// in-place URL decoder (dst and src can be the same)
void urldecode(char *dst, const char *src) 
{
    char a, b;
    while (*src) 
    {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) 
        {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a + b;
            src += 3;
        } else if (*src == '+') 
        {
            *dst++ = ' ';
            src++;
        } else 
        {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

/*!
 * \brief convert ascii string to 32 bit IP address
 *
 * \param[in]   address_string         IPv4 address or hostname in ascii e.g. "192.168.1.1" or "google.com"   
 * 
 * \return 32 bit number representing IPv4 address
 */
uint32_t address_string_to_ip(char *address_string)
{
    struct addrinfo hints, *res;
    struct sockaddr_in *saddr;
    uint32_t ip_raw = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only
    
    if (getaddrinfo(address_string, NULL, &hints, &res) == 0) 
    {
        // cast to sockaddr_in
        saddr = (struct sockaddr_in *)res->ai_addr;
        
        // extract 32-bit raw IP (Network Byte Order)
        ip_raw = saddr->sin_addr.s_addr;
        
        // printf("Raw 32-bit (Network Order): %08x\n", ip_raw);
        // printf("Raw 32-bit (Host Order):    %08x\n", ntohl(ip_raw));
        
        freeaddrinfo(res);
    }

    return (ip_raw);
}

int ip_string_to_int_array_pton(const char* ip_str, unsigned char* ip_array) 
{
    int err = 0;
    struct in_addr sa;
    
    // Use inet_pton to convert the IP string to a network address structure
    if (inet_pton(AF_INET, ip_str, &sa) == 1) 
    {
        // sa.s_addr is a uint32_t in network byte order.
        // Copy the bytes into the array.
        memcpy(ip_array, &sa.s_addr, 4);

        // Note: On little-endian systems, the bytes in ip_array will be reversed
        // unless a byte-swap is performed to get the 'human-readable' order of octets
        // in the array. For most network operations, keeping it in network order is correct.
        // To get the octets in the order 192, 168, 1, 1:
        ip_array[0] = (sa.s_addr >> 24) & 0xFF;
        ip_array[1] = (sa.s_addr >> 16) & 0xFF;
        ip_array[2] = (sa.s_addr >> 8) & 0xFF;
        ip_array[3] = sa.s_addr & 0xFF;
    }
    else    
    {
        perror("inet_pton failed");
        err = -1;
    }

    return(err);
}

void to_lowercase(char *str) 
{
    for (int i = 0; str[i] != '\0'; i++) 
    {
        // Cast to unsigned char to prevent undefined behavior with non-ASCII chars
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

/*!
 * \brief convert string to integer times ten plus tenths e.g. "78.32" => 783
 *
 * \param[in]  value_string index of cgi handler in cgi_handlers table
 * 
 * \return integer = value x 10
 */
int get_int_with_tenths_from_string(char *value_string)
{
    int whole_part = 0;
    int tenths_part = 0;
    int new_value = 0;
    char first_decimal = '0';
    char second_decimal = '0';

    sscanf(value_string, ".%c%c", &first_decimal, &second_decimal);
    sscanf(value_string, "%d.%c%c", &whole_part, &first_decimal, &second_decimal);

    // zero out invalid characters
    if ((first_decimal < '0') || (first_decimal > '9')) first_decimal = '0';
    if ((second_decimal < '0') || (second_decimal > '9')) second_decimal = '0';
    
    tenths_part = first_decimal - '0';

    // round up from second decimal
    if ((second_decimal - '0') >= 5)
    {
        tenths_part++;
    }

    // round up fron frist decimal
    if (tenths_part > 9)
    {
        tenths_part -=10;
        whole_part++;
    }

    CLIP(tenths_part, 0, 9);

    new_value = whole_part*10 + tenths_part;  
    
    if (new_value == 662)
    {
        printf("AHHHHHHHGGGGGG!!!!\n");
    }

    return(new_value);
}

/*!
 * \brief print list of gpio pins with specified state
 *
 * \param none
 *
 * \return 0
 */
int print_gpio_pins_matching_default(char *buffer, int len, GPIO_DEFAULT_T gpio_default)
{
    int i;
    bool first_item_printed = false;
    int printed = 0;
    int first_in_run = -1;
    int next_in_run = -1;

    if (len > 8)
    {
        buffer[0] = 0;
        first_item_printed = false;
        for(i=0; i<NUM_ROWS(sys->gpio_default) && printed < (len-8); i++)
        {
            if (sys->gpio_default[i] == gpio_default)
            {               
                if (!first_item_printed)
                {
                    printed += snprintf(buffer, len, "%d", i);
                    first_item_printed = true;
                    first_in_run = i;
                    next_in_run = i+1;
                }
                else
                {
                    if ((first_in_run >= 0) && (i == next_in_run) && (i < (NUM_ROWS(sys->gpio_default)-1))) // within in a run
                    {
                        next_in_run = i+1;
                    }
                    else if ((first_in_run >= 0) && (i != next_in_run) && (first_in_run+1 == next_in_run)) // end of single value run, start of new run
                    {
                        printed += snprintf(buffer+printed, len, "&#44; %d", i);
                        first_in_run = i;
                        next_in_run = i+1;
                    } 
                    else if ((first_in_run >= 0) && (i != next_in_run) && (first_in_run+1 != next_in_run)) // end of run, start of new run
                    {
                        printed += snprintf(buffer+printed, len, " - %d&#44; %d", next_in_run-1, i);
                        first_in_run = i;
                        next_in_run = i+1;
                    }
                    else if ((first_in_run >= 0) && (i == next_in_run) && (i == (NUM_ROWS(sys->gpio_default)-1))) // end of list and end of run
                    {
                        printed += snprintf(buffer+printed, len, " - %d", i);
                        first_in_run = -1;
                        next_in_run = -1;
                    }                    
                    else
                    {
                        printed += snprintf(buffer+printed, len, "&#44; %d", i);
                        first_in_run = i;
                        next_in_run = i+1;
                    }                        
                }          
            }
            else if ((first_in_run >= 0) && (first_in_run+1 != next_in_run)) // end of run with more than one value
            {
                printed += snprintf(buffer+printed, len, " - %d", next_in_run-1);
                first_in_run = -1;
                next_in_run = -1;                
            }
            else if ((first_in_run >= 0)) // end of run with a single value
            {
                first_in_run = -1;
                next_in_run = -1;                
            }            
        }

        if (printed == 0)
        {
            printed += snprintf(buffer, len, "none");
        }
    }  

    return(printed);
}
