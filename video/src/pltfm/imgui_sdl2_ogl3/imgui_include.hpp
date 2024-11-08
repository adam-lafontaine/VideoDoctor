#pragma once

#include "imgui_options.hpp"

// v1.91.5
#include "../../../../libs/imgui/backends/imgui_impl_sdl2.h"
#include "../../../../libs/imgui/backends/imgui_impl_opengl3.h"

#include <SDL2/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif


namespace ui
{
    class UIState
    {
    public:

        const char* window_title = 0;
        int window_width = 0;
        int window_height = 0;

#ifdef SHOW_IMGUI_DEMO

        bool show_demo_window = true;
        bool show_another_window = false;

#endif

        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        SDL_Window* window = 0;
        SDL_GLContext gl_context = 0;
    };


    bool init(UIState& state)
    {
        auto sdl_flags = SDL_INIT_VIDEO;
        sdl_flags |= SDL_INIT_TIMER;
        //sdl_flags |= SDL_INIT_GAMECONTROLLER;

        if (SDL_Init(sdl_flags) != 0)
        {
            return false;
        }

        // Create window with graphics context
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        SDL_Window* window = 
            SDL_CreateWindow(
                state.window_title, 
                SDL_WINDOWPOS_CENTERED, 
                SDL_WINDOWPOS_CENTERED, 
                state.window_width, 
                state.window_height, 
                window_flags);
        
        if (!window)
        {
            return false;
        }

        SDL_GLContext gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1); // Enable vsync

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
        //io.ConfigViewportsNoAutoMerge = true;
        //io.ConfigViewportsNoTaskBarIcon = true;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100
        static constexpr const char* glsl_version = "#version 100";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
        // GL 3.2 Core + GLSL 150
        static constexpr const char* glsl_version = "#version 150";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
        // GL 3.0 + GLSL 130
        static constexpr const char* glsl_version = "#version 130";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);

        // Load Fonts
        // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
        // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
        // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
        // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
        // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
        // - Read 'docs/FONTS.md' for more instructions and details.
        // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
        // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
        //io.Fonts->AddFontDefault();
        //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
        //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
        //IM_ASSERT(font != nullptr);

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        constexpr auto TEXT_WHITE = ImVec4(0.7f, 0.7f, 0.7f, 1);

        auto& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Text] = TEXT_WHITE;
        style.TabRounding = 0.0f;

        state.window = window;
        state.gl_context = gl_context;

        return true;
    }


    void new_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Rendering
        ImGui::DockSpaceOverViewport();
    }
    
    
    void render(UIState& state)
    {
        auto& io = ImGui::GetIO();
        auto& clear_color = state.clear_color;

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(state.window);
    }


    void close(UIState& state)
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        SDL_GL_DeleteContext(state.gl_context);
        SDL_DestroyWindow(state.window);
        SDL_Quit();
    }

}


/* demo window */

namespace ui
{

#ifdef SHOW_IMGUI_DEMO

    static inline void show_imgui_demo(UIState& state)
    {
        auto& io = ImGui::GetIO();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (state.show_demo_window)
            ImGui::ShowDemoWindow(&state.show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &state.show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &state.show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&state.clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (state.show_another_window)
        {
            ImGui::Begin("Another Window", &state.show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                state.show_another_window = false;
            ImGui::End();
        }
    }

#endif

}


/* opengl texture */

namespace ogl
{

    // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples

    struct TextureId { int value = -1; };

    class Texture
    {
    public:
        GLuint gl_ref;
        TextureId id;

        int image_width = 0;
        int image_height = 0;
        void* image_data = 0;
    };


    template <size_t N>
    class TextureList
    {
    public:
        static constexpr size_t count = N;

        Texture data[count] = { 0 };

        GLuint gl_ref_data[count] = { 0 };

        Texture& get_ogl_texture(TextureId id) { return data[id.value]; }

        ImTextureID get_imgui_texture(TextureId id) { return (ImTextureID)(intptr_t)get_ogl_texture(id).gl_ref; }
    };


    template <size_t N>
    static inline TextureList<N> create_textures()
    {
        TextureList<N> textures{};

        glGenTextures((GLsizei)N, textures.gl_ref_data);

        for (int i = 0; i < textures.count; i++)
        {
            auto& texture = textures.data[i];
            texture.id.value = i;
            texture.gl_ref = textures.gl_ref_data[i];
        }
        
        return textures;
    }


    template <typename P>
    static inline void init_texture(P* data, int width, int height, Texture& texture)
    {
        static_assert(sizeof(P) == 4);

        assert(data);

        texture.image_data = (void*)data;
        texture.image_width = width;
        texture.image_height = height;

        glActiveTexture(GL_TEXTURE0 + texture.id.value);
        glBindTexture(GL_TEXTURE_2D, texture.gl_ref);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
    }
    

    static inline void render_texture(Texture const& texture)
    {
        if (!texture.image_data)
        {
            return;
        }

        auto texture_id = texture.id.value;

        assert(texture_id >= 0);

        glActiveTexture(GL_TEXTURE0 + texture_id);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, 
            (GLsizei)texture.image_width, 
            (GLsizei)texture.image_height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, 
            (GLvoid*)texture.image_data);
    }
}