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
# 1. 准备配置头文件路径
# ============================================================
set(ENABLE_DEBUGGER ${LIBGO_ENABLE_DEBUGGER})
if(LIBGO_DISABLE_HOOK)
    set(ENABLE_HOOK OFF)
else()
    set(ENABLE_HOOK ON)
endif()

set(_cmake_config_output "${CMAKE_CURRENT_BINARY_DIR}/libgo/common/cmake_config.h")

configure_file(
    "${libgo_SOURCE_DIR}/libgo/common/cmake_config.h.in"
    "${_cmake_config_output}"
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
# 3. 平台特定源文件 & 依赖
# ============================================================
if(WIN32)
    # Windows 平台 - 使用内置 fiber context
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
    # ============================================================
    # Linux/Unix 平台 - 使用 Boost.Context (FetchContent)
    # ============================================================
    message(STATUS "libgo: Using Boost.Context for context switching (FetchContent)")

    # 架构映射
    set(_detected_arch "${CMAKE_SYSTEM_PROCESSOR}")
    string(TOLOWER "${_detected_arch}" _lower_arch)

    if(_lower_arch MATCHES "aarch64|arm64")
        set(_boost_arch "arm64")
        set(_boost_abi "aapcs")
    elseif(_lower_arch MATCHES "armv7|armhf|armv6|arm")
        set(_boost_arch "arm")
        set(_boost_abi "aapcs")
    elseif(_lower_arch MATCHES "x86_64|amd64")
        set(_boost_arch "x86_64")
        set(_boost_abi "sysv")
    elseif(_lower_arch MATCHES "i[3-6]86|x86")
        set(_boost_arch "x86")
        set(_boost_abi "sysv")
    elseif(_lower_arch MATCHES "riscv64")
        set(_boost_arch "riscv64")
        set(_boost_abi "sysv")
    elseif(_lower_arch MATCHES "mips64")
        set(_boost_arch "mips64")
        set(_boost_abi "o32")
    elseif(_lower_arch MATCHES "mips")
        set(_boost_arch "mips32")
        set(_boost_abi "o32")
    else()
        message(WARNING "libgo: Unknown architecture '${_detected_arch}', falling back to Boost auto-detection")
        set(_boost_arch "")
    endif()

    include(FetchContent)

    # 下载 Boost.Context 源码
    set(BOOST_CONTEXT_REPOSITORY "https://github.com/boostorg/context.git")
    set(BOOST_CONTEXT_TAG "boost-1.91.0")
    
    message(STATUS "libgo: Downloading Boost.Context from ${BOOST_CONTEXT_REPOSITORY}")
    
    FetchContent_Declare(
        boost_context
        GIT_REPOSITORY ${BOOST_CONTEXT_REPOSITORY}
        GIT_TAG ${BOOST_CONTEXT_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/boost-context-src"
        BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/boost-context-build"
        USES_TERMINAL_DOWNLOAD TRUE
    )

    FetchContent_GetProperties(boost_context)
    if(NOT boost_context_POPULATED)
        FetchContent_Populate(boost_context)
        message(STATUS "Boost.Context downloaded to: ${boost_context_SOURCE_DIR}")
    endif()

    # 调试：列出实际的目录结构
    file(GLOB BOOST_CONTEXT_SRC_CONTENTS "${boost_context_SOURCE_DIR}/src/*")
    message(STATUS "Boost.Context src contents: ${BOOST_CONTEXT_SRC_CONTENTS}")

    # 收集 Boost.Context 源文件（排除 untested.cpp）
    file(GLOB BOOST_CONTEXT_SOURCES 
        "${boost_context_SOURCE_DIR}/src/*.cpp"
        "${boost_context_SOURCE_DIR}/src/*.c"
    )
    
    # 移除 untested.cpp
    list(FILTER BOOST_CONTEXT_SOURCES EXCLUDE REGEX "untested\\.cpp$")
    
    # 根据架构添加对应的汇编文件 - 检查多种可能的位置
    if(_boost_arch)
        # 可能的汇编文件位置
        set(possible_asm_dirs
            "${boost_context_SOURCE_DIR}/src/${_boost_arch}"
            "${boost_context_SOURCE_DIR}/src/asm/${_boost_arch}"
            "${boost_context_SOURCE_DIR}/src/arch/${_boost_arch}"
            "${boost_context_SOURCE_DIR}/src/${_boost_abi}/${_boost_arch}"
        )
        
        set(asm_found FALSE)
        foreach(asm_dir ${possible_asm_dirs})
            if(EXISTS "${asm_dir}")
                message(STATUS "libgo: Found assembly directory: ${asm_dir}")
                
                # 收集汇编文件
                file(GLOB BOOST_CONTEXT_ASM_SOURCES 
                    "${asm_dir}/*.S" 
                    "${asm_dir}/*.s" 
                    "${asm_dir}/*.asm"
                    "${asm_dir}/make_${_boost_arch}_sysv_elf_gas.S"
                    "${asm_dir}/jump_${_boost_arch}_sysv_elf_gas.S"
                    "${asm_dir}/ontop_${_boost_arch}_sysv_elf_gas.S"
                )
                
                if(BOOST_CONTEXT_ASM_SOURCES)
                    list(APPEND BOOST_CONTEXT_SOURCES ${BOOST_CONTEXT_ASM_SOURCES})
                    set(asm_found TRUE)
                    message(STATUS "libgo: Found assembly files: ${BOOST_CONTEXT_ASM_SOURCES}")
                    break()
                endif()
            endif()
        endforeach()
        
        if(NOT asm_found)
            message(WARNING "libgo: No assembly files found for architecture ${_boost_arch}")
            # 尝试查找通用的汇编文件
            file(GLOB BOOST_CONTEXT_ASM_SOURCES "${boost_context_SOURCE_DIR}/src/*.S")
            if(BOOST_CONTEXT_ASM_SOURCES)
                list(APPEND BOOST_CONTEXT_SOURCES ${BOOST_CONTEXT_ASM_SOURCES})
                message(STATUS "libgo: Using generic assembly files: ${BOOST_CONTEXT_ASM_SOURCES}")
            endif()
        endif()
    endif()

    if(NOT BOOST_CONTEXT_SOURCES)
        message(FATAL_ERROR "libgo: No Boost.Context source files found")
    endif()

    # 创建 Boost.Context 静态库
    add_library(boost_context STATIC ${BOOST_CONTEXT_SOURCES})
    add_library(Boost::context ALIAS boost_context)

    # 设置 include 目录
    target_include_directories(boost_context
        PUBLIC
        $<BUILD_INTERFACE:${boost_context_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${boost_context_SOURCE_DIR}/src
        ${boost_context_SOURCE_DIR}/include
    )

    # 添加所有的 src 子目录到 include 路径
    file(GLOB BOOST_CONTEXT_SUBDIRS "${boost_context_SOURCE_DIR}/src/*")
    foreach(subdir ${BOOST_CONTEXT_SUBDIRS})
        if(IS_DIRECTORY "${subdir}")
            target_include_directories(boost_context PRIVATE "${subdir}")
        endif()
    endforeach()

    # 设置编译定义
    target_compile_definitions(boost_context
        PRIVATE
        -DBOOST_CONTEXT_DYN_LINK=1
        -DBOOST_CONTEXT_SOURCE=1
        -DBOOST_ALL_NO_LIB=1
        -DBOOST_CONTEXT_DECL=
        -DBOOST_CONTEXT_EXPORT=
        -DBOOST_CONTEXT_IMPORT=
        -D_GNU_SOURCE
        -D_REENTRANT
        -DBOOST_USE_UCONTEXT=1
    )

    # 根据架构设置特定的宏
    if(_lower_arch MATCHES "x86_64|amd64")
        target_compile_definitions(boost_context PRIVATE -DBOOST_CONTEXT_ASM_X86_64)
        target_compile_options(boost_context PRIVATE -Wa,--noexecstack)
    elseif(_lower_arch MATCHES "i[3-6]86|x86")
        target_compile_definitions(boost_context PRIVATE -DBOOST_CONTEXT_ASM_X86)
        target_compile_options(boost_context PRIVATE -Wa,--noexecstack)
    elseif(_lower_arch MATCHES "aarch64|arm64")
        target_compile_definitions(boost_context PRIVATE -DBOOST_CONTEXT_ASM_ARM64)
    elseif(_lower_arch MATCHES "arm")
        target_compile_definitions(boost_context PRIVATE -DBOOST_CONTEXT_ASM_ARM)
    elseif(_lower_arch MATCHES "riscv64")
        target_compile_definitions(boost_context PRIVATE -DBOOST_CONTEXT_ASM_RISCV64)
    endif()

    # 启用 C++14 标准
    set_target_properties(boost_context PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )

    set(BOOST_CONTEXT_FOUND TRUE)
    set(BOOST_CONTEXT_TARGET Boost::context)
    message(STATUS "libgo: Boost.Context built successfully with ${_boost_arch} architecture")

    unset(BOOST_CONTEXT_SOURCES)
    unset(BOOST_CONTEXT_ASM_SOURCES)
endif()

# ============================================================
# 4. 创建 Target
# ============================================================
add_library(libgo_core STATIC ${LIBGO_ALL_SOURCES})
add_library(libgo::libgo ALIAS libgo_core)

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
    target_include_directories(libgo_core PRIVATE "${libgo_SOURCE_DIR}/libgo/netio/windows")
    target_compile_definitions(libgo_core PRIVATE
        _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX
    )
    target_compile_options(libgo_core PRIVATE /W0 /EHsc /wd4819 /wd4267)
    target_link_libraries(libgo_core PUBLIC ws2_32)

elseif(UNIX)
    if(CMAKE_CROSSCOMPILING)
        set(CMAKE_THREAD_LIBS_INIT "-lpthread" CACHE STRING "" FORCE)
        set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "" FORCE)
        set(CMAKE_USE_PTHREADS_INIT 1 CACHE BOOL "" FORCE)
        set(THREADS_PREFER_PTHREAD_FLAG ON CACHE BOOL "" FORCE)
    endif()

    find_package(Threads REQUIRED)
    target_link_libraries(libgo_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    target_compile_options(libgo_core PRIVATE -fPIC -fno-strict-aliasing -Wall)

    if(TARGET Boost::context)
        target_link_libraries(libgo_core PUBLIC Boost::context)
        target_compile_definitions(libgo_core PUBLIC -DLIBGO_USE_BOOST_CONTEXT=1)

        if(_lower_arch MATCHES "x86_64|amd64|i[3-6]86|x86")
            target_compile_options(libgo_core PRIVATE -Wa,--noexecstack)
        endif()

        message(STATUS "libgo: Boost.Context linked successfully for ${_detected_arch}")
    else()
        message(FATAL_ERROR "libgo: Boost::context target not found")
    endif()
endif()

# ============================================================
# 7. 安装规则（按需启用）
# ============================================================
# install(TARGETS libgo_core EXPORT libgoTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
# install(DIRECTORY "${libgo_SOURCE_DIR}/libgo/" DESTINATION include/libgo FILES_MATCHING PATTERN "*.h")
# install(FILES "${_cmake_config_output}" DESTINATION include/libgo/common)