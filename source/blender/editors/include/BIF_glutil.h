/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation 2002-2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BIF_glutil.h
 *  \ingroup editorui
 */

#ifndef __BIF_GLUTIL_H__
#define __BIF_GLUTIL_H__

struct rcti;
struct rctf;

struct ImBuf;
struct bContext;
struct ColorManagedViewSettings;
struct ColorManagedDisplaySettings;

/* Several functions defined here are being DEPRECATED for Blender 2.8
 *
 * Do not use them in new code, and you are encouraged to
 * convert existing code to draw without these.
 *
 * These will be deleted before we ship 2.8!
 * - merwin
 */

void fdrawline(float x1, float y1, float x2, float y2); /* DEPRECATED */

void fdrawcheckerboard(float x1, float y1, float x2, float y2); /* DEPRECATED */

/* OpenGL stipple defines */
extern const unsigned char stipple_halftone[128];
extern const unsigned char stipple_quarttone[128];
extern const unsigned char stipple_diag_stripes_pos[128];
extern const unsigned char stipple_diag_stripes_neg[128];
extern const unsigned char stipple_checker_8px[128];

/**
 * Draw a circle outline with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param pos The vertex attribute number for position.
 * \param x Horizontal center.
 * \param y Vertical center.
 * \param radius The circle's radius.
 * \param nsegments The number of segments to use in drawing (more = smoother).
 */
void imm_draw_lined_circle(unsigned pos, float x, float y, float radius, int nsegments);

/* use this version when VertexFormat has a vec3 position */
void imm_draw_lined_circle_3D(unsigned pos, float x, float y, float radius, int nsegments);

/**
 * Draw a filled circle with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param pos The vertex attribute number for position.
 * \param x Horizontal center.
 * \param y Vertical center.
 * \param radius The circle's radius.
 * \param nsegments The number of segments to use in drawing (more = smoother).
 */
void imm_draw_filled_circle(unsigned pos, float x, float y, float radius, int nsegments);

/**
* Draw a lined box.
*
* \param pos The vertex attribute number for position.
* \param x1 left.
* \param y1 bottom.
* \param x2 right.
* \param y2 top.
*/
void imm_draw_line_box(unsigned pos, float x1, float y1, float x2, float y2);

/* use this version when VertexFormat has a vec3 position */
void imm_draw_line_box_3D(unsigned pos, float x1, float y1, float x2, float y2);

/* Draw a standard checkerboard to indicate transparent backgrounds */
void imm_draw_checker_box(float x1, float y1, float x2, float y2);

/**
* Pack color into 3 bytes
*
* \param x color.
*/
void imm_cpack(unsigned int x);

/**
* Draw a cylinder. Replacement for gluCylinder.
* _warning_ : Slow, better use it only if you no other choices.
*
* \param pos The vertex attribute number for position.
* \param nor The vertex attribute number for normal.
* \param base Specifies the radius of the cylinder at z = 0.
* \param top Specifies the radius of the cylinder at z = height.
* \param height Specifies the height of the cylinder.
* \param slices Specifies the number of subdivisions around the z axis.
* \param stacks Specifies the number of subdivisions along the z axis.
*/
void imm_cylinder(unsigned int pos, unsigned int nor, float base, float top, float height, int slices, int stacks);

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

#if 0 /* Obsolete / unused */
/**
 * Functions like a limited glDrawPixels, except ensures that
 * the image is displayed onscreen even if the \a x and \a y
 * coordinates for would be clipped. The routine respects the
 * glPixelZoom values, pixel unpacking parameters are _not_
 * respected.
 *
 * \attention This routine makes many assumptions: the rect data
 * is expected to be in RGBA unsigned byte format, the coordinate
 * (GLA_PIXEL_OFS, GLA_PIXEL_OFS) is assumed to be within the view frustum,
 * and the modelview and projection matrices are assumed to define a
 * 1-to-1 mapping to screen space.
 * \attention Furthermore, in the case of zoomed or unpixel aligned
 * images extending outside the view frustum, but still within the
 * window, some portion of the image may be visible left and/or
 * below of the given \a x and \a y coordinates. It is recommended
 * to use the glScissor functionality if images are to be drawn
 * with an inset view matrix.
 */
void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect);

/**
 * glaDrawPixelsTex - Functions like a limited glDrawPixels, but actually draws the
 * image using textures, which can be tremendously faster on low-end
 * cards, and also avoids problems with the raster position being
 * clipped when offscreen. The routine respects the glPixelZoom values,
 * pixel unpacking parameters are _not_ respected.
 *
 * \attention This routine makes many assumptions: the rect data
 * is expected to be in RGBA byte or float format, and the
 * modelview and projection matrices are assumed to define a
 * 1-to-1 mapping to screen space.
 */
void glaDrawPixelsTex(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect);
void glaDrawPixelsTex_clipping(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                               float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y);
#endif

/* To be used before calling immDrawPixelsTex
 * Default shader is GPU_SHADER_2D_IMAGE_COLOR
 * Returns a shader to be able to set uniforms */
struct GPUShader *immDrawPixelsTexSetup(int builtin);

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
void immDrawPixelsTex(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                      float xzoom, float yzoom, float color[4]);
void immDrawPixelsTex_clipping(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                               float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y,
                               float xzoom, float yzoom, float color[4]);
#if 0 /* Obsolete / unused */
/**
 * glaDrawPixelsAuto - Switches between texture or pixel drawing using UserDef.
 * only RGBA
 * needs glaDefine2DArea to be set.
 */
void glaDrawPixelsAuto(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect);
void glaDrawPixelsAuto_clipping(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect,
                                float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y);


void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY);
void glaDrawPixelsTexScaled_clipping(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY,
                                     float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y);
#endif

void immDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY,
                           float xzoom, float yzoom, float color[4]);
void immDrawPixelsTexScaled_clipping(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect, float scaleX, float scaleY,
                                     float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y,
                                     float xzoom, float yzoom, float color[4]);
/* 2D Drawing Assistance */

/** Define a 2D area (viewport, scissor, matrices) for OpenGL rendering.
 *
 * glaDefine2DArea and glaBegin2DDraw set up an OpenGL state appropriate
 * for drawing using both vertex (Vertex, etc) and raster (RasterPos, Rect)
 * commands. All coordinates should be at integer positions. There is little
 * to no reason to use glVertex2f etc. functions during 2D rendering, and
 * thus no reason to +-0.5 the coordinates or perform other silly
 * tricks.
 *
 * \param screen_rect The screen rectangle to be defined for 2D drawing.
 */
void glaDefine2DArea(struct rcti *screen_rect);
#if 0  /* UNUSED */

typedef struct gla2DDrawInfo gla2DDrawInfo;

gla2DDrawInfo *glaBegin2DDraw(struct rcti *screen_rect, struct rctf *world_rect);
void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *r_sc_x, int *r_sc_y);
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int r_screen[2]);

void glaEnd2DDraw(gla2DDrawInfo *di);

/** Adjust the transformation mapping of a 2d area */
void gla2DGetMap(gla2DDrawInfo *di, struct rctf *rect);
void gla2DSetMap(gla2DDrawInfo *di, struct rctf *rect);

#endif /* UNUSED */

void set_inverted_drawing(int enable);
void setlinestyle(int nr);

/* own working polygon offset */
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

void glaDrawBorderCorners(const struct rcti *border, float zoomx, float zoomy);

void immDrawBorderCorners(unsigned int pos, const struct rcti *border, float zoomx, float zoomy);

#endif /* __BIF_GLUTIL_H__ */

