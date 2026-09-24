target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PRIVATE
    calendar_string.c
    calendar_sun.c
    calendar_daylight_saving.c
    calendar_webui.c
    calendar_sntp.c
    calendar_numeric.c
)

