#pragma once

#include "../image/image.hpp"

#include <initializer_list>


namespace video
{
    namespace img = image;

    class FrameRGBA
    {
    public:
        u64 frame_handle = 0;

        img::ImageView view;
    };


    class VideoReader
    {
    public:

        u64 video_handle = 0;

        u32 frame_width = 0;
        u32 frame_height = 0;

        f64 fps = 0.0;
    };


    class VideoWriter
    {
    public:

        u64 video_handle = 0;

        u32 frame_width = 0;
        u32 frame_height = 0;
    };


    using FrameList = std::initializer_list<FrameRGBA>;


    bool create_frame(FrameRGBA& frame, u32 width, u32 height);

    void destroy_frame(FrameRGBA& frame);

    bool open_video(VideoReader& video, cstr filepath);

    void close_video(VideoReader& video);

    bool next_frame(VideoReader const& video, FrameRGBA const& frame_out);

    bool next_frame(VideoReader const& video, FrameList const& frames_out);

    void resize_frame(FrameRGBA const& src, FrameRGBA const& dst);


namespace crop
{
    bool create_video(VideoReader const& src, VideoWriter& dst, cstr dst_path, u32 width, u32 height);

    void crop_video(VideoReader const& src, VideoWriter& dst, FrameList const& src_out, FrameList const& dst_out);

    void close_video(VideoWriter& video);

    void save_and_close_video(VideoWriter& video);
}
}