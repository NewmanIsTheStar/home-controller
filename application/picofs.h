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

 int picofs_load_test_data(void);
int picofs_find_by_name(char *filename, char **header);

#endif
