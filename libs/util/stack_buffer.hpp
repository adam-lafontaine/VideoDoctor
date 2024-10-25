#pragma once

#include <cassert>
#include <cstddef>

using u32 = unsigned;


template <class T, size_t N>
class StackBuffer
{
public:
    T data_[N] = { 0 };
    static constexpr u32 capacity_ = N;

    u32 size = 0;
};


namespace stack_buffer
{
    template <class T, size_t N>
    inline void reset_buffer(StackBuffer<T, N>& buffer)
    {
        buffer.size = 0;
    }


    template <class T, size_t N>
    inline T* at(StackBuffer<T, N> const& buffer, u32 i)
    {
        assert(i < buffer.size);

        return (T*)(buffer.data_) + i;
    }


    template <class T, size_t N>
    inline void push(StackBuffer<T, N>& buffer, T item)
    {
        assert(buffer.size < buffer.capacity_);

        buffer.data_[buffer.size] = item;
        buffer.size++;
    }


    template <class T, size_t N>    
    inline T* push_elements(StackBuffer<T, N>& buffer, u32 n_elements)
    {
        assert(n_elements);

		if (n_elements == 0)
		{
			return nullptr;
		}
        
		assert(buffer.capacity_);

		auto is_valid =
			buffer.capacity_ &&
			buffer.size < buffer.capacity_;
        
        assert(is_valid);

		auto elements_available = (buffer.capacity_ - buffer.size) >= n_elements;
		assert(elements_available);

		if (!is_valid || !elements_available)
		{
			return nullptr;
		}

		auto data = buffer.data_ + buffer.size;

		buffer.size += n_elements;

		return data;
    }


    template <class T, size_t N, class FN>
    inline void for_each(StackBuffer<T, N> const& buffer, FN const& func)
    {
        for (u32 i = 0; i < buffer.size; i++)
        {
            func(buffer.data_[i]);
        }
    }
}