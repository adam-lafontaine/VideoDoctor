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
    constexpr u32 WIDTH_4K = 3840;
    constexpr u32 HEIGHT_4K = 2160;

    // 1080p video
    constexpr u32 WIDTH_1080P = WIDTH_4K / 2;
    constexpr u32 HEIGHT_1080P = HEIGHT_4K / 2;

    // display/preview 
    constexpr u32 DISPLAY_FRAME_HEIGHT = 360;
    constexpr u32 DISPLAY_FRAME_WIDTH = DISPLAY_FRAME_HEIGHT * WIDTH_4K / HEIGHT_4K;
    
    // image processing
    constexpr u32 PROCESS_IMAGE_WIDTH = DISPLAY_FRAME_WIDTH / 2;
    constexpr u32 PROCESS_IMAGE_HEIGHT = DISPLAY_FRAME_HEIGHT / 2;

    constexpr auto SRC_VIDEO_DIR = "/home/adam/Videos/src";
    constexpr auto OUT_VIDEO_TEMP_PATH = "./vdtemp.mp4";
    constexpr auto OUT_VIDEO_DIR = "/home/adam/Repos/VideoDoctor/video/build/";


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
        Generate,
        Pause
    };


    class VideoMotionState
    {
    public:

        vid::VideoReader src_video;

        motion::GradientMotion gm;

        Point2Du32 out_position;
        f32 out_position_acc;
        
        Rect2Du32 scan_region;
        Rect2Du32 out_limit_region;
        Rect2Du32 out_region;
    };


    void destroy_vms(VideoMotionState& vms)
    {
        motion::destroy(vms.gm);

        vid::close_video(vms.src_video);
    }


    class DisplayState
    {
    public:

        VideoMotionState vms;

        vid::VideoWriter dst_video;

        VideoLoadStatus load_status = VideoLoadStatus::NotLoaded;
        VideoPlayStatus play_status = VideoPlayStatus::NotLoaded;

        img::Image out_image;

        img::ImageView vfx_view;

        img::ImageView display_src_view;
        ImTextureID display_src_texture;

        img::ImageView display_vfx_view;
        ImTextureID display_vfx_texture;

        img::ImageView display_preview_view;
        ImTextureID display_preview_texture;
        
        img::Buffer32 display_buffer32;

        fs::path src_video_filepath;
        ImGui::FileBrowser fb_video;

        u32 out_width;
        u32 out_height;
        img::SubView preview_dst;

        img::ImageView out_view() { return img::make_view(out_image); }

        Vec2Du32 src_dims() { return { vms.src_video.frame_width, vms.src_video.frame_height }; }
        f32 src_fps() { return vms.src_video.fps; }
                
        u32 display_scale() { auto w = src_dims().x; return w ? w / display_src_view.width : 0; }

        bool motion_on;

        bool show_motion;
        bool show_scan_region;
        bool show_out_region;

        bool vfx_running;
    };
}


/* internal */

namespace video_display
{
namespace internal
{
    using VLS = VideoLoadStatus;
    using VPS = VideoPlayStatus;


    static void reset_video_status(DisplayState& state)
    {
        state.load_status = VLS::NotLoaded;
        state.play_status = VPS::NotLoaded;
    }

    
    void load_video_async(DisplayState& state);

    void reload_video_async(DisplayState& state);

    void play_video_async(DisplayState& state);

    void generate_video_async(DisplayState& state);

    void pause_video(DisplayState& state);

    void stop_video(DisplayState& state);

    void motion_detection_settings(DisplayState& state);

    void scan_region_settings(DisplayState& state);

    void display_region_settings(DisplayState& state);

    void start_vfx(DisplayState& state);
}
}


namespace video_display
{    
    inline void destroy(DisplayState& state)
    { 
        state.vfx_running = false;
        destroy_vms(state.vms);
        
        vid::close_video(state.dst_video); //!
        
        mb::destroy_buffer(state.display_buffer32);
        img::destroy_image(state.out_image);
    }


    inline bool init(DisplayState& state)
    {
        u32 display_w = DISPLAY_FRAME_WIDTH;
        u32 display_h = DISPLAY_FRAME_HEIGHT;

        auto n32 = 4;
        auto n_pixels32 = display_w * display_h * n32;

        state.display_buffer32 = img::create_buffer32(n_pixels32, "buffer32");
        if (!state.display_buffer32.ok)
        {
            return false;
        }

        mb::zero_buffer(state.display_buffer32);

        auto const make_display_view = [&](){ return img::make_view(display_w, display_h, state.display_buffer32); };

        state.vfx_view = make_display_view();
        state.display_vfx_view = make_display_view();
        state.display_preview_view = make_display_view();
        state.display_src_view = make_display_view();

        auto& fb = state.fb_video;
        fb.SetTitle("Video Select");
        fb.SetTypeFilters({".mp4"});
        fb.SetDirectory(fs::path(SRC_VIDEO_DIR));

        state.motion_on = true;

        state.show_motion = true;
        state.show_scan_region = true;
        state.show_out_region = true;

        state.vfx_running = false;

        internal::start_vfx(state);

        return true;
    }    
}


/* windows */

namespace video_display
{
    void video_frame_window(DisplayState& state)
    {
        using VLS = VideoLoadStatus;
        using VPS = VideoPlayStatus;

        auto view = state.display_src_view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_src_texture;

        auto open_disabled = state.play_status == VPS::Play;
        auto load_disabled = state.play_status == VPS::Play;
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

        if (state.load_status == VLS::NotLoaded)
        {
            if (ImGui::Button("Load"))
            {
                internal::load_video_async(state);
            }
        }
        else if (state.load_status == VLS::Loaded)
        {
            if (ImGui::Button("Reload"))
            {
                internal::reload_video_async(state);
            }
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

            ImGui::SameLine();            
            if (ImGui::Button("Generate"))
            {
                internal::generate_video_async(state);
            }
        }
        else if (state.play_status == VPS::Play || state.play_status == VPS::Generate)
        {
            ImGui::SameLine();
            if (ImGui::Button("Pause"))
            {
                internal::pause_video(state);
            }
        }
        
        if (play_pause_disabled) { ImGui::EndDisabled(); }
        
        auto src_dims = state.src_dims();
        auto src_w = src_dims.x;
        auto src_h = src_dims.y;
        auto src_fps = state.src_fps();        

        ImGui::Text("%ux%u %3.1f fps", src_w, src_h, src_fps);
       
        ImGui::End();
        
        state.fb_video.Display();
        if (state.fb_video.HasSelected())
        {
            internal::reset_video_status(state);

            state.src_video_filepath = state.fb_video.GetSelected();
            state.fb_video.ClearSelected();
        }
    }


    void video_preview_window(DisplayState& state)
    {
        auto view = state.out_view();
        auto display_view = state.display_preview_view;
        auto dims = ImVec2(display_view.width, display_view.height);
        auto texture = state.display_preview_texture;        

        ImGui::Begin("Preview");

        ImGui::Image(texture, dims);

        ImGui::Text("%ux%u", view.width, view.height);

        ImGui::End();
    }

    
    void video_vfx_window(DisplayState& state)
    {
        auto src_dims = state.src_dims();
        auto src_w = src_dims.x;
        auto src_h = src_dims.y;
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