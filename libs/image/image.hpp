#pragma once

#include "../span/span.hpp"

namespace mb = memory_buffer;

/*  image basic */

namespace image
{
    class RGBAu8
    {
    public:
        u8 red;
        u8 green;
        u8 blue;
        u8 alpha;
    };

    using Pixel = RGBAu8;
    using Image = Matrix2D<Pixel>;
    using ImageView = MatrixView2D<Pixel>;
    using ImageGray = Matrix2D<u8>;
    using GrayView = MatrixView2D<u8>;


    bool create_image(Image& image, u32 width, u32 height, cstr tag);

    void destroy_image(Image& image);


    inline u32 as_u32(Pixel p)
    {
        return  *((u32*)(&p));
    }


    inline Image as_image(ImageView const& view)
    {
        Image image;
        image.width = view.width;
        image.height = view.height;
        image.data_ = view.matrix_data_;

        return image;
    }
}


namespace image
{
    template <typename T>
    class MatrixSubView2D
    {
    public:
        T*  matrix_data_;
        u32 matrix_width;

        u32 x_begin;
        u32 y_begin;

        u32 width;
        u32 height;
    };


    using SubView = MatrixSubView2D<Pixel>;    
    using GraySubView = MatrixSubView2D<u8>;
}


namespace image
{
    using Buffer8 = MemoryBuffer<u8>;
    using Buffer32 = MemoryBuffer<Pixel>;


    constexpr inline Pixel to_pixel(u8 red, u8 green, u8 blue, u8 alpha)
    {
        Pixel p{};
        p.red = red;
        p.green = green;
        p.blue = blue;
        p.alpha = alpha;

        return p;
    }


    constexpr inline Pixel to_pixel(u8 red, u8 green, u8 blue)
    {
        return to_pixel(red, green, blue, 255);
    }


    constexpr inline Pixel to_pixel(u8 gray)
    {
        return to_pixel(gray, gray, gray);
    } 


    /*inline Buffer8 create_buffer8(u32 n_pixels)
	{
		Buffer8 buffer;
		mb::create_buffer(buffer, n_pixels);
		return buffer;
	}


    inline Buffer32 create_buffer32(u32 n_pixels)
	{
		Buffer32 buffer;
		mb::create_buffer(buffer, n_pixels);
		return buffer;
	}*/


    inline Buffer8 create_buffer8(u32 n_pixels, cstr tag)
	{
		Buffer8 buffer;
		mb::create_buffer(buffer, n_pixels, tag);
		return buffer;
	}


    inline Buffer32 create_buffer32(u32 n_pixels, cstr tag)
	{
		Buffer32 buffer;
		mb::create_buffer(buffer, n_pixels, tag);
		return buffer;
	}


    inline Rect2Du32 make_rect(u32 width, u32 height)
    {
        Rect2Du32 range{};
        range.x_begin = 0;
        range.x_end = width;
        range.y_begin = 0;
        range.y_end = height;

        return range;
    }


    inline Rect2Du32 make_rect(u32 x_begin, u32 y_begin, u32 width, u32 height)
    {
        Rect2Du32 range{};
        range.x_begin = x_begin;
        range.x_end = x_begin + width;
        range.y_begin = y_begin;
        range.y_end = y_begin + height;

        return range;
    }
}


/* row_begin */

namespace image
{
    template <typename T>
    static inline T* row_begin(MatrixView2D<T> const& view, u32 y)
    {
        return view.matrix_data_ + (u64)y * view.width;
    }


    template <typename T>
    static inline T* row_begin(MatrixSubView2D<T> const& view, u32 y)
    {
        return view.matrix_data_ + (u64)(view.y_begin + y) * view.matrix_width + view.x_begin;
    }


    template <typename T>
    static inline T* row_begin(MatrixSubView2D<T> const& view, i32 y)
    {
        return view.matrix_data_ + (i64)((i32)view.y_begin + y) * view.matrix_width + view.x_begin;
    }
}


/* xy_at */

namespace image
{
    template <typename T>
    static inline T* xy_at(MatrixView2D<T> const& view, u32 x, u32 y)
    {
        return row_begin(view, y) + x;
    }


    template <typename T>
    static inline T* xy_at(MatrixSubView2D<T> const& view, u32 x, u32 y)
    {
        return row_begin(view, y) + x;
    }
}


/* row_span */

namespace image
{
    template <typename T>
	static inline SpanView<T> row_span(MatrixView2D<T> const& view, u32 y)
	{
        SpanView<T> span{};

        span.data = view.matrix_data_ + (u64)y * view.width;
        span.length = view.width;

        return span;
	}


