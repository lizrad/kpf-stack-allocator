/**
 * Exercise: "DoubleEndedStackAllocator with Canaries" OR "Growing DoubleEndedStackAllocator with Canaries (VMEM)"
 * Group members: Amon Shokhin Ahmed (gs20m014), Karl Bittner (gs20m013), David Panagiotopulos (gs20m019)
 **/

#include "stdio.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <malloc.h>
#include <new>

// Use (void) to silent unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))

namespace Tests
{
    void Test_Case_Success(const char *name, bool passed)
    {
        if (passed)
        {
            printf("[%s] passed the test!\n", name);
        }
        else
        {
            printf("[%s] failed the test!\n", name);
        }
    }

    void Test_Case_Failure(const char *name, bool passed)
    {
        if (!passed)
        {
            printf("[%s] passed the test!\n", name);
        }
        else
        {
            printf("[%s] failed the test!\n", name);
        }
    }

    /**
     * Example of how a test case can look like. The test cases in the end will check for
     * allocation success, proper alignment, overlaps and similar situations. This is an
     * example so you can already try to cover all cases you judge as being important by
     * yourselves.
     **/
    template <class A> bool VerifyAllocationSuccess(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);

        if (mem == nullptr)
        {
            printf("[Error]: Allocator returned nullptr!\n");
            return false;
        }

        return true;
    }
} // namespace Tests

// Assignment functionality tests are going to be included here

// If set to 1, Free() and FreeBack() should assert if the memory canaries are corrupted
#define WITH_DEBUG_CANARIES 1

#if WITH_DEBUG_CANARIES
static const uint16_t CANARY = 0x0DD0;
#endif

struct Metadata
{
    Metadata(size_t content_size, uintptr_t previous_address)
        : content_size(content_size), previous_address(previous_address)
    {
    }
    size_t content_size;
    uintptr_t previous_address;
};

/**
 * You work on your DoubleEndedStackAllocator. Stick to the provided interface, this is
 * necessary for testing your assignment in the end. Don't remove or rename the public
 * interface of the allocator. Also don't add any additional initialization code, the
 * allocator needs to work after it was created and its constructor was called. You can
 * add additional public functions but those should only be used for your own testing.
 **/

class DoubleEndedStackAllocator
{
  public:
    DoubleEndedStackAllocator(size_t max_size)
    {
        void *begin = malloc(max_size);

        assertm(begin != nullptr, "Malloc failed!");

        // These values will stay constant throughout the object's lifetime (unless grow functionality is added)
        allocation_begin = reinterpret_cast<uintptr_t>(begin);
        allocation_end = reinterpret_cast<uintptr_t>(begin) + max_size;

        // Initialize the current addresses to the edges of the allocated space
        Reset();
    }
    ~DoubleEndedStackAllocator(void)
    {
        free(reinterpret_cast<void *>(allocation_begin));
    }

    // Copy and Move Constructors / Assignment Operators are explicitly deleted.
    // There are some ways to get them to work, but they all require the user to be very cautious and might be
    // unexpected (especially when called implicitly), so we think it's best to keep the user from invoking them
    // entirely.
    DoubleEndedStackAllocator(const DoubleEndedStackAllocator &other) = delete;             // Copy Constructor
    DoubleEndedStackAllocator(DoubleEndedStackAllocator &&other) = delete;                  // Move Constructor
    DoubleEndedStackAllocator &operator=(const DoubleEndedStackAllocator &other) = delete;  // Copy Assignment Operator
    DoubleEndedStackAllocator &operator=(const DoubleEndedStackAllocator &&other) = delete; // Move Assignment Operator

