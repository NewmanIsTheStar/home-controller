target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
        worker_tasks.c        
        hc_task.c
        discovery_task.c
        shelly.c 
        json_parser.c
        basic_commands.c
        basic_extensions.c
        basic_interpreter.c
        basic_parser.c                  
        ssi.c
        cgi.c        
)

set(WEB_FILES)
file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml")
pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${WEB_FILES})
