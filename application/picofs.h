#ifndef PICOFS_H
#define PICOFS_H

#include <stddef.h>
#include "shell.h"

// set this to 1 to use a chunk of RAM to simulate flash for testing/development purposes
#define FS_FAKE_FLASH (0)

#if FS_FAKE_FLASH == 1
#define FS_SECTOR_SIZE (1024)
#define FS_TEST_ROWS (128)
#define FS_FLASH_BASE ((char *)(&test_filesystem))
#define FS_START ((char *)(&test_filesystem))
#define FS_END ((char *)(&test_filesystem) + sizeof(test_filesystem))
#define FS_SIZE (sizeof(test_filesystem))
#define FS_NUM_SECTORS (FS_SIZE/FS_SECTOR_SIZE)
#else
#define FS_SECTOR_SIZE (4096)
#define FS_FLASH_BASE (XIP_BASE)
#define FS_START ((char *)(XIP_BASE + (PICO_FLASH_SIZE_BYTES/2)))
#define FS_END ((char *)(XIP_BASE + PICO_FLASH_SIZE_BYTES - (2*FLASH_SECTOR_SIZE)))
#define FS_SIZE (FS_END - FS_START)
#define FS_NUM_SECTORS (FS_SIZE/FS_SECTOR_SIZE)
#endif

#define FS_PAGE_SIZE (256)
#define FS_ERASED_CELL_VALUE (255)
#define FS_NUM_FID (255)        // 0-254
#define FS_INVALID_FID (255)
#define FS_MAX_SEQ (255)
#define FS_MAX_FILE_DESCRIPTORS (8)

#define BIT0 (0x01)
#define BIT1 (0x02)
#define BIT2 (0x04)
#define BIT3 (0x08)
#define BIT4 (0x10)
#define BIT5 (0x20)
#define BIT6 (0x40)
#define BIT7 (0x80)

#define FS_VERION (0)
#define PROT_NONE  (0)
#define PROT_READ  (1)
#define PROT_WRITE (2)
#define PROT_EXEC  (4)
#define MAP_SHARED  (1)
#define MAP_PRIVATE (2)
#define MAP_ANONYMOUS (0x20)
#define MAP_ANON    MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)

// file_status bit definitions
#define STS_DELETED (BIT0)
#define STS_UNUSED1 (BIT1)
#define STS_UNUSED2 (BIT2)
#define STS_UNUSED3 (BIT3)
#define STS_UNUSED4 (BIT4)
#define STS_UNUSED5 (BIT5)
#define STS_UNUSED6 (BIT6)
#define STS_UNUSED7 (BIT7)

// REAL FLASH
// #define FLASH_SCAN_START (0x10000000UL)
// #define FLASH_SCAN_END (0x10000000UL + PICO_FLASH_SIZE_BYTES)

// FAKE FLASH
#define FLASH_SCAN_START FS_START
#define FLASH_SCAN_END FS_END

// print to terminal or http shell
//#define picofs_printf(format, ...) printf("[picoFS] " format, ##__VA_ARGS__)
#define picofs_printf(format, ...) shell_printf(format, ##__VA_ARGS__)

typedef struct file_trailer
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
    int mmap_ref_count;           // number of active mappings     
} PICOFS_FD_T;

typedef struct file_metrics
{
    bool valid; 
    bool pending_deletion;
    FILE_TRAILER_T *trailer;
} FILE_STATUS_T;

