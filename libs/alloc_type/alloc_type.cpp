#pragma once

#include "alloc_type.hpp"
#include "../span/span.hpp"

#include <cstdlib>
#include <cassert>
#include <vector>
#include <cstdio>

template <typename T>
using List = std::vector<T>;

//#define LOG_ALLOC_TYPE

#if !defined NDEBUG && defined LOG_ALLOC_TYPE

#define alloc_type_log(...) printf(__VA_ARGS__)

#else
#define alloc_type_log(...)
#endif


#ifndef ALLOC_COUNT

namespace mem
{
    void free_any(void* ptr)
    {
        alloc_type_log("free_any(%p)\n", ptr);
        std::free(ptr);
    }


    void tag_any(void* ptr, u32 n_bytes, cstr tag)
    {
        alloc_type_log("tag_any(%p, %u, %s)\n", ptr, n_bytes, tag);
    }


    void untag_any(void* ptr)
    {
        alloc_type_log("untag_any(%s)\n", ptr);
    }

    
    void* malloc_memory(u32 n_elements, u32 element_size, cstr tag)
    {
        alloc_type_log("malloc_memory(%u, %u, %s)\n", n_elements, element_size, tag);

#if defined _WIN32

        return std::malloc(n_elements * element_size);

#else
        size_t alignment = 1;

        switch (element_size)
        {
        case 2:
        case 4:
        case 8:
            alignment = element_size;
            return std::aligned_alloc(alignment, n_elements * element_size);
            break;
        
        default:
            return std::malloc(n_elements * element_size);
        }

#endif
    }


    void free_memory(void* ptr, u32 element_size)
    {
        alloc_type_log("free_memory(%p, %u)\n", ptr, element_size);
        std::free(ptr);
    }


    void tag_memory(void* ptr, u32 n_elements, u32 element_size, cstr tag)
    {
        alloc_type_log("tag_memory(%p, %u, %u, %s)\n", ptr, n_elements, element_size, tag);
    }


    void tag_file_memory(void* ptr, u32 element_size, cstr file_path)
    {
        alloc_type_log("tag_file_memory(%p, %u, %s)", ptr, n_elements, file_path);
    }


    void untag_memory(void* ptr, u32 element_size)
    {
        alloc_type_log("untag_memory(%p, %u)\n", ptr, element_size);
    }
}

#else


namespace counts
{
    constexpr auto NO_TAG = "no tag";


    static constexpr cstr bit_width_str(u32 size)
    {
        switch (size)
        {
        case 1: return "8 bit";
        case 2: return "16 bit";
        case 4: return "32 bit";
        case 8: return "64 bit";    
        default: return "void/any";
        }
    }


    template <size_t MAX_ALLOC>
    class AllocLog
    {
    public:
        List<cstr> tags;
        List<cstr> actions;
        List<u32> sizes;
        List<u32> n_allocs;
        List<u32> n_bytes;

        AllocLog()
        {
            static_assert(MAX_ALLOC <= mem::AllocationStatus::MAX_SLOTS);

            tags.reserve(MAX_ALLOC);
            actions.reserve(MAX_ALLOC);
            sizes.reserve(MAX_ALLOC);
            n_allocs.reserve(MAX_ALLOC);
            n_bytes.reserve(MAX_ALLOC);
        }
    };


    template <size_t ELE_SZ, size_t MAX_ALLOC>
    class AllocCounts
    {
    public:
        static constexpr u32 element_size = ELE_SZ ? ELE_SZ : 1;
        static constexpr u32 max_allocations = MAX_ALLOC;

        cstr type_name = bit_width_str(ELE_SZ);

        void* keys[max_allocations] = { 0 };
        u32 byte_counts[max_allocations] = { 0 };
        u32 element_counts[max_allocations] = { 0 };
        cstr tags[max_allocations] = { 0 };

        u32 bytes_allocated = 0;
        u32 elements_allocated = 0;
        u32 n_allocations = 0;

