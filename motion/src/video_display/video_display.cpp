#include "video_display.hpp"
#include "../../../libs/util/stopwatch.hpp"
#include "../../../libs/util/numeric.hpp"

#include <thread>


/* vectors */

namespace vec
{
    namespace num = numeric;

    
    constexpr Vec2Df32 zero_f32 = { 0.0f, 0.0f };
    constexpr Vec2Du32 zero_u32 = { 0, 0 };
    constexpr Vec2Di32 zero_i32 = { 0, 0 };


    static Vec2Df32 to_direction(uangle rot)
    {
        return { num::cos(rot), num::sin(rot) };
    }


    static Vec2Df32 rotate(Vec2Df32 vec, Vec2Df32 direction)
    {
        return {
           vec.x * direction.x - vec.y * direction.y,
           vec.x * direction.y + vec.y * direction.x
        };
    }


    static Vec2Df32 rotate(Vec2Df32 vec, uangle rot)
    {
        return rotate(vec, to_direction(rot));
    }


    static Vec2Df32 add(Vec2Df32 a, Vec2Df32 b)
    {
        return { a.x + b.x, a.y + b.y };
    }


    static Vec2Df32 sub(Vec2Df32 a, Vec2Df32 b)
    {
        return { a.x - b.x, a.y - b.y };
    }
    

    static Vec2Df32 mul(Vec2Df32 a, f32 scalar)
    {
        return { a.x * scalar, a.y * scalar };
    }


    static f32 dot(Vec2Df32 a, Vec2Df32 b)
    {
        return a.x * b.x + a.y * b.y;
    }


    static Vec2Df32 unit(Vec2Df32 vec)
    {
        auto rsqrt = num::q_rsqrt(vec.x * vec.x + vec.y * vec.y); 
        return vec::mul(vec, rsqrt);
    }


    template <typename T>
    static Vec2Df32 to_f32(Vec2D<T> vec)
    {
        return { (f32)vec.x, (f32)vec.y };
    }


    template <typename uT>
    static Vec2D<uT> to_unsigned(Vec2Df32 vec)
    {
        return {
            num::round_to_unsigned<uT>(vec.x),
            num::round_to_unsigned<uT>(vec.y)
        };
    }

    static Vec2Du32 mul(Vec2Du32 a, u32 scalar)
    {
        return { a.x * scalar, a.y * scalar };
    }


    static Vec2Du32 mul(Vec2Du32 vec, f32 scalar)
    {
        return to_unsigned<u32>(mul(to_f32(vec), scalar));
    }

}


namespace video_display
{
    
    
    namespace num = numeric;
    
}


/* gray delta */

namespace video_display
{
namespace gd
{
    static Matrix32 make_matrix(u32 w, u32 h, img::Buffer32& buffer32)
    {
        Matrix32 mat{};
        mat.width = w;
        mat.height = h;
        mat.matrix_data_ = (f32*)mb::push_elements(buffer32, w * h);

        return mat;
    }
    
    
    static void next(GrayDelta& gd){ gd.index = (gd.index + 1) & gd.mask; }

    static Matrix32 front(GrayDelta const& gd) { return gd.list[gd.index]; }

    static f32 val_to_f32(u8 v) { return (f32)v; }


    static u8 abs_avg_delta(u8 v, f32 t) 
    {
        constexpr auto i_count = 1.0f / GrayDelta::count;
        constexpr auto thresh = 10.0f;

        return num::abs(t * i_count - v) >= thresh ? 255 : 0; 
    }


    static void update(GrayDelta& gd, img::GrayView const& src, img::GrayView const& dst)
    {
        auto t = img::to_span(gd.totals);
        auto f = img::to_span(front(gd));
        auto v = img::to_span(gd.values);
        auto o = img::to_span(gd.out);

        // report
        img::scale_down(src, gd.values);
        span::transform(v, t, o, abs_avg_delta);            
        img::scale_up(gd.out, dst);

        // update
        span::sub(t, f, t);
        span::transform(v, f, val_to_f32);
        span::add(t, f, t);
        next(gd);
    }


    static Point2Du32 locate_feature(GrayDelta& gd, Point2Du32 pos)
    {
        auto sensitivity = 0.7f;

        auto scale = (f32)SRC_VIDEO_WIDTH / gd.out.width;
        auto i_scale = 1.0f / scale;

        auto p = vec::mul(pos, i_scale);
        
        auto c = img::centroid(gd.out, p, sensitivity);

        return vec::mul(c, scale);
    }


    bool init(GrayDelta& gd, u32 width, u32 height)
    {
        auto n32 = width * height * (gd.count + 1);
        auto n8 = width * height * 2;

        auto& buffer32 = gd.buffer32;
        auto& buffer8 = gd.buffer8;

        buffer32 = img::create_buffer32(n32, "GrayAvg 32");
        if (!buffer32.ok)
        {
            return false;
        }

        buffer8 = img::create_buffer8(n8, "GrayAvg 8");
        if (!buffer8.ok)
        {
            return false;
        }

        mb::zero_buffer(buffer32);
        mb::zero_buffer(buffer8);

        for (u32 i = 0; i < gd.count; i++)
        {
            gd.list[i] = make_matrix(width, height, buffer32);
        }

        gd.totals = make_matrix(width, height, buffer32);

        gd.values = img::make_view(width, height, buffer8);
        gd.out = img::make_view(width, height, buffer8);

        return true;
    }


