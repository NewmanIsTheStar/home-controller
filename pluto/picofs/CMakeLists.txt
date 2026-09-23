# INTERFACE library
add_library(picofs INTERFACE)
add_library(project::picofs ALIAS picofs)

# source files
target_sources(picofs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/picofs_wrap.c  
    ${CMAKE_CURRENT_LIST_DIR}/picofs_read.c  
    ${CMAKE_CURRENT_LIST_DIR}/picofs_open.c
    ${CMAKE_CURRENT_LIST_DIR}/picofs_write.c
    ${CMAKE_CURRENT_LIST_DIR}/picofs_close.c
    ${CMAKE_CURRENT_LIST_DIR}/picofs_copy.c   
    ${CMAKE_CURRENT_LIST_DIR}/picofs_rename.c   
    ${CMAKE_CURRENT_LIST_DIR}/picofs_unlink.c
    ${CMAKE_CURRENT_LIST_DIR}/picofs_mmap.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_flash.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_page.c
    ${CMAKE_CURRENT_LIST_DIR}/picofs_test.c  
    ${CMAKE_CURRENT_LIST_DIR}/picofs_sector.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_defrag.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_list.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_file.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_crc.c 
    ${CMAKE_CURRENT_LIST_DIR}/picofs_ftruncate.c           
)

# headers
target_include_directories(picofs INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(picofs INTERFACE 
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
)

# filesystem wrapping options
target_link_options(picofs INTERFACE
    "-Wl,--wrap=_open"
    "-Wl,--wrap=open"
    "-Wl,--wrap=_read"
    "-Wl,--wrap=_write"
    "-Wl,--wrap=_lseek"
    "-Wl,--wrap=_close"
    "-Wl,--wrap=close"    
    "-Wl,--wrap=_unlink"
    "-Wl,--wrap=rename"  
    "-Wl,--wrap=ftruncate"
    "-Wl,--wrap=_ftruncate_r"  
    "-Wl,--wrap=access"   
)
