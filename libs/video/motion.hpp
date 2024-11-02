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

    void update(GrayMotion& mot, img::GrayView const& src, Rect2Du32 scan_rect, img::GrayView const& dst);

    Point2Du32 scale_location(GrayMotion& mot, u32 scale);

}