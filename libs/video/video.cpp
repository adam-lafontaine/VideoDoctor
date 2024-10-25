// sudo apt-get install ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "video.hpp"


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


/* api */

namespace video
{
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
}