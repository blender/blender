/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Utility drawing functions (rough equivalent to OpenGL's GLU)
 */

#pragma once

#include "BLI_sys_types.h"

struct rctf;

/* Draw 2D rectangles (replaces glRect functions) */
/* caller is responsible for vertex format & shader */
void immRectf(uint pos, float x1, float y1, float x2, float y2);
void immRecti(uint pos, int x1, int y1, int x2, int y2);
void immRectf_with_texco(uint pos, uint tex_coord, const rctf &p, const rctf &uv);

/**
 * Same as #immRectf / #immRecti but does not call #immBegin / #immEnd.
 * To use with #GPU_PRIM_TRIS.
 */
void immRectf_fast(uint pos, float x1, float y1, float x2, float y2);
void immRectf_fast_with_color(
    uint pos, uint col, float x1, float y1, float x2, float y2, const float color[4]);
void immRecti_fast_with_color(
    uint pos, uint col, int x1, int y1, int x2, int y2, const float color[4]);

/**
 * Pack color into 3 bytes
 *
 * This define converts a numerical value to the equivalent 24-bit
 * color, while not being endian-sensitive. On little-endian, this
 * is the same as doing a 'naive' indexing, on big-endian, it is not!
 *
 * \note BGR format (i.e. 0xBBGGRR)...
 *
 * \param x: color.
 */
void imm_cpack(uint x);

/**
 * Draw a circle outline with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param shdr_pos: The vertex attribute number for position.
 * \param x: Horizontal center.
 * \param y: Vertical center.
 * \param radius: The circle's radius.
 * \param nsegments: The number of segments to use in drawing (more = smoother).
 */
void imm_draw_circle_wire_2d(uint shdr_pos, float x, float y, float radius, int nsegments);
/**
 * Draw a filled circle with the given \a radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \param shdr_pos: The vertex attribute number for position.
 * \param x: Horizontal center.
 * \param y: Vertical center.
 * \param radius: The circle's radius.
 * \param nsegments: The number of segments to use in drawing (more = smoother).
 */
void imm_draw_circle_fill_2d(uint shdr_pos, float x, float y, float radius, int nsegments);

void imm_draw_circle_wire_aspect_2d(
    uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments);
void imm_draw_circle_fill_aspect_2d(
    uint shdr_pos, float x, float y, float radius_x, float radius_y, int nsegments);

/**
 * Use this version when #GPUVertFormat has a vec3 position.
 */
void imm_draw_circle_wire_3d(uint pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_wire_aspect_3d(
    uint pos, float x, float y, float radius_x, float radius_y, int nsegments);
void imm_draw_circle_dashed_3d(uint pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill_3d(uint pos, float x, float y, float radius, int nsegments);
void imm_draw_circle_fill_aspect_3d(
    uint pos, float x, float y, float radius_x, float radius_y, int nsegments);

/**
 * Same as 'imm_draw_disk_partial_fill_2d', except it draws a wire arc.
 */
void imm_draw_circle_partial_wire_2d(
    uint pos, float x, float y, float radius, int nsegments, float start, float sweep);
void imm_draw_circle_partial_wire_3d(
    uint pos, float x, float y, float z, float radius, int nsegments, float start, float sweep);

/**
 * Draw a filled arc with the given inner and outer radius.
 * The circle is centered at \a x, \a y and drawn in the XY plane.
 *
 * \note Arguments are `gluPartialDisk` compatible.
 *
 * \param pos: The vertex attribute number for position.
 * \param x: Horizontal center.
 * \param y: Vertical center.
 * \param rad_inner: The inner circle's radius.
 * \param rad_outer: The outer circle's radius (can be zero).
 * \param nsegments: The number of segments to use in drawing (more = smoother).
 * \param start: Specifies the starting angle, in degrees, of the disk portion.
 * \param sweep: Specifies the sweep angle, in degrees, of the disk portion.
 */
void imm_draw_disk_partial_fill_2d(uint pos,
                                   float x,
                                   float y,
                                   float rad_inner,
                                   float rad_outer,
                                   int nsegments,
                                   float start,
                                   float sweep);
void imm_draw_disk_partial_fill_3d(uint pos,
                                   float x,
                                   float y,
                                   float z,
                                   float rad_inner,
                                   float rad_outer,
                                   int nsegments,
                                   float start,
                                   float sweep);

/**
 * Draw a lined box.
 *
 * \param pos: The vertex attribute number for position.
 * \param x1: left.
 * \param y1: bottom.
 * \param x2: right.
 * \param y2: top.
 */
void imm_draw_box_wire_2d(uint pos, float x1, float y1, float x2, float y2);
void imm_draw_box_wire_3d(uint pos, float x1, float y1, float x2, float y2);

/**
 * Draw a standard checkerboard to indicate transparent backgrounds.
 */
void imm_draw_box_checker_2d_ex(float x1,
                                float y1,
                                float x2,
                                float y2,
                                const float color_primary[4],
                                const float color_secondary[4],
                                int checker_size);
void imm_draw_box_checker_2d(float x1, float y1, float x2, float y2, bool clear_alpha = false);

void imm_draw_cube_fill_3d(uint pos, const float center[3], const float aspect[3]);
void imm_draw_cube_wire_3d(uint pos, const float center[3], const float aspect[3]);
void imm_draw_cube_corners_3d(uint pos,
                              const float center[3],
                              const float aspect[3],
                              float factor);

/**
 * Draw a cylinder. Replacement for #gluCylinder.
 * \warning Slow, better use it only if you no other choices.
 *
 * \param pos: The vertex attribute number for position.
 * \param nor: The vertex attribute number for normal.
 * \param base: Specifies the radius of the cylinder at z = 0.
 * \param top: Specifies the radius of the cylinder at z = height.
 * \param height: Specifies the height of the cylinder.
 * \param slices: Specifies the number of subdivisions around the z axis.
 * \param stacks: Specifies the number of subdivisions along the z axis.
 */
void imm_draw_cylinder_fill_normal_3d(
    uint pos, uint nor, float base, float top, float height, int slices, int stacks);
void imm_draw_cylinder_wire_3d(
    uint pos, float base, float top, float height, int slices, int stacks);
void imm_draw_cylinder_fill_3d(
    uint pos, float base, float top, float height, int slices, int stacks);

void imm_drawcircball(const float cent[3], float radius, const float tmat[4][4], uint pos);