        AllocLog<MAX_ALLOC> log;
    };


    template <size_t ELE_SZ, size_t MAX_ALLOC>
    static void update_element_counts(AllocCounts<ELE_SZ, MAX_ALLOC>& ac, u32 slot)
    {
        ac.elements_allocated = ac.bytes_allocated / ac.element_size;
        ac.element_counts[slot] = ac.byte_counts[slot] / ac.element_size;
    }
}


namespace counts
{
    template <size_t ELE_SZ, size_t MAX_ALLOC>
    static void log_alloc(AllocCounts<ELE_SZ, MAX_ALLOC>& ac, cstr action, u32 slot)
    {
        ac.log.tags.push_back(ac.tags[slot]);
        ac.log.actions.push_back(action);
        ac.log.sizes.push_back(ac.byte_counts[slot]);
        ac.log.n_allocs.push_back(ac.n_allocations);
        ac.log.n_bytes.push_back(ac.bytes_allocated);

        alloc_type_log("%s<%u> %s | %u/%u (%u)\n", action, ac.element_size, ac.tags[slot], ac.n_allocations, ac.max_allocations, ac.bytes_allocated);
    }


    template <class AC>
    static void* add_allocation(AC& ac, u32 n_elements, cstr tag)
    {
        static_assert(ac.max_allocations <= mem::AllocationStatus::MAX_SLOTS);

        // find next available slot
        u32 i = 0; 
        for (;ac.keys[i] && i < ac.max_allocations; i++)
        { }

        assert(i < ac.max_allocations && "Allocation limit reached");
        if (i >= ac.max_allocations || ac.keys[i])
        {
            alloc_type_log("Allocation limit reached (%u)\n", ac.element_size);
            return 0;
        }

        size_t const n_bytes = n_elements * ac.element_size;

        void* data = 0;

        #if defined _WIN32
        data = std::malloc(n_bytes);
        #else
        data = std::aligned_alloc(ac.element_size, n_bytes);
        #endif
        
        assert(data && "Allocation failed");
        if (!data)
        {
            return 0;
        }        

        ac.n_allocations++;
        ac.bytes_allocated += n_bytes;
        ac.keys[i] = data;
        ac.byte_counts[i] = n_bytes;
        ac.tags[i] = tag ? tag : NO_TAG;

        update_element_counts(ac, i);

        log_alloc(ac, "malloc", i);

        return data;
    }


    template <class AC>
    static bool remove_allocation(AC& ac, void* ptr)
    {
        static_assert(ac.max_allocations <= mem::AllocationStatus::MAX_SLOTS);

        // find slot
        u32 i = 0;
        for (; ac.keys[i] != ptr && i < ac.max_allocations; i++)
        { }
        
        if (i >= ac.max_allocations)
        {
            //alloc_type_log("Allocation not found (%u)\n", ac.element_size);
            return false;
        }

        log_alloc(ac, "free", i);

        std::free(ac.keys[i]);

        ac.n_allocations--;
        ac.bytes_allocated -= ac.byte_counts[i];        
        ac.keys[i] = 0;        
        ac.tags[i] = 0;
        ac.byte_counts[i] = 0;

        update_element_counts(ac, i);

        return true;
    }


    template <class AC>
    static void tag_allocation(AC& ac, void* ptr, u32 n_elements, cstr tag)
    {
        static_assert(ac.max_allocations <= mem::AllocationStatus::MAX_SLOTS);

        // find slot, if any
        u32 i = 0;
        for (; ac.keys[i] != ptr && i < ac.max_allocations; i++)
        { }

        if (i < ac.max_allocations)
        {
            // already tagged
            return;
        }

        // find next available slot
        i = 0; 
        for (;ac.keys[i] && i < ac.max_allocations; i++)
        { }

        assert(i < ac.max_allocations && "Allocation limit reached");
        if (i >= ac.max_allocations || ac.keys[i])
        {
            alloc_type_log("Allocation limit reached (%u)\n", ac.element_size);
            return;
        }

        size_t const n_bytes = n_elements * ac.element_size;

        ac.bytes_allocated += n_bytes;
        ac.keys[i] = ptr;
        ac.tags[i] = tag;
        ac.byte_counts[i] = n_bytes;        

        ac.n_allocations++;

        log_alloc(ac, "tagged", i);

        update_element_counts(ac, i);
    }


