#pragma once

#include "image.hpp"
#include "../util/numeric.hpp"

#include "../stb_libs/stb_image_options.hpp"

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
        assert(view.width);
        assert(view.height);

        span::fill_32(to_span(view), color);
    }


    void fill(SubView const& view, Pixel color)
    {
        assert(view.matrix_data_);
        assert(view.width);
        assert(view.height);

        for (u32 y = 0; y < view.height; y++)
        {
            span::fill_32(row_span(view, y), color);
        }
    }


    void fill(GrayView const& view, u8 value)
    {
        assert(view.matrix_data_);
        assert(view.width);
        assert(view.height);

        span::fill_8(to_span(view), value);
    }


    void fill(GraySubView const& view, u8 value)
    {
        assert(view.matrix_data_);
        assert(view.width);
        assert(view.height);

        for (u32 y = 0; y < view.height; y++)
        {
            span::fill_8(row_span(view, y), value);
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
    template <class SRC, class DST, class FN, typename DT>
    static void transform_scale_up_matrix_t(DT tval, SRC const& src, DST const& dst, u32 scale, FN const& func)
    {
        constexpr u32 SCALE_MAX = 8;

        assert(scale <= SCALE_MAX);
        
        DT p = tval;

        DT* rd[SCALE_MAX] = { 0 };

        for (u32 ys = 0; ys < src.height; ys++)
        {
            auto yd = scale * ys;
            auto rs = row_begin(src, ys);

            for (u32 i = 0; i < scale; i++)
            {
                rd[i] = row_begin(dst, yd + i);
            }

            for (u32 xs = 0; xs < src.width; xs++)
            {                
                p = func(rs[xs]);

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        rd[v][u] = p;
                    }

                    rd[v] += scale;
                }
            }
        }
    }    
    
    
    template <class SRC, class DST, class FN>
    static void transform_scale_up_matrix_s(SRC const& src, DST const& dst, u32 scale, FN const& func)
    {
        for (u32 ys = 0; ys < src.height; ys++)
        {
            auto yd = scale * ys;
            auto rs = row_begin(src, ys);

            for (u32 xs = 0; xs < src.width; xs++)
            {
                auto xd = scale * xs;
                
                auto p = func(rs[xs]);

                for (u32 v = 0; v < scale; v++)
                {
                    auto rd = row_begin(dst, yd + v) + xd;
                    for (u32 u = 0; u < scale; u++)
                    {                        
                        rd[u] = p;
                    }
                }
            }
        }
    }


    template <class SRC1, class SRC2, class DST, class FN, typename DT>
    static void transform_scale_up_matrix_t(DT tval, SRC1 const& src1, SRC2 const& src2, DST const& dst, u32 scale, FN const& func)
    {
        constexpr u32 SCALE_MAX = 8;

        assert(scale <= SCALE_MAX);
        
        DT p = tval;

        DT* rd[SCALE_MAX] = { 0 };

        for (u32 ys = 0; ys < src1.height; ys++)
        {
            auto yd = scale * ys;
            auto rs1 = row_begin(src1, ys);
            auto rs2 = row_begin(src2, ys);

            for (u32 i = 0; i < scale; i++)
            {
                rd[i] = row_begin(dst, yd + i);
            }

            for (u32 xs = 0; xs < src1.width; xs++)
            {                
                p = func(rs1[xs], rs2[xs]);

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        rd[v][u] = p;
                    }

                    rd[v] += scale;
                }
            }
        }
    }


    template <class SRC1, class SRC2, class DST, class FN>
    static void transform_scale_up_matrix_s(SRC1 const& src1, SRC2 const& src2, DST const& dst, u32 scale, FN const& func)
    {
        for (u32 ys = 0; ys < src1.height; ys++)
        {
            auto yd = scale * ys;
            auto rs1 = row_begin(src1, ys);
            auto rs2 = row_begin(src2, ys);

            for (u32 xs = 0; xs < src1.width; xs++)
            {
                auto xd = scale * xs;
                
                auto p = func(rs1[xs], rs2[xs]);

                for (u32 v = 0; v < scale; v++)
                {
                    auto rd = row_begin(dst, yd + v) + xd;
                    for (u32 u = 0; u < scale; u++)
                    {                        
                        rd[u] = p;
                    }
                }
            }
        }
    }


    template <class SRC, class DST, class FN>
    static void transform_scale_up_matrix(SRC const& src, DST const& dst, u32 scale, FN const& func)
    {        
        switch (scale)
        {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        {
            auto tval = row_begin(dst, 0)[0];
            transform_scale_up_matrix_t(tval, src, dst, scale, func);
        }
            break;

        default:
            transform_scale_up_matrix_s(src, dst, scale, func);
            break;
        }
    }


    template <class SRC1, class SRC2, class DST, class FN>
    static void transform_scale_up_matrix(SRC1 const& src1, SRC2 const& src2, DST const& dst, u32 scale, FN const& func)
    {
        switch (scale)
        {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        {
            auto tval = row_begin(dst, 0)[0];
            transform_scale_up_matrix_t(tval, src1, src2, dst, scale, func);
        }
            break;

        default:
            transform_scale_up_matrix_s(src1, src2, dst, scale, func);
            break;
        }
    }


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


    void transform_scale_up(GrayView const& src, ImageView const& dst, fn<Pixel(u8)> const& func)
    {
        auto scale = dst.width / src.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

        transform_scale_up_matrix(src, dst, scale, func);
    }


    void transform_scale_up(GrayView const& src1, GrayView const& src2, ImageView const& dst, fn<Pixel(u8, u8)> const& func)
    {
        auto scale = dst.width / src1.width;

        assert(src1.matrix_data_);
        assert(src2.matrix_data_);
        assert(dst.matrix_data_);
        assert(src1.width == src2.width);
        assert(src1.height == src2.height);
        assert(dst.width == src1.width * scale);
        assert(dst.height == src1.height * scale);
        assert(scale > 1);

        transform_scale_up_matrix(src1, src2, dst, scale, func);
    }
}


