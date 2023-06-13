/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Object;
struct Scene;
struct SoftBody;

typedef struct BodyPoint {
  float origS[3], origE[3], origT[3], pos[3], vec[3], force[3];
  float goal;
  float prevpos[3], prevvec[3], prevdx[3], prevdv[3]; /* used for Heun integration */
  float impdv[3], impdx[3];
  int nofsprings;
  int *springs;
  float choke, choke2, frozen;
  float colball;
  short loc_flag; /* reserved by locale module specific states */
  // char octantflag;
  float mass;
  float springweight;
} BodyPoint;

/**
 * Allocates and initializes general main data.
 */
extern struct SoftBody *sbNew(void);

/**
 * Frees internal data and soft-body itself.
 */
extern void sbFree(struct Object *ob);

/**
 * Frees simulation data to reset simulation.
 */
extern void sbFreeSimulation(struct SoftBody *sb);

/**
 * Do one simulation step, reading and writing vertex locs from given array.
 * */
extern void sbObjectStep(struct Depsgraph *depsgraph,
                         struct Scene *scene,
                         struct Object *ob,
                         float cfra,
                         float (*vertexCos)[3],
                         int numVerts);

/**
 * Makes totally fresh start situation, resets time.
 */
extern void sbObjectToSoftbody(struct Object *ob);

/**
 * Soft-body global visible functions.
 * Links the soft-body module to a 'test for Interrupt' function, pass NULL to clear the callback.
 */
extern void sbSetInterruptCallBack(int (*f)(void));

/**
 * A precise position vector denoting the motion of the center of mass give a rotation/scale matrix
 * using averaging method, that's why estimate and not calculate see: this is kind of reverse
 * engineering: having to states of a point cloud and recover what happened our advantage here we
 * know the identity of the vertex there are others methods giving other results.
 *
 * \param ob: Any object that can do soft-body e.g. mesh, lattice, curve.
 * \param lloc: Output of the calculated location (or NULL).
 * \param lrot: Output of the calculated rotation (or NULL).
 * \param lscale: Output for the calculated scale (or NULL).
 *
 * For velocity & 2nd order stuff see: #vcloud_estimate_transform_v3.
 */
extern void SB_estimate_transform(Object *ob, float lloc[3], float lrot[3][3], float lscale[3][3]);

#ifdef __cplusplus
}
#endif
