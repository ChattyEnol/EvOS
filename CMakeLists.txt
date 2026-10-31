cmake_minimum_required(VERSION 3.20)

# 别在初始化时尝试去链接验证程序，否则裸机环境会直接报错。
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(EvOS C)

# 锁定 LLVM 和目标架构。
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET "x86_64-unknown-windows")

# 引入两个子模块目录。
add_subdirectory(EFI)
add_subdirectory(Evoncil)