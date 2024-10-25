#pragma once

#include "../../../libs/imgui/imgui.h"
#include "../../../libs/image/image.hpp"


namespace display
{
    namespace img = image;


    class DisplayState
    {
    public:

        img::ImageView video_frame_view;
        ImTextureID video_frame_texture;

        img::Buffer32 pixel_buffer;
    };


    inline void destroy(DisplayState& state)
    {
        mb::destroy_buffer(state.pixel_buffer);
    }


    inline bool init(DisplayState& state)
    {


        return true;
    }
}