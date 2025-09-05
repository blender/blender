/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "BLI_path_utils.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "BKE_modifier.hh"

#include "BLT_translation.hh"

#include "DNA_fluid_types.h"
#include "DNA_scene_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLI_math_vector.h"
#  include "BLI_string.h"
#  include "BLI_threads.h"

#  include "BKE_colorband.hh"
#  include "BKE_context.hh"
#  include "BKE_fluid.h"
#  include "BKE_particle.h"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "manta_fluid_API.h"

static void rna_Fluid_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);

  /* Needed for liquid domain objects */
  Object *ob = (Object *)ptr->owner_id;
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_Fluid_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Fluid_update(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

static void rna_Fluid_datacache_reset(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  if (settings->fmd && settings->fmd->domain) {
    Object *ob = (Object *)ptr->owner_id;
    int cache_map = (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                     FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES);

    /* In replay mode, always invalidate guiding cache too. */
    if (settings->cache_type == FLUID_DOMAIN_CACHE_REPLAY) {
      cache_map |= FLUID_DOMAIN_OUTDATED_GUIDE;
    }
    BKE_fluid_cache_free(settings, ob, cache_map);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}
static void rna_Fluid_noisecache_reset(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  if (settings->fmd && settings->fmd->domain) {
    Object *ob = (Object *)ptr->owner_id;
    int cache_map = FLUID_DOMAIN_OUTDATED_NOISE;
    BKE_fluid_cache_free(settings, ob, cache_map);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}
static void rna_Fluid_meshcache_reset(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  if (settings->fmd && settings->fmd->domain) {
    Object *ob = (Object *)ptr->owner_id;
    int cache_map = FLUID_DOMAIN_OUTDATED_MESH;
    BKE_fluid_cache_free(settings, ob, cache_map);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}
static void rna_Fluid_particlescache_reset(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  if (settings->fmd && settings->fmd->domain) {
    Object *ob = (Object *)ptr->owner_id;
    int cache_map = FLUID_DOMAIN_OUTDATED_PARTICLES;
    BKE_fluid_cache_free(settings, ob, cache_map);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}
static void rna_Fluid_guidingcache_reset(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  if (settings->fmd && settings->fmd->domain) {
    Object *ob = (Object *)ptr->owner_id;
    int cache_map = (FLUID_DOMAIN_OUTDATED_DATA | FLUID_DOMAIN_OUTDATED_NOISE |
                     FLUID_DOMAIN_OUTDATED_MESH | FLUID_DOMAIN_OUTDATED_PARTICLES |
                     FLUID_DOMAIN_OUTDATED_GUIDE);
    BKE_fluid_cache_free(settings, ob, cache_map);
  }
#  endif
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
}

static void rna_Fluid_effector_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidEffectorSettings *settings = (FluidEffectorSettings *)ptr->data;
  settings->flags |= FLUID_EFFECTOR_NEEDS_UPDATE;
#  endif

  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_flow_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidFlowSettings *settings = (FluidFlowSettings *)ptr->data;
  settings->flags |= FLUID_FLOW_NEEDS_UPDATE;
#  endif

  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_domain_data_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_modifier_reset(settings->fmd);
#  endif

  rna_Fluid_datacache_reset(bmain, scene, ptr);
  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_domain_noise_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_modifier_reset(settings->fmd);
#  endif

  rna_Fluid_noisecache_reset(bmain, scene, ptr);
  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_domain_mesh_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_modifier_reset(settings->fmd);
#  endif

  rna_Fluid_meshcache_reset(bmain, scene, ptr);
  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_domain_particles_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_modifier_reset(settings->fmd);
#  endif

  rna_Fluid_particlescache_reset(bmain, scene, ptr);
  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_reset_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
#  ifdef WITH_FLUID
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_modifier_reset(settings->fmd);
#  endif

  rna_Fluid_dependency_update(bmain, scene, ptr);
}

static void rna_Fluid_parts_create(Main *bmain,
                                   PointerRNA *ptr,
                                   const char *pset_name,
                                   const char *parts_name,
                                   const char *psys_name,
                                   int psys_type)
{
#  ifndef WITH_FLUID
  UNUSED_VARS(bmain, ptr, pset_name, parts_name, psys_name, psys_type);
#  else
  Object *ob = (Object *)ptr->owner_id;
  BKE_fluid_particle_system_create(bmain, ob, pset_name, parts_name, psys_name, psys_type);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
#  endif
}

static void rna_Fluid_parts_delete(Main *bmain, PointerRNA *ptr, int ptype)
{
#  ifndef WITH_FLUID
  UNUSED_VARS(bmain, ptr, ptype);
#  else
  Object *ob = (Object *)ptr->owner_id;
  BKE_fluid_particle_system_destroy(ob, ptype);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
#  endif
}

static bool rna_Fluid_parts_exists(PointerRNA *ptr, int ptype)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys;

  for (psys = static_cast<ParticleSystem *>(ob->particlesystem.first); psys; psys = psys->next) {
    if (psys->part->type == ptype) {
      return true;
    }
  }
  return false;
}

static void rna_Fluid_flip_parts_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  bool exists = rna_Fluid_parts_exists(ptr, PART_FLUID_FLIP);

  /* Only create a particle system in liquid domain mode.
   * Remove any remaining data from a liquid sim when switching to gas. */
  if (fmd->domain->type != FLUID_DOMAIN_TYPE_LIQUID) {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FLIP);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_FLIP;
    rna_Fluid_domain_data_reset(bmain, scene, ptr);
    return;
  }

  if (ob->type == OB_MESH && !exists) {
    rna_Fluid_parts_create(
        bmain, ptr, "LiquidParticleSettings", "Liquid", "Liquid Particle System", PART_FLUID_FLIP);
    fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_FLIP;
  }
  else {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FLIP);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_FLIP;
  }
  rna_Fluid_update(bmain, scene, ptr);
}

static void rna_Fluid_spray_parts_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  bool exists = rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAY);

  if (ob->type == OB_MESH && !exists) {
    rna_Fluid_parts_create(
        bmain, ptr, "SprayParticleSettings", "Spray", "Spray Particle System", PART_FLUID_SPRAY);
    fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_SPRAY;
  }
  else {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAY);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_SPRAY;
  }
}

static void rna_Fluid_bubble_parts_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  bool exists = rna_Fluid_parts_exists(ptr, PART_FLUID_BUBBLE);

  if (ob->type == OB_MESH && !exists) {
    rna_Fluid_parts_create(bmain,
                           ptr,
                           "BubbleParticleSettings",
                           "Bubbles",
                           "Bubble Particle System",
                           PART_FLUID_BUBBLE);
    fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_BUBBLE;
  }
  else {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_BUBBLE);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_BUBBLE;
  }
}

static void rna_Fluid_foam_parts_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  bool exists = rna_Fluid_parts_exists(ptr, PART_FLUID_FOAM);

  if (ob->type == OB_MESH && !exists) {
    rna_Fluid_parts_create(
        bmain, ptr, "FoamParticleSettings", "Foam", "Foam Particle System", PART_FLUID_FOAM);
    fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_FOAM;
  }
  else {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAM);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_FOAM;
  }
}

static void rna_Fluid_tracer_parts_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  bool exists = rna_Fluid_parts_exists(ptr, PART_FLUID_TRACER);

  if (ob->type == OB_MESH && !exists) {
    rna_Fluid_parts_create(bmain,
                           ptr,
                           "TracerParticleSettings",
                           "Tracers",
                           "Tracer Particle System",
                           PART_FLUID_TRACER);
    fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_TRACER;
  }
  else {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_TRACER);
    fmd->domain->particle_type &= ~FLUID_DOMAIN_PARTICLE_TRACER;
  }
}

static void rna_Fluid_combined_export_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  FluidModifierData *fmd;
  fmd = (FluidModifierData *)BKE_modifiers_findby_type(ob, eModifierType_Fluid);

  if (fmd->domain->sndparticle_combined_export == SNDPARTICLE_COMBINED_EXPORT_OFF) {
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAM);
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYBUBBLE);
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAMBUBBLE);
    rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAMBUBBLE);

    bool exists_spray = rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAY);
    bool exists_foam = rna_Fluid_parts_exists(ptr, PART_FLUID_FOAM);
    bool exists_bubble = rna_Fluid_parts_exists(ptr, PART_FLUID_BUBBLE);

    /* Re-add each particle type if enabled and no particle system exists for them anymore. */
    if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY) && !exists_spray) {
      rna_Fluid_spray_parts_update(bmain, scene, ptr);
    }
    if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) && !exists_foam) {
      rna_Fluid_foam_parts_update(bmain, scene, ptr);
    }
    if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) && !exists_bubble) {
      rna_Fluid_bubble_parts_update(bmain, scene, ptr);
    }
  }
  else if (fmd->domain->sndparticle_combined_export == SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM) {
    if (ob->type == OB_MESH && !rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAYFOAM)) {

      rna_Fluid_parts_create(bmain,
                             ptr,
                             "SprayFoamParticleSettings",
                             "Spray + Foam",
                             "Spray + Foam Particle System",
                             PART_FLUID_SPRAYFOAM);

      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_SPRAY;
      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_FOAM;

      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAY);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYBUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAMBUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAMBUBBLE);

      /* Re-add spray if enabled and no particle system exists for it anymore. */
      bool exists_bubble = rna_Fluid_parts_exists(ptr, PART_FLUID_BUBBLE);
      if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_BUBBLE) && !exists_bubble) {
        rna_Fluid_bubble_parts_update(bmain, scene, ptr);
      }
    }
  }
  else if (fmd->domain->sndparticle_combined_export == SNDPARTICLE_COMBINED_EXPORT_SPRAY_BUBBLE) {
    if (ob->type == OB_MESH && !rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAYBUBBLE)) {

      rna_Fluid_parts_create(bmain,
                             ptr,
                             "SprayBubbleParticleSettings",
                             "Spray + Bubbles",
                             "Spray + Bubble Particle System",
                             PART_FLUID_SPRAYBUBBLE);

      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_SPRAY;
      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_BUBBLE;

      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAY);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_BUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAMBUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAMBUBBLE);

      /* Re-add foam if enabled and no particle system exists for it anymore. */
      bool exists_foam = rna_Fluid_parts_exists(ptr, PART_FLUID_FOAM);
      if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_FOAM) && !exists_foam) {
        rna_Fluid_foam_parts_update(bmain, scene, ptr);
      }
    }
  }
  else if (fmd->domain->sndparticle_combined_export == SNDPARTICLE_COMBINED_EXPORT_FOAM_BUBBLE) {
    if (ob->type == OB_MESH && !rna_Fluid_parts_exists(ptr, PART_FLUID_FOAMBUBBLE)) {

      rna_Fluid_parts_create(bmain,
                             ptr,
                             "FoamBubbleParticleSettings",
                             "Foam + Bubble Particles",
                             "Foam + Bubble Particle System",
                             PART_FLUID_FOAMBUBBLE);

      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_FOAM;
      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_BUBBLE;

      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_BUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYBUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAMBUBBLE);

      /* Re-add foam if enabled and no particle system exists for it anymore. */
      bool exists_spray = rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAY);
      if ((fmd->domain->particle_type & FLUID_DOMAIN_PARTICLE_SPRAY) && !exists_spray) {
        rna_Fluid_spray_parts_update(bmain, scene, ptr);
      }
    }
  }
  else if (fmd->domain->sndparticle_combined_export ==
           SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM_BUBBLE)
  {
    if (ob->type == OB_MESH && !rna_Fluid_parts_exists(ptr, PART_FLUID_SPRAYFOAMBUBBLE)) {

      rna_Fluid_parts_create(bmain,
                             ptr,
                             "SprayFoamBubbleParticleSettings",
                             "Spray + Foam + Bubbles",
                             "Spray + Foam + Bubble Particle System",
                             PART_FLUID_SPRAYFOAMBUBBLE);

      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_SPRAY;
      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_FOAM;
      fmd->domain->particle_type |= FLUID_DOMAIN_PARTICLE_BUBBLE;

      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAY);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_BUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYFOAM);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_SPRAYBUBBLE);
      rna_Fluid_parts_delete(bmain, ptr, PART_FLUID_FOAMBUBBLE);
    }
  }
  else {
    /* sanity check, should not occur */
    printf("ERROR: Unexpected combined export setting encountered!");
  }
}

