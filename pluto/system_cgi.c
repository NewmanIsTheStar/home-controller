/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #define _GNU_SOURCE
 
#include "lwip/apps/fs.h"
#include "lwip/sockets.h"


#include "FreeRTOS.h"
// #include "FreeRTOSConfig.h"
#include "task.h"

#include "utility.h"
#include "config.h"

//#include "worker_tasks.h"
#include "pluto.h"
#include "calendar.h"
#include "web.h"
#include "application_cgi.h"
#include "shell_message.h"

// typedef const char *(*tMYCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state);
// typedef struct
// {
//     const char *pcCGIName;
//     tMYCGIHandler pfnCGIHandler;
// } MY_CGI_T;

// extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;
// extern WORKER_TASK_T worker_tasks[];

extern char current_calendar_web_page[50];



/*!
 * \brief print all the parameters passed to a cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
void dump_parameters(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;    
    
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            printf("Parameter: %s has Value: %s\n", param, value);
        }

        i++;
    }
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_time_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    int hour = 0;
    int minute = 0;
    char *param = NULL;
    char *value = NULL;
    int new_value = 0;
       
    printf("Entering time cgi handler\n");
    
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    //force daylight saving off -- I really hate that this is how it works!  We only get passed the parameter when checkbox is "on"
    sys->daylightsaving_enable = 0;

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("tz", param) == 0)
            {               
                //skip escaped '+' if present  (%2B)  NB: won't work if lowercase B used
                if ((value[0] == '%') && (value[1] == '2') && (value[2] == 'B')) value += 3;

                sscanf(value, "%d", &hour);
                sscanf(value, "%d%%3A%d", &hour, &minute);

                if ((hour > -14) && (hour < 14))
                {
                    if (hour >= 0)
                    {
                        // add the minutes
                        new_value = hour*60 + minute;
                    }
                    else
                    {
                        //subtract the minutes
                        new_value = hour*60 - minute;
                    }

                    sys->timezone_offset = new_value;
                }
            }

            if (strcasecmp("dsstart", param) == 0)
            {
                sanitize_daylight_saving_date(value, sys->daylightsaving_start, sizeof(sys->daylightsaving_start));

            }

            if (strcasecmp("dsend", param) == 0)
            {
                sanitize_daylight_saving_date(value, sys->daylightsaving_end, sizeof(sys->daylightsaving_end));
            }

            if (strcasecmp("ts1", param) == 0)
            {
                STRNCPY(sys->time_server[0], value, sizeof(sys->time_server[0]));
            }

            if (strcasecmp("ts2", param) == 0)
            {
                STRNCPY(sys->time_server[1], value, sizeof(sys->time_server[1]));
            }

            if (strcasecmp("ts3", param) == 0)
            {
                STRNCPY(sys->time_server[2], value, sizeof(sys->time_server[2]));
            }

            if (strcasecmp("ts4", param) == 0)
            {
                STRNCPY(sys->time_server[3], value, sizeof(sys->time_server[3]));
            }  

            if (strcasecmp("dsenable", param) == 0)
            {
                if (value[0])
                {
                    sys->daylightsaving_enable = 1;
                } 
                else
                {
                    sys->daylightsaving_enable = 0;
                }                              
            }

            if (strcasecmp("glat", param) == 0)
            {
                sscanf(value, "%lf", &(sys->latitude));
            }   
            
            if (strcasecmp("glng", param) == 0)
            {
                sscanf(value, "%lf", &(sys->longitude));
            }              
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/time.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_network_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    //force dhcp_enable off -- I really hate that this is how it works!  We only get passed the parameter when checkbox is "on"
    sys->dhcp_enable = 0;

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("ssid", param) == 0)
            {
                STRNCPY(sys->wifi_ssid, value, sizeof(sys->wifi_ssid));
            }

            if (strcasecmp("wpass", param) == 0)
            {
                if (strcasecmp(value, "********") != 0)
                {
                    STRNCPY(sys->wifi_password, value, sizeof(sys->wifi_password));
                }
            }   

            if (strcasecmp("hostn", param) == 0)
            {
                STRNCPY(sys->host_name, value, sizeof(sys->host_name));
            }

            if (strcasecmp("ipad", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(sys->ip_address))!=0)
                {
                    STRNCPY(sys->ip_address, value, sizeof(sys->ip_address));
                    if (!sys->dhcp_enable)
                    {
                        STRNCPY(web.ip_address_string, value, sizeof(web.ip_address_string));
                    }                    
                }

            }

            if (strcasecmp("nmsk", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(sys->network_mask))!=0)
                {
                    STRNCPY(sys->network_mask, value, sizeof(sys->network_mask));
                    if (!sys->dhcp_enable)
                    {
                        STRNCPY(web.network_mask_string, value, sizeof(web.network_mask_string));
                    }                     
                }                   
                
            }   

            if (strcasecmp("gatewy", param) == 0)
            {

                if (strncasecmp(value, "automatic+via+DHCP", sizeof(sys->gateway))!=0)
                {
                    STRNCPY(sys->gateway, value, sizeof(sys->gateway));
                    if (!sys->dhcp_enable)
                    {
                        STRNCPY(web.gateway_string, value, sizeof(web.gateway_string));
                    }                     
                }                   
                
            } 

            if (strcasecmp("dhcp", param) == 0)
            {
                if (value[0])
                {
                    sys->dhcp_enable = 1;
                } 
                else
                {
                    sys->dhcp_enable = 0;
                }                             
            }

                              
                       
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/network.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_reboot_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    printf("REBOOT requested\n");
    
    //request reboot
    pluto_application_restart(REBOOT_USER_REQUEST);

    return "/index.shtml";    
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_syslog_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;

       
    // vile design caused by web browser not sending unchecked parameters, they must be presumed unchecked
    sys->syslog_enable = 0;       

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            if (strcasecmp("sloge", param) == 0)
            {
                if (value[0])
                {
                    sys->syslog_enable = 1;
                } 
                else
                {
                    sys->syslog_enable = 0; // unfortunately will never occur, hence unconditionally forced to zero at start of function 
                }                              
            }
            
            if (strcasecmp("slog", param) == 0)
            {
                STRNCPY(sys->syslog_server_ip, value, sizeof(sys->syslog_server_ip));
            }  
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/syslog.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_units_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int new_use_archaic_units = 0;   

    // set off by default
    sys->use_simplified_english  = 0; 
    sys->use_monday_as_week_start = 0; 

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
 
    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {

            if (strcasecmp("uau", param) == 0)
            {
                if (value[0])
                {
                    new_use_archaic_units = 1;
                } 
                else
                {
                    new_use_archaic_units = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            }   

            if (strcasecmp("simpe", param) == 0)
            {
                if (value[0])
                {
                    sys->use_simplified_english = 1;
                } 
                else
                {
                    sys->use_simplified_english  = 0;  // this should never happen, since the parameter is only passed if "on"
                }   
            }   

            if (strcasecmp("mweek", param) == 0)
            {
                if (value[0])
                {
                    sys->use_monday_as_week_start = 1;
                } 
                else
                {
                    sys->use_monday_as_week_start = 0;  // this should never happen, since the parameter is only passed if "on"
                } 
            }                                                                   
        }

        i++;
    }

    // set the default week long calendar page
    set_calendar_html_page();  

    // check for change in units
    if (new_use_archaic_units != sys->use_archaic_units)
    {
        sys->use_archaic_units = new_use_archaic_units;

        switch (new_use_archaic_units)
        {
            case false:  // convert from archaic units to SI
                printf("TODO:  units changed: convert archiac to metric here.\n");
            break;
            case true:   // convert from SI to archaic units
                printf("TODO:  units changed: convert metric to archiac here.\n");                
                break;
            default:
            break;
        }
    }     

    // Send the next page back to the user
    config_changed();
    return "/units.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_software_load_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;

    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            
            if (strcasecmp("swlhst", param) == 0)
            {
                STRNCPY(web.software_server, value, sizeof(web.software_server));
            }  
            if (strcasecmp("swlurl", param) == 0)
            {
                STRNCPY(web.software_url, value, sizeof(web.software_url));
                urldecode(web.software_url, web.software_url);
            }  
            if (strcasecmp("swlfle", param) == 0)
            {
                STRNCPY(web.software_file, value, sizeof(web.software_file));
            }                          

            web.download_now = 1;
        }

        i++;
    }


    // Send the next page back to the user
    //config_changed();
    return "/software_load.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_personality_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    PERSONALITY_E new_personality = NO_PERSONALITY;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("pertyp", param) == 0)
            {               
                sscanf(value, "%d", (int *)&new_personality);

                switch(new_personality)
                {
                    case REMOTE_SWITCH:
                        sys->personality = new_personality;
                        break;                                              
                    
                    default:
                        printf("Invalid personality\n");
                        break;
                } 
                config_changed();              
            }
        }  
        i++;
    }


    // Send the next page back to the user
    return "/personality.shtml";
}



/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_wificountry_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);

            if (strcasecmp("wific", param) == 0)
            {
                STRNCPY(sys->wifi_country, value, sizeof(sys->wifi_country));
                deplus_string(sys->wifi_country, sizeof(sys->wifi_country));             
            }                                                         
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/network.shtml";
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_gpio_default_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    char *param = NULL;
    char *value = NULL;
    int len = 0;
    int temp = 0;
    int gpio_number = -1;
    int gpio_value = -1;
       

    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);    

            len = strlen(param);

            if (strcasecmp("gpion", param) == 0)
            {
                sscanf(value, "%d", &gpio_number);
            }                
               
            if (strcasecmp("gpiov", param) == 0)
            {
                sscanf(value, "%d", &gpio_value);
            }                
        }
        i++;
    }

    if (gpio_valid(gpio_number))
    {
        switch(gpio_value)
        {
            default:
                break;
            case GP_UNINITIALIZED:       
            case GP_INPUT_FLOATING:          
            case GP_INPUT_PULLED_HIGH:          
            case GP_INPUT_PULLED_LOW:
            case GP_OUTPUT_HIGH:
            case GP_OUTPUT_LOW:
                sys->gpio_default[gpio_number] = gpio_value;
                break;
        }
    }

    // write config changes to flash
    config_changed();
 
    // Send the next page back to the user
    return "/gpio_defaults.shtml";    
}


/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_mqtt_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            
            if (strcasecmp("mquser", param) == 0)
            {
                STRNCPY(sys->mqtt_user, value, sizeof(sys->mqtt_user));
            }

            if (strcasecmp("mqpass", param) == 0)
            {
                if (strcasecmp(value, "********") != 0)
                {
                    STRNCPY(sys->mqtt_password, value, sizeof(sys->mqtt_password));
                }
            }   

            if (strcasecmp("mqaddr", param) == 0)
            {
                STRNCPY(sys->mqtt_broker_address, value, sizeof(sys->mqtt_broker_address));
            }            
        }

        i++;
    }


    // Send the next page back to the user
    config_changed();
    return "/mqtt.shtml";
}

/*!
 * \brief cgi handler
 *
 * \param[in]  iIndex       index of cgi handler in cgi_handlers table
 * \param[in]  iNumParams   number of parameters
 * \param[in]  pcParam      parameter name
 * \param[in]  pcValue      parameter value 
 * 
 * \return nothing
 */
