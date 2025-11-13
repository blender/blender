/* SPDX-FileCopyrightText: 2011 AutoCRC (adaptive time step).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "DNA_material_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "BLT_translation.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#ifdef RNA_RUNTIME
static const EnumPropertyItem part_from_items[] = {
    {PART_FROM_VERT, "VERT", 0, "Vertices", ""},
    {PART_FROM_FACE, "FACE", 0, "Faces", ""},
    {PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

#ifndef RNA_RUNTIME
static const EnumPropertyItem part_reactor_from_items[] = {
    {PART_FROM_VERT, "VERT", 0, "Vertices", ""},
    {PART_FROM_FACE, "FACE", 0, "Faces", ""},
    {PART_FROM_VOLUME, "VOLUME", 0, "Volume", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem part_dist_items[] = {
    {PART_DISTR_JIT, "JIT", 0, "Jittered", ""},
    {PART_DISTR_RAND, "RAND", 0, "Random", ""},
    {PART_DISTR_GRID, "GRID", 0, "Grid", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem part_hair_dist_items[] = {
    {PART_DISTR_JIT, "JIT", 0, "Jittered", ""},
    {PART_DISTR_RAND, "RAND", 0, "Random", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem part_draw_as_items[] = {
    {PART_DRAW_NOT, "NONE", 0, "None", ""},
    {PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
    {PART_DRAW_DOT, "DOT", 0, "Point", ""},
    {PART_DRAW_CIRC, "CIRC", 0, "Circle", ""},
    {PART_DRAW_CROSS, "CROSS", 0, "Cross", ""},
    {PART_DRAW_AXIS, "AXIS", 0, "Axis", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem part_hair_draw_as_items[] = {
    {PART_DRAW_NOT, "NONE", 0, "None", ""},
    {PART_DRAW_REND, "RENDER", 0, "Rendered", ""},
    {PART_DRAW_PATH, "PATH", 0, "Path", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem part_ren_as_items[] = {
    {PART_DRAW_NOT, "NONE", 0, "None", ""},
    {PART_DRAW_HALO, "HALO", 0, "Halo", ""},
    {PART_DRAW_LINE, "LINE", 0, "Line", ""},
    {PART_DRAW_PATH, "PATH", 0, "Path", ""},
    {PART_DRAW_OB, "OBJECT", 0, "Object", ""},
    {PART_DRAW_GR, "COLLECTION", 0, "Collection", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem part_hair_ren_as_items[] = {
    {PART_DRAW_NOT, "NONE", 0, "None", ""},
    {PART_DRAW_PATH, "PATH", 0, "Path", ""},
    {PART_DRAW_OB, "OBJECT", 0, "Object", ""},
    {PART_DRAW_GR, "COLLECTION", 0, "Collection", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem part_type_items[] = {
    {PART_EMITTER, "EMITTER", 0, "Emitter", ""},
    // {PART_REACTOR, "REACTOR", 0, "Reactor", ""},
    {PART_HAIR, "HAIR", 0, "Hair", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem part_fluid_type_items[] = {
    {PART_FLUID, "FLUID", 0, "Fluid", ""},
    {PART_FLUID_FLIP, "FLIP", 0, "Liquid", ""},
    {PART_FLUID_SPRAY, "SPRAY", 0, "Spray", ""},
    {PART_FLUID_BUBBLE, "BUBBLE", 0, "Bubble", ""},
    {PART_FLUID_FOAM, "FOAM", 0, "Foam", ""},
    {PART_FLUID_TRACER, "TRACER", 0, "Tracer", ""},
    {PART_FLUID_SPRAYFOAM, "SPRAYFOAM", 0, "Spray-Foam", ""},
    {PART_FLUID_SPRAYBUBBLE, "SPRAYBUBBLE", 0, "Spray-Bubble", ""},
    {PART_FLUID_FOAMBUBBLE, "FOAMBUBBLE", 0, "Foam-Bubble", ""},
    {PART_FLUID_SPRAYFOAMBUBBLE, "SPRAYFOAMBUBBLE", 0, "Spray-Foam-Bubble", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_string_utils.hh"

#  include "DNA_cloth_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"

#  include "BLI_math_matrix.h"
#  include "BLI_math_vector.h"

#  include "BKE_boids.h"
#  include "BKE_cloth.hh"
#  include "BKE_context.hh"
#  include "BKE_customdata.hh"
#  include "BKE_deform.hh"
#  include "BKE_effect.h"
#  include "BKE_material.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_legacy_convert.hh"
#  include "BKE_modifier.hh"
#  include "BKE_particle.h"
#  include "BKE_pointcache.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

/* use for object space hair get/set */
static void rna_ParticleHairKey_location_object_info(PointerRNA *ptr,
                                                     ParticleSystemModifierData **psmd_pt,
                                                     ParticleData **pa_pt)
{
  HairKey *hkey = (HairKey *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;
  ModifierData *md;
  ParticleSystemModifierData *psmd = nullptr;
  ParticleSystem *psys;
  ParticleData *pa;
  int i;

  *psmd_pt = nullptr;
  *pa_pt = nullptr;

  /* given the pointer HairKey *hkey, we iterate over all particles in all
   * particle systems in the object "ob" in order to find
   * - the ParticleSystemData to which the HairKey (and hence the particle)
   *   belongs (will be stored in psmd_pt)
   * - the ParticleData to which the HairKey belongs (will be stored in pa_pt)
   *
   * not a very efficient way of getting hair key location data,
   * but it's the best we've got at the present
   *
   * IDEAS: include additional information in PointerRNA beforehand,
   * for example a pointer to the ParticleSystemModifierData to which the
   * hair-key belongs.
   */

  for (md = static_cast<ModifierData *>(ob->modifiers.first); md; md = md->next) {
    if (md->type == eModifierType_ParticleSystem) {
      psmd = (ParticleSystemModifierData *)md;
      if (psmd && psmd->mesh_final && psmd->psys) {
        psys = psmd->psys;
        for (i = 0, pa = psys->particles; i < psys->totpart; i++, pa++) {
          /* Hair-keys are stored sequentially in memory, so we can
           * find if it's the same particle by comparing pointers,
           * without having to iterate over them all. */
          if ((hkey >= pa->hair) && (hkey < pa->hair + pa->totkey)) {
            *psmd_pt = psmd;
            *pa_pt = pa;
            return;
          }
        }
      }
    }
  }
}

static void rna_ParticleHairKey_location_object_get(PointerRNA *ptr, float *values)
{
  HairKey *hkey = (HairKey *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystemModifierData *psmd;
  ParticleData *pa;

  rna_ParticleHairKey_location_object_info(ptr, &psmd, &pa);

  if (pa) {
    Mesh *hair_mesh = (psmd->psys->flag & PSYS_HAIR_DYNAMICS) ? psmd->psys->hair_out_mesh :
                                                                nullptr;
    if (hair_mesh) {
      const blender::Span<blender::float3> positions = hair_mesh->vert_positions();
      copy_v3_v3(values, positions[pa->hair_index + (hkey - pa->hair)]);
    }
    else {
      float hairmat[4][4];
      psys_mat_hair_to_object(ob, psmd->mesh_final, psmd->psys->part->from, pa, hairmat);
      copy_v3_v3(values, hkey->co);
      mul_m4_v3(hairmat, values);
    }
  }
  else {
    zero_v3(values);
  }
}

/* Helper function which returns index of the given hair_key in particle which owns it.
 * Works with cases when hair_key is coming from the particle which was passed here, and from the
 * original particle of the given one.
 *
 * Such trickery is needed to allow modification of hair keys in the original object using
 * evaluated particle and object to access proper hair matrix. */
static int hair_key_index_get(const Object *object,
                              /*const*/ HairKey *hair_key,
                              /*const*/ ParticleSystemModifierData *modifier,
                              /*const*/ ParticleData *particle)
{
  if (ARRAY_HAS_ITEM(hair_key, particle->hair, particle->totkey)) {
    return hair_key - particle->hair;
  }

  const ParticleSystem *particle_system = modifier->psys;
  const int particle_index = particle - particle_system->particles;

  const ParticleSystemModifierData *original_modifier = (ParticleSystemModifierData *)
      BKE_modifier_get_original(object, &modifier->modifier);
  const ParticleSystem *original_particle_system = original_modifier->psys;
  const ParticleData *original_particle = &original_particle_system->particles[particle_index];

  if (ARRAY_HAS_ITEM(hair_key, original_particle->hair, original_particle->totkey)) {
    return hair_key - original_particle->hair;
  }

  return -1;
}

/* Set hair_key->co to the given coordinate in object space (the given coordinate will be
 * converted to the proper space).
 *
 * The hair_key can be coming from both original and evaluated object. Object, modifier and
 * particle are to be from evaluated object, so that all the data needed for hair matrix is
 * present. */
static void hair_key_location_object_set(HairKey *hair_key,
                                         Object *object,
                                         ParticleSystemModifierData *modifier,
                                         ParticleData *particle,
                                         const float src_co[3])
{
  Mesh *hair_mesh = (modifier->psys->flag & PSYS_HAIR_DYNAMICS) ? modifier->psys->hair_out_mesh :
                                                                  nullptr;

  if (hair_mesh != nullptr) {
    const int hair_key_index = hair_key_index_get(object, hair_key, modifier, particle);
    if (hair_key_index == -1) {
      return;
    }
    blender::MutableSpan<blender::float3> positions = hair_mesh->vert_positions_for_write();
    copy_v3_v3(positions[particle->hair_index + (hair_key_index)], src_co);
    return;
  }

  float hairmat[4][4];
  psys_mat_hair_to_object(
      object, modifier->mesh_final, modifier->psys->part->from, particle, hairmat);

  float imat[4][4];
  invert_m4_m4(imat, hairmat);

  copy_v3_v3(hair_key->co, src_co);
  mul_m4_v3(imat, hair_key->co);
}

static void rna_ParticleHairKey_location_object_set(PointerRNA *ptr, const float *values)
{
  HairKey *hkey = (HairKey *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;

  ParticleSystemModifierData *psmd;
  ParticleData *pa;
  rna_ParticleHairKey_location_object_info(ptr, &psmd, &pa);

  if (pa == nullptr) {
    zero_v3(hkey->co);
    return;
  }

  hair_key_location_object_set(hkey, ob, psmd, pa, values);
}

static void rna_ParticleHairKey_co_object(HairKey *hairkey,
                                          Object *object,
                                          ParticleSystemModifierData *modifier,
                                          ParticleData *particle,
                                          float n_co[3])
{

  Mesh *hair_mesh = (modifier->psys->flag & PSYS_HAIR_DYNAMICS) ? modifier->psys->hair_out_mesh :
                                                                  nullptr;
  if (particle) {
    if (hair_mesh) {
      const blender::Span<blender::float3> positions = hair_mesh->vert_positions();
      copy_v3_v3(n_co, positions[particle->hair_index + (hairkey - particle->hair)]);
    }
    else {
      float hairmat[4][4];
      psys_mat_hair_to_object(
          object, modifier->mesh_final, modifier->psys->part->from, particle, hairmat);
      copy_v3_v3(n_co, hairkey->co);
      mul_m4_v3(hairmat, n_co);
    }
  }
  else {
    zero_v3(n_co);
  }
}

static void rna_ParticleHairKey_co_object_set(ID *id,
                                              HairKey *hair_key,
                                              Object *object,
                                              ParticleSystemModifierData *modifier,
                                              ParticleData *particle,
                                              const float co[3])
{

  if (particle == nullptr) {
    return;
  }

  /* Mark particle system as edited, so then particle_system_update() does not reset the hair
   * keys from path. This behavior is similar to how particle edit mode sets flags. */
  ParticleSystemModifierData *orig_modifier = (ParticleSystemModifierData *)
      BKE_modifier_get_original(object, &modifier->modifier);
  orig_modifier->psys->flag |= PSYS_EDITED;

  hair_key_location_object_set(hair_key, object, modifier, particle, co);

  /* Tag similar to brushes in particle edit mode, so the modifier stack is properly evaluated
   * with the same particle system recalc flags as during combing. */
  DEG_id_tag_update(id, ID_RECALC_GEOMETRY | ID_RECALC_PSYS_REDO);
}

static void rna_Particle_uv_on_emitter(ParticleData *particle,
                                       ReportList *reports,
                                       ParticleSystemModifierData *modifier,
                                       float r_uv[2])
{
#  if 0
  psys_particle_on_emitter(
      psmd, part->from, pa->num, pa->num_dmcache, pa->fuv, pa->foffset, co, nor, 0, 0, sd.orco, 0);
#  endif

  if (modifier->mesh_final == nullptr) {
    BKE_report(reports, RPT_ERROR, "uv_on_emitter() requires a modifier from an evaluated object");
    return;
  }

  /* Get UV-coordinate & color. */
  int num = particle->num_dmcache;
  int from = modifier->psys->part->from;

  if (modifier->mesh_final->uv_map_names().is_empty()) {
    BKE_report(reports, RPT_ERROR, "Mesh has no UV data");
    return;
  }
  BKE_mesh_tessface_ensure(modifier->mesh_final); /* BMESH - UNTIL MODIFIER IS UPDATED FOR POLYS */
#  include "BKE_mesh_legacy_convert.hh"

  if (ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
    if (particle->num < modifier->mesh_final->totface_legacy) {
      num = particle->num;
    }
  }

  /* Get UV-coordinate. */
  if (r_uv && ELEM(from, PART_FROM_FACE, PART_FROM_VOLUME) &&
      !ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD))
  {

    const MFace *mface = static_cast<const MFace *>(
        CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MFACE));
    const MTFace *mtface = static_cast<const MTFace *>(
        CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MTFACE));

    if (mface && mtface) {
      mtface += num;
      psys_interpolate_uvs(mtface, mface->v4, particle->fuv, r_uv);
      return;
    }
  }

  r_uv[0] = 0.0f;
  r_uv[1] = 0.0f;
}

