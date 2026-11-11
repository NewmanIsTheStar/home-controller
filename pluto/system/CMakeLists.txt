target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    system_cgi.c
    system_config.c
    system_ssi.c 
    system_web.c
)
