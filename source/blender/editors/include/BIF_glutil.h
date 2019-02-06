/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file \ingroup editorui
 */

#ifndef __BIF_GLUTIL_H__
#define __BIF_GLUTIL_H__

struct rctf;
struct rcti;

struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ImBuf;
struct bContext;

/* A few functions defined here are being DEPRECATED for Blender 2.8
 *
 * Do not use them in new code, and you are encouraged to
 * convert existing code to draw without these.
 *
 * These will be deleted before we ship 2.8!
 * - merwin
 */

/**
 * Returns a float value as obtained by glGetFloatv.
 * The param must cause only one value to be gotten from GL.
 */
float glaGetOneFloat(int param);
int glaGetOneInt(int param);

/**
 * Functions like glRasterPos2i, except ensures that the resulting
 * raster position is valid. \a known_good_x and \a known_good_y
 * should be coordinates of a point known to be within the current
 * view frustum.
 * \attention This routine should be used when the distance of \a x
 * and \a y away from the known good point is small (ie. for small icons
 * and for bitmap characters), when drawing large+zoomed images it is
 * possible for overflow to occur, the glaDrawPixelsSafe routine should
 * be used instead.
 */
void glaRasterPosSafe2f(float x, float y, float known_good_x, float known_good_y);

typedef struct IMMDrawPixelsTexState {
	struct GPUShader *shader;
	unsigned int pos;
	unsigned int texco;
	bool do_shader_unbind;
} IMMDrawPixelsTexState;

/* To be used before calling immDrawPixelsTex
 * Default shader is GPU_SHADER_2D_IMAGE_COLOR
 * Returns a shader to be able to set uniforms */
IMMDrawPixelsTexState immDrawPixelsTexSetup(int builtin);

/**
 * immDrawPixelsTex - Functions like a limited glDrawPixels, but actually draws the
 * image using textures, which can be tremendously faster on low-end
 * cards, and also avoids problems with the raster position being
 * clipped when offscreen. Pixel unpacking parameters and
 * the glPixelZoom values are _not_ respected.
 *
 * \attention Use immDrawPixelsTexSetup before calling this function.
 *
 * \attention This routine makes many assumptions: the rect data
 * is expected to be in RGBA byte or float format, and the
 * modelview and projection matrices are assumed to define a
 * 1-to-1 mapping to screen space.
 */
void immDrawPixelsTex(IMMDrawPixelsTexState *state,
                      float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                      float xzoom, float yzoom, float color[4]);
void immDrawPixelsTex_clipping(IMMDrawPixelsTexState *state,
                               float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                               float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y,
                               float xzoom, float yzoom, float color[4]);
void immDrawPixelsTexScaled(IMMDrawPixelsTexState *state,
                            float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY,
                            float xzoom, float yzoom, float color[4]);
void immDrawPixelsTexScaled_clipping(IMMDrawPixelsTexState *state,
                                     float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY,
                                     float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y,
                                     float xzoom, float yzoom, float color[4]);

void set_inverted_drawing(int enable);
void setlinestyle(int nr);

/* own working polygon offset */
float bglPolygonOffsetCalc(const float winmat[16], float viewdist, float dist);
void bglPolygonOffset(float viewdist, float dist);

/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw imbuf on a screen, preferably using GLSL display transform */
void glaDrawImBuf_glsl(struct ImBuf *ibuf, float x, float y, int zoomfilter,
                       struct ColorManagedViewSettings *view_settings,
                       struct ColorManagedDisplaySettings *display_settings,
                       float zoom_x, float zoom_y);
void glaDrawImBuf_glsl_clipping(struct ImBuf *ibuf, float x, float y, int zoomfilter,
                                struct ColorManagedViewSettings *view_settings,
                                struct ColorManagedDisplaySettings *display_settings,
                                float clip_min_x, float clip_min_y,
                                float clip_max_x, float clip_max_y,
                                float zoom_x, float zoom_y);


/* Draw imbuf on a screen, preferably using GLSL display transform */
void glaDrawImBuf_glsl_ctx(const struct bContext *C, struct ImBuf *ibuf, float x, float y, int zoomfilter,
                           float zoom_x, float zoom_y);
void glaDrawImBuf_glsl_ctx_clipping(const struct bContext *C,
                                    struct ImBuf *ibuf,
                                    float x, float y,
                                    int zoomfilter,
                                    float clip_min_x, float clip_min_y,
                                    float clip_max_x, float clip_max_y,
                                    float zoom_x, float zoom_y);

void immDrawBorderCorners(unsigned int pos, const struct rcti *border, float zoomx, float zoomy);

#endif /* __BIF_GLUTIL_H__ */
