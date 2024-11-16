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

        AVFrame* frame_av;
        AVPacket* packet;

        FrameRGBA frame_rgba;
    };


    class VideoWriterContext
    {
    public:
        AVFormatContext* format_ctx;

        AVCodecContext* video_codec_ctx;
        AVStream* video_stream;

        AVCodecContext* audio_codec_ctx;
        AVStream* audio_stream;

        AVFrame* frame_av;

        FrameRGBA frame_rgba;

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


    static inline AVFrame* av_frame(FrameRGBA const& frame_rgba)
    {
        return (AVFrame*)frame_rgba.frame_handle;
    }


    VideoFrame get_frame(VideoReader const& video)
    {
        auto ctx = get_context(video);

        VideoFrame frame;
        frame.rgba = ctx.frame_rgba.view;

        frame.gray.width = frame.rgba.width;
        frame.gray.height = frame.rgba.height;
        frame.gray.matrix_data_ = ctx.frame_av->data[0]; // assume YUV

        return frame;
    }


    VideoFrame get_frame(VideoWriter const& writer)
    {
        auto ctx = get_context(writer);

        VideoFrame frame;
        frame.rgba = ctx.frame_rgba.view;

        frame.gray.width = frame.rgba.width;
        frame.gray.height = frame.rgba.height;
        frame.gray.matrix_data_ = ctx.frame_av->data[0]; // assume YUV

        return frame;
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
    

    static void encode_video_frame(VideoWriterContext const& ctx, i64 pts)
    {
        auto encoder = ctx.video_codec_ctx;
        auto frame = ctx.frame_av;
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

        //av_packet_unref(packet);
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


    static void copy_frame(VideoReaderContext const& src_ctx, VideoWriterContext const& dst_ctx)
    {
        auto src_av = src_ctx.frame_av;
        auto src_rgba = av_frame(src_ctx.frame_rgba);
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        convert_frame(src_av, dst_av);
        convert_frame(src_av, src_rgba);
        convert_frame(src_av, dst_rgba);

        encode_video_frame(dst_ctx, src_ctx.frame_av->pts);
    }


    static void crop_frame(VideoReaderContext const& src_ctx, VideoWriterContext const& dst_ctx)
    { 
        auto decoder = src_ctx.video_codec_ctx;
        auto encoder = dst_ctx.video_codec_ctx;
        auto w = decoder->width;
        auto h = decoder->height;
        auto crop_w = encoder->width;
        auto crop_h = encoder->height;

        // TODO: detect crop position
        auto crop_x = w / 4;
        auto crop_y = h / 4;

        auto dst_data = dst_ctx.frame_av->data;
        auto dst_linesize = dst_ctx.frame_av->linesize;

        auto src_av = src_ctx.frame_av;
        auto src_rgba = av_frame(src_ctx.frame_rgba);
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        convert_frame(src_av, src_rgba);

        if (av_frame_get_buffer(dst_av, 32) < 0)
        {
            assert("*** av_frame_get_buffer / crop ***" && false);
        }

        #define USE_IMAGE

        #ifndef USE_IMAGE

        // Crop by setting src_data pointer for the RGBA frame
        u8* crop_data[AV_NUM_DATA_POINTERS] = { src_rgba->data[0] + crop_y * src_rgba->linesize[0] + crop_x * 4 };
        int crop_linesize[AV_NUM_DATA_POINTERS] = { src_rgba->linesize[0] };

        // Convert cropped RGBA frame to YUV420P        
        auto crop_sws = create_sws(src_rgba, crop_w, crop_h, dst_av, crop_w, crop_h);
        sws_scale(crop_sws, crop_data, crop_linesize, 0, crop_h,
                    dst_av->data, dst_av->linesize);                
        sws_freeContext(crop_sws);

        convert_frame(dst_av, dst_rgba);

        #else

        auto& src_view = src_ctx.frame_rgba.view;
        auto& dst_view = dst_ctx.frame_rgba.view;

        assert(dst_view.width == (u32)crop_w);
        assert(dst_view.height == (u32)crop_h);
        assert(dst_av->width == crop_w);
        assert(dst_av->height == crop_h);
        
        auto sub = img::sub_view(src_view, img::make_rect(crop_x, crop_y, crop_w, crop_h));
        img::copy(sub, dst_view);
        convert_frame(dst_rgba, dst_av);

        #endif

        encode_video_frame(dst_ctx, src_av->pts);        
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
        auto frame = ctx.frame_av;
        int video_stream_index = ctx.video_stream->index;

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
                        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));
                        on_read_video();
                    }
                }
            }
            av_packet_unref(packet);
        }
    }


    template <class FN1, class FN2> // std::function<void()>
    static void for_each_audio_video_frame(VideoReader const& src, FN1 const& on_read_video, FN2 const& on_read_audio)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.frame_av;
        int video_stream_index = ctx.video_stream->index;
        int audio_stream_index = -1;
        if (ctx.audio_stream)
        {
            audio_stream_index = ctx.audio_stream->index;
        }

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
                        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));
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
    }


    template <class FN> // std::function<void()>, std::function<bool()>
    static bool for_each_video_frame(VideoReader const& src, FN const& on_read_video, fn_bool const& cond)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.frame_av;
        int video_stream_index = ctx.video_stream->index;

        bool done = false;
        auto const read = [&]()
        { 
            done = av_read_frame(ctx.format_ctx, packet) < 0;
            return !done;
        };

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
                        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));
                        on_read_video();
                    }
                }
            }            
            av_packet_unref(packet);
        }

        return done;
    }


    template <class FN1, class FN2> // std::function<void()>, std::function<bool()>
    static bool for_each_audio_video_frame(VideoReader const& src, FN1 const& on_read_video, FN2 const& on_read_audio, fn_bool const& cond)
    {
        auto ctx = get_context(src);
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.frame_av;
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
                        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));
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

        av_frame_get_buffer(av_frame, 32);

        frame.frame_handle = (u64)av_frame;

        frame.view.width = width;
        frame.view.height = height;
        frame.view.matrix_data_ = (img::Pixel*)av_frame->data[0];        

        return true;
    }


    void resize_frame(FrameRGBA const& src, FrameRGBA const& dst)
    {
        auto av_src = (AVFrame*)src.frame_handle;
        auto av_dst = (AVFrame*)dst.frame_handle;

        convert_frame(av_src, av_dst);
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
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        ctx.video_stream = ctx.format_ctx->streams[video_stream_index];

        AVCodecParameters* cp = ctx.video_stream->codecpar;
        AVCodec* video_codec = avcodec_find_decoder(cp->codec_id);
        if (!video_codec)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        ctx.video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (!ctx.video_codec_ctx)
        {
            avformat_free_context(ctx.format_ctx);
            return false;
        }

        if (avcodec_parameters_to_context(ctx.video_codec_ctx, cp) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.video_codec_ctx);
            return false;
        }

        if (avcodec_open2(ctx.video_codec_ctx, video_codec, nullptr) != 0)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.video_codec_ctx);
            return false;
        }

        ctx.frame_av = av_frame_alloc();
        if (!ctx.frame_av)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.video_codec_ctx);
            return false;
        }

        ctx.packet = av_packet_alloc();
        if (!ctx.packet)
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.video_codec_ctx);
            av_frame_free(&ctx.frame_av);
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

        if (!create_frame(ctx.frame_rgba, video.frame_width, video.frame_height))
        {
            avformat_free_context(ctx.format_ctx);
            avcodec_free_context(&ctx.video_codec_ctx);
            av_frame_free(&ctx.frame_av);
            av_packet_free(&ctx.packet);
            return false;
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

        av_frame_free(&ctx.frame_av);
        av_packet_free(&ctx.packet);
        avcodec_close(ctx.video_codec_ctx);
        avcodec_close(ctx.audio_codec_ctx);
        avformat_close_input(&ctx.format_ctx);

        mem::free(&ctx);

        video.video_handle = 0;
    }

    
    void process_video(VideoReader const& src, fn_frame const& cb)
    {
        auto on_read = [&]()
        {
            cb(get_frame(src));
        };

        for_each_video_frame(src, on_read);
    }


    bool process_video(VideoReader const& src, fn_frame const& cb, fn_bool const& proc_cond)
    {
        auto on_read = [&]()
        {
            cb(get_frame(src));
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

        int w = (int)dst_width;
        int h = (int)dst_height;
        auto fmt = (int)src_ctx.video_codec_ctx->pix_fmt;

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

        if (!create_video_stream(src_ctx, ctx, dst_width, dst_height))
        {
            return false;
        }

        if (src_ctx.audio_stream && !create_audio_stream(src_ctx, ctx))
        {
            return false;
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

        if (!create_frame(ctx.frame_rgba, dst_width, dst_height))
        {
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
        
        av_frame_free(&ctx.frame_av);
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

        auto src_av = src_ctx.frame_av;
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        auto const on_read_video = [&]()
        {
            cb(get_frame(src), get_frame(dst).rgba);
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);
        };

        if (src_ctx.audio_stream)
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

        auto src_av = src_ctx.frame_av;
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        auto const on_read_video = [&]()
        {
            cb(get_frame(src), get_frame(dst).rgba);
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);
        };

        if (src_ctx.audio_stream)
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

}


/* Deprecated */

namespace video
{
    // Deprecated
    static bool read_next_frame(VideoReaderContext const& ctx)
    {
        auto packet = ctx.packet;
        auto decoder = ctx.video_codec_ctx;
        auto frame = ctx.frame_av;
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

            if (avcodec_receive_frame(ctx.video_codec_ctx, ctx.frame_av) < 0)
            {
                //assert("*** avcodec_receive_frame ***" && false);
                av_packet_unref(ctx.packet);
                continue;
            }

            break;
        }

        convert_frame(ctx.frame_av, av_frame(ctx.frame_rgba));

        return true;
    }


    // Deprecated
    void crop_video(VideoReader const& src, VideoWriter& dst, FrameList const& src_out, FrameList const& dst_out)
    {
        auto src_ctx = get_context(src);
        auto dst_ctx = get_context(dst);

        auto const crop = [&]()
        {            
            crop_frame(src_ctx, dst_ctx);

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

        for_each_video_frame(src, crop);
    }


    // Deprecated
    bool next_frame(VideoReader const& video, FrameRGBA const& frame_out)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }

        convert_frame(ctx.frame_av, av_frame(frame_out));

        av_packet_unref(ctx.packet);

        return true;
    }


    // Deprecated
    bool next_frame(VideoReader const& video, FrameList const& frames_out)
    {
        auto& ctx = get_context(video);        

        if (!read_next_frame(ctx))
        {
            return false;
        }        

        for (auto& frame : frames_out)
        {
            convert_frame(ctx.frame_av, av_frame(frame));
        }

        av_packet_unref(ctx.packet);

        return true;
    }
  
    
    void play_video(VideoReader const& video, FrameList const& frames_out)
    {
        auto ctx = get_context(video);

        auto const copy = [&]()
        {
            for (auto& out : frames_out)
            {
                convert_frame(ctx.frame_av, av_frame(out));
            }
        };

        for_each_video_frame(video, copy);
    }

    
    void process_video(VideoReader const& src, FrameRGBA const& dst, fn_frame_to_rgba const& cb, FrameList const& src_out, FrameList const& dst_out)
    {
        auto dst_view = dst.view;

        auto src_ctx = get_context(src);
        auto dst_av = av_frame(dst);

        auto on_read = [&]()
        {
            cb(get_frame(src), dst_view);

            for (auto& out : src_out)
            {
                convert_frame(src_ctx.frame_av, av_frame(out));
            }
            
            for (auto& out : dst_out)
            {
                convert_frame(dst_av, av_frame(out));
            }
        };

        for_each_video_frame(src, on_read);
    }


    bool process_video(VideoReader const& src, FrameRGBA const& dst, fn_frame_to_rgba const& cb, FrameList const& src_out, FrameList const& dst_out, fn_bool const& proc_cond)
    {
        auto dst_view = dst.view;

        auto src_ctx = get_context(src);
        auto dst_av = av_frame(dst);

        auto on_read = [&]()
        {
            cb(get_frame(src), dst_view);

            for (auto& out : src_out)
            {
                convert_frame(src_ctx.frame_av, av_frame(out));
            }
            
            for (auto& out : dst_out)
            {
                convert_frame(dst_av, av_frame(out));
            }
        };

        return for_each_video_frame(src, on_read, proc_cond);
    }


    void process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb, FrameList const& src_out, FrameList const& dst_out)
    {
        auto src_ctx = get_context(src);
        auto dst_ctx = get_context(dst);

        auto src_av = src_ctx.frame_av;
        auto src_rgba = av_frame(src_ctx.frame_rgba);
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        auto const on_read = [&]()
        {
            cb(get_frame(src), get_frame(dst).rgba);
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);         

            for (auto& out : src_out)
            {
                convert_frame(src_ctx.frame_av, av_frame(out));
            }

            auto dst_av = av_frame(dst_ctx.frame_rgba);
            for (auto& out : dst_out)
            {
                convert_frame(dst_av, av_frame(out));
            }
        };

        for_each_video_frame(src, on_read);
    }
    
    
    bool process_video(VideoReader const& src, VideoWriter& dst, fn_frame_to_rgba const& cb, FrameList const& src_out, FrameList const& dst_out, fn_bool const& proc_cond)
    {
        auto src_ctx = get_context(src);
        auto dst_ctx = get_context(dst);

        auto src_av = src_ctx.frame_av;
        auto src_rgba = av_frame(src_ctx.frame_rgba);
        auto dst_av = dst_ctx.frame_av;
        auto dst_rgba = av_frame(dst_ctx.frame_rgba);

        auto const on_read = [&]()
        {
            cb(get_frame(src), get_frame(dst).rgba);
            convert_frame(dst_rgba, dst_av);
            encode_video_frame(dst_ctx, src_av->pts);
            
            for (auto& out : src_out)
            {
                convert_frame(src_ctx.frame_av, av_frame(out));
            }

            auto dst_av = av_frame(dst_ctx.frame_rgba);
            for (auto& out : dst_out)
            {
                convert_frame(dst_av, av_frame(out));
            }
        };

        return for_each_video_frame(src, on_read, proc_cond);
    }

}