    /**
     * Memory layout of content is:
     * ...[previous Canary][   ][Metadata][Content][Canary][   ][next Metadata]....
     * Pointer points to border of Metadata and Content.
     * [Content] Block will be on aligned adress.
     * This means there might be unused space (note the [   ] blocks above) before Metadata.
     **/

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    // TODO: Check for edgecases: not power of two alignment, enough size left, overlap etc.
    void *Allocate(size_t size, int64_t alignment)
    {
        assertm(alignment % 2 == 0, "Allocation only works with an alignement of the power of two");

        uintptr_t offset_address = current_front_free_address;
        // Making sure there is enough space to write metadata
        offset_address += sizeof(Metadata);

        // Allocate using correct offeset address (provide prev metadata address)
        uintptr_t allocation_address = Allocate(size, (int)alignment, offset_address, current_front_address);

        // Update internal address pointers
        current_front_address = allocation_address;
        current_front_free_address = allocation_address + size;
#if WITH_DEBUG_CANARIES
        // Add canary size to get correct free address
        current_front_free_address += sizeof(CANARY);
#endif

        return reinterpret_cast<void *>(allocation_address);
    }

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    // TODO: Check for edgecases: not power of two alignment, enough size left, overlap etc.
    void *AllocateBack(size_t size, int64_t alignment)
    {
        assertm(alignment % 2 == 0, "Allocation only works with an alignement of the power of two");

        uintptr_t offset_address = current_back_free_address;

#if WITH_DEBUG_CANARIES
        // Making sure there is enough space to write canary
        offset_address -= sizeof(CANARY);
#endif

        // Making sure there is enough space for the content
        offset_address -= size;

        // Making sure there is enough space to write metadata
        offset_address -= sizeof(Metadata);

        // Allocate with negative alignment and correct offset address (provide prev metadata address)
        uintptr_t allocation_address = Allocate(size, -alignment, offset_address, current_back_address);

        // Update internal address pointers
        current_back_address = allocation_address;
        current_back_free_address = allocation_address - sizeof(Metadata);

        return reinterpret_cast<void *>(allocation_address);
    }

    // LIFO is assumed.
    // Frees the given memory by moving the internal front addresses
    void Free(void *memory)
    {
        assertm(current_front_address != allocation_begin, "Cannot free an empty front");

        // TODO: Handle on enmpty?
        uintptr_t address = reinterpret_cast<uintptr_t>(memory);
        assertm(address == current_front_address, "Free must be called LIFO!");

        Metadata *metadata = ReadMetadata(current_front_address);

        // Set current to previous address
        current_front_address = metadata->previous_address;

        // If beginning reached, set free address to beginning
        if (current_front_address == allocation_begin)
        {
            current_front_free_address = allocation_begin;
            return;
        }

        // Read data from new front
        metadata = ReadMetadata(current_front_address);
        // Add data size from new front
        current_front_free_address = current_front_address + metadata->content_size;

#if WITH_DEBUG_CANARIES
        // Add Canary
        current_front_free_address += sizeof(CANARY);
#endif
    }

    // LIFO is assumed.
    // Frees the given memory by moving the internal back addresses
    void FreeBack(void *memory)
    {
        assertm(current_back_address != allocation_end, "Cannot free an empty back");

        // TODO: Handle on enmpty?
        uintptr_t address = reinterpret_cast<uintptr_t>(memory);
        assertm(address == current_back_address, "FreeBack must be called LIFO!");

        Metadata *metadata = ReadMetadata(current_back_address);

        // Set current to previous address
        current_back_address = metadata->previous_address;

        // If end reached, set free address to end
        if (current_back_address == allocation_end)
        {
            current_back_free_address = allocation_end;
            return;
        }

        // Add data size from new back
        current_back_free_address = current_back_address - sizeof(Metadata);
    }

    // Clear the internal state so that the whole allocator range is available again.
    void Reset(void)
    {
        // Reset the pointers to the outer edges of the allocation
        current_front_address = allocation_begin;
        current_back_address = allocation_end;

        current_front_free_address = allocation_begin;
        current_back_free_address = allocation_end;
    }

  private:
    // The start address of the allocator
    uintptr_t allocation_begin;
    // The end address of the allocator (for fixed size)
    uintptr_t allocation_end;

    uintptr_t current_front_address;
    uintptr_t current_back_address;

    uintptr_t current_front_free_address;
    uintptr_t current_back_free_address;

    // Returns the the aligned address of the allocation
    uintptr_t Allocate(size_t size, int64_t alignment, uintptr_t offset_address, uintptr_t previous_address)
    {
        // Align address
        uintptr_t aligned_address = Align(offset_address, alignment);

        // Write metadata
        WriteMeta(aligned_address, size, previous_address);
#if WITH_DEBUG_CANARIES
        // Write canaries at end of content
        WriteCanary(aligned_address, size);
#endif
        return aligned_address;
    }

    void WriteMeta(uintptr_t address, size_t contentSize, uintptr_t previous_address)
    {
        uintptr_t metadataAddress = address - sizeof(Metadata);
        *reinterpret_cast<Metadata *>(metadataAddress) = Metadata(contentSize, previous_address);
    }

