
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STBI_NO_JPEG

//#define STBI_NO_PNG
//#define STBI_NO_BMP
//#define STBI_NO_SIMD
//#define STBI_NEON


//#define STBI_MALLOC stb_malloc_void
//#define STBI_REALLOC stb_realloc_void
//#define STBI_FREE stb_free_void


//#define IMAGE_READ
//#define IMAGE_WRITE
#define IMAGE_RESIZE


#ifdef IMAGE_READ
#include "stb_image.h"
#endif


#ifdef IMAGE_WRITE
#include "stb_image_write.h"
#endif


#ifdef IMAGE_RESIZE
#include "stb_image_resize2.h"
#endif
