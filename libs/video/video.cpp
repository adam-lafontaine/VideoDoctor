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
    class VideoReaderContext
    {
    public:
        AVFormatContext* format_ctx;

        AVCodecContext* video_codec_ctx;
        AVStream* video_stream;

        AVCodecContext* audio_codec_ctx;
        AVStream* audio_stream;
        
        AVPacket* packet;
        AVFrame* av_frame;
        
        AVFrame* av_rgba;

        VideoFrame display_frames[2];
        b8 display_frame_id = 0;

        img::Buffer32 buffer32;
        img::Buffer8 buffer8;

        VideoFrame display_frame_read() { return display_frames[display_frame_id]; }
        VideoFrame display_frame_write() { return display_frames[!display_frame_id]; }

    };


    class VideoWriterContext
    {
    public:
        AVFormatContext* format_ctx;

        AVCodecContext* video_codec_ctx;
        AVStream* video_stream;

        AVCodecContext* audio_codec_ctx;
        AVStream* audio_stream;

        AVFrame* av_frame;
        
        AVFrame* av_rgba;

        i64 packet_duration = -1;
    };


    static inline VideoReaderContext& get_context(VideoReader video)
    {
        return *(VideoReaderContext*)(video.video_handle);
    }


    static inline VideoWriterContext& get_context(VideoWriter video)
    {
        return *(VideoWriterContext*)(video.video_handle);
    }

    
    template <class CTX>
    static inline img::ImageView get_frame_rgba(CTX const& ctx) // TODO: replace
    {
        auto w = ctx.av_frame->width;
        auto h = ctx.av_frame->height;

        img::ImageView view{};

        view.width = w;
        view.height = h;
        view.matrix_data_ = (img::Pixel*)ctx.av_rgba->data[0];

        return view;
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

    // delete?
    static SwsContext* create_sws(AVFrame* src, int src_w, int src_h, AVFrame* dst, int dst_w, int dst_h)
    {
        return sws_getContext(
            src_w, src_h, 
            (AVPixelFormat)src->format,

            dst_w, dst_h, 
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
    

    static void convert_frame(AVFrame* src, AVFrame* dst, SwsContext* sws)
    {        
        sws_scale(
            sws,
            src->data, src->linesize, 0, src->height,
            dst->data, dst->linesize);
    }


    static void capture_frame(VideoReaderContext& ctx, SwsContext* sws)
    {
        convert_frame(ctx.av_frame, ctx.av_rgba, sws);

        ctx.display_frame_id = !ctx.display_frame_id;
        auto write_frame = ctx.display_frame_write();

        u32 w = write_frame.rgba.width;
        u32 h = write_frame.rgba.height;

        auto src_rgba = span::to_span((img::Pixel*)ctx.av_rgba->data[0], w * h);
        auto dst_rgba = img::to_span(write_frame.rgba);
        span::copy(src_rgba, dst_rgba);

        auto src_gray = span::to_span(ctx.av_frame->data[0], w * h);
        auto dst_gray = img::to_span(write_frame.gray);
        span::copy(src_gray, dst_gray);
    }
    

    static void encode_video_frame(VideoWriterContext const& ctx, i64 pts)
    {
        auto encoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        auto duration = ctx.packet_duration;
        auto stream = ctx.video_stream;

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
                packet.stream_index = stream->index;
                packet.duration = duration;
                // Rescale packet timestamps to output stream's time base
                av_packet_rescale_ts(&packet, encoder->time_base, stream->time_base);

                // Set DTS if needed (FFmpeg does this automatically in most cases)
                if (packet.dts == AV_NOPTS_VALUE) 
                {
                    packet.dts = packet.pts;  // Simple assignment if no B-frames
                }
                av_interleaved_write_frame(ctx.format_ctx, &packet);
                av_packet_unref(&packet);
            }
        }       
    }


    static void copy_audio(VideoReaderContext const& src_ctx, VideoWriterContext const& dst_ctx)
    {
        auto packet = src_ctx.packet;
        auto in_stream = src_ctx.audio_stream;
        auto out_stream = dst_ctx.audio_stream;

        packet->pts = av_rescale_q_rnd(packet->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX)); 
        packet->dts = av_rescale_q_rnd(packet->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX)); 
        packet->duration = av_rescale_q(packet->duration, in_stream->time_base, out_stream->time_base);
        packet->pos = -1; 
        packet->stream_index = out_stream->index;

        av_interleaved_write_frame(dst_ctx.format_ctx, packet);
    }
    
    
    static void flush_encoder(VideoWriterContext& ctx)
    {
        auto encoder = ctx.video_codec_ctx;
        auto duration = ctx.packet_duration;
        auto stream = ctx.video_stream;

        AVPacket packet;
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;

        // Flush the encoder
        avcodec_send_frame(ctx.video_codec_ctx, nullptr);
        while (avcodec_receive_packet(encoder, &packet) == 0) 
        {
            packet.stream_index = stream->index;
            // Rescale packet timestamps to output stream's time base
            av_packet_rescale_ts(&packet, encoder->time_base, stream->time_base);

            // Set DTS if needed (FFmpeg does this automatically in most cases)
            if (packet.dts == AV_NOPTS_VALUE) 
            {
                packet.dts = packet.pts;  // Simple assignment if no B-frames
            }
            av_interleaved_write_frame(ctx.format_ctx, &packet);
            av_packet_unref(&packet);
        }
    }

}