static void rna_Fluid_cache_startframe_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cache_startframe_set(settings, value);
}

static void rna_Fluid_cache_endframe_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cache_endframe_set(settings, value);
}

static void rna_Fluid_cachetype_mesh_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cachetype_mesh_set(settings, value);
}

static void rna_Fluid_cachetype_data_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cachetype_data_set(settings, value);
}

static void rna_Fluid_cachetype_particle_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cachetype_particle_set(settings, value);
}

static void rna_Fluid_cachetype_noise_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  BKE_fluid_cachetype_noise_set(settings, value);
}

static void rna_Fluid_cachetype_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;

  if (value != settings->cache_type) {
    settings->cache_type = value;
    settings->cache_flag = 0;
  }
}

static void rna_Fluid_guide_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  Object *par = (Object *)value.data;

  FluidModifierData *fmd_par = nullptr;

  if (par != nullptr) {
    fmd_par = (FluidModifierData *)BKE_modifiers_findby_type(par, eModifierType_Fluid);
    if (fmd_par && fmd_par->domain) {
      fds->guide_parent = static_cast<Object *>(value.data);
      copy_v3_v3_int(fds->guide_res, fmd_par->domain->res);
    }
  }
  else {
    fds->guide_parent = nullptr;
  }
}

static const EnumPropertyItem *rna_Fluid_cachetype_mesh_itemf(bContext * /*C*/,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = FLUID_DOMAIN_FILE_BIN_OBJECT;
  tmp.identifier = "BOBJECT";
  tmp.name = N_("Binary Object");
  tmp.description = N_("Binary object file format (.bobj.gz)");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FILE_OBJECT;
  tmp.identifier = "OBJECT";
  tmp.name = N_("Object");
  tmp.description = N_("Object file format (.obj)");
  RNA_enum_item_add(&item, &totitem, &tmp);

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_Fluid_cachetype_volume_itemf(bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/,
                                                                bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = FLUID_DOMAIN_FILE_UNI;
  tmp.identifier = "UNI";
  tmp.name = N_("Uni Cache");
  tmp.description = N_("Uni file format (.uni)");
  RNA_enum_item_add(&item, &totitem, &tmp);

#  ifdef WITH_OPENVDB
  tmp.value = FLUID_DOMAIN_FILE_OPENVDB;
  tmp.identifier = "OPENVDB";
  tmp.name = N_("OpenVDB");
  tmp.description = N_("OpenVDB file format (.vdb)");
  RNA_enum_item_add(&item, &totitem, &tmp);
#  endif

  /* Support for deprecated `.raw` format. */
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  if (fds->cache_data_format == FLUID_DOMAIN_FILE_RAW ||
      fds->cache_noise_format == FLUID_DOMAIN_FILE_RAW)
  {
    tmp.value = FLUID_DOMAIN_FILE_RAW;
    tmp.identifier = "RAW";
    tmp.name = N_("Raw Cache");
    tmp.description = N_("Raw file format (.raw)");
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_Fluid_cachetype_particle_itemf(bContext * /*C*/,
                                                                  PointerRNA * /*ptr*/,
                                                                  PropertyRNA * /*prop*/,
                                                                  bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = FLUID_DOMAIN_FILE_UNI;
  tmp.identifier = "UNI";
  tmp.name = N_("Uni Cache");
  tmp.description = N_("Uni file format");
  RNA_enum_item_add(&item, &totitem, &tmp);

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_Fluid_cache_directory_set(PointerRNA *ptr, const char *value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;

  if (STREQ(settings->cache_directory, value)) {
    return;
  }

  STRNCPY(settings->cache_directory, value);

  /* TODO(sebbas): Read cache state in order to set cache bake flags and cache pause frames
   * correctly. */
  // settings->cache_flag = 0;
}

static const EnumPropertyItem *rna_Fluid_cobafield_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;

  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = FLUID_DOMAIN_FIELD_FLAGS;
  tmp.identifier = "FLAGS";
  tmp.icon = 0;
  tmp.name = N_("Flags");
  tmp.description = N_("Flag grid of the fluid domain");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_PRESSURE;
  tmp.identifier = "PRESSURE";
  tmp.icon = 0;
  tmp.name = N_("Pressure");
  tmp.description = N_("Pressure field of the fluid domain");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_VELOCITY_X;
  tmp.identifier = "VELOCITY_X";
  tmp.icon = 0;
  tmp.name = N_("X Velocity");
  tmp.description = N_("X component of the velocity field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_VELOCITY_Y;
  tmp.identifier = "VELOCITY_Y";
  tmp.icon = 0;
  tmp.name = N_("Y Velocity");
  tmp.description = N_("Y component of the velocity field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_VELOCITY_Z;
  tmp.identifier = "VELOCITY_Z";
  tmp.icon = 0;
  tmp.name = N_("Z Velocity");
  tmp.description = N_("Z component of the velocity field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_FORCE_X;
  tmp.identifier = "FORCE_X";
  tmp.icon = 0;
  tmp.name = N_("X Force");
  tmp.description = N_("X component of the force field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_FORCE_Y;
  tmp.identifier = "FORCE_Y";
  tmp.icon = 0;
  tmp.name = N_("Y Force");
  tmp.description = N_("Y component of the force field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = FLUID_DOMAIN_FIELD_FORCE_Z;
  tmp.identifier = "FORCE_Z";
  tmp.icon = 0;
  tmp.name = N_("Z Force");
  tmp.description = N_("Z component of the force field");
  RNA_enum_item_add(&item, &totitem, &tmp);

  if (settings->type == FLUID_DOMAIN_TYPE_GAS) {
    tmp.value = FLUID_DOMAIN_FIELD_COLOR_R;
    tmp.identifier = "COLOR_R";
    tmp.icon = 0;
    tmp.name = N_("Red");
    tmp.description = N_("Red component of the color field");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_COLOR_G;
    tmp.identifier = "COLOR_G";
    tmp.icon = 0;
    tmp.name = N_("Green");
    tmp.description = N_("Green component of the color field");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_COLOR_B;
    tmp.identifier = "COLOR_B";
    tmp.icon = 0;
    tmp.name = N_("Blue");
    tmp.description = N_("Blue component of the color field");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_DENSITY;
    tmp.identifier = "DENSITY";
    tmp.icon = 0;
    tmp.name = N_("Density");
    tmp.description = N_("Quantity of soot in the fluid");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_FLAME;
    tmp.identifier = "FLAME";
    tmp.icon = 0;
    tmp.name = N_("Flame");
    tmp.description = N_("Flame field");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_FUEL;
    tmp.identifier = "FUEL";
    tmp.icon = 0;
    tmp.name = N_("Fuel");
    tmp.description = N_("Fuel field");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_HEAT;
    tmp.identifier = "HEAT";
    tmp.icon = 0;
    tmp.name = N_("Heat");
    tmp.description = N_("Temperature of the fluid");
    RNA_enum_item_add(&item, &totitem, &tmp);
  }
  else if (settings->type == FLUID_DOMAIN_TYPE_LIQUID) {
    tmp.value = FLUID_DOMAIN_FIELD_PHI;
    tmp.identifier = "PHI";
    tmp.icon = 0;
    tmp.name = N_("Fluid Level Set");
    tmp.description = N_("Level set representation of the fluid");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_PHI_IN;
    tmp.identifier = "PHI_IN";
    tmp.icon = 0;
    tmp.name = N_("Inflow Level Set");
    tmp.description = N_("Level set representation of the inflow");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_PHI_OUT;
    tmp.identifier = "PHI_OUT";
    tmp.icon = 0;
    tmp.name = N_("Outflow Level Set");
    tmp.description = N_("Level set representation of the outflow");
    RNA_enum_item_add(&item, &totitem, &tmp);

    tmp.value = FLUID_DOMAIN_FIELD_PHI_OBSTACLE;
    tmp.identifier = "PHI_OBSTACLE";
    tmp.icon = 0;
    tmp.name = N_("Obstacle Level Set");
    tmp.description = N_("Level set representation of the obstacles");
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_Fluid_data_depth_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;

  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = VDB_PRECISION_FULL_FLOAT;
  tmp.identifier = "32";
  tmp.icon = 0;
  tmp.name = N_("Full");
  tmp.description = N_("Use 32-bit floating-point numbers for all data");
  RNA_enum_item_add(&item, &totitem, &tmp);

  tmp.value = VDB_PRECISION_HALF_FLOAT;
  tmp.identifier = "16";
  tmp.icon = 0;
  tmp.name = N_("Half");
  tmp.description = N_("Use 16-bit floating-point numbers for all data");
  RNA_enum_item_add(&item, &totitem, &tmp);

  if (settings->type == FLUID_DOMAIN_TYPE_LIQUID) {
    tmp.value = VDB_PRECISION_MINI_FLOAT;
    tmp.identifier = "8";
    tmp.icon = 0;
    tmp.name = N_("Mini");
    tmp.description = N_("Use 8-bit floating-point numbers where possible, otherwise use 16-bit");
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_Fluid_domaintype_set(PointerRNA *ptr, int value)
{
  FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  Object *ob = (Object *)ptr->owner_id;
  BKE_fluid_domain_type_set(ob, settings, value);
  BKE_fluid_fields_sanitize(settings);
}

static std::optional<std::string> rna_FluidDomainSettings_path(const PointerRNA *ptr)
{
  const FluidDomainSettings *settings = (FluidDomainSettings *)ptr->data;
  const ModifierData *md = (ModifierData *)settings->fmd;
  char name_esc[sizeof(md->name) * 2];

  BLI_str_escape(name_esc, md->name, sizeof(name_esc));
  return fmt::format("modifiers[\"{}\"].domain_settings", name_esc);
}

static std::optional<std::string> rna_FluidFlowSettings_path(const PointerRNA *ptr)
{
  const FluidFlowSettings *settings = (FluidFlowSettings *)ptr->data;
  const ModifierData *md = (ModifierData *)settings->fmd;
  char name_esc[sizeof(md->name) * 2];

  BLI_str_escape(name_esc, md->name, sizeof(name_esc));
  return fmt::format("modifiers[\"{}\"].flow_settings", name_esc);
}

static std::optional<std::string> rna_FluidEffectorSettings_path(const PointerRNA *ptr)
{
  const FluidEffectorSettings *settings = (FluidEffectorSettings *)ptr->data;
  const ModifierData *md = (ModifierData *)settings->fmd;
  char name_esc[sizeof(md->name) * 2];

  BLI_str_escape(name_esc, md->name, sizeof(name_esc));
  return fmt::format("modifiers[\"{}\"].effector_settings", name_esc);
}

/* -------------------------------------------------------------------- */
/** \name Grid Accessors
 * \{ */

#  ifdef WITH_FLUID

static int rna_FluidModifier_grid_get_length(const PointerRNA *ptr,
                                             int length[RNA_MAX_ARRAY_DIMENSION])
{
  const FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  float *density = nullptr;
  int size = 0;

  if (fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
    /* high resolution smoke */
    int res[3];

    manta_noise_get_res(fds->fluid, res);
    size = res[0] * res[1] * res[2];

    density = manta_noise_get_density(fds->fluid);
  }
  else if (fds->fluid) {
    /* regular resolution */
    size = fds->res[0] * fds->res[1] * fds->res[2];
    density = manta_smoke_get_density(fds->fluid);
  }

  length[0] = (density) ? size : 0;
  return length[0];
}

static int rna_FluidModifier_color_grid_get_length(const PointerRNA *ptr,
                                                   int length[RNA_MAX_ARRAY_DIMENSION])
{
  rna_FluidModifier_grid_get_length(ptr, length);

  length[0] *= 4;
  return length[0];
}

static int rna_FluidModifier_velocity_grid_get_length(const PointerRNA *ptr,
                                                      int length[RNA_MAX_ARRAY_DIMENSION])
{
  const FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  float *vx = nullptr;
  float *vy = nullptr;
  float *vz = nullptr;
  int size = 0;

  /* Velocity data is always low-resolution. */
  if (fds->fluid) {
    size = 3 * fds->res[0] * fds->res[1] * fds->res[2];
    vx = manta_get_velocity_x(fds->fluid);
    vy = manta_get_velocity_y(fds->fluid);
    vz = manta_get_velocity_z(fds->fluid);
  }

  length[0] = (vx && vy && vz) ? size : 0;
  return length[0];
}

static int rna_FluidModifier_heat_grid_get_length(const PointerRNA *ptr,
                                                  int length[RNA_MAX_ARRAY_DIMENSION])
{
  const FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  float *heat = nullptr;
  int size = 0;

  /* Heat data is always low-resolution. */
  if (fds->fluid) {
    size = fds->res[0] * fds->res[1] * fds->res[2];
    heat = manta_smoke_get_heat(fds->fluid);
  }

  length[0] = (heat) ? size : 0;
  return length[0];
}

static void rna_FluidModifier_density_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_grid_get_length(ptr, length);
  float *density;

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  if (fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
    density = manta_noise_get_density(fds->fluid);
  }
  else {
    density = manta_smoke_get_density(fds->fluid);
  }

  memcpy(values, density, size * sizeof(float));

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}

static void rna_FluidModifier_velocity_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_velocity_grid_get_length(ptr, length);
  float *vx, *vy, *vz;
  int i;

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  vx = manta_get_velocity_x(fds->fluid);
  vy = manta_get_velocity_y(fds->fluid);
  vz = manta_get_velocity_z(fds->fluid);

  for (i = 0; i < size; i += 3) {
    *(values++) = *(vx++);
    *(values++) = *(vy++);
    *(values++) = *(vz++);
  }

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}

static void rna_FluidModifier_color_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_grid_get_length(ptr, length);

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  if (!fds->fluid) {
    memset(values, 0, size * sizeof(float));
  }
  else {
    if (fds->flags & FLUID_DOMAIN_USE_NOISE) {
      if (manta_noise_has_colors(fds->fluid)) {
        manta_noise_get_rgba(fds->fluid, values, 0);
      }
      else {
        manta_noise_get_rgba_fixed_color(fds->fluid, fds->active_color, values, 0);
      }
    }
    else {
      if (manta_smoke_has_colors(fds->fluid)) {
        manta_smoke_get_rgba(fds->fluid, values, 0);
      }
      else {
        manta_smoke_get_rgba_fixed_color(fds->fluid, fds->active_color, values, 0);
      }
    }
  }

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}

static void rna_FluidModifier_flame_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_grid_get_length(ptr, length);
  float *flame;

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  if (fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
    flame = manta_noise_get_flame(fds->fluid);
  }
  else {
    flame = manta_smoke_get_flame(fds->fluid);
  }

  if (flame) {
    memcpy(values, flame, size * sizeof(float));
  }
  else {
    memset(values, 0, size * sizeof(float));
  }

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}

static void rna_FluidModifier_heat_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_heat_grid_get_length(ptr, length);
  float *heat;

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  heat = manta_smoke_get_heat(fds->fluid);

  if (heat != nullptr) {
    /* scale heat values from -2.0-2.0 to -1.0-1.0. */
    for (int i = 0; i < size; i++) {
      values[i] = heat[i] * 0.5f;
    }
  }
  else {
    memset(values, 0, size * sizeof(float));
  }

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}

static void rna_FluidModifier_temperature_grid_get(PointerRNA *ptr, float *values)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;
  int length[RNA_MAX_ARRAY_DIMENSION];
  int size = rna_FluidModifier_grid_get_length(ptr, length);
  float *flame;

  BLI_rw_mutex_lock(static_cast<ThreadRWMutex *>(fds->fluid_mutex), THREAD_LOCK_READ);

  if (fds->flags & FLUID_DOMAIN_USE_NOISE && fds->fluid) {
    flame = manta_noise_get_flame(fds->fluid);
  }
  else {
    flame = manta_smoke_get_flame(fds->fluid);
  }

  if (flame) {
    /* Output is such that 0..1 maps to 0..1000K */
    float offset = fds->flame_ignition;
    float scale = fds->flame_max_temp - fds->flame_ignition;

    for (int i = 0; i < size; i++) {
      values[i] = (flame[i] > 0.01f) ? offset + flame[i] * scale : 0.0f;
    }
  }
  else {
    memset(values, 0, size * sizeof(float));
  }

  BLI_rw_mutex_unlock(static_cast<ThreadRWMutex *>(fds->fluid_mutex));
}
#  endif /* WITH_FLUID */

/** \} */

static void rna_FluidFlow_density_vgroup_get(PointerRNA *ptr, char *value)
{
  FluidFlowSettings *flow = (FluidFlowSettings *)ptr->data;
  rna_object_vgroup_name_index_get(ptr, value, flow->vgroup_density);
}

static int rna_FluidFlow_density_vgroup_length(PointerRNA *ptr)
{
  FluidFlowSettings *flow = (FluidFlowSettings *)ptr->data;
  return rna_object_vgroup_name_index_length(ptr, flow->vgroup_density);
}

static void rna_FluidFlow_density_vgroup_set(PointerRNA *ptr, const char *value)
{
  FluidFlowSettings *flow = (FluidFlowSettings *)ptr->data;
  rna_object_vgroup_name_index_set(ptr, value, &flow->vgroup_density);
}

static void rna_FluidFlow_uvlayer_set(PointerRNA *ptr, const char *value)
{
  FluidFlowSettings *flow = (FluidFlowSettings *)ptr->data;
  rna_object_uvlayer_name_set(ptr, value, flow->uvlayer_name, sizeof(flow->uvlayer_name));
}

static void rna_Fluid_use_color_ramp_set(PointerRNA *ptr, bool value)
{
  FluidDomainSettings *fds = (FluidDomainSettings *)ptr->data;

  fds->use_coba = value;

  if (value && fds->coba == nullptr) {
    fds->coba = BKE_colorband_add(false);
  }
}

static void rna_Fluid_flowsource_set(PointerRNA *ptr, int value)
{
  FluidFlowSettings *settings = (FluidFlowSettings *)ptr->data;

  if (value != settings->source) {
    settings->source = value;
  }
}

static const EnumPropertyItem *rna_Fluid_flowsource_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  FluidFlowSettings *settings = (FluidFlowSettings *)ptr->data;

  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  tmp.value = FLUID_FLOW_SOURCE_MESH;
  tmp.identifier = "MESH";
  tmp.icon = ICON_META_CUBE;
  tmp.name = N_("Mesh");
  tmp.description = N_("Emit fluid from mesh surface or volume");
  RNA_enum_item_add(&item, &totitem, &tmp);

  if (settings->type != FLUID_FLOW_TYPE_LIQUID) {
    tmp.value = FLUID_FLOW_SOURCE_PARTICLES;
    tmp.identifier = "PARTICLES";
    tmp.icon = ICON_PARTICLES;
    tmp.name = N_("Particle System");
    tmp.description = N_("Emit smoke from particles");
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_Fluid_flowtype_set(PointerRNA *ptr, int value)
{
  FluidFlowSettings *settings = (FluidFlowSettings *)ptr->data;

  if (value != settings->type) {
    short prev_value = settings->type;
    settings->type = value;

    /* Force flow source to mesh for liquids.
     * Also use different surface emission. Liquids should by default not emit around surface. */
    if (value == FLUID_FLOW_TYPE_LIQUID) {
      rna_Fluid_flowsource_set(ptr, FLUID_FLOW_SOURCE_MESH);
      settings->surface_distance = 0.0f;
    }
    /* Use some surface emission when switching to a gas emitter. Gases should by default emit a
     * bit around surface. */
    if (prev_value == FLUID_FLOW_TYPE_LIQUID) {
      settings->surface_distance = 1.0f;
    }
  }
}

#else

static void rna_def_fluid_domain_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem domain_types[] = {
      {FLUID_DOMAIN_TYPE_GAS, "GAS", 0, "Gas", "Create domain for gases"},
      {FLUID_DOMAIN_TYPE_LIQUID, "LIQUID", 0, "Liquid", "Create domain for liquids"},
      {0, nullptr, 0, nullptr, nullptr}};

  static const EnumPropertyItem prop_compression_items[] = {
      {VDB_COMPRESSION_ZIP, "ZIP", 0, "Zip", "Effective but slow compression"},
#  ifdef WITH_OPENVDB_BLOSC
      {VDB_COMPRESSION_BLOSC,
       "BLOSC",
       0,
       "Blosc",
       "Multithreaded compression, similar in size and quality as 'Zip'"},
#  endif
      {VDB_COMPRESSION_NONE, "NONE", 0, "None", "Do not use any compression"},
      {0, nullptr, 0, nullptr, nullptr}};

  static const EnumPropertyItem smoke_highres_sampling_items[] = {
      {SM_HRES_FULLSAMPLE, "FULLSAMPLE", 0, "Full Sample", ""},
      {SM_HRES_LINEAR, "LINEAR", 0, "Linear", ""},
      {SM_HRES_NEAREST, "NEAREST", 0, "Nearest", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem cache_types[] = {
      {FLUID_DOMAIN_CACHE_REPLAY, "REPLAY", 0, "Replay", "Use the timeline to bake the scene"},
      {FLUID_DOMAIN_CACHE_MODULAR,
       "MODULAR",
       0,
       "Modular",
       "Bake every stage of the simulation separately"},
      {FLUID_DOMAIN_CACHE_ALL, "ALL", 0, "All", "Bake all simulation settings at once"},
      {0, nullptr, 0, nullptr, nullptr}};

  /* OpenVDB data depth - generated dynamically based on domain type. */
  static const EnumPropertyItem fluid_data_depth_items[] = {
      {0, "NONE", 0, "", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem fluid_mesh_quality_items[] = {
      {FLUID_DOMAIN_MESH_IMPROVED,
       "IMPROVED",
       0,
       "Final",
       "Use improved particle level set (slower but more precise and with mesh smoothening "
       "options)"},
      {FLUID_DOMAIN_MESH_UNION,
       "UNION",
       0,
       "Preview",
       "Use union particle level set (faster but lower quality)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem fluid_guide_source_items[] = {
      {FLUID_DOMAIN_GUIDE_SRC_DOMAIN,
       "DOMAIN",
       0,
       "Domain",
       "Use a fluid domain for guiding (domain needs to be baked already so that velocities can "
       "be extracted). Guiding domain can be of any type (i.e. gas or liquid)."},
      {FLUID_DOMAIN_GUIDE_SRC_EFFECTOR,
       "EFFECTOR",
       0,
       "Effector",
       "Use guiding (effector) objects to create fluid guiding (guiding objects should be "
       "animated and baked once set up completely)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Cache type - generated dynamically based on domain type. */
  static const EnumPropertyItem cache_file_type_items[] = {
      {FLUID_DOMAIN_FILE_UNI, "UNI", 0, "Uni Cache", "Uni file format (.uni)"},
      {FLUID_DOMAIN_FILE_OPENVDB, "OPENVDB", 0, "OpenVDB", "OpenVDB file format (.vdb)"},
      {FLUID_DOMAIN_FILE_RAW, "RAW", 0, "Raw Cache", "Raw file format (.raw)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem interp_method_item[] = {
      {FLUID_DISPLAY_INTERP_LINEAR, "LINEAR", 0, "Linear", "Good smoothness and speed"},
      {FLUID_DISPLAY_INTERP_CUBIC,
       "CUBIC",
       0,
       "Cubic",
       "Smoothed high quality interpolation, but slower"},
      {FLUID_DISPLAY_INTERP_CLOSEST, "CLOSEST", 0, "Closest", "No interpolation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem axis_slice_position_items[] = {
      {SLICE_AXIS_AUTO,
       "AUTO",
       0,
       "Auto",
       "Adjust slice direction according to the view direction"},
      {SLICE_AXIS_X, "X", 0, "X", "Slice along the X axis"},
      {SLICE_AXIS_Y, "Y", 0, "Y", "Slice along the Y axis"},
      {SLICE_AXIS_Z, "Z", 0, "Z", "Slice along the Z axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem vector_draw_items[] = {
      {VECTOR_DRAW_NEEDLE, "NEEDLE", 0, "Needle", "Display vectors as needles"},
      {VECTOR_DRAW_STREAMLINE, "STREAMLINE", 0, "Streamlines", "Display vectors as streamlines"},
      {VECTOR_DRAW_MAC, "MAC", 0, "MAC Grid", "Display vector field as MAC grid"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem vector_field_items[] = {
      {FLUID_DOMAIN_VECTOR_FIELD_VELOCITY,
       "FLUID_VELOCITY",
       0,
       "Fluid Velocity",
       "Velocity field of the fluid domain"},
      {FLUID_DOMAIN_VECTOR_FIELD_GUIDE_VELOCITY,
       "GUIDE_VELOCITY",
       0,
       "Guide Velocity",
       "Guide velocity field of the fluid domain"},
      {FLUID_DOMAIN_VECTOR_FIELD_FORCE, "FORCE", 0, "Force", "Force field of the fluid domain"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem gridlines_color_field_items[] = {
      {FLUID_GRIDLINE_COLOR_TYPE_NONE, "NONE", 0, "None", "None"},
      {FLUID_GRIDLINE_COLOR_TYPE_FLAGS, "FLAGS", 0, "Flags", "Flag grid of the fluid domain"},
      {FLUID_GRIDLINE_COLOR_TYPE_RANGE,
       "RANGE",
       0,
       "Highlight Range",
       "Highlight the voxels with values of the color mapped field within the range"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem gridlines_cell_filter_items[] = {
      {FLUID_CELL_TYPE_NONE, "NONE", 0, "None", "Highlight the cells regardless of their type"},
      {FLUID_CELL_TYPE_FLUID, "FLUID", 0, "Fluid", "Highlight only the cells of type Fluid"},
      {FLUID_CELL_TYPE_OBSTACLE,
       "OBSTACLE",
       0,
       "Obstacle",
       "Highlight only the cells of type Obstacle"},
      {FLUID_CELL_TYPE_EMPTY, "EMPTY", 0, "Empty", "Highlight only the cells of type Empty"},
      {FLUID_CELL_TYPE_INFLOW, "INFLOW", 0, "Inflow", "Highlight only the cells of type Inflow"},
      {FLUID_CELL_TYPE_OUTFLOW,
       "OUTFLOW",
       0,
       "Outflow",
       "Highlight only the cells of type Outflow"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem sndparticle_boundary_items[] = {
      {SNDPARTICLE_BOUNDARY_DELETE,
       "DELETE",
       0,
       "Delete",
       "Delete secondary particles that are inside obstacles or left the domain"},
      {SNDPARTICLE_BOUNDARY_PUSHOUT,
       "PUSHOUT",
       0,
       "Push Out",
       "Push secondary particles that left the domain back into the domain"},
      {0, nullptr, 0, nullptr, nullptr}};

  static const EnumPropertyItem sndparticle_combined_export_items[] = {
      {SNDPARTICLE_COMBINED_EXPORT_OFF,
       "OFF",
       0,
       "Off",
       "Create a separate particle system for every secondary particle type"},
      {SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM,
       "SPRAY_FOAM",
       0,
       "Spray + Foam",
       "Spray and foam particles are saved in the same particle system"},
      {SNDPARTICLE_COMBINED_EXPORT_SPRAY_BUBBLE,
       "SPRAY_BUBBLES",
       0,
       "Spray + Bubbles",
       "Spray and bubble particles are saved in the same particle system"},
      {SNDPARTICLE_COMBINED_EXPORT_FOAM_BUBBLE,
       "FOAM_BUBBLES",
       0,
       "Foam + Bubbles",
       "Foam and bubbles particles are saved in the same particle system"},
      {SNDPARTICLE_COMBINED_EXPORT_SPRAY_FOAM_BUBBLE,
       "SPRAY_FOAM_BUBBLES",
       0,
       "Spray + Foam + Bubbles",
       "Create one particle system that contains all three secondary particle types"},
      {0, nullptr, 0, nullptr, nullptr}};

  static const EnumPropertyItem simulation_methods[] = {
      {FLUID_DOMAIN_METHOD_FLIP,
       "FLIP",
       0,
       "FLIP",
       "Use FLIP as the simulation method (more splashy behavior)"},
      {FLUID_DOMAIN_METHOD_APIC,
       "APIC",
       0,
       "APIC",
       "Use APIC as the simulation method (more energetic and stable behavior)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FluidDomainSettings", nullptr);
  RNA_def_struct_ui_text(srna, "Domain Settings", "Fluid domain settings");
  RNA_def_struct_sdna(srna, "FluidDomainSettings");
  RNA_def_struct_path_func(srna, "rna_FluidDomainSettings_path");

  prop = RNA_def_property(srna, "effector_weights", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EffectorWeights");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Effector Weights", "");

  /* object collections */

  prop = RNA_def_property(srna, "effector_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "effector_group");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Effector Collection", "Limit effectors to this collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_reset_dependency");

  prop = RNA_def_property(srna, "fluid_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "fluid_group");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Fluid Collection", "Limit fluid objects to this collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_reset_dependency");

  prop = RNA_def_property(srna, "force_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "force_group");
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Force Collection", "Limit forces to this collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_reset_dependency");

  /* grid access */

#  ifdef WITH_FLUID
  prop = RNA_def_property(srna, "density_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_density_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Density Grid", "Smoke density grid");

  prop = RNA_def_property(srna, "velocity_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_velocity_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_velocity_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Velocity Grid", "Smoke velocity grid");

  prop = RNA_def_property(srna, "flame_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_flame_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Flame Grid", "Smoke flame grid");

  prop = RNA_def_property(srna, "color_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_color_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_color_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Color Grid", "Smoke color grid");

  prop = RNA_def_property(srna, "heat_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_heat_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_heat_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Heat Grid", "Smoke heat grid");

  prop = RNA_def_property(srna, "temperature_grid", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 32);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_dynamic_array_funcs(prop, "rna_FluidModifier_grid_get_length");
  RNA_def_property_float_funcs(prop, "rna_FluidModifier_temperature_grid_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Temperature Grid", "Smoke temperature grid, range 0 to 1 represents 0 to 1000K");
#  endif /* WITH_FLUID */

  /* domain object data */

  prop = RNA_def_property(srna,
                          "start_point",
                          PROP_FLOAT,
                          PROP_XYZ); /* can change each frame when using adaptive domain */
  RNA_def_property_float_sdna(prop, nullptr, "p0");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "p0", "Start point");

  prop = RNA_def_property(srna,
                          "cell_size",
                          PROP_FLOAT,
                          PROP_XYZ); /* can change each frame when using adaptive domain */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "cell_size", "Cell Size");

  prop = RNA_def_property(srna,
                          "domain_resolution",
                          PROP_INT,
                          PROP_XYZ); /* can change each frame when using adaptive domain */
  RNA_def_property_int_sdna(prop, nullptr, "res");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "res", "Smoke Grid Resolution");

  /* adaptive domain options */

  prop = RNA_def_property(srna, "additional_res", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "adapt_res");
  RNA_def_property_range(prop, 0, 512);
  RNA_def_property_ui_text(prop, "Additional", "Maximum number of additional cells");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "adapt_margin", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "adapt_margin");
  RNA_def_property_range(prop, 2, 24);
  RNA_def_property_ui_text(
      prop, "Margin", "Margin added around fluid to minimize boundary interference");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "adapt_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.002, 6);
  RNA_def_property_ui_text(prop,
                           "Threshold",
                           "Minimum amount of fluid grid values (smoke density, fuel and heat) a "
                           "cell can contain, before it is considered empty");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_adaptive_domain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_ADAPTIVE_DOMAIN);
  RNA_def_property_ui_text(
      prop, "Adaptive Domain", "Adapt simulation resolution and size to fluid");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  /* fluid domain options */

  prop = RNA_def_property(srna, "resolution_max", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "maxres");
  RNA_def_property_range(prop, 6, 10000);
  RNA_def_property_ui_range(prop, 24, 10000, 2, -1);
  RNA_def_property_ui_text(
      prop,
      "Maximum Resolution",
      "Resolution used for the fluid domain. Value corresponds to the longest domain side "
      "(resolution for other domain sides is calculated automatically).");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "use_collision_border_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_FRONT);
  RNA_def_property_ui_text(prop, "Front", "Enable collisions with front domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_collision_border_back", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_BACK);
  RNA_def_property_ui_text(prop, "Back", "Enable collisions with back domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_collision_border_right", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_RIGHT);
  RNA_def_property_ui_text(prop, "Right", "Enable collisions with right domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_collision_border_left", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_LEFT);
  RNA_def_property_ui_text(prop, "Left", "Enable collisions with left domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_collision_border_top", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_TOP);
  RNA_def_property_ui_text(prop, "Top", "Enable collisions with top domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_collision_border_bottom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "border_collisions", FLUID_DOMAIN_BORDER_BOTTOM);
  RNA_def_property_ui_text(prop, "Bottom", "Enable collisions with bottom domain border");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
  RNA_def_property_float_sdna(prop, nullptr, "gravity");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1000.1, 1000.1);
  RNA_def_property_ui_text(prop, "Gravity", "Gravity in X, Y and Z direction");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "domain_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, domain_types);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Fluid_domaintype_set", nullptr);
  RNA_def_property_ui_text(prop, "Domain Type", "Change domain type of the simulation");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_flip_parts_update");

  prop = RNA_def_property(srna, "delete_in_obstacle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_DELETE_IN_OBSTACLE);
  RNA_def_property_ui_text(prop, "Clear In Obstacle", "Delete fluid inside obstacles");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* smoke domain options */

  prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "alpha");
  RNA_def_property_range(prop, -5.0, 5.0);
  RNA_def_property_ui_range(prop, -5.0, 5.0, 0.02, 5);
  RNA_def_property_ui_text(
      prop,
      "Buoyancy Density",
      "Buoyant force based on smoke density (higher value results in faster rising smoke)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "beta", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "beta");
  RNA_def_property_range(prop, -5.0, 5.0);
  RNA_def_property_ui_range(prop, -5.0, 5.0, 0.02, 5);
  RNA_def_property_ui_text(
      prop,
      "Buoyancy Heat",
      "Buoyant force based on smoke heat (higher value results in faster rising smoke)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "dissolve_speed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "diss_speed");
  RNA_def_property_range(prop, 1.0, 10000.0);
  RNA_def_property_ui_range(prop, 1.0, 10000.0, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Dissolve Speed",
      "Determine how quickly the smoke dissolves (lower value makes smoke disappear faster)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "vorticity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vorticity");
  RNA_def_property_range(prop, 0.0, 4.0);
  RNA_def_property_ui_text(prop, "Vorticity", "Amount of turbulence and rotation in smoke");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "highres_sampling", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, smoke_highres_sampling_items);
  RNA_def_property_ui_text(prop, "Emitter", "Method for sampling the high resolution flow");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_dissolve_smoke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_DISSOLVE);
  RNA_def_property_ui_text(prop, "Dissolve Smoke", "Let smoke disappear over time");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_dissolve_smoke_log", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_DISSOLVE_LOG);
  RNA_def_property_ui_text(
      prop,
      "Logarithmic Dissolve",
      "Dissolve smoke in a logarithmic fashion. Dissolves quickly at first, but lingers longer.");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* flame options */

  prop = RNA_def_property(srna, "burning_rate", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01, 4.0);
  RNA_def_property_ui_range(prop, 0.01, 2.0, 1.0, 5);
  RNA_def_property_ui_text(
      prop, "Speed", "Speed of the burning reaction (higher value results in smaller flames)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "flame_smoke", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 8.0);
  RNA_def_property_ui_range(prop, 0.0, 4.0, 1.0, 5);
  RNA_def_property_ui_text(prop, "Smoke", "Amount of smoke created by burning fuel");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "flame_vorticity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 2.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1.0, 5);
  RNA_def_property_ui_text(prop, "Vorticity", "Additional vorticity for the flames");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "flame_ignition", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.5, 5.0);
  RNA_def_property_ui_range(prop, 0.5, 2.5, 1.0, 5);
  RNA_def_property_ui_text(
      prop,
      "Minimum",
      "Minimum temperature of the flames (higher value results in faster rising flames)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "flame_max_temp", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0, 10.0);
  RNA_def_property_ui_range(prop, 1.0, 5.0, 1.0, 5);
  RNA_def_property_ui_text(
      prop,
      "Maximum",
      "Maximum temperature of the flames (higher value results in faster rising flames)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "flame_smoke_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Smoke Color", "Color of smoke emitted from burning fuel");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* noise options */

  prop = RNA_def_property(srna, "noise_strength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "noise_strength");
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 1, 2);
  RNA_def_property_ui_text(prop, "Strength", "Strength of noise");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_noisecache_reset");

  prop = RNA_def_property(srna, "noise_pos_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "noise_pos_scale");
  RNA_def_property_range(prop, 0.0001, 10.0);
  RNA_def_property_ui_text(
      prop, "Scale", "Scale of noise (higher value results in larger vortices)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_noisecache_reset");

  prop = RNA_def_property(srna, "noise_time_anim", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "noise_time_anim");
  RNA_def_property_range(prop, 0.0001, 10.0);
  RNA_def_property_ui_text(prop, "Time", "Animation time of noise");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_noisecache_reset");

  prop = RNA_def_property(srna, "noise_scale", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "noise_scale");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 1, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Noise Scale",
                           "The noise simulation is scaled up by this factor (compared to the "
                           "base resolution of the domain)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_noise_reset");

  prop = RNA_def_property(srna, "use_noise", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_NOISE);
  RNA_def_property_ui_text(prop, "Use Noise", "Enable fluid noise (using amplification)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_update");

  /* liquid domain options */

  prop = RNA_def_property(srna, "simulation_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "simulation_method");
  RNA_def_property_enum_items(prop, simulation_methods);
  RNA_def_property_ui_text(prop, "Simulation Method", "Change the underlying simulation method");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "flip_ratio", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_text(
      prop,
      "FLIP Ratio",
      "PIC/FLIP Ratio. A value of 1.0 will result in a completely FLIP based simulation. Use a "
      "lower value for simulations which should produce smaller splashes.");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_randomness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(prop, "Randomness", "Randomness factor for particle sampling");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_number", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 5);
  RNA_def_property_ui_text(
      prop, "Number", "Particle number factor (higher value results in more particles)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_min", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "particle_minimum");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop,
                           "Minimum",
                           "Minimum number of particles per cell (ensures that each cell has at "
                           "least this amount of particles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_max", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "particle_maximum");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop,
                           "Maximum",
                           "Maximum number of particles per cell (ensures that each cell has at "
                           "most this amount of particles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(prop,
                           "Radius",
                           "Particle radius factor. Increase this value if the simulation appears "
                           "to leak volume, decrease it if the simulation seems to gain volume.");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "particle_band_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_text(
      prop,
      "Width",
      "Particle (narrow) band width (higher value results in thicker band and more particles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_flip_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particle_type", FLUID_DOMAIN_PARTICLE_FLIP);
  RNA_def_property_ui_text(prop, "FLIP", "Create liquid particle system");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_flip_parts_update");

  prop = RNA_def_property(srna, "use_fractions", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_FRACTIONS);
  RNA_def_property_ui_text(
      prop,
      "Fractional Obstacles",
      "Fractional obstacles improve and smoothen the fluid-obstacle boundary");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "fractions_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001, 1.0);
  RNA_def_property_ui_range(prop, 0.01, 1.0, 0.05, -1);
  RNA_def_property_ui_text(prop,
                           "Obstacle Threshold",
                           "Determines how much fluid is allowed in an obstacle cell "
                           "(higher values will tag a boundary cell as an obstacle easier "
                           "and reduce the boundary smoothening effect)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "fractions_distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -5.0, 5.0);
  RNA_def_property_ui_range(prop, 0.01, 5.0, 0.1, -1);
  RNA_def_property_ui_text(prop,
                           "Obstacle Distance",
                           "Determines how far apart fluid and obstacle are (higher values will "
                           "result in fluid being further away from obstacles, smaller values "
                           "will let fluid move towards the inside of obstacles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "sys_particle_maximum", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sys_particle_maximum");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(
      prop,
      "System Maximum",
      "Maximum number of fluid particles that are allowed in this simulation");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* Viscosity options. */

  prop = RNA_def_property(srna, "use_viscosity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_VISCOSITY);
  RNA_def_property_ui_text(
      prop, "Use Viscosity", "Simulate fluids with high viscosity using a special solver");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "viscosity_value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0, 5.0, 1.0, 3);
  RNA_def_property_ui_text(prop,
                           "Strength",
                           "Viscosity of liquid (higher values result in more viscous fluids, a "
                           "value of 0 will still apply some viscosity)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* Diffusion options. */

  prop = RNA_def_property(srna, "use_diffusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_DIFFUSION);
  RNA_def_property_ui_text(
      prop, "Use Diffusion", "Enable fluid diffusion settings (e.g. viscosity, surface tension)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "surface_tension", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_text(
      prop,
      "Tension",
      "Surface tension of liquid (higher value results in greater hydrophobic behavior)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "viscosity_base", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "viscosity_base");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(
      prop,
      "Viscosity Base",
      "Viscosity setting: value that is multiplied by 10 to the power of (exponent*-1)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "viscosity_exponent", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "viscosity_exponent");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(
      prop,
      "Viscosity Exponent",
      "Negative exponent for the viscosity value (to simplify entering small values "
      "e.g. 5*10^-6)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* Mesh options. */

  prop = RNA_def_property(srna, "mesh_concave_upper", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(
      prop,
      "Upper Concavity",
      "Upper mesh concavity bound (high values tend to smoothen and fill out concave regions)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "mesh_concave_lower", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(
      prop,
      "Lower Concavity",
      "Lower mesh concavity bound (high values tend to smoothen and fill out concave regions)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "mesh_smoothen_pos", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Smoothen Pos", "Positive mesh smoothening");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "mesh_smoothen_neg", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop, "Smoothen Neg", "Negative mesh smoothening");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "mesh_scale", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "mesh_scale");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 1, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Mesh scale",
                           "The mesh simulation is scaled up by this factor (compared to the base "
                           "resolution of the domain). For best meshing, it is recommended to "
                           "adjust the mesh particle radius alongside this value.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_mesh_reset");

  prop = RNA_def_property(srna, "mesh_generator", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mesh_generator");
  RNA_def_property_enum_items(prop, fluid_mesh_quality_items);
  RNA_def_property_ui_text(prop, "Mesh generator", "Which particle level set generator to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_update");

  prop = RNA_def_property(srna, "use_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_MESH);
  RNA_def_property_ui_text(prop, "Use Mesh", "Enable fluid mesh (using amplification)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_update");

  prop = RNA_def_property(srna, "use_speed_vectors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_SPEED_VECTORS);
  RNA_def_property_ui_text(prop,
                           "Speed Vectors",
                           "Caches velocities of mesh vertices. These will be used "
                           "(automatically) when rendering with motion blur enabled.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "mesh_particle_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(prop,
                           "Radius",
                           "Particle radius factor (higher value results in larger (meshed) "
                           "particles). Needs to be adjusted after changing the mesh scale.");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  /* Secondary particles options. */

  prop = RNA_def_property(srna, "sndparticle_potential_min_wavecrest", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_min_wc");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 100.0, 3);
  RNA_def_property_ui_text(prop,
                           "Minimum Wave Crest Potential",
                           "Lower clamping threshold for marking fluid cells as wave crests "
                           "(lower value results in more marked cells)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_potential_max_wavecrest", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_max_wc");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 100.0, 3);
  RNA_def_property_ui_text(prop,
                           "Maximum Wave Crest Potential",
                           "Upper clamping threshold for marking fluid cells as wave crests "
                           "(higher value results in less marked cells)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_potential_min_trappedair", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_min_ta");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 10000.0, 100.0, 3);
  RNA_def_property_ui_text(prop,
                           "Minimum Trapped Air Potential",
                           "Lower clamping threshold for marking fluid cells where air is trapped "
                           "(lower value results in more marked cells)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_potential_max_trappedair", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_max_ta");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 100.0, 3);
  RNA_def_property_ui_text(prop,
                           "Maximum Trapped Air Potential",
                           "Upper clamping threshold for marking fluid cells where air is trapped "
                           "(higher value results in less marked cells)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_potential_min_energy", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_min_k");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 100.0, 3);
  RNA_def_property_ui_text(
      prop,
      "Minimum Kinetic Energy Potential",
      "Lower clamping threshold that indicates the fluid speed where cells start to emit "
      "particles (lower values result in generally more particles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_potential_max_energy", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_tau_max_k");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 1000.0, 100.0, 3);
  RNA_def_property_ui_text(
      prop,
      "Maximum Kinetic Energy Potential",
      "Upper clamping threshold that indicates the fluid speed where cells no longer emit more "
      "particles (higher value results in generally less particles)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_sampling_wavecrest", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sndparticle_k_wc");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_range(prop, 0, 10000, 1.0, -1);
  RNA_def_property_ui_text(prop,
                           "Wave Crest Sampling",
                           "Maximum number of particles generated per wave crest cell per frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_sampling_trappedair", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sndparticle_k_ta");
  RNA_def_property_range(prop, 0, 10000);
  RNA_def_property_ui_range(prop, 0, 10000, 1.0, -1);
  RNA_def_property_ui_text(prop,
                           "Trapped Air Sampling",
                           "Maximum number of particles generated per trapped air cell per frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_bubble_buoyancy", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_k_b");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 10.0, 2);
  RNA_def_property_ui_text(prop,
                           "Bubble Buoyancy",
                           "Amount of buoyancy force that rises bubbles (high value results in "
                           "bubble movement mainly upwards)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_bubble_drag", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_k_d");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 10.0, 2);
  RNA_def_property_ui_text(prop,
                           "Bubble Drag",
                           "Amount of drag force that moves bubbles along with the fluid (high "
                           "value results in bubble movement mainly along with the fluid)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_life_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_l_min");
  RNA_def_property_range(prop, 0.0, 10000.0);
  RNA_def_property_ui_range(prop, 0.0, 10000.0, 100.0, 1);
  RNA_def_property_ui_text(prop, "Minimum Lifetime", "Lowest possible particle lifetime");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_life_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "sndparticle_l_max");
  RNA_def_property_range(prop, 0.0, 10000.0);
  RNA_def_property_ui_range(prop, 0.0, 10000.0, 100.0, 1);
  RNA_def_property_ui_text(prop, "Maximum Lifetime", "Highest possible particle lifetime");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_boundary", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sndparticle_boundary");
  RNA_def_property_enum_items(prop, sndparticle_boundary_items);
  RNA_def_property_ui_text(
      prop, "Particles in Boundary", "How particles that left the domain are treated");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_combined_export", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sndparticle_combined_export");
  RNA_def_property_enum_items(prop, sndparticle_combined_export_items);
  RNA_def_property_ui_text(
      prop,
      "Combined Export",
      "Determines which particle systems are created from secondary particles");
  RNA_def_property_update(prop, 0, "rna_Fluid_combined_export_update");

  prop = RNA_def_property(srna, "sndparticle_potential_radius", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sndparticle_potential_radius");
  RNA_def_property_range(prop, 1, 4);
  RNA_def_property_ui_range(prop, 1, 4, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Potential Radius",
                           "Radius to compute potential for each cell (higher values are slower "
                           "but create smoother potential grids)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "sndparticle_update_radius", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sndparticle_update_radius");
  RNA_def_property_range(prop, 1, 4);
  RNA_def_property_ui_range(prop, 1, 4, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Update Radius",
                           "Radius to compute position update for each particle (higher values "
                           "are slower but particles move less chaotic)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "particle_scale", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "particle_scale");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 1, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Particle scale",
                           "The particle simulation is scaled up by this factor (compared to the "
                           "base resolution of the domain)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_particles_reset");

  prop = RNA_def_property(srna, "use_spray_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particle_type", FLUID_DOMAIN_PARTICLE_SPRAY);
  RNA_def_property_ui_text(prop, "Spray", "Create spray particle system");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_spray_parts_update");

  prop = RNA_def_property(srna, "use_bubble_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particle_type", FLUID_DOMAIN_PARTICLE_BUBBLE);
  RNA_def_property_ui_text(prop, "Bubble", "Create bubble particle system");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_bubble_parts_update");

  prop = RNA_def_property(srna, "use_foam_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particle_type", FLUID_DOMAIN_PARTICLE_FOAM);
  RNA_def_property_ui_text(prop, "Foam", "Create foam particle system");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_foam_parts_update");

  prop = RNA_def_property(srna, "use_tracer_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "particle_type", FLUID_DOMAIN_PARTICLE_TRACER);
  RNA_def_property_ui_text(prop, "Tracer", "Create tracer particle system");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_tracer_parts_update");

  /* fluid guiding options */

  prop = RNA_def_property(srna, "guide_alpha", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "guide_alpha");
  RNA_def_property_range(prop, 1.0, 100.0);
  RNA_def_property_ui_text(prop, "Weight", "Guiding weight (higher value results in greater lag)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "guide_beta", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "guide_beta");
  RNA_def_property_range(prop, 1, 50);
  RNA_def_property_ui_text(prop, "Size", "Guiding size (higher value results in larger vortices)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "guide_vel_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "guide_vel_factor");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_text(
      prop,
      "Velocity Factor",
      "Guiding velocity factor (higher value results in greater guiding velocities)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "guide_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "guide_source");
  RNA_def_property_enum_items(prop, fluid_guide_source_items);
  RNA_def_property_ui_text(prop, "Guiding source", "Choose where to get guiding velocities from");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_update");

  prop = RNA_def_property(srna, "guide_parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "guide_parent");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Fluid_guide_parent_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "",
                           "Use velocities from this object for the guiding effect (object needs "
                           "to have fluid modifier and be of type domain))");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_guidingcache_reset");

  prop = RNA_def_property(srna, "use_guide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_GUIDE);
  RNA_def_property_ui_text(prop, "Use Guiding", "Enable fluid guiding");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_update");

  /* cache options */

  prop = RNA_def_property(srna, "cache_frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_start");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Fluid_cache_startframe_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Start", "Frame on which the simulation starts (first frame baked)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "cache_frame_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_end");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_int_funcs(prop, nullptr, "rna_Fluid_cache_endframe_set", nullptr);
  RNA_def_property_ui_text(prop, "End", "Frame on which the simulation stops (last frame baked)");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "cache_frame_offset", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_offset");
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(
      prop,
      "Offset",
      "Frame offset that is used when loading the simulation from the cache. It is not considered "
      "when baking the simulation, only when loading it.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_property(srna, "cache_frame_pause_data", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_pause_data");

  prop = RNA_def_property(srna, "cache_frame_pause_noise", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_pause_noise");

  prop = RNA_def_property(srna, "cache_frame_pause_mesh", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_pause_mesh");

  prop = RNA_def_property(srna, "cache_frame_pause_particles", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_pause_particles");

  prop = RNA_def_property(srna, "cache_frame_pause_guide", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "cache_frame_pause_guide");

  prop = RNA_def_property(srna, "cache_mesh_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "cache_mesh_format");
  RNA_def_property_enum_items(prop, cache_file_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Fluid_cachetype_mesh_set", "rna_Fluid_cachetype_mesh_itemf");
  RNA_def_property_ui_text(
      prop, "File Format", "Select the file format to be used for caching surface data");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_meshcache_reset");

  prop = RNA_def_property(srna, "cache_data_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "cache_data_format");
  RNA_def_property_enum_items(prop, cache_file_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Fluid_cachetype_data_set", "rna_Fluid_cachetype_volume_itemf");
  RNA_def_property_ui_text(
      prop, "File Format", "Select the file format to be used for caching volumetric data");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "cache_particle_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "cache_particle_format");
  RNA_def_property_enum_items(prop, cache_file_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Fluid_cachetype_particle_set", "rna_Fluid_cachetype_particle_itemf");
  RNA_def_property_ui_text(
      prop, "File Format", "Select the file format to be used for caching particle data");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_particlescache_reset");

  prop = RNA_def_property(srna, "cache_noise_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "cache_noise_format");
  RNA_def_property_enum_items(prop, cache_file_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Fluid_cachetype_noise_set", "rna_Fluid_cachetype_volume_itemf");
  RNA_def_property_ui_text(
      prop, "File Format", "Select the file format to be used for caching noise data");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_noisecache_reset");

  prop = RNA_def_property(srna, "cache_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "cache_type");
  RNA_def_property_enum_items(prop, cache_types);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Fluid_cachetype_set", nullptr);
  RNA_def_property_ui_text(prop, "Type", "Change the cache type of the simulation");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "cache_resumable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_RESUMABLE_CACHE);
  RNA_def_property_ui_text(
      prop,
      "Resumable",
      "Additional data will be saved so that the bake jobs can be resumed after pausing. Because "
      "more data will be written to disk it is recommended to avoid enabling this option when "
      "baking at high resolutions.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "cache_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_maxlength(prop, FILE_MAX);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Fluid_cache_directory_set");
  RNA_def_property_string_sdna(prop, nullptr, "cache_directory");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_ui_text(prop, "Cache directory", "Directory that contains fluid cache files");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_update");

  prop = RNA_def_property(srna, "is_cache_baking_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_DATA);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "has_cache_baked_data", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_DATA);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "is_cache_baking_noise", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_NOISE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "has_cache_baked_noise", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_NOISE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "is_cache_baking_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_MESH);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "has_cache_baked_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_MESH);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "is_cache_baking_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_PARTICLES);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "has_cache_baked_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_PARTICLES);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "is_cache_baking_guide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_GUIDE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "has_cache_baked_guide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_GUIDE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  /* Read only checks, avoids individually accessing flags above. */
  prop = RNA_def_property(srna, "is_cache_baking_any", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKING_ALL);
  RNA_def_property_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "has_cache_baked_any", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", FLUID_DOMAIN_BAKED_ALL);
  RNA_def_property_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "export_manta_script", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_EXPORT_MANTA_SCRIPT);
  RNA_def_property_ui_text(
      prop,
      "Export Mantaflow Script",
      "Generate and export Mantaflow script from current domain settings during bake. This is "
      "only needed if you plan to analyze the cache (e.g. view grids, velocity vectors, "
      "particles) in Mantaflow directly (outside of Blender) after baking the simulation.");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "openvdb_cache_compress_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "openvdb_compression");
  RNA_def_property_enum_items(prop, prop_compression_items);
  RNA_def_property_ui_text(prop, "Compression", "Compression method to be used");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "openvdb_data_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "openvdb_data_depth");
  RNA_def_property_enum_items(prop, fluid_data_depth_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Fluid_data_depth_itemf");
  RNA_def_property_ui_text(
      prop,
      "Data Depth",
      "Bit depth for fluid particles and grids (lower bit values reduce file size)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  /* time options */

  prop = RNA_def_property(srna, "time_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "time_scale");
  RNA_def_property_range(prop, 0.0001, 10.0);
  RNA_def_property_ui_text(prop, "Time Scale", "Adjust simulation speed");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "use_adaptive_timesteps", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_DOMAIN_USE_ADAPTIVE_TIME);
  RNA_def_property_ui_text(
      prop,
      "Adaptive Time Steps",
      "Automatically decide when to perform multiple simulation steps per frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "cfl_condition", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "cfl_condition");
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_text(prop,
                           "CFL",
                           "Maximal velocity per cell (greater CFL numbers will minimize the "
                           "number of simulation steps and the computation time.)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "timesteps_min", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "timesteps_minimum");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 0, 100, 1, -1);
  RNA_def_property_ui_text(
      prop, "Minimum", "Minimum number of simulation steps to perform for one frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  prop = RNA_def_property(srna, "timesteps_max", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "timesteps_maximum");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_range(prop, 0, 100, 1, -1);
  RNA_def_property_ui_text(
      prop, "Maximum", "Maximum number of simulation steps to perform for one frame");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_datacache_reset");

  /* display settings */

  prop = RNA_def_property(srna, "use_slice", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "axis_slice_method", AXIS_SLICE_SINGLE);
  RNA_def_property_ui_text(prop, "Slice", "Perform a single slice of the domain object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "slice_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "slice_axis");
  RNA_def_property_enum_items(prop, axis_slice_position_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "slice_per_voxel", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "slice_per_voxel");
  RNA_def_property_range(prop, 0.0, 100.0);
  RNA_def_property_ui_range(prop, 0.0, 5.0, 0.1, 1);
  RNA_def_property_ui_text(
      prop, "Slice Per Voxel", "How many slices per voxel should be generated");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "slice_depth", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "slice_depth");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Position", "Position of the slice");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "display_thickness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "display_thickness");
  RNA_def_property_range(prop, 0.001, 1000.0);
  RNA_def_property_ui_range(prop, 0.1, 100.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of smoke display in the viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "display_interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "interp_method");
  RNA_def_property_enum_items(prop, interp_method_item);
  RNA_def_property_ui_text(
      prop, "Interpolation", "Interpolation method to use for smoke/fire volumes in solid mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_gridlines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_gridlines", 0);
  RNA_def_property_ui_text(prop, "Gridlines", "Show gridlines");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw_velocity", 0);
  RNA_def_property_ui_text(prop, "Vector Display", "Visualize vector fields");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "vector_draw_type");
  RNA_def_property_enum_items(prop, vector_draw_items);
  RNA_def_property_ui_text(prop, "Display Type", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_field", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "vector_field");
  RNA_def_property_enum_items(prop, vector_field_items);
  RNA_def_property_ui_text(prop, "Field", "Vector field to be represented by the display vectors");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_scale_with_magnitude", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vector_scale_with_magnitude", 0);
  RNA_def_property_ui_text(prop, "Magnitude", "Scale vectors with their magnitudes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_show_mac_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vector_draw_mac_components", VECTOR_DRAW_MAC_X);
  RNA_def_property_ui_text(prop, "X", "Show X-component of MAC Grid");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_show_mac_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vector_draw_mac_components", VECTOR_DRAW_MAC_Y);
  RNA_def_property_ui_text(prop, "Y", "Show Y-component of MAC Grid");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_show_mac_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "vector_draw_mac_components", VECTOR_DRAW_MAC_Z);
  RNA_def_property_ui_text(prop, "Z", "Show Z-component of MAC Grid");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "vector_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vector_scale");
  RNA_def_property_range(prop, 0.0, 1000.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Scale", "Multiplier for scaling the vectors");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  /* --------- Color mapping. --------- */

  prop = RNA_def_property(srna, "use_color_ramp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_coba", 0);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Fluid_use_color_ramp_set");
  RNA_def_property_ui_text(prop,
                           "Grid Display",
                           "Render a simulation field while mapping its voxels values to the "
                           "colors of a ramp or using a predefined color code");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  /* Color ramp field items are generated dynamically based on domain type. */
  static const EnumPropertyItem coba_field_items[] = {
      {0, "NONE", 0, "", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "color_ramp_field", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "coba_field");
  RNA_def_property_enum_items(prop, coba_field_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Fluid_cobafield_itemf");
  RNA_def_property_ui_text(prop, "Field", "Simulation field to color map");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "color_ramp_field_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "grid_scale");
  RNA_def_property_range(prop, 0.001, 100000.0);
  RNA_def_property_ui_range(prop, 0.001, 1000.0, 0.1, 3);
  RNA_def_property_ui_text(
      prop, "Scale", "Multiplier for scaling the selected field to color map");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "coba");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Color Ramp", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "clipping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clipping");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 6);
  RNA_def_property_ui_text(
      prop,
      "Clipping",
      "Value under which voxels are considered empty space to optimize rendering");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "gridlines_color_field", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gridlines_color_field");
  RNA_def_property_enum_items(prop, gridlines_color_field_items);
  RNA_def_property_ui_text(
      prop, "Color Gridlines", "Simulation field to color map onto gridlines");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "gridlines_lower_bound", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "gridlines_lower_bound");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1, 6);
  RNA_def_property_ui_text(prop, "Lower Bound", "Lower bound of the highlighting range");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "gridlines_upper_bound", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "gridlines_upper_bound");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1, 6);
  RNA_def_property_ui_text(prop, "Upper Bound", "Upper bound of the highlighting range");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "gridlines_range_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gridlines_range_color");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "Color used to highlight the range");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);

  prop = RNA_def_property(srna, "gridlines_cell_filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gridlines_cell_filter");
  RNA_def_property_enum_items(prop, gridlines_cell_filter_items);
  RNA_def_property_ui_text(prop, "Cell Type", "Cell type to be highlighted");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "velocity_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "velocity_scale");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Velocity Scale", "Factor to control the amount of motion blur");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_update");
}

static void rna_def_fluid_flow_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem flow_type_items[] = {
      {FLUID_FLOW_TYPE_SMOKE, "SMOKE", 0, "Smoke", "Add smoke"},
      {FLUID_FLOW_TYPE_SMOKEFIRE, "BOTH", 0, "Fire + Smoke", "Add fire and smoke"},
      {FLUID_FLOW_TYPE_FIRE, "FIRE", 0, "Fire", "Add fire"},
      {FLUID_FLOW_TYPE_LIQUID, "LIQUID", 0, "Liquid", "Add liquid"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem flow_behavior_items[] = {
      {FLUID_FLOW_BEHAVIOR_INFLOW, "INFLOW", 0, "Inflow", "Add fluid to simulation"},
      {FLUID_FLOW_BEHAVIOR_OUTFLOW, "OUTFLOW", 0, "Outflow", "Delete fluid from simulation"},
      {FLUID_FLOW_BEHAVIOR_GEOMETRY,
       "GEOMETRY",
       0,
       "Geometry",
       "Only use given geometry for fluid"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Flow source - generated dynamically based on flow type. */
  static const EnumPropertyItem flow_sources[] = {
      {0, "NONE", 0, "", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem flow_texture_types[] = {
      {FLUID_FLOW_TEXTURE_MAP_AUTO,
       "AUTO",
       0,
       "Generated",
       "Generated coordinates centered to flow object"},
      {FLUID_FLOW_TEXTURE_MAP_UV, "UV", 0, "UV", "Use UV layer for texture coordinates"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FluidFlowSettings", nullptr);
  RNA_def_struct_ui_text(srna, "Flow Settings", "Fluid flow settings");
  RNA_def_struct_sdna(srna, "FluidFlowSettings");
  RNA_def_struct_path_func(srna, "rna_FluidFlowSettings_path");

  prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "density");
  RNA_def_property_range(prop, 0.0, 10);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1.0, 4);
  RNA_def_property_ui_text(prop, "Density", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "smoke_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Smoke Color", "Color of smoke");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "fuel_amount", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10);
  RNA_def_property_ui_range(prop, 0.0, 5.0, 1.0, 4);
  RNA_def_property_ui_text(prop, "Flame Rate", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "temperature", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "temperature");
  RNA_def_property_range(prop, -10, 10);
  RNA_def_property_ui_range(prop, -10, 10, 1, 1);
  RNA_def_property_ui_text(
      prop, "Temperature Difference", "Temperature difference to ambient temperature");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "particle_system", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "psys");
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Particle Systems", "Particle systems emitted from the object");
  RNA_def_property_update(prop, 0, "rna_Fluid_reset_dependency");

  prop = RNA_def_property(srna, "flow_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, flow_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Fluid_flowtype_set", nullptr);
  RNA_def_property_ui_text(prop, "Flow Type", "Change type of fluid in the simulation");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "flow_behavior", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "behavior");
  RNA_def_property_enum_items(prop, flow_behavior_items);
  RNA_def_property_ui_text(prop, "Flow Behavior", "Change flow behavior in the simulation");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "flow_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "source");
  RNA_def_property_enum_items(prop, flow_sources);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Fluid_flowsource_set", "rna_Fluid_flowsource_itemf");
  RNA_def_property_ui_text(prop, "Source", "Change how fluid is emitted");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_ABSOLUTE);
  RNA_def_property_ui_text(prop,
                           "Absolute Density",
                           "Only allow given density value in emitter area and will not add up");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_initial_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_INITVELOCITY);
  RNA_def_property_ui_text(
      prop, "Initial Velocity", "Fluid has some initial velocity when it is emitted");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "velocity_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vel_multi");
  RNA_def_property_range(prop, -100.0, 100.0);
  RNA_def_property_ui_range(prop, -2.0, 2.0, 0.05, 5);
  RNA_def_property_ui_text(prop,
                           "Source",
                           "Multiplier of source velocity passed to fluid (source velocity is "
                           "non-zero only if object is moving)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "velocity_normal", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vel_normal");
  RNA_def_property_range(prop, -100.0, 100.0);
  RNA_def_property_ui_range(prop, -2.0, 2.0, 0.05, 5);
  RNA_def_property_ui_text(prop, "Normal", "Amount of normal directional velocity");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "velocity_random", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vel_random");
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0, 2.0, 0.05, 5);
  RNA_def_property_ui_text(prop, "Random", "Amount of random velocity");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "velocity_coord", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_float_sdna(prop, nullptr, "vel_coord");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, -1000.1, 1000.1);
  RNA_def_property_ui_text(
      prop,
      "Initial",
      "Additional initial velocity in X, Y and Z direction (added to source velocity)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "volume_density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.05, 5);
  RNA_def_property_ui_text(prop,
                           "Volume Emission",
                           "Controls fluid emission from within the mesh (higher value results in "
                           "greater emissions from inside the mesh)");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "surface_distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 0.05, 5);
  RNA_def_property_ui_text(
      prop,
      "Surface Emission",
      "Height (in domain grid units) of fluid emission above the mesh surface. Higher values "
      "result in emission further away from the mesh surface. If this value and the emitter size "
      "are smaller than the domain grid unit, fluid will not be created");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_plane_init", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_USE_PLANE_INIT);
  RNA_def_property_ui_text(
      prop,
      "Is Planar",
      "Treat this object as a planar and unclosed mesh. Fluid will only be emitted from the mesh "
      "surface and based on the surface emission value.");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "particle_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.1, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.5, 5.0, 0.05, 5);
  RNA_def_property_ui_text(prop, "Size", "Particle size in simulation cells");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_particle_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_USE_PART_SIZE);
  RNA_def_property_ui_text(
      prop, "Set Size", "Set particle size in simulation cells or use nearest cell");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_inflow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_USE_INFLOW);
  RNA_def_property_ui_text(prop, "Use Flow", "Control when to apply fluid flow");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "subframes", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 200);
  RNA_def_property_ui_range(prop, 0, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Subframes",
                           "Number of additional samples to take between frames to improve "
                           "quality of fast moving flows");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "density_vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_FluidFlow_density_vgroup_get",
                                "rna_FluidFlow_density_vgroup_length",
                                "rna_FluidFlow_density_vgroup_set");
  RNA_def_property_ui_text(
      prop, "Vertex Group", "Name of vertex group which determines surface emission rate");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "use_texture", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_FLOW_TEXTUREEMIT);
  RNA_def_property_ui_text(prop, "Use Texture", "Use a texture to control emission strength");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "texture_map_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "texture_type");
  RNA_def_property_enum_items(prop, flow_texture_types);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXTURE);
  RNA_def_property_ui_text(prop, "Mapping", "Texture mapping type");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "uvlayer_name");
  RNA_def_property_ui_text(prop, "UV Map", "UV map name");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_FluidFlow_uvlayer_set");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "noise_texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Texture", "Texture that controls emission strength");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "texture_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01, 10.0);
  RNA_def_property_ui_range(prop, 0.1, 5.0, 0.05, 5);
  RNA_def_property_ui_text(prop, "Size", "Size of texture mapping");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");

  prop = RNA_def_property(srna, "texture_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 200.0);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 0.05, 5);
  RNA_def_property_ui_text(prop, "Offset", "Z-offset of texture mapping");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_flow_reset");
}

static void rna_def_fluid_effector_settings(BlenderRNA *brna)
{
  static const EnumPropertyItem effector_type_items[] = {
      {FLUID_EFFECTOR_TYPE_COLLISION, "COLLISION", 0, "Collision", "Create collision object"},
      {FLUID_EFFECTOR_TYPE_GUIDE, "GUIDE", 0, "Guide", "Create guide object"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem fluid_guide_mode_items[] = {
      {FLUID_EFFECTOR_GUIDE_MAX,
       "MAXIMUM",
       0,
       "Maximize",
       "Compare velocities from previous frame with new velocities from current frame and keep "
       "the maximum"},
      {FLUID_EFFECTOR_GUIDE_MIN,
       "MINIMUM",
       0,
       "Minimize",
       "Compare velocities from previous frame with new velocities from current frame and keep "
       "the minimum"},
      {FLUID_EFFECTOR_GUIDE_OVERRIDE,
       "OVERRIDE",
       0,
       "Override",
       "Always write new guide velocities for every frame (each frame only contains current "
       "velocities from guiding objects)"},
      {FLUID_EFFECTOR_GUIDE_AVERAGED,
       "AVERAGED",
       0,
       "Averaged",
       "Take average of velocities from previous frame and new velocities from current frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FluidEffectorSettings", nullptr);
  RNA_def_struct_ui_text(srna, "Effector Settings", "Smoke collision settings");
  RNA_def_struct_sdna(srna, "FluidEffectorSettings");
  RNA_def_struct_path_func(srna, "rna_FluidEffectorSettings_path");

  prop = RNA_def_property(srna, "effector_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, effector_type_items);
  RNA_def_property_ui_text(prop, "Effector Type", "Change type of effector in the simulation");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_effector_reset");

  prop = RNA_def_property(srna, "surface_distance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 10.0);
  RNA_def_property_ui_range(prop, 0.0, 10.0, 0.05, 5);
  RNA_def_property_ui_text(
      prop, "Surface", "Additional distance around mesh surface to consider as effector");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_effector_reset");

  prop = RNA_def_property(srna, "use_plane_init", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_EFFECTOR_USE_PLANE_INIT);
  RNA_def_property_ui_text(prop, "Is Planar", "Treat this object as a planar, unclosed mesh");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_domain_data_reset");

  prop = RNA_def_property(srna, "velocity_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "vel_multi");
  RNA_def_property_range(prop, -100.0, 100.0);
  RNA_def_property_ui_text(prop, "Source", "Multiplier of obstacle velocity");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_effector_reset");

  prop = RNA_def_property(srna, "guide_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "guide_mode");
  RNA_def_property_enum_items(prop, fluid_guide_mode_items);
  RNA_def_property_ui_text(prop, "Guiding mode", "How to create guiding velocities");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Fluid_effector_reset");

  prop = RNA_def_property(srna, "use_effector", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", FLUID_EFFECTOR_USE_EFFEC);
  RNA_def_property_ui_text(prop, "Enabled", "Control when to apply the effector");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_effector_reset");

  prop = RNA_def_property(srna, "subframes", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 200);
  RNA_def_property_ui_range(prop, 0, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Subframes",
                           "Number of additional samples to take between frames to improve "
                           "quality of fast moving effector objects");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Fluid_effector_reset");
}

void RNA_def_fluid(BlenderRNA *brna)
{
  rna_def_fluid_domain_settings(brna);
  rna_def_fluid_flow_settings(brna);
  rna_def_fluid_effector_settings(brna);
}

#endif
