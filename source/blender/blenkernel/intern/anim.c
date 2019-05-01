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

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <stdlib.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "DEG_depsgraph_build.h"

#include "GPU_batch.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.anim"};

/* --------------------- */
/* forward declarations */

/* ******************************************************************** */
/* Animation Visualization */

/* Initialize the default settings for animation visualization */
void animviz_settings_init(bAnimVizSettings *avs)
{
  /* sanity check */
  if (avs == NULL) {
    return;
  }

  /* path settings */
  avs->path_bc = avs->path_ac = 10;

  avs->path_sf = 1;   /* xxx - take from scene instead? */
  avs->path_ef = 250; /* xxx - take from scene instead? */

  avs->path_viewflag = (MOTIONPATH_VIEW_KFRAS | MOTIONPATH_VIEW_KFNOS);

  avs->path_step = 1;

  avs->path_bakeflag |= MOTIONPATH_BAKE_HEADS;
}

/* ------------------- */

/* Free the given motion path's cache */
void animviz_free_motionpath_cache(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == NULL) {
    return;
  }

  /* free the path if necessary */
  if (mpath->points) {
    MEM_freeN(mpath->points);
  }

  GPU_VERTBUF_DISCARD_SAFE(mpath->points_vbo);
  GPU_BATCH_DISCARD_SAFE(mpath->batch_line);
  GPU_BATCH_DISCARD_SAFE(mpath->batch_points);

  /* reset the relevant parameters */
  mpath->points = NULL;
  mpath->length = 0;
}

/* Free the given motion path instance and its data
 * NOTE: this frees the motion path given!
 */
void animviz_free_motionpath(bMotionPath *mpath)
{
  /* sanity check */
  if (mpath == NULL) {
    return;
  }

  /* free the cache first */
  animviz_free_motionpath_cache(mpath);

  /* now the instance itself */
  MEM_freeN(mpath);
}

/* ------------------- */

/* Make a copy of motionpath data, so that viewing with copy on write works */
bMotionPath *animviz_copy_motionpath(const bMotionPath *mpath_src)
{
  bMotionPath *mpath_dst;

  if (mpath_src == NULL) {
    return NULL;
  }

  mpath_dst = MEM_dupallocN(mpath_src);
  mpath_dst->points = MEM_dupallocN(mpath_src->points);

  /* should get recreated on draw... */
  mpath_dst->points_vbo = NULL;
  mpath_dst->batch_line = NULL;
  mpath_dst->batch_points = NULL;

  return mpath_dst;
}

/* ------------------- */

/**
 * Setup motion paths for the given data.
 * \note Only used when explicitly calculating paths on bones which may/may not be consider already
 *
 * \param scene: Current scene (for frame ranges, etc.)
 * \param ob: Object to add paths for (must be provided)
 * \param pchan: Posechannel to add paths for (optional; if not provided, object-paths are assumed)
 */
