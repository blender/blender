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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_string.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_effect.h"
#include "BKE_lattice.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  pimd->flag = eParticleInstanceFlag_Parents | eParticleInstanceFlag_Unborn |
               eParticleInstanceFlag_Alive | eParticleInstanceFlag_Dead;
  pimd->psys = 1;
  pimd->position = 1.0f;
  pimd->axis = 2;
  pimd->space = eParticleInstanceSpace_World;
  pimd->particle_amount = 1.0f;
  pimd->particle_offset = 0.0f;

  STRNCPY(pimd->index_layer_name, "");
  STRNCPY(pimd->value_layer_name, "");
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  if (pimd->index_layer_name[0] != '\0' || pimd->value_layer_name[0] != '\0') {
    r_cddata_masks->lmask |= CD_MASK_MLOOPCOL;
  }
}

static bool isDisabled(const struct Scene *scene, ModifierData *md, bool useRenderParams)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  ParticleSystem *psys;
  ModifierData *ob_md;

  if (!pimd->ob) {
    return true;
  }

  psys = BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1);
  if (psys == NULL) {
    return true;
  }

  /* If the psys modifier is disabled we cannot use its data.
   * First look up the psys modifier from the object, then check if it is enabled.
   */
  for (ob_md = pimd->ob->modifiers.first; ob_md; ob_md = ob_md->next) {
    if (ob_md->type == eModifierType_ParticleSystem) {
      ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)ob_md;
      if (psmd->psys == psys) {
        int required_mode;

        if (useRenderParams) {
          required_mode = eModifierMode_Render;
        }
        else {
          required_mode = eModifierMode_Realtime;
        }

        if (!modifier_isEnabled(scene, ob_md, required_mode)) {
          return true;
        }

        break;
      }
    }
  }

  return false;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  if (pimd->ob != NULL) {
    DEG_add_object_relation(
        ctx->node, pimd->ob, DEG_OB_COMP_TRANSFORM, "Particle Instance Modifier");
    DEG_add_object_relation(
        ctx->node, pimd->ob, DEG_OB_COMP_GEOMETRY, "Particle Instance Modifier");
  }
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  walk(userData, ob, &pimd->ob, IDWALK_CB_NOP);
}

static bool particle_skip(ParticleInstanceModifierData *pimd, ParticleSystem *psys, int p)
{
  const bool between = (psys->part->childtype == PART_CHILD_FACES);
  ParticleData *pa;
  int totpart, randp, minp, maxp;

  if (p >= psys->totpart) {
    ChildParticle *cpa = psys->child + (p - psys->totpart);
    pa = psys->particles + (between ? cpa->pa[0] : cpa->parent);
  }
  else {
    pa = psys->particles + p;
  }

  if (pa) {
    if (pa->alive == PARS_UNBORN && (pimd->flag & eParticleInstanceFlag_Unborn) == 0) {
      return true;
    }
    if (pa->alive == PARS_ALIVE && (pimd->flag & eParticleInstanceFlag_Alive) == 0) {
      return true;
    }
    if (pa->alive == PARS_DEAD && (pimd->flag & eParticleInstanceFlag_Dead) == 0) {
      return true;
    }
  }

  if (pimd->particle_amount == 1.0f) {
    /* Early output, all particles are to be instanced. */
    return false;
  }

  /* Randomly skip particles based on desired amount of visible particles. */

  totpart = psys->totpart + psys->totchild;

  /* TODO make randomization optional? */
  randp = (int)(psys_frand(psys, 3578 + p) * totpart) % totpart;

  minp = (int)(totpart * pimd->particle_offset) % (totpart + 1);
  maxp = (int)(totpart * (pimd->particle_offset + pimd->particle_amount)) % (totpart + 1);

  if (maxp > minp) {
    return randp < minp || randp >= maxp;
  }
  else if (maxp < minp) {
    return randp < minp && randp >= maxp;
  }
  else {
    return true;
  }

  return false;
}

