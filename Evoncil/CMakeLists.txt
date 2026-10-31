# 1. 自动把内核源码编成一个名为 EvNoyau 的静态库
file(GLOB_RECURSE KERNEL_SOURCES
    "*.c"
    "Sources/HAL/*.c"
    "Sources/HAL/X64/*.c"
    "Sources/Noyau/*.c"
    "Sources/User/*.c"
    "Sources/Drivers/*.c"
    "Sources/File/*.c"
    "Sources/UI/*.c"
    "Library/Sources/*.c"
)
add_library(EvNoyau STATIC ${KERNEL_SOURCES})

target_include_directories(EvNoyau PRIVATE ".." "Include" "Library/Include")
target_compile_options(EvNoyau PRIVATE -ffreestanding -fno-stack-protector -mno-red-zone -O3)

# 2. 编译 Stub 并链接内核静态库，生成最终的内核 EFI 应用程序
file(GLOB_RECURSE STUB_SOURCES
    "../EFI/Evoncil/*.c"
    "../EFI/Evoncil/Sources/*.c"
)
add_executable(Evoncil ${STUB_SOURCES})

set_target_properties(Evoncil PROPERTIES
    OUTPUT_NAME "Evoncil"
    SUFFIX ".EFI"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Output/EFI/Evoncil"
)

target_include_directories(Evoncil PRIVATE
    ".." "Include" "Library/Include"
    "../EFI/Include" "../EFI/Include/X64" "../EFI/Evoncil/Include"
)
target_compile_options(Evoncil PRIVATE -ffreestanding -fno-stack-protector -mno-red-zone -O3)

# 让 Stub 融合你的内核静态库并链接成 EFI
target_link_libraries(Evoncil PRIVATE EvNoyau)
target_link_options(Evoncil PRIVATE
    -fuse-ld=lld
    -Wl,-entry:EvStub
    -Wl,-subsystem:efi_application
    -Wl,-nodefaultlib
)