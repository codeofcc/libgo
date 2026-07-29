# arm64-linux-gnu-toolchain.cmake

SET(CMAKE_SYSTEM_NAME Linux CACHE STRING "toolchain default")

SET(CMAKE_SYSTEM_PROCESSOR aarch64 CACHE STRING "toolchain default")

SET(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++ CACHE STRING "toolchain default")
SET(CMAKE_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_C_COMPILER_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_C_COMPILER_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_LINKER /usr/bin/aarch64-linux-gnu-ld CACHE STRING "toolchain default")
SET(CMAKE_NM /usr/bin/aarch64-linux-gnu-nm CACHE STRING "toolchain default")
SET(CMAKE_READELF /usr/bin/aarch64-linux-gnu-readelf CACHE STRING "toolchain default")
SET(CMAKE_OBJCOPY /usr/bin/aarch64-linux-gnu-objcopy CACHE STRING "toolchain default")
SET(CMAKE_OBJDUMP /usr/bin/aarch64-linux-gnu-objdump CACHE STRING "toolchain default")

SET(CMAKE_SYSROOT /home/yanfa/ubuntu20-arm64-sysroot CACHE STRING "toolchain default")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "toolchain default")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY CACHE STRING "toolchain default")
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY CACHE STRING "toolchain default")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY CACHE STRING "toolchain default")