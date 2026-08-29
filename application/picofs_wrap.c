//#define _GNU_SOURCE 
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
// #include "pluto.h"
// #include "picofs.h"
//#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "hardware/pio.h"
#include "hardware/clocks.h"
// #include "generated/ws2812.pio.h"

// Prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>


#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#include "stdarg.h"

// #include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
// #include "led_strip.h"
#include "udp.h"
// #include "message.h"
// #include "message_defs.h"
// #include "powerwall.h"
#include "shelly.h"
#include "discovery_task.h"
#include "picofs.h"

extern SemaphoreHandle_t picofs_mutex;

PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];

// Declare the real underlying write function provided by the toolchain/SDK
extern int __real__write(int fd, const char *ptr, int len);
extern int __real__read(int fd, char *ptr, int len);

// Hook for fopen()
int __wrap__open(const char *name, int flags, int mode) 
{
    int fd = -1;
    
    if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        // find a free slot in custom_fds
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
            xSemaphoreGive(picofs_mutex);
            return -1;
        }

        custom_fds[fd].in_use = true;

        if (picofs_open_by_name(fd, (char *)name, flags))
        {
            errno = ENOENT; // File not found
            custom_fds[fd].in_use = false;
            xSemaphoreGive(picofs_mutex);
            return -1;
        }

        xSemaphoreGive(picofs_mutex);
    }
    
   

    // Return index offset to avoid colliding with standard stdin/stdout/stderr (0, 1, 2)
    return fd + 3; 
}

// Hook for open()  -- forwards to __wrap__open
int __wrap_open(const char *name, int flags, ...) 
{
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return __wrap__open(name, flags, mode);
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
        if (fd == 0) 
        {
            // Forward stdin directly to the original SDK handler (UART/USB CDC)
            return __real__read(fd, ptr, len);
        }
        else
        {                
            return 0;
        }
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
        // forward directly to the original SDK/Newlib handler
        return __real__write(fd, ptr, len);
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
    if (fd < 3 || target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) 
    {
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
int __wrap__close(int fd) 
{
    int target_fd = fd - 3;
    
    if (fd < 3 || target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) 
    {
        errno = EBADF;
        return -1;
    }

    if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {    
        picofs_close(target_fd);
    
        custom_fds[target_fd].in_use = false;

        xSemaphoreGive(picofs_mutex);
    }

    return 0;
}

// Hook for close()
int __wrap_close(int fd) 
{
    return __wrap__close(fd); 
}

// Hook for fstat()
int __wrap__fstat(int fd, struct stat *st) 
{
    st->st_mode = S_IFREG; // Flag as a regular file
    return 0;
}

int _isatty(int fd) 
{
    if (fd < 3) return 1; // Stdin, stdout, stderr are tty devices
    return 0;
}

// Hook for unlink
int __wrap__unlink(const char *name) 
{
    int err = -1;

    if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {  
        err = picofs_unlink_by_name(name);

        xSemaphoreGive(picofs_mutex);
    }

    return(err);
}

// Hook for rename
int __wrap_rename(const char *old_path, const char *new_path) 
{

    return(picofs_rename(old_path, new_path));  // NB picofs_rename takes care of locking internally

    return 0;
}

// Hook for stat
int _stat(const char *filepath, struct stat *st) 
{    
    st->st_size = picofs_get_file_size((char *)filepath);

    return 0;
}


// Hook for ftruncate
int __wrap_ftruncate(int fd, off_t length) 
{
    int target_fd = fd - 3; 
    int result = 0;

    if (fd < 3 || target_fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[target_fd].in_use) 
    {
        errno = EBADF;
        return -1;
    }

    if (length < 0) 
    {
        errno = EINVAL; // Invalid argument
        return -1;
    }

    result = picofs_ftruncate(target_fd, length);
    
    if (result != 0) 
    {
        errno = EIO; // Or map to a more specific error like ENOSPC (No space left)
        return -1;
    }

    return 0; // Success
}

// TODO --- support reentrant version too
// #include <unistd.h>
// #include <reent.h>
// #include <errno.h>

// /**
//  * 1. The Internal Newlib Reentrant Wrapper
//  * This gets hit by internal Newlib operations.
//  */
// int __wrap__ftruncate_r(struct _reent *r, int fd, off_t length) {
//     // ⚠️ TODO: Map 'fd' to your specific file system handler (e.g., LittleFS, FatFS)
//     // Example pseudocode:
//     // if (!is_valid_fd(fd)) {
//     //     r->_errno = EBADF;
//     //     return -1;
//     // }
//     //
//     // int result = your_fs_truncate(fd, length);
//     // if (result != 0) {
//     //     r->_errno = EINVAL; // Or appropriate error mapping
//     //     return -1;
//     // }

//     return 0; // Return 0 on success
// }

// /**
//  * 2. The Standard POSIX Wrapper
//  * This gets hit if your application code calls ftruncate(fd, length) directly.
//  */
// int __wrap_ftruncate(int fd, off_t length) {
//     // Forward the call directly to the reentrant wrapper using the current context
//     return __wrap__ftruncate_r(_REENT, fd, length);
// }