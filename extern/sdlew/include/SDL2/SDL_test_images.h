
#ifndef _SDL_test_images_h
#define _SDL_test_images_h

#include "SDL.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDLTest_SurfaceImage_s {
  int width;
  int height;
  unsigned int bytes_per_pixel;
  const char *pixel_data;
} SDLTest_SurfaceImage_t;

SDL_Surface *SDLTest_ImageBlit();
SDL_Surface *SDLTest_ImageBlitColor();
SDL_Surface *SDLTest_ImageBlitAlpha();
SDL_Surface *SDLTest_ImageBlitBlendAdd();
SDL_Surface *SDLTest_ImageBlitBlend();
SDL_Surface *SDLTest_ImageBlitBlendMod();
SDL_Surface *SDLTest_ImageBlitBlendNone();
SDL_Surface *SDLTest_ImageBlitBlendAll();
SDL_Surface *SDLTest_ImageFace();
SDL_Surface *SDLTest_ImagePrimitives();
SDL_Surface *SDLTest_ImagePrimitivesBlend();

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

