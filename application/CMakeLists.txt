target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
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
        picofs_wrap.c  
        picofs_read.c  
        picofs_open.c
        picofs_write.c
        picofs_close.c
        picofs_copy.c   
        picofs_rename.c   
        picofs_unlink.c
        picofs_mmap.c 
        picofs_flash.c 
        picofs_page.c
        picofs_test.c  
        picofs_sector.c 
        picofs_defrag.c 
        picofs_list.c 
        picofs_file.c 
        picofs_crc.c 
        picofs_ftruncate.c             
)

set(WEB_FILES)
file(GLOB WEB_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html_files/*.shtml")
pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE ${WEB_FILES})


target_link_options(${PROJECT_NAME} PRIVATE
    "-Wl,--wrap=_open"
    "-Wl,--wrap=open"
    "-Wl,--wrap=_read"
    "-Wl,--wrap=_write"
    "-Wl,--wrap=_lseek"
    "-Wl,--wrap=_close"
    "-Wl,--wrap=close"    
    "-Wl,--wrap=_unlink"
    "-Wl,--wrap=rename"  
    "-Wl,--wrap=ftruncate"
    "-Wl,--wrap=_ftruncate_r"     
)
