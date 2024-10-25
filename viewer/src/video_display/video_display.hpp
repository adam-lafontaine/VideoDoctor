#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/imgui/imfilebrowser.hpp"
#include "../../../libs/video/video.hpp"
#include "../../../libs/util/stopwatch.hpp"

#include <thread>
#include <filesystem>

namespace fs = std::filesystem;


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

        fs::path video_filepath;

        ImGui::FileBrowser fb_video;
    };
}


/* internal */

namespace video_display
{
namespace internal
{
    using VLS = VideoLoadStatus;
    using VPS = VideoPlayStatus;


    static void reset_video(DisplayState& state)
    {
        state.load_status = VLS::NotLoaded;
        state.play_status = VPS::NotLoaded;
        vid::close_video(state.video);        
    }
    
    
    static bool load_video(DisplayState& state)
    {
        auto path = state.video_filepath;
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            return false;
        }

        reset_video(state);

        return vid::open_video(state.video, state.video_filepath.string().c_str());
    }


    static void load_video_async(DisplayState& state)
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
        constexpr f64 NANO = 1'000'000'000;

        auto target_ns = NANO / state.video.fps;

        state.play_status = VPS::Play;
        auto not_eof = true;

        Stopwatch sw;
        sw.start();
        while (state.play_status == VPS::Play && not_eof)
        {
            not_eof = vid::next_frame(state.video, state.display_frame);

            cap_framerate(sw, target_ns);
        }

        if (!not_eof)
        {
            reset_video(state);
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

        auto open_disabled = state.play_status == VPS::Play;
        auto load_disabled = state.load_status != VLS::NotLoaded;
        auto play_pause_disabled = state.load_status != VLS::Loaded;

        ImGui::Begin("Video");

        ImGui::Image(texture, dims);

        if (open_disabled) { ImGui::BeginDisabled(); }

        if (ImGui::Button("Open"))
        {
            state.fb_video.Open();
        }
        if (open_disabled) { ImGui::EndDisabled(); }

        ImGui::SameLine();
        ImGui::Text("path: %s", state.video_filepath.string().c_str());

        if (load_disabled) { ImGui::BeginDisabled(); }
        
        if (ImGui::Button("Load"))
        {
            internal::load_video_async(state);
        }
        if (load_disabled) { ImGui::EndDisabled(); }

        if (play_pause_disabled) { ImGui::BeginDisabled(); }

        if (state.load_status == VLS::InProgress)
        {
            ImGui::SameLine();
            ImGui::Text("Loading...");
        }
        else if (state.play_status == VPS::Pause)
        {
            ImGui::SameLine();
            if (ImGui::Button("Play"))
            {
                internal::play_video_async(state);
            }
        }
        else if (state.play_status == VPS::Play)
        {
            ImGui::SameLine();
            if (ImGui::Button("Pause"))
            {
                internal::pause_video(state);
            }
        }
        if (play_pause_disabled) { ImGui::EndDisabled(); }

        ImGui::Text("%3.1f fps", state.video.fps);
       
        ImGui::End();
        
        state.fb_video.Display();
        if (state.fb_video.HasSelected())
        {
            internal::reset_video(state);

            state.video_filepath = state.fb_video.GetSelected();
            state.fb_video.ClearSelected();
        }
    }


    inline void destroy(DisplayState& state)
    {
        internal::pause_video(state);
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

        auto& fb = state.fb_video;
        fb.SetTitle("Video Select");
        fb.SetTypeFilters({".mp4"});
        fb.SetDirectory(fs::path("/"));

        return true;
    }    
}