/* for_each_frame */

namespace video
{
    template <class FN> // std::function<void()>
    static void for_each_video_frame(VideoReader const& src, FN const& on_read_video)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        int video_stream_index = ctx.video_stream->index;

        SwsContext* sws = 0;

        while (av_read_frame(ctx.format_ctx, packet) >= 0) 
        {
            if (packet->stream_index == video_stream_index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(decoder, packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(decoder, frame) == 0) 
                    {     
                        if (!sws)
                        {
                            sws = create_sws(ctx.av_frame, ctx.av_rgba);
                        }
                        
                        capture_frame(ctx, sws);
                        on_read_video();
                    }
                }
            }
            av_packet_unref(packet);
        }

        sws_freeContext(sws);
    }


    template <class FN1, class FN2> // std::function<void()>
    static void for_each_audio_video_frame(VideoReader const& src, FN1 const& on_read_video, FN2 const& on_read_audio)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        int video_stream_index = ctx.video_stream->index;
        int audio_stream_index = -1;
        if (ctx.audio_stream)
        {
            audio_stream_index = ctx.audio_stream->index;
        }

        SwsContext* sws = 0;

        while (av_read_frame(ctx.format_ctx, packet) >= 0) 
        {
            if (packet->stream_index == video_stream_index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(decoder, packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(decoder, frame) == 0) 
                    {      
                        if (!sws)
                        {
                            sws = create_sws(ctx.av_frame, ctx.av_rgba);
                        }
                        
                        capture_frame(ctx, sws);
                        on_read_video();
                    }
                }
            }
            else if (packet->stream_index == audio_stream_index)
            {
                on_read_audio();
            }
            av_packet_unref(packet);
        }

        sws_freeContext(sws);
    }


    template <class FN> // std::function<void()>, std::function<bool()>
    static bool for_each_video_frame(VideoReader const& src, FN const& on_read_video, fn_bool const& cond)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        int video_stream_index = ctx.video_stream->index;

        bool done = false;
        auto const read = [&]()
        { 
            done = av_read_frame(ctx.format_ctx, packet) < 0;
            return !done;
        };

        SwsContext* sws = 0;

        while (cond() && read()) 
        {
            if (packet->stream_index == video_stream_index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(decoder, packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(decoder, frame) == 0) 
                    {
                        if (!sws)
                        {
                            sws = create_sws(ctx.av_frame, ctx.av_rgba);
                        }
                        
                        capture_frame(ctx, sws);
                        on_read_video();
                    }
                }
            }            
            av_packet_unref(packet);
        }

        sws_freeContext(sws);

        return done;
    }


    template <class FN1, class FN2> // std::function<void()>, std::function<bool()>
    static bool for_each_audio_video_frame(VideoReader const& src, FN1 const& on_read_video, FN2 const& on_read_audio, fn_bool const& cond)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        int video_stream_index = ctx.video_stream->index;
        int audio_stream_index = -1;
        if (ctx.audio_stream)
        {
            audio_stream_index = ctx.audio_stream->index;
        }

        bool done = false;
        auto const read = [&]()
        { 
            done = av_read_frame(ctx.format_ctx, packet) < 0;
            return !done;
        };

        SwsContext* sws = 0;

        while (cond() && read()) 
        {
            if (packet->stream_index == video_stream_index) 
            {
                // Send packet to decoder
                if (avcodec_send_packet(decoder, packet) == 0) 
                {
                    // Receive frame from decoder
                    while (avcodec_receive_frame(decoder, frame) == 0) 
                    {
                        if (!sws)
                        {
                            sws = create_sws(ctx.av_frame, ctx.av_rgba);
                        }
                        
                        capture_frame(ctx, sws);
                        on_read_video();
                    }
                }
            }
            else if (packet->stream_index == audio_stream_index)
            {
                on_read_audio();
            }
            av_packet_unref(packet);
        }

        sws_freeContext(sws);

        return done;
    }

}


