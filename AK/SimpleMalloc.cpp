#include "SimpleMalloc.h"
#include "Assertions.h"
#include "Types.h"
#include <sys/mman.h>
#include <cstring>
#include <cstdio>

namespace SimpleMalloc {

class AllocationBitmap {
public:
    static AllocationBitmap wrap(byte* data, unsigned size)
    {
        return AllocationBitmap(data, size);
    }

    ~AllocationBitmap()
    {
    }

    unsigned size() const { return m_size; }
    bool get(unsigned index) const
    {
        ASSERT(index < m_size);
        return 0 != (m_data[index / 8] & (1u << (index % 8)));
    }
    void set(unsigned index, bool value) const
    {
        ASSERT(index < m_size);
        if (value)
            m_data[index / 8] |= static_cast<byte>((1u << (index % 8)));
        else
            m_data[index / 8] &= static_cast<byte>(~(1u << (index % 8)));
    }

private:
    AllocationBitmap(byte* data, unsigned size)
        : m_data(data)
        , m_size(size)
    {
    }

    byte* m_data { nullptr };
    unsigned m_size { 0 };
};

template<dword chunkSize>
class ChunkAllocator {
public:
    void initialize(byte* base)
    {
        m_base = base;
        m_free = capacity_in_allocations();
        dump();
    }

    static constexpr dword capacity_in_allocations()
    {
        return 1048576 / chunkSize;
    }

    static constexpr dword capacity_in_bytes()
    {
        return capacity_in_allocations() * chunkSize;
    }

    byte* allocate()
    {
        auto bitmap = this->bitmap();
        for (dword i = 0; i < capacity_in_allocations(); ++i) {
            if (!bitmap.get(i)) {
                bitmap.set(i, true);
                --m_free;
                return pointer_to_chunk(i);
            }
        }
        return nullptr;
    }

    void dump() const
    {
        printf("ChunkAllocator<%u> @ %p, free: %u\n", chunkSize, m_base, m_free);
    }

    void free(byte* ptr)
    {
        ASSERT(is_in_allocator(ptr));
        auto bitmap = this->bitmap();
        auto chunk_index = chunk_index_from_pointer(ptr);
        ASSERT(bitmap.get(chunk_index));
        bitmap.set(chunk_index, false);
        ++m_free;
    }

    bool is_in_allocator(byte* ptr)
    {
        return ptr >= pointer_to_chunk(0) && ptr <= address_after_this_allocator();
    }

    dword chunk_index_from_pointer(byte* ptr)
    {
        return (ptr - pointer_to_chunk(0)) / chunkSize;
    }

    byte* pointer_to_chunk(dword index)
    {
        return m_base + size_of_allocation_bitmap_in_bytes() + (index * chunkSize);
    }

    AllocationBitmap bitmap()
    {
        return AllocationBitmap::wrap(m_base, capacity_in_allocations());
    }

    static constexpr dword size_of_allocation_bitmap_in_bytes()
    {
        return capacity_in_allocations() / 8;
    }

    byte* address_after_this_allocator() const
    {
        return m_base + size_of_allocation_bitmap_in_bytes() + capacity_in_bytes();
    }

    dword number_of_free_chunks() const
    {
        return m_free;
    }

private:
    byte* m_base { nullptr };
    dword m_free { capacity_in_allocations() };
};

struct Allocator {
    void initialize();
    void initialize_if_needed();
    void dump();

    ChunkAllocator<8> alloc8;
    ChunkAllocator<16> alloc16;
    ChunkAllocator<4096> alloc4096;
    ChunkAllocator<16384> alloc16384;

    byte* space;
    bool initialized { false };
};

static Allocator allocator;

void Allocator::initialize_if_needed()
{
    if (initialized)
        return;
    initialize();
    initialized = true;
}

void Allocator::initialize()
{
    space = (byte*)mmap((void*)0x20000000, 32 * MB, PROT_WRITE | PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT(space != MAP_FAILED);
    alloc8.initialize(space + 0x10000);
    alloc16.initialize(alloc8.address_after_this_allocator());
    alloc4096.initialize(alloc16.address_after_this_allocator());
    alloc16384.initialize(alloc4096.address_after_this_allocator());
}

void Allocator::dump()
{
    alloc8.dump();
    alloc16.dump();
    alloc4096.dump();
    alloc16384.dump();
}

void initialize()
{
    allocator.initialize();
}

void dump()
{
    allocator.dump();
}

byte* allocate(dword size)
{
    if (!size)
        return nullptr;
    allocator.initialize_if_needed();
    if (size <= 8) {
        if (auto* ptr = allocator.alloc8.allocate())
            return ptr;
    }
    if (size <= 16) {
        if (auto* ptr = allocator.alloc16.allocate())
            return ptr;
    }
    if (size <= 4096) {
        if (auto* ptr = allocator.alloc4096.allocate())
            return ptr;
    }
    if (size <= 16384) {
        if (auto* ptr = allocator.alloc16384.allocate())
            return ptr;
    }
    printf("SimpleMalloc: unsupported alloc size: %u\n", size);
    ASSERT_NOT_REACHED();
    return nullptr;
}

byte* allocate_zeroed(dword size)
{
    auto* ptr = allocate(size);
    if (!ptr)
        return nullptr;
    memset(ptr, 0, size);
    return ptr;
}

byte* reallocate(byte* ptr, dword size)
{
    // FIXME;
    (void) ptr;
    (void) size;
    ASSERT_NOT_REACHED();
    return nullptr;
}

void free(byte* ptr)
{
    if (!ptr)
        return;
    allocator.initialize_if_needed();
    if (allocator.alloc8.is_in_allocator(ptr)) {
        allocator.alloc8.free(ptr);
        return;
    }
    if (allocator.alloc16.is_in_allocator(ptr)) {
        allocator.alloc16.free(ptr);
        return;
    }
    if (allocator.alloc4096.is_in_allocator(ptr)) {
        allocator.alloc4096.free(ptr);
        return;
    }
    if (allocator.alloc16384.is_in_allocator(ptr)) {
        allocator.alloc16384.free(ptr);
        return;
    }
    ASSERT_NOT_REACHED();
}

}