/* resize */

namespace image
{
    template <class SRC, class DST>
    void scale_down_rgba_t(SRC const& src, DST const& dst, u32 scale)
    {     
        constexpr u32 SCALE_MAX = 8;

        assert(scale <= SCALE_MAX);

        const f32 i_scale = 1.0f / (scale * scale);

        Pixel* rd = 0;

        Pixel* rs[SCALE_MAX] = { 0 };

        Pixel ps;

        f32 red = 0.0f;
        f32 green = 0.0f;
        f32 blue = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            rd = row_begin(dst, yd);
            for (u32 i = 0; i < scale; i++)
            {
                rs[i] = row_begin(src, ys + i);
            }

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                red   = 0.0f;
                green = 0.0f;
                blue  = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        ps = rs[v][u];
                        red   += ps.red;
                        green += ps.green;
                        blue  += ps.blue;
                    }

                    rs[v] += scale;
                }

                red   *= i_scale;
                green *= i_scale;
                blue  *= i_scale;

                rd[xd] = to_pixel((u8)red, (u8)green, (u8)blue);
            }
        }
    }
    
    
    template <class SRC, class DST>
    static void scale_down_rgba_s(SRC const& src, DST const& dst, u32 scale)
    {
        f32 const i_scale = 1.0f / (scale * scale);

        Pixel* rd = 0;
        Pixel* rs = 0;

        Pixel ps;

        f32 red = 0.0f;
        f32 green = 0.0f;
        f32 blue = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            rd = row_begin(dst, yd);

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                auto xs = scale * xd;

                red   = 0.0f;
                green = 0.0f;
                blue  = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    rs = row_begin(src, ys + v) + xs;
                    for (u32 u = 0; u < scale; u++)
                    {
                        auto s = rs[u];
                        red   += s.red;
                        green += s.green;
                        blue  += s.blue;
                    }
                }

                red   *= i_scale;
                green *= i_scale;
                blue  *= i_scale;

                rd[xd] = to_pixel((u8)red, (u8)green, (u8)blue);
            }
        }
    }


    template <class SRC, class DST>
    static void scale_down_gray_t(SRC const& src, DST const& dst, u32 scale)
    {
        constexpr u32 SCALE_MAX = 8;

        assert(scale <= SCALE_MAX);

        const f32 i_scale = 1.0f / (scale * scale);

        u8* rd = 0;

        u8* rs[SCALE_MAX] = { 0 };

        f32 gray = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            rd = row_begin(dst, yd);
            for (u32 i = 0; i < scale; i++)
            {
                rs[i] = row_begin(src, ys + i);
            }

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                gray = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        gray += rs[v][u];
                    }

                    rs[v] += scale;
                }

                gray *= i_scale;

                rd[xd] = (u8)gray;
            }
        }
    }


    template <class SRC, class DST>
    static void scale_down_gray_s(SRC const& src, DST const& dst, u32 scale)
    {        
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
                        gray += rs[u];
                    }
                }

                gray *= i_scale;

                rd[xd] = (u8)gray;
            }
        }
    }


    template <class SRC, class DST>
    static void scale_down_rgba(SRC const& src, DST const& dst, u32 scale)
    {       
        switch (scale)
        {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            scale_down_rgba_t(src, dst, scale);
            break;

        default:
            scale_down_rgba_s(src, dst, scale);
            break;
        }        
    }


    template <class SRC, class DST>
    static void scale_down_gray(SRC const& src, DST const& dst, u32 scale)
    {       
        switch (scale)
        {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            scale_down_gray_t(src, dst, scale);
            break;

        default:
            scale_down_gray_s(src, dst, scale);
            break;
        }
    }
    

    template <class SRC, class DST, class T>
    void scale_up_matrix_t(T tval, SRC const& src, DST const& dst, u32 scale)
    {   
        constexpr u32 SCALE_MAX = 8;
        
        T ps = tval;

        T* rd[scale] = { 0 };
        
        for (u32 ys = 0; ys < src.height; ys++)
        {
            auto yd = scale * ys;
            auto rs = row_begin(src, ys);

            for (u32 i = 0; i < scale; i++)
            {
                rd[i] = row_begin(dst, yd + i);
            }

            for (u32 xs = 0; xs < src.width; xs++)
            {
                ps = rs[xs];

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        rd[v][u] = ps;
                    }

                    rd[v] += scale;
                }
            }
        }
    }


    template <class SRC, class DST>
    void scale_up_matrix_s(SRC const& src, DST const& dst, u32 scale)
    {        
        for (u32 ys = 0; ys < src.height; ys++)
        {
            auto yd = scale * ys;
            auto rs = row_begin(src, ys);

            for (u32 xs = 0; xs < src.width; xs++)
            {
                auto xd = scale * xs;

                auto p = rs[xs];

                for (u32 v = 0; v < scale; v++)
                {
                    auto rd = row_begin(dst, yd + v) + xd;
                    for (u32 u = 0; u < scale; u++)
                    {
                        rd[u] = p;
                    }
                }
            }
        }
    }


    template <class SRC, class DST>
    void scale_up_matrix(SRC const& src, DST const& dst, u32 scale)
    {
        switch (scale)
        {
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        {
            auto tval = row_begin(src, 0)[0];
            scale_up_matrix_t(tval, src, dst, scale);
        }
            break;

        default:
            scale_up_matrix_s(src, dst, scale);
            break;
        }
    }
    
    
    void scale_down(ImageView const& src, ImageView const& dst)
    {
        auto scale = src.width / dst.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.width);
        assert(dst.height);
        assert(src.width == scale * dst.width);
        assert(src.height == scale * dst.height);
        assert(scale > 1);
        
        scale_down_rgba(src, dst, scale);
    }


    void scale_down(GrayView const& src, GrayView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.width);
        assert(dst.height);

        auto scale = src.width / dst.width;
        
        assert(src.width == scale * dst.width);
        assert(src.height == scale * dst.height);
        assert(scale > 1);
        
        scale_down_gray(src, dst, scale);
    }


    void scale_up(ImageView const& src, ImageView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.width);
        assert(dst.height);

        auto scale = dst.width / src.width;
        
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

        scale_up_matrix(src, dst, scale);
    }


    void scale_up(GrayView const& src, GrayView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width);
        assert(src.height);
        assert(dst.width);
        assert(dst.height);

        auto scale = dst.width / src.width;
        
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

        scale_up_matrix(src, dst, scale);
    }


    void resize(ImageView const& src, ImageView const& dst)
    {        
        assert(src.width);
		assert(src.height);
		assert(src.matrix_data_);
		assert(dst.width);
		assert(dst.height);
        assert(dst.matrix_data_);

		int channels = 4;
        auto layout = stbir_pixel_layout::STBIR_RGBA_NO_AW; // alpha channel doesn't matter

		int width_src = (int)(src.width);
		int height_src = (int)(src.height);
		int stride_bytes_src = width_src * channels;
        u8* data_src = (u8*)src.matrix_data_;

		int width_dst = (int)(dst.width);
		int height_dst = (int)(dst.height);
		int stride_bytes_dst = width_dst * channels;
        u8* data_dst = (u8*)dst.matrix_data_;

        auto data = stbir_resize_uint8_linear(
			data_src, width_src, height_src, stride_bytes_src,
			data_dst, width_dst, height_dst, stride_bytes_dst,
			layout);

		assert(data && " *** stbir_resize_uint8_linear() failed *** ");
    }


    void resize(ImageView const& src, SubView const& dst)
    {        
        assert(src.width);
		assert(src.height);
		assert(src.matrix_data_);
		assert(dst.width);
		assert(dst.height);
        assert(dst.matrix_data_);

		int channels = 4;
        auto layout = stbir_pixel_layout::STBIR_RGBA_NO_AW; // alpha channel doesn't matter

		int width_src = (int)(src.width);
		int height_src = (int)(src.height);
		int stride_bytes_src = width_src * channels;
        u8* data_src = (u8*)src.matrix_data_;

		int width_dst = (int)(dst.width);
		int height_dst = (int)(dst.height);
		int stride_bytes_dst = (int)(dst.matrix_width) * channels;
        u8* data_dst = (u8*)row_begin(dst, 0);

        auto data = stbir_resize_uint8_linear(
			data_src, width_src, height_src, stride_bytes_src,
			data_dst, width_dst, height_dst, stride_bytes_dst,
			layout);

		assert(data && " *** stbir_resize_uint8_linear() failed *** ");
    }


    void resize(GrayView const& src, GrayView const& dst)
    {
        assert(src.width);
		assert(src.height);
		assert(src.matrix_data_);
		assert(dst.width);
		assert(dst.height);
        assert(dst.matrix_data_);

        int channels = 1;
        auto layout = stbir_pixel_layout::STBIR_1CHANNEL;

		int width_src = (int)(src.width);
		int height_src = (int)(src.height);
		int stride_bytes_src = width_src * channels;
        u8* data_src = (u8*)src.matrix_data_;

		int width_dst = (int)(dst.width);
		int height_dst = (int)(dst.height);
		int stride_bytes_dst = width_dst * channels;
        u8* data_dst = (u8*)dst.matrix_data_;

        auto data = stbir_resize_uint8_linear(
			data_src, width_src, height_src, stride_bytes_src,
			data_dst, width_dst, height_dst, stride_bytes_dst,
			layout);

		assert(data && " *** stbir_resize_uint8_linear() failed *** ");
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
            return to_pixel(sp);
        };

        span::transform(to_span(src), to_span(dst), func);
    }


    void map_scale_down(GrayView const& src, ImageView const& dst)
    {
        constexpr u32 SCALE_MAX = 8;
        auto scale = src.width / dst.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width == scale * dst.width);
        assert(src.height == scale * dst.height);
        assert(scale > 1);
        assert(scale <= SCALE_MAX);
        
        f32 const i_scale = 1.0f / (scale * scale);

        Pixel* rd = 0;
        u8* rs[SCALE_MAX] = { 0 };

        f32 gray = 0.0f;

        for (u32 yd = 0; yd < dst.height; yd++)
        {
            auto ys = scale * yd;

            rd = row_begin(dst, yd);
            for (u32 i = 0; i < scale; i++)
            {
                rs[i] = row_begin(src, ys + i);
            }

            for (u32 xd = 0; xd < dst.width; xd++)
            {
                gray = 0.0f;

                for (u32 v = 0; v < scale; v++)
                {
                    for (u32 u = 0; u < scale; u++)
                    {
                        gray += rs[v][u];
                    }

                    rs[v] += scale;
                }

                gray *= i_scale;

                rd[xd] = to_pixel((u8)gray);
            }
        }
    }


    void map_scale_up(GrayView const& src, ImageView const& dst)
    {
        auto scale = dst.width / src.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

        transform_scale_up_matrix(src, dst, scale, [](u8 p){ return to_pixel(p); });
    }
}


