target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    pluto_main.c
    pluto_wifi.c    
    pluto_malloc.c
    web.c    
    system_config.c
    swload_task.c
)

add_subdirectory(picofs)
add_subdirectory(config) 
add_subdirectory(dhcp)
add_subdirectory(dns)
add_subdirectory(ping)
add_subdirectory(calendar)
add_subdirectory(udp)
add_subdirectory(utility)
add_subdirectory(watchdog)

if(PICO_BOARD STREQUAL "pico_w")
    set(FREERTOS_PORT_DIR "${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/")
else()
    set(FREERTOS_PORT_DIR "${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/include/")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${FREERTOS_PORT_DIR}
        ${PROJECT_SOURCE_DIR}/FreeRTOS-Kernel/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/common/pico_base_headers/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/common/pico_rand/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/src/include/  
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/rp2_common/pico_lwip/include/ 
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/contrib/ports/freertos/include/ 
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/rp2_common/pico_cyw43_arch/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/cyw43-driver/src/
        ${PROJECT_SOURCE_DIR}/pico-sdk/src/rp2_common/pico_cyw43_driver/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/src/apps/altcp_tls/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/mbedtls/include/
        ${PROJECT_SOURCE_DIR}/pico-sdk/lib/lwip/contrib/apps/ping/
)




# redirect memory allocation to wrappers for FreeRTOS
target_link_options(${PROJECT_NAME} PRIVATE
    "-Wl,--wrap=malloc"
    "-Wl,--wrap=free"
    "-Wl,--wrap=calloc"
    "-Wl,--wrap=realloc"
)

# output memory use 
# to determine the maximum heap size temporarily set configTOTAL_HEAP_SIZE to 0 then Region Size - Used Size = free RAM
target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--print-memory-usage")

# pluto common libraries
set(pluto_link_libraries
        pico_cyw43_arch_lwip_sys_freertos
        pico_lwip_freertos
        pico_lwip_api        
        pico_lwip_sntp
        pico_lwip_http
        pico_lwip_mqtt
        pico_stdlib
        pico_rand
        pico_base_headers
        FreeRTOS-Kernel-Heap4
        pico_flash
        hardware_pio
        hardware_i2c
        pico_httpd_content
        )

target_link_libraries(${PROJECT_NAME} PRIVATE ${pluto_link_libraries})

# create githash used instead of APP_VERSION for debug builds
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/githash.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/githash.h
    @ONLY
)
