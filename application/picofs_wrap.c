//#define _GNU_SOURCE 

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include "pluto.h"
#include "picofs.h"
//#include <unistd.h>

// Example placeholder for your custom file system handle

PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];


/*
name: A pointer to a null-terminated string specifying the path to the file you want to open.
flags: A bitwise OR mask (|) determining the file access mode and operational behaviors.
mode: An optional argument (typically an octal number or permission macros) used only when a new file is being created (via O_CREAT or O_TMPFILE). 
If neither flag is provided, this parameter is ignored.

Common Flags (flags)
The access mode must include exactly one of the following core options:
O_RDONLY: Open for reading only.
O_WRONLY: Open for writing only.
O_RDWR: Open for both reading and writing.

You can bitwise OR (|) these with additional file creation or status flags:
O_CREAT: Create the file if it does not exist.
O_TRUNC: Truncate the file length to 0 if it already exists and is opened for writing.
O_APPEND: Move the file offset pointer to the end of the file before every write.
O_EXCL: Ensure that this call creates the file; if the file already exists, the call fails (used alongside O_CREAT).
*/

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

    if (picofs_open(fd, (char *)name, flags))
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;
    
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

    custom_fds[target_fd].data_offset += bytes_read;

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

    if (picofs_open(fd, (char *)name, O_WRONLY))
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;

    custom_fds[fd].file_status = 1;  // mark for deletion
    custom_fds[fd].data_len = 0;     // empty file
 
    picofs_close(fd);
    
    custom_fds[fd].in_use = false;

    return 0;
}