/* draw */

namespace image
{
    void draw_rect(ImageView const& view, Rect2Du32 const& rect, Pixel color, u32 thick)
    {
        auto region = sub_view(view, rect);
        auto w = region.width;
        auto h = region.height;
        auto t = thick;

        auto ok = w && h && t;
        if (!ok)
        {
            return;
        }

        auto top    = make_rect(0,     0,     w, t);
        auto bottom = make_rect(0,     h - t, w, t);
        auto left   = make_rect(0,     t,     t, h - 2 * t);
        auto right  = make_rect(w - t, t,     t, h - 2 * t);

        fill(sub_view(region, top), color);
        fill(sub_view(region, bottom), color);
        fill(sub_view(region, left), color);
        fill(sub_view(region, right), color);
    }
}


/* convolve */

namespace image
{
    static constexpr f32 GRAD_X_5X5[25] = 
    {
          0.0f,   0.0f, 0.0f,  0.0f,  0.0f,
        -0.08f, -0.12f, 0.0f, 0.12f, 0.08f
		-0.24f, -0.36f, 0.0f, 0.36f, 0.24f
		-0.08f, -0.12f, 0.0f, 0.12f, 0.08f,
          0.0f,   0.0f, 0.0f,  0.0f,  0.0f,
    };


    static constexpr f32 GRAD_Y_5X5[25] = 
    {
        0.0f, -0.08f, -0.24f, -0.08f, 0.0f,
		0.0f, -0.12f, -0.36f, -0.12f, 0.0f,
		0.0f,   0.0f,   0.0f,   0.0f, 0.0f,
		0.0f,  0.12f,  0.36f,  0.12f, 0.0f,
		0.0f,  0.08f,  0.24f,  0.08f, 0.0f,
    };