static void rna_ParticleSystem_co_hair(
    ParticleSystem *particlesystem, Object *object, int particle_no, int step, float n_co[3])
{
  ParticleSettings *part = nullptr;
  ParticleData *pars = nullptr;
  ParticleCacheKey *cache = nullptr;
  int totchild = 0;
  int totpart;
  int max_k = 0;

  if (particlesystem == nullptr) {
    return;
  }

  part = particlesystem->part;
  pars = particlesystem->particles;
  totpart = particlesystem->totcached;
  totchild = particlesystem->totchildcache;

  if (part == nullptr || pars == nullptr) {
    return;
  }

  if (ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR, PART_DRAW_NOT)) {
    return;
  }

  /* can happen for disconnected/global hair */
  if (part->type == PART_HAIR && !particlesystem->childcache) {
    totchild = 0;
  }

  if (particle_no < totpart && particlesystem->pathcache) {
    cache = particlesystem->pathcache[particle_no];
    max_k = int(cache->segments);
  }
  else if (particle_no < totpart + totchild && particlesystem->childcache) {
    cache = particlesystem->childcache[particle_no - totpart];

    if (cache->segments < 0) {
      max_k = 0;
    }
    else {
      max_k = int(cache->segments);
    }
  }
  else {
    return;
  }

  /* Strands key loop data stored in cache + step->co. */
  if (step >= 0 && step <= max_k) {
    copy_v3_v3(n_co, (cache + step)->co);
    mul_m4_v3(particlesystem->imat, n_co);
    mul_m4_v3(object->object_to_world().ptr(), n_co);
  }
}

static const EnumPropertyItem *rna_Particle_Material_itemf(bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{

  ParticleSettings *part = reinterpret_cast<ParticleSettings *>(ptr->owner_id);

  /* The context object might not be what we want when doing this from python. */
  Object *ob_found = nullptr;

  if (Object *ob_context = static_cast<Object *>(CTX_data_pointer_get(C, "object").data)) {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob_context->particlesystem) {
      if (psys->part == part) {
        ob_found = ob_context;
        break;
      }
    }
  }

  if (ob_found == nullptr) {
    /* Iterating over all object is slow, but no better solution exists at the moment. */
    for (Object *ob = static_cast<Object *>(CTX_data_main(C)->objects.first);
         ob && (ob_found == nullptr);
         ob = static_cast<Object *>(ob->id.next))
    {
      LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
        if (psys->part == part) {
          ob_found = ob;
          break;
        }
      }
    }
  }

  Material *ma;
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;
  int i;

  if (ob_found && ob_found->totcol > 0) {
    for (i = 1; i <= ob_found->totcol; i++) {
      ma = BKE_object_material_get(ob_found, i);
      tmp.value = i;
      tmp.icon = ICON_MATERIAL_DATA;
      if (ma) {
        tmp.name = ma->id.name + 2;
        tmp.identifier = tmp.name;
      }
      else {
        tmp.name = "Default Material";
        tmp.identifier = tmp.name;
      }
      RNA_enum_item_add(&item, &totitem, &tmp);
    }
  }
  else {
    tmp.value = 1;
    tmp.icon = ICON_MATERIAL_DATA;
    tmp.name = "Default Material";
    tmp.identifier = tmp.name;
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* return < 0 means invalid (no matching tessellated face could be found). */
static int rna_ParticleSystem_tessfaceidx_on_emitter(ParticleSystem *particlesystem,
                                                     ParticleSystemModifierData *modifier,
                                                     ParticleData *particle,
                                                     int particle_no,
                                                     float (**r_fuv)[4])
{
  ParticleSettings *part = nullptr;
  int totpart;
  int totchild = 0;
  int totface;
  int totvert;
  int num = -1;

  BKE_mesh_tessface_ensure(modifier->mesh_final); /* BMESH - UNTIL MODIFIER IS UPDATED FOR POLYS */
  totface = modifier->mesh_final->totface_legacy;
  totvert = modifier->mesh_final->verts_num;

  /* 1. check that everything is ok & updated */
  if (!particlesystem || !totface) {
    return num;
  }

  part = particlesystem->part;
  /* NOTE: only hair, keyed and baked particles may have cached items... */
  totpart = particlesystem->totcached != 0 ? particlesystem->totcached : particlesystem->totpart;
  totchild = particlesystem->totchildcache != 0 ? particlesystem->totchildcache :
                                                  particlesystem->totchild;

  /* can happen for disconnected/global hair */
  if (part->type == PART_HAIR && !particlesystem->childcache) {
    totchild = 0;
  }

  if (particle_no >= totpart + totchild) {
    return num;
  }

  /* 2. get matching face index. */
  if (particle_no < totpart) {
    num = (ELEM(particle->num_dmcache, DMCACHE_ISCHILD, DMCACHE_NOTFOUND)) ? particle->num :
                                                                             particle->num_dmcache;

    if (ELEM(part->from, PART_FROM_FACE, PART_FROM_VOLUME)) {
      if (num != DMCACHE_NOTFOUND && num < totface) {
        *r_fuv = &particle->fuv;
        return num;
      }
    }
    else if (part->from == PART_FROM_VERT) {
      if (num != DMCACHE_NOTFOUND && num < totvert) {
        const MFace *mface = static_cast<const MFace *>(
            CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MFACE));

        *r_fuv = &particle->fuv;

        /* This finds the first face to contain the emitting vertex,
         * this is not ideal, but is mostly fine as UV seams generally
         * map to equal-colored parts of a texture */
        for (int i = 0; i < totface; i++, mface++) {
          if (ELEM(num, mface->v1, mface->v2, mface->v3, mface->v4)) {
            return i;
          }
        }
      }
    }
  }
  else {
    ChildParticle *cpa = particlesystem->child + particle_no - totpart;
    num = cpa->num;

    if (part->childtype == PART_CHILD_FACES) {
      if (ELEM(part->from, PART_FROM_FACE, PART_FROM_VOLUME, PART_FROM_VERT)) {
        if (num != DMCACHE_NOTFOUND && num < totface) {
          *r_fuv = &cpa->fuv;
          return num;
        }
      }
    }
    else {
      ParticleData *parent = particlesystem->particles + cpa->parent;
      num = parent->num_dmcache;

      if (num == DMCACHE_NOTFOUND) {
        num = parent->num;
      }

      if (ELEM(part->from, PART_FROM_FACE, PART_FROM_VOLUME)) {
        if (num != DMCACHE_NOTFOUND && num < totface) {
          *r_fuv = &parent->fuv;
          return num;
        }
      }
      else if (part->from == PART_FROM_VERT) {
        if (num != DMCACHE_NOTFOUND && num < totvert) {
          const MFace *mface = static_cast<const MFace *>(
              CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MFACE));

          *r_fuv = &parent->fuv;

          /* This finds the first face to contain the emitting vertex,
           * this is not ideal, but is mostly fine as UV seams generally
           * map to equal-colored parts of a texture */
          for (int i = 0; i < totface; i++, mface++) {
            if (ELEM(num, mface->v1, mface->v2, mface->v3, mface->v4)) {
              return i;
            }
          }
        }
      }
    }
  }

  return -1;
}

static void rna_ParticleSystem_uv_on_emitter(ParticleSystem *particlesystem,
                                             ReportList *reports,
                                             ParticleSystemModifierData *modifier,
                                             ParticleData *particle,
                                             int particle_no,
                                             int uv_no,
                                             float r_uv[2])
{
  if (modifier->mesh_final == nullptr) {
    BKE_report(reports, RPT_ERROR, "Object was not yet evaluated");
    zero_v2(r_uv);
    return;
  }
  if (modifier->mesh_final->uv_map_names().is_empty()) {
    BKE_report(reports, RPT_ERROR, "Mesh has no UV data");
    zero_v2(r_uv);
    return;
  }

  {
    float (*fuv)[4];
    /* Note all sanity checks are done in this helper func. */
    const int num = rna_ParticleSystem_tessfaceidx_on_emitter(
        particlesystem, modifier, particle, particle_no, &fuv);

    if (num < 0) {
      /* No matching face found. */
      zero_v2(r_uv);
    }
    else {
      const MFace *mfaces = static_cast<const MFace *>(
          CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MFACE));
      const MFace *mface = &mfaces[num];
      const MTFace *mtface = (const MTFace *)CustomData_get_layer_n(
          &modifier->mesh_final->fdata_legacy, CD_MTFACE, uv_no);

      psys_interpolate_uvs(&mtface[num], mface->v4, *fuv, r_uv);
    }
  }
}

static void rna_ParticleSystem_mcol_on_emitter(ParticleSystem *particlesystem,
                                               ReportList *reports,
                                               ParticleSystemModifierData *modifier,
                                               ParticleData *particle,
                                               int particle_no,
                                               int vcol_no,
                                               float r_mcol[3])
{
  if (!CustomData_has_layer(&modifier->mesh_final->corner_data, CD_PROP_BYTE_COLOR)) {
    BKE_report(reports, RPT_ERROR, "Mesh has no VCol data");
    zero_v3(r_mcol);
    return;
  }

  {
    float (*fuv)[4];
    /* Note all sanity checks are done in this helper func. */
    const int num = rna_ParticleSystem_tessfaceidx_on_emitter(
        particlesystem, modifier, particle, particle_no, &fuv);

    if (num < 0) {
      /* No matching face found. */
      zero_v3(r_mcol);
    }
    else {
      const MFace *mfaces = static_cast<const MFace *>(
          CustomData_get_layer(&modifier->mesh_final->fdata_legacy, CD_MFACE));
      const MFace *mface = &mfaces[num];
      const MCol *mc = (const MCol *)CustomData_get_layer_n(
          &modifier->mesh_final->fdata_legacy, CD_MCOL, vcol_no);
      MCol mcol;

      psys_interpolate_mcol(&mc[num * 4], mface->v4, *fuv, &mcol);
      r_mcol[0] = float(mcol.b) / 255.0f;
      r_mcol[1] = float(mcol.g) / 255.0f;
      r_mcol[2] = float(mcol.r) / 255.0f;
    }
  }
}

static void particle_recalc(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr, short flag)
{
  if (ptr->type == &RNA_ParticleSystem) {
    Object *ob = (Object *)ptr->owner_id;
    ParticleSystem *psys = (ParticleSystem *)ptr->data;

    psys->recalc = flag;

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  else {
    DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY | flag);
  }

  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
}
static void rna_Particle_redo(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  particle_recalc(bmain, scene, ptr, ID_RECALC_PSYS_REDO);
}

static void rna_Particle_redo_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);
  rna_Particle_redo(bmain, scene, ptr);
}

static void rna_Particle_redo_count(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->data;
  DEG_relations_tag_update(bmain);
  psys_check_group_weights(part);
  particle_recalc(bmain, scene, ptr, ID_RECALC_PSYS_REDO);
}

static void rna_Particle_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  particle_recalc(bmain, scene, ptr, ID_RECALC_PSYS_RESET);
}

static void rna_Particle_reset_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);
  rna_Particle_reset(bmain, scene, ptr);
}

static void rna_Particle_change_type(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

  /* Iterating over all object is slow, but no better solution exists at the moment. */
  for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      if (psys->part == part) {
        psys_changed_type(ob, psys);
        psys->recalc |= ID_RECALC_PSYS_RESET;
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }
  }

  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
  DEG_relations_tag_update(bmain);
}

static void rna_Particle_change_physics_type(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  particle_recalc(bmain, scene, ptr, ID_RECALC_PSYS_RESET | ID_RECALC_PSYS_PHYS);

  ParticleSettings *part = (ParticleSettings *)ptr->data;

  if (part->phystype == PART_PHYS_BOIDS && part->boids == nullptr) {
    BoidState *state;

    part->boids = MEM_callocN<BoidSettings>("Boid Settings");
    boid_default_settings(part->boids);

    state = boid_new_state(part->boids);
    BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Separate));
    BLI_addtail(&state->rules, boid_new_rule(eBoidRuleType_Flock));

    ((BoidRule *)state->rules.first)->flag |= BOIDRULE_CURRENT;

    state->flag |= BOIDSTATE_CURRENT;
    BLI_addtail(&part->boids->states, state);
  }
  else if (part->phystype == PART_PHYS_FLUID && part->fluid == nullptr) {
    part->fluid = MEM_callocN<SPHFluidSettings>("SPH Fluid Settings");
    BKE_particlesettings_fluid_default_settings(part);
  }

  DEG_relations_tag_update(bmain);
}

static void rna_Particle_redo_child(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  particle_recalc(bmain, scene, ptr, ID_RECALC_PSYS_CHILD);
}

static void rna_Particle_cloth_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
}

static ParticleSystem *rna_particle_system_for_target(Object *ob, ParticleTarget *target)
{
  ParticleSystem *psys;
  ParticleTarget *pt;

  for (psys = static_cast<ParticleSystem *>(ob->particlesystem.first); psys; psys = psys->next) {
    for (pt = static_cast<ParticleTarget *>(psys->targets.first); pt; pt = pt->next) {
      if (pt == target) {
        return psys;
      }
    }
  }

  return nullptr;
}

static void rna_Particle_target_reset(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  if (ptr->type == &RNA_ParticleTarget) {
    Object *ob = (Object *)ptr->owner_id;
    ParticleTarget *pt = (ParticleTarget *)ptr->data;
    ParticleSystem *kpsys = nullptr, *psys = rna_particle_system_for_target(ob, pt);

    if (ELEM(pt->ob, ob, nullptr)) {
      kpsys = static_cast<ParticleSystem *>(BLI_findlink(&ob->particlesystem, pt->psys - 1));

      if (kpsys) {
        pt->flag |= PTARGET_VALID;
      }
      else {
        pt->flag &= ~PTARGET_VALID;
      }
    }
    else {
      if (pt->ob) {
        kpsys = static_cast<ParticleSystem *>(BLI_findlink(&pt->ob->particlesystem, pt->psys - 1));
      }

      if (kpsys) {
        pt->flag |= PTARGET_VALID;
      }
      else {
        pt->flag &= ~PTARGET_VALID;
      }
    }

    psys->recalc = ID_RECALC_PSYS_RESET;

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);
  }

  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
}

