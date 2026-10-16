# INTERFACE library
add_library(basic INTERFACE)
add_library(project::basic ALIAS basic)

# source files
target_sources(basic INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/basic_commands.c  
    ${CMAKE_CURRENT_LIST_DIR}/basic_extensions.c  
    ${CMAKE_CURRENT_LIST_DIR}/basic_interpreter.c
    ${CMAKE_CURRENT_LIST_DIR}/basic_parser.c             
)

# headers
target_include_directories(basic INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(basic INTERFACE 
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
    shell
)

