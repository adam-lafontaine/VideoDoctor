#include "motion.hpp"
#include "../util/numeric.hpp"


namespace motion
{
    namespace num = numeric;


    static Matrix32 make_matrix(u32 w, u32 h, img::Buffer32& buffer32)
    {
        Matrix32 mat{};
        mat.width = w;
        mat.height = h;
        mat.matrix_data_ = (f32*)mb::push_elements(buffer32, w * h);

        return mat;
    }


    static void next(GrayMotion& mot)
    { 
        static_assert(GrayMotion::count % 2 == 0);

        constexpr auto mask = GrayMotion::count - 1;

        mot.index = (mot.index + 1) & mask;
    }


    static Matrix32 front(GrayMotion const& mot) 
    { 
        return mot.list[mot.index]; 
    }


    static f32 val_to_f32(u8 v) { return (f32)v; }


    f32 map_f(f32 x)
    {
        f32 m = 1.0f;
        f32 b = 0.0f;

        auto ux = num::round_to_unsigned<u8>(num::floor(x * 10));

        switch (ux)
        {
        case 0:
        case 1:
            m = 4.5f;
            b = 0.0f;

        default:
            m = 0.125f;
            b = 0.875f;
        break;
        }

        return m * x + b;
    }


    static Rect2Du32 rect_scale_down(Rect2Du32 rect, u32 scale)
    {
        rect.x_begin /= scale;
        rect.x_end /= scale;
        rect.y_begin /= scale;
        rect.y_end /= scale;

        return rect;
    }


    Point2Du32 scale_point_up(Point2Du32 pt, u32 scale)
    {
        return {
            pt.x * scale,
            pt.y * scale
        };
    }
}


namespace motion
{
    bool create(GrayMotion& mot, u32 width, u32 height)
    {
        auto n32 = width * height * (mot.count + 1);
        auto n8 = width * height * 2;

        auto& buffer32 = mot.buffer32;
        auto& buffer8 = mot.buffer8;

        buffer32 = img::create_buffer32(n32, "Motion 32");
        if (!buffer32.ok)
        {
            return false;
        }

        buffer8 = img::create_buffer8(n8, "Motion 8");
        if (!buffer8.ok)
        {
            return false;
        }

        mb::zero_buffer(buffer32);
        mb::zero_buffer(buffer8);

        for (u32 i = 0; i < mot.count; i++)
        {
            mot.list[i] = make_matrix(width, height, buffer32);
        }

        mot.totals = make_matrix(width, height, buffer32);

        mot.values = img::make_view(width, height, buffer8);
        mot.out = img::make_view(width, height, buffer8);

        return true;
    }


    void destroy(GrayMotion& mot)
    {
        mb::destroy_buffer(mot.buffer32);
        mb::destroy_buffer(mot.buffer8);
    }


    void update(GrayMotion& mot, img::GrayView const& src)
    {
        constexpr auto i_count = 1.0f / GrayMotion::count;

        auto thresh = (1.0f - map_f(mot.motion_sensitivity)) * 255;

        auto loc_base = 0.5f;

        auto loc_s = mot.locate_sensitivity;

        auto const abs_avg_delta = [&](u8 v, f32 t)
        {
            return num::abs(t * i_count - v) >= thresh ? 255 : 0; 
        };

        auto t = img::to_span(mot.totals);
        auto f = img::to_span(front(mot));
        auto v = img::to_span(mot.values);
        auto o = img::to_span(mot.out);

        //img::scale_down(src, mot.values);
        img::resize(src, mot.values);
        span::transform(v, t, o, abs_avg_delta);

        mot.location = img::centroid(mot.out, mot.location, loc_s);

        span::sub(t, f, t);
        span::transform(v, f, val_to_f32);
        span::add(t, f, t);
        next(mot);
    }    


    void update(GrayMotion& mot, img::GrayView const& src, Rect2Du32 scan_rect)
    {
        constexpr auto i_count = 1.0f / GrayMotion::count;

        auto thresh = (1.0f - map_f(mot.motion_sensitivity)) * 255;

        auto loc_base = 0.5f;

        auto loc_s = mot.locate_sensitivity;

        auto const abs_avg_delta = [&](u8 v, f32 t)
        {
            return num::abs(t * i_count - v) >= thresh ? 255 : 0; 
        };

        auto t = img::to_span(mot.totals);
        auto f = img::to_span(front(mot));
        auto v = img::to_span(mot.values);
        auto o = img::to_span(mot.out);

        //img::scale_down(src, mot.values);
        img::resize(src, mot.values);
        span::transform(v, t, o, abs_avg_delta);

        auto scale = src.width / mot.values.width;
        auto rect = rect_scale_down(scan_rect, scale);

        Point2Du32 pt = {
            mot.location.x - rect.x_begin,
            mot.location.y - rect.y_begin
        };

        pt = img::centroid(img::sub_view(mot.out, rect), pt, loc_s);

        mot.location.x = pt.x + rect.x_begin;
        mot.location.y = pt.y + rect.y_begin;

        span::sub(t, f, t);
        span::transform(v, f, val_to_f32);
        span::add(t, f, t);
        next(mot);
    }


    void update(GrayMotion& mot, img::GrayView const& src, img::GrayView const& dst)
    {
        update(mot, src);
        //img::scale_up(mot.out, dst);
        img::resize(mot.out, dst);
    }


    void update(GrayMotion& mot, img::GrayView const& src, Rect2Du32 src_scan_rect, img::GrayView const& dst)
    {
        update(mot, src, src_scan_rect);
        //img::scale_up(mot.out, dst);
        img::resize(mot.out, dst);
    }
}


namespace motion
{
    bool create(GradientMotion& gm, u32 width, u32 height)
    {
        auto process_w = width;
        auto process_h = height;
        auto motion_w = process_w / 2;
        auto motion_h = process_h / 2;

        auto n_pixels8 = process_w * process_h * 3;

        gm.buffer8 = img::create_buffer8(n_pixels8, "buffer8");
        if (!gm.buffer8.ok)
        {
            return false;
        }

        mb::zero_buffer(gm.buffer8);

        gm.proc_gray_view = img::make_view(process_w, process_h, gm.buffer8);
        gm.proc_edges_view = img::make_view(process_w, process_h, gm.buffer8);
        gm.proc_motion_view = img::make_view(process_w, process_h, gm.buffer8);

        if (!motion::create(gm.edge_motion, motion_w, motion_h))
        {
            return false;
        }

        return true;
    }


    void update(GradientMotion& gm, img::GrayView const& src_gray, Rect2Du32 src_scan_rect)
    {
        auto& gray = gm.proc_gray_view;
        auto& edges = gm.proc_edges_view;
        auto& motion = gm.proc_motion_view;

        auto proc_scale = src_gray.width / gray.width;
        auto motion_scale = src_gray.width / gm.edge_motion.out.width;

        auto proc_scan_rect = rect_scale_down(src_scan_rect, proc_scale);

        //img::scale_down(src_gray, gray);
        img::resize(src_gray, gray);
        img::gradients(gray, edges);
        update(gm.edge_motion, edges, proc_scan_rect, motion);

        gm.src_location = scale_point_up(gm.edge_motion.location, motion_scale);
    }
}