/* create streams */

namespace video
{
    static bool create_video_stream(VideoReaderContext& src_ctx, VideoWriterContext& ctx, u32 width, u32 height)
    {
        auto src_stream = src_ctx.video_stream;
        
        auto src_codec = avcodec_find_decoder(src_stream->codecpar->codec_id);
        if (!src_codec)
        {
            assert("*** avcodec_find_decoder - video ***" && false);
            return false;
        }

        auto video_stream = avformat_new_stream(ctx.format_ctx, nullptr);
        if (!video_stream)
        {
            assert("*** avformat_alloc_output_context2 - video ***" && false);
            return false;
        }

        video_stream->time_base = src_stream->time_base;

        // Set up the output codec context
        auto dst_video_codec = avcodec_find_encoder(src_ctx.video_codec_ctx->codec_id);
        if (!dst_video_codec) 
        {
            assert("*** avcodec_find_encoder - video ***" && false);
            return false;
        }

        ctx.video_codec_ctx = avcodec_alloc_context3(dst_video_codec);
        if (!ctx.video_codec_ctx)
        {
            assert("*** avcodec_alloc_context3 - video ***" && false);
            return false;
        }
        
        ctx.video_codec_ctx->codec_id = src_codec->id;
        ctx.video_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        ctx.video_codec_ctx->pix_fmt = src_ctx.video_codec_ctx->pix_fmt;
        ctx.video_codec_ctx->width = (int)width;
        ctx.video_codec_ctx->height = (int)height;
        ctx.video_codec_ctx->time_base = src_stream->time_base;
        ctx.video_codec_ctx->framerate = src_stream->avg_frame_rate;

        if (avcodec_open2(ctx.video_codec_ctx, dst_video_codec, nullptr) != 0)
        {
            assert("*** avcodec_open2 - video ***" && false);
            return false;
        }

        if (avcodec_parameters_from_context(video_stream->codecpar, ctx.video_codec_ctx) < 0)
        {
            assert("*** avcodec_parameters_from_context - video ***" && false);
            return false;
        }

        ctx.video_stream = video_stream;

        return true;
    }