const char * cgi_basic_run_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    //int new_value = 0;
       
    //dump_parameters(iIndex, iNumParams, pcParam, pcValue);
    printf("cgi_basic_run_handler\n");

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
                       
        }

        i++;
    }

    // tell hc_task to run the script
    //hc_queue_send(HC_CMD_BASIC_SCRIPT);
    shell_queue_basic_script(NULL);

    // Send the next page back to the user
    //config_changed();
    return "/basic.shtml";
}


/*!
 * \brief initialize cgi handlers when LWIP_HTTPD_CGI == 1 (old style)
 * 
 * \return nothing
 */
void cgi_init(void)
{
    #if LWIP_HTTPD_CGI == 1
    http_set_cgi_handlers(cgi_handlers, NUM_ROWS(cgi_handlers));
    #endif
}


static const MY_CGI_T system_cgi_handlers[] = {
    // ** shell uri to ignore
    {"/listen.shtml",                   NULL}, 
    {"/commands.json",                  NULL}, 

    // *** system handlers ***
    {"/cgi_gpio_default.shtml",               cgi_gpio_default_handler}, 
    {"/cgi_wificountry.shtml",                cgi_wificountry_handler},      
    {"/cgi_network.shtml",                    cgi_network_handler}, 
    {"/cgi_reboot.shtml",                     cgi_reboot_handler},              
    {"/cgi_time.shtml",                       cgi_time_handler},
    {"/cgi_syslog.shtml",                     cgi_syslog_handler}, 
    {"/cgi_mqtt.shtml",                       cgi_mqtt_handler},    
    {"/cgi_units.shtml",                      cgi_units_handler},   
    {"/cgi_swload.shtml",                     cgi_software_load_handler},
    {"/cgi_personality.shtml",                cgi_personality_handler}, 
};

