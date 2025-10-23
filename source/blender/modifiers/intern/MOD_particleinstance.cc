/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_customdata.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(pimd, modifier));

  MEMCPY_STRUCT_AFTER(pimd, DNA_struct_default_get(ParticleInstanceModifierData), modifier);
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  if (pimd->index_layer_name[0] != '\0' || pimd->value_layer_name[0] != '\0') {
    r_cddata_masks->lmask |= CD_MASK_PROP_BYTE_COLOR;
  }
}

static bool is_disabled(const Scene *scene, ModifierData *md, bool use_render_params)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  ParticleSystem *psys;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  if (!pimd->ob || pimd->ob->type != OB_MESH) {
    return true;
  }

  psys = static_cast<ParticleSystem *>(BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1));
  if (psys == nullptr) {
    return true;
  }

  /* If the psys modifier is disabled we cannot use its data.
   * First look up the psys modifier from the object, then check if it is enabled.
   */
  LISTBASE_FOREACH (ModifierData *, ob_md, &pimd->ob->modifiers) {
    if (ob_md->type == eModifierType_ParticleSystem) {
      ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)ob_md;
      if (psmd->psys == psys) {
        int required_mode;

        if (use_render_params) {
          required_mode = eModifierMode_Render;
        }
        else {
          required_mode = eModifierMode_Realtime;
        }

        if (!BKE_modifier_is_enabled(scene, ob_md, required_mode)) {
          return true;
        }

        break;
      }
    }
  }

  return false;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  if (pimd->ob != nullptr) {
    DEG_add_object_relation(
        ctx->node, pimd->ob, DEG_OB_COMP_TRANSFORM, "Particle Instance Modifier");
    DEG_add_object_relation(
        ctx->node, pimd->ob, DEG_OB_COMP_GEOMETRY, "Particle Instance Modifier");
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;

  walk(user_data, ob, (ID **)&pimd->ob, IDWALK_CB_NOP);
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
    if (pa->flag & (PARS_UNEXIST | PARS_NO_DISP)) {
      return true;
    }
  }

  if (pimd->particle_amount == 1.0f) {
    /* Early output, all particles are to be instanced. */
    return false;
  }

  /* Randomly skip particles based on desired amount of visible particles. */

  totpart = psys->totpart + psys->totchild;

  /* TODO: make randomization optional? */
  randp = int(psys_frand(psys, 3578 + p) * totpart) % totpart;

  minp = int(totpart * pimd->particle_offset) % (totpart + 1);
  maxp = int(totpart * (pimd->particle_offset + pimd->particle_amount)) % (totpart + 1);

  if (maxp > minp) {
    return randp < minp || randp >= maxp;
  }
  if (maxp < minp) {
    return randp < minp && randp >= maxp;
  }

  return true;
}

