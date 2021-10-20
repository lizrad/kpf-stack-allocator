/**
 * Exercise: "GROWING DoubleEndedStackAllocator with Canaries (VMEM)"
 * Group members: Amon Shokhin Ahmed (gs20m014), Karl Bittner (gs20m013), David Panagiotopulos (gs20m019)
 **/

#include "stdio.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <malloc.h>
#include <new>
#include <strsafe.h>
#include <windows.h>

// Use (void) to silent unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))

struct Metadata;

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

    // Checks if the allocation works
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

    // Checks if the allocation on the back works
    template <class A> bool VerifyAllocationBackSuccess(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);

        if (mem == nullptr)
        {
            printf("[Error]: Allocator returned nullptr!\n");
            return false;
        }

        return true;
    }

    // Checks if the free works
    template <class A> bool VerifyFreeSuccess(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);
        allocator.Free(mem);

        void *mem2 = allocator.Allocate(size, alignment);

        if (mem != mem2)
        {
            printf("[Error]: Addresses are different!\n");
            return false;
        }

        return true;
    }

    // Checks if the free on the back works
    template <class A> bool VerifyFreeBackSuccess(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);
        allocator.FreeBack(mem);
        void *mem2 = allocator.AllocateBack(size, alignment);

        if (mem != mem2)
        {
            printf("[Error]: Addresses are different!\n");
            return false;
        }

        return true;
    }

    // Checks if the broken Canary after the data is noticed
    template <class A> bool VerifyCanaryAfterFailure(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        for (size_t i = 0; i < size + 2; i++)
        {
            *mem_pointer = 0xAA;
            mem_pointer++;
        }

        allocator.Free(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the broken Canary before the data is noticed
    template <class A> bool VerifyCanaryBeforeFailure(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        for (long unsigned int i = 0; i < sizeof(Metadata) + 2; i++)
        {
            *mem_pointer = 0xAA;
            mem_pointer--;
        }

        allocator.Free(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the broken Canary after the data is noticed (on the back side)
    template <class A> bool VerifyBackCanaryAfterFailure(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        for (size_t i = 0; i < size + 2; i++)
        {
            *mem_pointer = 0xAA;
            mem_pointer++;
        }

        allocator.FreeBack(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the broken Canary before the data is noticed (on the back side)
    template <class A> bool VerifyBackCanaryBeforeFailure(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        for (long unsigned int i = 0; i < sizeof(Metadata) + 2; i++)
        {
            *mem_pointer = 0xAA;
            mem_pointer--;
        }

        allocator.FreeBack(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the size inside the metadata being overwritten is noticed
    template <class A> bool VerifyMetadataSizeOverwritten(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        mem_pointer += sizeof(uintptr_t);
        for (int i = 0; i < 1; i++)
        {
            *mem_pointer = 0xAA;
            mem_pointer--;
        }

        allocator.Free(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }
    
    // Checks if the size inside the metadata being overwritten is noticed (on the back side)
    template <class A> bool VerifyBackMetadataSizeOverwritten(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        mem_pointer -= sizeof(uintptr_t);
        mem_pointer--;
        *mem_pointer = 0xAA;

        allocator.FreeBack(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the previous address inside the metadata being overwritten is noticed
    template <class A> bool VerifyMetadataPrevAddressOverwritten(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        mem_pointer -= sizeof(uintptr_t);
        *mem_pointer = *(mem_pointer + alignment);

        allocator.Free(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the previous address inside the metadata being overwritten is noticed (on the back side)
    template <class A> bool VerifyBackMetadataPrevAddressOverwritten(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size, alignment);
        uint8_t *mem_pointer = reinterpret_cast<uint8_t *>(mem);
        mem_pointer -= sizeof(uintptr_t);
        *mem_pointer = *(mem_pointer + alignment);

        allocator.FreeBack(mem);

        if (allocator.IsValid())
        {
            printf("[Error]: Allocator still valid!\n");
            return false;
        }

        return true;
    }

    // Checks if the allocation fails when the allocator is full (from the back side)
    template <class A> bool VerifyNullptrIfFullBack(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.AllocateBack(size / 3 + 1, alignment);
        void *mem2 = allocator.AllocateBack(size / 3 + 1, alignment);
        void *mem3 = allocator.AllocateBack(size / 3 + 1, alignment);

        if (mem == nullptr || mem2 == nullptr)
        {
            printf("[Error]: Allocator was full too early!\n");
            return false;
        }

        if (mem3 != nullptr)
        {
            printf("[Error]: Allocator didn't return nullptr when it should be full!\n");
            return false;
        }

        return true;
    }

    // Checks if the allocation fails when the allocator is full (from the front side)
    template <class A> bool VerifyNullptrIfFullFront(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size / 3 + 1, alignment);
        void *mem2 = allocator.Allocate(size / 3 + 1, alignment);
        void *mem3 = allocator.Allocate(size / 3 + 1, alignment);

        if (mem == nullptr || mem2 == nullptr)
        {
            printf("[Error]: Allocator was full too early!\n");
            return false;
        }

        if (mem3 != nullptr)
        {
            printf("[Error]: Allocator didn't return nullptr when it should be full!\n");
            return false;
        }

        return true;
    }

    // Checks if the allocation fails when the allocator is full (from the both sides)
    template <class A> bool VerifyNullptrIfFullMixed(A &allocator, size_t size, size_t alignment)
    {
        void *mem = allocator.Allocate(size / 3 + 1, alignment);
        void *mem2 = allocator.AllocateBack(size / 3 + 1, alignment);
        void *mem3 = allocator.Allocate(size / 3 + 1, alignment);
        void *mem4 = allocator.AllocateBack(size / 3 + 1, alignment);

        if (mem == nullptr || mem2 == nullptr)
        {
            printf("[Error]: Allocator was full too early!\n");
            return false;
        }

        if (mem3 != nullptr || mem4 != nullptr)
        {
            printf("[Error]: Allocator didn't return nullptr when it should be full!\n");
            return false;
        }

        return true;
    }

} // namespace Tests


// If set to 1, Free() and FreeBack() should assert if the memory canaries are corrupted
#define WITH_DEBUG_CANARIES 1

// Using mainly: https://docs.microsoft.com/en-us/windows/win32/memory/reserving-and-committing-memory
// Additional info used:
// https://social.msdn.microsoft.com/Forums/vstudio/en-US/117512d6-c485-471e-b48b-30a610881129/how-to-use-virtualalloc?forum=vcgeneral
// https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
// https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo
// https://docs.microsoft.com/en-us/windows/win32/memory/memory-protection-constants
#define USING_VIRTUAL_MEMORY 1

#if WITH_DEBUG_CANARIES
static const uint16_t CANARY = 0x0DD0;
#endif

// Placed in front of the data
struct Metadata
{
    Metadata(size_t content_size, uintptr_t previous_address)
        : content_size(content_size), previous_address(previous_address)
    {
    }
    // Size of the content
    size_t content_size;
    // Address of the previous data (allocated before this data)
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
#if USING_VIRTUAL_MEMORY
        // Look up page size
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        page_size = system_info.dwPageSize;

        // If given size is smaller than a page, use page size instead
        max_size = max(max_size, page_size);

        // Reserve the max size and set its memory protection constants to no access so errors are noticable
        void *begin = VirtualAlloc(NULL, max_size, MEM_RESERVE, PAGE_NOACCESS);

        // Not handling this here in release, because undefined behaviour would only appear in allocate functions where
        // we catch it by returning a nullptr
        assertm(begin != nullptr, "Memory reservation failed!");

#else
        void *begin = malloc(max_size);
        // not handling this here in release, because undefined behaviour would only appear in allocate functions where
        // we catch it by returning a nullptr
        assertm(begin != nullptr, "Malloc failed!");
#endif // USING_VIRTUAL_MEMORY

        // If we have a beginning, set allocator as valid
        if (begin != nullptr)
        {
            is_valid = true;
        }

        // These values will stay constant throughout the object's lifetime (unless grow functionality is added)
        allocation_begin = reinterpret_cast<uintptr_t>(begin);
        allocation_end = allocation_begin + max_size;

#if USING_VIRTUAL_MEMORY
        // setting page starts back one fictious page so first allocations immediately trigger a new commit
        page_start_front = allocation_begin - page_size;
        page_start_back = allocation_end;
#endif // USING_VIRTUAL_MEMORY
       // Initialize the current addresses to the edges of the allocated space
        Reset();
    }
    ~DoubleEndedStackAllocator(void)
    {
#if USING_VIRTUAL_MEMORY
        VirtualFree(reinterpret_cast<void *>(allocation_begin), 0, MEM_RELEASE);
#else
        free(reinterpret_cast<void *>(allocation_begin));
#endif // USING_VIRTUAL_MEMORY
    }

    // Copy and Move Constructors / Assignment Operators are explicitly deleted.
    // There are some ways to get them to work, but they all require the user to be very cautious and might be
    // unexpected (especially when called implicitly), so we think it's best to keep the user from invoking them
    // entirely.
    DoubleEndedStackAllocator(const DoubleEndedStackAllocator &other) = delete;             // Copy Constructor
    DoubleEndedStackAllocator(DoubleEndedStackAllocator &&other) = delete;                  // Move Constructor
    DoubleEndedStackAllocator &operator=(const DoubleEndedStackAllocator &other) = delete;  // Copy Assignment Operator
    DoubleEndedStackAllocator &operator=(const DoubleEndedStackAllocator &&other) = delete; // Move Assignment Operator

    // Returns `false` if the Allocator is in an unusable state, e.g. because the initial memory allocation failed.
    bool IsValid()
    {
        // Fun fact: casting nullptr to uintptr_t is controversial, so we do it the other way
        // https://stackoverflow.com/questions/60507186/can-nullptr-be-converted-to-uintptr-t-different-compilers-disagree
        // return reinterpret_cast<void *>(allocation_begin) != nullptr;
        return is_valid;
    }

    /**
     * Memory layout of content is:
     * ...[previous Content][previous Canary][   ][Canary][Metadata][Content][Canary][   ][next
     *Canary][nextMetadata].... Pointer points to border of Metadata and Content. [Content] Block will be on aligned
     *adress. This means there might be unused space (note the [   ] blocks above) before Metadata.
     **/

    // Alignment must be a power of two.
    // Returns a nullptr if there is not enough memory left.
    void *Allocate(size_t size, size_t alignment)
    {
        if (reinterpret_cast<void *>(next_free_address_front) == nullptr)
        {
            assertm(false, "Allocator did not allocate any memory");
            return nullptr;
        }
        // Check for power of two
        if (!alignment || (alignment & (alignment - 1)))
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
        if (aligned_address + size + sizeof(CANARY) > next_free_address_back)
#else
        if (aligned_address + size > next_free_address_back)
#endif
        {
            // Overlap -> out of space!
            assertm(false, "Allocate failed due to lack of space!");
            return nullptr;
        }

#if USING_VIRTUAL_MEMORY
#if WITH_DEBUG_CANARIES

        // while there is not enough space left on the current page
        while (aligned_address + size + sizeof(CANARY) > page_start_front + page_size)
#else
        while (aligned_address + size > page_start_front + page_size)
#endif
        {
            // try to commit another page
            if (!VirtualAlloc(reinterpret_cast<void *>(page_start_front + page_size), page_size, MEM_COMMIT,
                              PAGE_READWRITE))
            {
                // edgecase: there is enough space for the allocation from the current page on the other side but not
                // enough space to commit another page from this side
                // if that is not the case something else went wrong
                if (page_start_front + page_size < page_start_back)
                {
                    assertm(false, "Front page commit failed!");
                    return nullptr;
                }
            }

            page_start_front += page_size;
        }
#endif // USING_VIRTUAL_MEMORY

        // Allocate using correct offeset address (provide prev address)
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
    void *AllocateBack(size_t size, size_t alignment)
    {
        if (reinterpret_cast<void *>(next_free_address_back) == nullptr)
        {
            assertm(false, "Allocator did not allocate any memory");
            return nullptr;
        }
        // Check for power of two

        if (!alignment || (alignment & (alignment - 1)))
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

        uintptr_t aligned_address = Align(offset_address, -int64_t(alignment));
#if WITH_DEBUG_CANARIES
        if (aligned_address - sizeof(Metadata) - sizeof(CANARY) < next_free_address_front)
#else
        if (aligned_address - sizeof(Metadata) < next_free_address_front)
#endif
        {
            // Overlap -> out of space!
            assertm(false, "AllocateBack failed due to lack of space!");
            return nullptr;
        }

#if USING_VIRTUAL_MEMORY
#if WITH_DEBUG_CANARIES
        // while there is not enough space left on the current page
        while (aligned_address - sizeof(Metadata) - sizeof(CANARY) < page_start_back)
#else
        while (aligned_address - sizeof(Metadata) > page_start_back)
#endif
        {
            // commit another page
            if (!VirtualAlloc(reinterpret_cast<void *>(page_start_back - page_size), page_size, MEM_COMMIT,
                              PAGE_READWRITE))
            {
                // edgecase: there is enough space for the allocation from the current page on the other side but not
                // enough space to commit another page from this side
                // if that is not the case something else went wrong
                if (page_start_back - page_size > page_start_back + page_size)
                {
                    assertm(false, "Back page commit failed!");
                    return nullptr;
                }
            }
            page_start_back -= page_size;
        }
#endif // USING_VIRTUAL_MEMORY

        // Allocate with negative alignment and correct offset address (provide prev address)
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
        uintptr_t previous_address = metadata->previous_address;
        // Read data from new front
        Metadata *previous_metadata = ReadMetadata(previous_address);

#if WITH_DEBUG_CANARIES
        // Check canary before
        if (!IsCanaryValid(last_data_begin_address_front - sizeof(Metadata) - sizeof(CANARY)))
        {
            assertm(false, "First Canary was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check if content size was overwritten
        if (last_data_begin_address_front + metadata->content_size > allocation_end)
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check canary after
        if (!IsCanaryValid(last_data_begin_address_front + metadata->content_size))
        {
            assertm(false, "Second Canary was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check validity of previous address with canaries
        // Check if address is outside allocator range
        if (previous_address > allocation_end || previous_address < allocation_begin)
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Canary before prev address
        if (previous_address - sizeof(Metadata) - sizeof(CANARY) < allocation_begin ||
            !IsCanaryValid(previous_address - sizeof(Metadata) - sizeof(CANARY)))
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Canary after prev address
        if (previous_address + previous_metadata->content_size > allocation_end ||
            !IsCanaryValid(previous_address + previous_metadata->content_size))
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }
#endif

        // If beginning reached, set free address to beginning
        if (previous_address == allocation_begin)
        {
            last_data_begin_address_front = allocation_begin;
            next_free_address_front = allocation_begin;
            return;
        }

        // Set current to previous address
        last_data_begin_address_front = metadata->previous_address;

        // Add data size from new front
        next_free_address_front = last_data_begin_address_front + previous_metadata->content_size;

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
        uintptr_t previous_address = metadata->previous_address;
        Metadata *previous_metadata = ReadMetadata(previous_address);

#if WITH_DEBUG_CANARIES
        // Check canary before
        if (!IsCanaryValid(last_data_begin_address_back - sizeof(Metadata) - sizeof(CANARY)))
        {
            assertm(false, "First Canary was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check if content size was overwritten
        if (last_data_begin_address_back + metadata->content_size > allocation_end)
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check canary after
        if (!IsCanaryValid(last_data_begin_address_back + metadata->content_size))
        {
            assertm(false, "Second Canary was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Check validity of previous address with canaries
        // Check if address is outside allocator range
        if (previous_address > allocation_end || previous_address < allocation_begin)
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Canary before prev address
        if (previous_address - sizeof(Metadata) - sizeof(CANARY) < allocation_begin ||
            !IsCanaryValid(previous_address - sizeof(Metadata) - sizeof(CANARY)))
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

        // Canary after prev address
        if (previous_address + previous_metadata->content_size > allocation_end ||
            !IsCanaryValid(previous_address + previous_metadata->content_size))
        {
            assertm(false, "Metadata was overwritten - the memory is corrupted!");
            is_valid = false;
            return;
        }

#endif

        // If end reached, set free address to end
        if (previous_address == allocation_end)
        {
            last_data_begin_address_back = allocation_end;
            next_free_address_back = allocation_end;
            return;
        }

        // Set current to previous address
        last_data_begin_address_back = previous_address;

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
#if USING_VIRTUAL_MEMORY
    DWORD page_size;
    uintptr_t page_start_front;
    uintptr_t page_start_back;
#endif
    // The start address of the allocator
    uintptr_t allocation_begin;
    // The end address of the allocator (for fixed size)
    uintptr_t allocation_end;

    uintptr_t last_data_begin_address_front;
    uintptr_t last_data_begin_address_back;

    uintptr_t next_free_address_front;
    uintptr_t next_free_address_back;

    bool is_valid = false;

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

#define RUN_TESTS 1
int main()
{
// You can add your own tests here, I will call my tests at then end with a fresh instance of your allocator and a
// specific max_size
#if RUN_TESTS
    {
        // You can remove this, just showcasing how the test functions can be used
        DoubleEndedStackAllocator allocator(1024u);

        Tests::Test_Case_Success("Allocate() successful", Tests::VerifyAllocationSuccess(allocator, 32, 4));
        Tests::Test_Case_Success("AllocateBack() successful", Tests::VerifyAllocationBackSuccess(allocator, 32, 8));
        Tests::Test_Case_Success("Free() successful", Tests::VerifyFreeSuccess(allocator, 32, 4));
        Tests::Test_Case_Success("FreeBack() successful", Tests::VerifyFreeBackSuccess(allocator, 32, 8));

        DoubleEndedStackAllocator fullback(1024u);
        Tests::Test_Case_Success("nullptr is returned as soon as the back is too full",
                                 Tests::VerifyNullptrIfFullBack(fullback, 1024, 8));

        DoubleEndedStackAllocator fullfront(1024u);
        Tests::Test_Case_Success("nullptr is returned as soon as the front is too full",
                                 Tests::VerifyNullptrIfFullFront(fullfront, 1024, 8));

        DoubleEndedStackAllocator fullmixed(1024u);
        Tests::Test_Case_Success("nullptr is returned as soon as the middle is too full",
                                 Tests::VerifyNullptrIfFullFront(fullmixed, 1024, 8));

#if WITH_DEBUG_CANARIES
        // FAILURE Tests
        DoubleEndedStackAllocator a(1024u);
        Tests::Test_Case_Success("Canary before overwritten", Tests::VerifyCanaryBeforeFailure(a, 32, 8));
        DoubleEndedStackAllocator b(1024u);
        Tests::Test_Case_Success("Canary after overwritten", Tests::VerifyCanaryAfterFailure(b, 32, 8));
        DoubleEndedStackAllocator c(1024u);
        Tests::Test_Case_Success("Canary back before overwritten", Tests::VerifyBackCanaryBeforeFailure(c, 32, 8));
        DoubleEndedStackAllocator d(1024u);
        Tests::Test_Case_Success("Canary back after overwritten", Tests::VerifyBackCanaryAfterFailure(d, 32, 8));
        DoubleEndedStackAllocator e(1024u);
        Tests::Test_Case_Success("Metadata size overwritten", Tests::VerifyMetadataSizeOverwritten(e, 32, 8));
        DoubleEndedStackAllocator f(1024u);
        Tests::Test_Case_Success("Metadata address overwritten", Tests::VerifyMetadataPrevAddressOverwritten(f, 32, 8));
        DoubleEndedStackAllocator g(1024u);
        Tests::Test_Case_Success("Metadata back size overwritten", Tests::VerifyBackMetadataSizeOverwritten(f, 32, 8));
        DoubleEndedStackAllocator h(1024u);
        Tests::Test_Case_Success("Metadata back address overwritten",
                                 Tests::VerifyBackMetadataPrevAddressOverwritten(f, 32, 8));
#endif

        Tests::Test_Case_Failure("Allocate() does not return nullptr",
                                 [&allocator]() { return allocator.Allocate(32, 5) != nullptr; }());
        Tests::Test_Case_Failure("AllocateBack() does not return nullptr",
                                 [&allocator]() { return allocator.AllocateBack(32, 5) != nullptr; }());
    }
#endif

    // Here the assignment tests will happen - it will test basic allocator functionality.
    {
    }
}