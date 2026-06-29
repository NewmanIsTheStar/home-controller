#ifndef PICOFS_H
#define PICOFS_H

#include <stddef.h>
#include "shell.h"

// output to either terminal or http shell
//#define picofs_printf(format, ...) printf("[MACRO] " format, ##__VA_ARGS__)
#define picofs_printf(format, ...) shell_printf(format, ##__VA_ARGS__)

typedef struct file_header
{
    u8_t magic_number[4];   // "pfs"
    u8_t picofs_version;
    u8_t file_id;     
    u8_t file_sequence;
    u8_t file_padding;
    u32_t file_size;        
    u32_t crc;
    char name[16];
} FILE_HEADER_T;

typedef struct file_trailer
{
    u8_t magic_number[4];   // "sfp"   
    u32_t crc;
} FILE_TRAILER_T;

typedef struct file_test
{
    FILE_HEADER_T test_header;
    u8_t test_data[216];
    FILE_TRAILER_T test_trailer;
} FILE_TEST_T;

typedef enum
{
    PFS_DISPLAY_QUIET = 0,
    PFS_DISPLAY_PAGE_NUMBERS = 1,
    PFS_DISPLAY_PAGE_MAP = 2,
    PFS_DISPLAY_SHELL_PAGE_NUMBERS = 3,
    PFS_DISPLAY_SHELL_PAGE_MAP = 4    
} PFS_DISPLAY_TYPE_T;

int picofs_load_test_data(void);
int picofs_find_by_name(char *filename, char **header);
int picofs_list_all_files(void);
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display);
int picofs_find_contiguous_free_area(size_t size, u8_t **start_of_area);

#endif
