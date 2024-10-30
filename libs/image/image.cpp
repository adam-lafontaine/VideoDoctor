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

        span::transform(to_span(src), to_span(dst), func);
    }
}


/* map */

namespace image
{
    void map(GrayView const& src, ImageView const& dst)
    {
        assert(src.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.matrix_data_);
        assert(dst.width);
        assert(dst.height);
        assert(src.width == dst.width);
        assert(src.height == dst.height);

        fn<Pixel(u8)> func = [](u8 sp)
        {
            Pixel p{};
            p.red = sp;
            p.green = sp;
            p.blue = sp;
            p.alpha = 255;

            return p;
        };

        span::transform(to_span(src), to_span(dst), func);
    }
}


namespace image
{
    void scale_down(ImageView const& src, ImageView const& dst, u32 scale)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width == scale * dst.width);
        assert(src.height == scale * dst.height);
        assert(scale > 1);
        
        f32 const i_scale = 1.0f / (scale * scale);

        f32 red = 0.0f;
        f32 green = 0.0f;
        f32 blue = 0.0f;
        f32 alpha = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            auto rd = row_begin(dst, yd);

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                auto xs = scale * xd;

                red   = 0.0f;
                green = 0.0f;
                blue  = 0.0f;
                alpha = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    auto rs = row_begin(src, ys + v) + xs;
                    for (u32 u = 0; u < scale; u++)
                    {
                        auto s = rs[u];
                        red   += i_scale * s.red;
                        green += i_scale * s.green;
                        blue  += i_scale * s.blue;
                        alpha += i_scale * s.alpha;
                    }
                }

                auto& d = rd[xd];
                d.red   = (u8)red;
                d.green = (u8)green;
                d.blue  = (u8)blue;
                d.alpha = (u8)alpha;
            }
        }
    }


    void scale_down(GrayView const& src, GrayView const& dst, u32 scale)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width == scale * dst.width);
        assert(src.height == scale * dst.height);
        assert(scale > 1);
        
        f32 const i_scale = 1.0f / (scale * scale);

        f32 gray = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            auto rd = row_begin(dst, yd);

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                auto xs = scale * xd;

                gray = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    auto rs = row_begin(src, ys + v) + xs;
                    for (u32 u = 0; u < scale; u++)
                    {
                        gray += i_scale * rs[u];
                    }
                }

                rd[xd] = (u8)gray;
            }
        }
    }
}