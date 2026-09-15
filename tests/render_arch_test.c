/* Headless SDL regression for 0081aa0's 16-bit surface/cursor path. */
#include <SDL2/SDL.h>
#include <stdint.h>

int main(void)
{
    if (SDL_Init(0) != 0)
        return 1;

    SDL_Surface *src = SDL_CreateRGBSurfaceWithFormat(0, 641, 479, 16,
                                                       SDL_PIXELFORMAT_RGB555);
    SDL_Surface *cursor = SDL_CreateRGBSurfaceWithFormat(0, 2, 1, 16,
                                                          SDL_PIXELFORMAT_RGBA5551);
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, 1282, 958, 16,
                                                       SDL_PIXELFORMAT_RGBA5551);
    if (!src || !cursor || !dst)
        return SDL_Quit(), 2;

    ((uint16_t *)cursor->pixels)[0] = 0x1234;
    ((uint16_t *)cursor->pixels)[1] = 0x7c00;
    for (int i = 0; i < 2; ++i)
        ((uint16_t *)cursor->pixels)[i] = (uint16_t)((((uint16_t *)cursor->pixels)[i] << 1) | 1);
    if (((uint16_t *)cursor->pixels)[0] != (uint16_t)((0x1234u << 1) | 1) ||
        ((uint16_t *)cursor->pixels)[1] != (uint16_t)((0x7c00u << 1) | 1))
        return SDL_FreeSurface(src), SDL_FreeSurface(cursor), SDL_FreeSurface(dst), SDL_Quit(), 3;

    SDL_Rect d = {0, 0, dst->w, dst->h};
    if (SDL_BlitScaled(src, NULL, dst, &d) != 0 || dst->w != 1282 || dst->h != 958)
        return SDL_FreeSurface(src), SDL_FreeSurface(cursor), SDL_FreeSurface(dst), SDL_Quit(), 4;

    SDL_FreeSurface(src);
    SDL_FreeSurface(cursor);
    SDL_FreeSurface(dst);
    SDL_Quit();
    return 0;
}