static void store_float_in_vcol(MLoopCol *vcol, float float_value)
{
  const uchar value = unit_float_to_uchar_clamp(float_value);
  vcol->r = vcol->g = vcol->b = value;
  vcol->a = 1.0f;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  ParticleSimulationData sim;
  ParticleSystem *psys = NULL;
  ParticleData *pa = NULL;
  MPoly *mpoly, *orig_mpoly;
  MLoop *mloop, *orig_mloop;
  MVert *mvert, *orig_mvert;
  int totvert, totpoly, totloop, totedge;
  int maxvert, maxpoly, maxloop, maxedge, part_end = 0, part_start;
  int k, p, p_skip;
  short track = ctx->object->trackflag % 3, trackneg, axis = pimd->axis;
  float max_co = 0.0, min_co = 0.0, temp_co[3];
  float *size = NULL;
  float spacemat[4][4];
  const bool use_parents = pimd->flag & eParticleInstanceFlag_Parents;
  const bool use_children = pimd->flag & eParticleInstanceFlag_Children;
  bool between;

  trackneg = ((ctx->object->trackflag > 2) ? 1 : 0);

  if (pimd->ob == ctx->object) {
    pimd->ob = NULL;
    return mesh;
  }

  if (pimd->ob) {
    psys = BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1);
    if (psys == NULL || psys->totpart == 0) {
      return mesh;
    }
  }
  else {
    return mesh;
  }

  part_start = use_parents ? 0 : psys->totpart;

  part_end = 0;
  if (use_parents) {
    part_end += psys->totpart;
  }
  if (use_children) {
    part_end += psys->totchild;
  }

  if (part_end == 0) {
    return mesh;
  }

  sim.depsgraph = ctx->depsgraph;
  sim.scene = scene;
  sim.ob = pimd->ob;
  sim.psys = psys;
  sim.psmd = psys_get_modifier(pimd->ob, psys);
  between = (psys->part->childtype == PART_CHILD_FACES);

  if (pimd->flag & eParticleInstanceFlag_UseSize) {
    float *si;
    si = size = MEM_calloc_arrayN(part_end, sizeof(float), "particle size array");

    if (pimd->flag & eParticleInstanceFlag_Parents) {
      for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++, si++) {
        *si = pa->size;
      }
    }

    if (pimd->flag & eParticleInstanceFlag_Children) {
      ChildParticle *cpa = psys->child;

      for (p = 0; p < psys->totchild; p++, cpa++, si++) {
        *si = psys_get_child_size(psys, cpa, 0.0f, NULL);
      }
    }
  }

  switch (pimd->space) {
    case eParticleInstanceSpace_World:
      /* particle states are in world space already */
      unit_m4(spacemat);
      break;
    case eParticleInstanceSpace_Local:
      /* get particle states in the particle object's local space */
      invert_m4_m4(spacemat, pimd->ob->obmat);
      break;
    default:
      /* should not happen */
      BLI_assert(false);
      break;
  }

  totvert = mesh->totvert;
  totpoly = mesh->totpoly;
  totloop = mesh->totloop;
  totedge = mesh->totedge;

  /* count particles */
  maxvert = 0;
  maxpoly = 0;
  maxloop = 0;
  maxedge = 0;

  for (p = part_start; p < part_end; p++) {
    if (particle_skip(pimd, psys, p)) {
      continue;
    }

    maxvert += totvert;
    maxpoly += totpoly;
    maxloop += totloop;
    maxedge += totedge;
  }

  psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  if (psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) {
    float min[3], max[3];
    INIT_MINMAX(min, max);
    BKE_mesh_minmax(mesh, min, max);
    min_co = min[track];
    max_co = max[track];
  }

  result = BKE_mesh_new_nomain_from_template(mesh, maxvert, maxedge, 0, maxloop, maxpoly);

  mvert = result->mvert;
  orig_mvert = mesh->mvert;
  mpoly = result->mpoly;
  orig_mpoly = mesh->mpoly;
  mloop = result->mloop;
  orig_mloop = mesh->mloop;

  MLoopCol *mloopcols_index = CustomData_get_layer_named(
      &result->ldata, CD_MLOOPCOL, pimd->index_layer_name);
  MLoopCol *mloopcols_value = CustomData_get_layer_named(
      &result->ldata, CD_MLOOPCOL, pimd->value_layer_name);
  int *vert_part_index = NULL;
  float *vert_part_value = NULL;
  if (mloopcols_index != NULL) {
    vert_part_index = MEM_calloc_arrayN(maxvert, sizeof(int), "vertex part index array");
  }
  if (mloopcols_value) {
    vert_part_value = MEM_calloc_arrayN(maxvert, sizeof(float), "vertex part value array");
  }

  for (p = part_start, p_skip = 0; p < part_end; p++) {
    float prev_dir[3];
    float frame[4]; /* frame orientation quaternion */
    float p_random = psys_frand(psys, 77091 + 283 * p);

    /* skip particle? */
    if (particle_skip(pimd, psys, p)) {
      continue;
    }

    /* set vertices coordinates */
    for (k = 0; k < totvert; k++) {
      ParticleKey state;
      MVert *inMV;
      int vindex = p_skip * totvert + k;
      MVert *mv = mvert + vindex;

      inMV = orig_mvert + k;
      CustomData_copy_data(&mesh->vdata, &result->vdata, k, p_skip * totvert + k, 1);
      *mv = *inMV;

      if (vert_part_index != NULL) {
        vert_part_index[vindex] = p;
      }
      if (vert_part_value != NULL) {
        vert_part_value[vindex] = p_random;
      }

      /*change orientation based on object trackflag*/
      copy_v3_v3(temp_co, mv->co);
      mv->co[axis] = temp_co[track];
      mv->co[(axis + 1) % 3] = temp_co[(track + 1) % 3];
      mv->co[(axis + 2) % 3] = temp_co[(track + 2) % 3];

      /* get particle state */
      if ((psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) &&
          (pimd->flag & eParticleInstanceFlag_Path)) {
        float ran = 0.0f;
        if (pimd->random_position != 0.0f) {
          ran = pimd->random_position * BLI_hash_frand(psys->seed + p);
        }

        if (pimd->flag & eParticleInstanceFlag_KeepShape) {
          state.time = pimd->position * (1.0f - ran);
        }
        else {
          state.time = (mv->co[axis] - min_co) / (max_co - min_co) * pimd->position * (1.0f - ran);

          if (trackneg) {
            state.time = 1.0f - state.time;
          }

          mv->co[axis] = 0.0;
        }

        psys_get_particle_on_path(&sim, p, &state, 1);

        normalize_v3(state.vel);

        /* Incrementally Rotating Frame (Bishop Frame) */
        if (k == 0) {
          float hairmat[4][4];
          float mat[3][3];

          if (p < psys->totpart) {
            pa = psys->particles + p;
          }
          else {
            ChildParticle *cpa = psys->child + (p - psys->totpart);
            pa = psys->particles + (between ? cpa->pa[0] : cpa->parent);
          }
          psys_mat_hair_to_global(sim.ob,
                                  BKE_particle_modifier_mesh_final_get(sim.psmd),
                                  sim.psys->part->from,
                                  pa,
                                  hairmat);
          copy_m3_m4(mat, hairmat);
          /* to quaternion */
          mat3_to_quat(frame, mat);

          if (pimd->rotation > 0.0f || pimd->random_rotation > 0.0f) {
            float angle = 2.0f * M_PI *
                          (pimd->rotation +
                           pimd->random_rotation * (psys_frand(psys, 19957323 + p) - 0.5f));
            float eul[3] = {0.0f, 0.0f, angle};
            float rot[4];

            eul_to_quat(rot, eul);
            mul_qt_qtqt(frame, frame, rot);
          }

          /* note: direction is same as normal vector currently,
           * but best to keep this separate so the frame can be
           * rotated later if necessary
           */
          copy_v3_v3(prev_dir, state.vel);
        }
        else {
          float rot[4];

          /* incrementally rotate along bend direction */
          rotation_between_vecs_to_quat(rot, prev_dir, state.vel);
          mul_qt_qtqt(frame, rot, frame);

          copy_v3_v3(prev_dir, state.vel);
        }

        copy_qt_qt(state.rot, frame);
#if 0
        /* Absolute Frame (Frenet Frame) */
        if (state.vel[axis] < -0.9999f || state.vel[axis] > 0.9999f) {
          unit_qt(state.rot);
        }
        else {
          float cross[3];
          float temp[3] = {0.0f, 0.0f, 0.0f};
          temp[axis] = 1.0f;

          cross_v3_v3v3(cross, temp, state.vel);

          /* state.vel[axis] is the only component surviving from a dot product with the axis */
          axis_angle_to_quat(state.rot, cross, saacos(state.vel[axis]));
        }
#endif
      }
      else {
        state.time = -1.0;
        psys_get_particle_state(&sim, p, &state, 1);
      }

      mul_qt_v3(state.rot, mv->co);
      if (pimd->flag & eParticleInstanceFlag_UseSize) {
        mul_v3_fl(mv->co, size[p]);
      }
      add_v3_v3(mv->co, state.co);

      mul_m4_v3(spacemat, mv->co);
    }

    /* create edges and adjust edge vertex indices*/
    CustomData_copy_data(&mesh->edata, &result->edata, 0, p_skip * totedge, totedge);
    MEdge *me = &result->medge[p_skip * totedge];
    for (k = 0; k < totedge; k++, me++) {
      me->v1 += p_skip * totvert;
      me->v2 += p_skip * totvert;
    }

    /* create polys and loops */
    for (k = 0; k < totpoly; k++) {

      MPoly *inMP = orig_mpoly + k;
      MPoly *mp = mpoly + p_skip * totpoly + k;

      CustomData_copy_data(&mesh->pdata, &result->pdata, k, p_skip * totpoly + k, 1);
      *mp = *inMP;
      mp->loopstart += p_skip * totloop;

      {
        MLoop *inML = orig_mloop + inMP->loopstart;
        MLoop *ml = mloop + mp->loopstart;
        int j = mp->totloop;

        CustomData_copy_data(&mesh->ldata, &result->ldata, inMP->loopstart, mp->loopstart, j);
        for (; j; j--, ml++, inML++) {
          ml->v = inML->v + (p_skip * totvert);
          ml->e = inML->e + (p_skip * totedge);
          const int ml_index = (ml - mloop);
          if (mloopcols_index != NULL) {
            const int part_index = vert_part_index[ml->v];
            store_float_in_vcol(&mloopcols_index[ml_index],
                                (float)part_index / (float)(psys->totpart - 1));
          }
          if (mloopcols_value != NULL) {
            const float part_value = vert_part_value[ml->v];
            store_float_in_vcol(&mloopcols_value[ml_index], part_value);
          }
        }
      }
    }
    p_skip++;
  }

  if (psys->lattice_deform_data) {
    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;
  }

  if (size) {
    MEM_freeN(size);
  }

  MEM_SAFE_FREE(vert_part_index);
  MEM_SAFE_FREE(vert_part_value);

  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}
ModifierTypeInfo modifierType_ParticleInstance = {
    /* name */ "ParticleInstance",
    /* structName */ "ParticleInstanceModifierData",
    /* structSize */ sizeof(ParticleInstanceModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
