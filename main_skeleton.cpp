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

        // not handling this here in release, because undefined behaviour would only appear in allocate functions where
        // we catch it by returning a nullptr
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
     * ...[previous Content][previous Canary][   ][Canary][Metadata][Content][Canary][   ][next Canary][nextMetadata].... 
     * Pointer points to border of Metadata and Content. [Content] Block will be on aligned adress. This
     * means there might be unused space (note the [   ] blocks above) before Metadata.
     **/

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    void *Allocate(size_t size, int64_t alignment)
    {
        if (next_free_address_front == reinterpret_cast<uintptr_t>(nullptr))
        {
            assertm(false, "Allocator did not allocate any memory");
            return nullptr;
        }
        if (alignment % 2 != 0)
        {
            assertm(false, "Allocation only works with an alignement of the power of two");
            return nullptr;
        }

        uintptr_t offset_address = next_free_address_front;
#if WITH_DEBUG_CANARIES
        // Making sure there is enough space to write canary
        offset_address += sizeof(CANARY);
#endif
        // Making sure there is enough space to write metadata
        offset_address += sizeof(Metadata);

        uintptr_t aligned_address = Align(offset_address, alignment);

#if WITH_DEBUG_CANARIES
        if (aligned_address + size + sizeof(CANARY) * 2 > next_free_address_back)
#else
        if (aligned_address + size > next_free_address_back)
#endif
        {
            // Overlap -> out of space!
            assertm(false, "Allocate failed due to lack of space!");
            return nullptr;
        }

        // Allocate using correct offeset address (provide prev metadata address)
        uintptr_t allocation_address = AllocateInternal(size, aligned_address, last_data_begin_address_front);

        // Update internal address pointers
        last_data_begin_address_front = allocation_address;
        next_free_address_front = allocation_address + size;
#if WITH_DEBUG_CANARIES
        // Add canary size to get correct free address
        next_free_address_front += sizeof(CANARY);
#endif

        return reinterpret_cast<void *>(allocation_address);
    }

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    void *AllocateBack(size_t size, int64_t alignment)
    {
        if (next_free_address_back == reinterpret_cast<uintptr_t>(nullptr))
        {
            assertm(false, "Allocator did not allocate any memory");
            return nullptr;
        }
        if (alignment % 2 != 0)
        {
            assertm(false, "Allocation only works with an alignement of the power of two");
            return nullptr;
        }

        uintptr_t offset_address = next_free_address_back;

#if WITH_DEBUG_CANARIES
        // Making sure there is enough space to write canary
        offset_address -= sizeof(CANARY);
#endif

        // Making sure there is enough space for the content
        offset_address -= size;

        uintptr_t aligned_address = Align(offset_address, -alignment);
#if WITH_DEBUG_CANARIES
        if (aligned_address - sizeof(Metadata) - sizeof(CANARY) < next_free_address_front)
#elif
        if (aligned_address - sizeof(Metadata) < next_free_address_front)
#endif
        {
            // Overlap -> out of space!
            assertm(false, "AllocateBack failed due to lack of space!");
            return nullptr;
        }

        // Allocate with negative alignment and correct offset address (provide prev metadata address)
        uintptr_t allocation_address = AllocateInternal(size, aligned_address, last_data_begin_address_back);

        // Update internal address pointers
        last_data_begin_address_back = allocation_address;
        next_free_address_back = allocation_address - sizeof(Metadata);
#if WITH_DEBUG_CANARIES
        next_free_address_back -= sizeof(CANARY);
#endif

        return reinterpret_cast<void *>(allocation_address);
    }

    // LIFO is assumed.
    // Frees the given memory by moving the internal front addresses
    void Free(void *memory)
    {
        // Is there anything to free?
        if (last_data_begin_address_front == allocation_begin)
        {
            assertm(false, "Cannot free an empty front");
            return;
        }

        // Is the user calling LIFO as intended?
        uintptr_t address = reinterpret_cast<uintptr_t>(memory);
        if (address != last_data_begin_address_front)
        {

            assertm(false, "Free must be called LIFO!");
            return;
        }

        Metadata *metadata = ReadMetadata(last_data_begin_address_front);

        // Not returning if the asserts fail as canaries shouldnt be enabled in release build anyway
#if WITH_DEBUG_CANARIES
        // Check canaries
        assertm(IsCanaryValid(last_data_begin_address_front - sizeof(Metadata) - sizeof(CANARY)),
                "First canary was overwritten - the memory is corrupted!");
        assertm(IsCanaryValid(last_data_begin_address_front + metadata->content_size),
                "Second canary was overwritten - the memory is corrupted!");
#endif

        // Set current to previous address
        last_data_begin_address_front = metadata->previous_address;

        // If beginning reached, set free address to beginning
        if (last_data_begin_address_front == allocation_begin)
        {
            next_free_address_front = allocation_begin;
            return;
        }

        // Read data from new front
        metadata = ReadMetadata(last_data_begin_address_front);
        // Add data size from new front
        next_free_address_front = last_data_begin_address_front + metadata->content_size;

#if WITH_DEBUG_CANARIES
        // Add Canary
        next_free_address_front += sizeof(CANARY);
#endif
    }

    // LIFO is assumed.
    // Frees the given memory by moving the internal back addresses
    void FreeBack(void *memory)
    {
        // Is there anything to free?
        if (last_data_begin_address_back == allocation_end)
        {
            assertm(false, "Cannot free an empty back");
            return;
        }

        // Is the user calling LIFO as intended?
        uintptr_t address = reinterpret_cast<uintptr_t>(memory);
        if (address != last_data_begin_address_back)
        {
            assertm(false, "FreeBack must be called LIFO!");
            return;
        }
        Metadata *metadata = ReadMetadata(last_data_begin_address_back);

        // Not returning if the asserts fail as canaries shouldnt be enabled in release build anyway
#if WITH_DEBUG_CANARIES
        // Check canary
        assertm(IsCanaryValid(last_data_begin_address_back - sizeof(Metadata) - sizeof(CANARY)),
                "First Canary was overwritten - the memory is corrupted!");
        assertm(IsCanaryValid(last_data_begin_address_back + metadata->content_size),
                "Second Canary was overwritten - the memory is corrupted!");
#endif

        // Set current to previous address
        last_data_begin_address_back = metadata->previous_address;

        // If end reached, set free address to end
        if (last_data_begin_address_back == allocation_end)
        {
            next_free_address_back = allocation_end;
            return;
        }

        // Add data size from new back
        next_free_address_back = last_data_begin_address_back - sizeof(Metadata);
#if WITH_DEBUG_CANARIES
        next_free_address_back -= sizeof(CANARY);
#endif
    }

    // Clear the internal state so that the whole allocator range is available again.
    void Reset(void)
    {
        // Reset the pointers to the outer edges of the allocation
        last_data_begin_address_front = allocation_begin;
        last_data_begin_address_back = allocation_end;

        next_free_address_front = allocation_begin;
        next_free_address_back = allocation_end;
    }

  private:
    // The start address of the allocator
    uintptr_t allocation_begin;
    // The end address of the allocator (for fixed size)
    uintptr_t allocation_end;

    uintptr_t last_data_begin_address_front;
    uintptr_t last_data_begin_address_back;

    uintptr_t next_free_address_front;
    uintptr_t next_free_address_back;

    // Returns the the aligned address of the allocation
    uintptr_t AllocateInternal(size_t size, uintptr_t aligned_address, uintptr_t previous_address)
    {
        // Write metadata
        WriteMeta(aligned_address, size, previous_address);
#if WITH_DEBUG_CANARIES
        // Write canaries at end of content
        WriteCanary(aligned_address + size);
        WriteCanary(aligned_address - sizeof(Metadata) - sizeof(CANARY));
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
    static void WriteCanary(uintptr_t address)
    {
        *reinterpret_cast<uint16_t *>(address) = uint16_t(CANARY);
    }

    static bool IsCanaryValid(uintptr_t address)
    {
        return *reinterpret_cast<uint16_t *>(address) == uint16_t(CANARY);
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