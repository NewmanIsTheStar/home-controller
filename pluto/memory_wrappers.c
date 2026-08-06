#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// Structural alignment layout matching FreeRTOS heap_4.c internal definitions
#define heapALLOCATION_BYTE_ALIGNMENT    8
#define heapMINIMUM_BLOCK_SIZE          ( ( size_t ) ( xHeapStructSize << 1 ) )

typedef struct A_BLOCK_LINK {
    struct A_BLOCK_LINK *pxNextFreeBlock;
    size_t xBlockSize;
} BlockLink_t;

static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( heapALLOCATION_BYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) ( heapALLOCATION_BYTE_ALIGNMENT - 1 ) );
#define heapBLOCK_SIZE_IS_ALLOCATED( xBlockSize )    ( ( ( xBlockSize ) & ( ~ ( ( size_t ) 0 ) >> 1 ) ) == ( xBlockSize ) )
#define heapBLOCK_SIZE_MASK                          ( ~ ( ( size_t ) 0 ) >> 1 )
// Wrapper for malloc
void* __wrap_malloc(size_t size) {
    return pvPortMalloc(size);
}

// Wrapper for free
void __wrap_free(void* ptr) {
    vPortFree(ptr);
}

// Wrapper for calloc (FreeRTOS does not have a native calloc)
void* __wrap_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void* ptr = pvPortMalloc(total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

// // Wrapper for realloc (FreeRTOS does not have a native realloc)
// void* __wrap_realloc(void* ptr, size_t size) {
//     if (size == 0) {
//         vPortFree(ptr);
//         return NULL;
//     }
//     if (ptr == NULL) {
//         return pvPortMalloc(size);
//     }

//     // Allocate new space
//     void* new_ptr = pvPortMalloc(size);
//     if (new_ptr != NULL) {
//         // Note: FreeRTOS heap managers do not store an accessible block size,
//         // so this copy assumes the new size constraint. To optimize or prevent 
//         // out-of-bounds reading, consider custom tracking if using Heap_4 or Heap_5.
//         memcpy(new_ptr, ptr, size); 
//         vPortFree(ptr);
//     }
//     return new_ptr;
// }

// Wrapper for realloc when using FreeRTOS heap4
void* __wrap_realloc(void* ptr, size_t size) {
    if (size == 0) {
        vPortFree(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        return pvPortMalloc(size);
    }

    // Allocate new block
    void* new_ptr = pvPortMalloc(size);
    if (new_ptr != NULL) {
        // Step back from the user pointer to find the FreeRTOS BlockLink_t header
        uint8_t *puc = (uint8_t *) ptr;
        puc -= xHeapStructSize;
        BlockLink_t *pxLink = (BlockLink_t *) puc;

        // Extract the actual usable size of the old allocation
        size_t old_usable_size = (pxLink->xBlockSize & heapBLOCK_SIZE_MASK) - xHeapStructSize;
        
        // Copy only what fits into the smaller of the two blocks
        size_t copy_size = (old_usable_size < size) ? old_usable_size : size;
        
        memcpy(new_ptr, ptr, copy_size);
        vPortFree(ptr);
    }
    return new_ptr;
}

// # The Pico SDK's CMake build system inherently applies the --wrap flag to standard memory routines at compile time.

// # The Target:
// # When you call standard malloc(), the linker quietly translates it into __wrap_malloc().

// # The Memory Pool:
// # This wrapped function draws memory from a continuous block of unallocated runtime SRAM calculated by the linker file
// #  (memmap_default.ld), utilizing all remaining RAM left over after accounting for your code's binary size, 
// #  global/static variables (.data and .bss), and hardware stacks.

// # When you introduce FreeRTOS to a Pi Pico C SDK project, the default wrapping creates a famous architectural conflict.

// # The Dilemma: 
// # If you implement FreeRTOS's popular heap_4.c or heap_5.c schemes, the kernel expects to claim a chunk of
// #  memory for pvPortMalloc() via its own arrays. Meanwhile, your standard library functions and C++ new/delete keywords
// #   will continue to blindly look at the Pico SDK's wrapped heap.

// # The Consequence:
// # You end up with two competing memory managers wasting and fragmenting the Pico’s 264KB of SRAM. Even worse, if you
// #  try to use heap_3.c (which forwards FreeRTOS calls to standard malloc), the overlapping mutex locks from both the
// #   Pico SDK and FreeRTOS can conflict and cause a core deadlock.

// #   Setting these flags strips away the SDK's default wrapper hooks. This allows you to either rely entirely on standard
// #  Newlib memory with a properly tuned thread-safe integration layer or manually map __wrap_malloc pointers directly
// #   over to FreeRTOS's pvPortMalloc.


// # # Disable the Pico SDK's default memory and C++ allocation overrides
// # add_compile_definitions(SKIP_PICO_MALLOC=1)
// # add_compile_definitions(PICO_CXX_DISABLE_ALLOCATION_OVERRIDES=1)