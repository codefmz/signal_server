cmake_minimum_required(VERSION 3.16)

project(signalServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LibDataChannel CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)


# 带有 QOBJECT 的类必须在 qt_add_executable 中列出，否则会出现 moc 相关的链接错误
add_executable(signalServer SignalingServer.cpp main.cpp)

target_link_libraries(signalServer PRIVATE LibDataChannel::LibDataChannel plog::plog)
target_compile_definitions(signalServer PRIVATE RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(signalServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

