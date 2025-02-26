#include "video_display.hpp"
#include "../../../libs/util/stopwatch.hpp"
#include "../../../libs/util/numeric.hpp"
#include "../../../libs/stb_libs/qsprintf.hpp"

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


/* internal */

namespace video_display
{
    namespace num = numeric;


namespace internal
{
    using VLS = VideoLoadStatus;
    using VPS = VideoPlayStatus;


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


    static fs::path timestamp_file_path(fs::path const& dir, cstr name, cstr ext)
    {
        auto time = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();

        char buffer[256] = { 0 };
        auto str = span::make_view(256, buffer);

        span::sprintf(str, "%s_%I64d%s", name, ms, ext);

        return dir / span::to_cstr(str);
    }


    static bool load_src_video(VideoMotionState& vms, fs::path const& video_path)
    {
        if (!fs::exists(video_path) || !fs::is_regular_file(video_path))
        {
            assert("*** bad video path ***" && false);
            return false;
        }

        auto ok = vid::open_video(vms.src_video, video_path.string().c_str());
        if (!ok)
        {
            assert("*** vid::open_video ***" && false);
            return false;
        }

        return true;
    }


    static bool init_vms(VideoMotionState& vms)
    {
        auto w = vms.src_video.frame_width;
        auto h = vms.src_video.frame_height;

        u32 process_w = PROCESS_IMAGE_WIDTH;
        u32 process_h = PROCESS_IMAGE_HEIGHT;

        motion::destroy(vms.gm);

        if (!motion::create(vms.gm, process_w, process_h))
        {
            return false;
        }

        vms.gm.src_location = { w / 2, h / 2 };
        vms.out_position = { w / 2, h / 2 };

        vms.out_position_acc = 0.15f;

        vms.out_limit_region = img::make_rect(w, h);
        vms.scan_region = img::make_rect(w, h);

        return true;
    }
    
    
    static bool set_out_dimensions(DisplayState& state, u32 width, u32 height)
    {
        auto dims = state.src_dims();

        width = num::clamp(width, DISPLAY_FRAME_HEIGHT, dims.x);
        height = num::clamp(height, DISPLAY_FRAME_HEIGHT, dims.y);        
        
        img::destroy_image(state.out_image);

        if (!img::create_image(state.out_image, width, height, "out_image"))
        {
            return false;
        }

        state.out_width = width;
        state.out_height = height;

        auto display_w = state.display_preview_view.width;
        auto display_h = state.display_preview_view.height;

        auto wr = (f32)width / display_w;
        auto hr = (f32)height / display_h;

        u32 w = display_w;
        u32 h = display_h;

        if (wr > hr)
        {
            h = w * height / width;
        }
        else
        {
            w = h * width / height;
        }

        auto x = (display_w - w) / 2;
        auto y = (display_h - h) / 2;

        auto r = img::make_rect(x, y, w, h);

        state.preview_dst = img::sub_view(state.display_preview_view, r);

        return true;
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
    
    
    static bool load_video(DisplayState& state)
    {
        reset_video_status(state);

        auto& vms = state.vms;

        if (!load_src_video(vms, state.src_video_filepath))
        {
            return false;
        }

        auto dims = state.src_dims();

        assert(dims.x && dims.y && "*** No video dimensions ***");

        if (!init_vms(state.vms))
        {
            assert("*** init_vms ***" && false);
            return false;
        }

        auto w = WIDTH_720P;
        auto h = HEIGHT_720P;

        if (!set_out_dimensions(state, w, h))
        {
            assert("*** set_out_dimensions load_video ***" && false);
            return false;
        }

        state.vms.out_region = get_crop_rect(vms.out_position, w, h, vms.out_limit_region);

        return true;
    }


    static bool reload_video(DisplayState& state)
    {
        vid::close_video(state.vms.src_video);
        reset_video_status(state);

        if (!load_src_video(state.vms, state.src_video_filepath))
        {
            return false;
        }

        return true;
    }


    static Rect2Du32 rect_scale_down(Rect2Du32 rect, u32 scale)
    {
        rect.x_begin /= scale;
        rect.x_end /= scale;
        rect.y_begin /= scale;
        rect.y_end /= scale;

        return rect;
    }


    static void update_out_position(DisplayState& state)
    {
        if (!state.motion_on)
        {
            return;
        }

        auto& vms = state.vms;

        auto fp = vec::to_f32(vms.gm.src_location);
        auto dp = vec::to_f32(vms.out_position);

        auto d_px = vec::sub(fp, dp);
        
        auto acc = vms.out_position_acc;

        auto v_px = vec::mul(d_px, acc);

        vms.out_position = vec::to_unsigned<u32>(vec::add(dp, v_px));
    }


    static void update_vfx(DisplayState& state)
    {
        auto display_scale = state.display_scale();

        if (!display_scale || state.load_status != VideoLoadStatus::Loaded)
        {
            return;
        }

        auto& vms = state.vms;
        auto& out_rect = vms.out_region;
        auto& proc_gray = vms.gm.proc_gray_view;
        auto& proc_edges = vms.gm.proc_edges_view;
        auto& proc_motion = vms.gm.proc_motion_view;
        
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
            auto gray = vid::current_frame(vms.src_video).gray;
            img::map_scale_down(gray, state.vfx_view);
        }

        if (state.show_out_region)
        {
            auto rect = rect_scale_down(vms.out_limit_region, display_scale);
            img::draw_rect(state.vfx_view, rect, dark_green, line_th);

            rect = rect_scale_down(out_rect, display_scale);
            img::draw_rect(state.vfx_view, rect, green, line_th);
        }

        if (state.show_scan_region)
        {
            auto rect = rect_scale_down(vms.scan_region, display_scale);
            img::draw_rect(state.vfx_view, rect, red, line_th);
        }

        img::copy(state.vfx_view, state.display_vfx_view);
    }