    static void gradients_5x5(GraySubView const& src, GraySubView const& dst)
    {
        auto kx0 = (f32*)GRAD_X_5X5;
        auto kx1 = kx0 + 5;
        auto kx2 = kx1 + 5;
        auto kx3 = kx2 + 5;
        auto kx4 = kx3 + 5;

        auto ky0 = (f32*)GRAD_Y_5X5;
        auto ky1 = ky0 + 5;
        auto ky2 = ky1 + 5;
        auto ky3 = ky2 + 5;
        auto ky4 = ky3 + 5;

        u8* src_rows[5] = { 0 };

        auto const set_src_rows = [&](u32 y)
        {
            src_rows[0] = row_begin(src, (i32)y - 2) - 2;
            src_rows[1] = row_begin(src, (i32)y - 1) - 2;
            src_rows[2] = row_begin(src, (i32)y) - 2;
            src_rows[3] = row_begin(src, (i32)y + 1) - 2;
            src_rows[4] = row_begin(src, (i32)y + 2) - 2;
        };

        auto const grad = [&](u32 x)
        {
            auto s0 = src_rows[0] + x;
            auto s1 = src_rows[1] + x;
            auto s2 = src_rows[2] + x;
            auto s3 = src_rows[3] + x;
            auto s4 = src_rows[4] + x;

            f32 gx0 = 0.0f;
            f32 gx1 = 0.0f;
            f32 gx2 = 0.0f;
            f32 gx3 = 0.0f;
            f32 gx4 = 0.0f;

            f32 gy0 = 0.0f;
            f32 gy1 = 0.0f;
            f32 gy2 = 0.0f;
            f32 gy3 = 0.0f;
            f32 gy4 = 0.0f;

            for (u32 i = 0; i < 5; i++)
            {
                gx0 += s0[i] * kx0[i];
                gy0 += s0[i] * ky0[i];

                gx1 += s1[i] * kx1[i];
                gy1 += s1[i] * ky1[i];

                gx2 += s2[i] * kx2[i];
                gy2 += s2[i] * ky2[i];

                gx3 += s3[i] * kx3[i];
                gy3 += s3[i] * ky3[i];

                gx4 += s4[i] * kx4[i];
                gy4 += s4[i] * ky4[i];
            }

            auto gx = gx0 + gx1 + gx2 + gx3 + gx4;
            auto gy = gy0 + gy1 + gy2 + gy3 + gy4;

            return (u8)num::q_hypot(gx, gy);
        };

        auto w = dst.width;
        auto h = dst.height;

        for (u32 y = 0; y < h; y++)
        {
            set_src_rows(y);
            auto d = row_begin(dst, y);
            for (u32 x = 0; x < w; x++)
            {
                d[x] = grad(x);
            }
        }
    }
}


