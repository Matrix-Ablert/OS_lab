#ifndef BYTE_MEMORY_H
#define BYTE_MEMORY_H

enum ArenaType
{
    ARENA_16,
    ARENA_32,
    ARENA_64,
    ARENA_128,
    ARENA_256,
    ARENA_512,
    ARENA_1024,
    ARENA_MORE
};

struct Arena
{
    ArenaType type;
    int counter;
};

struct MemoryBlockListItem
{
    MemoryBlockListItem *previous;
    MemoryBlockListItem *next;
};

class ByteMemoryManager
{
private:
    static const int MEM_BLOCK_TYPES = 7;
    static const int minSize = 16;

    int arenaSize[MEM_BLOCK_TYPES];
    MemoryBlockListItem *arenas[MEM_BLOCK_TYPES];

public:
    ByteMemoryManager();

    void initialize();
    void *allocate(int size);
    void release(void *address);
    void *reallocate(void *address, int newSize);
    int getBlockCapacity(void *address);

private:
    bool getNewArena(int index);
};

#endif
