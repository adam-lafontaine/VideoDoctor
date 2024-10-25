#pragma once

#include "image.hpp"
#include "../util/numeric.hpp"

namespace sp = span;
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

        sp::fill_32(to_span(view), color);
    }


    void fill(SubView const& view, Pixel color)
    {
        assert(view.matrix_data_);

        for (u32 y = 0; y < view.height; y++)
        {
            sp::fill_32(row_span(view, y), color);
        }
    }
}


/* gradient */

namespace image
{
    void gradient_x(GrayView const& src, GrayView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width - 2);
        assert(dst.height == src.height - 2);

        /*
        constexpr f32 grad[9] = {
            -0.25f,  0.0f,  0.25f,
            -0.50f,  0.0f,  0.50f,
            -0.25f,  0.0f,  0.25f,
        };
        */

        auto r1 = row_begin(src, 0);
        auto r2 = row_begin(src, 1);
        auto r3 = row_begin(src, 2);
        auto d = row_begin(dst, 0);

        for (u32 y = 0; y < dst.height; y++)
        {
            for (u32 x = 0; x < dst.width; x++)
            {
                auto g = 0.25f * (r1[x + 2] - r1[x] + r3[x + 2] - r3[x]) + 0.5f * (r2[x + 2] - r2[x]);
                d[x] = num::round_to_unsigned<u8>(g);
            }

            r1 += src.width;
            r2 += src.width;
            r3 += src.width;
            d += dst.width;
        }
    }


    void gradient_y(GrayView const& src, GrayView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width - 2);
        assert(dst.height == src.height - 2);

        /*
        constexpr f32 grad[9] = {
            -0.25f, -0.50f, -0.25f,
             0.0f,   0.0f,   0.0f,
             0.25f,  0.50f,  0.25f,
        };
        */

        auto r1 = row_begin(src, 0);
        auto r2 = row_begin(src, 1);
        auto r3 = row_begin(src, 2);
        auto d = row_begin(dst, 0);

        auto sw = src.width;
        auto dw = dst.width;

        for (u32 y = 0; y < dst.height; y++)
        {
            for (u32 x = 0; x < dst.width; x++)
            {
                auto g = 0.25f * (r3[x] - r1[x] + r3[x + 2] - r1[x + 2]) + 0.5f * (r3[x + 1] - r1[x + 1]);
                d[x] = num::round_to_unsigned<u8>(g);
            }

            r1 += sw;
            r2 += sw;
            r3 += sw;
            d += dw;
        }
    }
}


/* scale */

namespace image
{
    void scale_down_max(GrayView const& src, GrayView const& dst)
    {
        constexpr u32 scale = 2;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width / scale);
        assert(dst.height == src.height / scale);

        auto r1 = row_begin(src, 0);
        auto r2 = row_begin(src, 1);
        auto d = row_begin(dst, 0);

        auto sw = scale * src.width;
        auto dw = dst.width;

        for (u32 y = 0; y < dst.height; y++)
        {
            u32 sx = 0;
            for (u32 x = 0; x < dst.width; x++)
            {
                d[x] = num::max(r1[sx], r1[sx + 1], r2[sx], r2[sx + 1]);
                sx += scale;
            }

            r1 += sw;
            r2 += sw;
            d += dw;
        }
    }
}