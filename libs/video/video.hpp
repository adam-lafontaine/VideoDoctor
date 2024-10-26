#pragma once

#include "../image/image.hpp"


namespace video
{
    namespace img = image;


    class FrameRGBA
    {
    public:
        u64 frame_handle = 0;

        img::ImageView view;

    };


    class Video
    {
    public:

        u64 video_handle = 0;

        u32 frame_width = 0;
        u32 frame_height = 0;

        f64 fps = 0.0;
    };


    bool create_frame(FrameRGBA& frame, u32 width, u32 height);

    void destroy_frame(FrameRGBA& frame);

    bool open_video(Video& video, cstr filepath);

    void close_video(Video& video);

    bool next_frame(Video const& video, FrameRGBA const& frame);
}