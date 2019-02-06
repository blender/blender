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
#ifndef __BKE_ANIM_H__
#define __BKE_ANIM_H__

/** \file \ingroup bke
 */
struct Depsgraph;
struct ListBase;
struct Main;
struct Object;
struct ParticleSystem;
struct Path;
struct ReportList;
struct Scene;
struct bAnimVizSettings;
struct bMotionPath;
struct bPoseChannel;

/* ---------------------------------------------------- */
/* Animation Visualization */

void animviz_settings_init(struct bAnimVizSettings *avs);

struct bMotionPath *animviz_copy_motionpath(const struct bMotionPath *mpath_src);

void animviz_free_motionpath_cache(struct bMotionPath *mpath);
void animviz_free_motionpath(struct bMotionPath *mpath);

struct bMotionPath *animviz_verify_motionpaths(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan);

void animviz_get_object_motionpaths(struct Object *ob, ListBase *targets);
void animviz_calc_motionpaths(struct Depsgraph *depsgraph,
                              struct Main *bmain,
                              struct Scene *scene,
                              ListBase *targets,
                              bool restore,
                              bool current_frame_only);

/* ---------------------------------------------------- */
/* Curve Paths */

void free_path(struct Path *path);
void calc_curvepath(struct Object *ob, struct ListBase *nurbs);
int where_on_path(struct Object *ob, float ctime, float vec[4], float dir[3], float quat[4], float *radius, float *weight);

/* ---------------------------------------------------- */
/* Dupli-Geometry */

struct ListBase *object_duplilist(struct Depsgraph *depsgraph, struct Scene *sce, struct Object *ob);
void free_object_duplilist(struct ListBase *lb);
int count_duplilist(struct Object *ob);

typedef struct DupliObject {
	struct DupliObject *next, *prev;
	struct Object *ob;
	float mat[4][4];
	float orco[3], uv[2];

	short type; /* from Object.transflag */
	char no_draw;

	/* Persistent identifier for a dupli object, for inter-frame matching of
	 * objects with motion blur, or inter-update matching for syncing. */
	int persistent_id[16]; /* 2*MAX_DUPLI_RECUR */

	/* Particle this dupli was generated from. */
	struct ParticleSystem *particle_system;

	/* Random ID for shading */
	unsigned int random_id;
} DupliObject;

#endif
