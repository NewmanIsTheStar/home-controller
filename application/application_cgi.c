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
#include "application_config.h"
#include "worker_tasks.h"
#include "pluto.h"
#include "hc_task.h"
#include "calendar.h"
#include "web.h"
#include "system_cgi.h"
#include "shell_message.h"


// typedef const char *(*tMYCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state);
// typedef struct
// {
//     const char *pcCGIName;
//     tMYCGIHandler pfnCGIHandler;
// } MY_CGI_T;

extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

extern char current_calendar_web_page[50];


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
const char * cgi_hc_automation_edit_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
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
            if (strcasecmp("x", param) == 0)
            {
                sscanf(value, "%d", &(connection_state->automation_file_number));

                if ((((WEB_SESSION_STATE_T *)connection_state)->automation_file_number < 0) || (((WEB_SESSION_STATE_T *)connection_state)->automation_file_number > 31))
                {
                    ((WEB_SESSION_STATE_T *)connection_state)->automation_file_number = hc_get_new_automation_number();
                    
                    //TODO: handle table full more gracefully
                    CLIP(((WEB_SESSION_STATE_T *)connection_state)->automation_file_number, 0, 31);
                }                
            }                 
        }

        i++;
    }

    // Note: Javascript will push the file when the user hits the save button
    // Once the file is receieved it is saved into our local flash file system
    // in the function httpd_post_receive_data()

    // Send the next page back to the user
    return "/edit.shtml";
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
const char * cgi_hc_automation_enable_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;
    //int whole_part = 0;
    //int tenths_part = 0;
    char *param = NULL;
    char *value = NULL;
    int automation_number = -1;
    int automation_enable = -1;

    // printf("cgi_automation_enable_handler\n");
    // dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            if (strcasecmp("row", param) == 0)
            {
                sscanf(value, "%d", &automation_number);              
            }
            
            if (strcasecmp("status", param) == 0)
            {
                 sscanf(value, "%d", &automation_enable);
            }            
        }

        i++;
    }

    if ((automation_number >= 0) && (automation_number <= 31))
    {
        if (automation_enable == 0)
        {
            //printf("automation %d disabled\n", automation_number);
            cfg->automation_state[automation_number] = AUTOMATION_DISABLED;
        }
        else if (automation_enable == 1)
        {
            //printf("automation %d enabled\n", automation_number);
            cfg->automation_state[automation_number] = AUTOMATION_ENABLED;
        }
    }      

    // Send the next page back to the user
    //config_changed();
    return "/placeholder";   // the return value is ignored
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
const char * cgi_hc_automation_delete_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;

    char *param = NULL;
    char *value = NULL;
    int automation_number = -1;
    char automation_file_name[16];
       
    // printf("cgi_hc_automation_delete_handler\n");
    
    // dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            //printf("Parameter: %s has Value: %s\n", param, value);
            if (strcasecmp("x", param) == 0)
            {                
                sscanf(value, "%d", &automation_number); 
                
                if (cfg && automation_number>= 0 && automation_number <=31)
                {
                    printf("cgi_hc_automation_delete_handler: setting %d to undefined\n", automation_number);
                    cfg->automation_state[automation_number] = AUTOMATION_UNDEFINED;
                    sprintf(cfg->automation_name[automation_number], "automation%02d", automation_number); 

                    // sprintf(web.delete_filename, "automation%02d", automation_number);
                    // hc_queue_send(HC_CMD_DELETE_FILE);

                    sprintf(automation_file_name, "automation%02d", automation_number);
                    shell_queue_unlink(automation_file_name);
                }
            }                       
        }

        i++;
    }


    // Send the next page back to the user
    //config_changed();
    return "/delete.shtml";
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
const char * cgi_hc_automation_save_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state)
{
    int i = 0;

    char *param = NULL;
    char *value = NULL;
    int automation_number = -1;
    int extracted_automation_number = 0;
       
    printf("cgi_hc_save_automation_handler\n");
    
    dump_parameters(iIndex, iNumParams, pcParam, pcValue);

    i = 0;
    while (i < iNumParams)
    {
        param = pcParam[i];
        value = pcValue[i];

        if (param && value)
        {
            if (strcasecmp("x", param) == 0)
            {                
                sscanf(value, "%d", &automation_number);                 
            }   

            if (strcasecmp("afname", param) == 0)
            {
                sscanf(value, "automation%02d", &extracted_automation_number);          
            }              

            if (strcasecmp("afname", param) == 0)
            {                              
                STRNCPY(web.edit_text_filename, value, sizeof(web.edit_text_filename));  //TODO: why is this here?

                 sscanf(value, "automation%02d", &extracted_automation_number);

                 if (((automation_number < 0) || (automation_number >31)) &&
                     ((extracted_automation_number >= 0) && (extracted_automation_number <= 31)))
                 {
                    // use number extracted from file name since x parameter is invalid (this occurs on the first edit)
                    automation_number = extracted_automation_number;
                 }
            }  

            if (strcasecmp("aname", param) == 0)
            {                              
                if (cfg && automation_number>= 0 && automation_number <=31)
                {
                    urldecode(cfg->automation_name[automation_number], value);
                }
            }                       
        }

        i++;
    }


    // Send the next page back to the user
    //config_changed();
    return "/hc_automation_list.shtml";
}




static const MY_CGI_T application_cgi_handlers[] = {
    {"/cgi_hc_automation_edit.shtml",         cgi_hc_automation_edit_handler},  
    {"/cgi_hc_automation_enable.shtml",       cgi_hc_automation_enable_handler},      
    {"/cgi_hc_automation_delete.shtml",       cgi_hc_automation_delete_handler}, 
    {"/cgi_hc_automation_save.shtml",         cgi_hc_automation_save_handler},     
          
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
void application_cgi_handler(struct fs_file *file, const char* uri, int iNumParams, 
                       char **pcParam, char **pcValue, void *connection_state) 
{
    int i;
    WEB_SESSION_STATE_T *session = NULL;

    // all html files requiring cgi have file names begining with "/cgi_"
    if (strncasecmp("/cgi_", uri, 5) == 0)
    {        
        for(i=0; i<NUM_ROWS(application_cgi_handlers); i++)
        {
            //printf("checking CGI URI: %s vs %s\n", application_cgi_handlers[i].pcCGIName, uri);
            if (strcasecmp(application_cgi_handlers[i].pcCGIName, uri) == 0) 
            {
                if (application_cgi_handlers[i].pfnCGIHandler)
                {                
                    printf("calling handler for %s\n", application_cgi_handlers[i].pcCGIName);
                    application_cgi_handlers[i].pfnCGIHandler(0, iNumParams, pcParam, pcValue, (WEB_SESSION_STATE_T *)connection_state);
                }
                break;
            }
        }
    }
}

