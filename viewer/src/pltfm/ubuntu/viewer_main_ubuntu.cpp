#include "imgui_include.hpp"
#include "../../../../libs/util/types.hpp"



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

    constexpr u32 N_TEXTURES = 1;
    ogl::TextureList<N_TEXTURES> textures;
}


static void end_program()
{
    run_state = RunState::End;
}


static bool is_running()
{
    return run_state != RunState::End;
}


int main()
{

}