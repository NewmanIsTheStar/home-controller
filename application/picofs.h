#ifndef PICOFS_H
#define PICOFS_H

#include <stddef.h>
#include "shell.h"

// output to either terminal or http shell
//#define picofs_printf(format, ...) printf("[MACRO] " format, ##__VA_ARGS__)
#define picofs_printf(format, ...) shell_printf(format, ##__VA_ARGS__)
#define FS_MAX_FILE_DESCRIPTORS (8)
#define FS_FLASH_START ((char *)(&test_filesystem))
#define FS_FLASH_END ((char *)(&test_filesystem) + sizeof(test_filesystem))
#define FS_VERION (0)

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4
#define MAP_SHARED  1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON    MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)

typedef struct file_header
{
    u8_t magic_number[4];   // "pfs"
    u8_t picofs_version;
    u8_t file_id;     
    u8_t file_sequence;
    u8_t file_status;
    u32_t file_size;        
    u32_t crc;
    char name[16];
} FILE_TRAILER_T;

typedef struct 
{
    bool in_use;
    int flags;                    // flags passed to open file
    char *file;                   // flash: file start
    size_t file_len;              // flash: length of file including trailer   
    u8_t file_status;             // flash: file status  
    char *cache;                  // RAM: file start
    size_t cache_len;             // RAM: cache size
    FILE_TRAILER_T cache_trailer; // holds trailer while file is being written to cache
    FILE_TRAILER_T *file_trailer; // flash or RAM: file trailer
    char *data;                   // flash or RAM: data contained in the file 
    size_t data_len;              // flash or RAM: data length
    size_t data_offset;           // flash or RAM: offset used by standard C library functions e.g. fread     
} PICOFS_FD_T;

typedef struct file_metrics
{
    bool valid;  
    FILE_TRAILER_T *trailer;
} FILE_METRICS_T;

typedef struct file_test
{
    u8_t test_data[224];
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
int picofs_find_by_name(const char *filename, FILE_TRAILER_T **trailer);
int picofs_list_all_files(void);
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display);
int picofs_find_contiguous_free_area(size_t size, u8_t **start_of_area);
bool picofs_file_in_use(FILE_TRAILER_T *file_trailer);
int picofs_fd_initialize(int fd, int flags, FILE_TRAILER_T *trailer);
int picofs_allocate_cache(int fd);
int picofs_deallocate_cache(int fd);
int picofs_open(int fd, const char *name, int flags);
int picofs_read(int fd, char *ptr, int len);
int picofs_write(int fd, char *ptr, int len);
int picofs_create_file_trailer(int fd, const char *name);
int picofs_close(int fd);
int picofs_delete(char *filename);
int picofs_copy(const char *src, const char *dst);
int picofs_is_latest_file_sequence(char *filename, u8_t file_id, u8_t sequence);
u8_t picofs_get_new_file_id(void);
bool picofs_is_file_deleted(u8_t file_id);
int picofs_unlink(const char *name);
int picofs_rename(const char *src, const char *dst);
int picofs_list_files_by_size(void);
void *picofs_mmap(void *addr, size_t len, int prot, int flags, int fd, u32_t offset);
int picofs_munmap(void *addr, size_t len);

#endif
