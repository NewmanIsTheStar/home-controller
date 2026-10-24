#ifndef SHELL_MESSAGE_H
#define SHELL_MESSAGE_H

typedef enum 
{
    SHELL_MSG_UNKNOWN = 0,
    SHELL_MSG_BASIC_INTERACTIVE,
    SHELL_MSG_BASIC_SCRIPT,
    SHELL_MSG_LIGHTS,
    SHELL_MSG_DUMP_PROGRAM,
    SHELL_MSG_LIST,
    SHELL_MSG_LIST_ALL,
    SHELL_MSG_PING,
    SHELL_MSG_PAGE_MAP,
    SHELL_MSG_PAGE_NUMBERS,
    SHELL_MSG_SAVE_TEXT_FILE,
    SHELL_MSG_DEFRAGMENT,
    SHELL_MSG_CLEANUP,
    SHELL_MSG_BASIC_FILE,
    SHELL_MSG_CAT_FILE,
    SHELL_MSG_SHELLY_DEVICE_DUMP,
    SHELL_MSG_HEXDUMP_FILE,
    SHELL_MSG_UNLINK,
    SHELL_MSG_DOWNLOAD_FILE,
    SHELL_MSG_VERIFY_FILE,
    SHELL_MSG_BOOT,
    SHELL_MSG_STAT,
    SHELL_MSG_COPY,
    SHELL_MSG_MOVE,
    SHELL_MSG_HEAP,
    SHELL_MSG_BASIC_KILL,    

    SHELL_MSG_BE_FOUR_BYTES      =   4294967295     // force enum to be 4 bytes long
} SHELL_MSG_T;


typedef struct 
{
    SHELL_MSG_T message_id;
    char filename[16];
} SM_GENERIC_FILE_T;

typedef struct 
{
    SHELL_MSG_T message_id;
    char src[16];
    char dst[16];    
} SM_GENERIC_DST_SRC_T;

typedef struct 
{
    SHELL_MSG_T message_id;
    char url[128];
} SM_GENERIC_URL_T;

typedef struct 
{
    SHELL_MSG_T message_id;
    char address[24];
} SM_PING_T;

int shell_queue_cat(char *filename);
int shell_do_cat(SM_GENERIC_FILE_T *msg);
int shell_queue_hexdump(char *filename);
int shell_do_hexdump(SM_GENERIC_FILE_T *msg);
int shell_queue_list(char *filename);
int shell_do_list(SM_GENERIC_FILE_T *msg);
int shell_queue_list_all(char *filename);
int shell_do_list_all(SM_GENERIC_FILE_T *msg);
int shell_queue_stat(char *filename);
int shell_do_stat(SM_GENERIC_FILE_T *msg);
int shell_queue_page_map(SHELL_MSG_T msg_id);
int shell_do_page_map(SM_GENERIC_FILE_T *msg);
int shell_queue_defragment(void);
int shell_do_defragment(SM_GENERIC_FILE_T *msg);
int shell_queue_cleanup(void);
int shell_do_cleanup(SM_GENERIC_FILE_T *msg);
int shell_queue_unlink(char *filename);
int shell_do_unlink(SM_GENERIC_FILE_T *msg);
int shell_queue_copy(char *dst, char *src);
int shell_do_copy(SM_GENERIC_DST_SRC_T *msg);
int shell_queue_move(char *dst, char *src);
int shell_do_move(SM_GENERIC_DST_SRC_T *msg);
int shell_queue_ping(char *address);
int shell_do_ping(SM_PING_T *msg);
int shell_queue_boot(char *filename);
int shell_do_boot(SM_GENERIC_FILE_T *msg);
int shell_queue_download(char *address);
int shell_do_download(SM_GENERIC_URL_T *msg);
int shell_queue_verify(char *address);
int shell_do_verify(SM_GENERIC_URL_T *msg);
int shell_queue_basic_file(char *filename);
int shell_do_basic_file(SM_GENERIC_FILE_T *msg);
int shell_queue_basic_script(char *filename);
int shell_do_basic_script(SM_GENERIC_FILE_T *msg);
int shell_queue_basic_interactive(char *filename);
int shell_do_basic_interactive(SM_GENERIC_FILE_T *msg);
int shell_queue_save_text_file(void);
int shell_do_save_text_file(SM_GENERIC_FILE_T *msg);
int shell_queue_heap(void);
int shell_do_heap(SM_GENERIC_FILE_T *msg);
int shell_queue_basic_kill(void);
int shell_do_basic_kill(SM_GENERIC_FILE_T *msg);

#endif