    Metadata *ReadMetadata(uintptr_t address) const
    {
        // TODO: Handle Metadata not found
        return reinterpret_cast<Metadata *>(address - sizeof(Metadata));
    }

#if WITH_DEBUG_CANARIES
    static void WriteCanary(uintptr_t address, size_t contentSize)
    {
        uintptr_t canaryAddress = address + contentSize;
        *reinterpret_cast<uint16_t *>(canaryAddress) = uint16_t(CANARY);
    }
#endif

    // Negative alignment for AlignDown
    uintptr_t Align(uintptr_t address, int64_t alignment)
    {
        return (address & ~(std::abs(alignment) - 1)) + alignment;
    }
};

#define RUN_TESTS 0
int main()
{
// You can add your own tests here, I will call my tests at then end with a fresh instance of your allocator and a
// specific max_size
#if RUN_TESTS
    {
        // You can remove this, just showcasing how the test functions can be used
        DoubleEndedStackAllocator allocator(1024u);
        Tests::Test_Case_Success("Allocate() does not return nullptr",
                                 [&allocator]() { return allocator.Allocate(32, 1) != nullptr; }());
    }
#endif // RUN_TEST

    DoubleEndedStackAllocator allocator(1024u);
    auto a = allocator.Allocate(32, 4);
    auto a_uint8_t_pointer = reinterpret_cast<uint8_t *>(a);
    for (int i = 0; i < 32; i++)
    {
        *a_uint8_t_pointer = 0xAA;
        a_uint8_t_pointer++;
    }
    auto b = allocator.Allocate(63, 8);
    auto b_uint8_t_pointer = reinterpret_cast<uint8_t *>(b);
    for (int i = 0; i < 63; i++)
    {
        *b_uint8_t_pointer = 0xBB;
        b_uint8_t_pointer++;
    }
    auto c = allocator.Allocate(121, 16);
    auto c_uint8_t_pointer = reinterpret_cast<uint8_t *>(c);
    for (int i = 0; i < 121; i++)
    {
        *c_uint8_t_pointer = 0xC1;
        c_uint8_t_pointer++;
    }
    auto d = allocator.AllocateBack(32, 4);
    auto d_uint8_t_pointer = reinterpret_cast<uint8_t *>(d);
    for (int i = 0; i < 32; i++)
    {
        *d_uint8_t_pointer = 0xDD;
        d_uint8_t_pointer++;
    }
    auto e = allocator.AllocateBack(63, 8);
    auto e_uint8_t_pointer = reinterpret_cast<uint8_t *>(e);
    for (int i = 0; i < 63; i++)
    {
        *e_uint8_t_pointer = 0xEE;
        e_uint8_t_pointer++;
    }
    auto f = allocator.AllocateBack(121, 16);
    auto f_uint8_t_pointer = reinterpret_cast<uint8_t *>(f);
    for (int i = 0; i < 121; i++)
    {
        *f_uint8_t_pointer = 0xFF;
        f_uint8_t_pointer++;
    }

    allocator.Free(c);

    auto g = allocator.Allocate(121, 16);
    auto g_uint8_t_pointer = reinterpret_cast<uint8_t *>(g);
    for (int i = 0; i < 121; i++)
    {
        *g_uint8_t_pointer = 0x99;
        g_uint8_t_pointer++;
    }

    allocator.FreeBack(f);

    auto h = allocator.AllocateBack(121, 16);
    auto h_uint8_t_pointer = reinterpret_cast<uint8_t *>(h);
    for (int i = 0; i < 121; i++)
    {
        *h_uint8_t_pointer = 0x88;
        h_uint8_t_pointer++;
    }

    allocator.Free(g);
    allocator.Free(b);
    allocator.Free(a);

    auto ii = allocator.Allocate(121, 4);
    auto ii_uint8_t_pointer = reinterpret_cast<uint8_t *>(ii);
    for (int i = 0; i < 121; i++)
    {
        *ii_uint8_t_pointer = 0x77;
        ii_uint8_t_pointer++;
    }

    allocator.FreeBack(h);
    allocator.FreeBack(e);
    allocator.FreeBack(d);

    auto j = allocator.AllocateBack(32, 4);
    auto j_uint8_t_pointer = reinterpret_cast<uint8_t *>(j);
    for (int i = 0; i < 32; i++)
    {
        *j_uint8_t_pointer = 0x66;
        j_uint8_t_pointer++;
    }

    // Here the assignment tests will happen - it will test basic allocator functionality.
    {
    }
}