
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_options.hpp"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"


namespace stb
{
    int qsnprintf(char *buf, int count, char const *fmt, ...)
    {
        //return stbsp_snprintf()

        int result;
        va_list va;
        va_start(va, fmt);

        result = stbsp_vsnprintf(buf, count, fmt, va);
        va_end(va);

        return result;
    }
}