# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_sources(${PROJECT_NAME} PRIVATE
#     utility_crc.c
#     utility_govee.c
#     utility_gpio.c 
#     utility_pluto_msg.c 
#     utility_primative.c 
#     utility_socket.c 
#     utility_string.c 
#     utility_syslog.c 
# )

# INTERFACE library
add_library(utility INTERFACE)
add_library(project::utility ALIAS utility)

# source files
target_sources(utility INTERFACE
    
    ${CMAKE_CURRENT_LIST_DIR}/utility_crc.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_govee.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_gpio.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_pluto_msg.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_primative.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_socket.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_string.c 
    ${CMAKE_CURRENT_LIST_DIR}/utility_syslog.c 
)

# headers
target_include_directories(utility INTERFACE 
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