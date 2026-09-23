# source files
target_sources(${PROJECT_NAME} PRIVATE
        worker_tasks.c        
        hc_task.c
        discovery_task.c
        shelly.c 
        shelly_discovery.c
        shelly_monitor.c
        json_parser.c
        basic_commands.c
        basic_extensions.c
        basic_interpreter.c
        basic_parser.c                  
        ssi.c
        cgi.c   
        shell.c   
        kasa_discovery.c 
        application_config.c       
)

# headers
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# picofs library
target_link_libraries(${PROJECT_NAME} PRIVATE project::picofs)

# web server 
set(WEB_FILES)
file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml")
pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${WEB_FILES})

