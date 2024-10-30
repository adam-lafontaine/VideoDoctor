#pragma once

#include "../image/image.hpp"

#include <initializer_list>
#include <functional>


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
    
    template <class T>
    using fn = std::function<T>;

    using gray_to_rgba = fn<void(img::GrayView const&, img::ImageView const&)>;


    bool create_frame(FrameRGBA& frame, u32 width, u32 height);

    void resize_frame(FrameRGBA const& src, FrameRGBA const& dst);

    void destroy_frame(FrameRGBA& frame);


    bool open_video(VideoReader& video, cstr filepath);

    void close_video(VideoReader& video);

    img::ImageView frame_view(VideoReader const& video);

    img::GrayView frame_gray_view(VideoReader const& video);

    void play_video(VideoReader const& video, FrameList const& frames_out);

    bool next_frame(VideoReader const& video, FrameRGBA const& frame_out);

    bool next_frame(VideoReader const& video, FrameList const& frames_out);
    
    bool create_video(VideoReader const& src, VideoWriter& dst, cstr dst_path, u32 width, u32 height);void close_video(VideoWriter& video);

    void close_video(VideoWriter& video);
    
    void save_and_close_video(VideoWriter& video);

    img::ImageView frame_view(VideoReader const& video);

    img::GrayView frame_gray_view(VideoReader const& video);

    void crop_video(VideoReader const& src, VideoWriter& dst, FrameList const& src_out, FrameList const& dst_out);

    void process_video(VideoReader const& src, VideoWriter& dst, gray_to_rgba const& cb, FrameList const& src_out, FrameList const& dst_out);

    
}