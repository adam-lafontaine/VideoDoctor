#pragma once

// Avoid redefinition errors when using with imgui

#ifdef STB_SPRINTF_IMPLEMENTATION
#undef STB_SPRINTF_IMPLEMENTATION
#endif

namespace stb
{
    int qsnprintf(char *buf, int count, char const *fmt, ...);
}