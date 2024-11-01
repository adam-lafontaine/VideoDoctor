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


    void fill(GrayView const& view, u8 value)
    {
        assert(view.matrix_data_);

        span::fill_8(to_span(view), value);
    }


    void fill(GraySubView const& view, u8 value)
    {
        assert(view.matrix_data_);

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


namespace image
{
    void scale_down(ImageView const& src, ImageView const& dst, u32 scale)
    {
        scale = scale ? scale : src.width / dst.width;

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

                rd[xd] = to_pixel((u8)red, (u8)green, (u8)blue, (u8)alpha);
            }
        }
    }


    void scale_down(GrayView const& src, GrayView const& dst, u32 scale)
    {
        scale = scale ? scale : src.width / dst.width;

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


    void scale_up(ImageView const& src, ImageView const& dst, u32 scale)
    {
        scale = scale ? scale : dst.width / src.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

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


    void scale_up(GrayView const& src, GrayView const& dst, u32 scale)
    {
        scale = scale ? scale : dst.width / src.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

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


    void map_scale_up(GrayView const& src, ImageView const& dst, u32 scale)
    {
        scale = scale ? scale : dst.width / src.width;

        assert(src.matrix_data_);
        assert(dst.matrix_data_);
        assert(dst.width == src.width * scale);
        assert(dst.height == src.height * scale);
        assert(scale > 1);

        for (u32 ys = 0; ys < src.height; ys++)
        {
            auto yd = scale * ys;
            auto rs = row_begin(src, ys);

            for (u32 xs = 0; xs < src.width; xs++)
            {
                auto xd = scale * xs;
                
                auto p = to_pixel(rs[xs]);

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
}


/* convolve */

namespace image
{
    static constexpr f32 GRAD_X_5X5[25] = 
    {
          0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        -0.08f, -0.12f, 0.0f, 0.12f, 0.08f
		-0.24f, -0.36f, 0.0f, 0.36f, 0.24f
		-0.08f, -0.12f, 0.0f, 0.12f, 0.08f,
          0.0f,  0.0f,  0.0f,  0.0f,  0.0f,
    };


    static constexpr f32 GRAD_Y_5X5[25] = 
    {
        0.0f, -0.08f, -0.24f, -0.08f, 0.0f,
		0.0f, -0.12f, -0.36f, -0.12f, 0.0f,
		0.0f,   0.0f,   0.0f,   0.0f, 0.0f,
		0.0f,  0.12f,  0.36f,  0.12f, 0.0f,
		0.0f,  0.08f,  0.24f,  0.08f, 0.0f,
    };
    

	static inline f32 gradient_at_xy_f32(GraySubView const& view, u32 x, u32 y)
    {
		constexpr i32 k_width = 5;
		constexpr i32 k_height = 5;

        auto kernel_x = GRAD_X_5X5;
        auto kernel_y = GRAD_Y_5X5;

        f32 grad_x = 0.0f;
        f32 grad_y = 0.0f;
        u32 w = 0;

        i32 rx = (i32)x - (k_width / 2);
        i32 ry = (i32)y - (k_height / 2);

        for (i32 v = 0; v < k_height; ++v)
        {
            auto s = row_begin(view, ry + v);
            for (i32 u = 0; u < k_width; ++u)
            {
                auto p = s[rx + u];

                grad_x += p * kernel_x[w];
                grad_y += p * kernel_y[w];
                ++w;
            }
        }

        return num::q_hypot(grad_x, grad_y);
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

        auto w = sub_dst.width;
        auto h = sub_dst.height;

        for (u32 y = 0; y < h; y++)
        {
            auto d = row_begin(sub_dst, y);
            for (u32 x = 0; x < w; x++)
            {
                d[x] = (u8)gradient_at_xy_f32(sub_src, x, y);
            }
        }

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
    Point2Du32 centroid(GrayView const& src, Point2Du32 default_pt, f32 sensitivity)
	{	
		f64 total = 0.0;
		f64 x_total = 0.0;
		f64 y_total = 0.0;

        auto w = src.width;
        auto h = src.height;

        auto s = num::clamp(sensitivity, 0.0f, 1.0f);
        
        auto total_min = (1.0f - s) * w * h;

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

		if (total <= total_min)
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


    Point2Du32 centroid(GrayView const& src, f32 sensitivity)
	{	
		Point2Du32 default_pt = { src.width / 2, src.height / 2 };

        return centroid(src, default_pt, sensitivity);
	}
}