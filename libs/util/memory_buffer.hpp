#pragma once

#include "../alloc_type/alloc_type.hpp"

#include <cassert>


template <typename T>
class MemoryBuffer
{
public:
	T* data_ = nullptr;
	u32 capacity_ = 0;
	u32 size_ = 0;

	bool ok = false;
};


namespace memory_buffer
{
	template <typename T>
	inline bool create_buffer(MemoryBuffer<T>& buffer, u32 n_elements, cstr tag)
	{
		assert(n_elements > 0);
		assert(!buffer.data_);

		if (n_elements == 0 || buffer.data_)
		{
			return false;
		}

		buffer.data_ = mem::malloc<T>(n_elements, tag);
		assert(buffer.data_);

		if (!buffer.data_)
		{
			return false;
		}

		buffer.capacity_ = n_elements;
		buffer.size_ = 0;
		buffer.ok = true;

		return true;
	}


	/*template <typename T>
	inline bool create_buffer(MemoryBuffer<T>& buffer, u32 n_elements)
	{
		return create_buffer(buffer, n_elements, "create_buffer");
	}*/


	template <typename T>
	inline void destroy_buffer(MemoryBuffer<T>& buffer)
	{
		if (buffer.data_)
		{
			mem::free(buffer.data_);
		}		

		buffer.data_ = nullptr;
		buffer.capacity_ = 0;
		buffer.size_ = 0;
		buffer.ok = 0;
	}
	

	template <typename T>
	inline void reset_buffer(MemoryBuffer<T>& buffer)
	{
		buffer.size_ = 0;
	}


	template <typename T>
	inline void zero_buffer(MemoryBuffer<T>& buffer)
	{
		for (u32 i = 0; i < buffer.capacity_; i++)
		{
			buffer.data_[i] = (T)0;
		}
	}


	template <typename T>
	inline T* push_elements(MemoryBuffer<T>& buffer, u32 n_elements)
	{
		assert(n_elements > 0);

		if (n_elements == 0)
		{
			return nullptr;
		}

		assert(buffer.data_);
		assert(buffer.capacity_);

		auto is_valid =
			buffer.data_ &&
			buffer.capacity_ &&
			buffer.size_ < buffer.capacity_;

		auto elements_available = (buffer.capacity_ - buffer.size_) >= n_elements;
		assert(elements_available > 0);

		if (!is_valid || !elements_available)
		{
			return nullptr;
		}

		auto data = buffer.data_ + buffer.size_;

		buffer.size_ += n_elements;

		return data;
	}


	template <typename T>
	inline void pop_elements(MemoryBuffer<T>& buffer, u32 n_elements)
	{
		if (!n_elements)
		{
			return;
		}

		assert(buffer.data_);
		assert(buffer.capacity_);
		assert(n_elements <= buffer.size_);

		if(n_elements > buffer.size_)
		{
			buffer.size_ = 0;
		}
		else
		{
			buffer.size_ -= n_elements;
		}
	}
}