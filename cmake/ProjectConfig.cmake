# Project Configuration
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress developer warnings to avoid FFmpeg package name mismatch warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "" FORCE)

# Module options for extensibility
option(BUILD_HUMAN_RECOGNITION "Build Human Recognition module" ON)
option(BUILD_AUDIO_VIDEO "Build Audio Video module" ON)
option(BUILD_MQTT_CLIENT "Build MQTT Client module" ON)
option(BUILD_SHARED_MODULES "Build modules as shared libraries" ON)
