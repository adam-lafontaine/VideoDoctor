#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/video/video.hpp"

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

        auto const load = [&]()
        {
            state.load_status = VLS::InProgress;
            auto ok = load_video(state);
            state.load_status = ok ? VLS::Loaded : VLS::Fail;
        };

        std::thread th(load);
        th.detach();
    }
}
}


namespace video_display
{
    void video_frame_window(DisplayState& state)
    {
        using VLS = VideoLoadStatus;

        auto view = state.display_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_frame_texture;

        auto btn_disabled = state.load_status != VLS::NotLoaded;

        auto red = img::to_pixel(100, 0, 0);
        auto green = img::to_pixel(0, 100, 0);
        auto blue = img::to_pixel(0, 0, 100);

        auto s = state.load_status;        

        auto color = s == VLS::NotLoaded ? blue : (s == VLS::Loaded ? green : red);

        img::fill(view, color);


        ImGui::Begin("Video");

        ImGui::Image(texture, dims);

        if (btn_disabled) { ImGui::BeginDisabled(); }

        if (ImGui::Button("Load"))
        {
            internal::load_video_async(state);
        }

        if (btn_disabled) { ImGui::EndDisabled(); }

        ImGui::SameLine();
        ImGui::Text("%d", (int)s);
       

        ImGui::End();
    }
}