# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_sources(${PROJECT_NAME} PRIVATE
#     system_cgi.c
#     system_config.c
#     system_ssi.c 
#     system_web.c
# )


# INTERFACE library
add_library(system INTERFACE)
add_library(project::system ALIAS system)

# source files
target_sources(system INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/system_cgi.c  
    ${CMAKE_CURRENT_LIST_DIR}/system_config.c  
    ${CMAKE_CURRENT_LIST_DIR}/system_ssi.c
    ${CMAKE_CURRENT_LIST_DIR}/system_web.c                          
)

# headers
target_include_directories(system INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(system INTERFACE
    pluto_core 
    utility
    application
    config
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
    basic
)

