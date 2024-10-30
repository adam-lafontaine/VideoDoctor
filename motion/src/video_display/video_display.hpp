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

    // 4K video
    constexpr u32 SRC_VIDEO_WIDTH = 3840;
    constexpr u32 SRC_VIDEO_HEIGHT = 2160;

    // 1080p video
    constexpr u32 DST_VIDEO_WIDTH = SRC_VIDEO_WIDTH / 2;
    constexpr u32 DST_VIDEO_HEIGHT = SRC_VIDEO_HEIGHT / 2;

    // display/preview    
    constexpr u32 DISPLAY_FRAME_HEIGHT = 360;
    constexpr u32 DISPLAY_FRAME_WIDTH = DISPLAY_FRAME_HEIGHT * SRC_VIDEO_WIDTH / SRC_VIDEO_HEIGHT;

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


    class DisplayState
    {
    public:

        vid::VideoReader src_video;
        vid::VideoWriter dst_video;
        
        img::GrayView edges_view;

        vid::FrameRGBA gray_frame;
        vid::FrameRGBA edges_frame;
        
        vid::FrameRGBA display_src_frame;
        vid::FrameRGBA display_gray_frame;
        vid::FrameRGBA display_edges_frame;
        vid::FrameRGBA display_preview_frame;

        ImTextureID display_src_texture;
        ImTextureID display_gray_texture;
        ImTextureID display_edges_texture;        
        ImTextureID display_preview_texture;

        VideoLoadStatus load_status = VideoLoadStatus::NotLoaded;
        VideoPlayStatus play_status = VideoPlayStatus::NotLoaded;

        fs::path src_video_filepath;

        ImGui::FileBrowser fb_video;



        img::Buffer8 u8_buffer;
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
        vid::close_video(state.src_video);        
    }
    
    
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


    static void fill_all(DisplayState& state, img::GrayView const& src, img::ImageView const& dst)
    {
        img::fill(state.display_gray_frame.view, img::to_pixel(100, 0, 0));
        img::fill(state.display_edges_frame.view, img::to_pixel(0, 100, 0));
        img::fill(dst, img::to_pixel(0, 0, 100));
    }

    
    static void process_video(DisplayState& state)
    {
        vid::FrameList src_frames = { state.display_src_frame };
        vid::FrameList dst_frames = { state.display_preview_frame };

        auto const proc = [&](auto const& vs, auto const& vd)
        {
            fill_all(state, vs, vd);
        };

        auto& src = state.src_video;
        auto& dst = state.dst_video;

        vid::process_video(src, dst, proc, src_frames, dst_frames);
        reset_video(state);
        vid::save_and_close_video(dst);
    }


    static void process_video_async(DisplayState& state)
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


    static void pause_video(DisplayState& state)
    {
        using VPS = VideoPlayStatus;

        state.play_status = VPS::Pause;
    }
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

        auto src_w = state.src_video.frame_width;
        auto src_h = state.src_video.frame_height;
        auto src_fps = state.src_video.fps;

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
        auto view = state.display_preview_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_preview_texture;

        ImGui::Begin("Preview");

        ImGui::Image(texture, dims);

        auto w = state.dst_video.frame_width;
        auto h = state.dst_video.frame_height;

        ImGui::Text("%ux%u", w, h);

        ImGui::End();
    }


    void video_gray_window(DisplayState& state)
    {
        auto view = state.display_gray_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_gray_texture;

        ImGui::Begin("Gray");

        ImGui::Image(texture, dims);

        auto w = state.dst_video.frame_width;
        auto h = state.dst_video.frame_height;

        ImGui::Text("%ux%u", w, h);

        ImGui::End();
    }


    void video_edges_window(DisplayState& state)
    {
        auto view = state.display_edges_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.display_edges_texture;

        ImGui::Begin("Edges");

        ImGui::Image(texture, dims);

        auto w = state.dst_video.frame_width;
        auto h = state.dst_video.frame_height;

        ImGui::Text("%ux%u", w, h);

        ImGui::End();
    }
}


namespace video_display
{    
    inline void destroy(DisplayState& state)
    {
        internal::pause_video(state);
        //vid::destroy_frame(state.src_video_frame);
        vid::destroy_frame(state.display_src_frame);
        //vid::destroy_frame(state.filter_frame);
        vid::destroy_frame(state.display_preview_frame);
        vid::close_video(state.src_video);
        vid::close_video(state.dst_video); //!
        mb::destroy_buffer(state.u8_buffer);
    }


    inline bool init(DisplayState& state)
    {
        u32 display_w = DISPLAY_FRAME_WIDTH;
        u32 display_h = DISPLAY_FRAME_HEIGHT;

        u32 src_w = SRC_VIDEO_WIDTH;
        u32 src_h = SRC_VIDEO_HEIGHT;

        auto n_u8_pixels = 2 * src_w * src_h;

        state.u8_buffer = img::create_buffer8(n_u8_pixels, "u8_buffer");
        if (!state.u8_buffer.ok)
        {
            return false;
        }

        if (!vid::create_frame(state.display_src_frame, display_w, display_h))
        {
            return false;
        }

        if (!vid::create_frame(state.display_preview_frame, display_w, display_h))
        {
            return false;
        }

        if (!vid::create_frame(state.display_gray_frame, display_w, display_h))
        {
            return false;
        }

        if (!vid::create_frame(state.display_edges_frame, display_w, display_h))
        {
            return false;
        }

        auto& fb = state.fb_video;
        fb.SetTitle("Video Select");
        fb.SetTypeFilters({".mp4"});
        fb.SetDirectory(fs::path(SRC_VIDEO_DIR));

        return true;
    }    
}