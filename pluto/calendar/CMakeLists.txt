# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# target_sources(${PROJECT_NAME} PRIVATE
#     calendar_string.c
#     calendar_sun.c
#     calendar_daylight_saving.c
#     calendar_webui.c
#     calendar_sntp.c
#     calendar_numeric.c
# )

# INTERFACE library
add_library(calendar INTERFACE)
add_library(project::calendar ALIAS calendar)

# source files
target_sources(calendar INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/calendar_string.c  
    ${CMAKE_CURRENT_LIST_DIR}/calendar_sun.c  
    ${CMAKE_CURRENT_LIST_DIR}/calendar_daylight_saving.c  
    ${CMAKE_CURRENT_LIST_DIR}/calendar_webui.c  
    ${CMAKE_CURRENT_LIST_DIR}/calendar_sntp.c  
    ${CMAKE_CURRENT_LIST_DIR}/calendar_numeric.c                              
)

# headers
target_include_directories(calendar INTERFACE 
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
