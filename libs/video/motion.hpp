#pragma once

#include "../image/image.hpp"

namespace motion
{
    namespace img = image;

    using Matrix32 = MatrixView2D<f32>;


    class GrayMotion
    {
    public:

        constexpr static u32 count = 0b1000; // 30fps

        f32 motion_sensitivity = 0.9f;
        f32 locate_sensitivity = 0.98f;

        u32 index = 0;

        Matrix32 list[count] = { 0 };
        Matrix32 totals;

        img::GrayView values;
        img::GrayView out;

        Point2Du32 location;

        img::Buffer32 buffer32;
        img::Buffer8 buffer8;
    };


    bool create(GrayMotion& mot, u32 width, u32 height);

    void destroy(GrayMotion& mot);

    void update(GrayMotion& mot, img::GrayView const& src);

    void update(GrayMotion& mot, img::GrayView const& src, Rect2Du32 scan_rect);

    void update(GrayMotion& mot, img::GrayView const& src, img::GrayView const& dst);

    void update(GrayMotion& mot, img::GrayView const& src, Rect2Du32 src_scan_rect, img::GrayView const& dst);

}


namespace motion
{
    class GradientMotion
    {
    public:
        img::GrayView proc_gray_view;
        img::GrayView proc_edges_view;
        img::GrayView proc_motion_view;
        
        Point2Du32 src_location;
        
        GrayMotion edge_motion;

        img::Buffer8 buffer8;
    };


    inline void destroy(GradientMotion& gm)
    {
        destroy(gm.edge_motion);
        mb::destroy_buffer(gm.buffer8);
    }


    bool create(GradientMotion& gm, u32 width, u32 height);

    void update(GradientMotion& gm, img::GrayView const& src_gray, Rect2Du32 src_scan_rect);
}