#define _GNU_SOURCE
#include <string.h>

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
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"


#include "stdarg.h"

// #include "weather.h"
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
// #include "powerwall.h"
#include "shelly.h"
#include "json_parser.h"
#include "pluto.h"
#include "ping_core.h"
#include "shell.h"

#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

typedef enum
{
    SHELLY_TYPE_SHSW_25,
    SHELLY_TYPE_PLUSWDUS,
    SHELLY_TYPE_PLUS1,
    SHELLY_TYPE_PLUS2PM,
    SHELLY_TYPE_UNKNOWN
} SHELLY_DEVICE_TYPE_T;

typedef struct
{
    u32_t ip;
    SHELLY_DEVICE_TYPE_T type;
} DISCOVERED_SHELLY_T;

typedef struct
{
    uint8_t shelly_device_ip[64][4];
    uint8_t shelly_device_type[64];
    uint8_t shelly_triple_device_index[255];
    uint8_t shelly_triple_name_index[255];
    char shelly_triple_value[255][32];
    char shelly_parameter_name[255][32];   
} SHELLY_CACHE_T;

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// global variables
JSON_PARSER_CONTEXT_T shelly_parser_context;
unsigned char rx_buffer[2048]; 
DISCOVERED_SHELLY_T discovered_shelly[32];
int num_discovered_shelly_devices = 0;
SHELLY_CACHE_T shelly_cache;


// prototypes
int query_status(char *ipstring);
int shelly_parse_header(char *buffer);
char *find_next_space_on_line(char *buffer);
int shelly_add_discovered_device(u32_t ip, SHELLY_DEVICE_TYPE_T type);
int shelly_dump_discovered_devices(void);
int shelly_cache_insert_json_kvps(char *device_ip);
int strip_quotes(char *dst, char *src);
//void ip_string_to_int_array_pton(const char* ip_str, unsigned char* ip_array);


