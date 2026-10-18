# INTERFACE library
add_library(shell INTERFACE)
add_library(project::shell ALIAS shell)

# source files
target_sources(shell INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/shell_http.c  
    ${CMAKE_CURRENT_LIST_DIR}/shell_message.c  
    ${CMAKE_CURRENT_LIST_DIR}/shell_cat.c
    ${CMAKE_CURRENT_LIST_DIR}/shell_hexdump.c
    ${CMAKE_CURRENT_LIST_DIR}/shell_list.c
    ${CMAKE_CURRENT_LIST_DIR}/shell_stat.c   
    ${CMAKE_CURRENT_LIST_DIR}/shell_page_map.c   
    ${CMAKE_CURRENT_LIST_DIR}/shell_defragment.c
    ${CMAKE_CURRENT_LIST_DIR}/shell_cleanup.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_unlink.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_copy.c
    ${CMAKE_CURRENT_LIST_DIR}/shell_move.c  
    ${CMAKE_CURRENT_LIST_DIR}/shell_ping.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_boot.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_download.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_basic.c 
    ${CMAKE_CURRENT_LIST_DIR}/shell_save_text_file.c                    
)

# headers
target_include_directories(shell INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(shell INTERFACE 
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
    basic
)

