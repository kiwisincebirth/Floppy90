#ifndef MEMORY_H
#define MEMORY_H

// void __not_in_flash_func(service_memory)(void);

// initialisation
void MemInit(void);
void LoadFDCBootBlock(void);

// service methods
void __not_in_flash_func(ServiceHighMemoryOperation)(word addr, byte bIsRead);
void __not_in_flash_func(ServiceExtraMemory)(word addr, byte bIsRead);
void __not_in_flash_func(ServiceExtraPorts)(word addr, byte bIsRead);

#endif // MEMORY_H
