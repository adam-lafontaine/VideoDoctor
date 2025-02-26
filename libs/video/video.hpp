#pragma once

#include "../image/image.hpp"

#include <initializer_list>
#include <functional>


namespace video
{
    namespace img = image;


    class VideoFrame
    {
    public:
        img::ImageView rgba;
        img::GrayView gray;
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

        bool write_audio = true;
    };
    
    
    template <class T>
    using fn = std::function<T>;

    using fn_frame_to_rgba = fn<void(VideoFrame, img::ImageView const&)>;

    using fn_frame = fn<void(VideoFrame)>;
    using fn_bool = fn<bool()>;


    bool open_video(VideoReader& video, cstr filepath);

    void close_video(VideoReader& video);

    void process_video(VideoReader const& src, fn_frame const& cb);

    bool process_video(VideoReader const& src, fn_frame const& cb, fn_bool const& proc_cond);
    
    
    bool create_video(VideoReader const& src, VideoWriter& dst, cstr dst_path, u32 dst_width, u32 dst_height);

    void close_video(VideoWriter& video);
    
    void save_and_close_video(VideoWriter& video);
    
    void process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb);

    bool process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb, fn_bool const& proc_cond);


    VideoFrame current_frame(VideoReader const& video);
    
}


/* Deprecated */

namespace video
{
    class FrameRGBA
    {
    public:
        u64 frame_handle = 0;

        img::ImageView view;
    };


    // Deprecated???
    bool create_frame(FrameRGBA& frame, u32 width, u32 height);

    // Deprecated???
    void destroy_frame(FrameRGBA& frame);



    using FrameList = std::initializer_list<FrameRGBA>;

    // Deprecated???
    bool next_frame(VideoReader const& video, FrameRGBA const& frame_out);  
}