/**
 * Exercise: "DoubleEndedStackAllocator with Canaries" OR "Growing DoubleEndedStackAllocator with Canaries (VMEM)"
 * Group members: Amon Shokhin Ahmed (gs20m014), Karl Bittner (gs20m013), David Panagiotopulos (gs20m019)
 **/

#include "stdio.h"
#include <cstdint>
#include <iostream>
#include <malloc.h>
#include <new>

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
static const size_t CANARY_SIZE = sizeof(CANARY);
#else
static const uintptr_t CANARY_SIZE = 0;
#endif

struct Metadata
{
    Metadata(uintptr_t prevMetadataAddress, size_t contentSize)
        : PrevMetadataAddress(prevMetadataAddress), ContentSize(contentSize)
    {
    }
    uintptr_t PrevMetadataAddress;
    size_t ContentSize;
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

        // TODO: Error handling

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
     * This means there might me unused space (note the [   ] blocks above) before Metadata.
     **/

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    // TODO: Check for edgecases: not power of two alignment, enough size left, overlap etc.
    void *Allocate(size_t size, size_t alignment)
    {

        uintptr_t actual_front = current_front_address;

        // special case - first allocation:
        // current_front_adress ist actually already free,
        // so we don't have to offset by content size and canary

        // else:
        if (current_front_address != allocation_begin)
        {
            // Offsetting front address that points at start of content by size of content and canary size to actually
            // get free space
            Metadata *metadata = ReadMetadata(current_front_address);
            actual_front += metadata->ContentSize;
#if WITH_DEBUG_CANARIES
            actual_front += sizeof(CANARY);
#endif // WITH_DEBUG_CANARIES
        }
        uintptr_t offset_address = actual_front;
        // making sure there is enough space to write metadata
        offset_address += sizeof(Metadata);

        // align front
        uintptr_t new_front = AlignUp(offset_address, alignment);

        // write metadata backwards from front
        WriteMeta(new_front, current_front_address, size);
#if WITH_DEBUG_CANARIES

        // write canaries at end of content
        WriteCanary(new_front, size);
#endif
        current_front_address = new_front;

        return reinterpret_cast<void *>(new_front);
    }

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    // TODO: Check for edgecases: not power of two alignment, enough size left, overlap etc.
    void *AllocateBack(size_t size, size_t alignment)
    {
        uintptr_t actual_back = current_back_address;

        // special case - first allocation:
        // current_back_address ist actually already free,
        // so we don't have to offset by content size and canary
        if (current_back_address != allocation_end)
        {
            // Offsetting back address that points at end of metadata by metadata size
            // free space
            actual_back -= sizeof(Metadata);
        }

        uintptr_t offset_address = actual_back;
#if WITH_DEBUG_CANARIES
        // making sure there is enough space to write canary
        offset_address -= sizeof(CANARY);
        // align front
#endif // WITH_DEBUG_CANARIES

        // making sure there is enough space for the content
        offset_address -= size;
        uintptr_t new_back = AlignDown(offset_address, alignment);

        // write metadata backwards from
        WriteMeta(new_back, current_back_address, size);
#if WITH_DEBUG_CANARIES

        // write canaries at end of content
        WriteCanary(new_back, size);
#endif
        current_back_address = new_back;

        return reinterpret_cast<void *>(new_back);
    }

    // LIFO is assumed.
    void Free(void *memory)
    {
        // TODO: Read metadata and move the pointer accordingly
    }
    void FreeBack(void *memory)
    {
        // TODO: Read metadata and move the pointer accordingly
    }

    // Clear the internal state so that the whole allocator range is available again.
    void Reset(void)
    {
        // Reset the pointers to the outer edges of the allocation
        current_front_address = allocation_begin;
        current_back_address = allocation_end;
    }
    static Metadata *ReadMetadata(uintptr_t address)
    {
        return reinterpret_cast<Metadata *>(address - sizeof(Metadata));
    }

  private:
    uintptr_t allocation_begin;
    uintptr_t allocation_end;

    uintptr_t current_front_address;
    uintptr_t current_back_address;

    void WriteMeta(uintptr_t address, uintptr_t prevMetadataAddress, size_t contentSize)
    {
        uintptr_t metadataAddress = address - sizeof(Metadata);
        *reinterpret_cast<Metadata *>(metadataAddress) = Metadata(prevMetadataAddress, contentSize);
    }

#if WITH_DEBUG_CANARIES
    static void WriteCanary(uintptr_t address, size_t contentSize)
    {
        uintptr_t canaryAddress = address + contentSize;
        *reinterpret_cast<uint16_t *>(canaryAddress) = uint16_t(CANARY);
    }
#endif

    uintptr_t AlignUp(uintptr_t address, size_t alignment)
    {
        return (address & ~(alignment - 1)) + alignment;
    }
    uintptr_t AlignDown(uintptr_t address, size_t alignment)
    {
        return (address & ~(alignment - 1)) - alignment;
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
        *c_uint8_t_pointer = 0xCC;
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

    // Here the assignment tests will happen - it will test basic allocator functionality.
    {
    }
}