    static bool create_audio_stream(VideoReaderContext& src_ctx, VideoWriterContext& ctx)
    {
        auto src_stream = src_ctx.audio_stream;
        if (!src_stream)
        {
            return false;
        }

        auto src_codec = avcodec_find_decoder(src_stream->codecpar->codec_id);
        if (!src_codec)
        {
            assert("*** avcodec_find_decoder - audio ***" && false);
            return false;
        }

        auto audio_stream = avformat_new_stream(ctx.format_ctx, nullptr);
        if (!audio_stream)
        {
            assert("*** avformat_alloc_output_context2 - audio ***" && false);
            return false;
        }

        auto dst_audio_codec = avcodec_find_encoder(src_ctx.audio_codec_ctx->codec_id);
        if (!dst_audio_codec) 
        {
            assert("*** avcodec_find_encoder - audio ***" && false);
            return false;
        }

        ctx.audio_codec_ctx = avcodec_alloc_context3(dst_audio_codec);
        if (!ctx.audio_codec_ctx)
        {
            assert("*** avcodec_alloc_context3 - audio ***" && false);
            return false;
        }

        ctx.audio_codec_ctx->codec_id = src_codec->id;
        ctx.audio_codec_ctx->sample_rate = src_ctx.audio_codec_ctx->sample_rate;
        ctx.audio_codec_ctx->channel_layout = src_ctx.audio_codec_ctx->channel_layout;
        ctx.audio_codec_ctx->channels = av_get_channel_layout_nb_channels(ctx.audio_codec_ctx->channel_layout);
        ctx.audio_codec_ctx->sample_fmt = src_ctx.audio_codec_ctx->sample_fmt;
        ctx.audio_codec_ctx->time_base = src_stream->time_base;

        if (ctx.format_ctx->oformat->flags & AVFMT_GLOBALHEADER) 
        { 
            ctx.audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; 
        } 
        
        if (avcodec_open2(ctx.audio_codec_ctx, dst_audio_codec, NULL) < 0) 
        { 
            assert("*** avcodec_open2 - audio ***" && false);
            return false;
        } 
        
        if (avcodec_parameters_from_context(audio_stream->codecpar, ctx.audio_codec_ctx) < 0) 
        { 
            assert("*** avcodec_parameters_from_context - audio ***" && false);
            return false;
        }

        ctx.audio_stream = audio_stream;

        return true;
    }
}


/* create frames */

namespace video
{
    static bool create_av_frame(VideoReaderContext& ctx)
    {
        ctx.av_frame = av_frame_alloc();
        if (!ctx.av_frame)
        {      
            assert("*** av_frame_alloc ***" && false);      
            return false;
        }

        return true;
    }


    static bool create_av_rgba(VideoReaderContext& ctx, u32 width, u32 height)
    {
        auto w = (int)width;
        auto h = (int)height;
        auto fmt = AV_PIX_FMT_RGBA;
        int align = 32;

        AVFrame* avframe = av_frame_alloc();
        if (!avframe)
        {
            assert("*** av_frame_alloc ***" && false);
            return false;
        }

        avframe->format = (int)fmt;
        avframe->width = w;
        avframe->height = h;

        if (av_image_alloc(avframe->data, avframe->linesize, w, h, fmt, align) < 0)
        {
            assert("*** av_image_alloc ***" && false);
            av_frame_free(&avframe);
            return false;
        }

        if (av_frame_get_buffer(avframe, align) < 0)
        {
            assert("*** av_frame_get_buffer ***" && false);
            av_frame_free(&avframe);
            return false;
        }

        ctx.av_rgba = avframe;

        return true;
    }


    static bool create_av_frame(VideoWriterContext& ctx, u32 width, u32 height, AVPixelFormat fmt)
    {
        int w = (int)width;
        int h = (int)height;
        int align = 32;

        AVFrame* avframe = av_frame_alloc();
        if (!avframe)
        {
            assert("*** av_frame_alloc ***" && false);
            return false;
        }

        avframe->format = (int)fmt;
        avframe->width = w;
        avframe->height = h;

        if (av_frame_get_buffer(avframe, align) < 0)
        {
            assert("*** av_frame_get_buffer ***" && false);
            av_frame_free(&avframe);
            return false;
        }

        ctx.av_frame = avframe;

        return true;
    }


