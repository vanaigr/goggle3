cmake_minimum_required(VERSION 3.28)

project(Goggle3 VERSION 1.0 LANGUAGES CXX)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS_DEBUG "-g")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-DGLEW_STATIC)

file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# TODO: use `missing-designated-field-initializers` for clang
add_compile_options(-Werror=missing-field-initializers)
add_compile_options(-fdiagnostics-color=always)

add_executable(${PROJECT_NAME} ${SOURCES})

find_package(PkgConfig REQUIRED)

pkg_check_modules(FREETYPE REQUIRED freetype2)
target_include_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARIES})

# libcurl4-openssl-dev
pkg_check_modules(LIBCURL REQUIRED libcurl)
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBCURL_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${LIBCURL_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBCURL_LIBRARIES})

target_link_libraries(${PROJECT_NAME} PRIVATE X11 GL GLEW)