/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int shelly_discover_devices(void)
{
    u32_t ip;
    u32_t mask;
    static u32_t search_start;
    static u32_t search_end;
    static u32_t ip_to_query;
    static bool search_in_progress = false;
    int values[4] = {0,0,0,0};
    u8_t byte = 0;
    int i;
    char ipstring[32];
    static int number_of_shelly_devices = 0;
    char device_type[32];
    char device_id[32];
    ip_addr_t ping_addr;
    int ping_err = -1;

    if (!search_in_progress)
    {
        printf("Beginning new network scan for shelly devices\n");

        printf("address = %s\n", web.ip_address_string);  
        printf("netmask = %s\n", web.network_mask_string);

        sscanf(web.ip_address_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);
        printf("%d.%d.%d.%d\n", values[0], values[1], values[2], values[3]);
        ip   = 0x00000000;
        for(i=0; i<4; i++)
        {
            byte = (u8_t)values[i];
            ip = ip<<8 | byte;
        }
        printf("ip = %08x\n", ip);

        sscanf(web.network_mask_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);    
        printf("%d.%d.%d.%d\n", values[0], values[1], values[2], values[3]);

        mask   = 0x00000000;
        for(i=0; i<4; i++)
        {
            byte = (u8_t)values[i];
            mask = mask<<8 | byte;
        }
        printf("mask = %08x\n", mask); 

        search_start = ip & mask;
        search_end = search_start | (0xffffffff ^ mask);
    
        //TEST TEST TEST
        search_start = 0xc0a8219f;
        search_end =   0xc0a821b5;

        for(i=0; i<NUM_ROWS(shelly_cache.shelly_device_ip); i++)
        {
            shelly_cache.shelly_device_ip[i][0] = 0;
            shelly_cache.shelly_device_ip[i][1] = 0;
            shelly_cache.shelly_device_ip[i][2] = 0;
            shelly_cache.shelly_device_ip[i][3] = 0;                        
            shelly_cache.shelly_device_type[i] = 0;        
        }

        for(i=0; i<NUM_ROWS(shelly_cache.shelly_triple_device_index); i++)
        {    
            shelly_cache.shelly_triple_device_index[i] = 255;
            shelly_cache.shelly_triple_name_index[i] = 255;
        }

        for(i=0; i<NUM_ROWS(shelly_cache.shelly_triple_value); i++)
        {     
            shelly_cache.shelly_triple_value[i][0] = 0;        
        }     
                
        for(i=0; i<NUM_ROWS(shelly_cache.shelly_parameter_name); i++)
        {       
            shelly_cache.shelly_parameter_name[i][0] = 0;        
        }            
        //END END END TEST TEST TEST

        printf("search range: %08x to %08x\n", search_start, search_end);
        ip_to_query=search_start;
        number_of_shelly_devices = 0;
        search_in_progress = true;


    }

    // for(ip_to_query=search_start+1; ip_to_query<search_end; ip_to_query++)

    ip_to_query++;

    if (ip_to_query<search_end)
    {
        //byte = *((u8_t *)&ip_to_query);
        //printf("byte = %0x\n", byte);
        sprintf(ipstring, "%d.%d.%d.%d", ((u8_t *)&ip_to_query)[3], ((u8_t *)&ip_to_query)[2], ((u8_t *)&ip_to_query)[1], ((u8_t *)&ip_to_query)[0]);

        printf("querying %s\n", ipstring);

        printf("ping %s\n", ipstring);

        //ipaddr_aton(PING_ADDR, &ping_addr);
        ping_addr.addr = htonl(ip_to_query);

        ping_err = ping_device(&ping_addr, 3); 
        printf("ping returned error = %d\n", ping_err);

        if (ping_err == 0)
        {
            printf("http request %s\n", ipstring);

            //query_status(ipstring);
            if (shelly_http_request(HTTP_GET, "/shelly", ipstring, NULL) == 200)
            {
                shelly_cache_insert_json_kvps((char *)&ip_to_query);

                // get device type
                if (jsonp_get_value(&shelly_parser_context, "root.\"type\"", device_type, sizeof(device_type), false))
                {
                    STRNCPY(device_type, "UNKNOWN", sizeof(device_type));
                }

                // get device id
                if (jsonp_get_value(&shelly_parser_context, "root.\"id\"", device_id, sizeof(device_id), false))
                {
                    STRNCPY(device_id, "UNKNOWN", sizeof(device_id));
                }

                if (strcasecmp(device_type, "\"SHSW-25\"") == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_SHSW_25);
                    shelly_cache_set_value((char *)&ip_to_query, "known_type", device_type);
                }
                else if (strncasecmp(device_id, "\"shellypluswdus", 15) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUSWDUS);
                    shelly_cache_set_value((char *)&ip_to_query, "known_type", device_id);
                }      
                else if (strncasecmp(device_id, "\"shellyplus1", 12) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUS1);
                    shelly_cache_set_value((char *)&ip_to_query, "known_type", device_id);
                }  
                else if (strncasecmp(device_id, "\"shellyplus2pm", 14) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUS2PM);
                    shelly_cache_set_value((char *)&ip_to_query, "known_type", device_id);
                }                                            
                else
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_UNKNOWN);
                }

                printf("IP = %s SHELLY_DEVICE_TYPE = %s SHELLY_DEVICE_ID = %s\n", ipstring, device_type, device_id);
                //shelly_dump_discovered_devices();
                
                number_of_shelly_devices++;
                printf("number of shelly devices discovered = %d\n", number_of_shelly_devices);


                
            }
        }
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>SHELLY DEVICES FOUND  = %d\n", number_of_shelly_devices);
    }
    else
    {
        printf("Network scan for shelly devices complete\n");
        search_in_progress = false;

        printf("number of shelly devices discovered = %d\n", number_of_shelly_devices);
        shelly_dump_discovered_devices();

        shelly_cache_dump();
    }

    return(search_in_progress);
}