    static void process_frame_read(DisplayState& state, vid::VideoFrame src_frame)
    {
        auto& vms = state.vms;
        auto& out_rect = vms.out_region;
        
        auto src_gray = src_frame.gray;
        auto src_rgba = src_frame.rgba;
        auto out = state.out_view();

        auto w = state.out_width;
        auto h = state.out_height;

        motion::update(vms.gm, src_gray, vms.scan_region);
        update_out_position(state);

        out_rect = get_crop_rect(vms.out_position, w, h, vms.out_limit_region);
        img::copy(img::sub_view(src_rgba, out_rect), out);
        img::resize(out, state.preview_dst);
    }


    static void process_frame_write(DisplayState& state, vid::VideoFrame src_frame, img::ImageView const& dst)
    {
        process_frame_read(state, src_frame);

        img::copy(state.out_view(), dst);
    }

    
    static void process_play_video(DisplayState& state)
    {
        img::fill(state.display_preview_view, img::to_pixel(0));

        auto& src_video = state.vms.src_video;

        auto const proc = [&](auto const& fr_src)
        {
            process_frame_read(state, fr_src);            
        };

        auto const cond = [&](){ return state.play_status == VPS::Play; };        

        if (vid::process_video(src_video, proc, cond))
        {
            reset_video_status(state);
        }
    }


