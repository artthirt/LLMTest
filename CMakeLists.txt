cmake_minimum_required(VERSION 3.16)

project(LLMTest VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

set(GGML_NATIVE ON CACHE BOOL "" FORCE)
set(GGML_BACKEND_DL OFF CACHE BOOL "" FORCE)
set(GGML_CUDA ON CACHE BOOL "" FORCE)
#set(GGML_VULKAN ON CACHE BOOL "" FORCE)
set(GGML_OPENMP ON CACHE BOOL "" FORCE)
set(GGML_AVX ON CACHE BOOL "" FORCE)
#set(GGML_AVX2 ON CACHE BOOL "" FORCE)
#set(GGML_AVX512 ON CACHE BOOL "" FORCE)
#set(GGML_AVX512_BF16 ON CACHE BOOL "" FORCE)
#set(GGML_AVX512_VBMI ON CACHE BOOL "" FORCE)
#set(GGML_AVX512_VNNI ON CACHE BOOL "" FORCE)
#set(GGML_CUDA_FA_ALL_QUANTS ON CACHE BOOL "" FORCE)
set(GGML_CUDA_GRAPHS ON CACHE BOOL "" FORCE)
#set(GGML_WEBGPU ON CACHE BOOL "" FORCE)
#set(GGML_RPC ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_MTMD OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
#set(LLAMA_LLGUIDANCE ON CACHE BOOL "" FORCE)
add_subdirectory(llama)

add_executable(LLMTest
    ${PROJECT_SOURCES}
    llamaworker.h llamaworker.cpp
)

target_link_libraries(LLMTest PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    llama
    llama-common
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy_if_different
        # $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        # $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:llama>
        $<TARGET_FILE:llama-common>
        $<TARGET_FILE:ggml>
        $<TARGET_FILE:ggml-base>
        $<TARGET_FILE:ggml-cpu>
        $<TARGET_FILE:ggml-cuda>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6Core_DIR}/../../../bin"
                "$ENV{QT_DIR}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --dir $<TARGET_FILE_DIR:${PROJECT_NAME}>
                --no-translations
                --no-compiler-runtime
                $<TARGET_FILE:${PROJECT_NAME}>
        )
    else()
        message(WARNING "windeployqt not found. Qt DLLs must be copied manually.")
    endif()
endif()
