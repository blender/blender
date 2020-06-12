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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

/**
 * variables on the UI for now
 * <pre>
 * float mediafrict;  friction to env
 * float nodemass;    softbody mass of *vertex*
 * float grav;        softbody amount of gravitation to apply
 *
 * float goalspring;  softbody goal springs
 * float goalfrict;   softbody goal springs friction
 * float mingoal;     quick limits for goal
 * float maxgoal;
 *
 * float inspring;    softbody inner springs
 * float infrict;     softbody inner springs friction
 * </pre>
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* types */
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_collection.h"
#include "BKE_collision.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "PIL_time.h"

static CLG_LogRef LOG = {"bke.softbody"};

/* callbacks for errors and interrupts and some goo */
static int (*SB_localInterruptCallBack)(void) = NULL;

/* ********** soft body engine ******* */

typedef enum { SB_EDGE = 1, SB_BEND = 2, SB_STIFFQUAD = 3, SB_HANDLE = 4 } type_spring;

typedef struct BodySpring {
  int v1, v2;
  float len, cf, load;
  float ext_force[3]; /* edges colliding and sailing */
  type_spring springtype;
  short flag;
} BodySpring;

typedef struct BodyFace {
  int v1, v2, v3;
  float ext_force[3]; /* faces colliding */
  short flag;
} BodyFace;

typedef struct ReferenceVert {
  float pos[3]; /* position relative to com */
  float mass;   /* node mass */
} ReferenceVert;

typedef struct ReferenceState {
  float com[3];         /* center of mass*/
  ReferenceVert *ivert; /* list of initial values */
} ReferenceState;

/*private scratch pad for caching and other data only needed when alive*/
typedef struct SBScratch {
  GHash *colliderhash;
  short needstobuildcollider;
  short flag;
  BodyFace *bodyface;
  int totface;
  float aabbmin[3], aabbmax[3];
  ReferenceState Ref;
} SBScratch;

typedef struct SB_thread_context {
  Scene *scene;
  Object *ob;
  float forcetime;
  float timenow;
  int ifirst;
  int ilast;
  ListBase *effectors;
  int do_deflector;
  float fieldfactor;
  float windfactor;
  int nr;
  int tot;
} SB_thread_context;

#define MID_PRESERVE 1

#define SOFTGOALSNAP 0.999f
/* if bp-> goal is above make it a *forced follow original* and skip all ODE stuff for this bp
 * removes *unnecessary* stiffness from ODE system
 */
#define HEUNWARNLIMIT 1 /* 500 would be fine i think for detecting severe *stiff* stuff */

#define BSF_INTERSECT 1 /* edge intersects collider face */

/* private definitions for bodypoint states */
#define SBF_DOFUZZY 1        /* Bodypoint do fuzzy */
#define SBF_OUTOFCOLLISION 2 /* Bodypoint does not collide  */

#define BFF_INTERSECT 1 /* collider edge   intrudes face */
#define BFF_CLOSEVERT 2 /* collider vertex repulses face */

/* humm .. this should be calculated from sb parameters and sizes. */
static float SoftHeunTol = 1.0f;

/* local prototypes */
static void free_softbody_intern(SoftBody *sb);

/*+++ frame based timing +++*/

/*physical unit of force is [kg * m / sec^2]*/

static float sb_grav_force_scale(Object *UNUSED(ob))
/* since unit of g is [m/sec^2] and F = mass * g we rescale unit mass of node to 1 gramm
 * put it to a function here, so we can add user options later without touching simulation code
 */
{
  return (0.001f);
}

static float sb_fric_force_scale(Object *UNUSED(ob))
/* rescaling unit of drag [1 / sec] to somehow reasonable
 * put it to a function here, so we can add user options later without touching simulation code
 */
{
  return (0.01f);
}

static float sb_time_scale(Object *ob)
/* defining the frames to *real* time relation */
{
  SoftBody *sb = ob->soft; /* is supposed to be there */
  if (sb) {
    return (sb->physics_speed);
    /* hrms .. this could be IPO as well :)
     * estimated range [0.001 sluggish slug - 100.0 very fast (i hope ODE solver can handle that)]
     * 1 approx = a unit 1 pendulum at g = 9.8 [earth conditions]  has period 65 frames
     * theory would give a 50 frames period .. so there must be something inaccurate ..
     * looking for that (BM). */
  }
  return (1.0f);
  /*
   * this would be frames/sec independent timing assuming 25 fps is default
   * but does not work very well with NLA
   * return (25.0f/scene->r.frs_sec)
   */
}
/*--- frame based timing ---*/

/* helper functions for everything is animatable jow_go_for2_5 +++++++*/
/* introducing them here, because i know: steps in properties  ( at frame timing )
 * will cause unwanted responses of the softbody system (which does inter frame calculations )
 * so first 'cure' would be: interpolate linear in time ..
 * Q: why do i write this?
 * A: because it happened once, that some eger coder 'streamlined' code to fail.
 * We DO linear interpolation for goals .. and i think we should do on animated properties as well
 */

/* animate sb->maxgoal, sb->mingoal */
static float _final_goal(Object *ob, BodyPoint *bp) /*jow_go_for2_5 */
{
  float f = -1999.99f;
  if (ob) {
    SoftBody *sb = ob->soft; /* is supposed to be there */
    if (!(ob->softflag & OB_SB_GOAL)) {
      return (0.0f);
    }
    if (sb && bp) {
      if (bp->goal < 0.0f) {
        return (0.0f);
      }
      f = sb->mingoal + bp->goal * fabsf(sb->maxgoal - sb->mingoal);
      f = pow(f, 4.0f);
      return (f);
    }
  }
  CLOG_ERROR(&LOG, "sb or bp == NULL");
  return f; /*using crude but spot able values some times helps debuggin */
}

static float _final_mass(Object *ob, BodyPoint *bp)
{
  if (ob) {
    SoftBody *sb = ob->soft; /* is supposed to be there */
    if (sb && bp) {
      return (bp->mass * sb->nodemass);
    }
  }
  CLOG_ERROR(&LOG, "sb or bp == NULL");
  return 1.0f;
}
/* helper functions for everything is animateble jow_go_for2_5 ------*/

/*+++ collider caching and dicing +++*/

/*
 * for each target object/face the axis aligned bounding box (AABB) is stored
 * faces parallel to global axes
 * so only simple "value" in [min, max] checks are used
 * float operations still
 */

/* just an ID here to reduce the prob for killing objects
 * ob->sumohandle points to we should not kill :)
 */
static const int CCD_SAFETY = 190561;

typedef struct ccdf_minmax {
  float minx, miny, minz, maxx, maxy, maxz;
} ccdf_minmax;

typedef struct ccd_Mesh {
  int mvert_num, tri_num;
  const MVert *mvert;
  const MVert *mprevvert;
  const MVertTri *tri;
  int safety;
  ccdf_minmax *mima;
  /* Axis Aligned Bounding Box AABB */
  float bbmin[3];
  float bbmax[3];
} ccd_Mesh;

