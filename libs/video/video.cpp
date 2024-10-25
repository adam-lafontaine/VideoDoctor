#include "video.hpp"
#include "../alloc_type/alloc_type.hpp"

// sudo apt-get install ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}


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

    };


    static VideoContext& get_context(Video video)
    {
        return *(VideoContext*)(video.handel);
    }
}


/* api */

namespace video
{
    bool init()
    {
        auto ctx = mem::malloc<VideoContext>("video context");
        if (!ctx)
        {
            return false;
        }

        return true;
    }


    bool create_frame(FrameRGBA& frame, u32 width, u32 height)
    {
        auto w = (int)width;
        auto h = (int)height;
        auto fmt = AV_PIX_FMT_RGBA;

        AVFrame* av_frame = av_frame_alloc();
        av_frame->format = fmt;
        av_frame->width = w;
        av_frame->height = h;

        if (av_image_alloc(av_frame->data, av_frame->linesize, w, h, fmt, 32) < 0)
        {
            return false;
        }

        frame.handel = (u64)av_frame;

        frame.view.width = width;
        frame.view.height = height;
        frame.view.matrix_data_ = (img::Pixel*)av_frame->data[0];        

        return true;
    }


    void destroy_frame(FrameRGBA& frame)
    {
        auto av_frame = (AVFrame*)frame.handel;
        av_frame_free(&av_frame);

        frame.handel = 0;
        frame.view.matrix_data_ = 0;
    }


    bool open_video(Video& video, cstr filepath)
    {
        auto data = mem::malloc<VideoContext>("video context");
        if (!data)
        {
            return false;
        }

        video.handel = (u64)data;

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

        video.frame_width = (u32)ctx.frame->width;
        video.frame_height = (u32)ctx.frame->height;

        return true;
    }


    void close_video(Video& video)
    {
        if (!video.handel)
        {
            return;
        }

        auto& ctx = get_context(video);

        av_frame_free(&ctx.frame);
        av_packet_free(&ctx.packet);
        avcodec_close(ctx.codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mem::free(&ctx);

        video.handel = 0;
    }
}