/* gradients */

namespace image
{
    void gradients(GrayView const& src, GrayView const& dst)
    {
        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(src.width == dst.width);
        assert(src.height == dst.height);

        // for 5x5 kernel = 5 / 2;
        constexpr u32 kd = 5 / 2;

        Rect2Du32 r{};
        r.x_begin = kd;
        r.x_end = src.width - kd;
        r.y_begin = kd;
        r.y_end = src.height - kd;

        auto sub_src = sub_view(src, r);
        auto sub_dst = sub_view(dst, r);

        gradients_5x5(sub_src, sub_dst);

        /*auto top = make_rect(0, 0, dst.width, kd);
        auto bottom = make_rect(0, r.y_end, dst.width, kd);
        auto left = make_rect(0, kd, kd, h);
        auto right = make_rect(r.x_end, kd, kd, h);

        fill(sub_view(dst, top), 0);
        fill(sub_view(dst, bottom), 0);
        fill(sub_view(dst, left), 0);
        fill(sub_view(dst, right), 0);*/        
    }
    
}


/* centroid */

namespace image
{
    template <class VIEW>
    Point2Du32 centroid_gray(VIEW const& src, Point2Du32 default_pt, f32 sensitivity)
	{	
		f64 total = 0.0;
		f64 x_total = 0.0;
		f64 y_total = 0.0;

        auto w = src.width;
        auto h = src.height;

        auto s = num::clamp(sensitivity, 0.0f, 1.0f);
        
        auto total_min = 1 + (1.0f - s) * (w * h - 1);

		for (u32 y = 0; y < h; ++y)
		{
			auto s = row_begin(src, y);
			for (u32 x = 0; x < w; ++x)
			{
				u64 val = s[x] ? 1 : 0;
				total += val;
				x_total += x * val;
				y_total += y * val;
			}
		}

		if (total < total_min)
		{			
            return default_pt;
		}
		else
		{
            return {
                (u32)(x_total / total),
			    (u32)(y_total / total)
            };
            
		}
	}


    Point2Du32 centroid(GrayView const& src, Point2Du32 default_pt, f32 sensitivity)
    {
        return centroid_gray(src, default_pt, sensitivity);
    }


    Point2Du32 centroid(GrayView const& src, f32 sensitivity)
	{	
		Point2Du32 default_pt = { src.width / 2, src.height / 2 };

        return centroid_gray(src, default_pt, sensitivity);
	}


    Point2Du32 centroid(GraySubView const& src, Point2Du32 default_pt, f32 sensitivity)
    {
        return centroid_gray(src, default_pt, sensitivity);
    }
}