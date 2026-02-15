cmake_minimum_required(VERSION 3.18)

project("MyQtApp" VERSION 11.45.14)

message("-- CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message("-- CMAKE_C_COMPILER: ${CMAKE_C_COMPILER} (${CMAKE_C_COMPILER_ID})")
message("-- CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")
message("-- CMAKE_CXX_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-builtin-macro-redefined)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_VERSION_MAJOR 6)

set(BOOST_ROOT "E:/mingw64/include/boost_1_89_0")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(fmt REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets REQUIRED)

# 重新定义__FILE__宏显示相对于项目根目录的路径
function(redefine_file_marco targetname)
    get_target_property(source_files "${targetname}" SOURCES)

    foreach(source_file ${source_files})
        get_property(defs SOURCE ${source_file} PROPERTY COMPILE_DEFINITIONS)
        get_filename_component(file_path "${source_file}" ABSOLUTE)
        string(REPLACE "${PROJECT_SOURCE_DIR}/" "" relpath "${file_path}")
        message("-- Setting __FILE__ for ${source_file} to \"${relpath}\"")
        if(defs)
            set(defs "${defs};__FILE__=\"${relpath}\"")
        else()
            set(defs "__FILE__=\"${relpath}\"")
        endif()
        set_property(SOURCE "${source_file}" PROPERTY COMPILE_DEFINITIONS "${defs}")
    endforeach()
endfunction()

file(GLOB SRC_FILES "src/**/*.cpp")
file(GLOB INC_FILES "include/**/*.h")

add_executable(
    ${PROJECT_NAME}

    main.cpp
    src/emmcdl_new/serialport.cpp
    src/emmcdl_new/crc.cpp
    src/emmcdl_new/xmlparser.cpp
    src/emmcdl_new/partition.cpp
    src/emmcdl_new/protocol.cpp
    src/emmcdl_new/sparse.cpp
    src/emmcdl_new/diskwriter.cpp
    src/emmcdl_new/dload.cpp
    src/emmcdl_new/ffu.cpp
    src/emmcdl_new/firehose.cpp
    src/emmcdl_new/sahara.cpp
    src/emmcdl_new/emmcdl.cpp
    src/emmcdl_new/utils.cpp

    src/spddump/BMPlatform.cpp
    src/spddump/common.cpp
    src/spddump/spd_dump.cpp
    src/spddump/Wrapper.cpp

    src/datatypes/redirector.cpp
    src/datatypes/thread.cpp
    src/datatypes/bytearray.cpp

    src/utils/exception_handler.cpp
    src/utils/logger.cpp
    src/utils/string_utils.cpp
    src/utils/time_utils.cpp
    src/utils/usb_utils.cpp

    include/widgets/basic/MyMainWindow.h
    src/widgets/basic/MyMainWindow.cpp
    include/widgets/basic/MyWidget.h
    src/widgets/basic/MyWidget.cpp
    
)

target_include_directories(
    ${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/emmcdl_new
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    fmt::fmt
    setupapi
)

