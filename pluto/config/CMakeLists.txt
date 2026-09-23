target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    config_change.c
    config_file.c
    config_read.c
    config_validate.c
    config_write.c 
)

# # INTERFACE library
# add_library(config INTERFACE)
# add_library(project::config ALIAS config)

# # source files
# target_sources(config INTERFACE
#     ${CMAKE_CURRENT_LIST_DIR}/config_change.c  
#     ${CMAKE_CURRENT_LIST_DIR}/config_file.c  
#     ${CMAKE_CURRENT_LIST_DIR}/config_read.c
#     ${CMAKE_CURRENT_LIST_DIR}/config_validate.c
#     ${CMAKE_CURRENT_LIST_DIR}/config_write.c          
# )

# # headers
# target_include_directories(config INTERFACE 
#     ${CMAKE_CURRENT_LIST_DIR}
# )
