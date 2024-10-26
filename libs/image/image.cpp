#pragma once

#include "image.hpp"
#include "../util/numeric.hpp"

namespace num = numeric;


namespace image
{
    bool create_image(Image& image, u32 width, u32 height, cstr tag)
    {
        auto data = mem::malloc<Pixel>(width * height, tag);
        if (!data)
        {
            return false;
        }

        image.data_ = data;
        image.width = width;
        image.height = height;

        return true;
    }

    
    void destroy_image(Image& image)
    {
        if (image.data_)
		{
			mem::free(image.data_);
			image.data_ = nullptr;
		}

		image.width = 0;
		image.height = 0;
    }
}


/* make_view */

namespace image
{
    ImageView make_view(Image const& image)
    {
        ImageView view{};

        view.width = image.width;
        view.height = image.height;
        view.matrix_data_ = image.data_;

        return view;
    }


    ImageView make_view(u32 width, u32 height, Buffer32& buffer)
    {
        ImageView view{};

        view.matrix_data_ = mb::push_elements(buffer, width * height);
        if (view.matrix_data_)
        {
            view.width = width;
            view.height = height;
        }

        return view;
    }


    GrayView make_view(u32 width, u32 height, Buffer8& buffer)
    {
        GrayView view{};

        view.matrix_data_ = mb::push_elements(buffer, width * height);
        if (view.matrix_data_)
        {
            view.width = width;
            view.height = height;
        }

        return view;
    }
}


/* fill */

namespace image
{
    void fill(ImageView const& view, Pixel color)
    {
        assert(view.matrix_data_);

        span::fill_32(to_span(view), color);
    }


    void fill(SubView const& view, Pixel color)
    {
        assert(view.matrix_data_);

        for (u32 y = 0; y < view.height; y++)
        {
            span::fill_32(row_span(view, y), color);
        }
    }
}


/* copy */

namespace image
{
    template <class VIEW_S, class VIEW_D>
    static void copy_view(VIEW_S const& src, VIEW_D const& dst)
    {
        span::copy(to_span(src), to_span(dst));
    }


    template <class VIEW_S, class VIEW_D>
    static void copy_sub_view(VIEW_S const& src, VIEW_D const& dst)
    {
        for (u32 y = 0; y < src.height; y++)
        {
            span::copy(row_span(src, y), row_span(dst, y));
        }
    }


    void copy(ImageView const& src, ImageView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width);
        assert(dst.height == src.height);

        copy_view(src, dst);
    }    


    void copy(ImageView const& src, SubView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width);
        assert(dst.height == src.height);

        copy_sub_view(src, dst);
    }


    void copy(SubView const& src, ImageView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width);
        assert(dst.height == src.height);

        copy_sub_view(src, dst);
    }


    void copy(SubView const& src, SubView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width);
        assert(dst.height == src.height);

        copy_sub_view(src, dst);
    }
}


/* transform */

namespace image
{
    void transform(ImageView const& src, ImageView const& dst, fn<Pixel(Pixel)> const& func)
    {
        assert(src.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.matrix_data_);
        assert(dst.width);
        assert(dst.height);
        assert(src.width == dst.width);
        assert(src.height == dst.height);

        auto s = to_span(src).data;
        auto d = to_span(dst).data;

        auto len = src.width * src.height;

        for (u32 i = 0; i < len; i++)
        {
            d[i] = func(s[i]);
        }
    }
}