bMotionPath *animviz_verify_motionpaths(ReportList *reports,
                                        Scene *scene,
                                        Object *ob,
                                        bPoseChannel *pchan)
{
  bAnimVizSettings *avs;
  bMotionPath *mpath, **dst;

  /* sanity checks */
  if (ELEM(NULL, scene, ob)) {
    return NULL;
  }

  /* get destination data */
  if (pchan) {
    /* paths for posechannel - assume that posechannel belongs to the object */
    avs = &ob->pose->avs;
    dst = &pchan->mpath;
  }
  else {
    /* paths for object */
    avs = &ob->avs;
    dst = &ob->mpath;
  }

  /* avoid 0 size allocs */
  if (avs->path_sf >= avs->path_ef) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Motion path frame extents invalid for %s (%d to %d)%s",
                (pchan) ? pchan->name : ob->id.name,
                avs->path_sf,
                avs->path_ef,
                (avs->path_sf == avs->path_ef) ? TIP_(", cannot have single-frame paths") : "");
    return NULL;
  }

  /* if there is already a motionpath, just return that,
   * provided it's settings are ok (saves extra free+alloc)
   */
  if (*dst != NULL) {
    int expected_length = avs->path_ef - avs->path_sf;

    mpath = *dst;

    /* Path is "valid" if length is valid,
     * but must also be of the same length as is being requested. */
    if ((mpath->start_frame != mpath->end_frame) && (mpath->length > 0)) {
      /* outer check ensures that we have some curve data for this path */
      if (mpath->length == expected_length) {
        /* return/use this as it is already valid length */
        return mpath;
      }
      else {
        /* clear the existing path (as the range has changed), and reallocate below */
        animviz_free_motionpath_cache(mpath);
      }
    }
  }
  else {
    /* create a new motionpath, and assign it */
    mpath = MEM_callocN(sizeof(bMotionPath), "bMotionPath");
    *dst = mpath;
  }

  /* set settings from the viz settings */
  mpath->start_frame = avs->path_sf;
  mpath->end_frame = avs->path_ef;

  mpath->length = mpath->end_frame - mpath->start_frame;

  if (avs->path_bakeflag & MOTIONPATH_BAKE_HEADS) {
    mpath->flag |= MOTIONPATH_FLAG_BHEAD;
  }
  else {
    mpath->flag &= ~MOTIONPATH_FLAG_BHEAD;
  }

  /* set default custom values */
  mpath->color[0] = 1.0; /* Red */
  mpath->color[1] = 0.0;
  mpath->color[2] = 0.0;

  mpath->line_thickness = 2;
  mpath->flag |= MOTIONPATH_FLAG_LINES; /* draw lines by default */

  /* allocate a cache */
  mpath->points = MEM_callocN(sizeof(bMotionPathVert) * mpath->length, "bMotionPathVerts");

  /* tag viz settings as currently having some path(s) which use it */
  avs->path_bakeflag |= MOTIONPATH_BAKE_HAS_PATHS;

  /* return it */
  return mpath;
}

/* ******************************************************************** */
/* Curve Paths - for curve deforms and/or curve following */

/* free curve path data
 * NOTE: frees the path itself!
 * NOTE: this is increasingly inaccurate with non-uniform BevPoint subdivisions [#24633]
 */
void free_path(Path *path)
{
  if (path->data) {
    MEM_freeN(path->data);
  }
  MEM_freeN(path);
}

/* calculate a curve-deform path for a curve
 * - only called from displist.c -> do_makeDispListCurveTypes
 */
