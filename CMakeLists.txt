cmake_minimum_required(VERSION 3.20)

# vcpkg toolchain - adjust path if your vcpkg is elsewhere
if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(PixelEraserPro VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CanvasWidget.cpp
    src/ImageProcessor.cpp
    src/ToolManager.cpp
    src/HistoryManager.cpp
    src/ExportDialog.cpp
    src/ResizeDialog.cpp
)

# Header files
set(HEADERS
    include/MainWindow.h
    include/CanvasWidget.h
    include/ImageProcessor.h
    include/ToolManager.h
    include/HistoryManager.h
    include/ExportDialog.h
    include/ResizeDialog.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Windows icon
if(WIN32)
    set(APP_ICON_RESOURCE resources/app.rc)
endif()

# Executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    ${APP_ICON_RESOURCE}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${OpenCV_LIBS}
)

# Windows specific
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
