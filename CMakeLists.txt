cmake_minimum_required(VERSION 3.15)
project(libgo LANGUAGES CXX C ASM)

if (NOT CMAKE_ASM_COMPILER_TARGET)
    set(CMAKE_ASM_COMPILER_TARGET "${CMAKE_CXX_COMPILER_TARGET}")

endif()

message(STATUS "libgo: CMAKE_C_COMPILER=${CMAKE_C_COMPILER} CMAKE_C_COMPILER_TARGET=${CMAKE_C_COMPILER_TARGET} CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} CMAKE_CXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET} CMAKE_ASM_COMPILER=${CMAKE_ASM_COMPILER} CMAKE_ASM_COMPILER_TARGET=${CMAKE_ASM_COMPILER_TARGET}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# FetchContent SOURCE_DIR 支持
# ============================================================
if(NOT libgo_SOURCE_DIR)
    set(libgo_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# ============================================================
# 选项定义
# ============================================================
option(LIBGO_ENABLE_DEBUGGER "Enable libgo debugger" OFF)
option(LIBGO_DISABLE_HOOK "Disable hook (use disable_hook netio)" OFF)

# ============================================================
# 1. 准备配置头文件路径
# ============================================================
# 定义 configure_file 需要的变量
set(ENABLE_DEBUGGER ${LIBGO_ENABLE_DEBUGGER})
if(LIBGO_DISABLE_HOOK)
    set(ENABLE_HOOK OFF)
else()
    set(ENABLE_HOOK ON)
endif()

set(_cmake_config_output "${CMAKE_CURRENT_BINARY_DIR}/libgo/common/cmake_config.h")

configure_file(
    "${libgo_SOURCE_DIR}/libgo/common/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libgo/common/cmake_config.h"
    @ONLY
)
message(STATUS "CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}")

# ============================================================
# 2. 收集源文件
# ============================================================
file(GLOB LIBGO_COMMON_SOURCES "${libgo_SOURCE_DIR}/libgo/common/*.cpp")
file(GLOB LIBGO_COROUTINE_SOURCES "${libgo_SOURCE_DIR}/libgo/coroutine/*.cpp")
file(GLOB LIBGO_SCHEDULER_SOURCES "${libgo_SOURCE_DIR}/libgo/scheduler/*.cpp")
file(GLOB LIBGO_TASK_SOURCES "${libgo_SOURCE_DIR}/libgo/task/*.cpp")
file(GLOB LIBGO_DEBUGGER_SOURCES "${libgo_SOURCE_DIR}/libgo/debugger/*.cpp")
file(GLOB LIBGO_ROUTINE_SYNC_SOURCES "${libgo_SOURCE_DIR}/libgo/routine_sync_libgo/*.cpp")
file(GLOB LIBGO_TIMER_SOURCES "${libgo_SOURCE_DIR}/libgo/timer/*.cpp")
file(GLOB LIBGO_CLS_SOURCES "${libgo_SOURCE_DIR}/libgo/cls/*.cpp")
file(GLOB LIBGO_DEFER_SOURCES "${libgo_SOURCE_DIR}/libgo/defer/*.cpp")
file(GLOB LIBGO_POOL_SOURCES "${libgo_SOURCE_DIR}/libgo/pool/*.cpp")
file(GLOB LIBGO_DEBUG_SOURCES "${libgo_SOURCE_DIR}/libgo/debug/*.cpp")
file(GLOB LIBGO_CONTEXT_SOURCES "${libgo_SOURCE_DIR}/libgo/context/*.cpp")
set(LIBGO_ALL_SOURCES
    ${LIBGO_COMMON_SOURCES}
    ${LIBGO_COROUTINE_SOURCES}
    ${LIBGO_SCHEDULER_SOURCES}
    ${LIBGO_TASK_SOURCES}
    ${LIBGO_ROUTINE_SYNC_SOURCES}
    ${LIBGO_TIMER_SOURCES}
    ${LIBGO_CLS_SOURCES}
    ${LIBGO_DEFER_SOURCES}
    ${LIBGO_POOL_SOURCES}
    ${LIBGO_DEBUG_SOURCES}
    ${LIBGO_CONTEXT_SOURCES}
)

if(LIBGO_ENABLE_DEBUGGER)
    list(APPEND LIBGO_ALL_SOURCES ${LIBGO_DEBUGGER_SOURCES})
endif()




# ============================================================
# 3. 平台特定源文件
# ============================================================
if(WIN32)
    # ============================================================
    # Windows 平台 - 使用内置 context
    # ============================================================
    file(GLOB LIBGO_NETIO_WIN_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/windows/*.cpp")

    file(GLOB LIBGO_FIBER_SOURCES "${libgo_SOURCE_DIR}/libgo/context/fiber/*.cpp")
    list(APPEND LIBGO_ALL_SOURCES ${LIBGO_NETIO_WIN_SOURCES} ${LIBGO_FIBER_SOURCES})

    if(NOT LIBGO_DISABLE_HOOK)
        list(APPEND LIBGO_ALL_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/windows/xhook/xhook.cpp")
    else()
        file(GLOB LIBGO_DISABLE_HOOK_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/disable_hook/*.cpp")
        list(APPEND LIBGO_ALL_SOURCES ${LIBGO_DISABLE_HOOK_SOURCES})
    endif()

elseif(UNIX)


    # 通用下载+解压函数
    # 用法: download_and_extract_if_needed(<url> <dest_dir> [SHA256 <hash>])
    function(download_and_extract_if_needed _url _dest)
        cmake_parse_arguments(_DL "" "SHA256" "" ${ARGN})

        # 如果目标已存在，跳过
        if(EXISTS "${_dest}")
            message(STATUS "[Download] ${_dest} already exists, skipping.")
            return()
        endif()

        get_filename_component(_archive "${_url}" NAME)
        get_filename_component(_parent "${_dest}" DIRECTORY)
        set(_archive_path "${_parent}/${_archive}")

        # 下载
        if(NOT EXISTS "${_archive_path}")
            message(STATUS "[Download] Downloading ${_archive}")
            file(MAKE_DIRECTORY "${_parent}")

            set(_dl_opts SHOW_PROGRESS STATUS _status LOG _log)
            if(DEFINED _DL_SHA256)
                list(APPEND _dl_opts EXPECT_HASH SHA256=${_DL_SHA256})
            endif()

            file(DOWNLOAD "${_url}" "${_archive_path}" ${_dl_opts})
            list(GET _status 0 _code)
            if(NOT _code EQUAL 0)
                file(REMOVE "${_archive_path}")
                message(FATAL_ERROR "[Download] Failed: ${_log}")
            endif()
        endif()

        # 解压到临时目录
        set(_tmp "${_parent}/_tmp_extract")
        file(REMOVE_RECURSE "${_tmp}")
        file(MAKE_DIRECTORY "${_tmp}")

        message(STATUS "[Download] Extracting to ${_dest}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_archive_path}"
            WORKING_DIRECTORY "${_tmp}"
            RESULT_VARIABLE _result
            ERROR_VARIABLE _error
        )

        if(NOT _result EQUAL 0)
            file(REMOVE_RECURSE "${_tmp}" "${_archive_path}")
            message(FATAL_ERROR "[Download] Extract failed: ${_error}")
        endif()

        # 自动处理顶层目录
        file(GLOB _items "${_tmp}/*")
        list(LENGTH _items _count)

        if(_count EQUAL 1)
            get_filename_component(_first "${_items}" NAME)
            if(IS_DIRECTORY "${_tmp}/${_first}")
                # 只有一个目录，使用它的内容
                file(GLOB _subitems "${_tmp}/${_first}/*")
                if(_subitems)
                    file(MAKE_DIRECTORY "${_dest}")
                    foreach(_item ${_subitems})
                        get_filename_component(_name "${_item}" NAME)
                        file(RENAME "${_item}" "${_dest}/${_name}")
                    endforeach()
                    file(REMOVE_RECURSE "${_tmp}")
                    message(STATUS "[Download] Succeeded: ${_dest}")
                    return()
                endif()
            endif()
        endif()

        # 直接移动所有内容
        file(MAKE_DIRECTORY "${_dest}")
        file(RENAME "${_tmp}" "${_dest}")

        message(STATUS "[Download] Succeeded: ${_dest}")
    endfunction()
    # ============================================================
    # Linux/Unix 平台 - 使用 Boost.Context (FetchContent)
    # ============================================================
    message(STATUS "libgo: Using Boost.Context for context switching (FetchContent)")

    include(FetchContent)

    # ============================================================
    # Boost 配置（写死，易于修改）
    # ============================================================
    set(BOOST_VERSION "1.90.0")
    # set(BOOST_VERSION_UNDERSCORE "1_91_0")
    # https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.gz
    # Boost 官方压缩包 URL
    set(BOOST_URL "https://github.com/boostorg/boost/releases/download/boost-${BOOST_VERSION}/boost-${BOOST_VERSION}-cmake.tar.gz")
    set(PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps")
    message(STATUS "libgo: Downloading Boost ${BOOST_VERSION} from ${BOOST_URL}")
    set(boost_SOURCE_DIR "${PACKAGE_DIR}/boost-${BOOST_VERSION}" CACHE PATH "Boost SOURCE DIR" FORCE)
    download_and_extract_if_needed(
        ${BOOST_URL}
        ${boost_SOURCE_DIR}
    )

    # 配置 Boost 构建选项 - 只构建 context 库
    set(BUILD_SHARED_LIBS OFF)
    set(BOOST_INCLUDE_LIBRARIES context)
    set(BOOST_BUILD_STATIC_LIBS ON)
    set(Boost_USE_STATIC_LIBS ON)

    # 添加 Boost 的 CMake 支持 (Boost 1.70+)
    if(EXISTS "${boost_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${boost_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/boost-build")

        if(TARGET Boost::context)
            set(BOOST_CONTEXT_FOUND TRUE)
            set(BOOST_CONTEXT_TARGET Boost::context)
            message(STATUS "libgo: Boost.Context built successfully")
        else()
            message(FATAL_ERROR "libgo: Failed to build Boost.Context")
        endif()
    else()
        message(FATAL_ERROR "libgo: Boost version ${BOOST_VERSION} does not support CMake (need 1.70+)")
    endif()

endif()

# ============================================================
# 4. 创建 Target
# ============================================================
add_library(libgo_core STATIC ${LIBGO_ALL_SOURCES})
add_library(libgo::libgo ALIAS libgo_core)
# if(UNIX)
#     # 强制取消 __has_attribute 的宏定义
#     target_compile_definitions(libgo_core PRIVATE
#         -U__has_attribute
#     )
# endif()
# ============================================================
# 5. 设置编译定义
# ============================================================
string(REPLACE "/" "\\\\" _cmake_config_output_escaped "${_cmake_config_output}")
target_compile_definitions(libgo_core PRIVATE
    LIBGO_CMAKE_CONFIG_H_PATH="${_cmake_config_output_escaped}"
)

# ============================================================
# 6. Include 目录 & 平台编译选项
# ============================================================
target_include_directories(libgo_core
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/libgo/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libgo>
)

if(WIN32)
    # ============================================================
    # Windows 编译选项
    # ============================================================
    target_include_directories(libgo_core
        PRIVATE "${libgo_SOURCE_DIR}/libgo/netio/windows"
    )
    target_compile_definitions(libgo_core PRIVATE
        _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX
    )
    target_compile_options(libgo_core PRIVATE /W0 /EHsc /wd4819 /wd4267)
    target_link_libraries(libgo_core PUBLIC ws2_32)

elseif(UNIX)
    # ============================================================
    # Linux/Unix 编译选项
    # ============================================================

    # ============================================================
    # 静态链接专用配置
    # ============================================================
    if(BUILD_SHARED_LIBS STREQUAL "OFF" OR CMAKE_EXE_LINKER_FLAGS MATCHES "-static")
        # 强制 FindThreads 使用正确的静态链接参数
        # glibc 2.31 静态链接要求: -lpthread -lc -lgcc_eh
        SET(CMAKE_THREAD_LIBS_INIT "-lpthread -lc -lgcc_eh" CACHE STRING "" FORCE)
        SET(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "" FORCE)
        SET(CMAKE_USE_PTHREADS_INIT 1 CACHE BOOL "" FORCE)
        SET(THREADS_FOUND TRUE CACHE BOOL "" FORCE)

        # 确保链接器搜索静态库时优先找到完整组合
        SET(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -static -Wl,--start-group -lpthread -lc -lgcc_eh -Wl,--end-group" CACHE STRING "" FORCE)
    endif()


    find_package(Threads REQUIRED)
    target_link_libraries(libgo_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    target_compile_options(libgo_core PRIVATE -fPIC -fno-strict-aliasing -Wall)

    # ============================================================
    # 链接 Boost.Context
    # ============================================================
    if(TARGET Boost::context)
        target_link_libraries(libgo_core PUBLIC Boost::context)
        target_compile_definitions(libgo_core PUBLIC -DLIBGO_USE_BOOST_CONTEXT=1)

        # 添加汇编相关的编译选项
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
            target_compile_options(libgo_core PRIVATE -Wa,--noexecstack)
        endif()

        message(STATUS "libgo: Boost.Context linked successfully")
    else()
        message(FATAL_ERROR "libgo: Boost::context target not found")
    endif()
endif()

# ============================================================
# 7. 安装规则
# ============================================================
# install(TARGETS libgo_core EXPORT libgoTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
# install(DIRECTORY "${libgo_SOURCE_DIR}/libgo/" DESTINATION include/libgo FILES_MATCHING PATTERN "*.h")
# install(FILES "${_cmake_config_output}" DESTINATION include/libgo/common)