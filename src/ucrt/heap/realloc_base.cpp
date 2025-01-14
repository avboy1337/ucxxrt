//
// realloc_base.cpp
//
//      Copyright (c) Microsoft Corporation. All rights reserved.
//
// Implementation of _realloc_base().  This is defined in a different source
// file from the realloc() function to allow realloc() to be replaced by the
// user.
//
#include <corecrt_internal.h>
#include <malloc.h>
#include <new.h>


extern"C" __declspec(noinline) void* __cdecl ExReallocatePoolWithTag(
    _In_ SIZE_T OldSize,
    _In_ SIZE_T NewSize,
    _In_ PVOID  OldBlock,
    _In_ __drv_strictTypeMatch(__drv_typeExpr) POOL_TYPE PoolType,
    _In_ ULONG Tag
)
{
    if (OldSize == 0)
    {
        return nullptr;
    }

#pragma warning(suppress: 4996)
    void* const NewBlock = ExAllocatePoolWithTag(PoolType, NewSize, Tag);
    if (NewBlock)
    {
        memset (NewBlock, 0, NewSize);
        memmove(NewBlock, OldBlock, OldSize);

        ExFreePoolWithTag(OldBlock, Tag);
        return NewBlock;
    }

    return nullptr;
}


// This function implements the logic of realloc().  It is called directly by
// the realloc() and _recalloc() functions in the Release CRT and is called by
// the debug heap in the Debug CRT.
//
// This function must be marked noinline, otherwise realloc and
// _realloc_base will have identical COMDATs, and the linker will fold
// them when calling one from the CRT. This is necessary because realloc
// needs to support users patching in custom implementations.
extern "C" __declspec(noinline) _CRTRESTRICT void* __cdecl _realloc_base(
    void*  const block,
    size_t const size
    )
{
    // If the block is a nullptr, just call malloc:
    if (block == nullptr)
        return _malloc_base(size);

    // If the new size is 0, just call free and return nullptr:
    if (size == 0)
    {
        _free_base(block);
        return nullptr;
    }

    // Ensure that the requested size is not too large:
    _VALIDATE_RETURN_NOEXC(_HEAP_MAXREQ >= size, ENOMEM, nullptr);

    for (;;)
    {
        void* const new_block = ExReallocatePoolWithTag(
            _msize_base(block), size, block, NonPagedPool, __ucxxrt_tag);
        if (new_block)
        {
            return new_block;
        }

        // Otherwise, see if we need to call the new handler, and if so call it.
        // If the new handler fails, just return nullptr:
        if (_query_new_mode() == 0 || !_callnewh(size))
        {
            errno = ENOMEM;
            return nullptr;
        }

        // The new handler was successful; try to allocate again...
    }
}