typedef struct file_test
{
    u8_t test_data[FS_PAGE_SIZE-sizeof(FILE_TRAILER_T)];
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

#define picofs_open(fd, name, flags)  (picofs_open_file((fd), (name), (flags), (FS_INVALID_FID), (false)))
#define picofs_open_by_name(fd, name, flags)  (picofs_open_file((fd), (name), (flags), (FS_INVALID_FID), (false)))
#define picofs_open_for_deletion_by_name(fd, name, flags)  (picofs_open_file((fd), (name), (flags), (FS_INVALID_FID), (true)))
#define picofs_open_for_deletion_by_fid(fd, fid, flags)  (picofs_open_file((fd), (NULL), (flags), (fid), (true)))
#define picofs_find_by_name(filename, trailer) (picofs_find_file((filename), (FS_INVALID_FID), (trailer)))
#define picofs_find_by_fid(fid, trailer) (picofs_find_file((NULL), (fid), (trailer)))
#define picofs_close(fid) (picofs_close_file((fid), (false)))
#define picofs_close_for_deletion(fid) (picofs_close_file((fid), (true)))
#define picofs_unlink_by_name(name) (picofs_unlink((name), (FS_INVALID_FID)))
#define picofs_unlink_by_fid(fid) (picofs_unlink((NULL), (fid)))

int picofs_unlink(const char *name, u8_t fid);
int picofs_find_file(const char *filename, u8_t fid, FILE_TRAILER_T **trailer);
int picofs_list_all_files_from_flash(bool ignore_crc);
int picofs_list_all_files_from_cache(void);
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display);
int picofs_find_contiguous_free_area(size_t requested_size, u8_t **start_of_area, size_t *actual_size);
bool picofs_file_in_use(FILE_TRAILER_T *file_trailer, int fd);
int picofs_fd_initialize(int fd, int flags, FILE_TRAILER_T *trailer);
int picofs_allocate_cache(int fd);
int picofs_deallocate_cache(int fd);
int picofs_open_file(int fd, const char *name, int flags, u8_t fid, bool disable_fid_rollover);
int picofs_read(int fd, char *ptr, int len);
int picofs_write(int fd, char *ptr, int len);
int picofs_create_file_trailer(int fd, const char *name);
int picofs_close_file(int fd, bool disable_purge);
int picofs_delete(char *filename);
int picofs_copy(const char *src, const char *dst);
int picofs_is_latest_file_sequence_from_flash(char *filename, u8_t file_id, u8_t sequence);
u8_t picofs_get_new_file_id(void);
bool picofs_is_file_deleted_from_flash(u8_t file_id);
bool picofs_is_file_deleted_from_cache(u8_t file_id);
int picofs_rename(const char *src, const char *dst);
int picofs_list_files_by_size(void);
void *picofs_mmap(void *addr, size_t len, int prot, int flags, int fd, u32_t offset);
int picofs_munmap(void *addr, size_t len);
int picofs_erase_obsolete_sectors(bool picofs_mutext_held);
// int picofs_consolidate_all_files(void);
int picofs_consolidate_all_files_in_flash(void);
int picofs_flash_erase_sector_range(int start_block, int end_block);
int picofs_consolidate_files_to_buffer(char * buffer, int len, u8_t exclude_fid);
int picofs_initialize(void);
int picofs_increment_sequence(FILE_TRAILER_T *trailer);
int picofs_purge_duplicates(char *filename, u8_t keep_fid);
int picofs_iter_next_file(FILE_TRAILER_T **current_file, bool ignore_crc);
int picofs_load_test_data(void);
int picofs_flash_program(char *dst, char *src, size_t len);
int picofs_flash_erase(char *dst, size_t len);
u8_t picofs_list_files_within_size_range(int size_lo, int size_hi, u8_t *file_id_list, int *file_size_list, int list_len);
u8_t picofs_find_file_at_location(char *search, FILE_TRAILER_T **trailer);
int picofs_refresh_files(void);
int picofs_ascending_size_compare(const void *a, const void *b);
int picofs_descending_size_compare(const void *a, const void *b);
u32_t picofs_get_start_sector(FILE_TRAILER_T *trailer);
u32_t picofs_get_end_sector(FILE_TRAILER_T *trailer);
int picofs_append_to_flash(char *dst, size_t dst_len, char *src, size_t src_len);
bool picofs_deleted_file_has_remnants_in_other_sectors(FILE_TRAILER_T *deleted_file);
bool picofs_deleted_file_ready_for_erasure(FILE_TRAILER_T *candidate_file);
void init_crc_subsystem(void);
uint32_t picofs_calculate_crc32(const uint8_t *data_ptr, size_t length);
int picofs_ftruncate(int fd, off_t length);
int picofs_generate_tab_completion_file_list(char *buffer, int len);
int picofs_get_file_size(char *filename);

#endif
