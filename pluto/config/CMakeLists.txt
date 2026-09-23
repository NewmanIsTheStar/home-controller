target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    config_change.c
    config_file.c
    config_read.c
    config_validate.c
    config_write.c 
)

