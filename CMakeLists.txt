cmake_minimum_required(VERSION 3.20)
project(Dreadcast CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Raylib 5.5
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

# EnTT v3.16.0
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.16.0
)
FetchContent_MakeAvailable(entt)

# nlohmann/json (data-driven items / abilities)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib EnTT::EnTT nlohmann_json::nlohmann_json)

# Copy assets next to the executable so font paths work when running from build/Release (etc.).
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets/ next to Dreadcast executable")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