// Send HTTP request
int shelly_http_request(HTTP_REQUEST_TYPE_T type, char *url, char *host, char *content)
{
    int socket = -1;
    int err = 0;
    char request[2048];
    int length = 0;
    char length_string[8];
    // lwip_err_t lwip_err = -1;
    int sent_bytes = 0;
    int received_bytes = 0;
    int retry;
    int ret;
    fd_set readset;
    struct timeval tv;  
    char *start_of_json = NULL;
    int http_error = 0;
        

    // type
    switch(type)
    {
        case HTTP_GET:
            sprintf(request, "GET ");
            break;
        case HTTP_POST:
            sprintf(request, "POST ");
            break;
        default:
            request[0] = 0;
            err = true;
    }

    // url
    STRAPPEND(request, url);
    STRAPPEND(request, " HTTP/1.1\r\n");

    // host
    STRAPPEND(request, "Host: ");
    STRAPPEND(request, host);
    STRAPPEND(request, "\r\n");

    // accept
    STRAPPEND(request, "Accept: */*\r\n");

    // content
    if (content)
    {
        sprintf(length_string, "%d", strlen(content));

        // STRAPPEND(request, "Content-Type: application/json\r\n");
        // STRAPPEND(request, "Content-Length: ");
        // STRAPPEND(request, length_string);
        // STRAPPEND(request, "\r\n\r\n");
        STRAPPEND(request, content);
    }
    STRAPPEND(request, "\r\n");

    // request length 
    length = strlen(request) + 1; // TODO figure out WTF the original code is doing by transmitting one less


    if (socket < 0) socket = establish_socket(host, 80, SOCK_STREAM);

    if(socket>= 0)
    {
        //printf("SEND [%d bytes]\n%s\n", length, request); 
        
        if (socket >= 0)
        {
            sent_bytes = send(socket, request, strlen(request), 0);              

            if (sent_bytes)
            {
                //printf("sent_bytes = %d\n", sent_bytes);

                //printf("waiting to receive response...\n");                    
                for (retry=0; retry<5; retry++)
                {
                    FD_ZERO(&readset);
                    FD_SET(socket, &readset);
                    tv.tv_sec = 2;
                    tv.tv_usec = 500;
    
                    ret = select(socket + 1, &readset, NULL, NULL, &tv);
    
                    if ((ret > 0) && FD_ISSET(socket, &readset))
                    {
                        received_bytes = recv(socket, rx_buffer, sizeof(rx_buffer), 0);

                        CLIP(received_bytes, 0, sizeof(rx_buffer) - 1);

                        // zero terminate
                        if (received_bytes < sizeof(rx_buffer)-1)
                        {
                            rx_buffer[received_bytes] = 0;
                        }
                        else
                        {
                            rx_buffer[sizeof(rx_buffer)-1] = 0;
                        }
    
                        if (received_bytes > 0)
                        {
                            // hex_dump(rx_buffer, received_bytes);

                            // print_printable_text(rx_buffer);

                            //printf("parse header\n");
                            http_error = shelly_parse_header(rx_buffer);
                            //printf("http error code = %d\n", http_error);

                            //printf("parse json\n");
                            start_of_json = strcasestr(rx_buffer, "\r\n{");
                            if (start_of_json)
                            {
                                //start_of_json += 2; // point to opening brace

                                jsonp_parse_buffer(&shelly_parser_context, start_of_json, false);
                                //jsonp_dump_key_value_pairs(&shelly_parser_context);                        
                            }

                            err = http_error;
                            break;
                        }
                        else
                        {
                            err = -2;
                        }
                    }
                    else
                    {
                        err = -3;
                    }         
                }        
            }
            else
            {
             
                err = -5;
                printf("error sending packet\n");
            }
        } 

        close(socket);
        socket = -1;
    }

    return (err);
}

// Send HTTP request
int shelly_parse_header(char *buffer)
{
    char *keyword = NULL;
    int http_error = -1;

    keyword = strcasestr(rx_buffer, "HTTP/");

    if (keyword)
    {
        keyword += strlen("HTTP/");

        keyword = find_next_space_on_line(keyword);

        if (keyword)
        {
            keyword++;

            sscanf(keyword, "%d", &http_error);            
        }
    }

    return(http_error);
}

// find next space on line
char *find_next_space_on_line(char *buffer)
{
    char *found = NULL;

    if (buffer)
    {
        while(*buffer != '\r' && *buffer != '\n' && *buffer != 0)
        {
            if (*buffer == ' ')
            {
                found = buffer;
                break;
            }

            buffer++;
        }
    }

    return(buffer);
}


// "type":"SHSW-25"/

// store discovered device
int shelly_add_discovered_device(u32_t ip, SHELLY_DEVICE_TYPE_T type)
{
    int err = 0;

    if (num_discovered_shelly_devices < NUM_ROWS(discovered_shelly))
    {
        discovered_shelly[num_discovered_shelly_devices].ip = ip;
        discovered_shelly[num_discovered_shelly_devices].type = type;

        num_discovered_shelly_devices++;
    }
    else
    {
        err = 1;
    }

    return(err);
}