static ccd_Mesh *ccd_mesh_make(Object *ob)
{
  CollisionModifierData *cmd;
  ccd_Mesh *pccd_M = NULL;
  ccdf_minmax *mima;
  const MVertTri *vt;
  float hull;
  int i;

  cmd = (CollisionModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Collision);

  /* first some paranoia checks */
  if (!cmd) {
    return NULL;
  }
  if (!cmd->mvert_num || !cmd->tri_num) {
    return NULL;
  }

  pccd_M = MEM_mallocN(sizeof(ccd_Mesh), "ccd_Mesh");
  pccd_M->mvert_num = cmd->mvert_num;
  pccd_M->tri_num = cmd->tri_num;
  pccd_M->safety = CCD_SAFETY;
  pccd_M->bbmin[0] = pccd_M->bbmin[1] = pccd_M->bbmin[2] = 1e30f;
  pccd_M->bbmax[0] = pccd_M->bbmax[1] = pccd_M->bbmax[2] = -1e30f;
  pccd_M->mprevvert = NULL;

  /* blow it up with forcefield ranges */
  hull = max_ff(ob->pd->pdef_sbift, ob->pd->pdef_sboft);

  /* alloc and copy verts*/
  pccd_M->mvert = MEM_dupallocN(cmd->xnew);
  /* note that xnew coords are already in global space, */
  /* determine the ortho BB */
  for (i = 0; i < pccd_M->mvert_num; i++) {
    const float *v;

    /* evaluate limits */
    v = pccd_M->mvert[i].co;
    pccd_M->bbmin[0] = min_ff(pccd_M->bbmin[0], v[0] - hull);
    pccd_M->bbmin[1] = min_ff(pccd_M->bbmin[1], v[1] - hull);
    pccd_M->bbmin[2] = min_ff(pccd_M->bbmin[2], v[2] - hull);

    pccd_M->bbmax[0] = max_ff(pccd_M->bbmax[0], v[0] + hull);
    pccd_M->bbmax[1] = max_ff(pccd_M->bbmax[1], v[1] + hull);
    pccd_M->bbmax[2] = max_ff(pccd_M->bbmax[2], v[2] + hull);
  }
  /* alloc and copy faces*/
  pccd_M->tri = MEM_dupallocN(cmd->tri);

  /* OBBs for idea1 */
  pccd_M->mima = MEM_mallocN(sizeof(ccdf_minmax) * pccd_M->tri_num, "ccd_Mesh_Faces_mima");

  /* anyhoo we need to walk the list of faces and find OBB they live in */
  for (i = 0, mima = pccd_M->mima, vt = pccd_M->tri; i < pccd_M->tri_num; i++, mima++, vt++) {
    const float *v;

    mima->minx = mima->miny = mima->minz = 1e30f;
    mima->maxx = mima->maxy = mima->maxz = -1e30f;

    v = pccd_M->mvert[vt->tri[0]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mvert[vt->tri[1]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mvert[vt->tri[2]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);
  }

  return pccd_M;
}
static void ccd_mesh_update(Object *ob, ccd_Mesh *pccd_M)
{
  CollisionModifierData *cmd;
  ccdf_minmax *mima;
  const MVertTri *vt;
  float hull;
  int i;

  cmd = (CollisionModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Collision);

  /* first some paranoia checks */
  if (!cmd) {
    return;
  }
  if (!cmd->mvert_num || !cmd->tri_num) {
    return;
  }

  if ((pccd_M->mvert_num != cmd->mvert_num) || (pccd_M->tri_num != cmd->tri_num)) {
    return;
  }

  pccd_M->bbmin[0] = pccd_M->bbmin[1] = pccd_M->bbmin[2] = 1e30f;
  pccd_M->bbmax[0] = pccd_M->bbmax[1] = pccd_M->bbmax[2] = -1e30f;

  /* blow it up with forcefield ranges */
  hull = max_ff(ob->pd->pdef_sbift, ob->pd->pdef_sboft);

  /* rotate current to previous */
  if (pccd_M->mprevvert) {
    MEM_freeN((void *)pccd_M->mprevvert);
  }
  pccd_M->mprevvert = pccd_M->mvert;
  /* alloc and copy verts*/
  pccd_M->mvert = MEM_dupallocN(cmd->xnew);
  /* note that xnew coords are already in global space, */
  /* determine the ortho BB */
  for (i = 0; i < pccd_M->mvert_num; i++) {
    const float *v;

    /* evaluate limits */
    v = pccd_M->mvert[i].co;
    pccd_M->bbmin[0] = min_ff(pccd_M->bbmin[0], v[0] - hull);
    pccd_M->bbmin[1] = min_ff(pccd_M->bbmin[1], v[1] - hull);
    pccd_M->bbmin[2] = min_ff(pccd_M->bbmin[2], v[2] - hull);

    pccd_M->bbmax[0] = max_ff(pccd_M->bbmax[0], v[0] + hull);
    pccd_M->bbmax[1] = max_ff(pccd_M->bbmax[1], v[1] + hull);
    pccd_M->bbmax[2] = max_ff(pccd_M->bbmax[2], v[2] + hull);

    /* evaluate limits */
    v = pccd_M->mprevvert[i].co;
    pccd_M->bbmin[0] = min_ff(pccd_M->bbmin[0], v[0] - hull);
    pccd_M->bbmin[1] = min_ff(pccd_M->bbmin[1], v[1] - hull);
    pccd_M->bbmin[2] = min_ff(pccd_M->bbmin[2], v[2] - hull);

    pccd_M->bbmax[0] = max_ff(pccd_M->bbmax[0], v[0] + hull);
    pccd_M->bbmax[1] = max_ff(pccd_M->bbmax[1], v[1] + hull);
    pccd_M->bbmax[2] = max_ff(pccd_M->bbmax[2], v[2] + hull);
  }

  /* anyhoo we need to walk the list of faces and find OBB they live in */
  for (i = 0, mima = pccd_M->mima, vt = pccd_M->tri; i < pccd_M->tri_num; i++, mima++, vt++) {
    const float *v;

    mima->minx = mima->miny = mima->minz = 1e30f;
    mima->maxx = mima->maxy = mima->maxz = -1e30f;

    /* mvert */
    v = pccd_M->mvert[vt->tri[0]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mvert[vt->tri[1]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mvert[vt->tri[2]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    /* mprevvert */
    v = pccd_M->mprevvert[vt->tri[0]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mprevvert[vt->tri[1]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);

    v = pccd_M->mprevvert[vt->tri[2]].co;
    mima->minx = min_ff(mima->minx, v[0] - hull);
    mima->miny = min_ff(mima->miny, v[1] - hull);
    mima->minz = min_ff(mima->minz, v[2] - hull);
    mima->maxx = max_ff(mima->maxx, v[0] + hull);
    mima->maxy = max_ff(mima->maxy, v[1] + hull);
    mima->maxz = max_ff(mima->maxz, v[2] + hull);
  }
  return;
}

static void ccd_mesh_free(ccd_Mesh *ccdm)
{
  /* Make sure we're not nuking objects we don't know. */
  if (ccdm && (ccdm->safety == CCD_SAFETY)) {
    MEM_freeN((void *)ccdm->mvert);
    MEM_freeN((void *)ccdm->tri);
    if (ccdm->mprevvert) {
      MEM_freeN((void *)ccdm->mprevvert);
    }
    MEM_freeN(ccdm->mima);
    MEM_freeN(ccdm);
    ccdm = NULL;
  }
}

static void ccd_build_deflector_hash_single(GHash *hash, Object *ob)
{
  /* only with deflecting set */
  if (ob->pd && ob->pd->deflect) {
    void **val_p;
    if (!BLI_ghash_ensure_p(hash, ob, &val_p)) {
      ccd_Mesh *ccdmesh = ccd_mesh_make(ob);
      *val_p = ccdmesh;
    }
  }
}

/**
 * \note collection overrides scene when not NULL.
 */
static void ccd_build_deflector_hash(Depsgraph *depsgraph,
                                     Collection *collection,
                                     Object *vertexowner,
                                     GHash *hash)
{
  if (!hash) {
    return;
  }

  unsigned int numobjects;
  Object **objects = BKE_collision_objects_create(
      depsgraph, vertexowner, collection, &numobjects, eModifierType_Collision);

  for (int i = 0; i < numobjects; i++) {
    Object *ob = objects[i];

    if (ob->type == OB_MESH) {
      ccd_build_deflector_hash_single(hash, ob);
    }
  }

  BKE_collision_objects_free(objects);
}

static void ccd_update_deflector_hash_single(GHash *hash, Object *ob)
{
  if (ob->pd && ob->pd->deflect) {
    ccd_Mesh *ccdmesh = BLI_ghash_lookup(hash, ob);
    if (ccdmesh) {
      ccd_mesh_update(ob, ccdmesh);
    }
  }
}

/**
 * \note collection overrides scene when not NULL.
 */
static void ccd_update_deflector_hash(Depsgraph *depsgraph,
                                      Collection *collection,
                                      Object *vertexowner,
                                      GHash *hash)
{
  if ((!hash) || (!vertexowner)) {
    return;
  }

  unsigned int numobjects;
  Object **objects = BKE_collision_objects_create(
      depsgraph, vertexowner, collection, &numobjects, eModifierType_Collision);

  for (int i = 0; i < numobjects; i++) {
    Object *ob = objects[i];

    if (ob->type == OB_MESH) {
      ccd_update_deflector_hash_single(hash, ob);
    }
  }

  BKE_collision_objects_free(objects);
}

/*--- collider caching and dicing ---*/

static int count_mesh_quads(Mesh *me)
{
  int a, result = 0;
  const MPoly *mp = me->mpoly;

  if (mp) {
    for (a = me->totpoly; a > 0; a--, mp++) {
      if (mp->totloop == 4) {
        result++;
      }
    }
  }
  return result;
}

static void add_mesh_quad_diag_springs(Object *ob)
{
  Mesh *me = ob->data;
  /*BodyPoint *bp;*/ /*UNUSED*/
  int a;

  if (ob->soft) {
    int nofquads;
    // float s_shear = ob->soft->shearstiff*ob->soft->shearstiff;

    nofquads = count_mesh_quads(me);
    if (nofquads) {
      const MLoop *mloop = me->mloop;
      const MPoly *mp = me->mpoly;
      BodySpring *bs;

      /* resize spring-array to hold additional quad springs */
      ob->soft->bspring = MEM_recallocN(ob->soft->bspring,
                                        sizeof(BodySpring) * (ob->soft->totspring + nofquads * 2));

      /* fill the tail */
      a = 0;
      bs = &ob->soft->bspring[ob->soft->totspring];
      /*bp= ob->soft->bpoint; */ /*UNUSED*/
      for (a = me->totpoly; a > 0; a--, mp++) {
        if (mp->totloop == 4) {
          bs->v1 = mloop[mp->loopstart + 0].v;
          bs->v2 = mloop[mp->loopstart + 2].v;
          bs->springtype = SB_STIFFQUAD;
          bs++;
          bs->v1 = mloop[mp->loopstart + 1].v;
          bs->v2 = mloop[mp->loopstart + 3].v;
          bs->springtype = SB_STIFFQUAD;
          bs++;
        }
      }

      /* now we can announce new springs */
      ob->soft->totspring += nofquads * 2;
    }
  }
}

static void add_2nd_order_roller(Object *ob, float UNUSED(stiffness), int *counter, int addsprings)
{
  /*assume we have a softbody*/
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp, *bpo;
  BodySpring *bs, *bs2, *bs3 = NULL;
  int a, b, c, notthis = 0, v0;
  if (!sb->bspring) {
    return;
  } /* we are 2nd order here so 1rst should have been build :) */
  /* first run counting  second run adding */
  *counter = 0;
  if (addsprings) {
    bs3 = ob->soft->bspring + ob->soft->totspring;
  }
  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    /*scan for neighborhood*/
    bpo = NULL;
    v0 = (sb->totpoint - a);
    for (b = bp->nofsprings; b > 0; b--) {
      bs = sb->bspring + bp->springs[b - 1];
      /* Nasty thing here that springs have two ends
       * so here we have to make sure we examine the other */
      if (v0 == bs->v1) {
        bpo = sb->bpoint + bs->v2;
        notthis = bs->v2;
      }
      else {
        if (v0 == bs->v2) {
          bpo = sb->bpoint + bs->v1;
          notthis = bs->v1;
        }
        else {
          CLOG_ERROR(&LOG, "oops we should not get here");
        }
      }
      if (bpo) { /* so now we have a 2nd order humpdidump */
        for (c = bpo->nofsprings; c > 0; c--) {
          bs2 = sb->bspring + bpo->springs[c - 1];
          if ((bs2->v1 != notthis) && (bs2->v1 > v0)) {
            (*counter)++; /*hit */
            if (addsprings) {
              bs3->v1 = v0;
              bs3->v2 = bs2->v1;
              bs3->springtype = SB_BEND;
              bs3++;
            }
          }
          if ((bs2->v2 != notthis) && (bs2->v2 > v0)) {
            (*counter)++; /* hit */
            if (addsprings) {
              bs3->v1 = v0;
              bs3->v2 = bs2->v2;
              bs3->springtype = SB_BEND;
              bs3++;
            }
          }
        }
      }
    }
    /*scan for neighborhood done*/
  }
}

static void add_2nd_order_springs(Object *ob, float stiffness)
{
  int counter = 0;
  BodySpring *bs_new;
  stiffness *= stiffness;

  add_2nd_order_roller(ob, stiffness, &counter, 0); /* counting */
  if (counter) {
    /* resize spring-array to hold additional springs */
    bs_new = MEM_callocN((ob->soft->totspring + counter) * sizeof(BodySpring), "bodyspring");
    memcpy(bs_new, ob->soft->bspring, (ob->soft->totspring) * sizeof(BodySpring));

    if (ob->soft->bspring) {
      MEM_freeN(ob->soft->bspring);
    }
    ob->soft->bspring = bs_new;

    add_2nd_order_roller(ob, stiffness, &counter, 1); /* adding */
    ob->soft->totspring += counter;
  }
}

static void add_bp_springlist(BodyPoint *bp, int springID)
{
  int *newlist;

  if (bp->springs == NULL) {
    bp->springs = MEM_callocN(sizeof(int), "bpsprings");
    bp->springs[0] = springID;
    bp->nofsprings = 1;
  }
  else {
    bp->nofsprings++;
    newlist = MEM_callocN(bp->nofsprings * sizeof(int), "bpsprings");
    memcpy(newlist, bp->springs, (bp->nofsprings - 1) * sizeof(int));
    MEM_freeN(bp->springs);
    bp->springs = newlist;
    bp->springs[bp->nofsprings - 1] = springID;
  }
}

/**
 * Do this once when sb is build it is `O(N^2)`
 * so scanning for springs every iteration is too expensive.
 */
static void build_bps_springlist(Object *ob)
{
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp;
  BodySpring *bs;
  int a, b;

  if (sb == NULL) {
    return; /* paranoia check */
  }

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    /* throw away old list */
    if (bp->springs) {
      MEM_freeN(bp->springs);
      bp->springs = NULL;
    }
    /* scan for attached inner springs */
    for (b = sb->totspring, bs = sb->bspring; b > 0; b--, bs++) {
      if (((sb->totpoint - a) == bs->v1)) {
        add_bp_springlist(bp, sb->totspring - b);
      }
      if (((sb->totpoint - a) == bs->v2)) {
        add_bp_springlist(bp, sb->totspring - b);
      }
    } /*for springs*/
  }   /*for bp*/
}

static void calculate_collision_balls(Object *ob)
{
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp;
  BodySpring *bs;
  int a, b, akku_count;
  float min, max, akku;

  if (sb == NULL) {
    return; /* paranoia check */
  }

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    bp->colball = 0;
    akku = 0.0f;
    akku_count = 0;
    min = 1e22f;
    max = -1e22f;
    /* first estimation based on attached */
    for (b = bp->nofsprings; b > 0; b--) {
      bs = sb->bspring + bp->springs[b - 1];
      if (bs->springtype == SB_EDGE) {
        akku += bs->len;
        akku_count++;
        min = min_ff(bs->len, min);
        max = max_ff(bs->len, max);
      }
    }

    if (akku_count > 0) {
      if (sb->sbc_mode == SBC_MODE_MANUAL) {
        bp->colball = sb->colball;
      }
      if (sb->sbc_mode == SBC_MODE_AVG) {
        bp->colball = akku / (float)akku_count * sb->colball;
      }
      if (sb->sbc_mode == SBC_MODE_MIN) {
        bp->colball = min * sb->colball;
      }
      if (sb->sbc_mode == SBC_MODE_MAX) {
        bp->colball = max * sb->colball;
      }
      if (sb->sbc_mode == SBC_MODE_AVGMINMAX) {
        bp->colball = (min + max) / 2.0f * sb->colball;
      }
    }
    else {
      bp->colball = 0;
    }
  } /*for bp*/
}

/* creates new softbody if didn't exist yet, makes new points and springs arrays */
static void renew_softbody(Scene *scene, Object *ob, int totpoint, int totspring)
{
  SoftBody *sb;
  int i;
  short softflag;
  if (ob->soft == NULL) {
    ob->soft = sbNew(scene);
  }
  else {
    free_softbody_intern(ob->soft);
  }
  sb = ob->soft;
  softflag = ob->softflag;

  if (totpoint) {
    sb->totpoint = totpoint;
    sb->totspring = totspring;

    sb->bpoint = MEM_mallocN(totpoint * sizeof(BodyPoint), "bodypoint");
    if (totspring) {
      sb->bspring = MEM_mallocN(totspring * sizeof(BodySpring), "bodyspring");
    }

    /* initialize BodyPoint array */
    for (i = 0; i < totpoint; i++) {
      BodyPoint *bp = &sb->bpoint[i];

      /* hum as far as i see this is overridden by _final_goal() now jow_go_for2_5 */
      /* sadly breaks compatibility with older versions */
      /* but makes goals behave the same for meshes, lattices and curves */
      if (softflag & OB_SB_GOAL) {
        bp->goal = sb->defgoal;
      }
      else {
        bp->goal = 0.0f;
        /* so this will definily be below SOFTGOALSNAP */
      }

      bp->nofsprings = 0;
      bp->springs = NULL;
      bp->choke = 0.0f;
      bp->choke2 = 0.0f;
      bp->frozen = 1.0f;
      bp->colball = 0.0f;
      bp->loc_flag = 0;
      bp->springweight = 1.0f;
      bp->mass = 1.0f;
    }
  }
}

static void free_softbody_baked(SoftBody *sb)
{
  SBVertex *key;
  int k;

  for (k = 0; k < sb->totkey; k++) {
    key = *(sb->keys + k);
    if (key) {
      MEM_freeN(key);
    }
  }
  if (sb->keys) {
    MEM_freeN(sb->keys);
  }

  sb->keys = NULL;
  sb->totkey = 0;
}
static void free_scratch(SoftBody *sb)
{
  if (sb->scratch) {
    /* todo make sure everything is cleaned up nicly */
    if (sb->scratch->colliderhash) {
      BLI_ghash_free(sb->scratch->colliderhash,
                     NULL,
                     (GHashValFreeFP)ccd_mesh_free); /*this hoepfully will free all caches*/
      sb->scratch->colliderhash = NULL;
    }
    if (sb->scratch->bodyface) {
      MEM_freeN(sb->scratch->bodyface);
    }
    if (sb->scratch->Ref.ivert) {
      MEM_freeN(sb->scratch->Ref.ivert);
    }
    MEM_freeN(sb->scratch);
    sb->scratch = NULL;
  }
}

/* only frees internal data */
static void free_softbody_intern(SoftBody *sb)
{
  if (sb) {
    int a;
    BodyPoint *bp;

    if (sb->bpoint) {
      for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
        /* free spring list */
        if (bp->springs != NULL) {
          MEM_freeN(bp->springs);
        }
      }
      MEM_freeN(sb->bpoint);
    }

    if (sb->bspring) {
      MEM_freeN(sb->bspring);
    }

    sb->totpoint = sb->totspring = 0;
    sb->bpoint = NULL;
    sb->bspring = NULL;

    free_scratch(sb);
    free_softbody_baked(sb);
  }
}

/* ************ dynamics ********** */

/* the most general (micro physics correct) way to do collision
 * (only needs the current particle position)
 *
 * it actually checks if the particle intrudes a short range force field generated
 * by the faces of the target object and returns a force to drive the particel out
 * the strength of the field grows exponentially if the particle is on the 'wrong' side of the face
 * 'wrong' side : projection to the face normal is negative (all referred to a vertex in the face)
 *
 * flaw of this: 'fast' particles as well as 'fast' colliding faces
 * give a 'tunnel' effect such that the particle passes through the force field
 * without ever 'seeing' it
 * this is fully compliant to heisenberg: h >= fuzzy(location) * fuzzy(time)
 * besides our h is way larger than in QM because forces propagate way slower here
 * we have to deal with fuzzy(time) in the range of 1/25 seconds (typical frame rate)
 * yup collision targets are not known here any better
 * and 1/25 second is very long compared to real collision events
 * Q: why not use 'simple' collision here like bouncing back a particle
 *   --> reverting is velocity on the face normal
 * A: because our particles are not alone here
 *    and need to tell their neighbors exactly what happens via spring forces
 * unless sbObjectStep( .. ) is called on sub frame timing level
 * BTW that also questions the use of a 'implicit' solvers on softbodies
 * since that would only valid for 'slow' moving collision targets and dito particles
 */

/* +++ dependency information functions*/

/**
 * \note collection overrides scene when not NULL.
 */
static int query_external_colliders(Depsgraph *depsgraph, Collection *collection)
{
  unsigned int numobjects;
  Object **objects = BKE_collision_objects_create(
      depsgraph, NULL, collection, &numobjects, eModifierType_Collision);
  BKE_collision_objects_free(objects);

  return (numobjects != 0);
}
/* --- dependency information functions*/

/* +++ the aabb "force" section*/
static int sb_detect_aabb_collisionCached(float UNUSED(force[3]),
                                          struct Object *vertexowner,
                                          float UNUSED(time))
{
  Object *ob;
  SoftBody *sb = vertexowner->soft;
  GHash *hash;
  GHashIterator *ihash;
  float aabbmin[3], aabbmax[3];
  int deflected = 0;
#if 0
  int a;
#endif

  if ((sb == NULL) || (sb->scratch == NULL)) {
    return 0;
  }
  copy_v3_v3(aabbmin, sb->scratch->aabbmin);
  copy_v3_v3(aabbmax, sb->scratch->aabbmax);

  hash = vertexowner->soft->scratch->colliderhash;
  ihash = BLI_ghashIterator_new(hash);
  while (!BLI_ghashIterator_done(ihash)) {

    ccd_Mesh *ccdm = BLI_ghashIterator_getValue(ihash);
    ob = BLI_ghashIterator_getKey(ihash);
    {
      /* only with deflecting set */
      if (ob->pd && ob->pd->deflect) {
        if (ccdm) {
          if ((aabbmax[0] < ccdm->bbmin[0]) || (aabbmax[1] < ccdm->bbmin[1]) ||
              (aabbmax[2] < ccdm->bbmin[2]) || (aabbmin[0] > ccdm->bbmax[0]) ||
              (aabbmin[1] > ccdm->bbmax[1]) || (aabbmin[2] > ccdm->bbmax[2])) {
            /* boxes don't intersect */
            BLI_ghashIterator_step(ihash);
            continue;
          }

          /* so now we have the 2 boxes overlapping */
          /* forces actually not used */
          deflected = 2;
        }
        else {
          /*aye that should be cached*/
          CLOG_ERROR(&LOG, "missing cache error");
          BLI_ghashIterator_step(ihash);
          continue;
        }
      } /* if (ob->pd && ob->pd->deflect) */
      BLI_ghashIterator_step(ihash);
    }
  } /* while () */
  BLI_ghashIterator_free(ihash);
  return deflected;
}
/* --- the aabb section*/

/* +++ the face external section*/
static int sb_detect_face_pointCached(float face_v1[3],
                                      const float face_v2[3],
                                      const float face_v3[3],
                                      float *damp,
                                      float force[3],
                                      struct Object *vertexowner,
                                      float time)
{
  Object *ob;
  GHash *hash;
  GHashIterator *ihash;
  float nv1[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3], aabbmax[3];
  float facedist, outerfacethickness, tune = 10.f;
  int a, deflected = 0;

  aabbmin[0] = min_fff(face_v1[0], face_v2[0], face_v3[0]);
  aabbmin[1] = min_fff(face_v1[1], face_v2[1], face_v3[1]);
  aabbmin[2] = min_fff(face_v1[2], face_v2[2], face_v3[2]);
  aabbmax[0] = max_fff(face_v1[0], face_v2[0], face_v3[0]);
  aabbmax[1] = max_fff(face_v1[1], face_v2[1], face_v3[1]);
  aabbmax[2] = max_fff(face_v1[2], face_v2[2], face_v3[2]);

  /* calculate face normal once again SIGH */
  sub_v3_v3v3(edge1, face_v1, face_v2);
  sub_v3_v3v3(edge2, face_v3, face_v2);
  cross_v3_v3v3(d_nvect, edge2, edge1);
  normalize_v3(d_nvect);

  hash = vertexowner->soft->scratch->colliderhash;
  ihash = BLI_ghashIterator_new(hash);
  while (!BLI_ghashIterator_done(ihash)) {

    ccd_Mesh *ccdm = BLI_ghashIterator_getValue(ihash);
    ob = BLI_ghashIterator_getKey(ihash);
    {
      /* only with deflecting set */
      if (ob->pd && ob->pd->deflect) {
        const MVert *mvert = NULL;
        const MVert *mprevvert = NULL;
        if (ccdm) {
          mvert = ccdm->mvert;
          a = ccdm->mvert_num;
          mprevvert = ccdm->mprevvert;
          outerfacethickness = ob->pd->pdef_sboft;
          if ((aabbmax[0] < ccdm->bbmin[0]) || (aabbmax[1] < ccdm->bbmin[1]) ||
              (aabbmax[2] < ccdm->bbmin[2]) || (aabbmin[0] > ccdm->bbmax[0]) ||
              (aabbmin[1] > ccdm->bbmax[1]) || (aabbmin[2] > ccdm->bbmax[2])) {
            /* boxes don't intersect */
            BLI_ghashIterator_step(ihash);
            continue;
          }
        }
        else {
          /*aye that should be cached*/
          CLOG_ERROR(&LOG, "missing cache error");
          BLI_ghashIterator_step(ihash);
          continue;
        }

        /* use mesh*/
        if (mvert) {
          while (a) {
            copy_v3_v3(nv1, mvert[a - 1].co);
            if (mprevvert) {
              mul_v3_fl(nv1, time);
              madd_v3_v3fl(nv1, mprevvert[a - 1].co, 1.0f - time);
            }
            /* origin to face_v2*/
            sub_v3_v3(nv1, face_v2);
            facedist = dot_v3v3(nv1, d_nvect);
            if (fabsf(facedist) < outerfacethickness) {
              if (isect_point_tri_prism_v3(nv1, face_v1, face_v2, face_v3)) {
                float df;
                if (facedist > 0) {
                  df = (outerfacethickness - facedist) / outerfacethickness;
                }
                else {
                  df = (outerfacethickness + facedist) / outerfacethickness;
                }

                *damp = df * tune * ob->pd->pdef_sbdamp;

                df = 0.01f * expf(-100.0f * df);
                madd_v3_v3fl(force, d_nvect, -df);
                deflected = 3;
              }
            }
            a--;
          } /* while (a)*/
        }   /* if (mvert) */
      }     /* if (ob->pd && ob->pd->deflect) */
      BLI_ghashIterator_step(ihash);
    }
  } /* while () */
  BLI_ghashIterator_free(ihash);
  return deflected;
}

static int sb_detect_face_collisionCached(float face_v1[3],
                                          const float face_v2[3],
                                          const float face_v3[3],
                                          float *damp,
                                          float force[3],
                                          struct Object *vertexowner,
                                          float time)
{
  Object *ob;
  GHash *hash;
  GHashIterator *ihash;
  float nv1[3], nv2[3], nv3[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3], aabbmax[3];
  float t, tune = 10.0f;
  int a, deflected = 0;

  aabbmin[0] = min_fff(face_v1[0], face_v2[0], face_v3[0]);
  aabbmin[1] = min_fff(face_v1[1], face_v2[1], face_v3[1]);
  aabbmin[2] = min_fff(face_v1[2], face_v2[2], face_v3[2]);
  aabbmax[0] = max_fff(face_v1[0], face_v2[0], face_v3[0]);
  aabbmax[1] = max_fff(face_v1[1], face_v2[1], face_v3[1]);
  aabbmax[2] = max_fff(face_v1[2], face_v2[2], face_v3[2]);

  hash = vertexowner->soft->scratch->colliderhash;
  ihash = BLI_ghashIterator_new(hash);
  while (!BLI_ghashIterator_done(ihash)) {

    ccd_Mesh *ccdm = BLI_ghashIterator_getValue(ihash);
    ob = BLI_ghashIterator_getKey(ihash);
    {
      /* only with deflecting set */
      if (ob->pd && ob->pd->deflect) {
        const MVert *mvert = NULL;
        const MVert *mprevvert = NULL;
        const MVertTri *vt = NULL;
        const ccdf_minmax *mima = NULL;

        if (ccdm) {
          mvert = ccdm->mvert;
          vt = ccdm->tri;
          mprevvert = ccdm->mprevvert;
          mima = ccdm->mima;
          a = ccdm->tri_num;

          if ((aabbmax[0] < ccdm->bbmin[0]) || (aabbmax[1] < ccdm->bbmin[1]) ||
              (aabbmax[2] < ccdm->bbmin[2]) || (aabbmin[0] > ccdm->bbmax[0]) ||
              (aabbmin[1] > ccdm->bbmax[1]) || (aabbmin[2] > ccdm->bbmax[2])) {
            /* boxes don't intersect */
            BLI_ghashIterator_step(ihash);
            continue;
          }
        }
        else {
          /*aye that should be cached*/
          CLOG_ERROR(&LOG, "missing cache error");
          BLI_ghashIterator_step(ihash);
          continue;
        }

        /* use mesh*/
        while (a--) {
          if ((aabbmax[0] < mima->minx) || (aabbmin[0] > mima->maxx) ||
              (aabbmax[1] < mima->miny) || (aabbmin[1] > mima->maxy) ||
              (aabbmax[2] < mima->minz) || (aabbmin[2] > mima->maxz)) {
            mima++;
            vt++;
            continue;
          }

          if (mvert) {

            copy_v3_v3(nv1, mvert[vt->tri[0]].co);
            copy_v3_v3(nv2, mvert[vt->tri[1]].co);
            copy_v3_v3(nv3, mvert[vt->tri[2]].co);

            if (mprevvert) {
              mul_v3_fl(nv1, time);
              madd_v3_v3fl(nv1, mprevvert[vt->tri[0]].co, 1.0f - time);

              mul_v3_fl(nv2, time);
              madd_v3_v3fl(nv2, mprevvert[vt->tri[1]].co, 1.0f - time);

              mul_v3_fl(nv3, time);
              madd_v3_v3fl(nv3, mprevvert[vt->tri[2]].co, 1.0f - time);
            }
          }

          /* switch origin to be nv2*/
          sub_v3_v3v3(edge1, nv1, nv2);
          sub_v3_v3v3(edge2, nv3, nv2);
          cross_v3_v3v3(d_nvect, edge2, edge1);
          normalize_v3(d_nvect);
          if (isect_line_segment_tri_v3(nv1, nv2, face_v1, face_v2, face_v3, &t, NULL) ||
              isect_line_segment_tri_v3(nv2, nv3, face_v1, face_v2, face_v3, &t, NULL) ||
              isect_line_segment_tri_v3(nv3, nv1, face_v1, face_v2, face_v3, &t, NULL)) {
            madd_v3_v3fl(force, d_nvect, -0.5f);
            *damp = tune * ob->pd->pdef_sbdamp;
            deflected = 2;
          }
          mima++;
          vt++;
        } /* while a */
      }   /* if (ob->pd && ob->pd->deflect) */
      BLI_ghashIterator_step(ihash);
    }
  } /* while () */
  BLI_ghashIterator_free(ihash);
  return deflected;
}

static void scan_for_ext_face_forces(Object *ob, float timenow)
{
  SoftBody *sb = ob->soft;
  BodyFace *bf;
  int a;
  float damp = 0.0f, choke = 1.0f;
  float tune = -10.0f;
  float feedback[3];

  if (sb && sb->scratch->totface) {

    bf = sb->scratch->bodyface;
    for (a = 0; a < sb->scratch->totface; a++, bf++) {
      bf->ext_force[0] = bf->ext_force[1] = bf->ext_force[2] = 0.0f;
      /*+++edges intruding*/
      bf->flag &= ~BFF_INTERSECT;
      zero_v3(feedback);
      if (sb_detect_face_collisionCached(sb->bpoint[bf->v1].pos,
                                         sb->bpoint[bf->v2].pos,
                                         sb->bpoint[bf->v3].pos,
                                         &damp,
                                         feedback,
                                         ob,
                                         timenow)) {
        madd_v3_v3fl(sb->bpoint[bf->v1].force, feedback, tune);
        madd_v3_v3fl(sb->bpoint[bf->v2].force, feedback, tune);
        madd_v3_v3fl(sb->bpoint[bf->v3].force, feedback, tune);
        //              madd_v3_v3fl(bf->ext_force, feedback, tune);
        bf->flag |= BFF_INTERSECT;
        choke = min_ff(max_ff(damp, choke), 1.0f);
      }
      /*---edges intruding*/

      /*+++ close vertices*/
      if ((bf->flag & BFF_INTERSECT) == 0) {
        bf->flag &= ~BFF_CLOSEVERT;
        tune = -1.0f;
        zero_v3(feedback);
        if (sb_detect_face_pointCached(sb->bpoint[bf->v1].pos,
                                       sb->bpoint[bf->v2].pos,
                                       sb->bpoint[bf->v3].pos,
                                       &damp,
                                       feedback,
                                       ob,
                                       timenow)) {
          madd_v3_v3fl(sb->bpoint[bf->v1].force, feedback, tune);
          madd_v3_v3fl(sb->bpoint[bf->v2].force, feedback, tune);
          madd_v3_v3fl(sb->bpoint[bf->v3].force, feedback, tune);
          //                  madd_v3_v3fl(bf->ext_force, feedback, tune);
          bf->flag |= BFF_CLOSEVERT;
          choke = min_ff(max_ff(damp, choke), 1.0f);
        }
      }
      /*--- close vertices*/
    }
    bf = sb->scratch->bodyface;
    for (a = 0; a < sb->scratch->totface; a++, bf++) {
      if ((bf->flag & BFF_INTERSECT) || (bf->flag & BFF_CLOSEVERT)) {
        sb->bpoint[bf->v1].choke2 = max_ff(sb->bpoint[bf->v1].choke2, choke);
        sb->bpoint[bf->v2].choke2 = max_ff(sb->bpoint[bf->v2].choke2, choke);
        sb->bpoint[bf->v3].choke2 = max_ff(sb->bpoint[bf->v3].choke2, choke);
      }
    }
  }
}

/*  --- the face external section*/

/* +++ the spring external section*/

static int sb_detect_edge_collisionCached(float edge_v1[3],
                                          const float edge_v2[3],
                                          float *damp,
                                          float force[3],
                                          struct Object *vertexowner,
                                          float time)
{
  Object *ob;
  GHash *hash;
  GHashIterator *ihash;
  float nv1[3], nv2[3], nv3[3], edge1[3], edge2[3], d_nvect[3], aabbmin[3], aabbmax[3];
  float t, el;
  int a, deflected = 0;

  minmax_v3v3_v3(aabbmin, aabbmax, edge_v1);
  minmax_v3v3_v3(aabbmin, aabbmax, edge_v2);

  el = len_v3v3(edge_v1, edge_v2);

  hash = vertexowner->soft->scratch->colliderhash;
  ihash = BLI_ghashIterator_new(hash);
  while (!BLI_ghashIterator_done(ihash)) {

    ccd_Mesh *ccdm = BLI_ghashIterator_getValue(ihash);
    ob = BLI_ghashIterator_getKey(ihash);
    {
      /* only with deflecting set */
      if (ob->pd && ob->pd->deflect) {
        const MVert *mvert = NULL;
        const MVert *mprevvert = NULL;
        const MVertTri *vt = NULL;
        const ccdf_minmax *mima = NULL;

        if (ccdm) {
          mvert = ccdm->mvert;
          mprevvert = ccdm->mprevvert;
          vt = ccdm->tri;
          mima = ccdm->mima;
          a = ccdm->tri_num;

          if ((aabbmax[0] < ccdm->bbmin[0]) || (aabbmax[1] < ccdm->bbmin[1]) ||
              (aabbmax[2] < ccdm->bbmin[2]) || (aabbmin[0] > ccdm->bbmax[0]) ||
              (aabbmin[1] > ccdm->bbmax[1]) || (aabbmin[2] > ccdm->bbmax[2])) {
            /* boxes don't intersect */
            BLI_ghashIterator_step(ihash);
            continue;
          }
        }
        else {
          /*aye that should be cached*/
          CLOG_ERROR(&LOG, "missing cache error");
          BLI_ghashIterator_step(ihash);
          continue;
        }

        /* use mesh*/
        while (a--) {
          if ((aabbmax[0] < mima->minx) || (aabbmin[0] > mima->maxx) ||
              (aabbmax[1] < mima->miny) || (aabbmin[1] > mima->maxy) ||
              (aabbmax[2] < mima->minz) || (aabbmin[2] > mima->maxz)) {
            mima++;
            vt++;
            continue;
          }

          if (mvert) {

            copy_v3_v3(nv1, mvert[vt->tri[0]].co);
            copy_v3_v3(nv2, mvert[vt->tri[1]].co);
            copy_v3_v3(nv3, mvert[vt->tri[2]].co);

            if (mprevvert) {
              mul_v3_fl(nv1, time);
              madd_v3_v3fl(nv1, mprevvert[vt->tri[0]].co, 1.0f - time);

              mul_v3_fl(nv2, time);
              madd_v3_v3fl(nv2, mprevvert[vt->tri[1]].co, 1.0f - time);

              mul_v3_fl(nv3, time);
              madd_v3_v3fl(nv3, mprevvert[vt->tri[2]].co, 1.0f - time);
            }
          }

          /* switch origin to be nv2*/
          sub_v3_v3v3(edge1, nv1, nv2);
          sub_v3_v3v3(edge2, nv3, nv2);

          cross_v3_v3v3(d_nvect, edge2, edge1);
          normalize_v3(d_nvect);
          if (isect_line_segment_tri_v3(edge_v1, edge_v2, nv1, nv2, nv3, &t, NULL)) {
            float v1[3], v2[3];
            float intrusiondepth, i1, i2;
            sub_v3_v3v3(v1, edge_v1, nv2);
            sub_v3_v3v3(v2, edge_v2, nv2);
            i1 = dot_v3v3(v1, d_nvect);
            i2 = dot_v3v3(v2, d_nvect);
            intrusiondepth = -min_ff(i1, i2) / el;
            madd_v3_v3fl(force, d_nvect, intrusiondepth);
            *damp = ob->pd->pdef_sbdamp;
            deflected = 2;
          }

          mima++;
          vt++;
        } /* while a */
      }   /* if (ob->pd && ob->pd->deflect) */
      BLI_ghashIterator_step(ihash);
    }
  } /* while () */
  BLI_ghashIterator_free(ihash);
  return deflected;
}

static void _scan_for_ext_spring_forces(
    Scene *scene, Object *ob, float timenow, int ifirst, int ilast, struct ListBase *effectors)
{
  SoftBody *sb = ob->soft;
  int a;
  float damp;
  float feedback[3];

  if (sb && sb->totspring) {
    for (a = ifirst; a < ilast; a++) {
      BodySpring *bs = &sb->bspring[a];
      bs->ext_force[0] = bs->ext_force[1] = bs->ext_force[2] = 0.0f;
      feedback[0] = feedback[1] = feedback[2] = 0.0f;
      bs->flag &= ~BSF_INTERSECT;

      if (bs->springtype == SB_EDGE) {
        /* +++ springs colliding */
        if (ob->softflag & OB_SB_EDGECOLL) {
          if (sb_detect_edge_collisionCached(
                  sb->bpoint[bs->v1].pos, sb->bpoint[bs->v2].pos, &damp, feedback, ob, timenow)) {
            add_v3_v3(bs->ext_force, feedback);
            bs->flag |= BSF_INTERSECT;
            // bs->cf=damp;
            bs->cf = sb->choke * 0.01f;
          }
        }
        /* ---- springs colliding */

        /* +++ springs seeing wind ... n stuff depending on their orientation*/
        /* note we don't use sb->mediafrict but use sb->aeroedge for magnitude of effect*/
        if (sb->aeroedge) {
          float vel[3], sp[3], pr[3], force[3];
          float f, windfactor = 0.25f;
          /*see if we have wind*/
          if (effectors) {
            EffectedPoint epoint;
            float speed[3] = {0.0f, 0.0f, 0.0f};
            float pos[3];
            mid_v3_v3v3(pos, sb->bpoint[bs->v1].pos, sb->bpoint[bs->v2].pos);
            mid_v3_v3v3(vel, sb->bpoint[bs->v1].vec, sb->bpoint[bs->v2].vec);
            pd_point_from_soft(scene, pos, vel, -1, &epoint);
            BKE_effectors_apply(
                effectors, NULL, sb->effector_weights, &epoint, force, NULL, speed);

            mul_v3_fl(speed, windfactor);
            add_v3_v3(vel, speed);
          }
          /* media in rest */
          else {
            add_v3_v3v3(vel, sb->bpoint[bs->v1].vec, sb->bpoint[bs->v2].vec);
          }
          f = normalize_v3(vel);
          f = -0.0001f * f * f * sb->aeroedge;
          /* (todo) add a nice angle dependent function done for now BUT */
          /* still there could be some nice drag/lift function, but who needs it */

          sub_v3_v3v3(sp, sb->bpoint[bs->v1].pos, sb->bpoint[bs->v2].pos);
          project_v3_v3v3(pr, vel, sp);
          sub_v3_v3(vel, pr);
          normalize_v3(vel);
          if (ob->softflag & OB_SB_AERO_ANGLE) {
            normalize_v3(sp);
            madd_v3_v3fl(bs->ext_force, vel, f * (1.0f - fabsf(dot_v3v3(vel, sp))));
          }
          else {
            madd_v3_v3fl(bs->ext_force, vel, f);  // to keep compatible with 2.45 release files
          }
        }
        /* --- springs seeing wind */
      }
    }
  }
}

static void *exec_scan_for_ext_spring_forces(void *data)
{
  SB_thread_context *pctx = (SB_thread_context *)data;
  _scan_for_ext_spring_forces(
      pctx->scene, pctx->ob, pctx->timenow, pctx->ifirst, pctx->ilast, pctx->effectors);
  return NULL;
}

static void sb_sfesf_threads_run(struct Depsgraph *depsgraph,
                                 Scene *scene,
                                 struct Object *ob,
                                 float timenow,
                                 int totsprings,
                                 int *UNUSED(ptr_to_break_func(void)))
{
  ListBase threads;
  SB_thread_context *sb_threads;
  int i, totthread, left, dec;

  /* wild guess .. may increase with better thread management 'above'
   * or even be UI option sb->spawn_cf_threads_nopts */
  int lowsprings = 100;

  ListBase *effectors = BKE_effectors_create(depsgraph, ob, NULL, ob->soft->effector_weights);

  /* figure the number of threads while preventing pretty pointless threading overhead */
  totthread = BKE_scene_num_threads(scene);
  /* what if we got zillions of CPUs running but less to spread*/
  while ((totsprings / totthread < lowsprings) && (totthread > 1)) {
    totthread--;
  }

  sb_threads = MEM_callocN(sizeof(SB_thread_context) * totthread, "SBSpringsThread");
  memset(sb_threads, 0, sizeof(SB_thread_context) * totthread);
  left = totsprings;
  dec = totsprings / totthread + 1;
  for (i = 0; i < totthread; i++) {
    sb_threads[i].scene = scene;
    sb_threads[i].ob = ob;
    sb_threads[i].forcetime = 0.0;  // not used here
    sb_threads[i].timenow = timenow;
    sb_threads[i].ilast = left;
    left = left - dec;
    if (left > 0) {
      sb_threads[i].ifirst = left;
    }
    else {
      sb_threads[i].ifirst = 0;
    }
    sb_threads[i].effectors = effectors;
    sb_threads[i].do_deflector = false;  // not used here
    sb_threads[i].fieldfactor = 0.0f;    // not used here
    sb_threads[i].windfactor = 0.0f;     // not used here
    sb_threads[i].nr = i;
    sb_threads[i].tot = totthread;
  }
  if (totthread > 1) {
    BLI_threadpool_init(&threads, exec_scan_for_ext_spring_forces, totthread);

    for (i = 0; i < totthread; i++) {
      BLI_threadpool_insert(&threads, &sb_threads[i]);
    }

    BLI_threadpool_end(&threads);
  }
  else {
    exec_scan_for_ext_spring_forces(&sb_threads[0]);
  }
  /* clean up */
  MEM_freeN(sb_threads);

  BKE_effectors_free(effectors);
}

/* --- the spring external section*/

static int choose_winner(
    float *w, float *pos, float *a, float *b, float *c, float *ca, float *cb, float *cc)
{
  float mindist, cp;
  int winner = 1;
  mindist = fabsf(dot_v3v3(pos, a));

  cp = fabsf(dot_v3v3(pos, b));
  if (mindist < cp) {
    mindist = cp;
    winner = 2;
  }

  cp = fabsf(dot_v3v3(pos, c));
  if (mindist < cp) {
    mindist = cp;
    winner = 3;
  }
  switch (winner) {
    case 1:
      copy_v3_v3(w, ca);
      break;
    case 2:
      copy_v3_v3(w, cb);
      break;
    case 3:
      copy_v3_v3(w, cc);
  }
  return (winner);
}

static int sb_detect_vertex_collisionCached(float opco[3],
                                            float facenormal[3],
                                            float *damp,
                                            float force[3],
                                            struct Object *vertexowner,
                                            float time,
                                            float vel[3],
                                            float *intrusion)
{
  Object *ob = NULL;
  GHash *hash;
  GHashIterator *ihash;
  float nv1[3], nv2[3], nv3[3], edge1[3], edge2[3], d_nvect[3], dv1[3], ve[3],
      avel[3] = {0.0, 0.0, 0.0}, vv1[3], vv2[3], vv3[3], coledge[3] = {0.0f, 0.0f, 0.0f},
      mindistedge = 1000.0f, outerforceaccu[3], innerforceaccu[3], facedist,
      /* n_mag, */ /* UNUSED */ force_mag_norm, minx, miny, minz, maxx, maxy, maxz,
      innerfacethickness = -0.5f, outerfacethickness = 0.2f, ee = 5.0f, ff = 0.1f, fa = 1;
  int a, deflected = 0, cavel = 0, ci = 0;
  /* init */
  *intrusion = 0.0f;
  hash = vertexowner->soft->scratch->colliderhash;
  ihash = BLI_ghashIterator_new(hash);
  outerforceaccu[0] = outerforceaccu[1] = outerforceaccu[2] = 0.0f;
  innerforceaccu[0] = innerforceaccu[1] = innerforceaccu[2] = 0.0f;
  /* go */
  while (!BLI_ghashIterator_done(ihash)) {

    ccd_Mesh *ccdm = BLI_ghashIterator_getValue(ihash);
    ob = BLI_ghashIterator_getKey(ihash);
    {
      /* only with deflecting set */
      if (ob->pd && ob->pd->deflect) {
        const MVert *mvert = NULL;
        const MVert *mprevvert = NULL;
        const MVertTri *vt = NULL;
        const ccdf_minmax *mima = NULL;

        if (ccdm) {
          mvert = ccdm->mvert;
          mprevvert = ccdm->mprevvert;
          vt = ccdm->tri;
          mima = ccdm->mima;
          a = ccdm->tri_num;

          minx = ccdm->bbmin[0];
          miny = ccdm->bbmin[1];
          minz = ccdm->bbmin[2];

          maxx = ccdm->bbmax[0];
          maxy = ccdm->bbmax[1];
          maxz = ccdm->bbmax[2];

          if ((opco[0] < minx) || (opco[1] < miny) || (opco[2] < minz) || (opco[0] > maxx) ||
              (opco[1] > maxy) || (opco[2] > maxz)) {
            /* outside the padded boundbox --> collision object is too far away */
            BLI_ghashIterator_step(ihash);
            continue;
          }
        }
        else {
          /*aye that should be cached*/
          CLOG_ERROR(&LOG, "missing cache error");
          BLI_ghashIterator_step(ihash);
          continue;
        }

        /* do object level stuff */
        /* need to have user control for that since it depends on model scale */
        innerfacethickness = -ob->pd->pdef_sbift;
        outerfacethickness = ob->pd->pdef_sboft;
        fa = (ff * outerfacethickness - outerfacethickness);
        fa *= fa;
        fa = 1.0f / fa;
        avel[0] = avel[1] = avel[2] = 0.0f;
        /* use mesh*/
        while (a--) {
          if ((opco[0] < mima->minx) || (opco[0] > mima->maxx) || (opco[1] < mima->miny) ||
              (opco[1] > mima->maxy) || (opco[2] < mima->minz) || (opco[2] > mima->maxz)) {
            mima++;
            vt++;
            continue;
          }

          if (mvert) {

            copy_v3_v3(nv1, mvert[vt->tri[0]].co);
            copy_v3_v3(nv2, mvert[vt->tri[1]].co);
            copy_v3_v3(nv3, mvert[vt->tri[2]].co);

            if (mprevvert) {
              /* Grab the average speed of the collider vertices before we spoil nvX
               * humm could be done once a SB steps but then we' need to store that too
               * since the AABB reduced probability to get here drastically
               * it might be a nice tradeoff CPU <--> memory.
               */
              sub_v3_v3v3(vv1, nv1, mprevvert[vt->tri[0]].co);
              sub_v3_v3v3(vv2, nv2, mprevvert[vt->tri[1]].co);
              sub_v3_v3v3(vv3, nv3, mprevvert[vt->tri[2]].co);

              mul_v3_fl(nv1, time);
              madd_v3_v3fl(nv1, mprevvert[vt->tri[0]].co, 1.0f - time);

              mul_v3_fl(nv2, time);
              madd_v3_v3fl(nv2, mprevvert[vt->tri[1]].co, 1.0f - time);

              mul_v3_fl(nv3, time);
              madd_v3_v3fl(nv3, mprevvert[vt->tri[2]].co, 1.0f - time);
            }
          }

          /* switch origin to be nv2*/
          sub_v3_v3v3(edge1, nv1, nv2);
          sub_v3_v3v3(edge2, nv3, nv2);
          /* Abuse dv1 to have vertex in question at *origin* of triangle. */
          sub_v3_v3v3(dv1, opco, nv2);

          cross_v3_v3v3(d_nvect, edge2, edge1);
          /* n_mag = */ /* UNUSED */ normalize_v3(d_nvect);
          facedist = dot_v3v3(dv1, d_nvect);
          // so rules are
          //

          if ((facedist > innerfacethickness) && (facedist < outerfacethickness)) {
            if (isect_point_tri_prism_v3(opco, nv1, nv2, nv3)) {
              force_mag_norm = (float)exp(-ee * facedist);
              if (facedist > outerfacethickness * ff) {
                force_mag_norm = (float)force_mag_norm * fa * (facedist - outerfacethickness) *
                                 (facedist - outerfacethickness);
              }
              *damp = ob->pd->pdef_sbdamp;
              if (facedist > 0.0f) {
                *damp *= (1.0f - facedist / outerfacethickness);
                madd_v3_v3fl(outerforceaccu, d_nvect, force_mag_norm);
                deflected = 3;
              }
              else {
                madd_v3_v3fl(innerforceaccu, d_nvect, force_mag_norm);
                if (deflected < 2) {
                  deflected = 2;
                }
              }
              if ((mprevvert) && (*damp > 0.0f)) {
                choose_winner(ve, opco, nv1, nv2, nv3, vv1, vv2, vv3);
                add_v3_v3(avel, ve);
                cavel++;
              }
              *intrusion += facedist;
              ci++;
            }
          }

          mima++;
          vt++;
        } /* while a */
      }   /* if (ob->pd && ob->pd->deflect) */
      BLI_ghashIterator_step(ihash);
    }
  } /* while () */

  if (deflected == 1) {  // no face but 'outer' edge cylinder sees vert
    force_mag_norm = (float)exp(-ee * mindistedge);
    if (mindistedge > outerfacethickness * ff) {
      force_mag_norm = (float)force_mag_norm * fa * (mindistedge - outerfacethickness) *
                       (mindistedge - outerfacethickness);
    }
    madd_v3_v3fl(force, coledge, force_mag_norm);
    *damp = ob->pd->pdef_sbdamp;
    if (mindistedge > 0.0f) {
      *damp *= (1.0f - mindistedge / outerfacethickness);
    }
  }
  if (deflected == 2) {  //  face inner detected
    add_v3_v3(force, innerforceaccu);
  }
  if (deflected == 3) {  //  face outer detected
    add_v3_v3(force, outerforceaccu);
  }

  BLI_ghashIterator_free(ihash);
  if (cavel) {
    mul_v3_fl(avel, 1.0f / (float)cavel);
  }
  copy_v3_v3(vel, avel);
  if (ci) {
    *intrusion /= ci;
  }
  if (deflected) {
    normalize_v3_v3(facenormal, force);
  }
  return deflected;
}

/* sandbox to plug in various deflection algos */
static int sb_deflect_face(Object *ob,
                           float *actpos,
                           float *facenormal,
                           float *force,
                           float *cf,
                           float time,
                           float *vel,
                           float *intrusion)
{
  float s_actpos[3];
  int deflected;
  copy_v3_v3(s_actpos, actpos);
  deflected = sb_detect_vertex_collisionCached(
      s_actpos, facenormal, cf, force, ob, time, vel, intrusion);
#if 0
  deflected = sb_detect_vertex_collisionCachedEx(
      s_actpos, facenormal, cf, force, ob, time, vel, intrusion);
#endif
  return (deflected);
}

/* hiding this for now .. but the jacobian may pop up on other tasks .. so i'd like to keep it */
#if 0
static void dfdx_spring(int ia, int ic, int op, float dir[3], float L, float len, float factor)
{
  float m, delta_ij;
  int i, j;
  if (L < len) {
    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
        delta_ij = (i == j ? (1.0f) : (0.0f));
        m = factor * (dir[i] * dir[j] + (1 - L / len) * (delta_ij - dir[i] * dir[j]));
        EIG_linear_solver_matrix_add(ia + i, op + ic + j, m);
      }
    }
  }
  else {
    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
        m = factor * dir[i] * dir[j];
        EIG_linear_solver_matrix_add(ia + i, op + ic + j, m);
      }
    }
  }
}

