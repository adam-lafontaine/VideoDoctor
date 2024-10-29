#include "video.hpp"
#include "../alloc_type/alloc_type.hpp"

// sudo apt-get install ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <cassert>

#include <functional>


namespace video
{
    class VideoContext
    {
    public:
        AVFormatContext* format_ctx;
        AVCodecContext* codec_ctx;
        AVStream* stream;

        AVFrame* frame_av;
        AVPacket* packet;

        FrameRGBA frame_rgba;        
    };


    class VideoGenContext
    {
    public:
        AVFormatContext* format_ctx;
        AVCodecContext* codec_ctx;
        AVStream* stream;

        AVFrame* frame_av;
        AVPacket* packet;

        FrameRGBA frame_rgba;

        int frame_pts = -1;

        i64 packet_duration = -1;
    };


    static VideoContext& get_context(Video video)
    {
        return *(VideoContext*)(video.video_handle);
    }


    static VideoGenContext& get_context(VideoGen video)
    {
        return *(VideoGenContext*)(video.video_handle);
    }


    static inline AVFrame* av_frame(FrameRGBA const& frame_rgba)
    {
        return (AVFrame*)frame_rgba.frame_handle;
    }



}


namespace video
{
    static SwsContext* create_sws(AVFrame* src, AVFrame* dst)
    {
        return sws_getContext(
            src->width, src->height, 
            (AVPixelFormat)src->format,

            dst->width, dst->height, 
            (AVPixelFormat)dst->format,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }


    static void convert_frame(AVFrame* src, AVFrame* dst)
    {
        auto sws_ctx = create_sws(src, dst);
        
        sws_scale(
            sws_ctx,
            src->data, src->linesize, 0, src->height,
            dst->data, dst->linesize);

        sws_freeContext(sws_ctx);
    }


    static AVFrame* create_avframe(u32 width, u32 height, AVPixelFormat fmt)
    {
        auto w = (int)width;
        auto h = (int)height;
        int align = 32;

        AVFrame* av_frame = av_frame_alloc();
        av_frame->format = (int)fmt;
        av_frame->width = w;
        av_frame->height = h;

        if (av_image_alloc(av_frame->data, av_frame->linesize, w, h, fmt, align) < 0)
        {
            av_frame_free(&av_frame);
            return 0;
        }

        return av_frame;
    }


    static bool read_next_frame(VideoContext const& ctx)
    {
        for (;;)
        {
            if (av_read_frame(ctx.format_ctx, ctx.packet) < 0)
            {
                //assert("*** av_read_frame ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (ctx.packet->stream_index != ctx.stream->index)
            {
                //assert("*** ctx.packet->stream_index != ctx.video_stream_index ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            if(avcodec_send_packet(ctx.codec_ctx, ctx.packet) < 0)
            {
                assert("*** avcodec_send_packet ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (avcodec_receive_frame(ctx.codec_ctx, ctx.frame_av) < 0)
            {
                //assert("*** avcodec_receive_frame ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            break;
        }

        return true;
    }
    

    static void for_each_frame(VideoContext const& ctx, std::function<void(VideoContext const&)> const& func)
    {
        while (av_read_frame(ctx.format_ctx, ctx.packet) >= 0) 
        {
            if (ctx.packet->stream_index == ctx.stream->index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(ctx.codec_ctx, ctx.packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(ctx.codec_ctx, ctx.frame_av) == 0) 
                    {
                        func(ctx);
                    }
                }
            }
            av_packet_unref(ctx.packet);
        }
    }


    static void encode_frame(VideoGenContext const& ctx, i64 pts)
    {
        auto encoder = ctx.codec_ctx;
        auto frame = ctx.frame_av;
        auto duration = ctx.packet_duration;

        // Set PTS (Presentation Time Stamp)
        frame->pts = pts;

        if (av_frame_make_writable(frame) < 0)
        {
            assert("*** av_frame_make_writable ***" && false);
        }

        // Send frame to encoder
        if (avcodec_send_frame(encoder, frame) >= 0) 
        {
            // Receive packet from encoder and write it
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = nullptr;
            packet.size = 0;

            while (avcodec_receive_packet(encoder, &packet) == 0) 
            {
                packet.stream_index = ctx.stream->index;
                packet.duration = duration;
                av_interleaved_write_frame(ctx.format_ctx, &packet);
                av_packet_unref(&packet);
            }
        }
       
    }


    static void for_each_frame(VideoContext const& src_ctx, VideoGenContext const& dst_ctx, std::function<void(VideoContext const&, VideoGenContext const&)> const& func)
    {
        auto packet = src_ctx.packet;
        auto decoder = src_ctx.codec_ctx;
        auto frame = src_ctx.frame_av;

        while (av_read_frame(src_ctx.format_ctx, packet) >= 0) 
        {
            if (packet->stream_index == src_ctx.stream->index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(decoder, packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(decoder, frame) == 0) 
                    {
                        func(src_ctx, dst_ctx);                        
                    }
                }
            }
            av_packet_unref(packet);
        }
    }


    static void copy_frame(VideoContext const& src_ctx, VideoGenContext const& dst_ctx)
    {
        convert_frame(src_ctx.frame_av, dst_ctx.frame_av);
        convert_frame(dst_ctx.frame_av, av_frame(dst_ctx.frame_rgba));

        encode_frame(dst_ctx, src_ctx.frame_av->pts);        
    }


    static void crop_frame(VideoContext const& src_ctx, VideoGenContext const& dst_ctx)
    {
        auto encoder = dst_ctx.codec_ctx;
        auto frame = dst_ctx.frame_av;
        auto tb = dst_ctx.stream->time_base.den / dst_ctx.stream->time_base.num;
        auto fr = src_ctx.stream->avg_frame_rate.den / src_ctx.stream->avg_frame_rate.num;


        // Set the source crop slice
        u8* src_data[AV_NUM_DATA_POINTERS] = {nullptr};
        int src_linesize[AV_NUM_DATA_POINTERS] = {0};

        auto w = src_ctx.codec_ctx->width;
        auto h = src_ctx.codec_ctx->height;
        auto fmt = src_ctx.codec_ctx->pix_fmt;
        auto crop_w = dst_ctx.codec_ctx->width;
        auto crop_h = dst_ctx.codec_ctx->height;

        // TODO: detect crop position
        auto crop_x = w / 4;
        auto crop_y = h / 4;

        av_image_fill_arrays(src_data, src_linesize, src_ctx.frame_av->data[0], fmt, w, h, 1);

        // Adjust src_data pointers for cropping offsets
        for (int i = 0; i < AV_NUM_DATA_POINTERS && src_ctx.frame_av->data[i]; i++) 
        {
            src_data[i] = src_ctx.frame_av->data[i] + crop_y * src_linesize[i] + crop_x * 4;
        }

        // Initialize cropping context
        auto sws = sws_getContext(crop_w, crop_h, fmt,  // Cropped input dimensions
                                crop_w, crop_h, fmt,  // Target output dimensions
                                SWS_BILINEAR, nullptr, nullptr, nullptr);

        // Crop using sws_scale by selecting a subsection of the frame data
        sws_scale(sws, src_data, src_linesize, 0, crop_h, frame->data, frame->linesize);
        
        

        convert_frame(frame, av_frame(dst_ctx.frame_rgba));

        /*convert_frame(src_ctx.frame_av, av_frame(src_ctx.frame_rgba));
        auto& src_rgba = src_ctx.frame_rgba.view;
        auto& dst_rgba = dst_ctx.frame_rgba.view;

        

        auto r = img::make_rect(crop_x, crop_y, dst_rgba.width, dst_rgba.height);
        auto sub = img::sub_view(src_rgba, r);
        img::copy(sub, dst_rgba);
        auto crop_rgba = av_frame(dst_ctx.frame_rgba);
        convert_frame(crop_rgba, dst_ctx.frame_av);*/

        
        

        if (av_frame_make_writable(frame) < 0)
        {
            assert("*** av_frame_make_writable ***" && false);
        }
        

        // Set PTS (Presentation Time Stamp)
        frame->pts = src_ctx.frame_av->pts;
        //dst_ctx.frame_av->pts

        

        // Send frame to encoder
        if (avcodec_send_frame(encoder, frame) >= 0) 
        {
            // Receive packet from encoder and write it
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = nullptr;
            packet.size = 0;

            while (avcodec_receive_packet(encoder, &packet) == 0) 
            {
                packet.stream_index = dst_ctx.stream->index;
                packet.duration = tb * fr;
                av_interleaved_write_frame(dst_ctx.format_ctx, &packet);
                av_packet_unref(&packet);
            }
        }

        av_frame_unref(frame);

        sws_freeContext(sws); // TODO: VideoGenContext
    }


    


    static bool write_frame(VideoGenContext& ctx)
    {
        ++ctx.frame_pts;
        ctx.frame_av->pts = ctx.frame_pts;

        auto ret = avcodec_send_frame(ctx.codec_ctx, ctx.frame_av);
        if (ret < 0)
        {
            assert("*** avcodec_send_frame ***" && false);
            return false;
        }

        while (ret >= 0) 
        {
            ret = avcodec_receive_packet(ctx.codec_ctx, ctx.packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
            {
                av_packet_unref(ctx.packet);
                break; // No more packets available right now
            } 
            else if (ret < 0) 
            {
                assert("*** avcodec_receive_packet ***" && false);
                return false;
            }

            ctx.packet->stream_index = ctx.stream->index;

            // Write the encoded packet to file
            //av_interleaved_write_frame(ctx.format_ctx, ctx.packet);
            av_write_frame(ctx.format_ctx, ctx.packet);
            av_packet_unref(ctx.packet);
        }

        return true;
    }


    static void flush_encoder(VideoGenContext& ctx)
    {
        // Flush the encoder
        avcodec_send_frame(ctx.codec_ctx, nullptr);
        while (avcodec_receive_packet(ctx.codec_ctx, ctx.packet) == 0) 
        {
            ctx.packet->stream_index = ctx.stream->index;
            //av_interleaved_write_frame(ctx.format_ctx, ctx.packet);
            av_write_frame(ctx.format_ctx, ctx.packet);
            av_packet_unref(ctx.packet);
        }
    }
}


/* api */

namespace video
{
    bool create_frame(FrameRGBA& frame, u32 width, u32 height)
    {
        auto w = (int)width;
        auto h = (int)height;
        auto fmt = AV_PIX_FMT_RGBA;
        int align = 32;

        auto av_frame = create_avframe(w, h, fmt);
        if (!av_frame)
        {
            return false;
        }

        frame.frame_handle = (u64)av_frame;

        frame.view.width = width;
        frame.view.height = height;
        frame.view.matrix_data_ = (img::Pixel*)av_frame->data[0];        

        return true;
    }


    void destroy_frame(FrameRGBA& frame)
    {
        if (!frame.frame_handle)
        {
            return;
        }

        auto av_frame = (AVFrame*)frame.frame_handle;
        av_frame_free(&av_frame);

        frame.frame_handle = 0;
        frame.view.matrix_data_ = 0;
    }


    bool open_video(Video& video, cstr filepath)
    {
        auto data = mem::malloc<VideoContext>("video context");
        if (!data)
        {
            return false;
        }

        video.video_handle = (u64)data;

        auto& ctx = get_context(video);

        ctx.format_ctx = avformat_alloc_context();

        if (avformat_open_input(&ctx.format_ctx, filepath, nullptr, nullptr) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        if (avformat_find_stream_info(ctx.format_ctx, nullptr) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        int video_stream_index = -1;
        for (u32 i = 0; i < ctx.format_ctx->nb_streams; ++i) 
        {
            if (ctx.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
            {
                video_stream_index = i;
                break;
            }        
        }

        if (video_stream_index < 0)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        ctx.stream = ctx.format_ctx->streams[video_stream_index];

        AVCodecParameters* cp = ctx.stream->codecpar;
        AVCodec* codec = avcodec_find_decoder(cp->codec_id);
        if (!codec)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        ctx.codec_ctx = avcodec_alloc_context3(codec);
        if (!ctx.codec_ctx)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        if (avcodec_parameters_to_context(ctx.codec_ctx, cp) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        if (avcodec_open2(ctx.codec_ctx, codec, nullptr) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        ctx.frame_av = av_frame_alloc();
        if (!ctx.frame_av)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        ctx.packet = av_packet_alloc();
        if (!ctx.packet)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame_av);
            return false;
        }

        video.frame_width = (u32)cp->width;
        video.frame_height = (u32)cp->height;

        auto stream = ctx.format_ctx->streams[video_stream_index];
        video.fps = av_q2d(stream->avg_frame_rate);

        if (!create_frame(ctx.frame_rgba, video.frame_width, video.frame_height))
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame_av);
            av_packet_free(&ctx.packet);
            return false;
        }

        return true;
    }


    void close_video(Video& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        av_frame_free(&ctx.frame_av);
        av_packet_free(&ctx.packet);
        avcodec_close(ctx.codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }


    bool next_frame(Video const& video, FrameRGBA const& frame_out)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }

        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));
        convert_frame(ctx.frame_av, av_frame(frame_out));

        av_packet_unref(ctx.packet);

        return true;
    }


    bool next_frame(Video const& video, FrameList const& frames_out)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }

        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));

