#include "../imgui_sdl2_ogl3/imgui_include.hpp"
#include "../../video_display/video_display.hpp"

namespace vd = video_display;


enum class RunState : int
{
    Begin,
    Run,
    End
};


namespace
{
    ui::UIState ui_state{};
    RunState run_state = RunState::Begin;

    vd::DisplayState vd_state;

    constexpr u32 N_TEXTURES = 3;
    ogl::TextureList<N_TEXTURES> textures;

    constexpr ogl::TextureId video_src_texture_id     = { 0 };
    constexpr ogl::TextureId video_preview_texture_id = { 1 };
    constexpr ogl::TextureId video_vfx_texture_id     = { 2 };
}


static void end_program()
{
    run_state = RunState::End;
}


static bool is_running()
{
    return run_state != RunState::End;
}


static void init_texture(image::ImageView const& src, ogl::TextureId ogl_id, ImTextureID& im_id)
{
    auto& dst = textures.get_ogl_texture(ogl_id);

    auto data = src.matrix_data_;
    auto w = (int)src.width;
    auto h = (int)src.height;

    ogl::init_texture(data, w, h, dst);

    im_id = textures.get_imgui_texture(ogl_id);
}


static void init_textures()
{
    textures = ogl::create_textures<N_TEXTURES>();

    init_texture(vd_state.display_src_view,     video_src_texture_id,     vd_state.display_src_texture);
    init_texture(vd_state.display_preview_view, video_preview_texture_id, vd_state.display_preview_texture);
    init_texture(vd_state.display_vfx_view,     video_vfx_texture_id,     vd_state.display_vfx_texture);
}


static void render_textures()
{
    ogl::render_texture(textures.get_ogl_texture(video_src_texture_id));
    ogl::render_texture(textures.get_ogl_texture(video_preview_texture_id));
    ogl::render_texture(textures.get_ogl_texture(video_vfx_texture_id));
}


static void handle_window_event(SDL_Event const& event, SDL_Window* window)
{
    switch (event.type)
    {
    case SDL_QUIT:
        end_program();
        break;

    case SDL_WINDOWEVENT:
    {
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        //case SDL_WINDOWEVENT_RESIZED:
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            glViewport(0, 0, w, h);
            break;

        case SDL_WINDOWEVENT_CLOSE:
            end_program();
            break;
        
        default:
            break;
        }
    } break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        auto key_code = event.key.keysym.sym;
        switch (key_code)
        {
            
    #ifndef NDEBUG
        case SDLK_ESCAPE:
            //sdl::print_message("ESC");
            end_program();
            break;
    #endif

        default:
            break;
        }

    } break;
    
    default:
        break;
    }
}


static void process_user_input()
{
    SDL_Event event;

    // Poll and handle events (inputs, window resize, etc.)
    while (SDL_PollEvent(&event))
    {
        handle_window_event(event, ui_state.window);
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}


static void render_imgui_frame()
{
    ui::new_frame();

#ifdef SHOW_IMGUI_DEMO
    ui::show_imgui_demo(ui_state);
#endif

    vd::video_frame_window(vd_state);
    vd::video_preview_window(vd_state);
    vd::video_vfx_window(vd_state);

    ui::render(ui_state);
}


static bool main_init()
{
    ui_state.window_title = "Motion Detect";
    ui_state.window_width = 1350;
    ui_state.window_height = 950;

    if (!ui::init(ui_state))
    {
        return false;
    }

    if (!vd::init(vd_state))
    {
        return false;
    }

    init_textures();

    return true;
}


static void main_close()
{
    ui::close(ui_state);
    vd::destroy(vd_state);
}


static void main_loop()
{
    while(is_running())
    {
        process_user_input();

        render_textures();

        render_imgui_frame();
    }
}


int main()
{
    if (!main_init())
    {
        return 1;
    }

    run_state = RunState::Run;

    main_loop();

    main_close();

    return 0;
}

#include "main_o.cpp"