static void dfdx_goal(int ia, int ic, int op, float factor)
{
  int i;
  for (i = 0; i < 3; i++) {
    EIG_linear_solver_matrix_add(ia + i, op + ic + i, factor);
  }
}

static void dfdv_goal(int ia, int ic, float factor)
{
  int i;
  for (i = 0; i < 3; i++) {
    EIG_linear_solver_matrix_add(ia + i, ic + i, factor);
  }
}
#endif /* if 0 */

static void sb_spring_force(
    Object *ob, int bpi, BodySpring *bs, float iks, float UNUSED(forcetime))
{
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp1, *bp2;

  float dir[3], dvel[3];
  float distance, forcefactor, kd, absvel, projvel, kw;
#if 0 /* UNUSED */
  int ia, ic;
#endif
  /* prepare depending on which side of the spring we are on */
  if (bpi == bs->v1) {
    bp1 = &sb->bpoint[bs->v1];
    bp2 = &sb->bpoint[bs->v2];
#if 0 /* UNUSED */
    ia = 3 * bs->v1;
    ic = 3 * bs->v2;
#endif
  }
  else if (bpi == bs->v2) {
    bp1 = &sb->bpoint[bs->v2];
    bp2 = &sb->bpoint[bs->v1];
#if 0 /* UNUSED */
    ia = 3 * bs->v2;
    ic = 3 * bs->v1;
#endif
  }
  else {
    /* TODO make this debug option */
    CLOG_WARN(&LOG, "bodypoint <bpi> is not attached to spring  <*bs>");
    return;
  }

  /* do bp1 <--> bp2 elastic */
  sub_v3_v3v3(dir, bp1->pos, bp2->pos);
  distance = normalize_v3(dir);
  if (bs->len < distance) {
    iks = 1.0f / (1.0f - sb->inspring) - 1.0f; /* inner spring constants function */
  }
  else {
    iks = 1.0f / (1.0f - sb->inpush) - 1.0f; /* inner spring constants function */
  }

  if (bs->len > 0.0f) { /* check for degenerated springs */
    forcefactor = iks / bs->len;
  }
  else {
    forcefactor = iks;
  }
  kw = (bp1->springweight + bp2->springweight) / 2.0f;
  kw = kw * kw;
  kw = kw * kw;
  switch (bs->springtype) {
    case SB_EDGE:
    case SB_HANDLE:
      forcefactor *= kw;
      break;
    case SB_BEND:
      forcefactor *= sb->secondspring * kw;
      break;
    case SB_STIFFQUAD:
      forcefactor *= sb->shearstiff * sb->shearstiff * kw;
      break;
    default:
      break;
  }

  madd_v3_v3fl(bp1->force, dir, (bs->len - distance) * forcefactor);

  /* do bp1 <--> bp2 viscous */
  sub_v3_v3v3(dvel, bp1->vec, bp2->vec);
  kd = sb->infrict * sb_fric_force_scale(ob);
  absvel = normalize_v3(dvel);
  projvel = dot_v3v3(dir, dvel);
  kd *= absvel * projvel;
  madd_v3_v3fl(bp1->force, dir, -kd);
}