    static bool create_av_rgba(VideoWriterContext& ctx, u32 width, u32 height)
    {
        auto w = (int)width;
        auto h = (int)height;
        auto fmt = AV_PIX_FMT_RGBA;
        int align = 32;

        AVFrame* avframe = av_frame_alloc();
        if (!avframe)
        {
            assert("*** av_frame_alloc ***" && false);
            return false;
        }

        avframe->format = (int)fmt;
        avframe->width = w;
        avframe->height = h;

        if (av_image_alloc(avframe->data, avframe->linesize, w, h, fmt, align) < 0)
        {
            assert("*** av_image_alloc ***" && false);
            av_frame_free(&avframe);
            return false;
        }

        if (av_frame_get_buffer(avframe, align) < 0)
        {
            assert("*** av_frame_get_buffer ***" && false);
            av_frame_free(&avframe);
            return false;
        }

        ctx.av_rgba = avframe;

        return true;
    }
}


/* api */

namespace video
{
    
    bool open_video(VideoReader& video, cstr filepath)
    {
        auto data = mem::malloc<VideoReaderContext>("video context");
        if (!data)
        {
            return false;
        }

        video.video_handle = (u64)data;

        auto& ctx = get_context(video);

        ctx.format_ctx = avformat_alloc_context();

        auto close_1 = [&](){ avformat_free_context(ctx.format_ctx); };

        if (avformat_open_input(&ctx.format_ctx, filepath, nullptr, nullptr) != 0)
        {
            close_1();
            return false;
        }

        if (avformat_find_stream_info(ctx.format_ctx, nullptr) != 0)
        {
            close_1();
            return false;
        }

        ctx.video_stream = 0;
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
            close_1();
            return false;
        }

        ctx.video_stream = ctx.format_ctx->streams[video_stream_index];

        AVCodecParameters* cp = ctx.video_stream->codecpar;
        AVCodec* video_codec = avcodec_find_decoder(cp->codec_id);
        if (!video_codec)
        {
            close_1();
            return false;
        }

        ctx.video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (!ctx.video_codec_ctx)
        {
            close_1();
            return false;
        }

        auto close_2 = [&]()
        {
            close_1();
            avcodec_free_context(&ctx.video_codec_ctx);
        };

        if (avcodec_parameters_to_context(ctx.video_codec_ctx, cp) != 0)
        {
            close_2();
            return false;
        }

        if (avcodec_open2(ctx.video_codec_ctx, video_codec, nullptr) != 0)
        {
            close_2();
            return false;
        }

        if (!create_av_frame(ctx))
        {
            close_2();
            return false;
        }

        auto close_3 = [&]()
        {
            close_2();
            av_frame_free(&ctx.av_frame);
        };

        ctx.packet = av_packet_alloc();
        if (!ctx.packet)
        {
            close_3();
            return false;
        }

        video.frame_width = (u32)cp->width;
        video.frame_height = (u32)cp->height;        
        video.fps = av_q2d(ctx.video_stream->avg_frame_rate);
        
        ctx.audio_codec_ctx = 0;
        ctx.audio_stream = 0;
        AVCodec* audio_codec = 0;
        int audio_stream_index = av_find_best_stream(ctx.format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);
        if (audio_stream_index >= 0)
        { 
            ctx.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            if (ctx.audio_codec_ctx)
            {
                ctx.audio_stream = ctx.format_ctx->streams[audio_stream_index];
                if (avcodec_parameters_to_context(ctx.audio_codec_ctx, ctx.audio_stream->codecpar) < 0)
                {
                    avcodec_close(ctx.audio_codec_ctx);
                    ctx.audio_codec_ctx = 0;
                    ctx.audio_stream = 0;
                }
            }
        }

        auto close_4 = [&]()
        {
            close_3();
            av_packet_free(&ctx.packet);
        };

