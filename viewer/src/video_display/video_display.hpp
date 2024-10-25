#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/video/video.hpp"
#include "../../../libs/util/stopwatch.hpp"

#include <thread>
#include <filesystem>

namespace fs = std::filesystem;


namespace video_display
{
    // temp
    constexpr auto VIDEO_PATH = "/media/adam/Samsung 1TB/Videos/2024_06_03_Peter_Pan.mp4";
}


namespace video_display
{
    namespace img = image;
    namespace vid = video;


    enum class VideoLoadStatus : u8
    {
        NotLoaded = 0,
        InProgress,
        Loaded,
        Fail
    };


    enum class VideoPlayStatus : u8
    {
        NotLoaded = 0,
        Play,
        Pause
    };


    class DisplayState
    {
    public:

        vid::Video video;

        vid::FrameRGBA display_frame;
        ImTextureID display_frame_texture;

        VideoLoadStatus load_status = VideoLoadStatus::NotLoaded;
        VideoPlayStatus play_status = VideoPlayStatus::NotLoaded;

    };


    inline void destroy(DisplayState& state)
    {
        vid::destroy_frame(state.display_frame);
        vid::close_video(state.video);
    }


    inline bool init(DisplayState& state)
    {
        u32 w = 640;
        u32 h = 360;
        
        if (!vid::create_frame(state.display_frame, w, h))
        {
            return false;
        }

        return true;
    }    
}


/* internal */

namespace video_display
{
namespace internal
{
    static bool load_video(DisplayState& state)
    {
        auto path = fs::path(VIDEO_PATH);
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            return false;
        }

        return vid::open_video(state.video, VIDEO_PATH);
    }


    static void load_video_async(DisplayState& state)
    {
        using VLS = VideoLoadStatus;
        using VPS = VideoPlayStatus;

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


    static void play_video(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        constexpr f64 NANO = 1'000'000'000;

        auto target_s = 1.0 / state.video.fps;
        auto target_ns = target_s * NANO;

        state.play_status = VPS::Play;

        Stopwatch sw;
        sw.start();
        while (state.play_status == VPS::Play)
        {
            vid::next_frame(state.video, state.display_frame);

            //cap_framerate(sw, target_ns);
        }
    }


    static void play_video_async(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        if (state.play_status != VPS::Pause)
        {
            return;
        }

        auto const play = [&]()
        {
            play_video(state);
        };

        std::thread th(play);
        th.detach();
    }


    static void pause_video(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        state.play_status = VPS::Pause;
    }
}
}


namespace video_display
{
    void video_frame_window(DisplayState& state)
    {
        using VLS = VideoLoadStatus;
        using VPS = VideoPlayStatus;

        auto view = state.display_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_frame_texture;

        auto load_disabled = state.load_status != VLS::NotLoaded;
        auto play_pause_disabled = state.load_status != VLS::Loaded;

        auto red = img::to_pixel(100, 0, 0);
        auto green = img::to_pixel(0, 100, 0);
        auto blue = img::to_pixel(0, 0, 100);

        auto s = state.load_status;        

        auto color = s == VLS::NotLoaded ? blue : (s == VLS::Loaded ? green : red);

        //img::fill(view, color);


        ImGui::Begin("Video");

        ImGui::Image(texture, dims);

        if (load_disabled) { ImGui::BeginDisabled(); }
        if (ImGui::Button("Load"))
        {
            internal::load_video_async(state);
        }
        if (load_disabled) { ImGui::EndDisabled(); }

        ImGui::SameLine();

        if (play_pause_disabled) { ImGui::BeginDisabled(); }

        if (state.play_status == VPS::Pause)
        {
            if (ImGui::Button("Play"))
            {
                internal::play_video_async(state);
            }
        }
        else if (state.play_status == VPS::Play)
        {
            if (ImGui::Button("Pause"))
            {
                internal::pause_video;
            }
        }
        if (play_pause_disabled) { ImGui::EndDisabled(); }
        
       

        ImGui::End();
    }
}