    template <class AC>
    static bool untag_allocation(AC& ac, void* ptr)
    {
        static_assert(ac.max_allocations <= mem::AllocationStatus::MAX_SLOTS);
        
        // find slot
        u32 i = 0;
        for (; ac.keys[i] != ptr && i < ac.max_allocations; i++)
        { }
        
        if (i >= ac.max_allocations)
        {
            //alloc_type_log("Allocation not found (%u)\n", ac.element_size);
            return false;
        }

        ac.n_allocations--;
        ac.bytes_allocated -= ac.byte_counts[i];        
        ac.keys[i] = 0;                

        log_alloc(ac, "untagged", i);
        ac.tags[i] = 0;
        ac.byte_counts[i] = 0;

        update_element_counts(ac, i);

        return true;
    }
}


/* allocation element sizes */

namespace mem
{
    using Counts_8 = counts::AllocCounts<1, 20>;
    using Counts_16 = counts::AllocCounts<2, 10>;
    using Counts_32 = counts::AllocCounts<4, 20>;
    using Counts_64 = counts::AllocCounts<8, 10>;
    

    Counts_8 alloc_8{};
    Counts_16 alloc_16{};
    Counts_32 alloc_32{};
    Counts_64 alloc_64{};    
}


/* malloc */

namespace mem
{
    void* malloc_memory(u32 n_elements, u32 element_size, cstr tag)
    {
        switch (element_size)
        {
        case 1:
            return counts::add_allocation(alloc_8, n_elements, tag);

        case 2:
            return counts::add_allocation(alloc_16, n_elements, tag);

        case 4:
            return counts::add_allocation(alloc_32, n_elements, tag);

        case 8:
            return counts::add_allocation(alloc_64, n_elements, tag);
        
        default:
            // TODO: custom alignments
            return counts::add_allocation(alloc_8, n_elements * element_size, tag);
        }
    }
}


/* free */

namespace mem
{
    static void free_unknown(void* ptr)
    {
        if (counts::remove_allocation(alloc_8, ptr))
        {
            return;
        }

        if (counts::remove_allocation(alloc_16, ptr))
        {
            return;
        }

        if (counts::remove_allocation(alloc_32, ptr))
        {
            return;
        }

        if (counts::remove_allocation(alloc_64, ptr))
        {
            return;
        }

        std::free(ptr);
    }


    void free_memory(void* ptr, u32 element_size)
    {
        bool result = false;

        switch (element_size)
        {
        case 1:
            result = counts::remove_allocation(alloc_8, ptr);
            break;

        case 2:
            result = counts::remove_allocation(alloc_16, ptr);
            break;

        case 4:
            result = counts::remove_allocation(alloc_32, ptr);
            break;

        case 8:
            result = counts::remove_allocation(alloc_64, ptr);
            break;
        
        default:
            result = counts::remove_allocation(alloc_8, ptr);
            break;
        }

        if (!result)
        {
            free_unknown(ptr);
        }
    }
}


/* tag */

namespace mem
{
    static long file_size(cstr file_path)
    {
        auto file = fopen(file_path, "rb");
        if (!file)
        {
            return 0;
        }

        fseek(file, 0, SEEK_END);

        auto size = ftell(file);
        fclose(file);

        return size;
    }


    static cstr get_file_name(cstr full_path)
    {
        auto str = span::to_string_view(full_path);
        auto c = str.begin + str.length;

        for (; c >= str.begin && *c != '/'; c--)
        { }

        //return (cstr)(c + 1); // no leading '/'
        return (cstr)c; // keep leading '/'
    }


