# INTERFACE library
add_library(pluto_core INTERFACE)
add_library(project::pluto_core ALIAS pluto_core)

# source files
target_sources(pluto_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pluto_boot.c  
    ${CMAKE_CURRENT_LIST_DIR}/pluto_boss.c  
    ${CMAKE_CURRENT_LIST_DIR}/pluto_gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/pluto_main.c
    ${CMAKE_CURRENT_LIST_DIR}/pluto_malloc.c
    ${CMAKE_CURRENT_LIST_DIR}/pluto_swload.c   
    ${CMAKE_CURRENT_LIST_DIR}/pluto_uf2.c   
    ${CMAKE_CURRENT_LIST_DIR}/pluto_wifi_ap.c
    ${CMAKE_CURRENT_LIST_DIR}/pluto_wifi_country.c 
    ${CMAKE_CURRENT_LIST_DIR}/pluto_wifi_sta.c 
    # ${CMAKE_CURRENT_LIST_DIR}/pluto_terminal.c
    ${CMAKE_CURRENT_LIST_DIR}/pluto_test.c               
)

# headers
target_include_directories(pluto_core INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# link dependencies
target_link_libraries(pluto_core INTERFACE 
    basic
    calendar
    config
    dhcp
    dns
    picofs
    ping
    shell
    system
    udp
    utility
    watchdog
    pico_stdlib 
    hardware_pio
    hardware_flash
    hardware_dma
    pico_cyw43_arch_lwip_sys_freertos
    shell
)

# # redirect memory allocation to wrappers for FreeRTOS
target_link_options(${PROJECT_NAME} PRIVATE
    "-Wl,--wrap=malloc"
    "-Wl,--wrap=free"
    "-Wl,--wrap=calloc"
    "-Wl,--wrap=realloc"
    "-Wl,--wrap=_malloc_r"
    "-Wl,--wrap=_free_r"
    "-Wl,--wrap=_calloc_r"
    "-Wl,--wrap=_realloc_r"
)
