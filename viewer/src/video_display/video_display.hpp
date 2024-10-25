#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/video/video.hpp"


namespace video_display
{
    namespace img = image;
    namespace vid = video;


    class DisplayState
    {
    public:

        vid::FrameRGBA video_frame;
        ImTextureID video_frame_texture;
    };


    inline void destroy(DisplayState& state)
    {
        vid::destroy_frame(state.video_frame);
    }


    inline bool init(DisplayState& state)
    {
        u32 w = 640;
        u32 h = 360;
        
        if (!vid::create_frame(state.video_frame, w, h))
        {
            return false;
        }

        return true;
    }


    void video_frame_window(DisplayState& state)
    {
        auto view = state.video_frame.view;
        auto dims = ImVec2(view.width, view.height);
        auto texture = state.video_frame_texture;

        img::fill(view, img::to_pixel(0,0,100));


        ImGui::Begin("Video");

        ImGui::Image(texture, dims);
       

        ImGui::End();
    }
}