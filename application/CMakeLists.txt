# # source files
# target_sources(${PROJECT_NAME} PRIVATE
#         worker_tasks.c        
#         hc_task.c
#         discovery_task.c
#         shelly.c 
#         shelly_discovery.c
#         shelly_monitor.c
#         json_parser.c
#         application_cgi.c   
#         application_ssi.c
#         kasa_discovery.c 
#         application_config.c  
#         application_web.c     
# )

# # headers
# target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# # picofs library
# target_link_libraries(${PROJECT_NAME} PRIVATE project::picofs)


# INTERFACE library
add_library(application INTERFACE)
add_library(project::application ALIAS application)

# source files
target_sources(application INTERFACE
    
    ${CMAKE_CURRENT_LIST_DIR}/worker_tasks.c 
    ${CMAKE_CURRENT_LIST_DIR}/hc_task.c 
    ${CMAKE_CURRENT_LIST_DIR}/discovery_task.c 
    ${CMAKE_CURRENT_LIST_DIR}/shelly.c 
    ${CMAKE_CURRENT_LIST_DIR}/shelly_discovery.c 
    ${CMAKE_CURRENT_LIST_DIR}/shelly_monitor.c 
    ${CMAKE_CURRENT_LIST_DIR}/json_parser.c 
    ${CMAKE_CURRENT_LIST_DIR}/application_cgi.c 
    ${CMAKE_CURRENT_LIST_DIR}/application_ssi.c 
    ${CMAKE_CURRENT_LIST_DIR}/kasa_discovery.c 
    ${CMAKE_CURRENT_LIST_DIR}/application_config.c 
    ${CMAKE_CURRENT_LIST_DIR}/application_web.c     
)

# headers
target_include_directories(application INTERFACE 
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