static void rna_Particle_target_redo(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  if (ptr->type == &RNA_ParticleTarget) {
    Object *ob = (Object *)ptr->owner_id;
    ParticleTarget *pt = (ParticleTarget *)ptr->data;
    ParticleSystem *psys = rna_particle_system_for_target(ob, pt);

    psys->recalc = ID_RECALC_PSYS_REDO;

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
  }
}

static void rna_Particle_hair_dynamics_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  if (psys && !psys->clmd) {
    psys->clmd = (ClothModifierData *)BKE_modifier_new(eModifierType_Cloth);
    psys->clmd->sim_parms->goalspring = 0.0f;
    psys->clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_RESIST_SPRING_COMPRESS;
    psys->clmd->coll_parms->flags &= ~CLOTH_COLLSETTINGS_FLAG_SELF;
    rna_Particle_redo(bmain, scene, ptr);
  }
  else {
    WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

static PointerRNA rna_particle_settings_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  ParticleSettings *part = psys->part;

  return RNA_id_pointer_create(reinterpret_cast<ID *>(part));
}

static void rna_particle_settings_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  int old_type = 0;

  if (psys->part) {
    old_type = psys->part->type;
    id_us_min(&psys->part->id);
  }

  psys->part = (ParticleSettings *)value.data;

  if (psys->part) {
    id_us_plus(&psys->part->id);
    psys_check_boid_data(psys);
    if (old_type != psys->part->type) {
      psys_changed_type(ob, psys);
    }
  }
}
static void rna_Particle_abspathtime_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  float delta = settings->end + settings->lifetime - settings->sta;
  if (settings->draw & PART_ABS_PATH_TIME) {
    settings->path_start = settings->sta + settings->path_start * delta;
    settings->path_end = settings->sta + settings->path_end * delta;
  }
  else {
    settings->path_start = (settings->path_start - settings->sta) / delta;
    settings->path_end = (settings->path_end - settings->sta) / delta;
  }
  rna_Particle_redo(bmain, scene, ptr);
}
static void rna_PartSettings_start_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;

  /* check for clipping */
  if (value > settings->end) {
    settings->end = value;
  }

#  if 0
  if (settings->type == PART_REACTOR && value < 1.0) {
    value = 1.0;
  }
  else
#  endif
  if (value < MINAFRAMEF) {
    value = MINAFRAMEF;
  }

  settings->sta = value;
}

static void rna_PartSettings_end_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;

  /* check for clipping */
  if (value < settings->sta) {
    settings->sta = value;
  }

  settings->end = value;
}

static void rna_PartSetings_timestep_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;

  settings->timetweak = value / 0.04f;
}

static float rna_PartSettings_timestep_get(PointerRNA *ptr)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;

  return settings->timetweak * 0.04f;
}

static void rna_PartSetting_hairlength_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  settings->normfac = value / 4.0f;
}

static float rna_PartSetting_hairlength_get(PointerRNA *ptr)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  return settings->normfac * 4.0f;
}

static void rna_PartSetting_linelentail_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  settings->draw_line[0] = value;
}

static float rna_PartSetting_linelentail_get(PointerRNA *ptr)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  return settings->draw_line[0];
}
static void rna_PartSetting_pathstartend_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;

  if (settings->type == PART_HAIR) {
    *min = 0.0f;
    *max = (settings->draw & PART_ABS_PATH_TIME) ? 100.0f : 1.0f;
  }
  else {
    *min = (settings->draw & PART_ABS_PATH_TIME) ? settings->sta : 0.0f;
    *max = (settings->draw & PART_ABS_PATH_TIME) ? MAXFRAMEF : 1.0f;
  }
}
static void rna_PartSetting_linelenhead_set(PointerRNA *ptr, float value)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  settings->draw_line[1] = value;
}

static float rna_PartSetting_linelenhead_get(PointerRNA *ptr)
{
  ParticleSettings *settings = (ParticleSettings *)ptr->data;
  return settings->draw_line[1];
}

static bool rna_PartSettings_is_fluid_get(PointerRNA *ptr)
{
  ParticleSettings *part = static_cast<ParticleSettings *>(ptr->data);
  return ELEM(part->type,
              PART_FLUID,
              PART_FLUID_FLIP,
              PART_FLUID_FOAM,
              PART_FLUID_SPRAY,
              PART_FLUID_BUBBLE,
              PART_FLUID_TRACER,
              PART_FLUID_SPRAYFOAM,
              PART_FLUID_SPRAYBUBBLE,
              PART_FLUID_FOAMBUBBLE,
              PART_FLUID_SPRAYFOAMBUBBLE);
}

static void rna_ParticleSettings_use_clump_curve_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ParticleSettings *part = static_cast<ParticleSettings *>(ptr->data);

  if (part->child_flag & PART_CHILD_USE_CLUMP_CURVE) {
    if (!part->clumpcurve) {
      BKE_particlesettings_clump_curve_init(part);
    }
  }

  rna_Particle_redo_child(bmain, scene, ptr);
}

static void rna_ParticleSettings_use_roughness_curve_update(Main *bmain,
                                                            Scene *scene,
                                                            PointerRNA *ptr)
{
  ParticleSettings *part = static_cast<ParticleSettings *>(ptr->data);

  if (part->child_flag & PART_CHILD_USE_ROUGH_CURVE) {
    if (!part->roughcurve) {
      BKE_particlesettings_rough_curve_init(part);
    }
  }

  rna_Particle_redo_child(bmain, scene, ptr);
}

static void rna_ParticleSettings_use_twist_curve_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ParticleSettings *part = static_cast<ParticleSettings *>(ptr->data);

  if (part->child_flag & PART_CHILD_USE_TWIST_CURVE) {
    if (!part->twistcurve) {
      BKE_particlesettings_twist_curve_init(part);
    }
  }

  rna_Particle_redo_child(bmain, scene, ptr);
}

static void rna_ParticleSystem_name_set(PointerRNA *ptr, const char *value)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *part = (ParticleSystem *)ptr->data;

  /* copy the new name into the name slot */
  STRNCPY_UTF8(part->name, value);

  BLI_uniquename(&ob->particlesystem,
                 part,
                 DATA_("ParticleSystem"),
                 '.',
                 offsetof(ParticleSystem, name),
                 sizeof(part->name));
}

static PointerRNA rna_ParticleSystem_active_particle_target_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  ParticleTarget *pt = static_cast<ParticleTarget *>(psys->targets.first);

  for (; pt; pt = pt->next) {
    if (pt->flag & PTARGET_CURRENT) {
      return RNA_pointer_create_with_parent(*ptr, &RNA_ParticleTarget, pt);
    }
  }
  return PointerRNA_NULL;
}
static void rna_ParticleSystem_active_particle_target_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&psys->targets) - 1);
}

static int rna_ParticleSystem_active_particle_target_index_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  ParticleTarget *pt = static_cast<ParticleTarget *>(psys->targets.first);
  int i = 0;

  for (; pt; pt = pt->next, i++) {
    if (pt->flag & PTARGET_CURRENT) {
      return i;
    }
  }

  return 0;
}

static void rna_ParticleSystem_active_particle_target_index_set(PointerRNA *ptr, int value)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  ParticleTarget *pt = static_cast<ParticleTarget *>(psys->targets.first);
  int i = 0;

  for (; pt; pt = pt->next, i++) {
    if (i == value) {
      pt->flag |= PTARGET_CURRENT;
    }
    else {
      pt->flag &= ~PTARGET_CURRENT;
    }
  }
}

static size_t rna_ParticleTarget_name_get_impl(PointerRNA *ptr,
                                               char *value,
                                               const size_t value_maxncpy)
{
  ParticleTarget *pt = static_cast<ParticleTarget *>(ptr->data);

  if (pt->flag & PTARGET_VALID) {
    ParticleSystem *psys = nullptr;

    if (pt->ob) {
      psys = static_cast<ParticleSystem *>(BLI_findlink(&pt->ob->particlesystem, pt->psys - 1));
    }
    else {
      Object *ob = (Object *)ptr->owner_id;
      psys = static_cast<ParticleSystem *>(BLI_findlink(&ob->particlesystem, pt->psys - 1));
    }

    if (psys) {
      int value_len;
      if (pt->ob) {
        value_len = BLI_snprintf_rlen(
            value, value_maxncpy, "%s: %s", pt->ob->id.name + 2, psys->name);
      }
      else {
        value_len = BLI_strncpy_rlen(value, psys->name, value_maxncpy);
      }
      return value_len;
    }
  }

  return BLI_strncpy_rlen(value, RPT_("Invalid target!"), value_maxncpy);
}

static void rna_ParticleTarget_name_get(PointerRNA *ptr, char *value)
{
  char value_buf[MAX_ID_NAME + MAX_ID_NAME + 64];
  const size_t value_buf_len = rna_ParticleTarget_name_get_impl(ptr, value_buf, sizeof(value_buf));
  memcpy(value, value_buf, value_buf_len + 1);
}

static int rna_ParticleTarget_name_length(PointerRNA *ptr)
{
  char value_buf[MAX_ID_NAME + MAX_ID_NAME + 64];
  return rna_ParticleTarget_name_get_impl(ptr, value_buf, sizeof(value_buf));
}

static int particle_id_check(const PointerRNA *ptr)
{
  const ID *id = ptr->owner_id;

  return (GS(id->name) == ID_PA);
}

static std::optional<std::string> rna_SPHFluidSettings_path(const PointerRNA *ptr)
{
  const SPHFluidSettings *fluid = (SPHFluidSettings *)ptr->data;

  if (particle_id_check(ptr)) {
    const ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

    if (part->fluid == fluid) {
      return "fluid";
    }
  }
  return std::nullopt;
}

static bool rna_ParticleSystem_multiple_caches_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  return (psys->ptcaches.first != psys->ptcaches.last);
}
static bool rna_ParticleSystem_editable_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  return psys_check_edited(psys);
}
static bool rna_ParticleSystem_edited_get(PointerRNA *ptr)
{
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  if (psys->part && psys->part->type == PART_HAIR) {
    return (psys->flag & PSYS_EDITED || (psys->edit && psys->edit->edited));
  }
  else {
    return (psys->pointcache->edit && psys->pointcache->edit->edited);
  }
}
static PointerRNA rna_ParticleDupliWeight_active_get(PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  ParticleDupliWeight *dw = static_cast<ParticleDupliWeight *>(part->instance_weights.first);

  for (; dw; dw = dw->next) {
    if (dw->flag & PART_DUPLIW_CURRENT) {
      return RNA_pointer_create_with_parent(*ptr, &RNA_ParticleDupliWeight, dw);
    }
  }
  return PointerRNA_NULL;
}
static void rna_ParticleDupliWeight_active_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&part->instance_weights) - 1);
}

static int rna_ParticleDupliWeight_active_index_get(PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  ParticleDupliWeight *dw = static_cast<ParticleDupliWeight *>(part->instance_weights.first);
  int i = 0;

  for (; dw; dw = dw->next, i++) {
    if (dw->flag & PART_DUPLIW_CURRENT) {
      return i;
    }
  }

  return 0;
}

static void rna_ParticleDupliWeight_active_index_set(PointerRNA *ptr, int value)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  ParticleDupliWeight *dw = static_cast<ParticleDupliWeight *>(part->instance_weights.first);
  int i = 0;

  for (; dw; dw = dw->next, i++) {
    if (i == value) {
      dw->flag |= PART_DUPLIW_CURRENT;
    }
    else {
      dw->flag &= ~PART_DUPLIW_CURRENT;
    }
  }
}

static size_t rna_ParticleDupliWeight_name_get_impl(PointerRNA *ptr,
                                                    char *value,
                                                    const size_t value_maxncpy)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  psys_find_group_weights(part);

  ParticleDupliWeight *dw = static_cast<ParticleDupliWeight *>(ptr->data);

  if (dw->ob) {
    return BLI_snprintf_utf8_rlen(value, value_maxncpy, "%s: %i", dw->ob->id.name + 2, dw->count);
  }

  return BLI_strncpy_utf8_rlen(value, "No object", value_maxncpy);
}

static void rna_ParticleDupliWeight_name_get(PointerRNA *ptr, char *value)
{
  char value_buf[MAX_ID_NAME + 64];
  const size_t value_buf_len = rna_ParticleDupliWeight_name_get_impl(
      ptr, value_buf, sizeof(value_buf));
  memcpy(value, value_buf, value_buf_len + 1);
}

static int rna_ParticleDupliWeight_name_length(PointerRNA *ptr)
{
  char value_buf[MAX_ID_NAME + 64];
  return rna_ParticleDupliWeight_name_get_impl(ptr, value_buf, sizeof(value_buf));
}

static const EnumPropertyItem *rna_Particle_type_itemf(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool * /*r_free*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

  if (ELEM(part->type, PART_HAIR, PART_EMITTER)) {
    return part_type_items;
  }
  else {
    return part_fluid_type_items;
  }
}

static const EnumPropertyItem *rna_Particle_from_itemf(bContext * /*C*/,
                                                       PointerRNA * /*ptr*/,
                                                       PropertyRNA * /*prop*/,
                                                       bool * /*r_free*/)
{
#  if 0
  if (part->type == PART_REACTOR) {
    return part_reactor_from_items;
  }
#  endif
  return part_from_items;
}