/**
 * Global CGI handler used when LWIP_HTTPD_CGI_SSI == 1 (new style)
 * 
 * @param uri The URI requested by the client (e.g., "/cgi_leds.shtml")
 * @param iNumParams Number of parameters parsed from the query string
 * @param pcParam Array of parameter name strings
 * @param pcValue Array of parameter value strings
 * @return The filename/URI of the page to load next (e.g., "/index.shtml")
 */
void httpd_cgi_handler(struct fs_file *file, const char* uri, int iNumParams, 
                       char **pcParam, char **pcValue, void *connection_state) 
{
    int i;
    WEB_SESSION_STATE_T *session = NULL;
    bool handled = false;

    // all html files requiring cgi have file names begining with "/cgi_"
    if (strncasecmp("/cgi_", uri, 5) == 0)
    { 
        // scan system handlers       
        for(i=0; i<NUM_ROWS(system_cgi_handlers); i++)
        {
            //printf("checking CGI URI: %s vs %s\n", system_cgi_handlers[i].pcCGIName, uri);
            if (strcasecmp(system_cgi_handlers[i].pcCGIName, uri) == 0) 
            {
                if (system_cgi_handlers[i].pfnCGIHandler)
                {                
                    printf("calling handler for %s\n", system_cgi_handlers[i].pcCGIName);
                    system_cgi_handlers[i].pfnCGIHandler(0, iNumParams, pcParam, pcValue, (WEB_SESSION_STATE_T *)connection_state);
                }
                handled = true;
                break;
            }
        }

        // scan application handlers
        httpd_cgi_handler(file, uri, iNumParams, pcParam, pcValue, connection_state); 

    }
}


