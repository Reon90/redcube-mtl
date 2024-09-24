cmake_minimum_required(VERSION 3.5)
project(redcube-mtl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)

find_package(CURL REQUIRED)
find_package(glm REQUIRED)

add_executable(redcube src/main.cpp)
target_include_directories(redcube PRIVATE libs/ libs/metal-cpp)
target_sources(
    redcube PRIVATE src/renderer.cpp
)

include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(redcube PRIVATE ${CURL_LIBRARIES})
target_link_libraries(redcube PRIVATE glm::glm)

target_link_libraries(redcube PRIVATE
    "-framework Metal"
    "-framework Foundation"
    "-framework MetalKit"
)

file(COPY src/shaders DESTINATION .)
