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

    # ============================================================
    # Linux/Unix 平台 - 网络 IO hook（netio/unix）
    # 之前 UNIX 分支只处理 Boost.Context，完全没有引用 netio/unix，
    # 导致 Linux/Android 上阻塞 socket 读不会让出协程。
    # ============================================================
    file(GLOB LIBGO_NETIO_UNIX_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/unix/*.cpp")

    if(NOT LIBGO_DISABLE_HOOK)
        list(APPEND LIBGO_ALL_SOURCES ${LIBGO_NETIO_UNIX_SOURCES})

        # 静态链接时必须提供 libc.a 的 __read/__recv 等内部符号（static_hook.cpp），
        # 否则 hook.cpp 的 dlsym(RTLD_NEXT) 路径在静态链接下拿不到原始函数。
        if(BUILD_SHARED_LIBS STREQUAL "OFF" OR CMAKE_EXE_LINKER_FLAGS MATCHES "-static")
            list(APPEND LIBGO_ALL_SOURCES
                "${libgo_SOURCE_DIR}/libgo/netio/unix/static_hook/static_hook.cpp")
            message(STATUS "libgo: netio/unix hooked (static link mode, static_hook enabled)")
        else()
            message(STATUS "libgo: netio/unix hooked (dynamic link mode, RTLD_NEXT)")
        endif()
    else()
        file(GLOB LIBGO_DISABLE_HOOK_SOURCES "${libgo_SOURCE_DIR}/libgo/netio/disable_hook/*.cpp")
        list(APPEND LIBGO_ALL_SOURCES ${LIBGO_DISABLE_HOOK_SOURCES})
        message(STATUS "libgo: netio hook DISABLED (disable_hook)")
    endif()

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
    # Linux/Unix 平台 - Boost.Context 的 fcontext 汇编
    #
    # libgo 只需要 Boost.Context 的 fcontext 三件套（make/jump/ontop），
    # 不需要 Boost 的其余 158 个库，也不需要 Boost 的头文件体系。
    #
    # 默认使用仓库内 vendored 的最小文件集：
    #   third_party/boost_context/src/asm/*.S   （约 240KB）
    # 替代原先"下载 Boost 1.90.0 全量 cmake tar.gz（129MB 下载 / 691MB 解包）
    # 再 add_subdirectory 整个 Boost 只为拿 context 一个库"的做法。
    #
    # 可用 LIBGO_BOOST_CONTEXT_SOURCE=vendored|fetch 切换：
    #   vendored - 编译仓库内的 .S（默认，离线可用）
    #   fetch    - 走原来的 Boost 全量下载路径（需要网络，保留作回退）
    # ============================================================
    set(LIBGO_BOOST_CONTEXT_SOURCE "vendored" CACHE STRING
        "Boost.Context source for fcontext: vendored (in-repo .S) or fetch (full Boost tarball)")
    set_property(CACHE LIBGO_BOOST_CONTEXT_SOURCE PROPERTY STRINGS vendored fetch)

    if(LIBGO_BOOST_CONTEXT_SOURCE STREQUAL "vendored")

        # ---- 按 架构/ABI/二进制格式 选出 3 个汇编文件 ----
        if(WIN32 OR CYGWIN)
            set(_libgo_binfmt pe)
        elseif(APPLE)
            set(_libgo_binfmt macho)
        else()
            set(_libgo_binfmt elf)
        endif()

        math(EXPR _libgo_bits "${CMAKE_SIZEOF_VOID_P} * 8")

        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]" OR
           CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(_libgo_abi aapcs)
        else()
            set(_libgo_abi sysv)
        endif()

        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR
           CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]64")
            set(_libgo_arch arm64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]")
            set(_libgo_arch arm)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64")
            set(_libgo_arch riscv64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^loongarch64")
            set(_libgo_arch loongarch64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc64)")
            set(_libgo_arch ppc64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc32)")
            set(_libgo_arch ppc32)
        elseif(_libgo_bits EQUAL 32)
            set(_libgo_arch i386)
        else()
            set(_libgo_arch x86_64)
        endif()

        # vendored 文件集支持的组合；不在列表内的架构回退到 fetch 路径
        set(_libgo_supported_archs x86_64 i386 arm arm64 riscv64 loongarch64 ppc32 ppc64)
        set(_libgo_supported_binfmts elf macho)

        if(NOT _libgo_arch IN_LIST _libgo_supported_archs OR
           NOT _libgo_binfmt IN_LIST _libgo_supported_binfmts)
            message(STATUS "libgo: vendored boost_context 无 ${_libgo_arch}/${_libgo_abi}/${_libgo_binfmt}，回退到 Boost 全量下载")
            set(LIBGO_BOOST_CONTEXT_SOURCE "fetch")
        else()
            set(_libgo_ctx_dir "${libgo_SOURCE_DIR}/third_party/boost_context")

            set(_libgo_ctx_asm)
            foreach(_fn make jump ontop)
                set(_f "${_libgo_ctx_dir}/src/asm/${_fn}_${_libgo_arch}_${_libgo_abi}_${_libgo_binfmt}_gas.S")
                if(NOT EXISTS "${_f}")
                    message(FATAL_ERROR "libgo: vendored boost_context 缺少 ${_f}")
                endif()
                list(APPEND _libgo_ctx_asm "${_f}")
            endforeach()

            list(APPEND LIBGO_ALL_SOURCES ${_libgo_ctx_asm})

            set(BOOST_CONTEXT_FOUND TRUE)
            set(BOOST_CONTEXT_TARGET libgo_boost_context)

            message(STATUS "libgo: Boost.Context(fcontext) 使用仓库内 vendored 汇编: ${_libgo_arch}/${_libgo_abi}/${_libgo_binfmt}")
        endif()

        unset(_libgo_binfmt)
        unset(_libgo_bits)
        unset(_libgo_abi)
        unset(_libgo_arch)
        unset(_libgo_supported_archs)
        unset(_libgo_supported_binfmts)
    endif()

    if(LIBGO_BOOST_CONTEXT_SOURCE STREQUAL "fetch")
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


    target_include_directories(libgo_core
        PRIVATE "${libgo_SOURCE_DIR}/libgo/netio/unix"
    )

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
    elseif(DEFINED _libgo_ctx_asm AND _libgo_ctx_asm)
        # vendored 模式：汇编已作为 libgo_core 的源文件直接编译，无需外部库。
        set_source_files_properties(${_libgo_ctx_asm} PROPERTIES LANGUAGE ASM)

        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i386|i686")
            target_compile_options(libgo_core PRIVATE -Wa,--noexecstack)
        endif()

        # Boost 的 .S 里有 #if/#include，需要走 C 预处理器
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set_property(SOURCE ${_libgo_ctx_asm} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set_property(SOURCE ${_libgo_ctx_asm} APPEND PROPERTY COMPILE_OPTIONS "-Wno-unused-command-line-argument")
        endif()

        message(STATUS "libgo: vendored Boost.Context(fcontext) 已直接编译进 libgo_core")
    else()
        message(FATAL_ERROR "libgo: Boost.Context (fcontext) 未找到")
    endif()
endif()

# ============================================================
# 7. 安装规则
# ============================================================
# install(TARGETS libgo_core EXPORT libgoTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
# install(DIRECTORY "${libgo_SOURCE_DIR}/libgo/" DESTINATION include/libgo FILES_MATCHING PATTERN "*.h")
# install(FILES "${_cmake_config_output}" DESTINATION include/libgo/common)