/**
 * Called by lwIP when an HTTP file is opened.
 * Return a pointer to your state data or NULL if not needed.
 */
void *fs_state_init(struct fs_file *file, const char *name)
{
    WEB_SESSION_STATE_T *session = NULL;

    if (strncasecmp("/cgi_", name, 5) == 0)
    {
        if (file->state == NULL)
        {
            // allocate block memory for this specific file request session
            session = (WEB_SESSION_STATE_T *)mem_malloc(sizeof(WEB_SESSION_STATE_T));
            if (session) 
            {
                // set baseline defaults
                session->thermostat_day = 0;
                session->thermostat_period_row = 0;
                session->rmtsw_relay_day = 0;
                session->rmtsw_relay_period_row = 0;
                session->automation_file_number = 0; 

                // bind custom tracking struct directly to the file instance
                file->state = session;             
                //printf("allocated memory for web context @%p\n", session);
            }
        }
        else
        {
            printf("refusing to allocate memory for web context as it already exists\n");
        } 
    }

    return session; 
}

/**
 * Called by lwIP when an HTTP file is closed.
 * Use this to free memory allocated in fs_state_init.
 */
void fs_state_free(struct fs_file *file, void *state)
{
    if (state != NULL) 
    {
        //printf("RELEASING web context memory @%p\n", file->state);    
        mem_free(state);
        file->state = NULL;
    }    
}