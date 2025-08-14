#include "SDL_rotozoom.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Compute bounding box of a w×h rectangle scaled (sx,sy) and rotated by angle (deg). */
static void compute_rotated_size(int w, int h, double angle_deg, double sx, double sy, int* outW, int* outH) {
    const double a  = angle_deg * M_PI / 180.0;
    const double cw = cos(a);
    const double sw = sin(a);
    const double hw = (double)w * sx * 0.5;
    const double hh = (double)h * sy * 0.5;

    const double x[4] = { -hw,  hw,  hw, -hw };
    const double y[4] = { -hh, -hh,  hh,  hh };

    double minx =  1e300, maxx = -1e300;
    double miny =  1e300, maxy = -1e300;

    for (int i = 0; i < 4; ++i) {
        const double rx = x[i]*cw - y[i]*sw;
        const double ry = x[i]*sw + y[i]*cw;

        if (rx < minx) { minx = rx; }
        if (rx > maxx) { maxx = rx; }
        if (ry < miny) { miny = ry; }
        if (ry > maxy) { maxy = ry; }
    }

    int W = (int)ceil(maxx - minx);
    int H = (int)ceil(maxy - miny);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    *outW = W;
    *outH = H;
}

/* Create ARGB8888 target surface, cleared to transparent. */
static SDL_Surface* create_target_surface(int w, int h) {
    SDL_Surface* dst = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!dst) return NULL;
    SDL_FillRect(dst, NULL, SDL_MapRGBA(dst->format, 0, 0, 0, 0));
    return dst;
}

/* Render src -> dst using a software renderer (no window/GL needed). */
static int render_rotozoom(SDL_Surface* src, SDL_Surface* dst, double angle_deg, double sx, double sy, int smooth) {
    int ok = 0;

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(dst);
    if (!renderer) {
        return 0;
    }

    const char* prev_quality = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, smooth ? "1" : "0");

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, src);
    if (!tex) {
        /* cleanup happens after the scoped block below */
        goto cleanup;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    /* Put all objects that would be skipped by 'goto cleanup' into a scope
       so we don't "jump into" their lifetimes in C++. */
    {
        int dstW = (int)lround((double)src->w * sx);
        int dstH = (int)lround((double)src->h * sy);
        if (dstW < 1) dstW = 1;
        if (dstH < 1) dstH = 1;

        SDL_Rect dstRect;
        dstRect.w = dstW;
        dstRect.h = dstH;
        dstRect.x = (dst->w - dstW) / 2;
        dstRect.y = (dst->h - dstH) / 2;

        SDL_Point center = { dstRect.w / 2, dstRect.h / 2 };

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        if (SDL_RenderCopyEx(renderer, tex, NULL, &dstRect, angle_deg, &center, SDL_FLIP_NONE) == 0) {
            ok = 1;
        }

        SDL_RenderPresent(renderer);
    }

cleanup:
    if (tex) {
        SDL_DestroyTexture(tex);
    }

    /* Restore previous hint */
    if (prev_quality) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, prev_quality);
    } else {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    }

    SDL_DestroyRenderer(renderer);
    return ok;
}

SDL_Surface* rotozoomSurfaceXY(SDL_Surface* src, double angle, double zoomx, double zoomy, int smooth) {
    if (!src || zoomx <= 0.0 || zoomy <= 0.0) return NULL;

    int outW = 1, outH = 1;
    compute_rotated_size(src->w, src->h, angle, zoomx, zoomy, &outW, &outH);

    SDL_Surface* dst = create_target_surface(outW, outH);
    if (!dst) return NULL;

    if (!render_rotozoom(src, dst, angle, zoomx, zoomy, smooth)) {
        SDL_FreeSurface(dst);
        return NULL;
    }
    return dst;
}

SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom, int smooth) {
    return rotozoomSurfaceXY(src, angle, zoom, zoom, smooth);
}

SDL_Surface* zoomSurface(SDL_Surface* src, double zoomx, double zoomy, int smooth) {
    return rotozoomSurfaceXY(src, 0.0, zoomx, zoomy, smooth);
}

SDL_Surface* shrinkSurface(SDL_Surface* src, int factorx, int factory) {
    if (!src || factorx < 1 || factory < 1) return NULL;

    int outW = src->w / factorx;
    int outH = src->h / factory;
    if (outW < 1) outW = 1;
    if (outH < 1) outH = 1;

    SDL_Surface* dst = create_target_surface(outW, outH);
    if (!dst) return NULL;

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(dst);
    if (!renderer) {
        SDL_FreeSurface(dst);
        return NULL;
    }

    const char* prev_quality = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); /* linear looks nicer when shrinking */

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, src);
    if (!tex) {
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(dst);
        return NULL;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    SDL_Rect dstRect = { 0, 0, outW, outH };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    if (SDL_RenderCopy(renderer, tex, NULL, &dstRect) != 0) {
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(dst);
        return NULL;
    }

    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(tex);

    if (prev_quality) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, prev_quality);
    } else {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    }

    SDL_DestroyRenderer(renderer);
    return dst;
}