static void store_float_in_vcol(blender::ColorGeometry4b *vcol, float float_value)
{
  const uchar value = unit_float_to_uchar_clamp(float_value);
  vcol->r = vcol->g = vcol->b = value;
  vcol->a = 1.0f;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  using namespace blender;
  Mesh *result;
  ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  ParticleSimulationData sim;
  ParticleSystem *psys = nullptr;
  ParticleData *pa = nullptr;
  int totvert, faces_num, totloop, totedge;
  int maxvert, maxpoly, maxloop, maxedge, part_end = 0, part_start;
  int k, p, p_skip;
  const uint track = uint(ctx->object->trackflag) % 3;
  short trackneg, axis = pimd->axis;
  float max_co = 0.0, min_co = 0.0, temp_co[3];
  float *size = nullptr;
  float spacemat[4][4];
  const bool use_parents = pimd->flag & eParticleInstanceFlag_Parents;
  const bool use_children = pimd->flag & eParticleInstanceFlag_Children;
  bool between;

  trackneg = ((ctx->object->trackflag > 2) ? 1 : 0);

  if (pimd->ob == ctx->object) {
    pimd->ob = nullptr;
    return mesh;
  }

  if (pimd->ob) {
    psys = static_cast<ParticleSystem *>(BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1));
    if (psys == nullptr || psys->totpart == 0) {
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
    si = size = MEM_calloc_arrayN<float>(part_end, __func__);

    if (pimd->flag & eParticleInstanceFlag_Parents) {
      for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++, si++) {
        *si = pa->size;
      }
    }

    if (pimd->flag & eParticleInstanceFlag_Children) {
      ChildParticle *cpa = psys->child;

      for (p = 0; p < psys->totchild; p++, cpa++, si++) {
        *si = psys_get_child_size(psys, cpa, 0.0f, nullptr);
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
      invert_m4_m4(spacemat, pimd->ob->object_to_world().ptr());
      break;
    default:
      /* should not happen */
      BLI_assert(false);
      break;
  }

  totvert = mesh->verts_num;
  faces_num = mesh->faces_num;
  totloop = mesh->corners_num;
  totedge = mesh->edges_num;

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
    maxpoly += faces_num;
    maxloop += totloop;
    maxedge += totedge;
  }

  psys_sim_data_init(&sim);

  if (psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) {
    if (const std::optional<blender::Bounds<blender::float3>> bounds = mesh->bounds_min_max()) {
      min_co = bounds->min[track];
      max_co = bounds->max[track];
    }
  }

  result = BKE_mesh_new_nomain_from_template(mesh, maxvert, maxedge, maxpoly, maxloop);

  const blender::OffsetIndices orig_faces = mesh->faces();
  const blender::Span<int> orig_corner_verts = mesh->corner_verts();
  const blender::Span<int> orig_corner_edges = mesh->corner_edges();
  blender::MutableSpan<blender::float3> positions = result->vert_positions_for_write();
  blender::MutableSpan<blender::int2> edges = result->edges_for_write();
  blender::MutableSpan<int> face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> corner_edges = result->corner_edges_for_write();
  blender::bke::MutableAttributeAccessor attributes = result->attributes_for_write();
  bke::SpanAttributeWriter mloopcols_index =
      attributes.lookup_or_add_for_write_span<ColorGeometry4b>(pimd->index_layer_name,
                                                               bke::AttrDomain::Corner);
  bke::SpanAttributeWriter mloopcols_value =
      attributes.lookup_or_add_for_write_span<ColorGeometry4b>(pimd->value_layer_name,
                                                               bke::AttrDomain::Corner);
  int *vert_part_index = nullptr;
  float *vert_part_value = nullptr;
  if (mloopcols_index) {
    vert_part_index = MEM_calloc_arrayN<int>(maxvert, "vertex part index array");
  }
  if (mloopcols_value) {
    vert_part_value = MEM_calloc_arrayN<float>(maxvert, "vertex part value array");
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
      int vindex = p_skip * totvert + k;

      CustomData_copy_data(&mesh->vert_data, &result->vert_data, k, vindex, 1);

      if (vert_part_index != nullptr) {
        vert_part_index[vindex] = p;
      }
      if (vert_part_value != nullptr) {
        vert_part_value[vindex] = p_random;
      }

      /* Change orientation based on object trackflag. */
      copy_v3_v3(temp_co, positions[vindex]);
      positions[vindex][axis] = temp_co[track];
      positions[vindex][(axis + 1) % 3] = temp_co[(track + 1) % 3];
      positions[vindex][(axis + 2) % 3] = temp_co[(track + 2) % 3];

      /* get particle state */
      if ((psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) &&
          (pimd->flag & eParticleInstanceFlag_Path))
      {
        float ran = 0.0f;
        if (pimd->random_position != 0.0f) {
          ran = pimd->random_position * BLI_hash_frand(psys->seed + p);
        }

        if (pimd->flag & eParticleInstanceFlag_KeepShape) {
          state.time = pimd->position * (1.0f - ran);
        }
        else {
          state.time = (positions[vindex][axis] - min_co) / (max_co - min_co) * pimd->position *
                       (1.0f - ran);

          if (trackneg) {
            state.time = 1.0f - state.time;
          }

          positions[vindex][axis] = 0.0;
        }

        psys_get_particle_on_path(&sim, p, &state, true);

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
          psys_mat_hair_to_global(sim.ob, sim.psmd->mesh_final, sim.psys->part->from, pa, hairmat);
          copy_m3_m4(mat, hairmat);
          /* to quaternion */
          mat3_to_quat(frame, mat);

          if (pimd->rotation > 0.0f || pimd->random_rotation > 0.0f) {
            float angle = 2.0f * M_PI *
                          (pimd->rotation +
                           pimd->random_rotation * (psys_frand(psys, 19957323 + p) - 0.5f));
            const float eul[3] = {0.0f, 0.0f, angle};
            float rot[4];

            eul_to_quat(rot, eul);
            mul_qt_qtqt(frame, frame, rot);
          }

          /* NOTE: direction is same as normal vector currently,
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
          axis_angle_to_quat(state.rot, cross, safe_acosf(state.vel[axis]));
        }
#endif
      }
      else {
        state.time = -1.0;
        psys_get_particle_state(&sim, p, &state, true);
      }

      mul_qt_v3(state.rot, positions[vindex]);
      if (pimd->flag & eParticleInstanceFlag_UseSize) {
        mul_v3_fl(positions[vindex], size[p]);
      }
      add_v3_v3(positions[vindex], state.co);

      mul_m4_v3(spacemat, positions[vindex]);
    }

    /* Create edges and adjust edge vertex indices. */
    CustomData_copy_data(&mesh->edge_data, &result->edge_data, 0, p_skip * totedge, totedge);
    blender::int2 *edge = &edges[p_skip * totedge];
    for (k = 0; k < totedge; k++, edge++) {
      (*edge)[0] += p_skip * totvert;
      (*edge)[1] += p_skip * totvert;
    }

    /* create faces and loops */
    for (k = 0; k < faces_num; k++) {
      const blender::IndexRange in_face = orig_faces[k];

      CustomData_copy_data(&mesh->face_data, &result->face_data, k, p_skip * faces_num + k, 1);
      const int dst_face_start = in_face.start() + p_skip * totloop;
      face_offsets[p_skip * faces_num + k] = dst_face_start;

      {
        int orig_corner_i = in_face.start();
        int dst_corner_i = dst_face_start;
        int j = in_face.size();

        CustomData_copy_data(
            &mesh->corner_data, &result->corner_data, in_face.start(), dst_face_start, j);
        for (; j; j--, orig_corner_i++, dst_corner_i++) {
          corner_verts[dst_corner_i] = orig_corner_verts[orig_corner_i] + (p_skip * totvert);
          corner_edges[dst_corner_i] = orig_corner_edges[orig_corner_i] + (p_skip * totedge);
          const int vert = corner_verts[dst_corner_i];
          if (mloopcols_index) {
            const int part_index = vert_part_index[vert];
            store_float_in_vcol(&mloopcols_index.span[dst_corner_i],
                                float(part_index) / float(psys->totpart - 1));
          }
          if (mloopcols_value) {
            const float part_value = vert_part_value[vert];
            store_float_in_vcol(&mloopcols_value.span[dst_corner_i], part_value);
          }
        }
      }
    }
    p_skip++;
  }

  psys_sim_data_free(&sim);

  if (size) {
    MEM_freeN(size);
  }

  MEM_SAFE_FREE(vert_part_index);
  MEM_SAFE_FREE(vert_part_value);

  mloopcols_index.finish();
  mloopcols_value.finish();

  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA particle_obj_ptr = RNA_pointer_get(ptr, "object");

  layout->use_property_split_set(true);

  layout->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!RNA_pointer_is_null(&particle_obj_ptr)) {
    layout->prop_search(ptr,
                        "particle_system",
                        &particle_obj_ptr,
                        "particle_systems",
                        IFACE_("Particle System"),
                        ICON_NONE);
  }
  else {
    layout->prop(ptr, "particle_system_index", UI_ITEM_NONE, IFACE_("Particle System"), ICON_NONE);
  }

  layout->separator();

  row = &layout->row(true, IFACE_("Create Instances"));
  row->prop(ptr, "use_normal", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_children", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "use_size", toggles_flag, std::nullopt, ICON_NONE);

  row = &layout->row(true, IFACE_("Show"));
  row->prop(ptr, "show_alive", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "show_dead", toggles_flag, std::nullopt, ICON_NONE);
  row->prop(ptr, "show_unborn", toggles_flag, std::nullopt, ICON_NONE);

  layout->prop(ptr,
               "particle_amount",
               UI_ITEM_NONE,
               CTX_IFACE_(BLT_I18NCONTEXT_COUNTABLE, "Amount"),
               ICON_NONE);
  layout->prop(ptr, "particle_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);

  layout->separator();

  layout->prop(ptr, "space", UI_ITEM_NONE, IFACE_("Coordinate Space"), ICON_NONE);
  row = &layout->row(true);
  row->prop(ptr, "axis", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void path_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_path", UI_ITEM_NONE, IFACE_("Create Along Paths"), ICON_NONE);
}

static void path_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_boolean_get(ptr, "use_path"));

  col = &layout->column(true);
  col->prop(ptr, "position", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(ptr, "random_position", UI_ITEM_R_SLIDER, IFACE_("Random"), ICON_NONE);
  col = &layout->column(true);
  col->prop(ptr, "rotation", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(ptr, "random_rotation", UI_ITEM_R_SLIDER, IFACE_("Random"), ICON_NONE);

  layout->prop(ptr, "use_preserve_shape", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void layers_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop_search(
      ptr, "index_layer_name", &obj_data_ptr, "vertex_colors", IFACE_("Index"), ICON_NONE);
  col->prop_search(
      ptr, "value_layer_name", &obj_data_ptr, "vertex_colors", IFACE_("Value"), ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_ParticleInstance, panel_draw);
  modifier_subpanel_register(
      region_type, "paths", "", path_panel_draw_header, path_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "layers", "Layers", nullptr, layers_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_ParticleInstance = {
    /*idname*/ "ParticleInstance",
    /*name*/ N_("ParticleInstance"),
    /*struct_name*/ "ParticleInstanceModifierData",
    /*struct_size*/ sizeof(ParticleInstanceModifierData),
    /*srna*/ &RNA_ParticleInstanceModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_PARTICLE_INSTANCE,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