    void destroy(GrayDelta& gd)
    {
        mb::destroy_buffer(gd.buffer32);
        mb::destroy_buffer(gd.buffer8);
    }
}
}


/* internal */

namespace video_display
{
namespace internal
{
    using VLS = VideoLoadStatus;
    using VPS = VideoPlayStatus;
    
    
    static bool load_video(DisplayState& state)
    {
        auto path = state.src_video_filepath;
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            assert("*** bad video path ***" && false);
            return false;
        }

        reset_video(state);

        auto& video = state.src_video;

        auto ok = vid::open_video(video, state.src_video_filepath.string().c_str());
        if (!ok)
        {
            assert("*** vid::open_video ***" && false);
            return false;
        }

        auto w = video.frame_width;
        auto h = video.frame_height;

        assert(w && h && "*** No video dimensions ***");
        assert(w == SRC_VIDEO_WIDTH);
        assert(h == SRC_VIDEO_HEIGHT);

        u32 crop_w = DST_VIDEO_WIDTH;
        u32 crop_h = DST_VIDEO_HEIGHT;
        cstr crop_path = OUT_VIDEO_PATH;
        ok = vid::create_video(state.src_video, state.dst_video, crop_path, crop_w, crop_h);
        if (!ok)
        {
            assert("*** vid::create_video ***" && false);
            return false;
        }

        return true;
    }


    static void cap_framerate(Stopwatch& sw, f64 target_ns)
    {
        constexpr f64 fudge = 0.9;

        auto sleep_ns = target_ns - sw.get_time_nano();
        if (sleep_ns > 0)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds((i64)(sleep_ns * fudge)));
        }

        sw.start();
    }


    static void fill_all(DisplayState& state, img::GrayView const& src, img::ImageView const& dst)
    {
        img::fill(state.display_gray_view, img::to_pixel(100, 0, 0));
        img::fill(state.display_edges_view, img::to_pixel(0, 100, 0));
        img::fill(state.display_motion_view, img::to_pixel(100, 100, 0));
        img::fill(dst, img::to_pixel(0, 0, 100));
    }


    static Rect2Du32 get_crop_rect(Point2Du32 pt)
    {
        constexpr auto x_min = (SRC_VIDEO_WIDTH - DST_VIDEO_WIDTH) / 2;
        constexpr auto y_min = (SRC_VIDEO_HEIGHT - DST_VIDEO_HEIGHT) / 2;
        constexpr auto x_max = SRC_VIDEO_WIDTH - x_min;
        constexpr auto y_max = SRC_VIDEO_HEIGHT - y_min;

        auto x = num::clamp(pt.x, x_min, x_max);
        auto y = num::clamp(pt.y, y_min, y_max);

        Rect2Du32 r{};
        r.x_begin = x - DST_VIDEO_WIDTH / 2;
        r.x_end = r.x_begin + DST_VIDEO_WIDTH;
        r.y_begin = y - DST_VIDEO_HEIGHT / 2;
        r.y_end = r.y_begin + DST_VIDEO_HEIGHT;

        return r;
    }


    static void process_frame(DisplayState& state, img::GrayView const& src, img::ImageView const& dst)
    {
        auto src_gray = vid::frame_gray_view(state.src_video);

        img::scale_down(src_gray, state.proc_gray_view);
        img::gradients(state.proc_gray_view, state.proc_edges_view);

        gd::update(state.edge_gd, state.proc_edges_view, state.proc_motion_view);

        state.feature_position = gd::locate_feature(state.edge_gd, state.feature_position);

        auto acc = 0.05f;

        auto fp = vec::to_f32(state.feature_position);
        auto dp = vec::to_f32(state.display_position);

        auto d_px = vec::sub(fp, dp);

        state.display_position = vec::to_unsigned<u32>(vec::add(dp, vec::mul(d_px, acc)));
        
        
        img::copy(img::sub_view(vid::frame_view(state.src_video), get_crop_rect(state.display_position)),  dst);
        
        img::map_scale_up(state.proc_gray_view, state.display_gray_view);
        img::map_scale_up(state.proc_edges_view, state.display_edges_view);
        img::map_scale_up(state.proc_motion_view, state.display_motion_view);
    }

    
    static void process_video(DisplayState& state)
    {
        vid::FrameList src_frames = { state.display_src_frame };
        vid::FrameList dst_frames = { state.display_preview_frame };

        auto const proc = [&](auto const& vs, auto const& vd)
        {
            process_frame(state, vs, vd);
        };

        auto& src = state.src_video;
        auto& dst = state.dst_video;

        vid::process_video(src, dst, proc, src_frames, dst_frames);
        reset_video(state);
        vid::save_and_close_video(dst);
    }


    void load_video_async(DisplayState& state)
    {
        auto const load = [&]()
        {
            state.load_status = VLS::InProgress;
            auto ok = load_video(state);
            if (ok)
            {
                state.load_status = VLS::Loaded;
                state.play_status = VPS::Pause;
            }
            else
            {
                state.load_status = VLS::NotLoaded;
                state.play_status = VPS::NotLoaded;
            }            
        };

        std::thread th(load);
        th.detach();
    }


    void process_video_async(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        if (state.play_status != VPS::Pause)
        {
            return;
        }

        auto const play = [&]()
        {
            state.play_status = VPS::Play;
            process_video(state);
            state.play_status = VPS::Pause;
        };

        std::thread th(play);
        th.detach();
    }
}
}