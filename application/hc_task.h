
#ifndef HC_TASK_H
#define HC_TASK_H

//#define SOCKADDR_LEN sizeof(struct sockaddr)

#define HC_CMD_UNKNOWN  (0)
#define HC_CMD_BASIC_INTERACTIVE (1)
#define HC_CMD_BASIC_SCRIPT (2)


typedef enum
{
    INPUT_BOOLEAN  = 0,
    INPUT_INTEGER  = 1,
    INPUT_STRING   = 2,
    INPUT_UNKNOWN  = 4294967295,   //INT_MAX inadequate 
} INPUT_TYPE_T;

typedef struct DEVICE_INPUT_STRUCT
{
    int device_id;
    int input_id;
    INPUT_TYPE_T input_type;
    int tracked_value_index;
} DEVICE_INPUT_T;

typedef struct TRACKED_BOOLEAN_STRUCT
{
    int occupancy;
    int boolean_value[32];
} TRACKED_BOOLEAN_T;

typedef struct TRACKED_INTEGER_STRUCT
{
    int occupancy;
    int interger_value[32];
} TRACKED_INTEGER_T;

typedef struct TRACKED_STRING_STRUCT
{
    int occupancy;
    int string_value[32][32];
} TRACKED_STRING_T;

typedef enum
{
    COND_NULL          = -1,
    COND_NOT           = -2,
    COND_OR            = -3,
    COND_AND           = -4,
    COND_OPEN_BRACKET  = -5,
    COND_CLOSE_BRACKET = -6,
    COND_UNKNOWN       = 4294967295,   //INT_MAX inadequate 
} CONDITION_ATOM_TYPE_T;

typedef struct TRIGGER_CONDITION_STRUCT
{
    int occupancy;
    int expresssion[32];
} TRIGGER_CONDITION_T;

void hc_task(__unused void *params);
bool hc_trigger(int *condition, int length);
bool hc_test_trigger(void);
void hc_pat_watchdog(void);
void hc_queue_send(uint8_t message);
void hc_load_basic_program(char *program, int len);

#endif