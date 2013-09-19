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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/modifiers/intern/MOD_meshcache_util.h
 *  \ingroup modifiers
 */

#ifndef __MOD_MESHCACHE_UTIL_H__
#define __MOD_MESHCACHE_UTIL_H__

struct MPoly;
struct MLoop;

/* MOD_meshcache_mdd.c */
bool MOD_meshcache_read_mdd_index(FILE *fp,
                                  float (*vertexCos)[3], const int vertex_tot,
                                  const int index, const float factor,
                                  const char **err_str);
bool MOD_meshcache_read_mdd_frame(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float frame,
                                  const char **err_str);
bool MOD_meshcache_read_mdd_times(const char *filepath,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float time, const float fps, const char time_mode,
                                  const char **err_str);

/* MOD_meshcache_pc2.c */
bool MOD_meshcache_read_pc2_index(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot,
                                  const int index, const float factor,
                                  const char **err_str);
bool MOD_meshcache_read_pc2_frame(FILE *fp,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float frame,
                                  const char **err_str);
bool MOD_meshcache_read_pc2_times(const char *filepath,
                                  float (*vertexCos)[3], const int verts_tot, const char interp,
                                  const float time, const float fps, const char time_mode,
                                  const char **err_str);

/* MOD_meshcache_util.c */
void MOD_meshcache_calc_range(const float frame, const char interp,
                              const int frame_tot,
                              int r_index_range[2], float *r_factor);

#define FRAME_SNAP_EPS 0.0001f

#endif  /* __MOD_MESHCACHE_UTIL_H__ */
