cmake_minimum_required(VERSION 3.15)
project(libgo LANGUAGES CXX C ASM)

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
# 1. 准备配置头文件路径（仅定义变量，不操作 target）
# ============================================================
set(_cmake_config_output "${CMAKE_CURRENT_BINARY_DIR}/libgo/common/cmake_config.h")

# 先生成头文件（这步可以在 add_library 之前）
configure_file(
    "${libgo_SOURCE_DIR}/libgo/common/cmake_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libgo/common/cmake_config.h"
    @ONLY
)
message(STATUS "CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}")
# ============================================================
# 2. 收集源文件（保持原有逻辑不变）
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
)

if(LIBGO_ENABLE_DEBUGGER)
    list(APPEND LIBGO_ALL_SOURCES ${LIBGO_DEBUGGER_SOURCES})
endif()

# ============================================================
# 3. 平台特定源文件
# ============================================================
if(WIN32)
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
    if(NOT LIBGO_DISABLE_HOOK)
        file(GLOB_RECURSE LIBGO_PLATFORM_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/unix/*.cpp")
    else()
        file(GLOB LIBGO_PLATFORM_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/disable_hook/*.cpp")
    endif()
    list(APPEND LIBGO_ALL_SOURCES ${LIBGO_PLATFORM_SOURCES})

    execute_process(
        COMMAND bash "${libgo_SOURCE_DIR}/third_party/select_asm.sh" "${libgo_SOURCE_DIR}" "jump"
        OUTPUT_VARIABLE LIBGO_JUMP_ASM OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE ASM_SELECT_RESULT
    )
    execute_process(
        COMMAND bash "${libgo_SOURCE_DIR}/third_party/select_asm.sh" "${libgo_SOURCE_DIR}" "make"
        OUTPUT_VARIABLE LIBGO_MAKE_ASM OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT ASM_SELECT_RESULT EQUAL 0)
        message(FATAL_ERROR "[libgo] select_asm.sh failed with code ${ASM_SELECT_RESULT}")
    endif()
    list(APPEND LIBGO_ALL_SOURCES ${LIBGO_JUMP_ASM} ${LIBGO_MAKE_ASM})
endif()

# ============================================================
# 4. 创建 Target ⬅️ 必须在这里先创建
# ============================================================
add_library(libgo_core STATIC ${LIBGO_ALL_SOURCES})
add_library(libgo::libgo ALIAS libgo_core)

# ============================================================
# 5. 设置编译定义 ⬅️ 移到 add_library 之后
# ============================================================
# 将绝对路径作为字符串传给编译器
string(REPLACE "/" "\\\\" _cmake_config_output_escaped "${_cmake_config_output}")
target_compile_definitions(libgo_core PRIVATE
    LIBGO_CMAKE_CONFIG_H_PATH="${_cmake_config_output_escaped}"
)

# ============================================================
# 6. Include 目录 & 平台编译选项
# ============================================================
target_include_directories(libgo_core
    PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/libgo/common"
    "${libgo_SOURCE_DIR}/libgo"
)


if(WIN32)
    target_include_directories(libgo_core
        PRIVATE "${libgo_SOURCE_DIR}/libgo/netio/windows"
    )
    target_compile_definitions(libgo_core PRIVATE
        _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX
    )
    target_compile_options(libgo_core PRIVATE /W0 /EHsc /wd4819 /wd4267)
    target_link_libraries(libgo_core PUBLIC ws2_32)
elseif(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(libgo_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    target_compile_options(libgo_core PRIVATE -fPIC -fno-strict-aliasing -Wall -D__const__=)
endif()

# ============================================================
# 7. 安装规则
# ============================================================
install(TARGETS libgo_core EXPORT libgoTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY "${libgo_SOURCE_DIR}/libgo/" DESTINATION include/libgo FILES_MATCHING PATTERN "*.h")
install(FILES "${_cmake_config_output}" DESTINATION include/libgo/common)