    void tag_memory(void* ptr, u32 n_elements, u32 element_size, cstr tag)
    {
        switch (element_size)
        {
        case 1:
            counts::tag_allocation(alloc_8, ptr, n_elements, tag);
            break;

        case 2:
            counts::tag_allocation(alloc_16, ptr, n_elements, tag);
            break;

        case 4:
            counts::tag_allocation(alloc_32, ptr, n_elements, tag);
            break;

        case 8:
            counts::tag_allocation(alloc_64, ptr, n_elements, tag);
            break;
        
        default:
            counts::tag_allocation(alloc_8, ptr, n_elements * element_size, tag);
            break;
        }
    }


    void tag_file_memory(void* ptr, u32 element_size, cstr file_path)
    {
        alloc_type_log("tag_file_memory(%p, %u, %s)", ptr, n_elements, file_path);

        auto size = file_size(file_path);
        counts::tag_allocation(alloc_8, ptr, size, get_file_name(file_path));
    }
}


/* untag */

namespace mem
{
    void untag_memory(void* ptr, u32 element_size)
    {
        switch (element_size)
        {
        case 1:
            counts::untag_allocation(alloc_8, ptr);
            break;

        case 2:
            counts::untag_allocation(alloc_16, ptr);
            break;

        case 4:
            counts::untag_allocation(alloc_32, ptr);
            break;

        case 8:
            counts::untag_allocation(alloc_64, ptr);
            break;
        
        default:
            counts::untag_allocation(alloc_8, ptr);
            break;
        }
    }
}


namespace mem
{
    template <size_t ELE_SZ, size_t MAX_ALLOC>
    static void set_status(counts::AllocCounts<ELE_SZ, MAX_ALLOC> const& src, AllocationStatus& dst)
    {
        dst.type_name = src.type_name;
        dst.element_size = src.element_size;
        dst.max_allocations = src.max_allocations;

        dst.bytes_allocated = src.bytes_allocated;
        dst.elements_allocated = src.elements_allocated;
        dst.n_allocations = src.n_allocations;

        u32 d = 0;
        for (u32 i = 0; i < src.max_allocations; i++)
        {
            if (src.keys[i])
            {
                dst.slot_tags[d] = src.tags[i];
                dst.slot_sizes[d] = src.byte_counts[i];
                d++;
            }            
        }
    }


    template <size_t ELE_SZ, size_t MAX_ALLOC>
    static void set_history(counts::AllocCounts<ELE_SZ, MAX_ALLOC> const& src, AllocationHistory& dst)
    {
        dst.type_name = src.type_name;
        dst.element_size = src.element_size;
        dst.max_allocations = src.max_allocations;

        auto& log = src.log;

        dst.n_items = (u32)log.tags.size();

        if (dst.n_items)
        {
            dst.tags = (cstr*)log.tags.data();
            dst.actions = (cstr*)log.actions.data();
            dst.sizes = (u32*)log.sizes.data();
            dst.n_allocs = (u32*)log.n_allocs.data();
            dst.n_bytes = (u32*)log.n_bytes.data();
        }
    }


    AllocationStatus query_status(u32 element_size)
    {
        AllocationStatus status{};

        switch (element_size)
        {
        case 1:
            set_status(alloc_8, status);
            break;

        case 2:
            set_status(alloc_16, status);
            break;

        case 4:
            set_status(alloc_32, status);
            break;

        case 8:
            set_status(alloc_64, status);
            break;
        
        default:
            set_status(alloc_8, status);
            break;
        }

        return status;
    }


    AllocationHistory query_history(u32 element_size)
    {
        AllocationHistory history{};

        switch (element_size)
        {
        case 1:
            set_history(alloc_8, history);
            break;

        case 2:
            set_history(alloc_16, history);
            break;

        case 4:
            set_history(alloc_32, history);
            break;

        case 8:
            set_history(alloc_64, history);
            break;
        
        default:
            set_history(alloc_8, history);
            break;
        }

        return history;
    }
}

#endif
