# INTERFACE library
add_library(udp INTERFACE)
add_library(project::udp ALIAS udp)

# source files
target_sources(udp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/udp.c                              
)

# headers
target_include_directories(udp INTERFACE 
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