        if (!create_av_rgba(ctx, video.frame_width, video.frame_height))
        {
            close_4();
            return false;
        }

        u32 n_display_pixels = 2 * video.frame_width * video.frame_height;

        ctx.buffer32 = img::create_buffer32(n_display_pixels, "display_frames rgba");
        ctx.buffer8 = img::create_buffer8(n_display_pixels, "display_frames gray");

        if (!ctx.buffer32.ok || !ctx.buffer8.ok)
        {
            close_4();
            mb::destroy_buffer(ctx.buffer32);
            mb::destroy_buffer(ctx.buffer8);
        }

        for (u32 i = 0; i < 2; i++)
        {
            ctx.display_frames[i].rgba = img::make_view(video.frame_width, video.frame_height, ctx.buffer32);
            ctx.display_frames[i].gray = img::make_view(video.frame_width, video.frame_height, ctx.buffer8);
        }

        return true;
    }


    void close_video(VideoReader& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);
        
        av_frame_free(&ctx.av_frame);
        av_frame_free(&ctx.av_rgba);
        av_packet_free(&ctx.packet);
        avcodec_close(ctx.video_codec_ctx);
        avcodec_close(ctx.audio_codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mb::destroy_buffer(ctx.buffer32);
        mb::destroy_buffer(ctx.buffer8);

        mem::free(&ctx);

        video.video_handle = 0;
    }

    
    void process_video(VideoReader const& src, fn_frame const& cb)
    {
        auto on_read = [&]()
        {
            cb(current_frame(src));
        };

        for_each_video_frame(src, on_read);
    }


    bool process_video(VideoReader const& src, fn_frame const& cb, fn_bool const& proc_cond)
    {
        auto on_read = [&]()
        {
            cb(current_frame(src));
        };

        return for_each_video_frame(src, on_read, proc_cond);
    }
   
    
    bool create_video(VideoReader const& src, VideoWriter& dst, cstr dst_path, u32 dst_width, u32 dst_height)
    {
        auto data = mem::malloc<VideoWriterContext>("video gen context");
        if (!data)
        {
            return false;
        }

        dst.video_handle = (u64)data;

        auto& src_ctx = get_context(src);
        auto& ctx = get_context(dst);
        
        auto fmt = src_ctx.video_codec_ctx->pix_fmt;

        if (!create_av_frame(ctx, dst_width, dst_height, fmt))
        {
            assert(false);
            return false;
        }
        
        if (avformat_alloc_output_context2(&ctx.format_ctx, nullptr, nullptr, dst_path) < 0)
        {
            assert("*** avformat_alloc_output_context2 ***" && false);
            return false;
        }

        if (!create_video_stream(src_ctx, ctx, dst_width, dst_height))
        {
            assert(false);
            return false;
        }

        if (dst.write_audio && (!src_ctx.audio_stream || !create_audio_stream(src_ctx, ctx)))
        {
            dst.write_audio = false;
        }

        if (!(ctx.format_ctx->oformat->flags & AVFMT_NOFILE) && avio_open(&ctx.format_ctx->pb, dst_path, AVIO_FLAG_WRITE) < 0)
        {
            assert("*** avio_open ***" && false);
            return false;
        }

        if (avformat_write_header(ctx.format_ctx, nullptr) < 0)
        {
            assert("*** avformat_write_header ***" && false);
            return false;
        }

        dst.frame_width = dst_width;
        dst.frame_height = dst_height;

        if (!create_av_rgba(ctx, dst_width, dst_height))
        {
            assert(false);
            return false;
        }

        auto tb = ctx.video_stream->time_base.den / ctx.video_stream->time_base.num;
        auto fr = src_ctx.video_stream->avg_frame_rate.den / src_ctx.video_stream->avg_frame_rate.num;

        ctx.packet_duration = tb * fr;

        return true;
    }


    void close_video(VideoWriter& video)
    {
        if (!video.video_handle)
        {
            return;
        }

        auto& ctx = get_context(video);

        avio_closep(&ctx.format_ctx->pb);
        
        av_frame_free(&ctx.av_frame);
        avcodec_free_context(&ctx.video_codec_ctx);
        if (ctx.audio_stream)
        {
            avcodec_free_context(&ctx.audio_codec_ctx);
        }
        avformat_free_context(ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }


    void save_and_close_video(VideoWriter& video)
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
    
    
    void process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb)
    {
        auto src_ctx = get_context(src);
        auto dst_ctx = get_context(dst);

        auto src_av = src_ctx.av_frame;
        auto dst_av = dst_ctx.av_frame;
        auto dst_rgba = dst_ctx.av_rgba;

        auto const on_read_video = [&]()
        {
            cb(current_frame(src), get_frame_rgba(dst_ctx));
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);
        };

        if (src_ctx.audio_stream && dst_ctx.audio_stream)
        {
            auto const on_read_audio = [&]()
            {
                copy_audio(src_ctx, dst_ctx);
            };

            for_each_audio_video_frame(src, on_read_video, on_read_audio);
        }
        else
        {
            for_each_video_frame(src, on_read_video);
        }
    }
    
    
    bool process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb, fn_bool const& proc_cond)
    {
        auto src_ctx = get_context(src);
        auto dst_ctx = get_context(dst);

        auto src_av = src_ctx.av_frame;
        auto dst_av = dst_ctx.av_frame;
        auto dst_rgba = dst_ctx.av_rgba;

        auto const on_read_video = [&]()
        {
            cb(current_frame(src), get_frame_rgba(dst_ctx));
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);
        };

        if (src_ctx.audio_stream && dst_ctx.audio_stream)
        {
            auto const on_read_audio = [&]()
            {
                copy_audio(src_ctx, dst_ctx);
            };

            return for_each_audio_video_frame(src, on_read_video, on_read_audio, proc_cond);
        }
        else
        {
            return for_each_video_frame(src, on_read_video, proc_cond);
        }
    }
    
    
    VideoFrame current_frame(VideoReader const& video)
    {
        return get_context(video).display_frame_read();
    }

}


