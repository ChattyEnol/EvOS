file(GLOB_RECURSE LOADER_SOURCES 
    "BOOT/*.c"
    "BOOT/Sources/*.c"
    "BOOT/Library/Sources/*.c"
)

add_executable(Loader ${LOADER_SOURCES})

# 把产物伪装成标准的 EFI 引导文件并精准放到原有的 Output 目录里
set_target_properties(Loader PROPERTIES
    OUTPUT_NAME "BOOTX64"
    SUFFIX ".EFI"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Output/EFI/BOOT"
)

target_include_directories(Loader PRIVATE
    "Include"
    "Include/X64"
    "BOOT/Library/Include"
    "BOOT/Library/Include/X64"
    "BOOT/Include"
    ".."
)

target_compile_options(Loader PRIVATE
    -ffreestanding -fno-stack-protector -mno-red-zone -O3
)

target_link_options(Loader PRIVATE
    -fuse-ld=lld
    -Wl,-entry:EvLoader
    -Wl,-subsystem:efi_application
    -Wl,-nodefaultlib
)