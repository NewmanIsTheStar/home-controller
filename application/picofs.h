#ifndef PICOFS_H
#define PICOFS_H

typedef struct file_header
{
    u8_t magic_number[4];
    u8_t picofs_version;
    u8_t file_id;     
    u8_t file_sequence;
    u8_t file_padding;
    u32_t file_size;        
    u32_t crc;
    char name[16];
} FILE_HEADER_T;

typedef enum
{
    PFS_DISPLAY_QUIET = 0,
    PFS_DISPLAY_PAGE_NUMBERS = 1,
    PFS_DISPLAY_PAGE_MAP = 2
} PFS_DISPLAY_TYPE_T;

int picofs_load_test_data(void);
int picofs_find_by_name(char *filename, char **header);
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display);

#endif