/* Deprecated */

namespace video
{
    // Deprecated
    static inline AVFrame* av_frame(FrameRGBA const& frame_rgba)
    {
        return (AVFrame*)frame_rgba.frame_handle;
    }


    // Deprecated
    static bool read_next_frame(VideoReaderContext const& ctx)
    {
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.av_frame;
        auto stream = ctx.video_stream;

        for (;;)
        {
            if (av_read_frame(ctx.format_ctx, ctx.packet) < 0)
            {
                //assert("*** av_read_frame ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (ctx.packet->stream_index != ctx.video_stream->index)
            {
                //assert("*** ctx.packet->stream_index != ctx.video_stream_index ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            if(avcodec_send_packet(ctx.video_codec_ctx, ctx.packet) < 0)
            {
                assert("*** avcodec_send_packet ***" && false);
                av_packet_unref(ctx.packet);
                return false;
            }

            if (avcodec_receive_frame(ctx.video_codec_ctx, ctx.av_frame) < 0)
            {
                //assert("*** avcodec_receive_frame ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            break;
        }

        convert_frame(ctx.av_frame, ctx.av_rgba);

        return true;
    }


    // Deprecated
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

    
    // Deprecated
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

        av_frame_get_buffer(av_frame, 32);

        frame.frame_handle = (u64)av_frame;

        frame.view.width = width;
        frame.view.height = height;
        frame.view.matrix_data_ = (img::Pixel*)av_frame->data[0];        

        return true;
    }


    // Deprecated
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


    // Deprecated
    bool next_frame(VideoReader const& video, FrameRGBA const& frame_out)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }

        convert_frame(ctx.av_frame, av_frame(frame_out));

        av_packet_unref(ctx.packet);

        return true;
    }

}