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
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ListBase;
struct Object;
struct ParticleSystem;
struct Scene;

/* ---------------------------------------------------- */
/* Dupli-Geometry */

struct ListBase *object_duplilist(struct Depsgraph *depsgraph,
                                  struct Scene *sce,
                                  struct Object *ob);
void free_object_duplilist(struct ListBase *lb);

typedef struct DupliObject {
  struct DupliObject *next, *prev;
  struct Object *ob;
  float mat[4][4];
  float orco[3], uv[2];

  short type; /* from Object.transflag */
  char no_draw;

  /* Persistent identifier for a dupli object, for inter-frame matching of
   * objects with motion blur, or inter-update matching for syncing. */
  int persistent_id[8]; /* MAX_DUPLI_RECUR */

  /* Particle this dupli was generated from. */
  struct ParticleSystem *particle_system;

  /* Random ID for shading */
  unsigned int random_id;
} DupliObject;

#ifdef __cplusplus
}
#endif
