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


namespace video
{

    static void convert_frame(AVFrame* src, AVFrame* dst)
    {
        auto sws_ctx = sws_getContext(
            src->width, src->height, 
            (AVPixelFormat)src->format,

            dst->width, dst->height, 
            (AVPixelFormat)dst->format,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        
        sws_scale(
            sws_ctx,
            src->data, src->linesize, 0, src->height,
            dst->data, dst->linesize);

        sws_freeContext(sws_ctx);
    }
}


namespace video
{
    class VideoContext
    {
    public:
        AVFormatContext* format_ctx;
        AVCodecContext* codec_ctx;

        AVFrame* frame;
        AVPacket* packet;

        int video_stream_index = -1;

    };


    static VideoContext& get_context(Video video)
    {
        return *(VideoContext*)(video.video_handle);
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

        AVFrame* av_frame = av_frame_alloc();
        av_frame->format = fmt;
        av_frame->width = w;
        av_frame->height = h;

        if (av_image_alloc(av_frame->data, av_frame->linesize, w, h, fmt, align) < 0)
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

        ctx.video_stream_index = video_stream_index;

        AVCodecParameters* cp = ctx.format_ctx->streams[video_stream_index]->codecpar;
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

        ctx.frame = av_frame_alloc();
        if (!ctx.frame)
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
            av_frame_free(&ctx.frame);
            return false;
        }

        video.frame_width = (u32)cp->width;
        video.frame_height = (u32)cp->height;

        auto stream = ctx.format_ctx->streams[video_stream_index];
        video.fps = av_q2d(stream->avg_frame_rate);

        return true;
    }


    void close_video(Video& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        av_frame_free(&ctx.frame);
        av_packet_free(&ctx.packet);
        avcodec_close(ctx.codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }


    bool next_frame(Video const& video, FrameRGBA const& frame)
    {
        auto& ctx = get_context(video);        

        for (;;)
        {
            if (av_read_frame(ctx.format_ctx, ctx.packet) < 0)
            {
                //assert("*** av_read_frame ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (ctx.packet->stream_index != ctx.video_stream_index)
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

            if (avcodec_receive_frame(ctx.codec_ctx, ctx.frame) < 0)
            {
                //assert("*** avcodec_receive_frame ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            break;
        }

        auto av_frame = (AVFrame*)frame.frame_handle;

        convert_frame(ctx.frame, av_frame);
        av_packet_unref(ctx.packet);

        return true;
    }


    bool next_frame(Video const& video, FrameList const& frames)
    {
        auto& ctx = get_context(video);        

        for (;;)
        {
            if (av_read_frame(ctx.format_ctx, ctx.packet) < 0)
            {
                //assert("*** av_read_frame ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (ctx.packet->stream_index != ctx.video_stream_index)
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

            if (avcodec_receive_frame(ctx.codec_ctx, ctx.frame) < 0)
            {
                //assert("*** avcodec_receive_frame ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            break;
        }

        for (auto& frame : frames)
        {
            auto av_frame = (AVFrame*)frame.frame_handle;
            convert_frame(ctx.frame, av_frame);
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