static const EnumPropertyItem *rna_Particle_dist_itemf(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool * /*r_free*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

  if (part->type == PART_HAIR) {
    return part_hair_dist_items;
  }
  else {
    return part_dist_items;
  }
}

static const EnumPropertyItem *rna_Particle_draw_as_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool * /*r_free*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

  if (part->type == PART_HAIR) {
    return part_hair_draw_as_items;
  }
  else {
    return part_draw_as_items;
  }
}

static const EnumPropertyItem *rna_Particle_ren_as_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool * /*r_free*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;

  if (part->type == PART_HAIR) {
    return part_hair_ren_as_items;
  }
  else {
    return part_ren_as_items;
  }
}

static PointerRNA rna_Particle_field1_get(PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  return RNA_pointer_create_with_parent(*ptr, &RNA_FieldSettings, part->pd);
}

static PointerRNA rna_Particle_field2_get(PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->owner_id;
  return RNA_pointer_create_with_parent(*ptr, &RNA_FieldSettings, part->pd2);
}

static void psys_vg_name_get__internal(PointerRNA *ptr, char *value, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = (ParticleSystem *)ptr->data;
  const ListBase *defbase = BKE_object_defgroup_list(ob);

  if (psys->vgroup[index] > 0) {
    bDeformGroup *defGroup = static_cast<bDeformGroup *>(
        BLI_findlink(defbase, psys->vgroup[index] - 1));

    if (defGroup) {
      strcpy(value, defGroup->name);
      return;
    }
  }

  value[0] = '\0';
}
static int psys_vg_name_len__internal(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  if (psys->vgroup[index] > 0) {
    const ListBase *defbase = BKE_object_defgroup_list(ob);
    bDeformGroup *defGroup = static_cast<bDeformGroup *>(
        BLI_findlink(defbase, psys->vgroup[index] - 1));

    if (defGroup) {
      return strlen(defGroup->name);
    }
  }
  return 0;
}
static void psys_vg_name_set__internal(PointerRNA *ptr, const char *value, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = (ParticleSystem *)ptr->data;

  if (value[0] == '\0') {
    psys->vgroup[index] = 0;
  }
  else {
    int defgrp_index = BKE_object_defgroup_name_index(ob, value);

    if (defgrp_index == -1) {
      return;
    }

    psys->vgroup[index] = defgrp_index + 1;
  }
}

static std::optional<std::string> rna_ParticleSystem_path(const PointerRNA *ptr)
{
  const ParticleSystem *psys = (ParticleSystem *)ptr->data;
  char name_esc[sizeof(psys->name) * 2];

  BLI_str_escape(name_esc, psys->name, sizeof(name_esc));
  return fmt::format("particle_systems[\"{}\"]", name_esc);
}

static void rna_ParticleSettings_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->data;
  rna_iterator_array_begin(iter, ptr, (void *)part->mtex, sizeof(MTex *), MAX_MTEX, 0, nullptr);
}

static PointerRNA rna_ParticleSettings_active_texture_get(PointerRNA *ptr)
{
  ParticleSettings *part = (ParticleSettings *)ptr->data;
  Tex *tex;

  tex = give_current_particle_texture(part);
  return RNA_id_pointer_create(reinterpret_cast<ID *>(tex));
}

static void rna_ParticleSettings_active_texture_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList * /*reports*/)
{
  ParticleSettings *part = (ParticleSettings *)ptr->data;

  set_current_particle_texture(part, static_cast<Tex *>(value.data));
}

/* irritating string functions for each index :/ */
static void rna_ParticleVGroup_name_get_0(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 0);
}
static void rna_ParticleVGroup_name_get_1(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 1);
}
static void rna_ParticleVGroup_name_get_2(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 2);
}
static void rna_ParticleVGroup_name_get_3(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 3);
}
static void rna_ParticleVGroup_name_get_4(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 4);
}
static void rna_ParticleVGroup_name_get_5(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 5);
}
static void rna_ParticleVGroup_name_get_6(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 6);
}
static void rna_ParticleVGroup_name_get_7(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 7);
}
static void rna_ParticleVGroup_name_get_8(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 8);
}
static void rna_ParticleVGroup_name_get_9(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 9);
}
static void rna_ParticleVGroup_name_get_10(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 10);
}
static void rna_ParticleVGroup_name_get_11(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 11);
}
static void rna_ParticleVGroup_name_get_12(PointerRNA *ptr, char *value)
{
  psys_vg_name_get__internal(ptr, value, 12);
}

static int rna_ParticleVGroup_name_len_0(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 0);
}
static int rna_ParticleVGroup_name_len_1(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 1);
}
static int rna_ParticleVGroup_name_len_2(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 2);
}
static int rna_ParticleVGroup_name_len_3(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 3);
}
static int rna_ParticleVGroup_name_len_4(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 4);
}
static int rna_ParticleVGroup_name_len_5(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 5);
}
static int rna_ParticleVGroup_name_len_6(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 6);
}
static int rna_ParticleVGroup_name_len_7(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 7);
}
static int rna_ParticleVGroup_name_len_8(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 8);
}
static int rna_ParticleVGroup_name_len_9(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 9);
}
static int rna_ParticleVGroup_name_len_10(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 10);
}
static int rna_ParticleVGroup_name_len_11(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 11);
}
static int rna_ParticleVGroup_name_len_12(PointerRNA *ptr)
{
  return psys_vg_name_len__internal(ptr, 12);
}

static void rna_ParticleVGroup_name_set_0(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 0);
}
static void rna_ParticleVGroup_name_set_1(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 1);
}
static void rna_ParticleVGroup_name_set_2(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 2);
}
static void rna_ParticleVGroup_name_set_3(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 3);
}
static void rna_ParticleVGroup_name_set_4(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 4);
}
static void rna_ParticleVGroup_name_set_5(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 5);
}
static void rna_ParticleVGroup_name_set_6(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 6);
}
static void rna_ParticleVGroup_name_set_7(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 7);
}
static void rna_ParticleVGroup_name_set_8(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 8);
}
static void rna_ParticleVGroup_name_set_9(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 9);
}
static void rna_ParticleVGroup_name_set_10(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 10);
}
static void rna_ParticleVGroup_name_set_11(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 11);
}
static void rna_ParticleVGroup_name_set_12(PointerRNA *ptr, const char *value)
{
  psys_vg_name_set__internal(ptr, value, 12);
}

#else

static void rna_def_particle_hair_key(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "ParticleHairKey", nullptr);
  RNA_def_struct_sdna(srna, "HairKey");
  RNA_def_struct_ui_text(srna, "Particle Hair Key", "Particle key for hair particle system");

  prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Time", "Relative time of key over hair length");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(prop, "Weight", "Weight for cloth simulation");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Location (Object Space)", "Location of the hair key in object space");
  RNA_def_property_float_funcs(prop,
                               "rna_ParticleHairKey_location_object_get",
                               "rna_ParticleHairKey_location_object_set",
                               nullptr);

  prop = RNA_def_property(srna, "co_local", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "co");
  RNA_def_property_ui_text(prop,
                           "Location",
                           "Location of the hair key in its local coordinate system, "
                           "relative to the emitting face");

  /* Aided co func */
  func = RNA_def_function(srna, "co_object", "rna_ParticleHairKey_co_object");
  RNA_def_function_ui_description(func, "Obtain hairkey location with particle and modifier data");
  parm = RNA_def_pointer(func, "object", "Object", "", "Object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "modifier", "ParticleSystemModifier", "", "Particle modifier");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "particle", "Particle", "", "hair particle");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_vector(
      func, "co", 3, nullptr, -FLT_MAX, FLT_MAX, "Co", "Exported hairkey location", -1e4, 1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "co_object_set", "rna_ParticleHairKey_co_object_set");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Set hairkey location with particle and modifier data");
  parm = RNA_def_pointer(func, "object", "Object", "", "Object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "modifier", "ParticleSystemModifier", "", "Particle modifier");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "particle", "Particle", "", "hair particle");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_float_vector(
      func, "co", 3, nullptr, -FLT_MAX, FLT_MAX, "Co", "Specified hairkey location", -1e4, 1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, PARM_REQUIRED);
}

static void rna_def_particle_key(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ParticleKey", nullptr);
  RNA_def_struct_ui_text(srna, "Particle Key", "Key location for a particle over time");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "co");
  RNA_def_property_ui_text(prop, "Location", "Key location");

  prop = RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "vel");
  RNA_def_property_ui_text(prop, "Velocity", "Key velocity");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "rot");
  RNA_def_property_ui_text(prop, "Rotation", "Key rotation quaternion");

  prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "ave");
  RNA_def_property_ui_text(prop, "Angular Velocity", "Key angular velocity");

  prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Time", "Time of key over the simulation");
}

static void rna_def_child_particle(BlenderRNA *brna)
{
  StructRNA *srna;
  // PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ChildParticle", nullptr);
  RNA_def_struct_ui_text(
      srna, "Child Particle", "Child particle interpolated from simulated or edited particles");

  // int num, parent;    /* num is face index on the final derived mesh */
  //
  // int pa[4];                /* nearest particles to the child, used for the interpolation */
  // float w[4];               /* interpolation weights for the above particles */
  // float fuv[4], foffset;    /* face vertex weights and offset */
  // float rand[3];
}

