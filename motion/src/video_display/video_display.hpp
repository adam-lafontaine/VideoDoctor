#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/imgui/imfilebrowser.hpp"
#include "../../../libs/video/video.hpp"
#include "../../../libs/video/motion.hpp"

#include <filesystem>

namespace fs = std::filesystem;


namespace video_display
{
    namespace img = image;
    namespace vid = video;

    // 4K video
    constexpr u32 SRC_VIDEO_WIDTH = 3840;
    constexpr u32 SRC_VIDEO_HEIGHT = 2160;

    // 1080p video
    constexpr u32 OUT_VIDEO_WIDTH = SRC_VIDEO_WIDTH / 2;
    constexpr u32 OUT_VIDEO_HEIGHT = SRC_VIDEO_HEIGHT / 2;

    // display/preview    
    constexpr u32 DISPLAY_FRAME_HEIGHT = 360;
    constexpr u32 DISPLAY_FRAME_WIDTH = DISPLAY_FRAME_HEIGHT * SRC_VIDEO_WIDTH / SRC_VIDEO_HEIGHT;

    // image processing
    constexpr u32 PROC_IMAGE_WIDTH = DISPLAY_FRAME_WIDTH / 2;
    constexpr u32 PROC_IMAGE_HEIGHT = DISPLAY_FRAME_HEIGHT / 2;

    // motion
    constexpr u32 MOTION_WIDTH = PROC_IMAGE_WIDTH / 2;
    constexpr u32 MOTION_HEIGHT = PROC_IMAGE_HEIGHT / 2;

    constexpr Point2Du32 SRC_CENTER_POS = { SRC_VIDEO_WIDTH / 2, SRC_VIDEO_HEIGHT / 2 };

    constexpr auto SRC_VIDEO_DIR = "/home/adam/Videos/src";
    constexpr auto OUT_VIDEO_PATH = "/home/adam/Repos/VideoDoctor/motion/build/out.mp4";


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


    class VideoMotionState
    {
    public:

        vid::VideoReader src_video;
        //vid::VideoWriter dst_video;
        vid::FrameRGBA out_frame;

        motion::GradientMotion gm;

        Point2Du32 out_position;
        f32 out_position_acc;
        
        Rect2Du32 scan_region;
        Rect2Du32 out_limit_region;
        Rect2Du32 out_region;

        img::ImageView out_view() { return out_frame.view; }
    };


    void destroy(VideoMotionState& vms)
    {
        motion::destroy(vms.gm);

        vid::close_video(vms.src_video);
        vid::destroy_frame(vms.out_frame);
        //vid::close_video(state.dst_video); //!
    }


    bool init(VideoMotionState& vms)
    {
        u32 process_w = PROC_IMAGE_WIDTH;
        u32 process_h = PROC_IMAGE_HEIGHT;

        if (!motion::create(vms.gm, process_w, process_h))
        {
            return false;
        }

        vms.gm.src_location = SRC_CENTER_POS;
        vms.out_position = SRC_CENTER_POS;

        vms.out_position_acc = 0.15f;

        vms.out_limit_region = img::make_rect(SRC_VIDEO_WIDTH, SRC_VIDEO_HEIGHT);
        vms.scan_region = img::make_rect(SRC_VIDEO_WIDTH, SRC_VIDEO_HEIGHT);
        //state.dst_region = SRC_CENTER_POS

        return true;
    }


    class DisplayState
    {
    public:

        VideoMotionState vms;

        VideoLoadStatus load_status = VideoLoadStatus::NotLoaded;
        VideoPlayStatus play_status = VideoPlayStatus::NotLoaded;

        img::ImageView vfx_view;

        img::ImageView display_src_view;
        img::ImageView display_gray_view;
        img::ImageView display_edges_view;
        img::ImageView display_motion_view;
        img::ImageView display_vfx_view;
        img::ImageView display_preview_view;        

        ImTextureID display_src_texture;
        ImTextureID display_gray_texture;
        ImTextureID display_edges_texture;
        ImTextureID display_motion_texture;
        ImTextureID display_vfx_texture;
        ImTextureID display_preview_texture;

        fs::path src_video_filepath;
        ImGui::FileBrowser fb_video;

        vid::FrameRGBA display_src_frame;
        vid::FrameRGBA display_preview_frame;
        img::Buffer32 buffer32;
        

        // ui properties
        bool motion_on;
        bool motion_x_on;
        bool motion_y_on;

        bool show_motion;
        bool show_scan_region;
        bool show_out_region;
    };
}


