# INTERFACE library
add_library(config INTERFACE)
add_library(project::config ALIAS config)

# source files
target_sources(config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/config_change.c  
    ${CMAKE_CURRENT_LIST_DIR}/config_file.c  
    ${CMAKE_CURRENT_LIST_DIR}/config_read.c
    ${CMAKE_CURRENT_LIST_DIR}/config_validate.c
    ${CMAKE_CURRENT_LIST_DIR}/config_write.c          
)

# headers
target_include_directories(config INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(system INTERFACE
    pluto_core 
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
    basic
)