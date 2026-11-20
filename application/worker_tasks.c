/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */



#include "FreeRTOS.h"
#include "task.h"
#include "worker_tasks.h"
#include "hc_task.h"
#include "discovery_task.h"

// worker tasks to launch and monitor
WORKER_TASK_T worker_tasks[] =
{
  //  function        name                    stack   priority        
  {   hc_task,        "Home Controller Task", 2*8096, 1},      
  {   discovery_task, "Discovery Task",       8096,   1},          

  //  end of table
  {   NULL,            NULL,                  0,      0,}
};