        for (auto& frame : frames_out)
        {
            convert_frame(ctx.frame_av, av_frame(frame));
        }

        av_packet_unref(ctx.packet);

        return true;
    }


    void resize_frame(FrameRGBA const& src, FrameRGBA const& dst)
    {
        auto av_src = (AVFrame*)src.frame_handle;
        auto av_dst = (AVFrame*)dst.frame_handle;

        convert_frame(av_src, av_dst);
    }
}


/* crop video */

namespace video
{
namespace crop
{
    bool create_video(Video const& src, VideoGen& dst, cstr dst_path, u32 width, u32 height)
    {
        auto& src_ctx = get_context(src);
        auto src_stream = src_ctx.stream;
        
        auto src_codec = avcodec_find_decoder(src_stream->codecpar->codec_id);
        if (!src_codec)
        {
            assert("*** avcodec_find_decoder ***" && false);
            return false;
        }

        auto data = mem::malloc<VideoGenContext>("video gen context");
        if (!data)
        {
            return false;
        }

        dst.video_handle = (u64)data;

        auto& ctx = get_context(dst);

        int w = (int)width;
        int h = (int)height;
        auto fmt = (int)src_ctx.codec_ctx->pix_fmt;

        ctx.frame_av = av_frame_alloc();
        if (!ctx.frame_av)
        {
            assert("*** av_frame_alloc ***" && false);
            return false;
        }

        ctx.frame_av->format = fmt;
        ctx.frame_av->width = w;
        ctx.frame_av->height = h;

        if (av_frame_get_buffer(ctx.frame_av, 32) < 0)
        {
            assert("*** av_frame_get_buffer ***" && false);
            return false;
        }
        
        if (avformat_alloc_output_context2(&ctx.format_ctx, nullptr, nullptr, dst_path) < 0)
        {
            assert("*** avformat_alloc_output_context2 ***" && false);
            return false;
        }

        auto stream = avformat_new_stream(ctx.format_ctx, nullptr);
        if (!stream)
        {
            assert("*** avformat_alloc_output_context2 ***" && false);
            return false;
        }

        ctx.stream = stream;
        stream->time_base = src_stream->time_base;

        // Set up the output codec context
        auto dst_codec = avcodec_find_encoder(src_ctx.codec_ctx->codec_id);
        if (!dst_codec) 
        {
            assert("*** avcodec_find_encoder ***" && false);
            return false;
        }

        ctx.codec_ctx = avcodec_alloc_context3(dst_codec);
        if (!ctx.codec_ctx)
        {
            assert("*** avcodec_alloc_context3 ***" && false);
            avformat_free_context(ctx.format_ctx);
            return false;
        }
        
        ctx.codec_ctx->codec_id = src_codec->id;
        ctx.codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO; // ?
        ctx.codec_ctx->pix_fmt = src_ctx.codec_ctx->pix_fmt;
        ctx.codec_ctx->width = (int)width;
        ctx.codec_ctx->height = (int)height;
        ctx.codec_ctx->time_base = src_stream->time_base;
        ctx.codec_ctx->framerate = src_stream->avg_frame_rate;

        if (avcodec_open2(ctx.codec_ctx, dst_codec, nullptr) != 0)
        {
            assert("*** avcodec_open2 ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        if (avcodec_parameters_from_context(stream->codecpar, ctx.codec_ctx) < 0)
        {
            assert("*** avcodec_parameters_from_context ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        if (!(ctx.format_ctx->oformat->flags & AVFMT_NOFILE) && avio_open(&ctx.format_ctx->pb, dst_path, AVIO_FLAG_WRITE) < 0)
        {
            assert("*** avio_open ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        if (avformat_write_header(ctx.format_ctx, nullptr) < 0)
        {
            assert("*** avformat_write_header ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        dst.frame_width = width;
        dst.frame_height = height;

        if (!create_frame(ctx.frame_rgba, width, height))
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame_av);
            return false;
        }

        ctx.packet = av_packet_alloc(); // TODO: delete?
        if (!ctx.packet)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame_av);
            return false;
        }

        auto tb = ctx.stream->time_base.den / ctx.stream->time_base.num;
        auto fr = src_ctx.stream->avg_frame_rate.den / src_ctx.stream->avg_frame_rate.num;

        ctx.packet_duration = tb * fr;

        return true;
    }


    void crop_video(Video const& src, VideoGen& dst, FrameList const& src_out, FrameList const& dst_out)
    {
        auto const crop = [&](auto const& src_ctx, auto const& dst_ctx)
        {            
            //crop_frame(src_ctx, dst_ctx);
            copy_frame(src_ctx, dst_ctx);

            for (auto& out : src_out)
            {
                convert_frame(src_ctx.frame_av, av_frame(out));
            }
            
            auto crop_rgba = av_frame(dst_ctx.frame_rgba);
            for (auto& out : dst_out)
            {
                convert_frame(crop_rgba, av_frame(out));
            }
        };

        for_each_frame(get_context(src), get_context(dst), crop);
    }


    /*bool next_frame(Video const& src, VideoGen& dst, Point2Du32 crop_xy, FrameList const& src_out, FrameList const& dst_out)
    {
        auto& src_ctx = get_context(src);
        auto& dst_ctx = get_context(dst);

        if (!read_next_frame(src_ctx))
        {
            // eof
            return false;
        }
        
        convert_frame(src_ctx.frame_av, av_frame(src_ctx.frame_rgba));
        auto& src_rgba = src_ctx.frame_rgba.view;
        auto& dst_rgba = dst_ctx.frame_rgba.view;
        auto r = img::make_rect(crop_xy.x, crop_xy.y, dst_rgba.width, dst_rgba.height);
        auto sub = img::sub_view(src_rgba, r);
        img::copy(sub, dst_rgba);
        auto crop_rgba = av_frame(dst_ctx.frame_rgba);
        convert_frame(crop_rgba, dst_ctx.frame_av);

        for (auto& out : src_out)
        {
            convert_frame(src_ctx.frame_av, av_frame(out));
        }
        
        for (auto& out : dst_out)
        {
            convert_frame(crop_rgba, av_frame(out));
        }

        if (!write_frame(dst_ctx))
        {
            assert("*** write_frame ***" && false);
            return false;
        }

        for (auto& out : src_out)
        {
            convert_frame(src_ctx.frame_av, av_frame(out));
        }

        for (auto& out : dst_out)
        {
            convert_frame(dst_ctx.frame_av, av_frame(out));
        }

        av_packet_unref(src_ctx.packet);

        return true;
    }*/


    void close_video(VideoGen& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        avio_closep(&ctx.format_ctx->pb);
        
        av_frame_free(&ctx.frame_av);
        av_packet_free(&ctx.packet);
        avcodec_free_context(&ctx.codec_ctx);
        avformat_free_context(ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }


    void save_and_close_video(VideoGen& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        flush_encoder(ctx);       

        av_write_trailer(ctx.format_ctx); 
        close_video(video);
    }


    
    
}
}