    static void process_generate_video(DisplayState& state)
    {
        img::fill(state.display_preview_view, img::to_pixel(0));

        auto& src_video = state.vms.src_video;
        auto& dst_video = state.dst_video;

        auto temp_path = OUT_VIDEO_TEMP_PATH;
        auto ok = vid::create_video(src_video, dst_video, temp_path, state.out_width, state.out_height);
        if (!ok)
        {
            assert("*** vid::create_video ***" && false);
            return;
        }

        auto const proc = [&](auto const& fr_src, auto const& v_out)
        {
            process_frame_write(state, fr_src, v_out);
        };

        auto const cond = [&](){ return state.play_status == VPS::Generate; };   

        if (vid::process_video(src_video, dst_video, proc, cond))
        {
            reset_video_status(state);
            vid::close_video(src_video);
            vid::save_and_close_video(dst_video);
            fs::rename(temp_path, timestamp_file_path(OUT_VIDEO_DIR, "out_video", VIDEO_EXTENSION));
        }
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


    void reload_video_async(DisplayState& state)
    {
        auto const load = [&]()
        {
            state.load_status = VLS::InProgress;
            auto ok = reload_video(state);
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


    void play_video_async(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        if (state.play_status != VPS::Pause)
        {
            return;
        }

        auto const play = [&]()
        {
            state.play_status = VPS::Play;
            process_play_video(state);
            state.play_status = VPS::Pause;
        };

        std::thread th(play);
        th.detach();
    }


    void generate_video_async(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        if (state.play_status != VPS::Pause)
        {
            return;
        }

        auto const gen = [&]()
        {
            state.play_status = VPS::Generate;
            process_generate_video(state);
            state.play_status = VPS::Pause;
        };

        std::thread th(gen);
        th.detach();
    }


    void pause_video(DisplayState& state)
    {
        state.play_status = VPS::Pause;
    }


    void motion_detection_settings(DisplayState& state)
    {
        auto& vms = state.vms;

        ImGui::SeparatorText("Motion Detection");

        ImGui::Checkbox("ON/OFF", &state.motion_on);

        ImGui::SameLine();

        ImGui::Checkbox("Show motion", &state.show_motion);

        ImGui::Text("Sensitivity");
        ImGui::SliderFloat(
            "Motion##Slider",
            &vms.gm.edge_motion.motion_sensitivity,
            0.5f, 0.9999f,
            "%6.4f"
        );

        ImGui::SliderFloat(
            "Locate",
            &vms.gm.edge_motion.locate_sensitivity,
            0.9f, 0.9999f,
            "%6.4f"
        );

        ImGui::SliderFloat(
            "Movement",
            &vms.out_position_acc,
            0.05f, 0.5f,
            "%6.4f"
        );

        if (ImGui::Button("Reset##motion_detection_settings"))
        {
            state.motion_on = true;
            state.show_motion = true;
            vms.gm.edge_motion.motion_sensitivity = 0.9f;
            vms.gm.edge_motion.locate_sensitivity = 0.98;
            vms.out_position_acc = 0.15f;
        }
    }


    void scan_region_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Scan Region");

        ImGui::Checkbox("Show scan region", &state.show_scan_region);

        auto& vms = state.vms;

        int src_width = vms.src_video.frame_width;
        int src_height = vms.src_video.frame_height;

        if (!src_width)
        {
            return;
        }

        static bool lock_to_display = false;

        ImGui::Checkbox("Lock to display", &lock_to_display);

        auto& dst_region = vms.out_region;
        auto& scan_region = vms.scan_region;
        
        if (lock_to_display)
        {
            scan_region = dst_region;
        }

        int x_begin = scan_region.x_begin;
        int x_end = scan_region.x_end;
        int x_min = 0;
        int x_max = src_width;

        int y_begin = scan_region.y_begin;
        int y_end = scan_region.y_end;
        int y_min = 0;
        int y_max = src_height;

        if (lock_to_display) { ImGui::BeginDisabled(); }

        ImGui::DragIntRange2(
            "Scan X", 
            &x_begin, &x_end, 
            4, 
            x_min, x_max, 
            "Min: %d", "Max: %d");

        ImGui::DragIntRange2(
            "Scan Y", 
            &y_begin, &y_end, 
            4, 
            y_min, y_max, 
            "Min: %d", "Max: %d");

        if (!lock_to_display)
        {
            scan_region.x_begin = (u32)x_begin;
            scan_region.x_end = (u32)x_end;

            scan_region.y_begin = (u32)y_begin;
            scan_region.y_end = (u32)y_end;
        }

        if (ImGui::Button("Reset##scan_region_settings"))
        {
            state.show_scan_region = true;
            scan_region.x_begin = (u32)x_min;
            scan_region.x_end = (u32)x_max;
            scan_region.y_begin = (u32)y_min;
            scan_region.y_end = (u32)y_max;
        }
        
        if (lock_to_display) { ImGui::EndDisabled(); }
    }


    void display_region_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Display Region");

        ImGui::Checkbox("Show display region", &state.show_out_region);

        auto& vms = state.vms;

        auto& dst_region = vms.out_limit_region;

        int src_width = vms.src_video.frame_width;
        int src_height = vms.src_video.frame_height;

        if (!src_width)
        {
            return;
        }

        auto dst_view = state.out_view();
        int dst_width = dst_view.width;
        int dst_height = dst_view.height;

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

        if (ImGui::Button("Reset##display_region_settings"))
        {
            state.show_out_region = true;
            dst_region.x_begin = (u32)x_min;
            dst_region.x_end = (u32)x_max;
            dst_region.y_begin = (u32)y_min;
            dst_region.y_end = (u32)y_max;
        }
    }


    void out_video_settings(DisplayState& state)
    {
        ImGui::SeparatorText("Out Video");

        constexpr auto N = IM_ARRAYSIZE(OUT_SIZES);

        char labels[N][6] = { 0 };
        for (u32 i = 0; i < N; i++)
        {
            stb::qsnprintf(labels[i], 6, "%u", OUT_SIZES[i]);
        }

        static int width_id = 0;
        static int height_id = 0;
        static u32 src_width = 0;
        static u32 src_height = 0;
        auto dims = state.src_dims();

        if (!dims.x && !dims.y)
        {
            return;
        }

        // new video loaded
        auto set_ui_dims = dims.x && dims.y && (src_width != dims.x || src_height != dims.y);

        if (set_ui_dims)
        {
            src_width = dims.x;
            src_height = dims.y;

            for (int i = 0; i < N; i++)
            {
                auto size = OUT_SIZES[i];
                if (state.out_width == size)
                {
                    width_id = i;
                }

                if (state.out_height == size)
                {
                    height_id = i;
                }
            }
        }

        auto combo_disabled = state.play_status != VideoPlayStatus::Pause;

        if (combo_disabled) { ImGui::BeginDisabled(); }

        if (ImGui::BeginCombo("Width##WidthCombo", labels[width_id]))
        {
            for (int i = 0; i < N; i++)
            {
                if (OUT_SIZES[i] > dims.x)
                {
                    continue;
                }

                auto is_selected = (width_id == i);
                
                if (ImGui::Selectable(labels[i], is_selected))
                {
                    width_id = i;
                }

                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Height##HeightCombo", labels[height_id]))
        {
            for (int i = 0; i < N; i++)
            {
                if (OUT_SIZES[i] > dims.y)
                {
                    continue;
                }

                auto is_selected = (height_id == i);
                if (ImGui::Selectable(labels[i]))
                {
                    height_id = i;
                }

                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button("Set"))
        {
            u32 w = OUT_SIZES[width_id];
            u32 h = OUT_SIZES[height_id];

            auto& vms = state.vms;

            set_out_dimensions(state, w, h);
            state.vms.out_region = get_crop_rect(vms.out_position, w, h, vms.out_limit_region);
        }

        if (combo_disabled) { ImGui::EndDisabled(); }
    }


    void start_vfx(DisplayState& state)
    {
        state.vfx_running = true;

        auto const run = [&]()
        {
            Stopwatch sw;
            sw.start();
            while (state.vfx_running)
            {
                update_vfx(state);
                cap_framerate(sw, 16);
            }
        };

        std::thread th(run);
        th.detach();
    }
}
}