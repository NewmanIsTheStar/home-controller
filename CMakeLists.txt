cmake_minimum_required(VERSION 3.13..3.27)

# =========================================================================================
# Set release version
set(APP_VERSION "01.00.03")

# Set build type
set(CMAKE_BUILD_TYPE "Debug")
#set(CMAKE_BUILD_TYPE "Release")

# Set board type
#set(PICO_BOARD pico_w CACHE STRING "Board type")  # no longer supported!
set(PICO_BOARD pico2_w CACHE STRING "Board type")
# =========================================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =========================================================================================
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${CMAKE_CURRENT_SOURCE_DIR}/pluto/pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# =========================================================================================
# Note: The old sdk, toolchain and picotool versions above are ignored when we build using
#       the pico-sdk sub-module so they do not need to be updated.  These placeholders
#       cannot be deleted because they are required by the pi pico vscode extension.
# ========================================================================================= 

#set(CMAKE_EXECUTE_PROCESS_COMMAND_ECHO STDOUT)

# Compile definitions
add_compile_definitions(PLUTO_VER="${APP_VERSION}")
add_compile_definitions(PICO_CYW43_ARCH_FREERTOS)
add_compile_definitions(PICO_INCLUDE_RTC_DATETIME)
add_compile_definitions(FAKE_RTC)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")       
        add_compile_definitions(USE_GIT_HASH_AS_VERSION)
endif()

# Set sdk path
set(PLUTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pluto")
set(PICO_SDK_PATH "${PLUTO_DIR}/pico-sdk" CACHE PATH "Path to Pico SDK" FORCE)

# Set picotool location: i) Use system picotool or ii) build and cache picotool locally
unset(picotool_DIR CACHE)
#set(PICOTOOL_FETCH_FROM_GIT ON)

# Initialize sdk for entire project
set(SKIP_PICO_MALLOC 1)
include("${PICO_SDK_PATH}/pico_sdk_init.cmake")
project(home-controller C CXX ASM) 
pico_sdk_init()

# Create "generated" directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)

    
# =========================================================================================
# home-controller
add_executable(${PROJECT_NAME})

# Process both subdirectories
add_subdirectory(pluto)  
add_subdirectory(application)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_NAME="${PROJECT_NAME}")
target_compile_definitions(${PROJECT_NAME} PRIVATE INCORPORATE_HOME_CONTROLLER)

pico_set_program_name(${PROJECT_NAME} "${PROJECT_NAME}")
pico_set_program_version(${PROJECT_NAME} ${APP_VERSION})


# Enable/disable output over UART/USB
if(CMAKE_BUILD_TYPE STREQUAL "Release")
        pico_enable_stdio_usb(${PROJECT_NAME} 0)    # this should be 1 but it started causing random crashes at startup with some sdk versions!
        pico_enable_stdio_uart(${PROJECT_NAME} 0)        
else() 
        pico_enable_stdio_usb(${PROJECT_NAME} 0)
        pico_enable_stdio_uart(${PROJECT_NAME} 1)        
endif()

# Create uf2 
pico_add_extra_outputs(${PROJECT_NAME})
