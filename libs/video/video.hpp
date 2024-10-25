#pragma once

#include "../image/image.hpp"


namespace video
{
    namespace img = image;


    class FrameRGBA
    {
    public:
        u64 handel = 0;

        img::ImageView view;

    };


    bool create_frame(FrameRGBA& frame, u32 width, u32 height);

    void destroy_frame(FrameRGBA& frame);
}