#pragma once

#include "../util/memory_buffer.hpp"
#include "../util/stack_buffer.hpp"

#include <functional>

template <class F>
using fn = std::function<F>;

#define SPAN_STRING


#ifdef SPAN_STRING

//#include "../qsprintf/qsprintf.hpp"
#include "../stb_libs/stb_sprintf.h"


class StringView
{
public:
    char* begin = 0;
    u32 capacity = 0;
    u32 length = 0;
};

#endif

namespace mb = memory_buffer;
namespace sb = stack_buffer;


template <typename T>
class SpanView
{
public:
	T* data = 0;
	u32 length = 0;
};


using ByteView = SpanView<u8>;


namespace span
{
    template <typename T>
    inline SpanView<T> make_view(MemoryBuffer<T> const& buffer)
    {
        SpanView<T> view{};

        view.data = buffer.data_;
        view.length = buffer.capacity_;

        return view;
    }


    template <typename T>
    inline SpanView<T> push_span(MemoryBuffer<T>& buffer, u32 length)
    {
        SpanView<T> view{};

        auto data = mb::push_elements(buffer, length);
        if (data)
        {
            view.data = data;
            view.length = length;
        }

        return view;
    }


    template <typename T, u32 N>
    inline SpanView<T> push_span(StackBuffer<T, N>& buffer, u32 length)
    {
        SpanView<T> view{};

        auto data = sb::push_elements(buffer, length);
        if (data)
        {
            view.data = data;
            view.length = length;
        }

        return view;
    }


    template <typename T>
    inline SpanView<T> to_span(T* data, u32 length)
    {
        SpanView<T> span{};
        span.data = data;
        span.length = length;

        return span;
    }
}


namespace span
{
    void copy_u8(u8* src, u8* dst, u64 len);


    void fill_u8(u8* dst, u8 value, u64 len);


    void fill_u32(u32* dst, u32 value, u64 len);


    template <typename T>
    inline void copy(SpanView<T> const& src, SpanView<T> const& dst)
    {
        copy_u8((u8*)src.data, (u8*)dst.data, src.length * sizeof(T));
    }


    template <typename T>
    inline void fill_32(SpanView<T> const& dst, T value)
    {
        static_assert(sizeof(T) == sizeof(u32));
        auto val = *((u32*)&value);
        fill_u32((u32*)dst.data, val, dst.length);
    }


    template <typename T>
    inline void fill_8(SpanView<T> const& dst, T value)
    {
        static_assert(sizeof(T) == sizeof(u8));

        auto val = *((u8*)&value);
        fill_u8((u8*)dst.data, val, dst.length);
    }
    
    
    template <typename T>
	inline void fill(SpanView<T> const& dst, T value)
	{
        T* d = dst.data;
		for (u32 i = 0; i < dst.length; ++i)
		{
			d[i] = value;
		}
	}
}


namespace span
{    
    inline void add(SpanView<f32> const& a, SpanView<f32> const& b, SpanView<f32> const& dst)
    {
        auto len = a.length; // == b.length == dst.length

        for (u32 i = 0; i < len; i++)
        {
            dst.data[i] = a.data[i] + b.data[i];
        }
    }
    

    inline void sub(SpanView<f32> const& a, SpanView<f32> const& b, SpanView<f32> const& dst)
    {
        auto len = a.length; // == b.length == dst.length

        for (u32 i = 0; i < len; i++)
        {
            dst.data[i] = a.data[i] - b.data[i];
        }
    }
    

    inline f32 dot(SpanView<f32> const& a, SpanView<f32> const& b)
    {
        auto len = a.length; // == b.length

        f32 res = 0.0f;
        for (u32 i = 0; i < len; i++)
        {
            res += a.data[i] * b.data[i];
        }

        return res;
    }
}


#ifdef SPAN_STRING

/* string view */

namespace span
{
    inline constexpr u32 strlen(cstr text)
    {
        u32 len = 0;

        for (; text[len]; len++) {}

        return len;
    }


    inline cstr to_cstr(StringView const& view)
    {
        return (cstr)view.begin;
    }


    inline constexpr StringView to_string_view(cstr text)
    {
        StringView view{};

        view.begin = (char*)text;
        view.capacity = strlen(text);
        view.length = view.capacity;

        return view;
    }


    inline void zero_string(StringView& view)
    {
        view.length = 0;

        fill_u8((u8*)view.begin, 0, view.capacity);
    }


    inline StringView make_view(u32 capacity, MemoryBuffer<u8>& buffer)
    {
        StringView view{};

        auto data = mb::push_elements(buffer, capacity);
        if (data)
        {
            view.begin = (char*)data;
            view.capacity = capacity;
            view.length = 0;
            
            zero_string(view);
        }

        return view;
    }


    inline StringView make_view(u32 capacity, char* buffer)
    {
        StringView view{};

        view.begin = buffer;
        view.capacity = capacity;
        view.length = 0;

        return view;
    }


    inline void set_length(StringView& view)
    {
        view.length = view.capacity;

        for (u32 i = 0; i < view.capacity; i++)
        {
            if (!view.begin[i])
            {
                view.length = i;
                return;
            }
        }
    }


    template <typename... VA_ARGS>
    inline void sprintf(StringView& view, cstr fmt, VA_ARGS... va_args)
    {
        view.length = (u32)stb_snprintf(view.begin, (int)view.capacity, fmt, va_args...);
    }

}

#endif // SPAN_STRING


/* transform */

namespace span
{
    template <typename S, typename D, class FUNC>
    inline void transform(SpanView<S> const& src, SpanView<D> const& dst, FUNC const& func)
    {
        auto len = src.length;
        constexpr auto N = 8;

        assert(len % N == 0);

        auto s = src.data;
        auto d = dst.data;

        for (u32 i = 0; i < len; i += N)
        {
            d[i] = func(s[i]);
            d[i + 1] = func(s[i + 1]);
            d[i + 2] = func(s[i + 2]);
            d[i + 3] = func(s[i + 3]);
            d[i + 4] = func(s[i + 4]);
            d[i + 5] = func(s[i + 5]);
            d[i + 6] = func(s[i + 6]);
            d[i + 7] = func(s[i + 7]);
        }
    }


    template <typename S1, typename S2, typename D, class FUNC>
    inline void transform(SpanView<S1> const& src1, SpanView<S2> const& src2, SpanView<D> const& dst, FUNC const& func)
    {
        auto len = src1.length;
        constexpr auto N = 8;

        assert(len % N == 0);

        auto s1 = src1.data;
        auto s2 = src2.data;
        auto d = dst.data;

        for (u32 i = 0; i < len; i += N)
        {
            d[i] = func(s1[i], s2[i]);
            d[i + 1] = func(s1[i + 1], s2[i + 1]);
            d[i + 2] = func(s1[i + 2], s2[i + 2]);
            d[i + 3] = func(s1[i + 3], s2[i + 3]);
            d[i + 4] = func(s1[i + 4], s2[i + 4]);
            d[i + 5] = func(s1[i + 5], s2[i + 5]);
            d[i + 6] = func(s1[i + 6], s2[i + 6]);
            d[i + 7] = func(s1[i + 7], s2[i + 7]);
        }
    }
}