static void rna_def_particle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem alive_items[] = {
      // {PARS_KILLED, "KILLED", 0, "Killed", ""},
      {PARS_DEAD, "DEAD", 0, "Dead", ""},
      {PARS_UNBORN, "UNBORN", 0, "Unborn", ""},
      {PARS_ALIVE, "ALIVE", 0, "Alive", ""},
      {PARS_DYING, "DYING", 0, "Dying", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Particle", nullptr);
  RNA_def_struct_sdna(srna, "ParticleData");
  RNA_def_struct_ui_text(srna, "Particle", "Particle in a particle system");

  /* Particle State & Previous State */
  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "state.co");
  RNA_def_property_ui_text(prop, "Particle Location", "");

  prop = RNA_def_property(srna, "velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "state.vel");
  RNA_def_property_ui_text(prop, "Particle Velocity", "");

  prop = RNA_def_property(srna, "angular_velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "state.ave");
  RNA_def_property_ui_text(prop, "Angular Velocity", "");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "state.rot");
  RNA_def_property_ui_text(prop, "Rotation", "");

  prop = RNA_def_property(srna, "prev_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "prev_state.co");
  RNA_def_property_ui_text(prop, "Previous Particle Location", "");

  prop = RNA_def_property(srna, "prev_velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "prev_state.vel");
  RNA_def_property_ui_text(prop, "Previous Particle Velocity", "");

  prop = RNA_def_property(srna, "prev_angular_velocity", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "prev_state.ave");
  RNA_def_property_ui_text(prop, "Previous Angular Velocity", "");

  prop = RNA_def_property(srna, "prev_rotation", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "prev_state.rot");
  RNA_def_property_ui_text(prop, "Previous Rotation", "");

  /* Hair & Keyed Keys */

  prop = RNA_def_property(srna, "hair_keys", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "hair", "totkey");
  RNA_def_property_struct_type(prop, "ParticleHairKey");
  RNA_def_property_ui_text(prop, "Hair", "");

  prop = RNA_def_property(srna, "particle_keys", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "keys", "totkey");
  RNA_def_property_struct_type(prop, "ParticleKey");
  RNA_def_property_ui_text(prop, "Keyed States", "");

  // float fuv[4], foffset; /* Coordinates on face/edge number "num" and depth along. */
  //                        /* Face normal for volume emission. */

  prop = RNA_def_property(srna, "birth_time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "time");
  // RNA_def_property_range(prop, lowerLimitf, upperLimitf);
  RNA_def_property_ui_text(prop, "Birth Time", "");

  prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
  // RNA_def_property_range(prop, lowerLimitf, upperLimitf);
  RNA_def_property_ui_text(prop, "Lifetime", "");

  prop = RNA_def_property(srna, "die_time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "dietime");
  // RNA_def_property_range(prop, lowerLimitf, upperLimitf);
  RNA_def_property_ui_text(prop, "Die Time", "");

  prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
  // RNA_def_property_range(prop, lowerLimitf, upperLimitf);
  RNA_def_property_ui_text(prop, "Size", "");

  // int num; /* index to vert/edge/face */
  // int num_dmcache; /* index to derived mesh data (face) to avoid slow lookups */
  // int pad;
  // int totkey;

  /* flag */
  prop = RNA_def_property(srna, "is_exist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", PARS_UNEXIST);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Exists", "");

  prop = RNA_def_property(srna, "is_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", PARS_NO_DISP);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Visible", "");

  prop = RNA_def_property(srna, "alive_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "alive");
  RNA_def_property_enum_items(prop, alive_items);
  RNA_def_property_ui_text(prop, "Alive State", "");

  // short rt2;

  /* UVs */
  func = RNA_def_function(srna, "uv_on_emitter", "rna_Particle_uv_on_emitter");
  RNA_def_function_ui_description(func,
                                  "Obtain UV coordinates for a particle on an evaluated mesh.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func,
                         "modifier",
                         "ParticleSystemModifier",
                         "",
                         "Particle modifier from an evaluated object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_property(func, "uv", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_array(parm, 2);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

static void rna_def_particle_dupliweight(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ParticleDupliWeight", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Particle Instance Object Weight",
                         "Weight of a particle instance object in a collection");
  RNA_def_struct_sdna(srna, "ParticleDupliWeight");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ParticleDupliWeight_name_get", "rna_ParticleDupliWeight_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Particle instance object name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Count", "The number of times this object is repeated with respect to other objects");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");
}

static void rna_def_fluid_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem sph_solver_items[] = {
      {SPH_SOLVER_DDR,
       "DDR",
       0,
       "Double-Density",
       "An artistic solver with strong surface tension effects (original)"},
      {SPH_SOLVER_CLASSICAL, "CLASSICAL", 0, "Classical", "A more physically-accurate solver"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SPHFluidSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_SPHFluidSettings_path");
  RNA_def_struct_ui_text(srna, "SPH Fluid Settings", "Settings for particle fluids physics");

  /* Fluid settings */
  prop = RNA_def_property(srna, "solver", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "solver");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, sph_solver_items);
  RNA_def_property_ui_text(
      prop, "SPH Solver", "The code used to calculate internal forces on particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "spring_force", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "spring_k");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Spring Force", "Spring force");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "fluid_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "radius");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Interaction Radius", "Fluid interaction radius");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "rest_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Rest Length", "Spring rest length (factor of particle radius)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_viscoelastic_springs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_VISCOELASTIC_SPRINGS);
  RNA_def_property_ui_text(
      prop, "Viscoelastic Springs", "Use viscoelastic springs instead of Hooke's springs");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_initial_rest_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_CURRENT_REST_LENGTH);
  RNA_def_property_ui_text(
      prop,
      "Initial Rest Length",
      "Use the initial length as spring rest length instead of 2 * particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "plasticity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "plasticity_constant");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(
      prop,
      "Plasticity",
      "How much the spring rest length can change after the elastic limit is crossed");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "yield_ratio", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "yield_ratio");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Elastic Limit",
      "How much the spring has to be stretched/compressed in order to change its rest length");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "spring_frames", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(
      prop,
      "Spring Frames",
      "Create springs for this number of frames since particles birth (0 is always)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* Viscosity */
  prop = RNA_def_property(srna, "linear_viscosity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "viscosity_omega");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Viscosity", "Linear viscosity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "stiff_viscosity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "viscosity_beta");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Stiff Viscosity", "Creates viscosity for expanding fluid");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* Double density relaxation */
  prop = RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "stiffness_k");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Stiffness", "How incompressible the fluid is (speed of sound)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "repulsion", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "stiffness_knear");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Repulsion Factor",
      "How strongly the fluid tries to keep from clustering (factor of stiffness)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "rest_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rest_density");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Rest Density", "Fluid rest density");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* Buoyancy */
  prop = RNA_def_property(srna, "buoyancy", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "buoyancy");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Buoyancy",
      "Artificial buoyancy force in negative gravity direction based on pressure "
      "differences inside the fluid");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* Factor flags */

  prop = RNA_def_property(srna, "use_factor_repulsion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_FAC_REPULSION);
  RNA_def_property_ui_text(prop, "Factor Repulsion", "Repulsion is a factor of stiffness");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_factor_density", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_FAC_DENSITY);
  RNA_def_property_ui_text(
      prop,
      "Factor Density",
      "Density is calculated as a factor of default density (depends on particle size)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_factor_radius", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_FAC_RADIUS);
  RNA_def_property_ui_text(
      prop, "Factor Radius", "Interaction radius is a factor of 4 * particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_factor_stiff_viscosity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_FAC_VISCOSITY);
  RNA_def_property_ui_text(
      prop, "Factor Stiff Viscosity", "Stiff viscosity is a factor of normal viscosity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_factor_rest_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPH_FAC_REST_LENGTH);
  RNA_def_property_ui_text(
      prop, "Factor Rest Length", "Spring rest length is a factor of 2 * particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");
}

static void rna_def_particle_settings_mtex(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem texco_items[] = {
      {TEXCO_GLOB, "GLOBAL", 0, "Global", "Use global coordinates for the texture coordinates"},
      {TEXCO_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Use linked object's coordinates for texture coordinates"},
      {TEXCO_UV, "UV", 0, "UV", "Use UV coordinates for texture coordinates"},
      {TEXCO_ORCO,
       "ORCO",
       0,
       "Generated",
       "Use the original undeformed coordinates of the object"},
      {TEXCO_STRAND,
       "STRAND",
       0,
       "Strand / Particle",
       "Use normalized strand texture coordinate (1D) or particle age (X) and trail position (Y)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_mapping_items[] = {
      {MTEX_FLAT, "FLAT", 0, "Flat", "Map X and Y coordinates directly"},
      {MTEX_CUBE, "CUBE", 0, "Cube", "Map using the normal vector"},
      {MTEX_TUBE, "TUBE", 0, "Tube", "Map with Z as central axis"},
      {MTEX_SPHERE, "SPHERE", 0, "Sphere", "Map with Z as central axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_x_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_y_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_z_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ParticleSettingsTextureSlot", "TextureSlot");
  RNA_def_struct_sdna(srna, "MTex");
  RNA_def_struct_ui_text(srna,
                         "Particle Settings Texture Slot",
                         "Texture slot for textures in a Particle Settings data-block");

  prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "texco");
  RNA_def_property_enum_items(prop, texco_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXTURE);
  RNA_def_property_ui_text(prop,
                           "Texture Coordinates",
                           "Texture coordinates used to map the texture onto the background");
  RNA_def_property_update(prop, 0, "rna_Particle_reset_dependency");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "object");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Object", "Object to use for mapping with Object texture coordinates");
  RNA_def_property_update(prop, 0, "rna_Particle_reset_dependency");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "uvname");
  RNA_def_property_ui_text(
      prop, "UV Map", "UV map to use for mapping with UV texture coordinates");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "mapping_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "projx");
  RNA_def_property_enum_items(prop, prop_x_mapping_items);
  RNA_def_property_ui_text(prop, "X Mapping", "");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "mapping_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "projy");
  RNA_def_property_enum_items(prop, prop_y_mapping_items);
  RNA_def_property_ui_text(prop, "Y Mapping", "");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "mapping_z", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "projz");
  RNA_def_property_enum_items(prop, prop_z_mapping_items);
  RNA_def_property_ui_text(prop, "Z Mapping", "");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_mapping_items);
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* map to */
  prop = RNA_def_property(srna, "use_map_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_TIME);
  RNA_def_property_ui_text(prop, "Emission Time", "Affect the emission time of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_life", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_LIFE);
  RNA_def_property_ui_text(prop, "Life Time", "Affect the life time of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_density", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_DENS);
  RNA_def_property_ui_text(prop, "Density", "Affect the density of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_SIZE);
  RNA_def_property_ui_text(prop, "Size", "Affect the particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_IVEL);
  RNA_def_property_ui_text(prop, "Initial Velocity", "Affect the particle initial velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_FIELD);
  RNA_def_property_ui_text(prop, "Force Field", "Affect the particle force fields");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_gravity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_GRAVITY);
  RNA_def_property_ui_text(prop, "Gravity", "Affect the particle gravity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_damp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_DAMP);
  RNA_def_property_ui_text(prop, "Damp", "Affect the particle velocity damping");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_map_clump", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_CLUMP);
  RNA_def_property_ui_text(prop, "Clump", "Affect the child clumping");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_map_kink_amp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_KINK_AMP);
  RNA_def_property_ui_text(prop, "Kink Amplitude", "Affect the child kink amplitude");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_map_kink_freq", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_KINK_FREQ);
  RNA_def_property_ui_text(prop, "Kink Frequency", "Affect the child kink frequency");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_map_rough", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_ROUGH);
  RNA_def_property_ui_text(prop, "Rough", "Affect the child rough");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_map_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_LENGTH);
  RNA_def_property_ui_text(prop, "Length", "Affect the child hair length");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_map_twist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mapto", PAMAP_TWIST);
  RNA_def_property_ui_text(prop, "Twist", "Affect the child twist");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* influence factors */
  prop = RNA_def_property(srna, "time_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "timefac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(
      prop, "Emission Time Factor", "Amount texture affects particle emission time");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "life_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "lifefac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Life Time Factor", "Amount texture affects particle life time");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "density_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "padensfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Density Factor", "Amount texture affects particle density");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "size_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sizefac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Size Factor", "Amount texture affects physical particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "velocity_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ivelfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(
      prop, "Velocity Factor", "Amount texture affects particle initial velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "field_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fieldfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Field Factor", "Amount texture affects particle force fields");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "gravity_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "gravityfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Gravity Factor", "Amount texture affects particle gravity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "damp_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "dampfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Damp Factor", "Amount texture affects particle damping");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "length_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "lengthfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Length Factor", "Amount texture affects child hair length");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clumpfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Clump Factor", "Amount texture affects child clump");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_amp_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "kinkampfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(
      prop, "Kink Amplitude Factor", "Amount texture affects child kink amplitude");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_freq_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "kinkfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(
      prop, "Kink Frequency Factor", "Amount texture affects child kink frequency");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "rough_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "roughfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Rough Factor", "Amount texture affects child roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "twist_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "twistfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Twist Factor", "Amount texture affects child twist");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");
}

