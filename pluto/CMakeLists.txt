target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    pluto_main.c
    pluto_wifi.c    
    pluto_malloc.c
    pluto_boot.c
    pluto_uf2.c
    system_cgi.c
    # pluto_terminal.c
    system_web.c    
    system_config.c
    system_ssi.c
    swload_task.c
)

add_subdirectory(shell)
add_subdirectory(picofs)
add_subdirectory(config) 
add_subdirectory(dhcp)
add_subdirectory(dns)
add_subdirectory(ping)
add_subdirectory(calendar)
add_subdirectory(udp)
add_subdirectory(utility)
add_subdirectory(watchdog)
add_subdirectory(basic)


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
    "-Wl,--wrap=_malloc_r"
    "-Wl,--wrap=_free_r"
    "-Wl,--wrap=_calloc_r"
    "-Wl,--wrap=_realloc_r"
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
        hardware_uart
        pico_httpd_content
        # pico_stdio_usb
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

# ******************************************************
# ***generate embedded file system for the web server***
# ******************************************************

# define a flat staging directory inside the build folder
set(WEB_STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/web_staging")

# create the staging folder during configuration
file(MAKE_DIRECTORY "${WEB_STAGING_DIR}")

# add pluto web files
file(GLOB ALL_SOURCE_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml"
    "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.ico"
)
# check if the application directory exists
set(APP_HTML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../application/html_files")
if(EXISTS "${APP_HTML_DIR}")
    # add the application web files if present
    file(GLOB APP_FILES 
        "${APP_HTML_DIR}/*.shtml"
        "${APP_HTML_DIR}/*.ico"
    )
    list(APPEND ALL_SOURCE_FILES ${APP_FILES})
endif()

set(STAGED_FILES)

# copy each web file into the staging directory
foreach(SRC_FILE ${ALL_SOURCE_FILES})
    get_filename_component(FILE_NAME "${SRC_FILE}" NAME)
    set(DEST_FILE "${WEB_STAGING_DIR}/${FILE_NAME}")
    
    add_custom_command(
        OUTPUT "${DEST_FILE}"
        COMMAND ${CMAKE_COMMAND} -E copy "${SRC_FILE}" "${DEST_FILE}"
        DEPENDS "${SRC_FILE}"
        COMMENT "Staging ${FILE_NAME} for lwIP"
    )
    list(APPEND STAGED_FILES "${DEST_FILE}")
endforeach()

# create a custom target so CMake actually runs the copy commands
add_custom_target(stage_web_files DEPENDS ${STAGED_FILES})

# generate the HTTPD content using the staged files list
pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${STAGED_FILES})

# ensure the staging completes before the Pico tool runs
add_dependencies(pico_httpd_content_pico_set_lwip_httpd_content stage_web_files)
