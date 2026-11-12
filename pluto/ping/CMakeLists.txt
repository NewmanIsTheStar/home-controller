# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_sources(${PROJECT_NAME} PRIVATE
#     ping_core.c
# )

# INTERFACE library
add_library(ping INTERFACE)
add_library(project::ping ALIAS ping)

# source files
target_sources(ping INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ping_core.c      
)

# headers
target_include_directories(ping INTERFACE 
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