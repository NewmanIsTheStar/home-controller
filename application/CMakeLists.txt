# source files
target_sources(${PROJECT_NAME} PRIVATE
        worker_tasks.c        
        hc_task.c
        discovery_task.c
        shelly.c 
        shelly_discovery.c
        shelly_monitor.c
        json_parser.c
        # basic_commands.c
        # basic_extensions.c
        # basic_interpreter.c
        # basic_parser.c                  
        ssi.c
        application_cgi.c   
        # shell_http.c   
        # shell_message.c
        # shell_cat.c
        # shell_hexdump.c
        # shell_list.c
        # shell_stat.c
        # shell_page_map.c
        # shell_defragment.c
        # shell_cleanup.c
        # shell_unlink.c
        # shell_copy.c
        # shell_move.c
        # shell_ping.c
        # shell_boot.c
        # shell_download.c
        # shell_basic.c
        kasa_discovery.c 
        application_config.c       
)

# headers
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# picofs library
target_link_libraries(${PROJECT_NAME} PRIVATE project::picofs)

# web server ***MOVED TO top level CMakeLists.txt
# set(WEB_FILES)
# file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml")
# pico_add_library(pico_httpd_content NOFLAG)
# pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${WEB_FILES})

# # add html files to global property to be picked up by top level CMakeLists.txt
# file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml")
# set_property(GLOBAL PROPERTY GLOBAL_WEB_FILES APPEND PROPERTY GLOBAL_WEB_FILES ${WEB_FILES})
