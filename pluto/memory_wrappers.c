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