namespace video_display
{    
    inline void destroy(DisplayState& state)
    { 
        destroy(state.vms);

        vid::destroy_frame(state.display_src_frame);
        vid::destroy_frame(state.display_preview_frame);
        
        mb::destroy_buffer(state.buffer32);        
    }


    inline bool init(DisplayState& state)
    {
        if (!init(state.vms))
        {
            return false;
        }

        u32 display_w = DISPLAY_FRAME_WIDTH;
        u32 display_h = DISPLAY_FRAME_HEIGHT;        

        if (!vid::create_frame(state.display_src_frame, display_w, display_h))
        {
            return false;
        }

        if (!vid::create_frame(state.display_preview_frame, display_w, display_h))
        {
            return false;
        }

        state.display_src_view = state.display_src_frame.view;
        state.display_preview_view = state.display_preview_frame.view;

        auto n_pixels32 = display_w * display_h * 5;
        

        state.buffer32 = img::create_buffer32(n_pixels32, "buffer32");
        if (!state.buffer32.ok)
        {
            return false;
        }

        mb::zero_buffer(state.buffer32);        

        state.display_gray_view = img::make_view(display_w, display_h, state.buffer32);
        state.display_edges_view = img::make_view(display_w, display_h, state.buffer32);
        state.display_motion_view = img::make_view(display_w, display_h, state.buffer32);
        state.display_vfx_view = img::make_view(display_w, display_h, state.buffer32);
        state.vfx_view = img::make_view(display_w, display_h, state.buffer32);        

        auto& fb = state.fb_video;
        fb.SetTitle("Video Select");
        fb.SetTypeFilters({".mp4"});
        fb.SetDirectory(fs::path(SRC_VIDEO_DIR));

        state.motion_on = true;
        state.motion_x_on = true;
        state.motion_y_on = true;

        state.show_motion = true;
        state.show_scan_region = true;
        state.show_out_region = true;        

        return true;
    }    
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
        vid::close_video(state.vms.src_video);
    }

    
    void load_video_async(DisplayState& state);

    void process_video_async(DisplayState& state);

    void motion_detection_settings(DisplayState& state);

    void scan_region_settings(DisplayState& state);

    void display_region_settings(DisplayState& state);
}
}


/* windows */

namespace video_display
{
    void video_frame_window(DisplayState& state)
    {
        using VLS = VideoLoadStatus;
        using VPS = VideoPlayStatus;

        auto view = state.display_src_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_src_texture;

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
        ImGui::Text("file: %s", state.src_video_filepath.string().c_str());

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
                internal::process_video_async(state);
            }
        }
        
        if (play_pause_disabled) { ImGui::EndDisabled(); }

        auto src_w = state.vms.src_video.frame_width;
        auto src_h = state.vms.src_video.frame_height;
        auto src_fps = state.vms.src_video.fps;

        ImGui::Text("%ux%u %3.1f fps", src_w, src_h, src_fps);
       
        ImGui::End();
        
        state.fb_video.Display();
        if (state.fb_video.HasSelected())
        {
            internal::reset_video(state);

            state.src_video_filepath = state.fb_video.GetSelected();
            state.fb_video.ClearSelected();
        }
    }


    void video_preview_window(DisplayState& state)
    {
        auto view = state.vms.out_view();
        auto display_view = state.display_preview_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_preview_texture;        

        ImGui::Begin("Preview");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", view.width, view.height);

        ImGui::End();
    }


    void video_gray_window(DisplayState& state)
    {
        auto view = state.vms.gm.proc_gray_view;
        auto display_view = state.display_gray_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_gray_texture;        

        ImGui::Begin("Gray");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", view.width, view.height);

        ImGui::End();
    }


    void video_edges_window(DisplayState& state)
    {
        auto view = state.vms.gm.proc_edges_view;
        auto display_view = state.display_edges_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_edges_texture;

        ImGui::Begin("Edges");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", view.width, view.height);

        ImGui::End();
    }


    void video_motion_window(DisplayState& state)
    {
        auto view = state.vms.gm.proc_motion_view;
        auto display_view = state.display_motion_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_motion_texture;        

        ImGui::Begin("Motion");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", view.width, view.height);        

        ImGui::End();
    }

    
    void video_vfx_window(DisplayState& state)
    {
        auto src_w = state.vms.src_video.frame_width;
        auto src_h = state.vms.src_video.frame_height;
        auto display_view = state.display_vfx_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_vfx_texture;

        ImGui::Begin("VFX");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", src_w, src_h);

        internal::motion_detection_settings(state);
        internal::scan_region_settings(state);
        internal::display_region_settings(state);

        ImGui::End();
    }



}