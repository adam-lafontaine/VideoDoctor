#pragma once

#include "../util/types.hpp"


//#define ALLOC_COUNT


namespace mem
{    
    void* malloc_memory(u32 n_elements, u32 element_size, cstr tag);

    void free_memory(void* ptr, u32 element_size);    

    void tag_memory(void* ptr, u32 n_elements, u32 element_size, cstr tag);

    void tag_file_memory(void* ptr, u32 element_size, cstr file_path);

    void untag_memory(void* ptr, u32 element_size);
}


namespace mem
{
    template <typename T>
    inline T* malloc(u32 n_elements, cstr tag)
    {
        return (T*)malloc_memory(n_elements, (u32)sizeof(T), tag);
    }


    template <typename T>
    inline T* malloc()
    {
        return malloc<T>(1, "");
    }


    template <typename T>
    inline T* malloc(cstr tag)
    {
        return malloc<T>(1, tag);
    }


    template <typename T>
    inline void free(T* ptr)
    {
        free_memory((void*)ptr, (u32)sizeof(T));
    }


    template <typename T>
    inline void tag(T* data, u32 n_elements, cstr tag)
    {
        tag_memory((void*)data, n_elements, (u32)sizeof(T), tag);
    }


    template <typename T>
    inline void tag_file(T* data, cstr file_path)
    {
        tag_file_memory((void*)data, (u32)sizeof(T), file_path);
    }


    template <typename T>
    inline void untag(T* ptr)
    {
        untag_memory((void*)ptr, (u32)sizeof(T));
    }
}

#ifdef ALLOC_COUNT

namespace mem
{ 

    struct AllocationStatus
    {
        static constexpr u32 MAX_SLOTS = 50;

        cstr type_name = 0;
        u32 element_size = 0;
        u32 max_allocations = 0;

        u32 bytes_allocated = 0;
        u32 elements_allocated = 0;

        u32 n_allocations = 0;

        // TODO: max allocations
        cstr slot_tags[MAX_SLOTS] = { 0 };
        u32 slot_sizes[MAX_SLOTS] = { 0 };
    };


    struct AllocationHistory
    {
        cstr type_name = 0;
        u32 element_size = 0;
        u32 max_allocations = 0;

        u32 n_items = 0;

        cstr* tags = 0;
        cstr* actions = 0;
        u32* sizes = 0;
        u32* n_allocs = 0;
        u32* n_bytes = 0;

    };


    AllocationStatus query_status(u32 element_size);

    AllocationHistory query_history(u32 element_size);
}

#endif