void calc_curvepath(Object *ob, ListBase *nurbs)
{
  BevList *bl;
  BevPoint *bevp, *bevpn, *bevpfirst, *bevplast;
  PathPoint *pp;
  Nurb *nu;
  Path *path;
  float *fp, *dist, *maxdist, xyz[3];
  float fac, d = 0, fac1, fac2;
  int a, tot, cycl = 0;

  /* in a path vertices are with equal differences: path->len = number of verts */
  /* NOW WITH BEVELCURVE!!! */

  if (ob == NULL || ob->type != OB_CURVE) {
    return;
  }

  if (ob->runtime.curve_cache->path) {
    free_path(ob->runtime.curve_cache->path);
  }
  ob->runtime.curve_cache->path = NULL;

  /* weak! can only use first curve */
  bl = ob->runtime.curve_cache->bev.first;
  if (bl == NULL || !bl->nr) {
    return;
  }

  nu = nurbs->first;

  ob->runtime.curve_cache->path = path = MEM_callocN(sizeof(Path), "calc_curvepath");

  /* if POLY: last vertice != first vertice */
  cycl = (bl->poly != -1);

  tot = cycl ? bl->nr : bl->nr - 1;

  path->len = tot + 1;
  /* Exception: vector handle paths and polygon paths should be subdivided
   * at least a factor resolution. */
  if (path->len < nu->resolu * SEGMENTSU(nu)) {
    path->len = nu->resolu * SEGMENTSU(nu);
  }

  dist = (float *)MEM_mallocN(sizeof(float) * (tot + 1), "calcpathdist");

  /* all lengths in *dist */
  bevp = bevpfirst = bl->bevpoints;
  fp = dist;
  *fp = 0.0f;
  for (a = 0; a < tot; a++) {
    fp++;
    if (cycl && a == tot - 1) {
      sub_v3_v3v3(xyz, bevpfirst->vec, bevp->vec);
    }
    else {
      sub_v3_v3v3(xyz, (bevp + 1)->vec, bevp->vec);
    }

    *fp = *(fp - 1) + len_v3(xyz);
    bevp++;
  }

  path->totdist = *fp;

  /* the path verts  in path->data */
  /* now also with TILT value */
  pp = path->data = (PathPoint *)MEM_callocN(sizeof(PathPoint) * path->len, "pathdata");

  bevp = bevpfirst;
  bevpn = bevp + 1;
  bevplast = bevpfirst + (bl->nr - 1);
  if (UNLIKELY(bevpn > bevplast)) {
    bevpn = cycl ? bevpfirst : bevplast;
  }
  fp = dist + 1;
  maxdist = dist + tot;
  fac = 1.0f / ((float)path->len - 1.0f);
  fac = fac * path->totdist;

  for (a = 0; a < path->len; a++) {

    d = ((float)a) * fac;

    /* we're looking for location (distance) 'd' in the array */
    if (LIKELY(tot > 0)) {
      while ((fp < maxdist) && (d >= *fp)) {
        fp++;
        if (bevp < bevplast) {
          bevp++;
        }
        bevpn = bevp + 1;
        if (UNLIKELY(bevpn > bevplast)) {
          bevpn = cycl ? bevpfirst : bevplast;
        }
      }

      fac1 = (*(fp)-d) / (*(fp) - *(fp - 1));
      fac2 = 1.0f - fac1;
    }
    else {
      fac1 = 1.0f;
      fac2 = 0.0f;
    }

    interp_v3_v3v3(pp->vec, bevp->vec, bevpn->vec, fac2);
    pp->vec[3] = fac1 * bevp->tilt + fac2 * bevpn->tilt;
    pp->radius = fac1 * bevp->radius + fac2 * bevpn->radius;
    pp->weight = fac1 * bevp->weight + fac2 * bevpn->weight;
    interp_qt_qtqt(pp->quat, bevp->quat, bevpn->quat, fac2);
    normalize_qt(pp->quat);

    pp++;
  }

  MEM_freeN(dist);
}

static int interval_test(const int min, const int max, int p1, const int cycl)
{
  if (cycl) {
    p1 = mod_i(p1 - min, (max - min + 1)) + min;
  }
  else {
    if (p1 < min) {
      p1 = min;
    }
    else if (p1 > max) {
      p1 = max;
    }
  }
  return p1;
}

/* calculate the deformation implied by the curve path at a given parametric position,
 * and returns whether this operation succeeded.
 *
 * note: ctime is normalized range <0-1>
 *
 * returns OK: 1/0
 */