// store discovered device
int shelly_dump_discovered_devices(void)
{
    int err = 0;
    int i = 0;
    FILE *fp;

    fp = fopen("discovered", "w");

    if (fp)
    {
        if (num_discovered_shelly_devices)
        {
            fprintf(fp, "[\n");

            for(i=0; i<num_discovered_shelly_devices; i++)
            {
                //printf("IP = %d.%d.%d.%d Type = %d\n", ((u8_t *)&discovered_shelly[i].ip)[3], ((u8_t *)&discovered_shelly[i].ip)[2], ((u8_t *)&discovered_shelly[i].ip)[1], ((u8_t *)&discovered_shelly[i].ip)[0], discovered_shelly[i].type);
                //fprintf(fp, "IP = %d.%d.%d.%d Type = %d\n", ((u8_t *)&discovered_shelly[i].ip)[3], ((u8_t *)&discovered_shelly[i].ip)[2], ((u8_t *)&discovered_shelly[i].ip)[1], ((u8_t *)&discovered_shelly[i].ip)[0], discovered_shelly[i].type);                
                fprintf(fp, "  { \"ip\": \"%d.%d.%d.%d\", \"type\": %d }%s", ((u8_t *)&discovered_shelly[i].ip)[3], ((u8_t *)&discovered_shelly[i].ip)[2], ((u8_t *)&discovered_shelly[i].ip)[1], ((u8_t *)&discovered_shelly[i].ip)[0], discovered_shelly[i].type, (i+1)<num_discovered_shelly_devices?",\n":"\n");                
                 
            }
             fprintf(fp, "]\n");
        }

        fclose(fp);
    }

    return(err);
}

int shelly_cache_insert_json_kvps(char *device_ip)
{
    int err = 0;
    int key_index;
    int key_heirarchy;
    char name[32];
    char value[64];

    for(key_index=0; key_index<NUM_ROWS(shelly_parser_context.jsonp_key); key_index++)
    {
        name[0] = 0;
        
        if ((shelly_parser_context.jsonp_key[key_index][0] != 255))
        {
            for (key_heirarchy=0; key_heirarchy<NUM_ROWS(shelly_parser_context.jsonp_key[0]); key_heirarchy++)
            {
                if ((shelly_parser_context.jsonp_key[key_index][key_heirarchy] != 255))
                {
                    if (key_heirarchy != 0)
                    {
                        STRAPPEND(name, ".");
                    }

                    strip_quotes(value, shelly_parser_context.jsonp_token[shelly_parser_context.jsonp_key[key_index][key_heirarchy]]);

                    STRAPPEND(name, value);
                }
            }
        
            shelly_cache_set_value(device_ip, name, shelly_parser_context.jsonp_value[key_index]);
        }
    }

    return(err);
}

int shelly_cache_set_value(char *device_ip, char *parameter_name, char *parameter_value)
{
    int err = 0;
    uint8_t device_index = 255;
    uint8_t parameter_index = 255;

    err = shelly_cache_insert_device(device_ip, &device_index);
    //printf("err = %d device index = %d\n", err, device_index);

    if(!err)
    {
        err = shelly_cache_insert_parameter(device_index, parameter_name, &parameter_index);
        //printf("err = %d parameter_index = %d\n", err, parameter_index);
    }

    if(!err)
    {
        CLIP(parameter_index, 0, NUM_ROWS(shelly_cache.shelly_triple_value));
        strncpy(shelly_cache.shelly_triple_value[parameter_index], parameter_value, sizeof(shelly_cache.shelly_triple_value[parameter_index])); 
        //printf("parameter value [%d] = %s\n", parameter_index, shelly_cache.shelly_triple_value[parameter_index]);
    }

    return(err);
}

