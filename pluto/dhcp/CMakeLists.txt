# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_sources(${PROJECT_NAME} PRIVATE
#     dhcpserver.c
# )

# INTERFACE library
add_library(dhcp INTERFACE)
add_library(project::dhcp ALIAS dhcp)

# source files
target_sources(dhcp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/dhcpserver.c      
)

# headers
target_include_directories(dhcp INTERFACE 
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