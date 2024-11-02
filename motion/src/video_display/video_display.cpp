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

        vid::create_frame(state.dst_frame, crop_w, crop_h);

        /*cstr crop_path = OUT_VIDEO_PATH;
        ok = vid::create_video(state.src_video, state.dst_video, crop_path, crop_w, crop_h);
        if (!ok)
        {
            assert("*** vid::create_video ***" && false);
            return false;
        }*/

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


    static Rect2Du32 get_crop_rect(Point2Du32 pt, u32 crop_w, u32 crop_h, Rect2Du32 bounds)
    {
        auto w = bounds.x_end - bounds.x_begin;
        auto h = bounds.y_end - bounds.y_begin;

        auto w2 = crop_w / 2;
        auto h2 = crop_h / 2;

        auto x_min = bounds.x_begin + w2;
        auto y_min = bounds.y_begin + h2;
        auto x_max = bounds.x_end - w2;
        auto y_max = bounds.y_end - h2;

        auto x = num::clamp(pt.x, x_min, x_max);
        auto y = num::clamp(pt.y, y_min, y_max);

        Rect2Du32 r{};
        r.x_begin = x - w2;
        r.x_end   = r.x_begin + crop_w;
        r.y_begin = y - h2;
        r.y_end   = r.y_begin + crop_h;

        return r;
    }


    static void update_dst_position(DisplayState& state)
    {
        if (!state.motion_on)
        {
            return;
        }

        auto acc = 0.08f;

        auto fp = vec::to_f32(state.feature_position);
        auto dp = vec::to_f32(state.dst_position);

        auto d_px = vec::sub(fp, dp);
        auto v_px = vec::mul(d_px, acc);

        auto pos = vec::to_unsigned<u32>(vec::add(dp, v_px));

        if (state.motion_x_on)
        {
            state.dst_position.x = pos.x;
        }

        if (state.motion_y_on)
        {
            state.dst_position.y = pos.y;
        }
    }


    static Rect2Du32 rect_scale_down(Rect2Du32 rect, u32 scale)
    {
        rect.x_begin /= scale;
        rect.x_end /= scale;
        rect.y_begin /= scale;
        rect.y_end /= scale;

        return rect;
    }


    static void process_frame(DisplayState& state, img::GrayView const& src, img::ImageView const& dst)
    {
        constexpr auto motion_scale = SRC_VIDEO_WIDTH / MOTION_WIDTH;
        constexpr auto proc_scale = SRC_VIDEO_WIDTH / PROC_IMAGE_WIDTH;
        constexpr auto display_scale = SRC_VIDEO_WIDTH / DISPLAY_FRAME_WIDTH;

        auto src_gray = vid::frame_gray_view(state.src_video);
        auto& proc_gray = state.proc_gray_view;
        auto& proc_edges = state.proc_edges_view;
        auto& proc_motion = state.proc_motion_view;

        img::scale_down(src_gray, proc_gray);
        img::gradients(proc_gray, proc_edges);

        auto proc_scan_rect = rect_scale_down(state.src_scan_region, proc_scale);

        motion::update(state.edge_motion, proc_edges, proc_scan_rect, proc_motion);
        state.feature_position = motion::scale_location(state.edge_motion, motion_scale);

        update_dst_position(state);
        auto dst_rect = get_crop_rect(state.dst_position, dst.width, dst.height, state.src_dst_region);
        
        img::copy(img::sub_view(vid::frame_view(state.src_video), dst_rect), dst);
        
        img::map_scale_up(proc_gray, state.display_gray_view);
        img::map_scale_up(proc_edges, state.display_edges_view);
        img::map_scale_up(proc_motion, state.display_motion_view);
        
        // TODO: add overlays
        constexpr auto blue = img::to_pixel(0, 0, 255);
        constexpr auto green = img::to_pixel(0, 255, 0);
        constexpr auto dark_green = img::to_pixel(0, 100, 0);
        constexpr auto red = img::to_pixel(255, 0, 0);
        u32 line_th = 4;        

        if (state.show_motion)
        {
            auto const dm = [&](u8 d, u8 m){ return m ? blue : img::to_pixel(d); };
            img::transform_scale_up(proc_gray, proc_motion, state.vfx_view, dm);
        }
        else
        {
            img::map_scale_up(proc_gray, state.vfx_view);
        }

        if (state.show_dst_region)
        {
            auto rect = rect_scale_down(state.src_dst_region, display_scale);
            img::draw_rect(state.vfx_view, rect, dark_green, line_th);
        }

        if (state.show_scan_region)
        {
            auto rect = rect_scale_down(state.src_scan_region, display_scale);
            img::draw_rect(state.vfx_view, rect, red, line_th);
        }

        if (state.show_dst_region)
        {
            auto rect = rect_scale_down(dst_rect, display_scale);
            img::draw_rect(state.vfx_view, rect, green, line_th);
        }

        img::copy(state.vfx_view, state.display_vfx_view);
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
        //auto& dst = state.dst_video;
        auto& dst = state.dst_frame;

        vid::process_video(src, dst, proc, src_frames, dst_frames);
        reset_video(state);
        //vid::save_and_close_video(dst);
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
    

    void motion_detection_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Motion Detection");

        ImGui::Checkbox("ON/OFF", &state.motion_on);

        ImGui::SameLine();
        ImGui::Checkbox("X", &state.motion_x_on);

        ImGui::SameLine();
        ImGui::Checkbox("Y", &state.motion_y_on);

        ImGui::Checkbox("Show motion", &state.show_motion);

        ImGui::Text("Sensitivity");
        ImGui::SliderFloat(
            "Motion##Slider",
            &state.edge_motion.motion_sensitivity,
            0.5f, 0.9999f,
            "%6.4f"
        );

        ImGui::SliderFloat(
            "Locate",
            &state.edge_motion.locate_sensitivity,
            0.9f, 0.9999f,
            "%6.4f"
        );
    }


    void scan_region_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Scan Region");

        ImGui::Checkbox("Show scan region", &state.show_scan_region);

        auto& scan_region = state.src_scan_region;

        int x_begin = scan_region.x_begin;
        int x_end = scan_region.x_end;
        int x_min = 0;
        int x_max = state.src_video.frame_width;

        ImGui::DragIntRange2(
            "Scan X", 
            &x_begin, &x_end, 
            4, 
            x_min, x_max, 
            "Min: %d", "Max: %d");
        
        scan_region.x_begin = (u32)x_begin;
        scan_region.x_end = (u32)x_end;

        int y_begin = scan_region.y_begin;
        int y_end = scan_region.y_end;
        int y_min = 0;
        int y_max = state.src_video.frame_height;

        ImGui::DragIntRange2(
            "Scan Y", 
            &y_begin, &y_end, 
            4, 
            y_min, y_max, 
            "Min: %d", "Max: %d");

        scan_region.y_begin = (u32)y_begin;
        scan_region.y_end = (u32)y_end;
    }


    void display_region_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Display Region");

        ImGui::Checkbox("Show display region", &state.show_dst_region);

        auto& dst_region = state.src_dst_region;

        int src_width = state.src_video.frame_width;
        int src_height = state.src_video.frame_height;

        if (!src_width)
        {
            return;
        }

        int dst_width = state.dst_frame.view.width;
        int dst_height = state.dst_frame.view.height;

        static int x_begin;
        static int x_end;

        x_begin = dst_region.x_begin;
        x_end = dst_region.x_end;

        int x_min = 0;
        int x_max = src_width;

        int b = x_begin;
        int e = x_end;

        ImGui::DragIntRange2(
            "Display X", 
            &x_begin, &x_end, 
            4, 
            x_min, x_max, 
            "Min: %d", "Max: %d");
        
        if (x_begin < b)
        {
            dst_region.x_begin = (u32)num::max(0, x_begin);
        }
        else if (x_begin > b)
        {
            dst_region.x_begin = (u32)num::clamp(x_begin, 0, x_end - dst_width);
        }
        else if (x_end < e)
        {
            dst_region.x_end = (u32)num::clamp(x_end, x_begin + dst_width, src_width);
        }
        else if (x_end > e)
        {
            dst_region.x_end = (u32)num::min(x_end, src_width);
        }

        static int y_begin;
        static int y_end;

        y_begin = dst_region.y_begin;
        y_end = dst_region.y_end;

        int y_min = 0;
        int y_max = src_height;

        b = y_begin;
        e = y_end;

        ImGui::DragIntRange2(
            "Display Y", 
            &y_begin, &y_end, 
            4, 
            y_min, y_max, 
            "Min: %d", "Max: %d");

        if (y_begin < b)
        {
            dst_region.y_begin = (u32)num::max(0, y_begin);
        }
        else if (y_begin > b)
        {
            dst_region.y_begin = (u32)num::clamp(y_begin, 0, y_end - dst_height);
        }
        else if (y_end < e)
        {
            dst_region.y_end = (u32)num::clamp(y_end, y_begin + dst_height, src_height);
        }
        else if (y_end > e)
        {
            dst_region.y_end = (u32)num::min(y_end, src_height);
        }
    }
}
}