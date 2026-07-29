set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm64)   # 改为 arm64

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(SYSROOT /home/yanfa/ubuntu20-arm64-sysroot)
set(TARGET_TRIPLE aarch64-linux-gnu)

set(CMAKE_C_FLAGS "--target=${TARGET_TRIPLE} --sysroot=${SYSROOT}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "--target=${TARGET_TRIPLE} --sysroot=${SYSROOT}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "--target=${TARGET_TRIPLE} --sysroot=${SYSROOT}" CACHE STRING "" FORCE)