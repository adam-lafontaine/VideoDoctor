#include "imgui_options.hpp"

// use a known ini configuration below
// prevents the user from having to place windows when first using the application
// imgui/imgui.cpp modifications at ImGui::UpdateSettings and ImGui::SaveIniSettingsToDisk
//#define USE_INI_STR

// prevent imgui from saving imgui.ini
// used with USE_INI_STR
//#define DO_NOT_SAVE_INI

// create a script for converting imgui.ini to a string to paste below
//#define SAVE_INI_STR_SCRIPT


#ifdef USE_INI_STR

// save custom ini to disk
// makes for easier modifcation during development

namespace ini_str
{
    constexpr auto INI_STR = ""


/* end INI_STR */; 

} // ini_str

#endif // USE_INI_STR


#ifdef SAVE_INI_STR_SCRIPT

namespace ini_str
{
    // add a .bat (windows) file for converting imgui.ini to a string to paste above
    void ini_to_str_script();
}

#endif // SAVE_INI_STR_SCRIPT


#define IMGUI_IMPLEMENTATION
#define IMGUI_USE_STB_SPRINTF

#include "../../../../libs/imgui/misc/single_file/imgui_single_file.h"

#include "../../../../libs/imgui/backends/imgui_impl_sdl2.cpp"
#include "../../../../libs/imgui/backends/imgui_impl_opengl3.cpp"


#ifdef SAVE_INI_STR_SCRIPT

#include <fstream>

namespace ini_str
{
    // add a .bat (windows) file for converting imgui.ini to a string to paste above
    inline void ini_to_str_script()
    {
        constexpr auto str = 

        "#!/bin/bash\n"
        "\n"
        "# chmod +x convert_ini.sh"
        "\n"
        "# Define the input and output files\n"
        "inputFile=\"imgui.ini\"\n"
        "outputFile=\"ini_str.txt\"\n"
        "\n"
        "# Clear the output file\n"
        "> \"$outputFile\"\n"
        "\n"
        "# Write text to the file\n"
        "while IFS= read -r line\n"
        "do\n"
        "echo \"\\\"$line\\\\n\\\"\" >> \"$outputFile\"\n"
        "done < \"$inputFile\"\n"
        "\n"
        "# Confirm the write operation\n"
        "if [ $? -eq 0 ]; then\n"
        "    echo \"Text written to $outputFile successfully.\"\n"
        "else\n"
        "    echo \"Failed to write to $outputFile.\"\n"
        "fi\n"
        /* end string */;

        std::ofstream file("convert_ini.sh");
        if (file.is_open())
        {
            file << str;
            file.close();
        }

    }
}

#endif