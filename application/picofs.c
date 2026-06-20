#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "pluto.h"

// Example placeholder for your custom file system handle
typedef struct {
    bool in_use;
    // YourCustomFileHandle my_fs_handle;
} pico_fd_t;

#define MAX_CUSTOM_FDS 8
pico_fd_t custom_fds[MAX_CUSTOM_FDS];

// Hook for fopen()
int __wrap__open(const char *name, int flags, int mode) {
    // 1. Find a free slot in custom_fds
    int fd = -1;
    for (int i = 0; i < MAX_CUSTOM_FDS; i++) {
        if (!custom_fds[i].in_use) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) {
        errno = ENFILE; // Too many open files
        return -1;
    }

    // 2. Call your custom file system open logic here
    // if (my_fs_open(&custom_fds[fd].my_fs_handle, name, flags) != SUCCESS) {
    //     errno = ENOENT; // File not found
    //     return -1;
    // }

    custom_fds[fd].in_use = true;
    
    // Return index offset to avoid colliding with standard stdin/stdout/stderr (0, 1, 2)
    return fd + 3; 
}

// Hook for fread()
int __wrap__read(int fd, char *ptr, int len) 
{
    int i;

    printf("read called with len = %d\n", len);

    if (fd < 3) {
        // Handle standard input if necessary
        return 0;
    }
    
    int target_fd = fd - 3;
    if (target_fd >= MAX_CUSTOM_FDS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }

    // Call your custom file system read logic here
    // int bytes_read = my_fs_read(&custom_fds[target_fd].my_fs_handle, ptr, len);
    // return bytes_read;
    CLIP(len, 0, 20);

    for(i=0; i<len; i++)
    {
        if (i % 2)
        {
             ptr[i]=' ';
        }
        else
        {
            ptr[i]=i%256 + 'A';
        }
    }

    printf("read returning %d\n", i);

    return(i); 
}

// Hook for fwrite()
int __wrap__write(int fd, char *ptr, int len) {
    // Retain SDK standard output routing for printf via stdout/stderr (1 and 2)
    if (fd == 1 || fd == 2) {
        // Let Pico SDK handles default stdio output (UART/USB)
        extern int __wrap__write(int fd, char *ptr, int len);
        return __wrap__write(fd, ptr, len);
    }

    int target_fd = fd - 3;
    if (target_fd >= MAX_CUSTOM_FDS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }

    // Call your custom file system write logic here
    // int bytes_written = my_fs_write(&custom_fds[target_fd].my_fs_handle, ptr, len);
    // return bytes_written;

    return len;
}

// Hook for fseek()
int __wrap__lseek(int fd, int ptr, int dir) {
    int target_fd = fd - 3;
    if (fd < 3 || target_fd >= MAX_CUSTOM_FDS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }
    
    // Call your custom file system seek logic here
    // return my_fs_seek(&custom_fds[target_fd].my_fs_handle, ptr, dir);
    return 0;
}

// Hook for fclose()
int __wrap__close(int fd) {
    int target_fd = fd - 3;
    if (fd < 3 || target_fd >= MAX_CUSTOM_FDS || !custom_fds[target_fd].in_use) {
        errno = EBADF;
        return -1;
    }

    // Call your custom file system close logic here
    // my_fs_close(&custom_fds[target_fd].my_fs_handle);
    
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
