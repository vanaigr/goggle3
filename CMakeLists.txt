cmake_minimum_required(VERSION 3.28)

project(Goggle3 VERSION 1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES "src/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(PkgConfig REQUIRED)

pkg_check_modules(FREETYPE REQUIRED freetype2)

target_include_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${SOME_PACKAGE_CFLAGS_OTHER})

target_link_libraries(${PROJECT_NAME} PRIVATE X11 GL)
