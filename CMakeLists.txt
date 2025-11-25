cmake_minimum_required(VERSION 3.5)

project(Detection LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenVINO REQUIRED)

set(RESOURCES
    res.qrc
)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/ui)

if(ANDROID)
  add_library(Detection SHARED
    src/main.cpp
    src/mainwindow.cpp
    include/mainwindow.h
    src/buffdetector.cpp
    include/buffdetector.h
    src/mediaprocessor.cpp
    include/mediaprocessor.h
    ui/mainwindow.ui
    ${RESOURCES}
  )
else()
  add_executable(Detection
    src/main.cpp
    src/mainwindow.cpp
    include/mainwindow.h
    src/buffdetector.cpp
    include/buffdetector.h
    src/mediaprocessor.cpp
    include/mediaprocessor.h
    ui/mainwindow.ui
    ${RESOURCES}
  )
endif()

target_link_libraries(Detection PRIVATE Qt5::Widgets)
target_link_libraries(Detection PRIVATE ${OpenCV_LIBS})
target_link_libraries(Detection PRIVATE openvino::runtime)

target_include_directories(Detection PUBLIC ${OpenCV_INCLUDE_DIRS})
target_include_directories(Detection PUBLIC ${OpenVINO_INCLUDE_DIRS})