/* since this is definitely the most CPU consuming task here .. try to spread it */
/* core function _softbody_calc_forces_slice_in_a_thread */
/* result is int to be able to flag user break */
static int _softbody_calc_forces_slice_in_a_thread(Scene *scene,
                                                   Object *ob,
                                                   float forcetime,
                                                   float timenow,
                                                   int ifirst,
                                                   int ilast,
                                                   int *UNUSED(ptr_to_break_func(void)),
                                                   ListBase *effectors,
                                                   int do_deflector,
                                                   float fieldfactor,
                                                   float windfactor)
{
  float iks;
  int bb, do_selfcollision, do_springcollision, do_aero;
  int number_of_points_here = ilast - ifirst;
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp;

  /* initialize */
  if (sb) {
    /* check conditions for various options */
    /* +++ could be done on object level to squeeze out the last bits of it */
    do_selfcollision = ((ob->softflag & OB_SB_EDGES) && (sb->bspring) &&
                        (ob->softflag & OB_SB_SELF));
    do_springcollision = do_deflector && (ob->softflag & OB_SB_EDGES) &&
                         (ob->softflag & OB_SB_EDGECOLL);
    do_aero = ((sb->aeroedge) && (ob->softflag & OB_SB_EDGES));
    /* --- could be done on object level to squeeze out the last bits of it */
  }
  else {
    CLOG_ERROR(&LOG, "expected a SB here");
    return (999);
  }

  /* debugerin */
  if (sb->totpoint < ifirst) {
    printf("Aye 998");
    return (998);
  }
  /* debugerin */

  bp = &sb->bpoint[ifirst];
  for (bb = number_of_points_here; bb > 0; bb--, bp++) {
    /* clear forces  accumulator */
    bp->force[0] = bp->force[1] = bp->force[2] = 0.0;
    /* naive ball self collision */
    /* needs to be done if goal snaps or not */
    if (do_selfcollision) {
      int attached;
      BodyPoint *obp;
      BodySpring *bs;
      int c, b;
      float velcenter[3], dvel[3], def[3];
      float distance;
      float compare;
      float bstune = sb->ballstiff;

      /* Running in a slice we must not assume anything done with obp
       * neither alter the data of obp. */
      for (c = sb->totpoint, obp = sb->bpoint; c > 0; c--, obp++) {
        compare = (obp->colball + bp->colball);
        sub_v3_v3v3(def, bp->pos, obp->pos);
        /* rather check the AABBoxes before ever calculating the real distance */
        /* mathematically it is completely nuts, but performance is pretty much (3) times faster */
        if ((fabsf(def[0]) > compare) || (fabsf(def[1]) > compare) || (fabsf(def[2]) > compare)) {
          continue;
        }
        distance = normalize_v3(def);
        if (distance < compare) {
          /* exclude body points attached with a spring */
          attached = 0;
          for (b = obp->nofsprings; b > 0; b--) {
            bs = sb->bspring + obp->springs[b - 1];
            if ((ilast - bb == bs->v2) || (ilast - bb == bs->v1)) {
              attached = 1;
              continue;
            }
          }
          if (!attached) {
            float f = bstune / (distance) + bstune / (compare * compare) * distance -
                      2.0f * bstune / compare;

            mid_v3_v3v3(velcenter, bp->vec, obp->vec);
            sub_v3_v3v3(dvel, velcenter, bp->vec);
            mul_v3_fl(dvel, _final_mass(ob, bp));

            madd_v3_v3fl(bp->force, def, f * (1.0f - sb->balldamp));
            madd_v3_v3fl(bp->force, dvel, sb->balldamp);
          }
        }
      }
    }
    /* naive ball self collision done */

    if (_final_goal(ob, bp) < SOFTGOALSNAP) { /* omit this bp when it snaps */
      float auxvect[3];
      float velgoal[3];

      /* do goal stuff */
      if (ob->softflag & OB_SB_GOAL) {
        /* true elastic goal */
        float ks, kd;
        sub_v3_v3v3(auxvect, bp->pos, bp->origT);
        ks = 1.0f / (1.0f - _final_goal(ob, bp) * sb->goalspring) - 1.0f;
        bp->force[0] += -ks * (auxvect[0]);
        bp->force[1] += -ks * (auxvect[1]);
        bp->force[2] += -ks * (auxvect[2]);

        /* calculate damping forces generated by goals*/
        sub_v3_v3v3(velgoal, bp->origS, bp->origE);
        kd = sb->goalfrict * sb_fric_force_scale(ob);
        add_v3_v3v3(auxvect, velgoal, bp->vec);

        if (forcetime >
            0.0f) { /* make sure friction does not become rocket motor on time reversal */
          bp->force[0] -= kd * (auxvect[0]);
          bp->force[1] -= kd * (auxvect[1]);
          bp->force[2] -= kd * (auxvect[2]);
        }
        else {
          bp->force[0] -= kd * (velgoal[0] - bp->vec[0]);
          bp->force[1] -= kd * (velgoal[1] - bp->vec[1]);
          bp->force[2] -= kd * (velgoal[2] - bp->vec[2]);
        }
      }
      /* done goal stuff */

      /* gravitation */
      if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
        float gravity[3];
        copy_v3_v3(gravity, scene->physics_settings.gravity);

        /* Individual mass of node here. */
        mul_v3_fl(gravity,
                  sb_grav_force_scale(ob) * _final_mass(ob, bp) *
                      sb->effector_weights->global_gravity);

        add_v3_v3(bp->force, gravity);
      }

      /* particle field & vortex */
      if (effectors) {
        EffectedPoint epoint;
        float kd;
        float force[3] = {0.0f, 0.0f, 0.0f};
        float speed[3] = {0.0f, 0.0f, 0.0f};

        /* just for calling function once */
        float eval_sb_fric_force_scale = sb_fric_force_scale(ob);

        pd_point_from_soft(scene, bp->pos, bp->vec, sb->bpoint - bp, &epoint);
        BKE_effectors_apply(effectors, NULL, sb->effector_weights, &epoint, force, NULL, speed);

        /* apply forcefield*/
        mul_v3_fl(force, fieldfactor * eval_sb_fric_force_scale);
        add_v3_v3(bp->force, force);

        /* BP friction in moving media */
        kd = sb->mediafrict * eval_sb_fric_force_scale;
        bp->force[0] -= kd * (bp->vec[0] + windfactor * speed[0] / eval_sb_fric_force_scale);
        bp->force[1] -= kd * (bp->vec[1] + windfactor * speed[1] / eval_sb_fric_force_scale);
        bp->force[2] -= kd * (bp->vec[2] + windfactor * speed[2] / eval_sb_fric_force_scale);
        /* now we'll have nice centrifugal effect for vortex */
      }
      else {
        /* BP friction in media (not) moving*/
        float kd = sb->mediafrict * sb_fric_force_scale(ob);
        /* assume it to be proportional to actual velocity */
        bp->force[0] -= bp->vec[0] * kd;
        bp->force[1] -= bp->vec[1] * kd;
        bp->force[2] -= bp->vec[2] * kd;
        /* friction in media done */
      }
      /* +++cached collision targets */
      bp->choke = 0.0f;
      bp->choke2 = 0.0f;
      bp->loc_flag &= ~SBF_DOFUZZY;
      if (do_deflector && !(bp->loc_flag & SBF_OUTOFCOLLISION)) {
        float cfforce[3], defforce[3] = {0.0f, 0.0f, 0.0f}, vel[3] = {0.0f, 0.0f, 0.0f},
                          facenormal[3], cf = 1.0f, intrusion;
        float kd = 1.0f;

        if (sb_deflect_face(ob, bp->pos, facenormal, defforce, &cf, timenow, vel, &intrusion)) {
          if (intrusion < 0.0f) {
            sb->scratch->flag |= SBF_DOFUZZY;
            bp->loc_flag |= SBF_DOFUZZY;
            bp->choke = sb->choke * 0.01f;
          }

          sub_v3_v3v3(cfforce, bp->vec, vel);
          madd_v3_v3fl(bp->force, cfforce, -cf * 50.0f);

          madd_v3_v3fl(bp->force, defforce, kd);
        }
      }
      /* ---cached collision targets */

      /* +++springs */
      iks = 1.0f / (1.0f - sb->inspring) - 1.0f; /* inner spring constants function */
      if (ob->softflag & OB_SB_EDGES) {
        if (sb->bspring) { /* spring list exists at all ? */
          int b;
          BodySpring *bs;
          for (b = bp->nofsprings; b > 0; b--) {
            bs = sb->bspring + bp->springs[b - 1];
            if (do_springcollision || do_aero) {
              add_v3_v3(bp->force, bs->ext_force);
              if (bs->flag & BSF_INTERSECT) {
                bp->choke = bs->cf;
              }
            }
            // sb_spring_force(Object *ob, int bpi, BodySpring *bs, float iks, float forcetime)
            sb_spring_force(ob, ilast - bb, bs, iks, forcetime);
          } /* loop springs */
        }   /* existing spring list */
      }     /*any edges*/
      /* ---springs */
    }       /*omit on snap */
  }         /*loop all bp's*/
  return 0; /*done fine*/
}

