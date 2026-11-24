cmake_minimum_required(VERSION 3.20)

# vcpkg toolchain - automatically detect from VCPKG_ROOT env var or standard C:/vcpkg
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    endif()
endif()

project(PixelEraserPro VERSION 2.0.4 LANGUAGES CXX)

# Generate version header from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
    @ONLY
)

# Generate version.txt for Inno Setup installer
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.txt "${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)
find_package(OpenCV REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CanvasWidget.cpp
    src/ImageProcessor.cpp
    src/ToolManager.cpp
    src/HistoryManager.cpp
    src/ExportDialog.cpp
    src/SettingsDialog.cpp
    src/ResizeDialog.cpp
    src/Upscaler.cpp
    src/UpscaleDialog.cpp
    src/BackgroundRemover.cpp
    src/RemoverDialog.cpp
    src/GlassWidgets.cpp
    src/UpdateChecker.cpp
    src/UpdateDialog.cpp
)

# Header files
set(HEADERS
    include/MainWindow.h
    include/CanvasWidget.h
    include/ImageProcessor.h
    include/ToolManager.h
    include/HistoryManager.h
    include/ExportDialog.h
    include/SettingsDialog.h
    include/ResizeDialog.h
    include/Upscaler.h
    include/UpscaleDialog.h
    include/BackgroundRemover.h
    include/RemoverDialog.h
    include/GlassWidgets.h
    include/UpdateChecker.h
    include/UpdateDialog.h
    include/OnnxRuntimeHelper.h
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
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${OpenCV_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    ${OpenCV_LIBS}
    onnxruntime::onnxruntime
)

# Windows specific
if(WIN32)
    target_link_libraries(${PROJECT_NAME} dwmapi)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE ON
    )

    # Copy ONNX Runtime core + CUDA EP DLLs beside the executable (beats C:\Windows\System32)
    get_target_property(_ort_release_dll onnxruntime::onnxruntime IMPORTED_LOCATION_RELEASE)
    if(_ort_release_dll)
        get_filename_component(_ort_bin_dir "${_ort_release_dll}" DIRECTORY)
        foreach(_ort_dll_name onnxruntime.dll onnxruntime_providers_shared.dll onnxruntime_providers_cuda.dll onnxruntime_providers_tensorrt.dll)
            if(EXISTS "${_ort_bin_dir}/${_ort_dll_name}")
                add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_ort_bin_dir}/${_ort_dll_name}"
                        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_ort_dll_name}"
                )
            endif()
        endforeach()
    endif()
endif()
