#ifndef SDL_ROTOZOOM_SDL2_H
#define SDL_ROTOZOOM_SDL2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <SDL2/SDL.h>

/* Rotate + uniform zoom. Returns a new ARGB8888 surface (caller frees). */
SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom, int smooth);

/* Rotate + non-uniform zoom (X/Y). Returns a new ARGB8888 surface (caller frees). */
SDL_Surface* rotozoomSurfaceXY(SDL_Surface* src, double angle, double zoomx, double zoomy, int smooth);

/* Zoom only (no rotation). Returns a new ARGB8888 surface (caller frees). */
SDL_Surface* zoomSurface(SDL_Surface* src, double zoomx, double zoomy, int smooth);

/* Integer shrink by factors (>=1). Returns a new ARGB8888 surface (caller frees). */
SDL_Surface* shrinkSurface(SDL_Surface* src, int factorx, int factory);

#ifdef __cplusplus
}
#endif

#endif /* SDL_ROTOZOOM_SDL2_H */