    template <typename T>
    static inline SpanView<T> row_span(MatrixSubView2D<T> const& view, u32 y)
    {
        SpanView<T> span{};

        span.data = view.matrix_data_ + (u64)(view.y_begin + y) * view.matrix_width + view.x_begin;
        span.length = view.width;

        return span;
    }


    template <typename T>
    static inline SpanView<T> to_span(MatrixView2D<T> const& view)
    {
        SpanView<T> span{};

        span.data = view.matrix_data_;
        span.length = view.width * view.height;

        return span;
    }


    template <typename T>
    static inline SpanView<T> sub_span(MatrixView2D<T> const& view, u32 y, u32 x_begin, u32 x_end)
    {
        SpanView<T> span{};

        span.data = view.matrix_data_ + (u64)(y * view.width) + x_begin;
        span.length = x_end - x_begin;

        return span;
    }


    template <typename T>
    static inline SpanView<T> sub_span(MatrixSubView2D<T> const& view, u32 y, u32 x_begin, u32 x_end)
    {
        SpanView<T> span{};

        span.data = view.matrix_data_ + (u64)((view.y_begin + y) * view.matrix_width + view.x_begin) + x_begin;
        span.length = x_end - x_begin;

        return span;
    }
}


/* make_view */

namespace image
{
    ImageView make_view(Image const& image);

    ImageView make_view(u32 width, u32 height, Buffer32& buffer);

    GrayView make_view(u32 width, u32 height, Buffer8& buffer);
}


/* sub_view */

namespace image
{
    template <typename T>
    inline MatrixSubView2D<T> sub_view(MatrixView2D<T> const& view, Rect2Du32 const& range)
    {
        MatrixSubView2D<T> sub_view{};

        sub_view.matrix_data_ = view.matrix_data_;
        sub_view.matrix_width = view.width;
        sub_view.x_begin = range.x_begin;
        sub_view.y_begin = range.y_begin;
        sub_view.width = range.x_end - range.x_begin;
        sub_view.height = range.y_end - range.y_begin;

        return sub_view;
    }


    template <typename T>
    inline MatrixSubView2D<T> sub_view(MatrixSubView2D<T> const& view, Rect2Du32 const& range)
    {
        MatrixSubView2D<T> sub_view{};

        sub_view.matrix_data_ = view.matrix_data_;
        sub_view.matrix_width = view.matrix_width;

        sub_view.x_begin = range.x_begin + view.x_begin;
		sub_view.y_begin = range.y_begin + view.y_begin;

		sub_view.width = range.x_end - range.x_begin;
		sub_view.height = range.y_end - range.y_begin;

        return sub_view;
    }


    template <typename T>
    inline MatrixSubView2D<T> sub_view(MatrixView2D<T> const& view)
    {
        auto range = make_range(view.width, view.height);
        return sub_view(view, range);
    }
}


/* fill */

namespace image
{
    void fill(ImageView const& view, Pixel color);

    void fill(SubView const& view, Pixel color);

    void fill(GrayView const& view, u8 value);

    void fill(GraySubView const& view, u8 value);
}


/* copy */

namespace image
{
    void copy(ImageView const& src, ImageView const& dst);

    void copy(ImageView const& src, SubView const& dst);

    void copy(SubView const& src, ImageView const& dst);

    void copy(SubView const& src, SubView const& dst);
}


/* transform */

namespace image
{
    void transform(ImageView const& src, ImageView const& dst, fn<Pixel(Pixel)> const& func);

    void transform_scale_up(GrayView const& src, ImageView const& dst, fn<Pixel(u8)> const& func);

    void transform_scale_up(GrayView const& src1, GrayView const& src2, ImageView const& dst, fn<Pixel(u8, u8)> const& func);
}


/* resize */

namespace image
{
    void scale_down(ImageView const& src, ImageView const& dst);

    void scale_down(GrayView const& src, GrayView const& dst);
    

    void scale_up(ImageView const& src, ImageView const& dst);

    void scale_up(GrayView const& src, GrayView const& dst);
    

    void resize(ImageView const& src, ImageView const& dst);

    void resize(ImageView const& src, SubView const& dst);

    void resize(GrayView const& src, GrayView const& dst);
}


/* map */

namespace image
{
    void map(GrayView const& src, ImageView const& dst);

    void map_scale_down(GrayView const& src, ImageView const& dst);

    void map_scale_up(GrayView const& src, ImageView const& dst);
}


/* draw */

namespace image
{
    void draw_rect(ImageView const& view, Rect2Du32 const& rect, Pixel color, u32 thick);
}


/* gradients */

namespace image
{
    void gradients(GrayView const& src, GrayView const& dst);
}


/* centroid */

namespace image
{
    Point2Du32 centroid(GrayView const& src, Point2Du32 default_pt, f32 sensitivity);

    Point2Du32 centroid(GraySubView const& src, Point2Du32 default_pt, f32 sensitivity);
}