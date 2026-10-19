/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef APPLICATION_WEB_H
#define APPLICATION_WEB_H


typedef struct APP_WEB_VARIABLES
{
  int automation_running;           
} APP_WEB_VARIABLES_T;                  //remember to add initialization code when adding to this structure !!!

typedef struct 
{
  int automation_file_number;
} WEB_SESSION_STATE_T;

typedef const char *(*tMYCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[], WEB_SESSION_STATE_T *connection_state);
typedef struct
{
    const char *pcCGIName;
    tMYCGIHandler pfnCGIHandler;
} MY_CGI_T;

int application_web_init(void);


#endif

