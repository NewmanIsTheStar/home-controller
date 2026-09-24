target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    utility_crc.c
    utility_govee.c
    utility_gpio.c 
    utility_pluto_msg.c 
    utility_primative.c 
    utility_socket.c 
    utility_string.c 
    utility_syslog.c 
)