static void *exec_softbody_calc_forces(void *data)
{
  SB_thread_context *pctx = (SB_thread_context *)data;
  _softbody_calc_forces_slice_in_a_thread(pctx->scene,
                                          pctx->ob,
                                          pctx->forcetime,
                                          pctx->timenow,
                                          pctx->ifirst,
                                          pctx->ilast,
                                          NULL,
                                          pctx->effectors,
                                          pctx->do_deflector,
                                          pctx->fieldfactor,
                                          pctx->windfactor);
  return NULL;
}

static void sb_cf_threads_run(Scene *scene,
                              Object *ob,
                              float forcetime,
                              float timenow,
                              int totpoint,
                              int *UNUSED(ptr_to_break_func(void)),
                              struct ListBase *effectors,
                              int do_deflector,
                              float fieldfactor,
                              float windfactor)
{
  ListBase threads;
  SB_thread_context *sb_threads;
  int i, totthread, left, dec;

  /* wild guess .. may increase with better thread management 'above'
   * or even be UI option sb->spawn_cf_threads_nopts. */
  int lowpoints = 100;

  /* figure the number of threads while preventing pretty pointless threading overhead */
  totthread = BKE_scene_num_threads(scene);
  /* what if we got zillions of CPUs running but less to spread*/
  while ((totpoint / totthread < lowpoints) && (totthread > 1)) {
    totthread--;
  }

  /* printf("sb_cf_threads_run spawning %d threads\n", totthread); */

  sb_threads = MEM_callocN(sizeof(SB_thread_context) * totthread, "SBThread");
  memset(sb_threads, 0, sizeof(SB_thread_context) * totthread);
  left = totpoint;
  dec = totpoint / totthread + 1;
  for (i = 0; i < totthread; i++) {
    sb_threads[i].scene = scene;
    sb_threads[i].ob = ob;
    sb_threads[i].forcetime = forcetime;
    sb_threads[i].timenow = timenow;
    sb_threads[i].ilast = left;
    left = left - dec;
    if (left > 0) {
      sb_threads[i].ifirst = left;
    }
    else {
      sb_threads[i].ifirst = 0;
    }
    sb_threads[i].effectors = effectors;
    sb_threads[i].do_deflector = do_deflector;
    sb_threads[i].fieldfactor = fieldfactor;
    sb_threads[i].windfactor = windfactor;
    sb_threads[i].nr = i;
    sb_threads[i].tot = totthread;
  }

  if (totthread > 1) {
    BLI_threadpool_init(&threads, exec_softbody_calc_forces, totthread);

    for (i = 0; i < totthread; i++) {
      BLI_threadpool_insert(&threads, &sb_threads[i]);
    }

    BLI_threadpool_end(&threads);
  }
  else {
    exec_softbody_calc_forces(&sb_threads[0]);
  }
  /* clean up */
  MEM_freeN(sb_threads);
}

static void softbody_calc_forces(
    struct Depsgraph *depsgraph, Scene *scene, Object *ob, float forcetime, float timenow)
{
  /* rule we never alter free variables :bp->vec bp->pos in here !
   * this will ruin adaptive stepsize AKA heun! (BM)
   */
  SoftBody *sb = ob->soft; /* is supposed to be there */
  /*BodyPoint *bproot;*/   /* UNUSED */
  /* float gravity; */     /* UNUSED */
  /* float iks; */
  float fieldfactor = -1.0f, windfactor = 0.25;
  int do_deflector /*, do_selfcollision*/, do_springcollision, do_aero;

  /* gravity = sb->grav * sb_grav_force_scale(ob); */ /* UNUSED */

  /* check conditions for various options */
  do_deflector = query_external_colliders(depsgraph, sb->collision_group);
#if 0
  do_selfcollision=((ob->softflag & OB_SB_EDGES) && (sb->bspring)&& (ob->softflag & OB_SB_SELF));
#endif
  do_springcollision = do_deflector && (ob->softflag & OB_SB_EDGES) &&
                       (ob->softflag & OB_SB_EDGECOLL);
  do_aero = ((sb->aeroedge) && (ob->softflag & OB_SB_EDGES));