int where_on_path(Object *ob,
                  float ctime,
                  float vec[4],
                  float dir[3],
                  float quat[4],
                  float *radius,
                  float *weight)
{
  Curve *cu;
  Nurb *nu;
  BevList *bl;
  Path *path;
  PathPoint *pp, *p0, *p1, *p2, *p3;
  float fac;
  float data[4];
  int cycl = 0, s0, s1, s2, s3;
  ListBase *nurbs;

  if (ob == NULL || ob->type != OB_CURVE) {
    return 0;
  }
  cu = ob->data;
  if (ob->runtime.curve_cache == NULL || ob->runtime.curve_cache->path == NULL ||
      ob->runtime.curve_cache->path->data == NULL) {
    CLOG_WARN(&LOG, "no path!");
    return 0;
  }
  path = ob->runtime.curve_cache->path;
  pp = path->data;

  /* test for cyclic */
  bl = ob->runtime.curve_cache->bev.first;
  if (!bl) {
    return 0;
  }
  if (!bl->nr) {
    return 0;
  }
  if (bl->poly > -1) {
    cycl = 1;
  }

  /* values below zero for non-cyclic curves give strange results */
  BLI_assert(cycl || ctime >= 0.0f);

  ctime *= (path->len - 1);

  s1 = (int)floor(ctime);
  fac = (float)(s1 + 1) - ctime;

  /* path->len is corrected for cyclic */
  s0 = interval_test(0, path->len - 1 - cycl, s1 - 1, cycl);
  s1 = interval_test(0, path->len - 1 - cycl, s1, cycl);
  s2 = interval_test(0, path->len - 1 - cycl, s1 + 1, cycl);
  s3 = interval_test(0, path->len - 1 - cycl, s1 + 2, cycl);

  p0 = pp + s0;
  p1 = pp + s1;
  p2 = pp + s2;
  p3 = pp + s3;

  /* NOTE: commented out for follow constraint
   *
   *       If it's ever be uncommented watch out for curve_deform_verts()
   *       which used to temporary set CU_FOLLOW flag for the curve and no
   *       longer does it (because of threading issues of such a thing.
   */
  // if (cu->flag & CU_FOLLOW) {

  key_curve_tangent_weights(1.0f - fac, data, KEY_BSPLINE);

  interp_v3_v3v3v3v3(dir, p0->vec, p1->vec, p2->vec, p3->vec, data);

  /* make compatible with vectoquat */
  negate_v3(dir);
  //}

  nurbs = BKE_curve_editNurbs_get(cu);
  if (!nurbs) {
    nurbs = &cu->nurb;
  }
  nu = nurbs->first;

  /* make sure that first and last frame are included in the vectors here  */
  if (nu->type == CU_POLY) {
    key_curve_position_weights(1.0f - fac, data, KEY_LINEAR);
  }
  else if (nu->type == CU_BEZIER) {
    key_curve_position_weights(1.0f - fac, data, KEY_LINEAR);
  }
  else if (s0 == s1 || p2 == p3) {
    key_curve_position_weights(1.0f - fac, data, KEY_CARDINAL);
  }
  else {
    key_curve_position_weights(1.0f - fac, data, KEY_BSPLINE);
  }

  vec[0] = data[0] * p0->vec[0] + data[1] * p1->vec[0] + data[2] * p2->vec[0] +
           data[3] * p3->vec[0]; /* X */
  vec[1] = data[0] * p0->vec[1] + data[1] * p1->vec[1] + data[2] * p2->vec[1] +
           data[3] * p3->vec[1]; /* Y */
  vec[2] = data[0] * p0->vec[2] + data[1] * p1->vec[2] + data[2] * p2->vec[2] +
           data[3] * p3->vec[2]; /* Z */
  vec[3] = data[0] * p0->vec[3] + data[1] * p1->vec[3] + data[2] * p2->vec[3] +
           data[3] * p3->vec[3]; /* Tilt, should not be needed since we have quat still used */

  if (quat) {
    float totfac, q1[4], q2[4];

    totfac = data[0] + data[3];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(q1, p0->quat, p3->quat, data[3] / totfac);
    }
    else {
      copy_qt_qt(q1, p1->quat);
    }

    totfac = data[1] + data[2];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(q2, p1->quat, p2->quat, data[2] / totfac);
    }
    else {
      copy_qt_qt(q2, p3->quat);
    }

    totfac = data[0] + data[1] + data[2] + data[3];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(quat, q1, q2, (data[1] + data[2]) / totfac);
    }
    else {
      copy_qt_qt(quat, q2);
    }
  }

  if (radius) {
    *radius = data[0] * p0->radius + data[1] * p1->radius + data[2] * p2->radius +
              data[3] * p3->radius;
  }

  if (weight) {
    *weight = data[0] * p0->weight + data[1] * p1->weight + data[2] * p2->weight +
              data[3] * p3->weight;
  }

  return 1;
}
