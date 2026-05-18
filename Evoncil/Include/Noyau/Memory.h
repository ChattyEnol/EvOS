/** Noyau/Memory.h
 * 
 * (C) Charity Enol
 * 
 * 内存管理模块，先定接口。
 * 按道理，内存管理的接口全是向外的，对内的函数声明我全放源文件开头去了。
 */

#ifndef NOYAU_MEMORY_H
#define NOYAU_MEMORY_H

#include <World/World.h>
#include <stdint.h>

// 初始化：内核启动第一件事就是把 World 里的内存信息吃掉。
void InitMemory(MEMORY *memory);

uint64_t GetFreeMemorySize(void);
uint64_t GetUsedMemorySize(void);

/**
 * 设备地址映射，这个需要物理地址，所以得独立出来啊。
 * 目前也就只有显存吧？
 */

// 我设想输入想要的物理基址和大小，然后返回一个虚拟地址。
void *MapDeviceMemory(uint64_t physical_address, uint64_t size);

/**
 * 内存分配和回收。
 * 这里是面向用户的，因此全是虚拟地址。
 */

// 我设想的是输入大小，模块自己计算需要多少块，并返回首地址。
void *AllocatePage(uint64_t size);
void FreePage(void *address, uint64_t size);

/**
 * 临时来一个调试函数吧。
 * 获取物理地址。
 * void GetPhysicalAddress();
 */

// uint64_t GetPhysicalAddress(void *address);

#endif // NOYAU_MEMORY_H