static void rna_def_particle_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem phys_type_items[] = {
      {PART_PHYS_NO, "NO", 0, "None", ""},
      {PART_PHYS_NEWTON, "NEWTON", 0, "Newtonian", ""},
      {PART_PHYS_KEYED, "KEYED", 0, "Keyed", ""},
      {PART_PHYS_BOIDS, "BOIDS", 0, "Boids", ""},
      {PART_PHYS_FLUID, "FLUID", 0, "Fluid", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rot_mode_items[] = {
      {0, "NONE", 0, "None", ""},
      {PART_ROT_NOR, "NOR", 0, "Normal", ""},
      {PART_ROT_NOR_TAN, "NOR_TAN", 0, "Normal-Tangent", ""},
      {PART_ROT_VEL, "VEL", 0, "Velocity / Hair", ""},
      {PART_ROT_GLOB_X, "GLOB_X", 0, "Global X", ""},
      {PART_ROT_GLOB_Y, "GLOB_Y", 0, "Global Y", ""},
      {PART_ROT_GLOB_Z, "GLOB_Z", 0, "Global Z", ""},
      {PART_ROT_OB_X, "OB_X", 0, "Object X", ""},
      {PART_ROT_OB_Y, "OB_Y", 0, "Object Y", ""},
      {PART_ROT_OB_Z, "OB_Z", 0, "Object Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem ave_mode_items[] = {
      {0, "NONE", 0, "None", ""},
      {PART_AVE_VELOCITY, "VELOCITY", 0, "Velocity", ""},
      {PART_AVE_HORIZONTAL, "HORIZONTAL", 0, "Horizontal", ""},
      {PART_AVE_VERTICAL, "VERTICAL", 0, "Vertical", ""},
      {PART_AVE_GLOBAL_X, "GLOBAL_X", 0, "Global X", ""},
      {PART_AVE_GLOBAL_Y, "GLOBAL_Y", 0, "Global Y", ""},
      {PART_AVE_GLOBAL_Z, "GLOBAL_Z", 0, "Global Z", ""},
      {PART_AVE_RAND, "RAND", 0, "Random", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem react_event_items[] = {
      {PART_EVENT_DEATH, "DEATH", 0, "Death", ""},
      {PART_EVENT_COLLIDE, "COLLIDE", 0, "Collision", ""},
      {PART_EVENT_NEAR, "NEAR", 0, "Proximity", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem child_type_items[] = {
      {0, "NONE", 0, "None", ""},
      {PART_CHILD_PARTICLES, "SIMPLE", 0, "Simple", ""},
      {PART_CHILD_FACES, "INTERPOLATED", 0, "Interpolated", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* TODO: names, tool-tips. */
  static const EnumPropertyItem integrator_type_items[] = {
      {PART_INT_EULER, "EULER", 0, "Euler", ""},
      {PART_INT_VERLET, "VERLET", 0, "Verlet", ""},
      {PART_INT_MIDPOINT, "MIDPOINT", 0, "Midpoint", ""},
      {PART_INT_RK4, "RK4", 0, "RK4", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem kink_type_items[] = {
      {PART_KINK_NO, "NO", 0, "Nothing", ""},
      {PART_KINK_CURL, "CURL", 0, "Curl", ""},
      {PART_KINK_RADIAL, "RADIAL", 0, "Radial", ""},
      {PART_KINK_WAVE, "WAVE", 0, "Wave", ""},
      {PART_KINK_BRAID, "BRAID", 0, "Braid", ""},
      {PART_KINK_SPIRAL, "SPIRAL", 0, "Spiral", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem draw_col_items[] = {
      {PART_DRAW_COL_NONE, "NONE", 0, "None", ""},
      {PART_DRAW_COL_MAT, "MATERIAL", 0, "Material", ""},
      {PART_DRAW_COL_VEL, "VELOCITY", 0, "Velocity", ""},
      {PART_DRAW_COL_ACC, "ACCELERATION", 0, "Acceleration", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ParticleSettings", "ID");
  RNA_def_struct_ui_text(
      srna, "Particle Settings", "Particle settings, reusable by multiple particle systems");
  RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

  rna_def_mtex_common(brna,
                      srna,
                      "rna_ParticleSettings_mtex_begin",
                      "rna_ParticleSettings_active_texture_get",
                      "rna_ParticleSettings_active_texture_set",
                      nullptr,
                      "ParticleSettingsTextureSlot",
                      "ParticleSettingsTextureSlots",
                      "rna_Particle_reset_dependency",
                      nullptr);

  /* Fluid particle type can't be checked from the type value in RNA
   * as it's not shown in the menu. */
  prop = RNA_def_property(srna, "is_fluid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_PartSettings_is_fluid_get", nullptr);
  RNA_def_property_ui_text(prop, "Fluid", "Particles were created by a fluid simulation");

  /* flag */
  prop = RNA_def_property(srna, "use_react_start_end", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_REACT_STA_END);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Start/End", "Give birth to unreacted particles eventually");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_react_multiple", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_REACT_MULTIPLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Multi React", "React multiple times");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_regrow_hair", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_HAIR_REGROW);
  RNA_def_property_ui_text(prop, "Regrow", "Regrow hair for each frame");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_unborn", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_UNBORN);
  RNA_def_property_ui_text(prop, "Unborn", "Show particles before they are emitted");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_dead", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_DIED);
  RNA_def_property_ui_text(prop, "Died", "Show particles after they have died");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_emit_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_TRAND);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Random", "Emit in random order of elements");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_even_distribution", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_EDISTR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Even Distribution",
                           "Use even distribution from faces based on face areas or edge lengths");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_die_on_collision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_DIE_ON_COL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Die on Hit", "Particles die when they collide with a deflector object");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_size_deflect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_SIZE_DEFL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Size Deflect", "Use particle's size in deflection");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_rotations", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_ROTATIONS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Rotations", "Calculate particle rotations");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_dynamic_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_ROT_DYN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Dynamic", "Particle rotations are affected by collisions and effectors");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_multiply_size_mass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_SIZEMASS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mass from Size", "Multiply mass by particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_advanced_hair", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", PART_HIDE_ADVANCED_HAIR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Advanced", "Use full physics calculations for growing hair");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "lock_boids_to_surface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_BOIDS_2D);
  RNA_def_property_ui_text(prop, "Boids 2D", "Constrain boids to a surface");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_hair_bspline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_HAIR_BSPLINE);
  RNA_def_property_ui_text(prop, "B-Spline", "Interpolate hair using B-Splines");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "invert_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_GRID_INVERT);
  RNA_def_property_ui_text(
      prop, "Invert Grid", "Invert what is considered object and what is not");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "hexagonal_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_GRID_HEXAGONAL);
  RNA_def_property_ui_text(prop, "Hexagonal Grid", "Create the grid in a hexagonal pattern");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "apply_effector_to_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_CHILD_EFFECT);
  RNA_def_property_ui_text(prop, "Affect Children", "Apply effectors to children");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "create_long_hair_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_CHILD_LONG_HAIR);
  RNA_def_property_ui_text(prop, "Long Hair", "Calculate children that suit long hair well");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "apply_guide_to_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_CHILD_GUIDE);
  RNA_def_property_ui_text(prop, "apply_guide_to_children", "");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_self_effect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PART_SELF_EFFECT);
  RNA_def_property_ui_text(prop, "Self Effect", "Particle effectors affect themselves");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, part_type_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_type_itemf");
  RNA_def_property_ui_text(prop, "Type", "Particle type");
  RNA_def_property_update(prop, 0, "rna_Particle_change_type");

  prop = RNA_def_property(srna, "emit_from", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "from");
  RNA_def_property_enum_items(prop, part_reactor_from_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_from_itemf");
  RNA_def_property_ui_text(prop, "Emit From", "Where to emit particles from");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "distribution", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "distr");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, part_dist_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_dist_itemf");
  RNA_def_property_ui_text(
      prop, "Distribution", "How to distribute particles on selected element");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* physics modes */
  prop = RNA_def_property(srna, "physics_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "phystype");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, phys_type_items);
  RNA_def_property_ui_text(prop, "Physics Type", "Particle physics type");
  RNA_def_property_update(prop, 0, "rna_Particle_change_physics_type");

  prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "rotmode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, rot_mode_items);
  RNA_def_property_ui_text(
      prop,
      "Orientation Axis",
      "Particle orientation axis (does not affect Explode modifier's results)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "angular_velocity_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "avemode");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, ave_mode_items);
  RNA_def_property_ui_text(
      prop, "Angular Velocity Axis", "What axis is used to change particle rotation with time");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "react_event", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "reactevent");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, react_event_items);
  RNA_def_property_ui_text(prop, "React On", "The event of target particles to react on");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* Draw flag. */
  prop = RNA_def_property(srna, "show_guide_hairs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_GUIDE_HAIRS);
  RNA_def_property_ui_text(prop, "Guide Hairs", "Show guide hairs");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_hair_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_HAIR_GRID);
  RNA_def_property_ui_text(prop, "Guide Hairs", "Show hair simulation grid");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_VEL);
  RNA_def_property_ui_text(prop, "Velocity", "Show particle velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_SIZE);
  RNA_def_property_ui_text(prop, "Size", "Show particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_health", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_HEALTH);
  RNA_def_property_ui_text(prop, "Health", "Display boid health");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_absolute_path_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_ABS_PATH_TIME);
  RNA_def_property_ui_text(prop, "Absolute Path Time", "Path timing is in absolute frames");
  RNA_def_property_update(prop, 0, "rna_Particle_abspathtime_update");

  prop = RNA_def_property(srna, "use_parent_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_PARENT);
  RNA_def_property_ui_text(prop, "Parents", "Render parent particles");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_PARTICLESETTINGS);
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "show_number", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_NUM);
  RNA_def_property_ui_text(prop, "Number", "Show particle number");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_collection_pick_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_RAND_GR);
  RNA_def_property_ui_text(prop, "Pick Random", "Pick objects from collection randomly");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_collection_count", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_COUNT_GR);
  RNA_def_property_ui_text(prop, "Use Count", "Use object multiple times in the same collection");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_count");

  prop = RNA_def_property(srna, "use_global_instance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_GLOBAL_OB);
  RNA_def_property_ui_text(prop, "Global", "Use object's global coordinates for duplication");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_rotation_instance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_ROTATE_OB);
  RNA_def_property_ui_text(prop,
                           "Rotation",
                           "Use object's rotation for duplication (global x-axis is aligned "
                           "particle rotation axis)");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_scale_instance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "draw", PART_DRAW_NO_SCALE_OB);
  RNA_def_property_ui_text(prop, "Scale", "Use object's scale for duplication");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_render_adaptive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_REN_ADAPT);
  RNA_def_property_ui_text(prop, "Adaptive Render", "Display steps of the particle path");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_velocity_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_VEL_LENGTH);
  RNA_def_property_ui_text(prop, "Speed", "Multiply line length by particle speed");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_whole_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_WHOLE_GR);
  RNA_def_property_ui_text(prop, "Whole Collection", "Use whole collection at once");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "use_strand_primitive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw", PART_DRAW_REN_STRAND);
  RNA_def_property_ui_text(prop, "Strand Render", "Use the strand primitive for rendering");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "display_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "draw_as");
  RNA_def_property_enum_items(prop, part_draw_as_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_draw_as_itemf");
  RNA_def_property_ui_text(prop, "Particle Display", "How particles are displayed in viewport");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_PARTICLESETTINGS);
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "render_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "ren_as");
  RNA_def_property_enum_items(prop, part_ren_as_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_ren_as_itemf");
  RNA_def_property_ui_text(prop, "Particle Rendering", "How particles are rendered");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "display_color", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "draw_col");
  RNA_def_property_enum_items(prop, draw_col_items);
  RNA_def_property_ui_text(prop, "Display Color", "Display additional particle data as a color");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "display_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "draw_size");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_range(prop, 0, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Display Size", "Size of particles on viewport");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "child_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "childtype");
  RNA_def_property_enum_items(prop, child_type_items);
  RNA_def_property_ui_text(prop, "Children From", "Create child particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "display_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "draw_step");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_range(prop, 0, 7, 1, -1);
  RNA_def_property_ui_text(prop, "Steps", "How many steps paths are displayed with (power of 2)");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "render_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "ren_step");
  RNA_def_property_range(prop, 0, 20);
  RNA_def_property_ui_range(prop, 0, 9, 1, -1);
  RNA_def_property_ui_text(prop, "Render", "How many steps paths are rendered with (power of 2)");

  prop = RNA_def_property(srna, "hair_step", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 2, SHRT_MAX);
  RNA_def_property_ui_range(prop, 2, 50, 1, 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Segments", "Number of hair segments");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "bending_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "bending_random");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Random Bending Stiffness", "Random stiffness of hairs");
  RNA_def_property_update(prop, 0, "rna_Particle_cloth_update");

  /* TODO: not found in UI, read-only? */
  prop = RNA_def_property(srna, "keys_step", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, SHRT_MAX); /* TODO: min,max. */
  RNA_def_property_ui_text(prop, "Keys Step", "");

  /* adaptive path rendering */
  prop = RNA_def_property(srna, "adaptive_angle", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "adapt_angle");
  RNA_def_property_range(prop, 0, 45);
  RNA_def_property_ui_text(
      prop, "Degrees", "How many degrees path has to curve to make another render segment");

  prop = RNA_def_property(srna, "adaptive_pixel", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "adapt_pix");
  RNA_def_property_range(prop, 0, 50);
  RNA_def_property_ui_text(
      prop, "Pixel", "How many pixels path has to cover to make another render segment");

  prop = RNA_def_property(srna, "display_percentage", PROP_INT, PROP_PERCENTAGE);
  RNA_def_property_int_sdna(prop, nullptr, "disp");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Display", "Percentage of particles to display in 3D view");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "material", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "omat");
  RNA_def_property_range(prop, 1, 32767);
  RNA_def_property_ui_text(
      prop, "Material Index", "Index of material slot used for rendering particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "material_slot", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "omat");
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Particle_Material_itemf");
  RNA_def_property_ui_text(prop, "Material Slot", "Material slot used for rendering particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "integrator", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, integrator_type_items);
  RNA_def_property_ui_text(prop,
                           "Integration",
                           "Algorithm used to calculate physics, from the fastest to the "
                           "most stable and accurate: Midpoint, Euler, Verlet, RK4");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "kink", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, kink_type_items);
  RNA_def_property_ui_text(prop, "Kink", "Type of periodic offset on the path");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_PARTICLESETTINGS);
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_ui_text(prop, "Axis", "Which axis to use for offset");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "color_maximum", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "color_vec_max");
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "Color Maximum", "Maximum length of the particle color vector");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  /* general values */
  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "sta"); /* Optional if prop names are the same. */
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(prop, nullptr, "rna_PartSettings_start_set", nullptr);
  RNA_def_property_ui_text(prop, "Start", "Frame number to start emitting particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "frame_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "end");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);

  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_funcs(prop, nullptr, "rna_PartSettings_end_set", nullptr);
  RNA_def_property_ui_text(prop, "End", "Frame number to stop emitting particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "lifetime", PROP_FLOAT, PROP_TIME);
  RNA_def_property_range(prop, 1.0f, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Lifetime", "Life span of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "lifetime_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "randlife");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Random", "Give the particle life a random variation");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "time_tweak", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "timetweak");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_range(prop, 0, 10, 1, 3);
  RNA_def_property_ui_text(
      prop, "Tweak", "A multiplier for physics timestep (1.0 means one frame = 1/25 seconds)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "timestep", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_PartSettings_timestep_get", "rna_PartSetings_timestep_set", nullptr);
  RNA_def_property_range(prop, 0.0001, 100.0);
  RNA_def_property_ui_range(prop, 0.01, 10, 1, 3);
  RNA_def_property_ui_text(
      prop, "Timestep", "The simulation timestep per frame (seconds per frame)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "use_adaptive_subframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "time_flag", PART_TIME_AUTOSF);
  RNA_def_property_ui_text(
      prop, "Automatic Subframes", "Automatically set the number of subframes");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "subframes", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(
      prop,
      "Subframes",
      "Subframes to simulate for improved stability and finer granularity simulations "
      "(dt = timestep / (subframes + 1))");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "courant_target", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0001, 10);
  RNA_def_property_ui_text(
      prop,
      "Adaptive Subframe Threshold",
      "The relative distance a particle can move before requiring more subframes "
      "(target Courant number); 0.01 to 0.3 is the recommended range");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "jitter_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_float_sdna(prop, nullptr, "jitfac");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Amount", "Amount of jitter applied to the sampling");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "effect_hair", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "eff_hair");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Stiffness", "Hair stiffness for effectors");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "totpart");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_range(prop, 0, 1000000, 1, -1);
  RNA_def_property_ui_text(prop, "Number", "Total number of particles");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(
      srna, "userjit", PROP_INT, PROP_UNSIGNED); /* TODO: can we get a better name for userjit? */
  RNA_def_property_int_sdna(prop, nullptr, "userjit");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop, "Particles/Face", "Emission locations per face (0 = automatic)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "grid_resolution", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "grid_res");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(
      prop, 1, 250); /* ~15M particles in a cube (ouch!), but could be very usable in a plane */
  RNA_def_property_ui_range(prop, 1, 50, 1, -1); /* ~100k particles in a cube */
  RNA_def_property_ui_text(prop, "Resolution", "The resolution of the particle grid");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "grid_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "grid_rand");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Grid Randomness", "Add random offset to the grid locations");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "effector_amount", PROP_INT, PROP_UNSIGNED);
  /* In theory PROP_ANIMATABLE perhaps should be cleared,
   * but animating this can give some interesting results! */
  RNA_def_property_range(prop, 0, 10000); /* 10000 effectors will be SLOW, but who knows */
  RNA_def_property_ui_range(prop, 0, 100, 1, -1);
  RNA_def_property_ui_text(
      prop, "Effector Number", "How many particles are effectors (0 is all particles)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* initial velocity factors */
  prop = RNA_def_property(srna, "normal_factor", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "normfac"); /* Optional if prop names are the same. */
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_range(prop, 0, 100, 1, 3);
  RNA_def_property_ui_text(
      prop, "Normal", "Let the surface normal give the particle a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "object_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "obfac");
  RNA_def_property_range(prop, -200.0f, 200.0f);
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Object Velocity", "Let the object give the particle a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "factor_random", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "randfac"); /* Optional if prop names are the same. */
  RNA_def_property_range(prop, 0.0f, 200.0f);
  RNA_def_property_ui_range(prop, 0, 100, 1, 3);
  RNA_def_property_ui_text(prop, "Random", "Give the starting velocity a random variation");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "particle_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "partfac");
  RNA_def_property_range(prop, -200.0f, 200.0f);
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Particle", "Let the target particle give the particle a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "tangent_factor", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "tanfac");
  RNA_def_property_range(prop, -1000.0f, 1000.0f);
  RNA_def_property_ui_range(prop, -100, 100, 1, 2);
  RNA_def_property_ui_text(
      prop, "Tangent", "Let the surface tangent give the particle a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "tangent_phase", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "tanphase");
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Rotation", "Rotate the surface tangent");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "reactor_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "reactfac");
  RNA_def_property_range(prop, -10.0f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Reactor",
      "Let the vector away from the target particle's location give the particle "
      "a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "object_align_factor", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "ob_vel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -200.0f, 200.0f);
  RNA_def_property_ui_range(prop, -100, 100, 1, 3);
  RNA_def_property_ui_text(
      prop,
      "Object Aligned",
      "Let the emitter object orientation give the particle a starting velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "angular_velocity_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "avefac");
  RNA_def_property_range(prop, -200.0f, 200.0f);
  RNA_def_property_ui_range(prop, -100, 100, 10, 3);
  RNA_def_property_ui_text(
      prop, "Angular Velocity", "Angular velocity amount (in radians per second)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "phase_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "phasefac");
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Phase", "Rotation around the chosen orientation axis");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "rotation_factor_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "randrotfac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Random Orientation", "Randomize particle orientation");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "phase_factor_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "randphasefac");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_ui_text(
      prop, "Random Phase", "Randomize rotation around the chosen orientation axis");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "hair_length", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_funcs(
      prop, "rna_PartSetting_hairlength_get", "rna_PartSetting_hairlength_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Hair Length", "Length of the hair");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* physical properties */
  prop = RNA_def_property(srna, "mass", PROP_FLOAT, PROP_UNIT_MASS);
  RNA_def_property_range(prop, 0.00000001f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 4);
  RNA_def_property_ui_text(prop, "Mass", "Mass of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "size");
  RNA_def_property_range(prop, 0.001f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 3);
  RNA_def_property_ui_text(prop, "Size", "The size of the particles");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "size_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "randsize");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Random Size", "Give the particle size a random variation");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "collision_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_sdna(prop, nullptr, "collision_group");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Collision Collection", "Limit colliders to this collection");
  RNA_def_property_update(prop, 0, "rna_Particle_reset_dependency");

  /* global physical properties */
  prop = RNA_def_property(srna, "drag_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "dragfac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Drag", "Amount of air drag");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "brownian_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "brownfac");
  RNA_def_property_range(prop, 0.0f, 200.0f);
  RNA_def_property_ui_range(prop, 0, 20, 1, 3);
  RNA_def_property_ui_text(prop, "Brownian", "Amount of random, erratic particle movement");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "damping", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "dampfac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Damp", "Amount of damping");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* random length */
  prop = RNA_def_property(srna, "length_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "randlength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Random Length", "Give path length a random variation");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  /* children */

  prop = RNA_def_property(srna, "child_percent", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(
      prop, nullptr, "child_percent"); /* Optional if prop names are the same. */
  RNA_def_property_range(prop, 0, 100000);
  RNA_def_property_ui_range(prop, 0, 1000, 1, -1);
  RNA_def_property_ui_text(prop, "Children Per Parent", "Number of children per parent");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "rendered_child_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "child_render_percent");
  RNA_def_property_range(prop, 0, 100000);
  RNA_def_property_ui_range(prop, 0, 10000, 1, -1);
  RNA_def_property_ui_text(
      prop, "Rendered Children", "Number of children per parent for rendering");

  prop = RNA_def_property(srna, "virtual_parents", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "parents");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Virtual Parents", "Relative amount of virtual parents");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "childsize");
  RNA_def_property_range(prop, 0.001f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01f, 100.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Child Size", "A multiplier for the child particle size");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_size_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "childrandsize");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Random Child Size", "Random variation to the size of the child particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "childrad");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Child Radius", "Radius of children around parent");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_roundness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "childflat");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Child Roundness", "Roundness of children around parent");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* clumping */
  prop = RNA_def_property(srna, "clump_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clumpfac");
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clump", "Amount of clumping");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "clump_shape", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clumppow");
  RNA_def_property_range(prop, -0.999f, 0.999f);
  RNA_def_property_ui_text(prop, "Shape", "Shape of clumping");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_clump_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "child_flag", PART_CHILD_USE_CLUMP_CURVE);
  RNA_def_property_ui_text(prop, "Use Clump Curve", "Use a curve to define clump tapering");
  RNA_def_property_update(prop, 0, "rna_ParticleSettings_use_clump_curve_update");

  prop = RNA_def_property(srna, "clump_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clumpcurve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Clump Curve", "Curve defining clump tapering");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_clump_noise", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "child_flag", PART_CHILD_USE_CLUMP_NOISE);
  RNA_def_property_ui_text(prop, "Use Clump Noise", "Create random clumps around the parent");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "clump_noise_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clump_noise_size");
  RNA_def_property_range(prop, 0.00001f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01f, 10.0f, 0.1f, 3);
  RNA_def_property_ui_text(prop, "Clump Noise Size", "Size of clump noise");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* kink */
  prop = RNA_def_property(srna, "kink_amplitude", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "kink_amp");
  RNA_def_property_range(prop, -100000.0f, 100000.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Amplitude", "The amplitude of the offset");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_amplitude_clump", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "kink_amp_clump");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Amplitude Clump", "How much clump affects kink amplitude");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_amplitude_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "kink_amp_random");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Amplitude Random", "Random variation of the amplitude");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_frequency", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "kink_freq");
  RNA_def_property_range(prop, -100000.0f, 100000.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Frequency", "The frequency of the offset (1/total length)");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_shape", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -0.999f, 0.999f);
  RNA_def_property_ui_text(prop, "Shape", "Adjust the offset to the beginning/end");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_flat", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Flatness", "How flat the hairs are");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_extra_steps", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(
      prop, "Extra Steps", "Extra steps for resolution of special kink features");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "kink_axis_random", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Axis Random", "Random variation of the orientation");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* rough */
  prop = RNA_def_property(srna, "roughness_1", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough1");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Roughness 1", "Amount of location dependent roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_1_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough1_size");
  RNA_def_property_range(prop, 0.01f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Size 1", "Size of location dependent roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough2");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Roughness 2", "Amount of random roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_2_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough2_size");
  RNA_def_property_range(prop, 0.01f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.01f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Size 2", "Size of random roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_2_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "rough2_thres");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Threshold", "Amount of particles left untouched by random roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_endpoint", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough_end");
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Roughness Endpoint", "Amount of endpoint roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "roughness_end_shape", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rough_end_shape");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Shape", "Shape of endpoint roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_roughness_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "child_flag", PART_CHILD_USE_ROUGH_CURVE);
  RNA_def_property_ui_text(prop, "Use Roughness Curve", "Use a curve to define roughness");
  RNA_def_property_update(prop, 0, "rna_ParticleSettings_use_roughness_curve_update");

  prop = RNA_def_property(srna, "roughness_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "roughcurve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Roughness Curve", "Curve defining roughness");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_length", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clength");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Length", "Length of child paths");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_length_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clength_thres");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Threshold", "Amount of particles left untouched by child path length");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* parting */
  prop = RNA_def_property(srna, "child_parting_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "parting_fac");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Parting Factor", "Create parting in the children based on parent strands");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_parting_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "parting_min");
  RNA_def_property_range(prop, 0.0f, 180.0f);
  RNA_def_property_ui_text(prop,
                           "Parting Minimum",
                           "Minimum root to tip angle (tip distance/root distance for long hair)");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "child_parting_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "parting_max");
  RNA_def_property_range(prop, 0.0f, 180.0f);
  RNA_def_property_ui_text(prop,
                           "Parting Maximum",
                           "Maximum root to tip angle (tip distance/root distance for long hair)");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* branching */
  prop = RNA_def_property(srna, "branch_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "branch_thres");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Threshold of branching");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* drawing stuff */
  prop = RNA_def_property(srna, "line_length_tail", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_PartSetting_linelentail_get", "rna_PartSetting_linelentail_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Tail", "Length of the line's tail");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "line_length_head", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_PartSetting_linelenhead_get", "rna_PartSetting_linelenhead_set", nullptr);
  RNA_def_property_range(prop, 0.0f, 100000.0f);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Head", "Length of the line's head");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "path_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "path_start");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_PartSetting_pathstartend_range");
  RNA_def_property_ui_text(prop, "Path Start", "Starting time of path");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "path_end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "path_end");
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_PartSetting_pathstartend_range");
  RNA_def_property_ui_text(prop, "Path End", "End time of path");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "trail_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "trail_count");
  RNA_def_property_range(prop, 1, 100000);
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);
  RNA_def_property_ui_text(prop, "Trail Count", "Number of trail particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  /* keyed particles */
  prop = RNA_def_property(srna, "keyed_loops", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "keyed_loops");
  RNA_def_property_range(prop, 1.0f, 10000.0f);
  RNA_def_property_ui_range(prop, 1.0f, 100.0f, 1, 3);
  RNA_def_property_ui_text(prop, "Loop Count", "Number of times the keys are looped");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  /* Evaluated mesh support. */
  prop = RNA_def_property(srna, "use_modifier_stack", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_modifier_stack", 0);
  RNA_def_property_ui_text(
      prop,
      "Use Modifier Stack",
      "Emit particles from mesh with modifiers applied "
      "(must use same subdivision surface level for viewport and render for correct results)");
  RNA_def_property_update(prop, 0, "rna_Particle_change_type");

  /* draw objects & collections */
  prop = RNA_def_property(srna, "instance_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "instance_collection");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Instance Collection", "Show objects in this collection in place of particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_count");

  prop = RNA_def_property(srna, "instance_weights", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "instance_weights", nullptr);
  RNA_def_property_struct_type(prop, "ParticleDupliWeight");
  RNA_def_property_ui_text(prop,
                           "Instance Collection Weights",
                           "Weights for all of the objects in the instance collection");

  prop = RNA_def_property(srna, "active_instanceweight", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleDupliWeight");
  RNA_def_property_pointer_funcs(
      prop, "rna_ParticleDupliWeight_active_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Instance Object", "");

  prop = RNA_def_property(srna, "active_instanceweight_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_ParticleDupliWeight_active_index_get",
                             "rna_ParticleDupliWeight_active_index_set",
                             "rna_ParticleDupliWeight_active_index_range");
  RNA_def_property_ui_text(prop, "Active Instance Object Index", "");

  prop = RNA_def_property(srna, "instance_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Instance Object", "Show this object in place of particles");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_dependency");

  /* boids */
  prop = RNA_def_property(srna, "boids", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "BoidSettings");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Boid Settings", "");

  /* Fluid particles */
  prop = RNA_def_property(srna, "fluid", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SPHFluidSettings");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "SPH Fluid Settings", "");

  /* Effector weights */
  prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EffectorWeights");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Effector Weights", "");

  /* animation here? */
  rna_def_animdata_common(srna);

  prop = RNA_def_property(srna, "force_field_1", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pd");
  RNA_def_property_struct_type(prop, "FieldSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Particle_field1_get", nullptr, nullptr, nullptr);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Force Field 1", "");

  prop = RNA_def_property(srna, "force_field_2", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pd2");
  RNA_def_property_struct_type(prop, "FieldSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Particle_field2_get", nullptr, nullptr, nullptr);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Force Field 2", "");

  /* twist */
  prop = RNA_def_property(srna, "twist", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -100000.0f, 100000.0f);
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Twist", "Number of turns around parent along the strand");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "use_twist_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "child_flag", PART_CHILD_USE_TWIST_CURVE);
  RNA_def_property_ui_text(prop, "Use Twist Curve", "Use a curve to define twist");
  RNA_def_property_update(prop, 0, "rna_ParticleSettings_use_twist_curve_update");

  prop = RNA_def_property(srna, "twist_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "twistcurve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Twist Curve", "Curve defining twist");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* hair shape */
  prop = RNA_def_property(srna, "use_close_tip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "shape_flag", PART_SHAPE_CLOSE_TIP);
  RNA_def_property_ui_text(prop, "Close Tip", "Set tip radius to zero");
  RNA_def_property_update(
      prop, 0, "rna_Particle_redo"); /* TODO: Only need to tell the render engine to update. */

  prop = RNA_def_property(srna, "shape", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, -1.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Shape", "Strand shape parameter");
  RNA_def_property_update(
      prop, 0, "rna_Particle_redo"); /* TODO: Only need to tell the render engine to update. */

  prop = RNA_def_property(srna, "root_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "rad_root");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
  RNA_def_property_ui_text(prop, "Root Diameter", "Strand diameter width at the root");
  RNA_def_property_update(
      prop, 0, "rna_Particle_redo"); /* TODO: Only need to tell the render engine to update. */

  prop = RNA_def_property(srna, "tip_radius", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "rad_tip");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
  RNA_def_property_ui_text(prop, "Tip Diameter", "Strand diameter width at the tip");
  RNA_def_property_update(
      prop, 0, "rna_Particle_redo"); /* TODO: Only need to tell the render engine to update. */

  prop = RNA_def_property(srna, "radius_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rad_scale");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1, 2);
  RNA_def_property_ui_text(prop, "Diameter Scale", "Multiplier of diameter properties");
  RNA_def_property_update(
      prop, 0, "rna_Particle_redo"); /* TODO: Only need to tell the render engine to update. */
}

static void rna_def_particle_target(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mode_items[] = {
      {PTARGET_MODE_FRIEND, "FRIEND", 0, "Friend", ""},
      {PTARGET_MODE_NEUTRAL, "NEUTRAL", 0, "Neutral", ""},
      {PTARGET_MODE_ENEMY, "ENEMY", 0, "Enemy", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ParticleTarget", nullptr);
  RNA_def_struct_ui_text(srna, "Particle Target", "Target particle system");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ParticleTarget_name_get", "rna_ParticleTarget_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Particle target name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "ob");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Target Object",
      "The object that has the target particle system (empty if same object)");
  RNA_def_property_update(prop, 0, "rna_Particle_target_reset");

  prop = RNA_def_property(srna, "system", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "psys");
  RNA_def_property_range(prop, 1, INT_MAX);
  RNA_def_property_ui_text(
      prop, "Target Particle System", "The index of particle system on the target object");
  RNA_def_property_update(prop, 0, "rna_Particle_target_reset");

  prop = RNA_def_property(srna, "time", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "time");
  RNA_def_property_range(prop, 0.0, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Time", "");
  RNA_def_property_update(prop, 0, "rna_Particle_target_redo");

  prop = RNA_def_property(srna, "duration", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "duration");
  RNA_def_property_range(prop, 0.0, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Duration", "");
  RNA_def_property_update(prop, 0, "rna_Particle_target_redo");

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PTARGET_VALID);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Valid", "Keyed particles target is valid");

  prop = RNA_def_property(srna, "alliance", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Mode", "");
  RNA_def_property_update(prop, 0, "rna_Particle_target_reset");
}
static void rna_def_particle_system(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "ParticleSystem", nullptr);
  RNA_def_struct_ui_text(srna, "Particle System", "Particle system in an object");
  RNA_def_struct_ui_icon(srna, ICON_PARTICLE_DATA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Particle system name");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER | NA_RENAME, nullptr);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ParticleSystem_name_set");
  RNA_def_struct_name_property(srna, prop);

  RNA_define_lib_overridable(true);

  /* access to particle settings is redirected through functions */
  /* to allow proper id-buttons functionality */
  prop = RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
  // RNA_def_property_pointer_sdna(prop, nullptr, "part");
  RNA_def_property_struct_type(prop, "ParticleSettings");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_pointer_funcs(
      prop, "rna_particle_settings_get", "rna_particle_settings_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Settings", "Particle system settings");
  RNA_def_property_update(prop, 0, "rna_Particle_reset_dependency");

  prop = RNA_def_property(srna, "particles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "particles", "totpart");
  RNA_def_property_struct_type(prop, "Particle");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "Particles", "Particles generated by the particle system");

  prop = RNA_def_property(srna, "child_particles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "child", "totchild");
  RNA_def_property_struct_type(prop, "ChildParticle");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(
      prop, "Child Particles", "Child particles generated by the particle system");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(
      prop, "Seed", "Offset in the random number table, to get a different randomized result");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "child_seed", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(
      prop,
      "Child Seed",
      "Offset in the random number table for child particles, to get a different "
      "randomized result");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* hair */
  prop = RNA_def_property(srna, "is_global_hair", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PSYS_GLOBAL_HAIR);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Global Hair", "Hair keys are in global coordinate space");

  prop = RNA_def_property(srna, "use_hair_dynamics", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PSYS_HAIR_DYNAMICS);
  RNA_def_property_ui_text(prop, "Hair Dynamics", "Enable hair dynamics using cloth simulation");
  RNA_def_property_update(prop, 0, "rna_Particle_hair_dynamics_update");

  prop = RNA_def_property(srna, "cloth", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clmd");
  RNA_def_property_struct_type(prop, "ClothModifier");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Cloth", "Cloth dynamics for hair");

  /* reactor */
  prop = RNA_def_property(srna, "reactor_target_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "target_ob");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Reactor Target Object",
                           "For reactor systems, the object that has the target particle system "
                           "(empty if same object)");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "reactor_target_particle_system", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "target_psys");
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_text(prop,
                           "Reactor Target Particle System",
                           "For reactor systems, index of particle system on the target object");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  /* keyed */
  prop = RNA_def_property(srna, "use_keyed_timing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PSYS_KEYED_TIMING);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Keyed Timing", "Use key times");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "targets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleTarget");
  RNA_def_property_ui_text(prop, "Targets", "Target particle systems");

  prop = RNA_def_property(srna, "active_particle_target", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleTarget");
  RNA_def_property_pointer_funcs(
      prop, "rna_ParticleSystem_active_particle_target_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Particle Target", "");

  prop = RNA_def_property(srna, "active_particle_target_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_ParticleSystem_active_particle_target_index_get",
                             "rna_ParticleSystem_active_particle_target_index_set",
                             "rna_ParticleSystem_active_particle_target_index_range");
  RNA_def_property_ui_text(prop, "Active Particle Target Index", "");

  /* vertex groups */

  /* NOTE: internally store as ints, access as strings. */
#  if 0 /* int access. works ok but isn't useful for the UI */
  prop = RNA_def_property(srna, "vertex_group_density", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "vgroup[0]");
  RNA_def_property_ui_text(prop, "Vertex Group Density", "Vertex group to control density");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");
#  endif

  prop = RNA_def_property(srna, "vertex_group_density", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_0",
                                "rna_ParticleVGroup_name_len_0",
                                "rna_ParticleVGroup_name_set_0");
  RNA_def_property_ui_text(prop, "Vertex Group Density", "Vertex group to control density");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_density", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_DENSITY));
  RNA_def_property_ui_text(
      prop, "Vertex Group Density Negate", "Negate the effect of the density vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_velocity", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_1",
                                "rna_ParticleVGroup_name_len_1",
                                "rna_ParticleVGroup_name_set_1");
  RNA_def_property_ui_text(prop, "Vertex Group Velocity", "Vertex group to control velocity");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_VEL));
  RNA_def_property_ui_text(
      prop, "Vertex Group Velocity Negate", "Negate the effect of the velocity vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_length", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_2",
                                "rna_ParticleVGroup_name_len_2",
                                "rna_ParticleVGroup_name_set_2");
  RNA_def_property_ui_text(prop, "Vertex Group Length", "Vertex group to control length");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "invert_vertex_group_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_LENGTH));
  RNA_def_property_ui_text(
      prop, "Vertex Group Length Negate", "Negate the effect of the length vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  prop = RNA_def_property(srna, "vertex_group_clump", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_3",
                                "rna_ParticleVGroup_name_len_3",
                                "rna_ParticleVGroup_name_set_3");
  RNA_def_property_ui_text(prop, "Vertex Group Clump", "Vertex group to control clump");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_clump", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_CLUMP));
  RNA_def_property_ui_text(
      prop, "Vertex Group Clump Negate", "Negate the effect of the clump vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "vertex_group_kink", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_4",
                                "rna_ParticleVGroup_name_len_4",
                                "rna_ParticleVGroup_name_set_4");
  RNA_def_property_ui_text(prop, "Vertex Group Kink", "Vertex group to control kink");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_kink", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_KINK));
  RNA_def_property_ui_text(
      prop, "Vertex Group Kink Negate", "Negate the effect of the kink vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "vertex_group_roughness_1", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_5",
                                "rna_ParticleVGroup_name_len_5",
                                "rna_ParticleVGroup_name_set_5");
  RNA_def_property_ui_text(
      prop, "Vertex Group Roughness 1", "Vertex group to control roughness 1");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_roughness_1", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_ROUGH1));
  RNA_def_property_ui_text(prop,
                           "Vertex Group Roughness 1 Negate",
                           "Negate the effect of the roughness 1 vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "vertex_group_roughness_2", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_6",
                                "rna_ParticleVGroup_name_len_6",
                                "rna_ParticleVGroup_name_set_6");
  RNA_def_property_ui_text(
      prop, "Vertex Group Roughness 2", "Vertex group to control roughness 2");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_roughness_2", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_ROUGH2));
  RNA_def_property_ui_text(prop,
                           "Vertex Group Roughness 2 Negate",
                           "Negate the effect of the roughness 2 vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "vertex_group_roughness_end", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_7",
                                "rna_ParticleVGroup_name_len_7",
                                "rna_ParticleVGroup_name_set_7");
  RNA_def_property_ui_text(
      prop, "Vertex Group Roughness End", "Vertex group to control roughness end");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_roughness_end", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_ROUGHE));
  RNA_def_property_ui_text(prop,
                           "Vertex Group Roughness End Negate",
                           "Negate the effect of the roughness end vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "vertex_group_size", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_8",
                                "rna_ParticleVGroup_name_len_8",
                                "rna_ParticleVGroup_name_set_8");
  RNA_def_property_ui_text(prop, "Vertex Group Size", "Vertex group to control size");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_SIZE));
  RNA_def_property_ui_text(
      prop, "Vertex Group Size Negate", "Negate the effect of the size vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_tangent", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_9",
                                "rna_ParticleVGroup_name_len_9",
                                "rna_ParticleVGroup_name_set_9");
  RNA_def_property_ui_text(prop, "Vertex Group Tangent", "Vertex group to control tangent");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_tangent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_TAN));
  RNA_def_property_ui_text(
      prop, "Vertex Group Tangent Negate", "Negate the effect of the tangent vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_rotation", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_10",
                                "rna_ParticleVGroup_name_len_10",
                                "rna_ParticleVGroup_name_set_10");
  RNA_def_property_ui_text(prop, "Vertex Group Rotation", "Vertex group to control rotation");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_ROT));
  RNA_def_property_ui_text(
      prop, "Vertex Group Rotation Negate", "Negate the effect of the rotation vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_field", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_11",
                                "rna_ParticleVGroup_name_len_11",
                                "rna_ParticleVGroup_name_set_11");
  RNA_def_property_ui_text(prop, "Vertex Group Field", "Vertex group to control field");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "invert_vertex_group_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_EFFECTOR));
  RNA_def_property_ui_text(
      prop, "Vertex Group Field Negate", "Negate the effect of the field vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_reset");

  prop = RNA_def_property(srna, "vertex_group_twist", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_ParticleVGroup_name_get_12",
                                "rna_ParticleVGroup_name_len_12",
                                "rna_ParticleVGroup_name_set_12");
  RNA_def_property_ui_text(prop, "Vertex Group Twist", "Vertex group to control twist");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  prop = RNA_def_property(srna, "invert_vertex_group_twist", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vg_neg", (1 << PSYS_VG_TWIST));
  RNA_def_property_ui_text(
      prop, "Vertex Group Twist Negate", "Negate the effect of the twist vertex group");
  RNA_def_property_update(prop, 0, "rna_Particle_redo_child");

  /* pointcache */
  prop = RNA_def_property(srna, "point_cache", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "pointcache");
  RNA_def_property_struct_type(prop, "PointCache");
  RNA_def_property_ui_text(prop, "Point Cache", "");

  prop = RNA_def_property(srna, "has_multiple_caches", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_multiple_caches_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Multiple Caches", "Particle system has multiple point caches");

  /* offset ob */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "parent");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Parent", "Use this object's coordinate system instead of global coordinate system");
  RNA_def_property_update(prop, 0, "rna_Particle_redo");

  /* hair or cache editing */
  prop = RNA_def_property(srna, "is_editable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_editable_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Editable", "Particle system can be edited in particle mode");

  prop = RNA_def_property(srna, "is_edited", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_ParticleSystem_edited_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Edited", "Particle system has been edited in particle mode");

  /* Read-only: this is calculated internally. Changing it would only affect
   * the next time-step. The user should change ParticlSettings.subframes or
   * ParticleSettings.courant_target instead. */
  prop = RNA_def_property(srna, "dt_frac", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f / 101.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Timestep", "The current simulation time step size, as a fraction of a frame");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  RNA_define_lib_overridable(false);

  RNA_def_struct_path_func(srna, "rna_ParticleSystem_path");

  /* extract cached hair location data */
  func = RNA_def_function(srna, "co_hair", "rna_ParticleSystem_co_hair");
  RNA_def_function_ui_description(func, "Obtain cache hair data");
  parm = RNA_def_pointer(func, "object", "Object", "", "Object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_int(func, "particle_no", 0, INT_MIN, INT_MAX, "Particle no", "", INT_MIN, INT_MAX);
  RNA_def_int(func, "step", 0, INT_MIN, INT_MAX, "step no", "", INT_MIN, INT_MAX);
  parm = RNA_def_float_vector(
      func, "co", 3, nullptr, -FLT_MAX, FLT_MAX, "Co", "Exported hairkey location", -1e4, 1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  /* extract hair UVs */
  func = RNA_def_function(srna, "uv_on_emitter", "rna_ParticleSystem_uv_on_emitter");
  RNA_def_function_ui_description(func, "Obtain uv for all particles");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "modifier", "ParticleSystemModifier", "", "Particle modifier");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "particle", "Particle", "", "Particle");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_int(func, "particle_no", 0, INT_MIN, INT_MAX, "Particle no", "", INT_MIN, INT_MAX);
  RNA_def_int(func, "uv_no", 0, INT_MIN, INT_MAX, "UV no", "", INT_MIN, INT_MAX);
  parm = RNA_def_property(func, "uv", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_array(parm, 2);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  /* Extract hair vertex-colors. */
  func = RNA_def_function(srna, "mcol_on_emitter", "rna_ParticleSystem_mcol_on_emitter");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Obtain mcol for all particles");
  parm = RNA_def_pointer(func, "modifier", "ParticleSystemModifier", "", "Particle modifier");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "particle", "Particle", "", "Particle");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_int(func, "particle_no", 0, INT_MIN, INT_MAX, "Particle no", "", INT_MIN, INT_MAX);
  RNA_def_int(func, "vcol_no", 0, INT_MIN, INT_MAX, "vcol no", "", INT_MIN, INT_MAX);
  parm = RNA_def_property(func, "mcol", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(parm, 3);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

void RNA_def_particle(BlenderRNA *brna)
{
  rna_def_particle_target(brna);
  rna_def_fluid_settings(brna);
  rna_def_particle_hair_key(brna);
  rna_def_particle_key(brna);

  rna_def_child_particle(brna);
  rna_def_particle(brna);
  rna_def_particle_dupliweight(brna);
  rna_def_particle_system(brna);
  rna_def_particle_settings_mtex(brna);
  rna_def_particle_settings(brna);
}

#endif