  /* iks  = 1.0f/(1.0f-sb->inspring)-1.0f; */ /* inner spring constants function */ /* UNUSED */
  /* bproot= sb->bpoint; */ /* need this for proper spring addressing */            /* UNUSED */

  if (do_springcollision || do_aero) {
    sb_sfesf_threads_run(depsgraph, scene, ob, timenow, sb->totspring, NULL);
  }

  /* after spring scan because it uses Effoctors too */
  ListBase *effectors = BKE_effectors_create(depsgraph, ob, NULL, sb->effector_weights);

  if (do_deflector) {
    float defforce[3];
    do_deflector = sb_detect_aabb_collisionCached(defforce, ob, timenow);
  }

  sb_cf_threads_run(scene,
                    ob,
                    forcetime,
                    timenow,
                    sb->totpoint,
                    NULL,
                    effectors,
                    do_deflector,
                    fieldfactor,
                    windfactor);

  /* finally add forces caused by face collision */
  if (ob->softflag & OB_SB_FACECOLL) {
    scan_for_ext_face_forces(ob, timenow);
  }

  /* finish matrix and solve */
  BKE_effectors_free(effectors);
}

static void softbody_apply_forces(Object *ob, float forcetime, int mode, float *err, int mid_flags)
{
  /* time evolution */
  /* actually does an explicit euler step mode == 0 */
  /* or heun ~ 2nd order runge-kutta steps, mode 1, 2 */
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp;
  float dx[3] = {0}, dv[3], aabbmin[3], aabbmax[3], cm[3] = {0.0f, 0.0f, 0.0f};
  float timeovermass /*, freezeloc=0.00001f, freezeforce=0.00000000001f*/;
  float maxerrpos = 0.0f, maxerrvel = 0.0f;
  int a, fuzzy = 0;

  forcetime *= sb_time_scale(ob);

  aabbmin[0] = aabbmin[1] = aabbmin[2] = 1e20f;
  aabbmax[0] = aabbmax[1] = aabbmax[2] = -1e20f;

  /* old one with homogeneous masses  */
  /* claim a minimum mass for vertex */
#if 0
  if (sb->nodemass > 0.009999f) {
    timeovermass = forcetime / sb->nodemass;
  }
  else {
    timeovermass = forcetime / 0.009999f;
  }
#endif

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    /* Now we have individual masses. */
    /* claim a minimum mass for vertex */
    if (_final_mass(ob, bp) > 0.009999f) {
      timeovermass = forcetime / _final_mass(ob, bp);
    }
    else {
      timeovermass = forcetime / 0.009999f;
    }

    if (_final_goal(ob, bp) < SOFTGOALSNAP) {
      /* this makes t~ = t */
      if (mid_flags & MID_PRESERVE) {
        copy_v3_v3(dx, bp->vec);
      }

      /**
       * So here is:
       * <pre>
       * (v)' = a(cceleration) =
       *     sum(F_springs)/m + gravitation + some friction forces + more forces.
       * </pre>
       *
       * The ( ... )' operator denotes derivate respective time.
       *
       * The euler step for velocity then becomes:
       * <pre>
       * v(t + dt) = v(t) + a(t) * dt
       * </pre>
       */
      mul_v3_fl(bp->force, timeovermass); /* individual mass of node here */
      /* some nasty if's to have heun in here too */
      copy_v3_v3(dv, bp->force);

      if (mode == 1) {
        copy_v3_v3(bp->prevvec, bp->vec);
        copy_v3_v3(bp->prevdv, dv);
      }

      if (mode == 2) {
        /* be optimistic and execute step */
        bp->vec[0] = bp->prevvec[0] + 0.5f * (dv[0] + bp->prevdv[0]);
        bp->vec[1] = bp->prevvec[1] + 0.5f * (dv[1] + bp->prevdv[1]);
        bp->vec[2] = bp->prevvec[2] + 0.5f * (dv[2] + bp->prevdv[2]);
        /* compare euler to heun to estimate error for step sizing */
        maxerrvel = max_ff(maxerrvel, fabsf(dv[0] - bp->prevdv[0]));
        maxerrvel = max_ff(maxerrvel, fabsf(dv[1] - bp->prevdv[1]));
        maxerrvel = max_ff(maxerrvel, fabsf(dv[2] - bp->prevdv[2]));
      }
      else {
        add_v3_v3(bp->vec, bp->force);
      }

      /* this makes t~ = t+dt */
      if (!(mid_flags & MID_PRESERVE)) {
        copy_v3_v3(dx, bp->vec);
      }

      /* so here is (x)'= v(elocity) */
      /* the euler step for location then becomes */
      /* x(t + dt) = x(t) + v(t~) * dt */
      mul_v3_fl(dx, forcetime);

      /* the freezer coming sooner or later */
#if 0
      if ((dot_v3v3(dx, dx) < freezeloc) && (dot_v3v3(bp->force, bp->force) < freezeforce)) {
        bp->frozen /= 2;
      }
      else {
        bp->frozen = min_ff(bp->frozen * 1.05f, 1.0f);
      }
      mul_v3_fl(dx, bp->frozen);
#endif
      /* again some nasty if's to have heun in here too */
      if (mode == 1) {
        copy_v3_v3(bp->prevpos, bp->pos);
        copy_v3_v3(bp->prevdx, dx);
      }

      if (mode == 2) {
        bp->pos[0] = bp->prevpos[0] + 0.5f * (dx[0] + bp->prevdx[0]);
        bp->pos[1] = bp->prevpos[1] + 0.5f * (dx[1] + bp->prevdx[1]);
        bp->pos[2] = bp->prevpos[2] + 0.5f * (dx[2] + bp->prevdx[2]);
        maxerrpos = max_ff(maxerrpos, fabsf(dx[0] - bp->prevdx[0]));
        maxerrpos = max_ff(maxerrpos, fabsf(dx[1] - bp->prevdx[1]));
        maxerrpos = max_ff(maxerrpos, fabsf(dx[2] - bp->prevdx[2]));

        /* bp->choke is set when we need to pull a vertex or edge out of the collider.
         * the collider object signals to get out by pushing hard. on the other hand
         * we don't want to end up in deep space so we add some <viscosity>
         * to balance that out */
        if (bp->choke2 > 0.0f) {
          mul_v3_fl(bp->vec, (1.0f - bp->choke2));
        }
        if (bp->choke > 0.0f) {
          mul_v3_fl(bp->vec, (1.0f - bp->choke));
        }
      }
      else {
        add_v3_v3(bp->pos, dx);
      }
    } /*snap*/
    /* so while we are looping BPs anyway do statistics on the fly */
    minmax_v3v3_v3(aabbmin, aabbmax, bp->pos);
    if (bp->loc_flag & SBF_DOFUZZY) {
      fuzzy = 1;
    }
  } /*for*/

  if (sb->totpoint) {
    mul_v3_fl(cm, 1.0f / sb->totpoint);
  }
  if (sb->scratch) {
    copy_v3_v3(sb->scratch->aabbmin, aabbmin);
    copy_v3_v3(sb->scratch->aabbmax, aabbmax);
  }

  if (err) { /* so step size will be controlled by biggest difference in slope */
    if (sb->solverflags & SBSO_OLDERR) {
      *err = max_ff(maxerrpos, maxerrvel);
    }
    else {
      *err = maxerrpos;
    }
    // printf("EP %f EV %f\n", maxerrpos, maxerrvel);
    if (fuzzy) {
      *err /= sb->fuzzyness;
    }
  }
}

/* used by heun when it overshoots */
static void softbody_restore_prev_step(Object *ob)
{
  SoftBody *sb = ob->soft; /* is supposed to be there*/
  BodyPoint *bp;
  int a;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    copy_v3_v3(bp->vec, bp->prevvec);
    copy_v3_v3(bp->pos, bp->prevpos);
  }
}

#if 0
static void softbody_store_step(Object *ob)
{
  SoftBody *sb = ob->soft; /* is supposed to be there*/
  BodyPoint *bp;
  int a;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    copy_v3_v3(bp->prevvec, bp->vec);
    copy_v3_v3(bp->prevpos, bp->pos);
  }
}

/* used by predictors and correctors */
static void softbody_store_state(Object *ob, float *ppos, float *pvel)
{
  SoftBody *sb = ob->soft; /* is supposed to be there*/
  BodyPoint *bp;
  int a;
  float *pp = ppos, *pv = pvel;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {

    copy_v3_v3(pv, bp->vec);
    pv += 3;

    copy_v3_v3(pp, bp->pos);
    pp += 3;
  }
}

/* used by predictors and correctors */
static void softbody_retrieve_state(Object *ob, float *ppos, float *pvel)
{
  SoftBody *sb = ob->soft; /* is supposed to be there*/
  BodyPoint *bp;
  int a;
  float *pp = ppos, *pv = pvel;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {

    copy_v3_v3(bp->vec, pv);
    pv += 3;

    copy_v3_v3(bp->pos, pp);
    pp += 3;
  }
}

/* used by predictors and correctors */
static void softbody_swap_state(Object *ob, float *ppos, float *pvel)
{
  SoftBody *sb = ob->soft; /* is supposed to be there*/
  BodyPoint *bp;
  int a;
  float *pp = ppos, *pv = pvel;
  float temp[3];

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {

    copy_v3_v3(temp, bp->vec);
    copy_v3_v3(bp->vec, pv);
    copy_v3_v3(pv, temp);
    pv += 3;

    copy_v3_v3(temp, bp->pos);
    copy_v3_v3(bp->pos, pp);
    copy_v3_v3(pp, temp);
    pp += 3;
  }
}
#endif

/* care for bodypoints taken out of the 'ordinary' solver step
 * because they are screwed to goal by bolts
 * they just need to move along with the goal in time
 * we need to adjust them on sub frame timing in solver
 * so now when frame is done .. put 'em to the position at the end of frame
 */
static void softbody_apply_goalsnap(Object *ob)
{
  SoftBody *sb = ob->soft; /* is supposed to be there */
  BodyPoint *bp;
  int a;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    if (_final_goal(ob, bp) >= SOFTGOALSNAP) {
      copy_v3_v3(bp->prevpos, bp->pos);
      copy_v3_v3(bp->pos, bp->origT);
    }
  }
}

static void apply_spring_memory(Object *ob)
{
  SoftBody *sb = ob->soft;
  BodySpring *bs;
  BodyPoint *bp1, *bp2;
  int a;
  float b, l, r;

  if (sb && sb->totspring) {
    b = sb->plastic;
    for (a = 0; a < sb->totspring; a++) {
      bs = &sb->bspring[a];
      bp1 = &sb->bpoint[bs->v1];
      bp2 = &sb->bpoint[bs->v2];
      l = len_v3v3(bp1->pos, bp2->pos);
      r = bs->len / l;
      if ((r > 1.05f) || (r < 0.95f)) {
        bs->len = ((100.0f - b) * bs->len + b * l) / 100.0f;
      }
    }
  }
}

/* expects full initialized softbody */
static void interpolate_exciter(Object *ob, int timescale, int time)
{
  SoftBody *sb = ob->soft;
  BodyPoint *bp;
  float f;
  int a;

  f = (float)time / (float)timescale;

  for (a = sb->totpoint, bp = sb->bpoint; a > 0; a--, bp++) {
    bp->origT[0] = bp->origS[0] + f * (bp->origE[0] - bp->origS[0]);
    bp->origT[1] = bp->origS[1] + f * (bp->origE[1] - bp->origS[1]);
    bp->origT[2] = bp->origS[2] + f * (bp->origE[2] - bp->origS[2]);
    if (_final_goal(ob, bp) >= SOFTGOALSNAP) {
      bp->vec[0] = bp->origE[0] - bp->origS[0];
      bp->vec[1] = bp->origE[1] - bp->origS[1];
      bp->vec[2] = bp->origE[2] - bp->origS[2];
    }
  }
}

/* ************ convertors ********** */

/* for each object type we need;
 * - xxxx_to_softbody(Object *ob)      : a full (new) copy, creates SB geometry
 */

/* Resetting a Mesh SB object's springs */
/* Spring length are caculted from'raw' mesh vertices that are NOT altered by modifier stack. */
static void springs_from_mesh(Object *ob)
{
  SoftBody *sb;
  Mesh *me = ob->data;
  BodyPoint *bp;
  int a;
  float scale = 1.0f;

  sb = ob->soft;
  if (me && sb) {
    /* using bp->origS as a container for spring calculations here
     * will be overwritten sbObjectStep() to receive
     * actual modifier stack positions
     */
    if (me->totvert) {
      bp = ob->soft->bpoint;
      for (a = 0; a < me->totvert; a++, bp++) {
        copy_v3_v3(bp->origS, me->mvert[a].co);
        mul_m4_v3(ob->obmat, bp->origS);
      }
    }
    /* recalculate spring length for meshes here */
    /* public version shrink to fit */
    if (sb->springpreload != 0) {
      scale = sb->springpreload / 100.0f;
    }
    for (a = 0; a < sb->totspring; a++) {
      BodySpring *bs = &sb->bspring[a];
      bs->len = scale * len_v3v3(sb->bpoint[bs->v1].origS, sb->bpoint[bs->v2].origS);
    }
  }
}

/* makes totally fresh start situation */
static void mesh_to_softbody(Scene *scene, Object *ob)
{
  SoftBody *sb;
  Mesh *me = ob->data;
  MEdge *medge = me->medge;
  BodyPoint *bp;
  BodySpring *bs;
  int a, totedge;
  int defgroup_index, defgroup_index_mass, defgroup_index_spring;

  if (ob->softflag & OB_SB_EDGES) {
    totedge = me->totedge;
  }
  else {
    totedge = 0;
  }

  /* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
  renew_softbody(scene, ob, me->totvert, totedge);

  /* we always make body points */
  sb = ob->soft;
  bp = sb->bpoint;

  defgroup_index = me->dvert ? (sb->vertgroup - 1) : -1;
  defgroup_index_mass = me->dvert ? BKE_object_defgroup_name_index(ob, sb->namedVG_Mass) : -1;
  defgroup_index_spring = me->dvert ? BKE_object_defgroup_name_index(ob, sb->namedVG_Spring_K) :
                                      -1;

  for (a = 0; a < me->totvert; a++, bp++) {
    /* get scalar values needed  *per vertex* from vertex group functions,
     * so we can *paint* them nicely ..
     * they are normalized [0.0..1.0] so may be we need amplitude for scale
     * which can be done by caller but still .. i'd like it to go this way
     */

    if (ob->softflag & OB_SB_GOAL) {
      BLI_assert(bp->goal == sb->defgoal);
    }
    if ((ob->softflag & OB_SB_GOAL) && (defgroup_index != -1)) {
      bp->goal *= BKE_defvert_find_weight(&me->dvert[a], defgroup_index);
    }

    /* to proof the concept
     * this enables per vertex *mass painting*
     */

    if (defgroup_index_mass != -1) {
      bp->mass *= BKE_defvert_find_weight(&me->dvert[a], defgroup_index_mass);
    }

    if (defgroup_index_spring != -1) {
      bp->springweight *= BKE_defvert_find_weight(&me->dvert[a], defgroup_index_spring);
    }
  }

  /* but we only optionally add body edge springs */
  if (ob->softflag & OB_SB_EDGES) {
    if (medge) {
      bs = sb->bspring;
      for (a = me->totedge; a > 0; a--, medge++, bs++) {
        bs->v1 = medge->v1;
        bs->v2 = medge->v2;
        bs->springtype = SB_EDGE;
      }

      /* insert *diagonal* springs in quads if desired */
      if (ob->softflag & OB_SB_QUADS) {
        add_mesh_quad_diag_springs(ob);
      }

      build_bps_springlist(ob); /* scan for springs attached to bodypoints ONCE */
      /* insert *other second order* springs if desired */
      if (sb->secondspring > 0.0000001f) {
        /* exploits the first run of build_bps_springlist(ob); */
        add_2nd_order_springs(ob, sb->secondspring);
        /* yes we need to do it again. */
        build_bps_springlist(ob);
      }
      springs_from_mesh(ob); /* write the 'rest'-length of the springs */
      if (ob->softflag & OB_SB_SELF) {
        calculate_collision_balls(ob);
      }
    }
  }
}

