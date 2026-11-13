# INTERFACE library
add_library(dns INTERFACE)
add_library(project::dns ALIAS dns)

# source files
target_sources(dns INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dnsserver.c      
)

# headers
target_include_directories(dns INTERFACE 
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