int shelly_cache_get_value(uint8_t *device_ip, char *parameter_name, char *value, size_t value_len)  //TODO: would it be simpler to just compare IP as a 32 object?
{
    int err = -1;
    uint8_t device_index;
    uint8_t name_index;
    uint8_t parameter_index;


    for(device_index=0; device_index<NUM_ROWS(shelly_cache.shelly_device_ip); device_index++)
    {
        //printf("%d.%d.%d.%d vs %d.%d.%d.%d\n", device_ip[0], device_ip[1], device_ip[2], device_ip[3], shelly_cache.shelly_device_ip[device_index][0], shelly_cache.shelly_device_ip[device_index][1], shelly_cache.shelly_device_ip[device_index][2], shelly_cache.shelly_device_ip[device_index][3]);
        if ((device_ip[0] == shelly_cache.shelly_device_ip[device_index][0]) &&
            (device_ip[1] == shelly_cache.shelly_device_ip[device_index][1]) &&
            (device_ip[2] == shelly_cache.shelly_device_ip[device_index][2]) &&
            (device_ip[3] == shelly_cache.shelly_device_ip[device_index][3]))
        {
            // ip match
            //printf("IP MATCH\n");
            err = 0;
            break;
        }
    }

    if (!err)
    {
        for(name_index=0; name_index<NUM_ROWS(shelly_cache.shelly_parameter_name); name_index++)
        {
            err = -2;

            //printf("%s vs %s\n", parameter_name, shelly_cache.shelly_parameter_name[name_index]);
            if (strncmp(parameter_name, shelly_cache.shelly_parameter_name[name_index], sizeof(shelly_cache.shelly_parameter_name[name_index])) == 0)
            {
                // name match
                err = 0;
                break;
            }
        }
    }

    //printf("Seeking device(%d) = %0x parameter name (%d) = %s\n", device_index, *device_ip, name_index, shelly_cache.shelly_parameter_name[name_index]);

    if (!err)
    {
        for(parameter_index=0; parameter_index<NUM_ROWS(shelly_cache.shelly_triple_device_index); parameter_index++)
        {
            err = -3;

            //printf("device_index: %d vs %d  name_index %d vs %d\n", shelly_cache.shelly_triple_device_index[parameter_index], device_index, shelly_cache.shelly_triple_name_index[parameter_index], name_index);
            if ((shelly_cache.shelly_triple_device_index[parameter_index] == device_index) &&
                (shelly_cache.shelly_triple_name_index[parameter_index] == name_index))
            {
                // ip and name match
                STRNCPY(value, shelly_cache.shelly_triple_value[parameter_index], value_len);
                //printf("value = %s\n", value);
                err = 0;
                break;
            }
        }
    }

    return(err);
}

int shelly_cache_insert_parameter(uint8_t device_index, char *parameter_name, uint8_t *parameter_index)
{
    int err = -1;
    bool name_exists = false;
    uint8_t i = 0;
    uint8_t j = 0;

    for(j=0; j<NUM_ROWS(shelly_cache.shelly_triple_device_index); j++)
    {
        if (shelly_cache.shelly_triple_device_index[j] == 255)
        {
            // found empty parameter row j
            shelly_cache.shelly_triple_device_index[j] = device_index;
            err = 0;
            break;
        }
    }

    if (!err)
    {
        name_exists = false;

        for(i=0; i < NUM_ROWS(shelly_cache.shelly_parameter_name); i++)
        {
            if (strcmp(parameter_name, shelly_cache.shelly_parameter_name[i]) == 0)
            {
                // found parameter name already exists
                shelly_cache.shelly_triple_name_index[j] = i;
                name_exists = true;
                *parameter_index = j;
                break;
            }
        }

        if (!name_exists)
        {
            err = -1;

            for(i=0; i <NUM_ROWS(shelly_cache.shelly_parameter_name); i++)
            {
                // find empty row
                if (shelly_cache.shelly_parameter_name[i][0] == 0)
                {
                    // insert new name into row
                    strncpy(shelly_cache.shelly_parameter_name[i], parameter_name, sizeof(shelly_cache.shelly_parameter_name[i]));
                    
                    // insert index of new name into parameter row 
                    shelly_cache.shelly_triple_name_index[j] = i;
                    err = 0;
                    *parameter_index = j;
                    break;
                }
            }
        }         
    }

   

    return(err);
}

int shelly_cache_insert_device(uint8_t *device_ip, uint8_t *device_index)
{
    int err = -1;
    uint8_t i = 0;

    for(i=0; i < NUM_ROWS(shelly_cache.shelly_device_ip); i++)
    {
        if ((shelly_cache.shelly_device_ip[i][0] == device_ip[0]) &&
            (shelly_cache.shelly_device_ip[i][1] == device_ip[1]) &&
            (shelly_cache.shelly_device_ip[i][2] == device_ip[2]) &&
            (shelly_cache.shelly_device_ip[i][3] == device_ip[3]))
        {
            err = 0;
            *device_index = i;
            break;
        }
    }

    if (err)
    {
        for(i=0; i < NUM_ROWS(shelly_cache.shelly_device_ip); i++)
        {
           if ((shelly_cache.shelly_device_ip[i][0] == 0) &&
                (shelly_cache.shelly_device_ip[i][1] == 0) &&
                (shelly_cache.shelly_device_ip[i][2] == 0) &&
                (shelly_cache.shelly_device_ip[i][3] == 0))
            {
                err = 0;
                *device_index = i;

                shelly_cache.shelly_device_ip[i][0] = device_ip[0];
                shelly_cache.shelly_device_ip[i][1] = device_ip[1];
                shelly_cache.shelly_device_ip[i][2] = device_ip[2];
                shelly_cache.shelly_device_ip[i][3] = device_ip[3]; 
                break;
            }
        }               
    }

    return(err);
}