static void mesh_faces_to_scratch(Object *ob)
{
  SoftBody *sb = ob->soft;
  const Mesh *me = ob->data;
  MLoopTri *looptri, *lt;
  BodyFace *bodyface;
  int a;
  /* alloc and copy faces*/

  sb->scratch->totface = poly_to_tri_count(me->totpoly, me->totloop);
  looptri = lt = MEM_mallocN(sizeof(*looptri) * sb->scratch->totface, __func__);
  BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, looptri);

  bodyface = sb->scratch->bodyface = MEM_mallocN(sizeof(BodyFace) * sb->scratch->totface,
                                                 "SB_body_Faces");

  for (a = 0; a < sb->scratch->totface; a++, lt++, bodyface++) {
    bodyface->v1 = me->mloop[lt->tri[0]].v;
    bodyface->v2 = me->mloop[lt->tri[1]].v;
    bodyface->v3 = me->mloop[lt->tri[2]].v;
    zero_v3(bodyface->ext_force);
    bodyface->ext_force[0] = bodyface->ext_force[1] = bodyface->ext_force[2] = 0.0f;
    bodyface->flag = 0;
  }

  MEM_freeN(looptri);
}
static void reference_to_scratch(Object *ob)
{
  SoftBody *sb = ob->soft;
  ReferenceVert *rp;
  BodyPoint *bp;
  float accu_pos[3] = {0.f, 0.f, 0.f};
  float accu_mass = 0.f;
  int a;

  sb->scratch->Ref.ivert = MEM_mallocN(sizeof(ReferenceVert) * sb->totpoint, "SB_Reference");
  bp = ob->soft->bpoint;
  rp = sb->scratch->Ref.ivert;
  for (a = 0; a < sb->totpoint; a++, rp++, bp++) {
    copy_v3_v3(rp->pos, bp->pos);
    add_v3_v3(accu_pos, bp->pos);
    accu_mass += _final_mass(ob, bp);
  }
  mul_v3_fl(accu_pos, 1.0f / accu_mass);
  copy_v3_v3(sb->scratch->Ref.com, accu_pos);
  /* printf("reference_to_scratch\n"); */
}

/*
 * helper function to get proper spring length
 * when object is rescaled
 */
static float globallen(float *v1, float *v2, Object *ob)
{
  float p1[3], p2[3];
  copy_v3_v3(p1, v1);
  mul_m4_v3(ob->obmat, p1);
  copy_v3_v3(p2, v2);
  mul_m4_v3(ob->obmat, p2);
  return len_v3v3(p1, p2);
}

static void makelatticesprings(Lattice *lt, BodySpring *bs, int dostiff, Object *ob)
{
  BPoint *bp = lt->def, *bpu;
  int u, v, w, dv, dw, bpc = 0, bpuc;

  dv = lt->pntsu;
  dw = dv * lt->pntsv;

  for (w = 0; w < lt->pntsw; w++) {

    for (v = 0; v < lt->pntsv; v++) {

      for (u = 0, bpuc = 0, bpu = NULL; u < lt->pntsu; u++, bp++, bpc++) {

        if (w) {
          bs->v1 = bpc;
          bs->v2 = bpc - dw;
          bs->springtype = SB_EDGE;
          bs->len = globallen((bp - dw)->vec, bp->vec, ob);
          bs++;
        }
        if (v) {
          bs->v1 = bpc;
          bs->v2 = bpc - dv;
          bs->springtype = SB_EDGE;
          bs->len = globallen((bp - dv)->vec, bp->vec, ob);
          bs++;
        }
        if (u) {
          bs->v1 = bpuc;
          bs->v2 = bpc;
          bs->springtype = SB_EDGE;
          bs->len = globallen((bpu)->vec, bp->vec, ob);
          bs++;
        }

        if (dostiff) {

          if (w) {
            if (v && u) {
              bs->v1 = bpc;
              bs->v2 = bpc - dw - dv - 1;
              bs->springtype = SB_BEND;
              bs->len = globallen((bp - dw - dv - 1)->vec, bp->vec, ob);
              bs++;
            }
            if ((v < lt->pntsv - 1) && (u != 0)) {
              bs->v1 = bpc;
              bs->v2 = bpc - dw + dv - 1;
              bs->springtype = SB_BEND;
              bs->len = globallen((bp - dw + dv - 1)->vec, bp->vec, ob);
              bs++;
            }
          }

          if (w < lt->pntsw - 1) {
            if (v && u) {
              bs->v1 = bpc;
              bs->v2 = bpc + dw - dv - 1;
              bs->springtype = SB_BEND;
              bs->len = globallen((bp + dw - dv - 1)->vec, bp->vec, ob);
              bs++;
            }
            if ((v < lt->pntsv - 1) && (u != 0)) {
              bs->v1 = bpc;
              bs->v2 = bpc + dw + dv - 1;
              bs->springtype = SB_BEND;
              bs->len = globallen((bp + dw + dv - 1)->vec, bp->vec, ob);
              bs++;
            }
          }
        }
        bpu = bp;
        bpuc = bpc;
      }
    }
  }
}

/* makes totally fresh start situation */
static void lattice_to_softbody(Scene *scene, Object *ob)
{
  Lattice *lt = ob->data;
  SoftBody *sb;
  int totvert, totspring = 0, a;
  BodyPoint *bp;
  BPoint *bpnt = lt->def;
  int defgroup_index, defgroup_index_mass, defgroup_index_spring;

  totvert = lt->pntsu * lt->pntsv * lt->pntsw;

  if (ob->softflag & OB_SB_EDGES) {
    totspring = ((lt->pntsu - 1) * lt->pntsv + (lt->pntsv - 1) * lt->pntsu) * lt->pntsw +
                lt->pntsu * lt->pntsv * (lt->pntsw - 1);
    if (ob->softflag & OB_SB_QUADS) {
      totspring += 4 * (lt->pntsu - 1) * (lt->pntsv - 1) * (lt->pntsw - 1);
    }
  }

  /* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
  renew_softbody(scene, ob, totvert, totspring);
  sb = ob->soft; /* can be created in renew_softbody() */
  bp = sb->bpoint;

  defgroup_index = lt->dvert ? (sb->vertgroup - 1) : -1;
  defgroup_index_mass = lt->dvert ? BKE_object_defgroup_name_index(ob, sb->namedVG_Mass) : -1;
  defgroup_index_spring = lt->dvert ? BKE_object_defgroup_name_index(ob, sb->namedVG_Spring_K) :
                                      -1;

  /* same code used as for mesh vertices */
  for (a = 0; a < totvert; a++, bp++, bpnt++) {

    if (ob->softflag & OB_SB_GOAL) {
      BLI_assert(bp->goal == sb->defgoal);
    }

    if ((ob->softflag & OB_SB_GOAL) && (defgroup_index != -1)) {
      bp->goal *= BKE_defvert_find_weight(&lt->dvert[a], defgroup_index);
    }
    else {
      bp->goal *= bpnt->weight;
    }

    if (defgroup_index_mass != -1) {
      bp->mass *= BKE_defvert_find_weight(&lt->dvert[a], defgroup_index_mass);
    }

    if (defgroup_index_spring != -1) {
      bp->springweight *= BKE_defvert_find_weight(&lt->dvert[a], defgroup_index_spring);
    }
  }

  /* create some helper edges to enable SB lattice to be useful at all */
  if (ob->softflag & OB_SB_EDGES) {
    makelatticesprings(lt, ob->soft->bspring, ob->softflag & OB_SB_QUADS, ob);
    build_bps_springlist(ob); /* link bps to springs */
  }
}

/* makes totally fresh start situation */
static void curve_surf_to_softbody(Scene *scene, Object *ob)
{
  Curve *cu = ob->data;
  SoftBody *sb;
  BodyPoint *bp;
  BodySpring *bs;
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bpnt;
  int a, curindex = 0;
  int totvert, totspring = 0, setgoal = 0;

  totvert = BKE_nurbList_verts_count(&cu->nurb);

  if (ob->softflag & OB_SB_EDGES) {
    if (ob->type == OB_CURVE) {
      totspring = totvert - BLI_listbase_count(&cu->nurb);
    }
  }

  /* renew ends with ob->soft with points and edges, also checks & makes ob->soft */
  renew_softbody(scene, ob, totvert, totspring);
  sb = ob->soft; /* can be created in renew_softbody() */

  /* set vars now */
  bp = sb->bpoint;
  bs = sb->bspring;

  /* weights from bpoints, same code used as for mesh vertices */
  /* if ((ob->softflag & OB_SB_GOAL) && sb->vertgroup) 2.4x hack*/
  /* new! take the weights from curve vertex anyhow */
  if (ob->softflag & OB_SB_GOAL) {
    setgoal = 1;
  }

  for (nu = cu->nurb.first; nu; nu = nu->next) {
    if (nu->bezt) {
      /* Bezier case; this is nicly said naive; who ever wrote this part,
       * it was not me (JOW) :).
       *
       * a: never ever make tangent handles (sub) and or (ob)ject to collision.
       * b: rather calculate them using some C2
       *    (C2= continuous in second derivate -> no jump in bending ) condition.
       *
       * Not too hard to do, but needs some more code to care for;
       * some one may want look at it (JOW 2010/06/12). */
      for (bezt = nu->bezt, a = 0; a < nu->pntsu; a++, bezt++, bp += 3, curindex += 3) {
        if (setgoal) {
          bp->goal *= bezt->weight;

          /* all three triples */
          (bp + 1)->goal = bp->goal;
          (bp + 2)->goal = bp->goal;
          /*do not collide handles */
          (bp + 1)->loc_flag |= SBF_OUTOFCOLLISION;
          (bp + 2)->loc_flag |= SBF_OUTOFCOLLISION;
        }

        if (totspring) {
          if (a > 0) {
            bs->v1 = curindex - 3;
            bs->v2 = curindex;
            bs->springtype = SB_HANDLE;
            bs->len = globallen((bezt - 1)->vec[0], bezt->vec[0], ob);
            bs++;
          }
          bs->v1 = curindex;
          bs->v2 = curindex + 1;
          bs->springtype = SB_HANDLE;
          bs->len = globallen(bezt->vec[0], bezt->vec[1], ob);
          bs++;

          bs->v1 = curindex + 1;
          bs->v2 = curindex + 2;
          bs->springtype = SB_HANDLE;
          bs->len = globallen(bezt->vec[1], bezt->vec[2], ob);
          bs++;
        }
      }
    }
    else {
      for (bpnt = nu->bp, a = 0; a < nu->pntsu * nu->pntsv; a++, bpnt++, bp++, curindex++) {
        if (setgoal) {
          bp->goal *= bpnt->weight;
        }
        if (totspring && a > 0) {
          bs->v1 = curindex - 1;
          bs->v2 = curindex;
          bs->springtype = SB_EDGE;
          bs->len = globallen((bpnt - 1)->vec, bpnt->vec, ob);
          bs++;
        }
      }
    }
  }

  if (totspring) {
    build_bps_springlist(ob); /* link bps to springs */
    if (ob->softflag & OB_SB_SELF) {
      calculate_collision_balls(ob);
    }
  }
}

/* copies softbody result back in object */
static void softbody_to_object(Object *ob, float (*vertexCos)[3], int numVerts, int local)
{
  SoftBody *sb = ob->soft;
  if (sb) {
    BodyPoint *bp = sb->bpoint;
    int a;
    if (sb->solverflags & SBSO_ESTIMATEIPO) {
      SB_estimate_transform(ob, sb->lcom, sb->lrot, sb->lscale);
    }
    /* inverse matrix is not uptodate... */
    invert_m4_m4(ob->imat, ob->obmat);

    for (a = 0; a < numVerts; a++, bp++) {
      copy_v3_v3(vertexCos[a], bp->pos);
      if (local == 0) {
        mul_m4_v3(ob->imat, vertexCos[a]); /* softbody is in global coords, baked optionally not */
      }
    }
  }
}

/* +++ ************ maintaining scratch *************** */
static void sb_new_scratch(SoftBody *sb)
{
  if (!sb) {
    return;
  }
  sb->scratch = MEM_callocN(sizeof(SBScratch), "SBScratch");
  sb->scratch->colliderhash = BLI_ghash_ptr_new("sb_new_scratch gh");
  sb->scratch->bodyface = NULL;
  sb->scratch->totface = 0;
  sb->scratch->aabbmax[0] = sb->scratch->aabbmax[1] = sb->scratch->aabbmax[2] = 1.0e30f;
  sb->scratch->aabbmin[0] = sb->scratch->aabbmin[1] = sb->scratch->aabbmin[2] = -1.0e30f;
  sb->scratch->Ref.ivert = NULL;
}
/* --- ************ maintaining scratch *************** */

/* ************ Object level, exported functions *************** */

/* allocates and initializes general main data */
SoftBody *sbNew(Scene *scene)
{
  SoftBody *sb;

  sb = MEM_callocN(sizeof(SoftBody), "softbody");

  sb->mediafrict = 0.5f;
  sb->nodemass = 1.0f;
  sb->grav = 9.8f;
  sb->physics_speed = 1.0f;
  sb->rklimit = 0.1f;

  sb->goalspring = 0.5f;
  sb->goalfrict = 0.0f;
  sb->mingoal = 0.0f;
  sb->maxgoal = 1.0f;
  sb->defgoal = 0.7f;

  sb->inspring = 0.5f;
  sb->infrict = 0.5f;
  /*todo backward file compat should copy inspring to inpush while reading old files*/
  sb->inpush = 0.5f;

  sb->interval = 10;
  if (scene != NULL) {
    sb->sfra = scene->r.sfra;
    sb->efra = scene->r.efra;
  }

  sb->colball = 0.49f;
  sb->balldamp = 0.50f;
  sb->ballstiff = 1.0f;
  sb->sbc_mode = 1;

  sb->minloops = 10;
  sb->maxloops = 300;

  sb->choke = 3;
  sb_new_scratch(sb);
  /*todo backward file compat should set sb->shearstiff = 1.0f while reading old files*/
  sb->shearstiff = 1.0f;
  sb->solverflags |= SBSO_OLDERR;

  sb->shared = MEM_callocN(sizeof(*sb->shared), "SoftBody_Shared");
  sb->shared->pointcache = BKE_ptcache_add(&sb->shared->ptcaches);

  if (!sb->effector_weights) {
    sb->effector_weights = BKE_effector_add_weights(NULL);
  }

  sb->last_frame = MINFRAME - 1;

  return sb;
}

/* frees all */
void sbFree(Object *ob)
{
  SoftBody *sb = ob->soft;
  if (sb == NULL) {
    return;
  }

  free_softbody_intern(sb);

  if ((ob->id.tag & LIB_TAG_NO_MAIN) == 0) {
    /* Only free shared data on non-CoW copies */
    BKE_ptcache_free_list(&sb->shared->ptcaches);
    sb->shared->pointcache = NULL;
    MEM_freeN(sb->shared);
  }
  if (sb->effector_weights) {
    MEM_freeN(sb->effector_weights);
  }
  MEM_freeN(sb);

  ob->soft = NULL;
}

void sbFreeSimulation(SoftBody *sb)
{
  free_softbody_intern(sb);
}

