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
        AVStream* stream;

        AVFrame* frame;
        AVPacket* packet;

        FrameRGBA frame_rgba;        
    };


    class VideoGenContext
    {
    public:
        AVFormatContext* format_ctx;
        AVCodecContext* codec_ctx;
        AVStream* stream;

        AVFrame* frame;

        FrameRGBA frame_rgba;
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


    static int get_bytes_per_pixel(AVPixelFormat pix_fmt) 
    {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt);
        if (!desc) 
        {
            return -1;
        }

        if (desc->flags & AV_PIX_FMT_FLAG_PLANAR) 
        {
            // Planar formats, where we deal with each plane separately
            return 1; // Usually, each plane has 1 byte per pixel component in planar formats
        } 
        else 
        {
            // Packed formats
            int bits_per_pixel = 0;
            for (int i = 0; i < desc->nb_components; i++) 
            {
                bits_per_pixel += desc->comp[i].depth;
            }

            return (bits_per_pixel + 7) / 8;  // Round up to nearest byte
        }
    }


    static bool write_frame(VideoGenContext& dst_ctx)
    {
        // Encode cropped frame and write it to the output file
        AVPacket out_packet;
        av_init_packet(&out_packet);
        out_packet.data = nullptr;
        out_packet.size = 0;

        if (avcodec_send_frame(dst_ctx.codec_ctx, dst_ctx.frame) < 0)
        {
            assert("*** avcodec_send_frame ***" && false);
            return false;
        }

        if (avcodec_receive_packet(dst_ctx.codec_ctx, &out_packet) < 0)
        {
            assert("*** avcodec_send_frame ***" && false);
            return false;
        }        
        
        if (!av_write_frame(dst_ctx.format_ctx, &out_packet))
        {
            assert("*** avcodec_send_frame ***" && false);
            return false;
        }

        av_packet_unref(&out_packet);

        return true;
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

        if (!create_frame(ctx.frame_rgba, video.frame_width, video.frame_height))
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.codec_ctx);
            av_frame_free(&ctx.frame);
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

        av_frame_free(&ctx.frame);
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

        convert_frame(ctx.frame, av_frame(ctx.frame_rgba));
        convert_frame(ctx.frame, av_frame(frame_out));

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

        convert_frame(ctx.frame, av_frame(ctx.frame_rgba));

        for (auto& frame : frames_out)
        {
            convert_frame(ctx.frame, av_frame(frame));
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
        auto fmt = src_ctx.codec_ctx->pix_fmt;

        ctx.frame = create_avframe(w, h, fmt);
        if (!ctx.frame)
        {
            assert("*** create_avframe ***" && false);
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
            av_frame_free(&ctx.frame);
            return false;
        }

        return true;
    }


    bool next_frame(Video const& src, VideoGen& dst, Point2Du32 crop_xy, FrameList const& src_out, FrameList const& dst_out)
    {
        auto& src_ctx = get_context(src);
        auto& dst_ctx = get_context(dst);

        if (!read_next_frame(src_ctx))
        {
            // eof
            return false;
        }
        
        convert_frame(src_ctx.frame, av_frame(src_ctx.frame_rgba));
        for (auto& out : src_out)
        {
            convert_frame(src_ctx.frame, av_frame(out));
        }

        auto& src_rgba = src_ctx.frame_rgba.view;
        auto& dst_rgba = dst_ctx.frame_rgba.view;

        auto r = img::make_rect(crop_xy.x, crop_xy.y, dst_rgba.width, dst_rgba.height);
        auto sub = img::sub_view(src_rgba, r);
        img::copy(sub, dst_rgba);

        auto crop_rgba = av_frame(dst_ctx.frame_rgba);
        convert_frame(crop_rgba, dst_ctx.frame);
        for (auto& out : dst_out)
        {
            convert_frame(crop_rgba, av_frame(out));
        }

        /*if (!write_frame(dst_ctx))
        {
            assert("*** write_frame ***" && false);
            return false;
        }

        for (auto& frame : src_out)
        {
            auto av_frame = (AVFrame*)frame.frame_handle;
            convert_frame(src_ctx.frame, av_frame);
        }

        for (auto& frame : dst_out)
        {
            auto av_frame = (AVFrame*)frame.frame_handle;
            convert_frame(dst_ctx.frame, av_frame);
        }*/

        av_packet_unref(src_ctx.packet);

        return true;
    }


    void close_video(VideoGen& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        avio_close(ctx.format_ctx->pb);
        
        av_frame_free(&ctx.frame);
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

        av_write_trailer(ctx.format_ctx); 
        close_video(video);
    }
}
}