int shelly_cache_clear(void)
{
    int i;

    for(i=0; i<NUM_ROWS(shelly_cache.shelly_device_ip); i++)
    {
        shelly_cache.shelly_device_ip[i][0] = 0;
        shelly_cache.shelly_device_ip[i][1] = 0;
        shelly_cache.shelly_device_ip[i][2] = 0;
        shelly_cache.shelly_device_ip[i][3] = 0;

        shelly_cache.shelly_device_type[i] = 255;
    }

    for (i=0; i<NUM_ROWS(shelly_cache.shelly_triple_device_index); i++)
    {
        shelly_cache.shelly_triple_device_index[i] = 255;
        shelly_cache.shelly_triple_name_index[i] = 255;
    }

    for(i=0; i<NUM_ROWS(shelly_cache.shelly_parameter_name); i++)
    {
        shelly_cache.shelly_parameter_name[i][0] = 0;   
    }

    return(0);
}
 
int shelly_cache_dump(void)
{
    int i;

    for (i=0; i<NUM_ROWS(shelly_cache.shelly_triple_value); i++)
    {
        if ((shelly_cache.shelly_triple_device_index[i] != 255) && (shelly_cache.shelly_triple_name_index[i] != 255))
        {
            printf("%8d: %d.%d.%d.%d %s = %s\n", i,
                                            shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][3],
                                            shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][2],
                                            shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][1],
                                            shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][0],
                                            shelly_cache.shelly_parameter_name[shelly_cache.shelly_triple_name_index[i]],
                                            shelly_cache.shelly_triple_value[i]);
        }
        else
        {
            //printf("Skipping device_index %d name_index %d value = %s\n", shelly_cache.shelly_triple_device_index[i], shelly_cache.shelly_triple_name_index[i], shelly_cache.shelly_triple_value[i]);
        }
    }
    return(0);
}

int shelly_cache_device_dump(char *ipv4_string)
{
    int i;
    u32_t ip;
    int values[4] = {0,0,0,0};
    u8_t byte = 0;
    int num_cache_entries = 0;

    sscanf(ipv4_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);
    
    ip   = 0x00000000;
    for(i=0; i<4; i++)
    {
        byte = (u8_t)values[i];
        ip = ip<<8 | byte;
    }

    shell_printf("shelly cache entries for device @ %d.%d.%d.%d\n",
                shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][3],
                shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][2],
                shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][1],
                shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]][0]);

    for (i=0; i<NUM_ROWS(shelly_cache.shelly_triple_value); i++)
    {
        if ((shelly_cache.shelly_triple_device_index[i] != 255) &&
            (shelly_cache.shelly_triple_name_index[i] != 255) &&
            (ip == *(u32_t *)shelly_cache.shelly_device_ip[shelly_cache.shelly_triple_device_index[i]]))
        {
            shell_printf("\t%s = %s\n",
                shelly_cache.shelly_parameter_name[shelly_cache.shelly_triple_name_index[i]],
                shelly_cache.shelly_triple_value[i]);

            num_cache_entries++; 
        }
    }

    if (!num_cache_entries)
    {
        shell_printf("\tnone\n");
    }

    return(0);
}

int strip_quotes(char *dst, char *src)
{
    int err = 0;
    int len;
    int start;
    int end;
    int i = 0;
    int j = 0;

    len = strlen(src);

    if ((len > 0) && (src[0] == '"') && (src[len-1] == '"'))
    {
        start = 1;
        end = len-1; 
    }
    else
    {
        start = 0;
        end = len;
    }

    for (i=start; i < end; i++)
    {
        dst[j++] = src[i];
    }

    dst[j] = 0;

    return(err);
}