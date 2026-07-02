#include "byte_memory.h"
#include "memory.h"
#include "os_constant.h"
#include "os_modules.h"
#include "program.h"
#include "stdlib.h"

ByteMemoryManager::ByteMemoryManager()
{
    initialize();
}

void ByteMemoryManager::initialize()
{
    int size = minSize;
    for (int i = 0; i < MEM_BLOCK_TYPES; ++i)
    {
        arenas[i] = nullptr;
        arenaSize[i] = size;
        size <<= 1;
    }
}

void *ByteMemoryManager::allocate(int size)
{
    if (size <= 0)
    {
        return nullptr;
    }

    int index = 0;
    while (index < MEM_BLOCK_TYPES && arenaSize[index] < size)
    {
        ++index;
    }

    PCB *pcb = programManager.running;
    AddressPoolType poolType = (pcb && pcb->pageDirectoryAddress)
                                   ? AddressPoolType::USER
                                   : AddressPoolType::KERNEL;

    if (index == MEM_BLOCK_TYPES)
    {
        int pageAmount = (size + sizeof(Arena) + PAGE_SIZE - 1) / PAGE_SIZE;
        int page = memoryManager.allocatePages(poolType, pageAmount);
        if (!page)
        {
            return nullptr;
        }

        Arena *arena = (Arena *)page;
        arena->type = ARENA_MORE;
        arena->counter = pageAmount;
        return (void *)(page + sizeof(Arena));
    }

    if (!arenas[index] && !getNewArena(index))
    {
        return nullptr;
    }

    MemoryBlockListItem *item = arenas[index];
    arenas[index] = item->next;
    if (arenas[index])
    {
        arenas[index]->previous = nullptr;
    }

    Arena *arena = (Arena *)((int)item & 0xfffff000);
    --arena->counter;
    return item;
}

void ByteMemoryManager::release(void *address)
{
    if (!address)
    {
        return;
    }

    PCB *pcb = programManager.running;
    AddressPoolType poolType = (pcb && pcb->pageDirectoryAddress)
                                   ? AddressPoolType::USER
                                   : AddressPoolType::KERNEL;

    Arena *arena = (Arena *)((int)address & 0xfffff000);
    if (arena->type == ARENA_MORE)
    {
        memoryManager.releasePages(poolType, (int)arena, arena->counter);
        return;
    }

    MemoryBlockListItem *item = (MemoryBlockListItem *)address;
    int type = (int)arena->type;

    item->previous = nullptr;
    item->next = arenas[type];
    if (item->next)
    {
        item->next->previous = item;
    }
    arenas[type] = item;
    ++arena->counter;

    int totalBlocks = (PAGE_SIZE - sizeof(Arena)) / arenaSize[type];
    if (arena->counter != totalBlocks)
    {
        return;
    }

    MemoryBlockListItem *current = arenas[type];
    while (current)
    {
        MemoryBlockListItem *next = current->next;
        if ((int)arena == ((int)current & (int)0xfffff000))
        {
            if (current->previous)
            {
                current->previous->next = current->next;
            }
            else
            {
                arenas[type] = current->next;
            }

            if (current->next)
            {
                current->next->previous = current->previous;
            }
        }
        current = next;
    }

    memoryManager.releasePages(poolType, (int)arena, 1);
}

void *ByteMemoryManager::reallocate(void *address, int newSize)
{
    if (!address)
    {
        return allocate(newSize);
    }

    if (newSize <= 0)
    {
        release(address);
        return nullptr;
    }

    int oldCapacity = getBlockCapacity(address);
    if (newSize <= oldCapacity)
    {
        return address;
    }

    void *newAddress = allocate(newSize);
    if (!newAddress)
    {
        return nullptr;
    }

    int copySize = oldCapacity < newSize ? oldCapacity : newSize;
    memcpy(address, newAddress, copySize);
    release(address);
    return newAddress;
}

int ByteMemoryManager::getBlockCapacity(void *address)
{
    if (!address)
    {
        return 0;
    }

    Arena *arena = (Arena *)((int)address & 0xfffff000);
    if (arena->type == ARENA_MORE)
    {
        return arena->counter * PAGE_SIZE - sizeof(Arena);
    }

    return arenaSize[(int)arena->type];
}

bool ByteMemoryManager::getNewArena(int index)
{
    PCB *pcb = programManager.running;
    AddressPoolType poolType = (pcb && pcb->pageDirectoryAddress)
                                   ? AddressPoolType::USER
                                   : AddressPoolType::KERNEL;

    int page = memoryManager.allocatePages(poolType, 1);
    if (!page)
    {
        return false;
    }

    Arena *arena = (Arena *)page;
    arena->type = (ArenaType)index;

    int blockCount = (PAGE_SIZE - sizeof(Arena)) / arenaSize[index];
    arena->counter = blockCount;

    int address = page + sizeof(Arena);
    MemoryBlockListItem *previous = nullptr;
    arenas[index] = (MemoryBlockListItem *)address;

    for (int i = 0; i < blockCount; ++i)
    {
        MemoryBlockListItem *current = (MemoryBlockListItem *)(address + i * arenaSize[index]);
        current->previous = previous;
        current->next = nullptr;

        if (previous)
        {
            previous->next = current;
        }
        previous = current;
    }

    return true;
}
