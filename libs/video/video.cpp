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

        return true;
    }


    static void crop_frame(AVFrame* src, AVFrame* dst, int dst_width, int dst_height, int crop_x, int crop_y) 
    {
        auto sws_ctx = create_sws(src, dst);

        uint8_t* srcSlice[AV_NUM_DATA_POINTERS];
        int srcStride[AV_NUM_DATA_POINTERS];
        
        for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) 
        {
            srcSlice[i] = src->data[i] + crop_y * src->linesize[i] + crop_x * (i == 0 ? 1 : 0);
            srcStride[i] = src->linesize[i];
        }
        
        sws_scale(
            sws_ctx,
            srcSlice, srcStride, 0, dst_height,
            dst->data, dst->linesize);

        sws_freeContext(sws_ctx);
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

        if (ctx.packet)
        {
            av_packet_free(&ctx.packet);
        }
        
        avcodec_close(ctx.codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }


    bool next_frame(Video const& video, FrameRGBA const& frame)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }

        auto av_frame = (AVFrame*)frame.frame_handle;

        convert_frame(ctx.frame, av_frame);
        av_packet_unref(ctx.packet);

        return true;
    }


    bool next_frame(Video const& video, FrameList const& frames)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
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


/* crop video */

namespace video
{
namespace crop
{
    bool create_video(Video const& src, Video& dst, cstr dst_path, u32 width, u32 height)
    {
        auto data = mem::malloc<VideoContext>("video context");
        if (!data)
        {
            return false;
        }

        dst.video_handle = (u64)data;

        auto& ctx = get_context(dst);

        auto& src_ctx = get_context(src);
        auto src_stream = src_ctx.format_ctx->streams[src_ctx.video_stream_index];
        auto src_cp = src_stream->codecpar;
        auto codec = avcodec_find_decoder(src_cp->codec_id);
        if (!codec)
        {
            assert("*** avcodec_find_decoder ***" && false);
            return false;
        }
        
        if (avformat_alloc_output_context2(&ctx.format_ctx, nullptr, nullptr, dst_path) < 0)
        {
            assert("***  ***" && false);
            return false;
        }

        auto stream = avformat_new_stream(ctx.format_ctx, nullptr);
        if (!stream)
        {
            assert("*** avformat_alloc_output_context2 ***" && false);
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        ctx.codec_ctx = avcodec_alloc_context3(codec);
        if (!ctx.codec_ctx)
        {
            assert("*** avcodec_alloc_context3 ***" && false);
            avformat_free_context(ctx.format_ctx);
            return false;
        }
        
        ctx.codec_ctx->codec_id = codec->id;
        ctx.codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        ctx.codec_ctx->pix_fmt = src_ctx.codec_ctx->pix_fmt;
        ctx.codec_ctx->width = (int)width;
        ctx.codec_ctx->height = (int)height;
        ctx.codec_ctx->time_base = src_stream->time_base;

        if (avcodec_open2(ctx.codec_ctx, codec, nullptr) != 0)
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

        if (avio_open(&ctx.format_ctx->pb, dst_path, AVIO_FLAG_WRITE) < 0)
        {
            assert("*** avio_open ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        if (avformat_write_header(ctx.format_ctx, nullptr) < 0)
        {
            assert("***  ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        ctx.frame = av_frame_alloc();
        if (!ctx.frame)
        {
            assert("*** avformat_write_header ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        int w = (int)width;
        int h = (int)height;
        auto fmt = (AVPixelFormat)src_ctx.frame->format;
        int align = 32;

        ctx.frame->format = fmt;
        ctx.frame->width = w;
        ctx.frame->height = h;
        if (av_image_alloc(ctx.frame->data, ctx.frame->linesize, w, h, fmt, align) < 0)
        {
            assert("*** av_image_alloc ***" && false);
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            return false;
        }

        ctx.packet = 0;

        /*ctx.packet = av_packet_alloc();
        if (!ctx.packet)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame);
            return false;
        }*/

        dst.frame_width = width;
        dst.frame_height = height;

        return true;
    }


    bool next_frame(Video const& src, Video& dst, Point2Du32 crop_xy)
    {
        auto& src_ctx = get_context(src);        

        if (!read_next_frame(src_ctx))
        {
            // eof
            
            return false;
        }

        auto& dst_ctx = get_context(dst);

        auto w = (int)dst.frame_width;
        auto h = (int)dst.frame_height;
        auto x = (int)crop_xy.x;
        auto y = (int)crop_xy.y;
        crop_frame(src_ctx.frame, dst_ctx.frame, w, h, x, y);
        
        AVPacket out_packet;
        av_init_packet(&out_packet);
        out_packet.data = nullptr;
        out_packet.size = 0;

        avcodec_send_frame(dst_ctx.codec_ctx, dst_ctx.frame);
        avcodec_receive_packet(dst_ctx.codec_ctx, &out_packet);
        av_write_frame(dst_ctx.format_ctx, &out_packet);

        av_packet_unref(&out_packet);
        av_packet_unref(src_ctx.packet);

        return true;
    }


    void save_and_close_video(Video& video)
    {
        auto& ctx = get_context(video);

        av_write_trailer(ctx.format_ctx);
        avio_close(ctx.format_ctx->pb);
        
        close_video(video);
    }
}
}