/* makes totally fresh start situation */
void sbObjectToSoftbody(Object *ob)
{
  // ob->softflag |= OB_SB_REDO;

  free_softbody_intern(ob->soft);
}

static int object_has_edges(Object *ob)
{
  if (ob->type == OB_MESH) {
    return ((Mesh *)ob->data)->totedge;
  }
  else if (ob->type == OB_LATTICE) {
    return 1;
  }
  else {
    return 0;
  }
}

/* SB global visible functions */
void sbSetInterruptCallBack(int (*f)(void))
{
  SB_localInterruptCallBack = f;
}

static void softbody_update_positions(Object *ob,
                                      SoftBody *sb,
                                      float (*vertexCos)[3],
                                      int numVerts)
{
  BodyPoint *bp;
  int a;

  if (!sb || !sb->bpoint) {
    return;
  }

  for (a = 0, bp = sb->bpoint; a < numVerts; a++, bp++) {
    /* store where goals are now */
    copy_v3_v3(bp->origS, bp->origE);
    /* copy the position of the goals at desired end time */
    copy_v3_v3(bp->origE, vertexCos[a]);
    /* vertexCos came from local world, go global */
    mul_m4_v3(ob->obmat, bp->origE);
    /* just to be save give bp->origT a defined value
     * will be calculated in interpolate_exciter() */
    copy_v3_v3(bp->origT, bp->origE);
  }
}

/* void SB_estimate_transform */
/* input   Object *ob out (says any object that can do SB like mesh, lattice, curve )
 * output  float lloc[3], float lrot[3][3], float lscale[3][3]
 * that is:
 * a precise position vector denoting the motion of the center of mass
 * give a rotation/scale matrix using averaging method, that's why estimate and not calculate
 * see: this is kind of reverse engineering: having to states of a point cloud and recover what
 * happened our advantage here we know the identity of the vertex there are others methods giving
 * other results. lloc, lrot, lscale are allowed to be NULL, just in case you don't need it.
 * should be pretty useful for pythoneers :)
 * not! velocity .. 2nd order stuff
 * vcloud_estimate_transform_v3 see
 */

void SB_estimate_transform(Object *ob, float lloc[3], float lrot[3][3], float lscale[3][3])
{
  BodyPoint *bp;
  ReferenceVert *rp;
  SoftBody *sb = NULL;
  float(*opos)[3];
  float(*rpos)[3];
  float com[3], rcom[3];
  int a;

  if (!ob || !ob->soft) {
    return; /* why did we get here ? */
  }
  sb = ob->soft;
  if (!sb || !sb->bpoint) {
    return;
  }
  opos = MEM_callocN((sb->totpoint) * 3 * sizeof(float), "SB_OPOS");
  rpos = MEM_callocN((sb->totpoint) * 3 * sizeof(float), "SB_RPOS");
  /* might filter vertex selection with a vertex group */
  for (a = 0, bp = sb->bpoint, rp = sb->scratch->Ref.ivert; a < sb->totpoint; a++, bp++, rp++) {
    copy_v3_v3(rpos[a], rp->pos);
    copy_v3_v3(opos[a], bp->pos);
  }

  vcloud_estimate_transform_v3(sb->totpoint, opos, NULL, rpos, NULL, com, rcom, lrot, lscale);
  // sub_v3_v3(com, rcom);
  if (lloc) {
    copy_v3_v3(lloc, com);
  }
  copy_v3_v3(sb->lcom, com);
  if (lscale) {
    copy_m3_m3(sb->lscale, lscale);
  }
  if (lrot) {
    copy_m3_m3(sb->lrot, lrot);
  }

  MEM_freeN(opos);
  MEM_freeN(rpos);
}

static void softbody_reset(Object *ob, SoftBody *sb, float (*vertexCos)[3], int numVerts)
{
  BodyPoint *bp;
  int a;

  for (a = 0, bp = sb->bpoint; a < numVerts; a++, bp++) {
    copy_v3_v3(bp->pos, vertexCos[a]);
    mul_m4_v3(ob->obmat, bp->pos); /* yep, sofbody is global coords*/
    copy_v3_v3(bp->origS, bp->pos);
    copy_v3_v3(bp->origE, bp->pos);
    copy_v3_v3(bp->origT, bp->pos);
    bp->vec[0] = bp->vec[1] = bp->vec[2] = 0.0f;

    /* the bp->prev*'s are for rolling back from a canceled try to propagate in time
     * adaptive step size algo in a nutshell:
     * 1.  set scheduled time step to new dtime
     * 2.  try to advance the scheduled time step, being optimistic execute it
     * 3.  check for success
     * 3.a we 're fine continue, may be we can increase scheduled time again ?? if so, do so!
     * 3.b we did exceed error limit --> roll back, shorten the scheduled time and try again at 2.
     * 4.  check if we did reach dtime
     * 4.a nope we need to do some more at 2.
     * 4.b yup we're done
     */

    copy_v3_v3(bp->prevpos, bp->pos);
    copy_v3_v3(bp->prevvec, bp->vec);
    copy_v3_v3(bp->prevdx, bp->vec);
    copy_v3_v3(bp->prevdv, bp->vec);
  }

  /* make a nice clean scratch struct */
  free_scratch(sb);   /* clear if any */
  sb_new_scratch(sb); /* make a new */
  sb->scratch->needstobuildcollider = 1;
  zero_v3(sb->lcom);
  unit_m3(sb->lrot);
  unit_m3(sb->lscale);

  /* copy some info to scratch */
  /* we only need that if we want to reconstruct IPO */
  if (1) {
    reference_to_scratch(ob);
    SB_estimate_transform(ob, NULL, NULL, NULL);
    SB_estimate_transform(ob, NULL, NULL, NULL);
  }
  switch (ob->type) {
    case OB_MESH:
      if (ob->softflag & OB_SB_FACECOLL) {
        mesh_faces_to_scratch(ob);
      }
      break;
    case OB_LATTICE:
      break;
    case OB_CURVE:
    case OB_SURF:
      break;
    default:
      break;
  }
}

static void softbody_step(
    struct Depsgraph *depsgraph, Scene *scene, Object *ob, SoftBody *sb, float dtime)
{
  /* the simulator */
  float forcetime;
  double sct, sst;

  sst = PIL_check_seconds_timer();
  /* Integration back in time is possible in theory, but pretty useless here.
   * So we refuse to do so. Since we do not know anything about 'outside' changes
   * especially colliders we refuse to go more than 10 frames.
   */
  if (dtime < 0 || dtime > 10.5f) {
    return;
  }

  ccd_update_deflector_hash(depsgraph, sb->collision_group, ob, sb->scratch->colliderhash);

  if (sb->scratch->needstobuildcollider) {
    ccd_build_deflector_hash(depsgraph, sb->collision_group, ob, sb->scratch->colliderhash);
    sb->scratch->needstobuildcollider = 0;
  }

  if (sb->solver_ID < 2) {
    /* special case of 2nd order Runge-Kutta type AKA Heun */
    int mid_flags = 0;
    float err = 0;
    /* Set defaults guess we shall do one frame */
    float forcetimemax = 1.0f;
    /* Set defaults guess 1/100 is tight enough */
    float forcetimemin = 0.01f;
    /* How far did we get without violating error condition. */
    float timedone = 0.0;
    /* Loops = counter for emergency brake we don't want to lock up the system if physics fail. */
    int loops = 0;

    SoftHeunTol = sb->rklimit; /* humm .. this should be calculated from sb parameters and sizes */
    /* adjust loop limits */
    if (sb->minloops > 0) {
      forcetimemax = dtime / sb->minloops;
    }
    if (sb->maxloops > 0) {
      forcetimemin = dtime / sb->maxloops;
    }

    if (sb->solver_ID > 0) {
      mid_flags |= MID_PRESERVE;
    }

    forcetime = forcetimemax; /* hope for integrating in one step */
    while ((fabsf(timedone) < fabsf(dtime)) && (loops < 2000)) {
      /* set goals in time */
      interpolate_exciter(ob, 200, (int)(200.0f * (timedone / dtime)));

      sb->scratch->flag &= ~SBF_DOFUZZY;
      /* do predictive euler step */
      softbody_calc_forces(depsgraph, scene, ob, forcetime, timedone / dtime);

      softbody_apply_forces(ob, forcetime, 1, NULL, mid_flags);

      /* crop new slope values to do averaged slope step */
      softbody_calc_forces(depsgraph, scene, ob, forcetime, timedone / dtime);

      softbody_apply_forces(ob, forcetime, 2, &err, mid_flags);
      softbody_apply_goalsnap(ob);

      if (err > SoftHeunTol) { /* error needs to be scaled to some quantity */

        if (forcetime > forcetimemin) {
          forcetime = max_ff(forcetime / 2.0f, forcetimemin);
          softbody_restore_prev_step(ob);
          // printf("down, ");
        }
        else {
          timedone += forcetime;
        }
      }
      else {
        float newtime = forcetime * 1.1f; /* hope for 1.1 times better conditions in next step */

        if (sb->scratch->flag & SBF_DOFUZZY) {
          ///* stay with this stepsize unless err really small */
          // if (err > SoftHeunTol/(2.0f*sb->fuzzyness)) {
          newtime = forcetime;
          //}
        }
        else {
          if (err > SoftHeunTol / 2.0f) { /* stay with this stepsize unless err really small */
            newtime = forcetime;
          }
        }
        timedone += forcetime;
        newtime = min_ff(forcetimemax, max_ff(newtime, forcetimemin));
        // if (newtime > forcetime) printf("up, ");
        if (forcetime > 0.0f) {
          forcetime = min_ff(dtime - timedone, newtime);
        }
        else {
          forcetime = max_ff(dtime - timedone, newtime);
        }
      }
      loops++;
      if (sb->solverflags & SBSO_MONITOR) {
        sct = PIL_check_seconds_timer();
        if (sct - sst > 0.5) {
          printf("%3.0f%% \r", 100.0f * timedone / dtime);
        }
      }
      /* ask for user break */
      if (SB_localInterruptCallBack && SB_localInterruptCallBack()) {
        break;
      }
    }
    /* move snapped to final position */
    interpolate_exciter(ob, 2, 2);
    softbody_apply_goalsnap(ob);

    //              if (G.debug & G_DEBUG) {
    if (sb->solverflags & SBSO_MONITOR) {
      if (loops > HEUNWARNLIMIT) { /* monitor high loop counts */
        printf("\r needed %d steps/frame", loops);
      }
    }
  }
  else if (sb->solver_ID == 2) {
    /* do semi "fake" implicit euler */
    // removed
  } /*SOLVER SELECT*/
  else if (sb->solver_ID == 4) {
    /* do semi "fake" implicit euler */
  } /*SOLVER SELECT*/
  else if (sb->solver_ID == 3) {
    /* do "stupid" semi "fake" implicit euler */
    // removed

  } /*SOLVER SELECT*/
  else {
    CLOG_ERROR(&LOG, "softbody no valid solver ID!");
  } /*SOLVER SELECT*/
  if (sb->plastic) {
    apply_spring_memory(ob);
  }

  if (sb->solverflags & SBSO_MONITOR) {
    sct = PIL_check_seconds_timer();
    if ((sct - sst > 0.5) || (G.debug & G_DEBUG)) {
      printf(" solver time %f sec %s\n", sct - sst, ob->id.name);
    }
  }
}

static void sbStoreLastFrame(struct Depsgraph *depsgraph, Object *object, float framenr)
{
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  Object *object_orig = DEG_get_original_object(object);
  object->soft->last_frame = framenr;
  object_orig->soft->last_frame = framenr;
}

/* simulates one step. framenr is in frames */
void sbObjectStep(struct Depsgraph *depsgraph,
                  Scene *scene,
                  Object *ob,
                  float cfra,
                  float (*vertexCos)[3],
                  int numVerts)
{
  SoftBody *sb = ob->soft;
  PointCache *cache;
  PTCacheID pid;
  float dtime, timescale;
  int framedelta, framenr, startframe, endframe;
  int cache_result;
  cache = sb->shared->pointcache;

  framenr = (int)cfra;
  framedelta = framenr - cache->simframe;

  BKE_ptcache_id_from_softbody(&pid, ob, sb);
  BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

  /* check for changes in mesh, should only happen in case the mesh
   * structure changes during an animation */
  if (sb->bpoint && numVerts != sb->totpoint) {
    BKE_ptcache_invalidate(cache);
    return;
  }

  /* clamp frame ranges */
  if (framenr < startframe) {
    BKE_ptcache_invalidate(cache);
    return;
  }
  else if (framenr > endframe) {
    framenr = endframe;
  }

  /* verify if we need to create the softbody data */
  if (sb->bpoint == NULL ||
      ((ob->softflag & OB_SB_EDGES) && !ob->soft->bspring && object_has_edges(ob))) {

    switch (ob->type) {
      case OB_MESH:
        mesh_to_softbody(scene, ob);
        break;
      case OB_LATTICE:
        lattice_to_softbody(scene, ob);
        break;
      case OB_CURVE:
      case OB_SURF:
        curve_surf_to_softbody(scene, ob);
        break;
      default:
        renew_softbody(scene, ob, numVerts, 0);
        break;
    }

    softbody_update_positions(ob, sb, vertexCos, numVerts);
    softbody_reset(ob, sb, vertexCos, numVerts);
  }

  /* still no points? go away */
  if (sb->totpoint == 0) {
    return;
  }
  if (framenr == startframe) {
    BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);

    /* first frame, no simulation to do, just set the positions */
    softbody_update_positions(ob, sb, vertexCos, numVerts);

    BKE_ptcache_validate(cache, framenr);
    cache->flag &= ~PTCACHE_REDO_NEEDED;

    sbStoreLastFrame(depsgraph, ob, framenr);

    return;
  }

  /* try to read from cache */
  bool can_write_cache = DEG_is_active(depsgraph);
  bool can_simulate = (framenr == sb->last_frame + 1) && !(cache->flag & PTCACHE_BAKED) &&
                      can_write_cache;

  cache_result = BKE_ptcache_read(&pid, (float)framenr + scene->r.subframe, can_simulate);

  if (cache_result == PTCACHE_READ_EXACT || cache_result == PTCACHE_READ_INTERPOLATED ||
      (!can_simulate && cache_result == PTCACHE_READ_OLD)) {
    softbody_to_object(ob, vertexCos, numVerts, sb->local);

    BKE_ptcache_validate(cache, framenr);

    if (cache_result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED &&
        can_write_cache) {
      BKE_ptcache_write(&pid, framenr);
    }

    sbStoreLastFrame(depsgraph, ob, framenr);

    return;
  }
  else if (cache_result == PTCACHE_READ_OLD) {
    /* pass */
  }
  else if (/*ob->id.lib || */
           /* "library linking & pointcaches" has to be solved properly at some point */
           (cache->flag & PTCACHE_BAKED)) {
    /* if baked and nothing in cache, do nothing */
    if (can_write_cache) {
      BKE_ptcache_invalidate(cache);
    }
    return;
  }

  if (!can_simulate) {
    return;
  }

  /* if on second frame, write cache for first frame */
  if (cache->simframe == startframe &&
      (cache->flag & PTCACHE_OUTDATED || cache->last_exact == 0)) {
    BKE_ptcache_write(&pid, startframe);
  }

  softbody_update_positions(ob, sb, vertexCos, numVerts);

  /* checking time: */
  dtime = framedelta * timescale;

  /* do simulation */
  softbody_step(depsgraph, scene, ob, sb, dtime);

  softbody_to_object(ob, vertexCos, numVerts, 0);

  BKE_ptcache_validate(cache, framenr);
  BKE_ptcache_write(&pid, framenr);

  sbStoreLastFrame(depsgraph, ob, framenr);
}
