//#define _GNU_SOURCE 

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "pluto.h"
#include "picofs.h"
//#include <unistd.h>


PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];


// Hook for fopen()
int __wrap__open(const char *name, int flags, int mode) 
{
    // find a free slot in custom_fds
    int fd = -1;
    for (int i = 0; i < FS_MAX_FILE_DESCRIPTORS; i++) 
    {
        if (!custom_fds[i].in_use) 
        {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    custom_fds[fd].in_use = true;

    if (picofs_open_by_name(fd, (char *)name, flags))
    {
        errno = ENOENT; // File not found
        custom_fds[fd].in_use = false;
        return -1;
    }

    // TEST TEST TEST setting in_use prior to calling picofs_open()
    //custom_fds[fd].in_use = true;
    
    // Return index offset to avoid colliding with standard stdin/stdout/stderr (0, 1, 2)
    return fd + 3; 
}

// Hook for fread()
/*!
 * \brief direct, unbuffered read from a low-level file descriptor
 *
 * \param fd   The file descriptor (e.g. 0 for standard input, or a value returned by open())
 * \param ptr  A pointer to the buffer memory where data will be stored
 * \param len  The maximum number of bytes to read
 * 
 * \return The number of bytes actually read (can be less than count), 0 on End-Of-File (EOF), or -1 on error
 */
int __wrap__read(int fd, char *ptr, int len) 
{
    int bytes_read = 0;
    int target_fd = fd - 3;

    if (fd < 3) 
    {
        // Handle standard input if necessary
        return 0;
    }
    
    if (target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) 
    {
        errno = EBADF;
        return -1;
    }

    bytes_read = picofs_read(target_fd, ptr, len);

    //custom_fds[target_fd].data_offset += bytes_read;  THIS IS DONE IS picofs_read()

    return(bytes_read); 
}

// Hook for fwrite()
/*!
 * \brief write from a low-level file descriptor
 *
 * \param fd   The file descriptor (e.g. 1 for standard output, or a value returned by open())
 * \param ptr  A pointer to the buffer memory where data will be stored
 * \param len  The maximum number of bytes to read
 * 
 * \return The number of bytes actually written (can be less than count), 0 on End-Of-File (EOF), or -1 on error
 */
int __wrap__write(int fd, char *ptr, int len) 
{
    int bytes_written = 0;
    int target_fd = fd - 3;    

    // retain SDK standard output routing for printf via stdout/stderr (1 and 2)
    if (fd == 1 || fd == 2) 
    {
        // Let Pico SDK handles default stdio output (UART/USB)
        extern int __wrap__write(int fd, char *ptr, int len);
        return __wrap__write(fd, ptr, len);
    }

    if (target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) 
    {
        errno = EBADF;
        return -1;
    }

    bytes_written = picofs_write(target_fd, ptr, len);

    return(bytes_written);
}

// Hook for fseek()  -- untested
/*!
 * \brief repositions the file offset of the open file fd to offset (ptr?) according to the directive whence (dir?)
 *
 * \param fd   The file descriptor (e.g., 0 for standard input, or a value returned by open())
 * \param ptr  !!!Assumed to mean offset
 * \param dir  !!!Assumed to mean whence
 * 
 * \return The number of bytes actually read (can be less than count), 0 on End-Of-File (EOF), or -1 on error
 */
int __wrap__lseek(int fd, int ptr, int dir) 
{
    int target_fd = fd - 3;
    if (fd < 3 || target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }
    
    // Call your custom file system seek logic here
    // return my_fs_seek(&custom_fds[target_fd].my_fs_handle, ptr, dir);

    // TODO: a *lot* of bounds checking and memory allocation! (i.e. seeking outside range of existing file)
    switch(dir)
    {
    default:
    case SEEK_SET:
        custom_fds[fd].data_offset = ptr;
        break;
    case SEEK_CUR:
        custom_fds[fd].data_offset += ptr;
        break;
    case SEEK_END: 
        custom_fds[fd].data_offset = custom_fds[fd].data_len + ptr;
        break;
    }

    return custom_fds[fd].data_offset;  // TODO: return -1 on error
}

// Hook for fclose()
int __wrap__close(int fd) {
    int target_fd = fd - 3;
    if (fd < 3 || target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }

    picofs_close(target_fd);
    
    custom_fds[target_fd].in_use = false;
    return 0;
}

// Hook for fstat()
int __wrap__fstat(int fd, struct stat *st) {
    st->st_mode = S_IFREG; // Flag as a regular file
    return 0;
}

int _isatty(int fd) {
    if (fd < 3) return 1; // Stdin, stdout, stderr are tty devices
    return 0;
}

int __wrap__unlink(const char *name) 
{
    return(picofs_unlink_by_name(name));
}

// Declaration of the original SDK rename function
//extern int __real_rename(const char *old_path, const char *new_path);

int __wrap_rename(const char *old_path, const char *new_path) 
{

    return(picofs_rename(old_path, new_path));

    return 0;
}



