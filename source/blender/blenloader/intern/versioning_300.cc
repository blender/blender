/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */
/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <algorithm>
#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base_safe.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_multi_value_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_light_types.h"
#include "DNA_lineart_types.h"
#include "DNA_listBase.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_tracking_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_asset.hh"
#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_curve.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_data_transfer.h"
#include "BKE_deform.hh"
#include "BKE_fcurve.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "BLO_readfile.hh"

#include "readfile.hh"

#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "versioning_common.hh"

static CLG_LogRef LOG = {"blend.doversion"};

static IDProperty *idproperty_find_ui_container(IDProperty *idprop_group)
{
  LISTBASE_FOREACH (IDProperty *, prop, &idprop_group->data.group) {
    if (prop->type == IDP_GROUP && STREQ(prop->name, "_RNA_UI")) {
      return prop;
    }
  }
  return nullptr;
}

static void version_idproperty_move_data_int(IDPropertyUIDataInt *ui_data,
                                             const IDProperty *prop_ui_data)
{
  IDProperty *min = IDP_GetPropertyFromGroup(prop_ui_data, "min");
  if (min != nullptr) {
    ui_data->min = ui_data->soft_min = IDP_coerce_to_int_or_zero(min);
  }
  IDProperty *max = IDP_GetPropertyFromGroup(prop_ui_data, "max");
  if (max != nullptr) {
    ui_data->max = ui_data->soft_max = IDP_coerce_to_int_or_zero(max);
  }
  IDProperty *soft_min = IDP_GetPropertyFromGroup(prop_ui_data, "soft_min");
  if (soft_min != nullptr) {
    ui_data->soft_min = IDP_coerce_to_int_or_zero(soft_min);
    ui_data->soft_min = std::min(ui_data->soft_min, ui_data->min);
  }
  IDProperty *soft_max = IDP_GetPropertyFromGroup(prop_ui_data, "soft_max");
  if (soft_max != nullptr) {
    ui_data->soft_max = IDP_coerce_to_int_or_zero(soft_max);
    ui_data->soft_max = std::max(ui_data->soft_max, ui_data->max);
  }
  IDProperty *step = IDP_GetPropertyFromGroup(prop_ui_data, "step");
  if (step != nullptr) {
    ui_data->step = IDP_coerce_to_int_or_zero(soft_max);
  }
  IDProperty *default_value = IDP_GetPropertyFromGroup(prop_ui_data, "default");
  if (default_value != nullptr) {
    if (default_value->type == IDP_ARRAY) {
      if (default_value->subtype == IDP_INT) {
        ui_data->default_array = MEM_malloc_arrayN<int>(size_t(default_value->len), __func__);
        memcpy(ui_data->default_array,
               IDP_array_int_get(default_value),
               sizeof(int) * default_value->len);
        ui_data->default_array_len = default_value->len;
      }
    }
    else if (default_value->type == IDP_INT) {
      ui_data->default_value = IDP_coerce_to_int_or_zero(default_value);
    }
  }
}

static void version_idproperty_move_data_float(IDPropertyUIDataFloat *ui_data,
                                               const IDProperty *prop_ui_data)
{
  IDProperty *min = IDP_GetPropertyFromGroup(prop_ui_data, "min");
  if (min != nullptr) {
    ui_data->min = ui_data->soft_min = IDP_coerce_to_double_or_zero(min);
  }
  IDProperty *max = IDP_GetPropertyFromGroup(prop_ui_data, "max");
  if (max != nullptr) {
    ui_data->max = ui_data->soft_max = IDP_coerce_to_double_or_zero(max);
  }
  IDProperty *soft_min = IDP_GetPropertyFromGroup(prop_ui_data, "soft_min");
  if (soft_min != nullptr) {
    ui_data->soft_min = IDP_coerce_to_double_or_zero(soft_min);
    ui_data->soft_min = std::max(ui_data->soft_min, ui_data->min);
  }
  IDProperty *soft_max = IDP_GetPropertyFromGroup(prop_ui_data, "soft_max");
  if (soft_max != nullptr) {
    ui_data->soft_max = IDP_coerce_to_double_or_zero(soft_max);
    ui_data->soft_max = std::min(ui_data->soft_max, ui_data->max);
  }
  IDProperty *step = IDP_GetPropertyFromGroup(prop_ui_data, "step");
  if (step != nullptr) {
    ui_data->step = IDP_coerce_to_float_or_zero(step);
  }
  IDProperty *precision = IDP_GetPropertyFromGroup(prop_ui_data, "precision");
  if (precision != nullptr) {
    ui_data->precision = IDP_coerce_to_int_or_zero(precision);
  }
  IDProperty *default_value = IDP_GetPropertyFromGroup(prop_ui_data, "default");
  if (default_value != nullptr) {
    if (default_value->type == IDP_ARRAY) {
      const int array_len = default_value->len;
      ui_data->default_array_len = array_len;
      if (default_value->subtype == IDP_FLOAT) {
        ui_data->default_array = MEM_malloc_arrayN<double>(size_t(array_len), __func__);
        const float *old_default_array = IDP_array_float_get(default_value);
        for (int i = 0; i < ui_data->default_array_len; i++) {
          ui_data->default_array[i] = double(old_default_array[i]);
        }
      }
      else if (default_value->subtype == IDP_DOUBLE) {
        ui_data->default_array = MEM_malloc_arrayN<double>(size_t(array_len), __func__);
        memcpy(ui_data->default_array,
               IDP_array_double_get(default_value),
               sizeof(double) * array_len);
      }
    }
    else if (ELEM(default_value->type, IDP_DOUBLE, IDP_FLOAT)) {
      ui_data->default_value = IDP_coerce_to_double_or_zero(default_value);
    }
  }
}

static void version_idproperty_move_data_string(IDPropertyUIDataString *ui_data,
                                                const IDProperty *prop_ui_data)
{
  IDProperty *default_value = IDP_GetPropertyFromGroup(prop_ui_data, "default");
  if (default_value != nullptr && default_value->type == IDP_STRING) {
    ui_data->default_value = BLI_strdup(IDP_string_get(default_value));
  }
}

static void version_idproperty_ui_data(IDProperty *idprop_group)
{
  /* `nullptr` check here to reduce verbosity of calls to this function. */
  if (idprop_group == nullptr) {
    return;
  }

  IDProperty *ui_container = idproperty_find_ui_container(idprop_group);
  if (ui_container == nullptr) {
    return;
  }

  LISTBASE_FOREACH (IDProperty *, prop, &idprop_group->data.group) {
    IDProperty *prop_ui_data = IDP_GetPropertyFromGroup(ui_container, prop->name);
    if (prop_ui_data == nullptr) {
      continue;
    }

    if (!IDP_ui_data_supported(prop)) {
      continue;
    }

    IDPropertyUIData *ui_data = IDP_ui_data_ensure(prop);

    IDProperty *subtype = IDP_GetPropertyFromGroup(prop_ui_data, "subtype");
    if (subtype != nullptr && subtype->type == IDP_STRING) {
      const char *subtype_string = IDP_string_get(subtype);
      int result = PROP_NONE;
      RNA_enum_value_from_id(rna_enum_property_subtype_items, subtype_string, &result);
      ui_data->rna_subtype = result;
    }

    IDProperty *description = IDP_GetPropertyFromGroup(prop_ui_data, "description");
    if (description != nullptr && description->type == IDP_STRING) {
      ui_data->description = BLI_strdup(IDP_string_get(description));
    }

    /* Type specific data. */
    switch (IDP_ui_data_type(prop)) {
      case IDP_UI_DATA_TYPE_STRING:
        version_idproperty_move_data_string((IDPropertyUIDataString *)ui_data, prop_ui_data);
        break;
      case IDP_UI_DATA_TYPE_ID:
        break;
      case IDP_UI_DATA_TYPE_INT:
        version_idproperty_move_data_int((IDPropertyUIDataInt *)ui_data, prop_ui_data);
        break;
      case IDP_UI_DATA_TYPE_FLOAT:
        version_idproperty_move_data_float((IDPropertyUIDataFloat *)ui_data, prop_ui_data);
        break;
      case IDP_UI_DATA_TYPE_BOOLEAN:
      case IDP_UI_DATA_TYPE_UNSUPPORTED:
        BLI_assert_unreachable();
        break;
    }

    IDP_FreeFromGroup(ui_container, prop_ui_data);
  }

  IDP_FreeFromGroup(idprop_group, ui_container);
}

static void do_versions_idproperty_bones_recursive(Bone *bone)
{
  version_idproperty_ui_data(bone->prop);
  LISTBASE_FOREACH (Bone *, child_bone, &bone->childbase) {
    do_versions_idproperty_bones_recursive(child_bone);
  }
}

static void do_versions_idproperty_seq_recursive(ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    version_idproperty_ui_data(strip->prop);
    if (strip->type == STRIP_TYPE_META) {
      do_versions_idproperty_seq_recursive(&strip->seqbase);
    }
  }
}

/**
 * For every data block that supports them, initialize the new IDProperty UI data struct based on
 * the old more complicated storage. Assumes only the top level of IDProperties below the parent
 * group had UI data in a "_RNA_UI" group.
 *
 * \note The following IDProperty groups in DNA aren't exposed in the UI or are runtime-only, so
 * they don't have UI data: wmOperator, bAddon, bUserMenuItem_Op, wmKeyMapItem, wmKeyConfigPref,
 * uiList, FFMpegCodecData, View3DShading, bToolRef, TimeMarker, ViewLayer, bPoseChannel.
 */
static void do_versions_idproperty_ui_data(Main *bmain)
{
  /* ID data. */
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    IDProperty *idprop_group = IDP_GetProperties(id);
    version_idproperty_ui_data(idprop_group);
  }
  FOREACH_MAIN_ID_END;

  /* Bones. */
  LISTBASE_FOREACH (bArmature *, armature, &bmain->armatures) {
    LISTBASE_FOREACH (Bone *, bone, &armature->bonebase) {
      do_versions_idproperty_bones_recursive(bone);
    }
  }

  /* Nodes and node sockets. */
  LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      version_idproperty_ui_data(node->prop);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->inputs_legacy) {
      version_idproperty_ui_data(socket->prop);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->outputs_legacy) {
      version_idproperty_ui_data(socket->prop);
    }
  }

  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    /* The UI data from exposed node modifier properties is just copied from the corresponding node
     * group, but the copying only runs when necessary, so we still need to version data here. */
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Nodes) {
        NodesModifierData *nmd = (NodesModifierData *)md;
        version_idproperty_ui_data(nmd->settings.properties);
      }
    }

    /* Object post bones. */
    if (ob->type == OB_ARMATURE && ob->pose != nullptr) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        version_idproperty_ui_data(pchan->prop);
      }
    }
  }

  /* Sequences. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      do_versions_idproperty_seq_recursive(&scene->ed->seqbase);
    }
  }
}

static void sort_linked_ids(Main *bmain)
{
  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ListBase temp_list;
    BLI_listbase_clear(&temp_list);
    LISTBASE_FOREACH_MUTABLE (ID *, id, lb) {
      if (ID_IS_LINKED(id)) {
        BLI_remlink(lb, id);
        BLI_addtail(&temp_list, id);
        id_sort_by_name(&temp_list, id, nullptr);
      }
    }
    BLI_movelisttolist(lb, &temp_list);
  }
  FOREACH_MAIN_LISTBASE_END;
}

static void assert_sorted_ids(Main *bmain)
{
#ifndef NDEBUG
  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ID *id_prev = nullptr;
    LISTBASE_FOREACH (ID *, id, lb) {
      if (id_prev == nullptr) {
        continue;
      }
      BLI_assert(id_prev->lib != id->lib || BLI_strcasecmp(id_prev->name, id->name) < 0);
    }
  }
  FOREACH_MAIN_LISTBASE_END;
#else
  UNUSED_VARS_NDEBUG(bmain);
#endif
}

static void move_vertex_group_names_to_object_data(Main *bmain)
{
  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    if (ELEM(object->type, OB_MESH, OB_LATTICE, OB_GPENCIL_LEGACY)) {
      ListBase *new_defbase = BKE_object_defgroup_list_mutable(object);

      /* Choose the longest vertex group name list among all linked duplicates. */
      if (BLI_listbase_count(&object->defbase) < BLI_listbase_count(new_defbase)) {
        BLI_freelistN(&object->defbase);
      }
      else {
        /* Clear the list in case the it was already assigned from another object. */
        BLI_freelistN(new_defbase);
        *new_defbase = object->defbase;
        BKE_object_defgroup_active_index_set(object, object->actdef);
      }
    }
  }
}

static void do_versions_sequencer_speed_effect_recursive(Scene *scene, const ListBase *seqbase)
{
  /* Old SpeedControlVars->flags. */
#define STRIP_SPEED_INTEGRATE (1 << 0)
#define STRIP_SPEED_COMPRESS_IPO_Y (1 << 2)

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->type == STRIP_TYPE_SPEED) {
      SpeedControlVars *v = (SpeedControlVars *)strip->effectdata;
      const char *substr = nullptr;
      float globalSpeed_legacy = v->globalSpeed_legacy;
      if (strip->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
        if (globalSpeed_legacy == 1.0f) {
          v->speed_control_type = SEQ_SPEED_STRETCH;
        }
        else {
          v->speed_control_type = SEQ_SPEED_MULTIPLY;
          v->speed_fader = globalSpeed_legacy *
                           (float(strip->input1->len) /
                            max_ff(float(blender::seq::time_right_handle_frame_get(scene,
                                                                                   strip->input1) -
                                         strip->input1->start),
                                   1.0f));
        }
      }
      else if (v->flags & STRIP_SPEED_INTEGRATE) {
        v->speed_control_type = SEQ_SPEED_MULTIPLY;
        v->speed_fader = strip->speed_fader_legacy * globalSpeed_legacy;
      }
      else if (v->flags & STRIP_SPEED_COMPRESS_IPO_Y) {
        globalSpeed_legacy *= 100.0f;
        v->speed_control_type = SEQ_SPEED_LENGTH;
        v->speed_fader_length = strip->speed_fader_legacy * globalSpeed_legacy;
        substr = "speed_length";
      }
      else {
        v->speed_control_type = SEQ_SPEED_FRAME_NUMBER;
        v->speed_fader_frame_number = int(strip->speed_fader_legacy * globalSpeed_legacy);
        substr = "speed_frame_number";
      }

      v->flags &= ~(STRIP_SPEED_INTEGRATE | STRIP_SPEED_COMPRESS_IPO_Y);

      if (substr || globalSpeed_legacy != 1.0f) {
        FCurve *fcu = id_data_find_fcurve(
            &scene->id, strip, &RNA_Strip, "speed_factor", 0, nullptr);
        if (fcu) {
          if (globalSpeed_legacy != 1.0f) {
            for (int i = 0; i < fcu->totvert; i++) {
              BezTriple *bezt = &fcu->bezt[i];
              bezt->vec[0][1] *= globalSpeed_legacy;
              bezt->vec[1][1] *= globalSpeed_legacy;
              bezt->vec[2][1] *= globalSpeed_legacy;
            }
          }
          if (substr) {
            char *new_path = BLI_string_replaceN(fcu->rna_path, "speed_factor", substr);
            MEM_freeN(fcu->rna_path);
            fcu->rna_path = new_path;
          }
        }
      }
    }
    else if (strip->type == STRIP_TYPE_META) {
      do_versions_sequencer_speed_effect_recursive(scene, &strip->seqbase);
    }
  }

#undef STRIP_SPEED_INTEGRATE
#undef STRIP_SPEED_COMPRESS_IPO_Y
}

static bool do_versions_sequencer_color_tags(Strip *strip, void * /*user_data*/)
{
  strip->color_tag = STRIP_COLOR_NONE;
  return true;
}

static bool do_versions_sequencer_color_balance_sop(Strip *strip, void * /*user_data*/)
{
  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    if (smd->type == eSeqModifierType_ColorBalance) {
      StripColorBalance *cb = &((ColorBalanceModifierData *)smd)->color_balance;
      cb->method = SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN;
      for (int i = 0; i < 3; i++) {
        copy_v3_fl(cb->slope, 1.0f);
        copy_v3_fl(cb->offset, 1.0f);
        copy_v3_fl(cb->power, 1.0f);
      }
    }
  }
  return true;
}

/**
 * If a node used to realize instances implicitly and will no longer do so in 3.0, add a "Realize
 * Instances" node in front of it to avoid changing behavior. Don't do this if the node will be
 * replaced anyway though.
 */
static void version_geometry_nodes_add_realize_instance_nodes(bNodeTree *ntree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (ELEM(node->type_legacy,
             GEO_NODE_CAPTURE_ATTRIBUTE,
             GEO_NODE_SEPARATE_COMPONENTS,
             GEO_NODE_CONVEX_HULL,
             GEO_NODE_CURVE_LENGTH,
             GEO_NODE_MESH_BOOLEAN,
             GEO_NODE_FILLET_CURVE,
             GEO_NODE_RESAMPLE_CURVE,
             GEO_NODE_CURVE_TO_MESH,
             GEO_NODE_TRIM_CURVE,
             GEO_NODE_REPLACE_MATERIAL,
             GEO_NODE_SUBDIVIDE_MESH,
             GEO_NODE_TRIANGULATE))
    {
      bNodeSocket *geometry_socket = static_cast<bNodeSocket *>(node->inputs.first);
      add_realize_instances_before_socket(ntree, node, geometry_socket);
    }
    /* Also realize instances for the profile input of the curve to mesh node. */
    if (node->type_legacy == GEO_NODE_CURVE_TO_MESH) {
      bNodeSocket *profile_socket = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
      add_realize_instances_before_socket(ntree, node, profile_socket);
    }
  }
}

/**
 * The geometry nodes modifier used to realize instances for the next modifier implicitly. Now it
 * is done with the realize instances node. It also used to convert meshes to point clouds
 * automatically, which is also now done with a specific node.
 */
static bNodeTree *add_realize_node_tree(Main *bmain)
{
  bNodeTree *node_tree = blender::bke::node_tree_add_tree(
      bmain, "Realize Instances 2.93 Legacy", "GeometryNodeTree");

  node_tree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);
  node_tree->tree_interface.add_socket(
      "Geometry", "", "NodeSocketGeometry", NODE_INTERFACE_SOCKET_INPUT, nullptr);

  bNode *group_input = blender::bke::node_add_static_node(nullptr, *node_tree, NODE_GROUP_INPUT);
  group_input->locx_legacy = -400.0f;
  bNode *group_output = blender::bke::node_add_static_node(nullptr, *node_tree, NODE_GROUP_OUTPUT);
  group_output->locx_legacy = 500.0f;
  group_output->flag |= NODE_DO_OUTPUT;

  bNode *join = blender::bke::node_add_static_node(nullptr, *node_tree, GEO_NODE_JOIN_GEOMETRY);
  join->locx_legacy = group_output->locx_legacy - 175.0f;
  join->locy_legacy = group_output->locy_legacy;
  bNode *conv = blender::bke::node_add_static_node(
      nullptr, *node_tree, GEO_NODE_POINTS_TO_VERTICES);
  conv->locx_legacy = join->locx_legacy - 175.0f;
  conv->locy_legacy = join->locy_legacy - 70.0;
  bNode *separate = blender::bke::node_add_static_node(
      nullptr, *node_tree, GEO_NODE_SEPARATE_COMPONENTS);
  separate->locx_legacy = join->locx_legacy - 350.0f;
  separate->locy_legacy = join->locy_legacy + 50.0f;
  bNode *realize = blender::bke::node_add_static_node(
      nullptr, *node_tree, GEO_NODE_REALIZE_INSTANCES);
  realize->locx_legacy = separate->locx_legacy - 200.0f;
  realize->locy_legacy = join->locy_legacy;

  blender::bke::node_add_link(*node_tree,
                              *group_input,
                              *static_cast<bNodeSocket *>(group_input->outputs.first),
                              *realize,
                              *static_cast<bNodeSocket *>(realize->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *realize,
                              *static_cast<bNodeSocket *>(realize->outputs.first),
                              *separate,
                              *static_cast<bNodeSocket *>(separate->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *conv,
                              *static_cast<bNodeSocket *>(conv->outputs.first),
                              *join,
                              *static_cast<bNodeSocket *>(join->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *separate,
                              *static_cast<bNodeSocket *>(BLI_findlink(&separate->outputs, 3)),
                              *join,
                              *static_cast<bNodeSocket *>(join->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *separate,
                              *static_cast<bNodeSocket *>(BLI_findlink(&separate->outputs, 1)),
                              *conv,
                              *static_cast<bNodeSocket *>(conv->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *separate,
                              *static_cast<bNodeSocket *>(BLI_findlink(&separate->outputs, 2)),
                              *join,
                              *static_cast<bNodeSocket *>(join->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *separate,
                              *static_cast<bNodeSocket *>(separate->outputs.first),
                              *join,
                              *static_cast<bNodeSocket *>(join->inputs.first));
  blender::bke::node_add_link(*node_tree,
                              *join,
                              *static_cast<bNodeSocket *>(join->outputs.first),
                              *group_output,
                              *static_cast<bNodeSocket *>(group_output->inputs.first));

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    blender::bke::node_set_selected(*node, false);
  }

  version_socket_update_is_used(node_tree);
  return node_tree;
}

static void strip_speed_factor_fix_rna_path(Strip *strip, ListBase *fcurves)
{
  char name_esc[(sizeof(strip->name) - 2) * 2];
  BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));
  char *path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].pitch", name_esc);
  FCurve *fcu = BKE_fcurve_find(fcurves, path, 0);
  if (fcu != nullptr) {
    MEM_freeN(fcu->rna_path);
    fcu->rna_path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].speed_factor", name_esc);
  }
  MEM_freeN(path);
}

static bool version_fix_seq_meta_range(Strip *strip, void *user_data)
{
  Scene *scene = (Scene *)user_data;
  if (strip->type == STRIP_TYPE_META) {
    blender::seq::time_update_meta_strip_range(scene, strip);
  }
  return true;
}

static bool strip_speed_factor_set(Strip *strip, void *user_data)
{
  const Scene *scene = static_cast<const Scene *>(user_data);
  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    /* Move `pitch` animation to `speed_factor` */
    if (scene->adt && scene->adt->action) {
      strip_speed_factor_fix_rna_path(strip, &scene->adt->action->curves);
    }
    if (scene->adt && !BLI_listbase_is_empty(&scene->adt->drivers)) {
      strip_speed_factor_fix_rna_path(strip, &scene->adt->drivers);
    }

    /* Pitch value of 0 has been found in some files. This would cause problems. */
    if (strip->pitch_legacy <= 0.0f) {
      strip->pitch_legacy = 1.0f;
    }

    strip->speed_factor = strip->pitch_legacy;
  }
  else {
    strip->speed_factor = 1.0f;
  }
  return true;
}

static void version_geometry_nodes_replace_transfer_attribute_node(bNodeTree *ntree)
{
  using namespace blender;
  using namespace blender::bke;
  /* Otherwise `ntree->typeInfo` is null. */
  blender::bke::node_tree_set_type(*ntree);
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != GEO_NODE_TRANSFER_ATTRIBUTE_DEPRECATED) {
      continue;
    }
    bNodeSocket *old_geometry_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Source");
    const NodeGeometryTransferAttribute *storage = (const NodeGeometryTransferAttribute *)
                                                       node->storage;
    switch (storage->mode) {
      case GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST_FACE_INTERPOLATED: {
        bNode *sample_nearest_surface = blender::bke::node_add_static_node(
            nullptr, *ntree, GEO_NODE_SAMPLE_NEAREST_SURFACE);
        sample_nearest_surface->parent = node->parent;
        sample_nearest_surface->custom1 = storage->data_type;
        sample_nearest_surface->locx_legacy = node->locx_legacy;
        sample_nearest_surface->locy_legacy = node->locy_legacy;
        static auto socket_remap = []() {
          Map<std::string, std::string> map;
          map.add_new("Attribute", "Value");
          map.add_new("Attribute_001", "Value");
          map.add_new("Attribute_002", "Value");
          map.add_new("Attribute_003", "Value");
          map.add_new("Attribute_004", "Value");
          map.add_new("Source", "Mesh");
          map.add_new("Source Position", "Sample Position");
          return map;
        }();
        node_tree_relink_with_socket_id_map(*ntree, *node, *sample_nearest_surface, socket_remap);
        break;
      }
      case GEO_NODE_ATTRIBUTE_TRANSFER_NEAREST: {
        /* These domains weren't supported by the index transfer mode, but were selectable. */
        const AttrDomain domain = ELEM(AttrDomain(storage->domain),
                                       AttrDomain::Instance,
                                       AttrDomain::Curve) ?
                                      AttrDomain::Point :
                                      AttrDomain(storage->domain);

        /* Use a sample index node to retrieve the data with this node's index output. */
        bNode *sample_index = blender::bke::node_add_static_node(
            nullptr, *ntree, GEO_NODE_SAMPLE_INDEX);
        NodeGeometrySampleIndex *sample_storage = static_cast<NodeGeometrySampleIndex *>(
            sample_index->storage);
        sample_storage->data_type = storage->data_type;
        sample_storage->domain = int8_t(domain);
        sample_index->parent = node->parent;
        sample_index->locx_legacy = node->locx_legacy + 25.0f;
        sample_index->locy_legacy = node->locy_legacy;
        if (old_geometry_socket->link) {
          blender::bke::node_add_link(
              *ntree,
              *old_geometry_socket->link->fromnode,
              *old_geometry_socket->link->fromsock,
              *sample_index,
              *blender::bke::node_find_socket(*sample_index, SOCK_IN, "Geometry"));
        }

        bNode *sample_nearest = blender::bke::node_add_static_node(
            nullptr, *ntree, GEO_NODE_SAMPLE_NEAREST);
        sample_nearest->parent = node->parent;
        sample_nearest->custom1 = storage->data_type;
        sample_nearest->custom2 = int8_t(domain);
        sample_nearest->locx_legacy = node->locx_legacy - 25.0f;
        sample_nearest->locy_legacy = node->locy_legacy;
        if (old_geometry_socket->link) {
          blender::bke::node_add_link(
              *ntree,
              *old_geometry_socket->link->fromnode,
              *old_geometry_socket->link->fromsock,
              *sample_nearest,
              *blender::bke::node_find_socket(*sample_nearest, SOCK_IN, "Geometry"));
        }
        static auto sample_nearest_remap = []() {
          Map<std::string, std::string> map;
          map.add_new("Source Position", "Sample Position");
          return map;
        }();
        node_tree_relink_with_socket_id_map(*ntree, *node, *sample_nearest, sample_nearest_remap);

        static auto sample_index_remap = []() {
          Map<std::string, std::string> map;
          map.add_new("Attribute", "Value");
          map.add_new("Attribute_001", "Value");
          map.add_new("Attribute_002", "Value");
          map.add_new("Attribute_003", "Value");
          map.add_new("Attribute_004", "Value");
          map.add_new("Source Position", "Sample Position");
          return map;
        }();
        node_tree_relink_with_socket_id_map(*ntree, *node, *sample_index, sample_index_remap);

        blender::bke::node_add_link(
            *ntree,
            *sample_nearest,
            *blender::bke::node_find_socket(*sample_nearest, SOCK_OUT, "Index"),
            *sample_index,
            *blender::bke::node_find_socket(*sample_index, SOCK_IN, "Index"));
        break;
      }
      case GEO_NODE_ATTRIBUTE_TRANSFER_INDEX: {
        bNode *sample_index = blender::bke::node_add_static_node(
            nullptr, *ntree, GEO_NODE_SAMPLE_INDEX);
        NodeGeometrySampleIndex *sample_storage = static_cast<NodeGeometrySampleIndex *>(
            sample_index->storage);
        sample_storage->data_type = storage->data_type;
        sample_storage->domain = storage->domain;
        sample_storage->clamp = 1;
        sample_index->parent = node->parent;
        sample_index->locx_legacy = node->locx_legacy;
        sample_index->locy_legacy = node->locy_legacy;
        const bool index_was_linked =
            blender::bke::node_find_socket(*node, SOCK_IN, "Index")->link != nullptr;
        static auto socket_remap = []() {
          Map<std::string, std::string> map;
          map.add_new("Attribute", "Value");
          map.add_new("Attribute_001", "Value");
          map.add_new("Attribute_002", "Value");
          map.add_new("Attribute_003", "Value");
          map.add_new("Attribute_004", "Value");
          map.add_new("Source", "Geometry");
          map.add_new("Index", "Index");
          return map;
        }();
        node_tree_relink_with_socket_id_map(*ntree, *node, *sample_index, socket_remap);

        if (!index_was_linked) {
          /* Add an index input node, since the new node doesn't use an implicit input. */
          bNode *index = blender::bke::node_add_static_node(nullptr, *ntree, GEO_NODE_INPUT_INDEX);
          index->parent = node->parent;
          index->locx_legacy = node->locx_legacy - 25.0f;
          index->locy_legacy = node->locy_legacy - 25.0f;
          blender::bke::node_add_link(
              *ntree,
              *index,
              *blender::bke::node_find_socket(*index, SOCK_OUT, "Index"),
              *sample_index,
              *blender::bke::node_find_socket(*sample_index, SOCK_IN, "Index"));
        }
        break;
      }
    }
    /* The storage must be freed manually because the node type isn't defined anymore. */
    MEM_freeN(node->storage);
    blender::bke::node_remove_node(nullptr, *ntree, *node, false);
  }
}

/**
 * The mesh primitive nodes created a uv map with a hardcoded name. Now they are outputting the uv
 * map as a socket instead. The versioning just inserts a Store Named Attribute node after
 * primitive nodes.
 */
static void version_geometry_nodes_primitive_uv_maps(bNodeTree &ntree)
{
  blender::Vector<bNode *> new_nodes;
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree.nodes) {
    if (!ELEM(node->type_legacy,
              GEO_NODE_MESH_PRIMITIVE_CONE,
              GEO_NODE_MESH_PRIMITIVE_CUBE,
              GEO_NODE_MESH_PRIMITIVE_CYLINDER,
              GEO_NODE_MESH_PRIMITIVE_GRID,
              GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE,
              GEO_NODE_MESH_PRIMITIVE_UV_SPHERE))
    {
      continue;
    }
    bNodeSocket *primitive_output_socket = nullptr;
    bNodeSocket *uv_map_output_socket = nullptr;
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      if (STREQ(socket->name, "UV Map")) {
        uv_map_output_socket = socket;
      }
      if (socket->type == SOCK_GEOMETRY) {
        primitive_output_socket = socket;
      }
    }
    if (uv_map_output_socket != nullptr) {
      continue;
    }
    uv_map_output_socket = &version_node_add_socket(
        ntree, *node, SOCK_OUT, "NodeSocketVector", "UV Map");

    bNode *store_attribute_node = &version_node_add_empty(ntree,
                                                          "GeometryNodeStoreNamedAttribute");
    new_nodes.append(store_attribute_node);
    store_attribute_node->parent = node->parent;
    store_attribute_node->locx_legacy = node->locx_legacy + 25;
    store_attribute_node->locy_legacy = node->locy_legacy;
    auto &storage = *MEM_callocN<NodeGeometryStoreNamedAttribute>(__func__);
    store_attribute_node->storage = &storage;
    storage.domain = int8_t(blender::bke::AttrDomain::Corner);
    /* Intentionally use 3D instead of 2D vectors, because 2D vectors did not exist in older
     * releases and would make the file crash when trying to open it. */
    storage.data_type = CD_PROP_FLOAT3;

    bNodeSocket &store_attribute_geometry_input = version_node_add_socket(
        ntree, *store_attribute_node, SOCK_IN, "NodeSocketGeometry", "Geometry");
    bNodeSocket &store_attribute_name_input = version_node_add_socket(
        ntree, *store_attribute_node, SOCK_IN, "NodeSocketString", "Name");
    bNodeSocket &store_attribute_value_input = version_node_add_socket(
        ntree, *store_attribute_node, SOCK_IN, "NodeSocketVector", "Value");
    bNodeSocket &store_attribute_geometry_output = version_node_add_socket(
        ntree, *store_attribute_node, SOCK_OUT, "NodeSocketGeometry", "Geometry");
    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      if (link->fromsock == primitive_output_socket) {
        link->fromnode = store_attribute_node;
        link->fromsock = &store_attribute_geometry_output;
      }
    }

    bNodeSocketValueString *name_value = static_cast<bNodeSocketValueString *>(
        store_attribute_name_input.default_value);
    const char *uv_map_name = node->type_legacy == GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE ? "UVMap" :
                                                                                        "uv_map";
    STRNCPY_UTF8(name_value->value, uv_map_name);

    version_node_add_link(ntree,
                          *node,
                          *primitive_output_socket,
                          *store_attribute_node,
                          store_attribute_geometry_input);
    version_node_add_link(
        ntree, *node, *uv_map_output_socket, *store_attribute_node, store_attribute_value_input);
  }

  /* Move nodes to the front so that they are drawn behind existing nodes. */
  for (bNode *node : new_nodes) {
    BLI_remlink(&ntree.nodes, node);
    BLI_addhead(&ntree.nodes, node);
  }
  if (!new_nodes.is_empty()) {
    blender::bke::node_rebuild_id_vector(ntree);
  }
}

/**
 * When extruding from loose edges, the extrude geometry node used to create flat faces due to the
 * default of the old "shade_smooth" attribute. Since the "false" value has changed with the
 * "sharp_face" attribute, add nodes to propagate the new attribute in its inverted "smooth" form.
 */
static void version_geometry_nodes_extrude_smooth_propagation(bNodeTree &ntree)
{
  using namespace blender;
  Vector<bNode *> new_nodes;
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree.nodes) {
    if (node->idname != StringRef("GeometryNodeExtrudeMesh")) {
      continue;
    }
    if (static_cast<const NodeGeometryExtrudeMesh *>(node->storage)->mode !=
        GEO_NODE_EXTRUDE_MESH_EDGES)
    {
      continue;
    }
    bNodeSocket *geometry_in_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Mesh");
    bNodeSocket *geometry_out_socket = blender::bke::node_find_socket(*node, SOCK_OUT, "Mesh");

    Map<bNodeSocket *, bNodeLink *> in_links_per_socket;
    MultiValueMap<bNodeSocket *, bNodeLink *> out_links_per_socket;
    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      in_links_per_socket.add(link->tosock, link);
      out_links_per_socket.add(link->fromsock, link);
    }

    bNodeLink *geometry_in_link = in_links_per_socket.lookup_default(geometry_in_socket, nullptr);
    Span<bNodeLink *> geometry_out_links = out_links_per_socket.lookup(geometry_out_socket);
    if (!geometry_in_link || geometry_out_links.is_empty()) {
      continue;
    }

    const bool versioning_already_done = [&]() {
      if (geometry_in_link->fromnode->idname != StringRef("GeometryNodeCaptureAttribute")) {
        return false;
      }
      bNode *capture_node = geometry_in_link->fromnode;
      const NodeGeometryAttributeCapture &capture_storage =
          *static_cast<const NodeGeometryAttributeCapture *>(capture_node->storage);
      if (capture_storage.data_type_legacy != CD_PROP_BOOL ||
          bke::AttrDomain(capture_storage.domain) != bke::AttrDomain::Face)
      {
        return false;
      }
      bNodeSocket *capture_in_socket = blender::bke::node_find_socket(
          *capture_node, SOCK_IN, "Value_003");
      bNodeLink *capture_in_link = in_links_per_socket.lookup_default(capture_in_socket, nullptr);
      if (!capture_in_link) {
        return false;
      }
      if (capture_in_link->fromnode->idname != StringRef("GeometryNodeInputShadeSmooth")) {
        return false;
      }
      if (geometry_out_links.size() != 1) {
        return false;
      }
      bNodeLink *geometry_out_link = geometry_out_links.first();
      if (geometry_out_link->tonode->idname != StringRef("GeometryNodeSetShadeSmooth")) {
        return false;
      }
      bNode *set_smooth_node = geometry_out_link->tonode;
      bNodeSocket *smooth_in_socket = blender::bke::node_find_socket(
          *set_smooth_node, SOCK_IN, "Shade Smooth");
      bNodeLink *connecting_link = in_links_per_socket.lookup_default(smooth_in_socket, nullptr);
      if (!connecting_link) {
        return false;
      }
      if (connecting_link->fromnode != capture_node) {
        return false;
      }
      return true;
    }();
    if (versioning_already_done) {
      continue;
    }

    bNode &capture_node = version_node_add_empty(ntree, "GeometryNodeCaptureAttribute");
    capture_node.parent = node->parent;
    capture_node.locx_legacy = node->locx_legacy - 25;
    capture_node.locy_legacy = node->locy_legacy;
    new_nodes.append(&capture_node);
    auto *capture_node_storage = MEM_callocN<NodeGeometryAttributeCapture>(__func__);
    capture_node.storage = capture_node_storage;
    capture_node_storage->data_type_legacy = CD_PROP_BOOL;
    capture_node_storage->domain = int8_t(bke::AttrDomain::Face);
    bNodeSocket &capture_node_geo_in = version_node_add_socket(
        ntree, capture_node, SOCK_IN, "NodeSocketGeometry", "Geometry");
    bNodeSocket &capture_node_geo_out = version_node_add_socket(
        ntree, capture_node, SOCK_OUT, "NodeSocketGeometry", "Geometry");
    bNodeSocket &capture_node_value_in = version_node_add_socket(
        ntree, capture_node, SOCK_IN, "NodeSocketBool", "Value_003");
    bNodeSocket &capture_node_attribute_out = version_node_add_socket(
        ntree, capture_node, SOCK_OUT, "NodeSocketBool", "Attribute_003");

    bNode &is_smooth_node = version_node_add_empty(ntree, "GeometryNodeInputShadeSmooth");
    is_smooth_node.parent = node->parent;
    is_smooth_node.locx_legacy = capture_node.locx_legacy - 25;
    is_smooth_node.locy_legacy = capture_node.locy_legacy;
    bNodeSocket &is_smooth_out = version_node_add_socket(
        ntree, is_smooth_node, SOCK_OUT, "NodeSocketBool", "Smooth");
    new_nodes.append(&is_smooth_node);
    version_node_add_link(
        ntree, is_smooth_node, is_smooth_out, capture_node, capture_node_value_in);
    version_node_add_link(ntree, capture_node, capture_node_geo_out, *node, *geometry_in_socket);
    geometry_in_link->tonode = &capture_node;
    geometry_in_link->tosock = &capture_node_geo_in;

    bNode &set_smooth_node = version_node_add_empty(ntree, "GeometryNodeSetShadeSmooth");
    set_smooth_node.custom1 = int16_t(blender::bke::AttrDomain::Face);
    set_smooth_node.parent = node->parent;
    set_smooth_node.locx_legacy = node->locx_legacy + 25;
    set_smooth_node.locy_legacy = node->locy_legacy;
    new_nodes.append(&set_smooth_node);
    bNodeSocket &set_smooth_node_geo_in = version_node_add_socket(
        ntree, set_smooth_node, SOCK_IN, "NodeSocketGeometry", "Geometry");
    bNodeSocket &set_smooth_node_geo_out = version_node_add_socket(
        ntree, set_smooth_node, SOCK_OUT, "NodeSocketGeometry", "Geometry");
    bNodeSocket &set_smooth_node_smooth_in = version_node_add_socket(
        ntree, set_smooth_node, SOCK_IN, "NodeSocketBool", "Shade Smooth");

    version_node_add_link(
        ntree, *node, *geometry_out_socket, set_smooth_node, set_smooth_node_geo_in);

    for (bNodeLink *link : geometry_out_links) {
      link->fromnode = &set_smooth_node;
      link->fromsock = &set_smooth_node_geo_out;
    }
    version_node_add_link(ntree,
                          capture_node,
                          capture_node_attribute_out,
                          set_smooth_node,
                          set_smooth_node_smooth_in);
  }

  /* Move nodes to the front so that they are drawn behind existing nodes. */
  for (bNode *node : new_nodes) {
    BLI_remlink(&ntree.nodes, node);
    BLI_addhead(&ntree.nodes, node);
  }
  if (!new_nodes.is_empty()) {
    blender::bke::node_rebuild_id_vector(ntree);
  }
}

/* Change the action strip (if a NLA strip is preset) to HOLD instead of HOLD FORWARD to maintain
 * backwards compatibility. */
static void version_nla_action_strip_hold(Main *bmain)
{
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    AnimData *adt = BKE_animdata_from_id(id);
    /* We only want to preserve existing behavior if there's an action and 1 or more NLA strips. */
    if (adt == nullptr || adt->action == nullptr ||
        adt->act_extendmode != NLASTRIP_EXTEND_HOLD_FORWARD)
    {
      continue;
    }

    if (BKE_nlatrack_has_strips(&adt->nla_tracks)) {
      adt->act_extendmode = NLASTRIP_EXTEND_HOLD;
    }
  }
  FOREACH_MAIN_ID_END;
}

void do_versions_after_linking_300(FileData * /*fd*/, Main *bmain)
{
  if (MAIN_VERSION_FILE_ATLEAST(bmain, 300, 0) && !MAIN_VERSION_FILE_ATLEAST(bmain, 300, 1)) {
    /* Set zero user text objects to have a fake user. */
    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      if (text->id.us == 0) {
        id_fake_user_set(&text->id);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 3)) {
    sort_linked_ids(bmain);
    assert_sorted_ids(bmain);
  }

  if (MAIN_VERSION_FILE_ATLEAST(bmain, 300, 3)) {
    assert_sorted_ids(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 11)) {
    move_vertex_group_names_to_object_data(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 13)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        do_versions_sequencer_speed_effect_recursive(scene, &scene->ed->seqbase);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 25)) {
    version_node_socket_index_animdata(bmain, NTREE_SHADER, SH_NODE_BSDF_PRINCIPLED, 4, 2, 25);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 26)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      ImagePaintSettings *imapaint = &tool_settings->imapaint;
      if (imapaint->canvas != nullptr &&
          ELEM(imapaint->canvas->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE))
      {
        imapaint->canvas = nullptr;
      }
      if (imapaint->stencil != nullptr &&
          ELEM(imapaint->stencil->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE))
      {
        imapaint->stencil = nullptr;
      }
      if (imapaint->clone != nullptr &&
          ELEM(imapaint->clone->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE))
      {
        imapaint->clone = nullptr;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 28)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_add_realize_instance_nodes(ntree);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 30)) {
    do_versions_idproperty_ui_data(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 32)) {
    /* Update Switch Node Non-Fields switch input to Switch_001. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }

      LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
        if (link->tonode->type_legacy == GEO_NODE_SWITCH) {
          if (STREQ(link->tosock->identifier, "Switch")) {
            bNode *to_node = link->tonode;

            uint8_t mode = ((NodeSwitch *)to_node->storage)->input_type;
            if (ELEM(mode,
                     SOCK_GEOMETRY,
                     SOCK_OBJECT,
                     SOCK_COLLECTION,
                     SOCK_TEXTURE,
                     SOCK_MATERIAL))
            {
              link->tosock = link->tosock->next;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 33)) {
    /* This was missing from #move_vertex_group_names_to_object_data. */
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      if (ELEM(object->type, OB_MESH, OB_LATTICE, OB_GPENCIL_LEGACY)) {
        /* This uses the fact that the active vertex group index starts counting at 1. */
        if (BKE_object_defgroup_active_index_get(object) == 0) {
          BKE_object_defgroup_active_index_set(object, object->actdef);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 35)) {
    /* Add a new modifier to realize instances from previous modifiers.
     * Previously that was done automatically by geometry nodes. */
    bNodeTree *realize_instances_node_tree = nullptr;
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH_MUTABLE (ModifierData *, md, &ob->modifiers) {
        if (md->type != eModifierType_Nodes) {
          continue;
        }
        if (md->next == nullptr) {
          break;
        }
        if (md->next->type == eModifierType_Nodes) {
          continue;
        }
        NodesModifierData *nmd = (NodesModifierData *)md;
        if (nmd->node_group == nullptr) {
          continue;
        }

        NodesModifierData *new_nmd = (NodesModifierData *)BKE_modifier_new(eModifierType_Nodes);
        STRNCPY_UTF8(new_nmd->modifier.name, "Realize Instances 2.93 Legacy");
        BKE_modifier_unique_name(&ob->modifiers, &new_nmd->modifier);
        BLI_insertlinkafter(&ob->modifiers, md, new_nmd);
        if (realize_instances_node_tree == nullptr) {
          realize_instances_node_tree = add_realize_node_tree(bmain);
        }
        new_nmd->node_group = realize_instances_node_tree;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 37)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == GEO_NODE_BOUNDING_BOX) {
            bNodeSocket *geometry_socket = static_cast<bNodeSocket *>(node->inputs.first);
            add_realize_instances_before_socket(ntree, node, geometry_socket);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 301, 6)) {
    { /* Ensure driver variable names are unique within the driver. */
      ID *id;
      FOREACH_MAIN_ID_BEGIN (bmain, id) {
        AnimData *adt = BKE_animdata_from_id(id);
        if (adt == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
          ChannelDriver *driver = fcu->driver;
          /* Ensure the uniqueness front to back. Given a list of identically
           * named variables, the last one gets to keep its original name. This
           * matches the evaluation order, and thus shouldn't change the evaluated
           * value of the driver expression. */
          LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
            BLI_uniquename(&driver->variables,
                           dvar,
                           dvar->name,
                           '_',
                           offsetof(DriverVar, name),
                           sizeof(dvar->name));
          }
        }
      }
      FOREACH_MAIN_ID_END;
    }

    /* Ensure tiled image sources contain a UDIM token. */
    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      if (ima->source == IMA_SRC_TILED) {
        BKE_image_ensure_tile_token(ima->filepath, sizeof(ima->filepath));
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 14)) {
    /* Sequencer channels region. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_SEQ) {
            continue;
          }
          SpaceSeq *sseq = (SpaceSeq *)sl;
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          sseq->flag |= SEQ_CLAMP_VIEW;

          if (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
            continue;
          }

          ARegion *timeline_region = BKE_region_find_in_listbase_by_type(regionbase,
                                                                         RGN_TYPE_WINDOW);

          if (timeline_region == nullptr) {
            continue;
          }

          timeline_region->v2d.cur.ymax = 8.5f;
          timeline_region->v2d.align &= ~V2D_ALIGN_NO_NEG_Y;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 5)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed == nullptr) {
        continue;
      }
      blender::seq::foreach_strip(&ed->seqbase, strip_speed_factor_set, scene);
      blender::seq::foreach_strip(&ed->seqbase, version_fix_seq_meta_range, scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 6)) {
    /* In the Dope Sheet, for every mode other than Timeline, open the Properties panel. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_ACTION) {
            continue;
          }

          /* Skip the timeline, it shouldn't get its Properties panel opened. */
          SpaceAction *saction = (SpaceAction *)sl;
          if (saction->mode == SACTCONT_TIMELINE) {
            continue;
          }

          const bool is_first_space = sl == area->spacedata.first;
          ListBase *regionbase = is_first_space ? &area->regionbase : &sl->regionbase;
          ARegion *region = BKE_region_find_in_listbase_by_type(regionbase, RGN_TYPE_UI);
          if (region == nullptr) {
            continue;
          }

          region->flag &= ~RGN_FLAG_HIDDEN;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 1)) {
    /* Split the transfer attribute node into multiple smaller nodes. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_replace_transfer_attribute_node(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 13)) {
    version_nla_action_strip_hold(bmain);
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

static void version_switch_node_input_prefix(Main *bmain)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    if (ntree->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == GEO_NODE_SWITCH) {
          LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
            /* Skip the "switch" socket. */
            if (socket == node->inputs.first) {
              continue;
            }
            STRNCPY_UTF8(socket->name, socket->name[0] == 'A' ? "False" : "True");

            /* Replace "A" and "B", but keep the unique number suffix at the end. */
            char number_suffix[8];
            STRNCPY_UTF8(number_suffix, socket->identifier + 1);
            BLI_string_join(
                socket->identifier, sizeof(socket->identifier), socket->name, number_suffix);
          }
        }
      }
    }
  }
  FOREACH_NODETREE_END;
}

static bool replace_bbone_len_scale_rnapath(char **p_old_path, int *p_index)
{
  char *old_path = *p_old_path;

  if (old_path == nullptr) {
    return false;
  }

  int len = strlen(old_path);

  if (BLI_str_endswith(old_path, ".bbone_curveiny") ||
      BLI_str_endswith(old_path, ".bbone_curveouty"))
  {
    old_path[len - 1] = 'z';
    return true;
  }

  if (BLI_str_endswith(old_path, ".bbone_scaleinx") ||
      BLI_str_endswith(old_path, ".bbone_scaleiny") ||
      BLI_str_endswith(old_path, ".bbone_scaleoutx") ||
      BLI_str_endswith(old_path, ".bbone_scaleouty"))
  {
    int index = (old_path[len - 1] == 'y' ? 2 : 0);

    old_path[len - 1] = 0;

    if (p_index) {
      *p_index = index;
    }
    else {
      *p_old_path = BLI_sprintfN("%s[%d]", old_path, index);
      MEM_freeN(old_path);
    }

    return true;
  }

  return false;
}

static void do_version_bbone_len_scale_fcurve_fix(FCurve *fcu)
{
  /* Update driver variable paths. */
  if (fcu->driver) {
    LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
      DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
        replace_bbone_len_scale_rnapath(&dtar->rna_path, nullptr);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  /* Update F-Curve's path. */
  replace_bbone_len_scale_rnapath(&fcu->rna_path, &fcu->array_index);
}

static void do_version_bones_bbone_len_scale(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    if (bone->flag & BONE_ADD_PARENT_END_ROLL) {
      bone->bbone_flag |= BBONE_ADD_PARENT_END_ROLL;
    }

    copy_v3_fl3(bone->scale_in, bone->scale_in_x, 1.0f, bone->scale_in_z);
    copy_v3_fl3(bone->scale_out, bone->scale_out_x, 1.0f, bone->scale_out_z);

    do_version_bones_bbone_len_scale(&bone->childbase);
  }
}

static void do_version_constraints_spline_ik_joint_bindings(ListBase *lb)
{
  /* Binding array data could be freed without properly resetting its size data. */
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
      bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
      if (data->points == nullptr) {
        data->numpoints = 0;
      }
    }
  }
}

static bNodeSocket *do_version_replace_float_size_with_vector(bNodeTree *ntree,
                                                              bNode *node,
                                                              bNodeSocket *socket)
{
  const bNodeSocketValueFloat *socket_value = (const bNodeSocketValueFloat *)socket->default_value;
  const float old_value = socket_value->value;
  blender::bke::node_remove_socket(*ntree, *node, *socket);
  bNodeSocket *new_socket = blender::bke::node_add_socket(
      *ntree,
      *node,
      SOCK_IN,
      *blender::bke::node_static_socket_type(SOCK_VECTOR, PROP_TRANSLATION),
      "Size",
      "Size");
  bNodeSocketValueVector *value_vector = (bNodeSocketValueVector *)new_socket->default_value;
  copy_v3_fl(value_vector->value, old_value);
  return new_socket;
}

static bool strip_transform_origin_set(Strip *strip, void * /*user_data*/)
{
  StripTransform *transform = strip->data->transform;
  if (strip->data->transform != nullptr) {
    transform->origin[0] = transform->origin[1] = 0.5f;
  }
  return true;
}

static bool strip_transform_filter_set(Strip *strip, void * /*user_data*/)
{
  StripTransform *transform = strip->data->transform;
  if (strip->data->transform != nullptr) {
    transform->filter = SEQ_TRANSFORM_FILTER_BILINEAR;
  }
  return true;
}

static bool strip_meta_channels_ensure(Strip *strip, void * /*user_data*/)
{
  if (strip->type == STRIP_TYPE_META) {
    blender::seq::channels_ensure(&strip->channels);
  }
  return true;
}

static void do_version_subsurface_methods(bNode *node)
{
  if (node->type_legacy == SH_NODE_SUBSURFACE_SCATTERING) {
    if (!ELEM(node->custom1, SHD_SUBSURFACE_BURLEY, SHD_SUBSURFACE_RANDOM_WALK_SKIN)) {
      node->custom1 = SHD_SUBSURFACE_RANDOM_WALK;
    }
  }
  else if (node->type_legacy == SH_NODE_BSDF_PRINCIPLED) {
    if (!ELEM(node->custom2, SHD_SUBSURFACE_BURLEY, SHD_SUBSURFACE_RANDOM_WALK_SKIN)) {
      node->custom2 = SHD_SUBSURFACE_RANDOM_WALK;
    }
  }
}

static void version_geometry_nodes_add_attribute_input_settings(NodesModifierData *nmd)
{
  using namespace blender;
  if (nmd->settings.properties == nullptr) {
    return;
  }
  /* Before versioning the properties, make sure it hasn't been done already. */
  LISTBASE_FOREACH (const IDProperty *, property, &nmd->settings.properties->data.group) {
    if (strstr(property->name, "_use_attribute") || strstr(property->name, "_attribute_name")) {
      return;
    }
  }

  LISTBASE_FOREACH_MUTABLE (IDProperty *, property, &nmd->settings.properties->data.group) {
    if (!ELEM(property->type, IDP_FLOAT, IDP_INT, IDP_ARRAY)) {
      continue;
    }

    if (strstr(property->name, "_use_attribute") || strstr(property->name, "_attribute_name")) {
      continue;
    }

    char use_attribute_prop_name[MAX_IDPROP_NAME];
    SNPRINTF(use_attribute_prop_name, "%s%s", property->name, "_use_attribute");

    IDProperty *use_attribute_prop = bke::idprop::create(use_attribute_prop_name, 0).release();
    IDP_AddToGroup(nmd->settings.properties, use_attribute_prop);

    char attribute_name_prop_name[MAX_IDPROP_NAME];
    SNPRINTF(attribute_name_prop_name, "%s%s", property->name, "_attribute_name");

    IDProperty *attribute_prop = bke::idprop::create(attribute_name_prop_name, "").release();
    IDP_AddToGroup(nmd->settings.properties, attribute_prop);
  }
}

/* Copy of the function before the fixes. */
static void legacy_vec_roll_to_mat3_normalized(const float nor[3],
                                               const float roll,
                                               float r_mat[3][3])
{
  const float SAFE_THRESHOLD = 1.0e-5f;     /* theta above this value has good enough precision. */
  const float CRITICAL_THRESHOLD = 1.0e-9f; /* above this is safe under certain conditions. */
  const float THRESHOLD_SQUARED = CRITICAL_THRESHOLD * CRITICAL_THRESHOLD;

  const float x = nor[0];
  const float y = nor[1];
  const float z = nor[2];

  const float theta = 1.0f + y;          /* remapping Y from [-1,+1] to [0,2]. */
  const float theta_alt = x * x + z * z; /* Helper value for matrix calculations. */
  float rMatrix[3][3], bMatrix[3][3];

  BLI_ASSERT_UNIT_V3(nor);

  /* When theta is close to zero (nor is aligned close to negative Y Axis),
   * we have to check we do have non-null X/Z components as well.
   * Also, due to float precision errors, nor can be (0.0, -0.99999994, 0.0) which results
   * in theta being close to zero. This will cause problems when theta is used as divisor.
   */
  if (theta > SAFE_THRESHOLD || (theta > CRITICAL_THRESHOLD && theta_alt > THRESHOLD_SQUARED)) {
    /* nor is *not* aligned to negative Y-axis (0,-1,0). */

    bMatrix[0][1] = -x;
    bMatrix[1][0] = x;
    bMatrix[1][1] = y;
    bMatrix[1][2] = z;
    bMatrix[2][1] = -z;

    if (theta > SAFE_THRESHOLD) {
      /* nor differs significantly from negative Y axis (0,-1,0): apply the general case. */
      bMatrix[0][0] = 1 - x * x / theta;
      bMatrix[2][2] = 1 - z * z / theta;
      bMatrix[2][0] = bMatrix[0][2] = -x * z / theta;
    }
    else {
      /* nor is close to negative Y axis (0,-1,0): apply the special case. */
      bMatrix[0][0] = (x + z) * (x - z) / -theta_alt;
      bMatrix[2][2] = -bMatrix[0][0];
      bMatrix[2][0] = bMatrix[0][2] = 2.0f * x * z / theta_alt;
    }
  }
  else {
    /* nor is very close to negative Y axis (0,-1,0): use simple symmetry by Z axis. */
    unit_m3(bMatrix);
    bMatrix[0][0] = bMatrix[1][1] = -1.0;
  }

  /* Make Roll matrix */
  axis_angle_normalized_to_mat3(rMatrix, nor, roll);

  /* Combine and output result */
  mul_m3_m3m3(r_mat, rMatrix, bMatrix);
}

static void correct_bone_roll_value(const float head[3],
                                    const float tail[3],
                                    const float check_x_axis[3],
                                    const float check_y_axis[3],
                                    float *r_roll)
{
  const float SAFE_THRESHOLD = 1.0e-5f;
  float vec[3], bone_mat[3][3], vec2[3];

  /* Compute the Y axis vector. */
  sub_v3_v3v3(vec, tail, head);
  normalize_v3(vec);

  /* Only correct when in the danger zone. */
  if (1.0f + vec[1] < SAFE_THRESHOLD * 2 && (vec[0] || vec[2])) {
    /* Use the armature matrix to double-check if adjustment is needed.
     * This should minimize issues if the file is bounced back and forth between
     * 2.92 and 2.91, provided Edit Mode isn't entered on the armature in 2.91. */
    vec_roll_to_mat3(vec, *r_roll, bone_mat);

    UNUSED_VARS_NDEBUG(check_y_axis);
    BLI_assert(dot_v3v3(bone_mat[1], check_y_axis) > 0.999f);

    if (dot_v3v3(bone_mat[0], check_x_axis) < 0.999f) {
      /* Recompute roll using legacy code to interpret the old value. */
      legacy_vec_roll_to_mat3_normalized(vec, *r_roll, bone_mat);
      mat3_to_vec_roll(bone_mat, vec2, r_roll);
      BLI_assert(compare_v3v3(vec, vec2, 0.001f));
    }
  }
}

/* Update the armature Bone roll fields for bones very close to -Y direction. */
static void do_version_bones_roll(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    /* Parent-relative orientation (used for posing). */
    correct_bone_roll_value(
        bone->head, bone->tail, bone->bone_mat[0], bone->bone_mat[1], &bone->roll);

    /* Absolute orientation (used for Edit mode). */
    correct_bone_roll_value(
        bone->arm_head, bone->arm_tail, bone->arm_mat[0], bone->arm_mat[1], &bone->arm_roll);

    do_version_bones_roll(&bone->childbase);
  }
}

static void version_geometry_nodes_set_position_node_offset(bNodeTree *ntree)
{
  /* Add the new Offset socket. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != GEO_NODE_SET_POSITION) {
      continue;
    }
    if (BLI_listbase_count(&node->inputs) < 4) {
      /* The offset socket didn't exist in the file yet. */
      return;
    }
    bNodeSocket *old_offset_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 3));
    if (old_offset_socket->type == SOCK_VECTOR) {
      /* Versioning happened already. */
      return;
    }
    /* Change identifier of old socket, so that there is no name collision. */
    STRNCPY_UTF8(old_offset_socket->identifier, "Offset_old");
    blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_VECTOR, PROP_TRANSLATION, "Offset", "Offset");
  }

  /* Relink links that were connected to Position while Offset was enabled. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tonode->type_legacy != GEO_NODE_SET_POSITION) {
      continue;
    }
    if (!STREQ(link->tosock->identifier, "Position")) {
      continue;
    }
    bNodeSocket *old_offset_socket = static_cast<bNodeSocket *>(
        BLI_findlink(&link->tonode->inputs, 3));
    /* This assumes that the offset is not linked to something else. That seems to be a reasonable
     * assumption, because the node is probably only ever used in one or the other mode. */
    const bool offset_enabled =
        ((bNodeSocketValueBoolean *)old_offset_socket->default_value)->value;
    if (offset_enabled) {
      /* Relink to new offset socket. */
      link->tosock = old_offset_socket->next;
    }
  }

  /* Remove old Offset socket. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy != GEO_NODE_SET_POSITION) {
      continue;
    }
    bNodeSocket *old_offset_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 3));
    blender::bke::node_remove_socket(*ntree, *node, *old_offset_socket);
  }
}

static void version_node_tree_socket_id_delim(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      version_node_socket_id_delim(socket);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      version_node_socket_id_delim(socket);
    }
  }
}

static bool version_merge_still_offsets(Strip *strip, void * /*user_data*/)
{
  strip->startofs -= strip->startstill_legacy;
  strip->endofs -= strip->endstill_legacy;
  strip->startstill_legacy = 0;
  strip->endstill_legacy = 0;
  return true;
}

static bool version_set_seq_single_frame_content(Strip *strip, void * /*user_data*/)
{
  if ((strip->len == 1) &&
      (strip->type == STRIP_TYPE_IMAGE ||
       (strip->is_effect() && blender::seq::effect_get_num_inputs(strip->type) == 0)))
  {
    strip->flag |= SEQ_SINGLE_FRAME_CONTENT;
  }
  return true;
}

static bool version_seq_fix_broken_sound_strips(Strip *strip, void * /*user_data*/)
{
  if (strip->type != STRIP_TYPE_SOUND_RAM || strip->speed_factor != 0.0f) {
    return true;
  }

  strip->speed_factor = 1.0f;
  blender::seq::retiming_data_clear(strip);

  /* Broken files do have negative start offset, which should not be present in sound strips. */
  if (strip->startofs < 0) {
    strip->startofs = 0.0f;
  }

  return true;
}

/* Those `version_liboverride_rnacollections_*` functions mimic the old, pre-3.0 code to find
 * anchor and source items in the given list of modifiers, constraints etc., using only the
 * `subitem_local` data of the override property operation.
 *
 * Then they convert it into the new, proper `subitem_reference` data for the anchor, and
 * `subitem_local` for the source.
 *
 * NOTE: Here only the stored override ID is available, unlike in the `override_apply` functions.
 */

static void version_liboverride_rnacollections_insertion_object_constraints(
    ListBase *constraints, IDOverrideLibraryProperty *op)
{
  LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    if (opop->operation != LIBOVERRIDE_OP_INSERT_AFTER) {
      continue;
    }
    bConstraint *constraint_anchor = static_cast<bConstraint *>(
        BLI_listbase_string_or_index_find(constraints,
                                          opop->subitem_local_name,
                                          offsetof(bConstraint, name),
                                          opop->subitem_local_index));
    bConstraint *constraint_src = constraint_anchor != nullptr ?
                                      constraint_anchor->next :
                                      static_cast<bConstraint *>(constraints->first);

    if (constraint_src == nullptr) {
      /* Invalid case, just remove that override property operation. */
      CLOG_ERROR(&LOG, "Could not find source constraint in stored override data");
      BKE_lib_override_library_property_operation_delete(op, opop);
      continue;
    }

    opop->subitem_reference_name = opop->subitem_local_name;
    opop->subitem_local_name = BLI_strdup(constraint_src->name);
    opop->subitem_reference_index = opop->subitem_local_index;
    opop->subitem_local_index++;
  }
}

static void version_liboverride_rnacollections_insertion_object(Object *object)
{
  IDOverrideLibrary *liboverride = object->id.override_library;
  IDOverrideLibraryProperty *op;

  op = BKE_lib_override_library_property_find(liboverride, "modifiers");
  if (op != nullptr) {
    LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != LIBOVERRIDE_OP_INSERT_AFTER) {
        continue;
      }
      ModifierData *mod_anchor = static_cast<ModifierData *>(
          BLI_listbase_string_or_index_find(&object->modifiers,
                                            opop->subitem_local_name,
                                            offsetof(ModifierData, name),
                                            opop->subitem_local_index));
      ModifierData *mod_src = mod_anchor != nullptr ?
                                  mod_anchor->next :
                                  static_cast<ModifierData *>(object->modifiers.first);

      if (mod_src == nullptr) {
        /* Invalid case, just remove that override property operation. */
        CLOG_ERROR(&LOG, "Could not find source modifier in stored override data");
        BKE_lib_override_library_property_operation_delete(op, opop);
        continue;
      }

      opop->subitem_reference_name = opop->subitem_local_name;
      opop->subitem_local_name = BLI_strdup(mod_src->name);
      opop->subitem_reference_index = opop->subitem_local_index;
      opop->subitem_local_index++;
    }
  }

  op = BKE_lib_override_library_property_find(liboverride, "grease_pencil_modifiers");
  if (op != nullptr) {
    LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != LIBOVERRIDE_OP_INSERT_AFTER) {
        continue;
      }
      GpencilModifierData *gp_mod_anchor = static_cast<GpencilModifierData *>(
          BLI_listbase_string_or_index_find(&object->greasepencil_modifiers,
                                            opop->subitem_local_name,
                                            offsetof(GpencilModifierData, name),
                                            opop->subitem_local_index));
      GpencilModifierData *gp_mod_src = gp_mod_anchor != nullptr ?
                                            gp_mod_anchor->next :
                                            static_cast<GpencilModifierData *>(
                                                object->greasepencil_modifiers.first);

      if (gp_mod_src == nullptr) {
        /* Invalid case, just remove that override property operation. */
        CLOG_ERROR(&LOG, "Could not find source GP modifier in stored override data");
        BKE_lib_override_library_property_operation_delete(op, opop);
        continue;
      }

      opop->subitem_reference_name = opop->subitem_local_name;
      opop->subitem_local_name = BLI_strdup(gp_mod_src->name);
      opop->subitem_reference_index = opop->subitem_local_index;
      opop->subitem_local_index++;
    }
  }

  op = BKE_lib_override_library_property_find(liboverride, "constraints");
  if (op != nullptr) {
    version_liboverride_rnacollections_insertion_object_constraints(&object->constraints, op);
  }

  if (object->pose != nullptr) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      char rna_path[26 + (sizeof(pchan->name) * 2) + 1];
      char name_esc[sizeof(pchan->name) * 2];
      BLI_str_escape(name_esc, pchan->name, sizeof(name_esc));
      SNPRINTF_UTF8(rna_path, "pose.bones[\"%s\"].constraints", name_esc);
      op = BKE_lib_override_library_property_find(liboverride, rna_path);
      if (op != nullptr) {
        version_liboverride_rnacollections_insertion_object_constraints(&pchan->constraints, op);
      }
    }
  }
}

static void version_liboverride_rnacollections_insertion_animdata(ID *id)
{
  AnimData *anim_data = BKE_animdata_from_id(id);
  if (anim_data == nullptr) {
    return;
  }

  IDOverrideLibrary *liboverride = id->override_library;
  IDOverrideLibraryProperty *op;

  op = BKE_lib_override_library_property_find(liboverride, "animation_data.nla_tracks");
  if (op != nullptr) {
    LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != LIBOVERRIDE_OP_INSERT_AFTER) {
        continue;
      }
      /* NLA tracks are only referenced by index, which limits possibilities, basically they are
       * always added at the end of the list, see #rna_NLA_tracks_override_apply.
       *
       * This makes things simple here. */
      opop->subitem_reference_name = opop->subitem_local_name;
      opop->subitem_local_name = nullptr;
      opop->subitem_reference_index = opop->subitem_local_index;
      opop->subitem_local_index++;
    }
  }
}

static void versioning_replace_legacy_mix_rgb_node(bNodeTree *ntree)
{
  version_node_input_socket_name(ntree, SH_NODE_MIX_RGB_LEGACY, "Fac", "Factor_Float");
  version_node_input_socket_name(ntree, SH_NODE_MIX_RGB_LEGACY, "Color1", "A_Color");
  version_node_input_socket_name(ntree, SH_NODE_MIX_RGB_LEGACY, "Color2", "B_Color");
  version_node_output_socket_name(ntree, SH_NODE_MIX_RGB_LEGACY, "Color", "Result_Color");
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_MIX_RGB_LEGACY) {
      STRNCPY_UTF8(node->idname, "ShaderNodeMix");
      node->type_legacy = SH_NODE_MIX;
      NodeShaderMix *data = MEM_callocN<NodeShaderMix>(__func__);
      data->blend_type = node->custom1;
      data->clamp_result = (node->custom2 & SHD_MIXRGB_CLAMP) ? 1 : 0;
      data->clamp_factor = 1;
      data->data_type = SOCK_RGBA;
      data->factor_mode = NODE_MIX_MODE_UNIFORM;
      node->storage = data;
    }
  }
}

static void version_fix_image_format_copy(Main *bmain, ImageFormatData *format)
{
  /* Fix bug where curves in image format were not properly copied to file output
   * node, incorrectly sharing a pointer with the scene settings. Copy the data
   * structure now as it should have been done in the first place. */
  if (format->view_settings.curve_mapping) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (format != &scene->r.im_format && ELEM(format->view_settings.curve_mapping,
                                                scene->view_settings.curve_mapping,
                                                scene->r.im_format.view_settings.curve_mapping))
      {
        format->view_settings.curve_mapping = BKE_curvemapping_copy(
            format->view_settings.curve_mapping);
        break;
      }
    }

    /* Remove any invalid curves with missing data. */
    if (format->view_settings.curve_mapping->cm[0].curve == nullptr) {
      BKE_curvemapping_free(format->view_settings.curve_mapping);
      format->view_settings.curve_mapping = nullptr;
      format->view_settings.flag &= ~COLORMANAGE_VIEW_USE_CURVES;
    }
  }
}

/**
 * Some editors would manually manage visibility of regions, or lazy create them based on
 * context. Ensure they are always there now, and use the new #ARegionType.poll().
 */
static void version_ensure_missing_regions(ScrArea *area, SpaceLink *sl)
{
  ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;

  switch (sl->spacetype) {
    case SPACE_FILE: {
      if (ARegion *ui_region = do_versions_add_region_if_not_found(
              regionbase, RGN_TYPE_UI, "versioning: UI region for file", RGN_TYPE_TOOLS))
      {
        ui_region->alignment = RGN_ALIGN_TOP;
        ui_region->flag |= RGN_FLAG_DYNAMIC_SIZE;
      }

      if (ARegion *exec_region = do_versions_add_region_if_not_found(
              regionbase, RGN_TYPE_EXECUTE, "versioning: execute region for file", RGN_TYPE_UI))
      {
        exec_region->alignment = RGN_ALIGN_BOTTOM;
        exec_region->flag = RGN_FLAG_DYNAMIC_SIZE;
      }

      if (ARegion *tool_props_region = do_versions_add_region_if_not_found(
              regionbase,
              RGN_TYPE_TOOL_PROPS,
              "versioning: tool props region for file",
              RGN_TYPE_EXECUTE))
      {
        tool_props_region->alignment = RGN_ALIGN_RIGHT;
        tool_props_region->flag = RGN_FLAG_HIDDEN;
      }
      break;
    }
    case SPACE_CLIP: {
      ARegion *region;

      region = do_versions_ensure_region(
          regionbase, RGN_TYPE_UI, "versioning: properties region for clip", RGN_TYPE_HEADER);
      region->alignment = RGN_ALIGN_RIGHT;
      region->flag &= ~RGN_FLAG_HIDDEN;

      region = do_versions_ensure_region(
          regionbase, RGN_TYPE_CHANNELS, "versioning: channels region for clip", RGN_TYPE_UI);
      region->alignment = RGN_ALIGN_LEFT;
      region->flag &= ~RGN_FLAG_HIDDEN;
      region->v2d.scroll = V2D_SCROLL_BOTTOM;
      region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

      region = do_versions_ensure_region(
          regionbase, RGN_TYPE_PREVIEW, "versioning: preview region for clip", RGN_TYPE_WINDOW);
      region->flag &= ~RGN_FLAG_HIDDEN;

      break;
    }
    case SPACE_SEQ: {
      ARegion *region;

      do_versions_ensure_region(regionbase,
                                RGN_TYPE_CHANNELS,
                                "versioning: channels region for sequencer",
                                RGN_TYPE_TOOLS);

      region = do_versions_ensure_region(regionbase,
                                         RGN_TYPE_PREVIEW,
                                         "versioning: preview region for sequencer",
                                         RGN_TYPE_CHANNELS);
      sequencer_init_preview_region(region);

      break;
    }
  }
}

/**
 * Change override RNA path from `frame_{start,end}` to `frame_{start,end}_raw`.
 * See #102662.
 */
static void version_liboverride_nla_strip_frame_start_end(IDOverrideLibrary *liboverride,
                                                          const char *parent_rna_path,
                                                          NlaStrip *strip)
{
  if (!strip) {
    return;
  }

  /* Escape the strip name for inclusion in the RNA path. */
  char name_esc_strip[sizeof(strip->name) * 2];
  BLI_str_escape(name_esc_strip, strip->name, sizeof(name_esc_strip));

  const std::string rna_path_strip = std::string(parent_rna_path) + ".strips[\"" + name_esc_strip +
                                     "\"]";

  { /* Rename .frame_start -> .frame_start_raw: */
    const std::string rna_path_prop = rna_path_strip + ".frame_start";
    BKE_lib_override_library_property_rna_path_change(
        liboverride, rna_path_prop.c_str(), (rna_path_prop + "_raw").c_str());
  }

  { /* Rename .frame_end -> .frame_end_raw: */
    const std::string rna_path_prop = rna_path_strip + ".frame_end";
    BKE_lib_override_library_property_rna_path_change(
        liboverride, rna_path_prop.c_str(), (rna_path_prop + "_raw").c_str());
  }

  { /* Remove .frame_start_ui: */
    const std::string rna_path_prop = rna_path_strip + ".frame_start_ui";
    BKE_lib_override_library_property_search_and_delete(liboverride, rna_path_prop.c_str());
  }

  { /* Remove .frame_end_ui: */
    const std::string rna_path_prop = rna_path_strip + ".frame_end_ui";
    BKE_lib_override_library_property_search_and_delete(liboverride, rna_path_prop.c_str());
  }

  /* Handle meta-strip contents. */
  LISTBASE_FOREACH (NlaStrip *, substrip, &strip->strips) {
    version_liboverride_nla_strip_frame_start_end(liboverride, rna_path_strip.c_str(), substrip);
  }
}

/** Fix the `frame_start` and `frame_end` overrides on NLA strips. See #102662. */
static void version_liboverride_nla_frame_start_end(ID *id, AnimData *adt)
{
  IDOverrideLibrary *liboverride = id->override_library;
  if (!liboverride) {
    return;
  }

  int track_index;
  LISTBASE_FOREACH_INDEX (NlaTrack *, track, &adt->nla_tracks, track_index) {
    char *rna_path_track = BLI_sprintfN("animation_data.nla_tracks[%d]", track_index);

    LISTBASE_FOREACH (NlaStrip *, strip, &track->strips) {
      version_liboverride_nla_strip_frame_start_end(liboverride, rna_path_track, strip);
    }

    MEM_freeN(rna_path_track);
  }
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_300(FileData *fd, Library * /*lib*/, Main *bmain)
{
  /* The #SCE_SNAP_SEQ flag has been removed in favor of the #SCE_SNAP which can be used for each
   * snap_flag member individually. */
  enum { SCE_SNAP_SEQ = (1 << 7) };

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 1)) {
    /* Set default value for the new bisect_threshold parameter in the mirror modifier. */
    if (!DNA_struct_member_exists(fd->filesdna, "MirrorModifierData", "float", "bisect_threshold"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Mirror) {
            MirrorModifierData *mmd = (MirrorModifierData *)md;
            /* This was the previous hard-coded value. */
            mmd->bisect_threshold = 0.001f;
          }
        }
      }
    }
    /* Grease Pencil: Set default value for dilate pixels. */
    if (!DNA_struct_member_exists(fd->filesdna, "BrushGpencilSettings", "int", "dilate_pixels")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->gpencil_settings) {
          brush->gpencil_settings->dilate_pixels = 1;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 2)) {
    version_switch_node_input_prefix(bmain);

    if (!DNA_struct_member_exists(fd->filesdna, "bPoseChannel", "float", "custom_scale_xyz[3]")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          copy_v3_fl(pchan->custom_scale_xyz, pchan->custom_scale);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 4)) {
    /* Add a properties sidebar to the spreadsheet editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SPREADSHEET) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *new_sidebar = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_UI, "sidebar for spreadsheet", RGN_TYPE_FOOTER);
            if (new_sidebar != nullptr) {
              new_sidebar->alignment = RGN_ALIGN_RIGHT;
              new_sidebar->flag |= RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }

    /* Enable spreadsheet filtering in old files without row filters. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SPREADSHEET) {
            SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;
            sspreadsheet->filter_flag |= SPREADSHEET_FILTER_ENABLE;
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_BOUNDING_BOX, "Mesh", "Bounding Box");
      }
    }
    FOREACH_NODETREE_END;

    if (!DNA_struct_member_exists(fd->filesdna, "FileAssetSelectParams", "short", "import_method"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_FILE) {
              SpaceFile *sfile = (SpaceFile *)sl;
              if (sfile->asset_params) {
                sfile->asset_params->import_method = FILE_ASSET_IMPORT_APPEND;
              }
            }
          }
        }
      }
    }

    /* Initialize length-wise scale B-Bone settings. */
    if (!DNA_struct_member_exists(fd->filesdna, "Bone", "int", "bbone_flag")) {
      /* Update armature data and pose channels. */
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_bbone_len_scale(&arm->bonebase);
      }

      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            copy_v3_fl3(pchan->scale_in, pchan->scale_in_x, 1.0f, pchan->scale_in_z);
            copy_v3_fl3(pchan->scale_out, pchan->scale_out_x, 1.0f, pchan->scale_out_z);
          }
        }
      }

      /* Update action curves and drivers. */
      LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
        LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &act->curves) {
          do_version_bbone_len_scale_fcurve_fix(fcu);
        }
      }

      BKE_animdata_main_cb(bmain, [](ID * /*id*/, AnimData *adt) {
        LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &adt->drivers) {
          do_version_bbone_len_scale_fcurve_fix(fcu);
        }
      });
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 5)) {
    /* Add a dataset sidebar to the spreadsheet editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SPREADSHEET) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *spreadsheet_dataset_region = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_CHANNELS, "spreadsheet dataset region", RGN_TYPE_FOOTER);

            if (spreadsheet_dataset_region) {
              spreadsheet_dataset_region->alignment = RGN_ALIGN_LEFT;
              spreadsheet_dataset_region->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 6)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          /* Disable View Layers filter. */
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;
            space_outliner->filter |= SO_FILTER_NO_VIEW_LAYERS;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag |= SCE_SNAP_SEQ;
      short snap_mode = tool_settings->snap_mode;
      short snap_node_mode = tool_settings->snap_node_mode;
      short snap_uv_mode = tool_settings->snap_uv_mode;
      tool_settings->snap_mode &= ~((1 << 4) | (1 << 5) | (1 << 6));
      tool_settings->snap_node_mode &= ~((1 << 5) | (1 << 6));
      tool_settings->snap_uv_mode &= ~(1 << 4);
      if (snap_mode & (1 << 4)) {
        tool_settings->snap_mode |= (1 << 6); /* SCE_SNAP_TO_INCREMENT */
      }
      if (snap_mode & (1 << 5)) {
        tool_settings->snap_mode |= (1 << 4); /* SCE_SNAP_TO_EDGE_MIDPOINT */
      }
      if (snap_mode & (1 << 6)) {
        tool_settings->snap_mode |= (1 << 5); /* SCE_SNAP_TO_EDGE_PERPENDICULAR */
      }
      if (snap_node_mode & (1 << 5)) {
        tool_settings->snap_node_mode |= (1 << 0); /* SCE_SNAP_TO_NODE_X */
      }
      if (snap_node_mode & (1 << 6)) {
        tool_settings->snap_node_mode |= (1 << 1); /* SCE_SNAP_TO_NODE_Y */
      }
      if (snap_uv_mode & (1 << 4)) {
        tool_settings->snap_uv_mode |= (1 << 6); /* SCE_SNAP_TO_INCREMENT */
      }

      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode = SEQ_SNAP_TO_STRIPS | SEQ_SNAP_TO_CURRENT_FRAME |
                                           SEQ_SNAP_TO_STRIP_HOLD;
      sequencer_tool_settings->snap_distance = 15;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 8)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->master_collection != nullptr) {
        BLI_strncpy_utf8(scene->master_collection->id.name + 2,
                         BKE_SCENE_COLLECTION_NAME,
                         sizeof(scene->master_collection->id.name) - 2);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 9)) {
    /* Fix a bug where reordering FCurves and bActionGroups could cause some corruption. Just
     * reconstruct all the action groups & ensure that the FCurves of a group are continuously
     * stored (i.e. not mixed with other groups) to be sure. See #89435. */
    LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
      BKE_action_groups_reconstruct(act);
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == GEO_NODE_SUBDIVIDE_MESH) {
            STRNCPY_UTF8(node->idname, "GeometryNodeMeshSubdivide");
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 10)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      if (tool_settings->snap_uv_mode & (1 << 4)) {
        tool_settings->snap_uv_mode |= (1 << 6); /* SCE_SNAP_TO_INCREMENT */
        tool_settings->snap_uv_mode &= ~(1 << 4);
      }
    }
    LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
      if (!(mat->lineart.flags & LRT_MATERIAL_CUSTOM_OCCLUSION_EFFECTIVENESS)) {
        mat->lineart.mat_occlusion = 1;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 13)) {
    /* Convert Surface Deform to sparse-capable bind structure. */
    if (!DNA_struct_member_exists(
            fd->filesdna, "SurfaceDeformModifierData", "int", "mesh_verts_num"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SurfaceDeform) {
            SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
            if (smd->bind_verts_num && smd->verts) {
              smd->mesh_verts_num = smd->bind_verts_num;

              for (uint i = 0; i < smd->bind_verts_num; i++) {
                smd->verts[i].vertex_idx = i;
              }
            }
          }
        }
        if (ob->type == OB_GPENCIL_LEGACY) {
          LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
            if (md->type == eGpencilModifierType_Lineart) {
              LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
              lmd->flags |= MOD_LINEART_USE_CACHE;
              lmd->chain_smooth_tolerance = 0.2f;
            }
          }
        }
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "WorkSpace", "AssetLibraryReference", "asset_library"))
    {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        BKE_asset_library_reference_init_default(&workspace->asset_library_ref);
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "FileAssetSelectParams", "AssetLibraryReference", "asset_library_ref"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_FILE) {
              SpaceFile *sfile = (SpaceFile *)space;
              if (sfile->browse_mode != FILE_BROWSE_MODE_ASSETS) {
                continue;
              }
              BKE_asset_library_reference_init_default(&sfile->asset_params->asset_library_ref);
            }
          }
        }
      }
    }

    /* Set default 2D annotation placement. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      ts->gpencil_v2d_align = GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 14)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag &= ~SCE_SNAP_SEQ;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 15)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->flag |= SEQ_TIMELINE_SHOW_GRID;
          }
        }
      }
    }
  }

  /* Font names were copied directly into ID names, see: #90417. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 16)) {
    ListBase *lb = which_libbase(bmain, ID_VF);
    BKE_main_id_repair_duplicate_names_listbase(bmain, lb);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 17)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "normals_constant_screen_size"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.normals_constant_screen_size = 7.0f;
            }
          }
        }
      }
    }

    /* Fix SplineIK constraint's inconsistency between binding points array and its stored size.
     */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      /* NOTE: Objects should never have SplineIK constraint, so no need to apply this fix on
       * their constraints. */
      if (ob->pose) {
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          do_version_constraints_spline_ik_joint_bindings(&pchan->constraints);
        }
      }
    }
  }

  /* Move visibility from Cycles to Blender. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 17)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      IDProperty *cvisibility = version_cycles_visibility_properties_from_ID(&object->id);
      int flag = 0;

      if (cvisibility) {
        flag |= version_cycles_property_boolean(cvisibility, "camera", true) ? 0 : OB_HIDE_CAMERA;
        flag |= version_cycles_property_boolean(cvisibility, "diffuse", true) ? 0 :
                                                                                OB_HIDE_DIFFUSE;
        flag |= version_cycles_property_boolean(cvisibility, "glossy", true) ? 0 : OB_HIDE_GLOSSY;
        flag |= version_cycles_property_boolean(cvisibility, "transmission", true) ?
                    0 :
                    OB_HIDE_TRANSMISSION;
        flag |= version_cycles_property_boolean(cvisibility, "scatter", true) ?
                    0 :
                    OB_HIDE_VOLUME_SCATTER;
        flag |= version_cycles_property_boolean(cvisibility, "shadow", true) ? 0 : OB_HIDE_SHADOW;
      }

      IDProperty *cobject = version_cycles_properties_from_ID(&object->id);
      if (cobject) {
        flag |= version_cycles_property_boolean(cobject, "is_holdout", false) ? OB_HOLDOUT : 0;
        flag |= version_cycles_property_boolean(cobject, "is_shadow_catcher", false) ?
                    OB_SHADOW_CATCHER :
                    0;
      }

      if (object->type == OB_LAMP) {
        flag |= OB_HIDE_CAMERA | OB_SHADOW_CATCHER;
      }

      /* Clear unused bits from old version, and add new flags. */
      object->visibility_flag &= (OB_HIDE_VIEWPORT | OB_HIDE_SELECT | OB_HIDE_RENDER);
      object->visibility_flag |= flag;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 18)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "WorkSpace", "AssetLibraryReference", "asset_library_ref"))
    {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        BKE_asset_library_reference_init_default(&workspace->asset_library_ref);
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "FileAssetSelectParams", "AssetLibraryReference", "asset_library_ref"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype != SPACE_FILE) {
              continue;
            }

            SpaceFile *sfile = (SpaceFile *)space;
            if (sfile->browse_mode != FILE_BROWSE_MODE_ASSETS) {
              continue;
            }
            BKE_asset_library_reference_init_default(&sfile->asset_params->asset_library_ref);
          }
        }
      }
    }

    /* Previously, only text ending with `.py` would run, apply this logic
     * to existing files so text that happens to have the "Register" enabled
     * doesn't suddenly start running code on startup that was previously ignored. */
    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      if ((text->flags & TXT_ISSCRIPT) && !BLI_path_extension_check(text->id.name + 2, ".py")) {
        text->flags &= ~TXT_ISSCRIPT;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 19)) {
    /* Disable Fade Inactive Overlay by default as it is redundant after introducing flash on
     * mode transfer. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.flag &= ~V3D_OVERLAY_FADE_INACTIVE;
          }
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
      sequencer_tool_settings->overlap_mode = SEQ_OVERLAP_SHUFFLE;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 20)) {
    /* Use new vector Size socket in Cube Mesh Primitive node. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }

      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
        if (link->tonode->type_legacy == GEO_NODE_MESH_PRIMITIVE_CUBE) {
          bNode *node = link->tonode;
          if (STREQ(link->tosock->identifier, "Size") && link->tosock->type == SOCK_FLOAT) {
            bNode *link_fromnode = link->fromnode;
            bNodeSocket *link_fromsock = link->fromsock;
            bNodeSocket *socket = link->tosock;
            BLI_assert(socket);

            bNodeSocket *new_socket = do_version_replace_float_size_with_vector(
                ntree, node, socket);
            blender::bke::node_add_link(
                *ntree, *link_fromnode, *link_fromsock, *node, *new_socket);
          }
        }
      }

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != GEO_NODE_MESH_PRIMITIVE_CUBE) {
          continue;
        }
        LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
          if (STREQ(socket->identifier, "Size") && (socket->type == SOCK_FLOAT)) {
            do_version_replace_float_size_with_vector(ntree, node, socket);
            break;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 22)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "LineartGpencilModifierData", "bool", "use_crease_on_smooth"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->type == OB_GPENCIL_LEGACY) {
          LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
            if (md->type == eGpencilModifierType_Lineart) {
              LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
              lmd->calculation_flags |= MOD_LINEART_USE_CREASE_ON_SMOOTH_SURFACES;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 23)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_FILE) {
            SpaceFile *sfile = (SpaceFile *)sl;
            if (sfile->asset_params) {
              sfile->asset_params->base_params.recursion_level = FILE_SELECT_MAX_RECURSIONS;
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            int seq_show_safe_margins = (sseq->flag & SEQ_PREVIEW_SHOW_SAFE_MARGINS);
            int seq_show_gpencil = (sseq->flag & SEQ_PREVIEW_SHOW_GPENCIL);
            int seq_show_fcurves = (sseq->flag & SEQ_TIMELINE_SHOW_FCURVES);
            int seq_show_safe_center = (sseq->flag & SEQ_PREVIEW_SHOW_SAFE_CENTER);
            int seq_show_metadata = (sseq->flag & SEQ_PREVIEW_SHOW_METADATA);
            int seq_show_strip_name = (sseq->flag & SEQ_TIMELINE_SHOW_STRIP_NAME);
            int seq_show_strip_source = (sseq->flag & SEQ_TIMELINE_SHOW_STRIP_SOURCE);
            int seq_show_strip_duration = (sseq->flag & SEQ_TIMELINE_SHOW_STRIP_DURATION);
            int seq_show_grid = (sseq->flag & SEQ_TIMELINE_SHOW_GRID);
            int show_strip_offset = (sseq->draw_flag & SEQ_TIMELINE_SHOW_STRIP_OFFSETS);
            sseq->preview_overlay.flag = (seq_show_safe_margins | seq_show_gpencil |
                                          seq_show_safe_center | seq_show_metadata);
            sseq->timeline_overlay.flag = (seq_show_fcurves | seq_show_strip_name |
                                           seq_show_strip_source | seq_show_strip_duration |
                                           seq_show_grid | show_strip_offset);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 24)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
      sequencer_tool_settings->pivot_point = V3D_AROUND_CENTER_MEDIAN;

      if (scene->ed != nullptr) {
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_transform_origin_set, nullptr);
      }
    }
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->preview_overlay.flag |= SEQ_PREVIEW_SHOW_OUTLINE_SELECTED;
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.min[1] = 4.0f;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 25)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          do_version_subsurface_methods(node);
        }
      }
    }
    FOREACH_NODETREE_END;

    enum {
      R_EXR_TILE_FILE = (1 << 10),
      R_FULL_SAMPLE = (1 << 15),
    };
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.scemode &= ~(R_EXR_TILE_FILE | R_FULL_SAMPLE);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 25)) {
    enum {
      DENOISER_NLM = 1,
      DENOISER_OPENIMAGEDENOISE = 4,
    };

    /* Removal of NLM denoiser. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      IDProperty *cscene = version_cycles_properties_from_ID(&scene->id);

      if (cscene) {
        if (version_cycles_property_int(cscene, "denoiser", DENOISER_NLM) == DENOISER_NLM) {
          version_cycles_property_int_set(cscene, "denoiser", DENOISER_OPENIMAGEDENOISE);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 26)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Nodes) {
          version_geometry_nodes_add_attribute_input_settings((NodesModifierData *)md);
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_FILE: {
              SpaceFile *sfile = (SpaceFile *)sl;
              if (sfile->params) {
                sfile->params->flag &= ~(FILE_PARAMS_FLAG_UNUSED_1 | FILE_PARAMS_FLAG_UNUSED_2 |
                                         FILE_PARAMS_FLAG_UNUSED_3 | FILE_PATH_TOKENS_ALLOW);
              }

              /* New default import method: Append with reuse. */
              if (sfile->asset_params) {
                sfile->asset_params->import_method = FILE_ASSET_IMPORT_APPEND_REUSE;
              }
              break;
            }
            default:
              break;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 29)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_SEQ: {
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              LISTBASE_FOREACH (ARegion *, region, regionbase) {
                if (region->regiontype == RGN_TYPE_WINDOW) {
                  region->v2d.max[1] = blender::seq::MAX_CHANNELS;
                }
              }
              break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 31)) {
    /* Swap header with the tool header so the regular header is always on the edge. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *region_tool = nullptr, *region_head = nullptr;
          int region_tool_index = -1, region_head_index = -1, i;
          LISTBASE_FOREACH_INDEX (ARegion *, region, regionbase, i) {
            if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
              region_tool = region;
              region_tool_index = i;
            }
            else if (region->regiontype == RGN_TYPE_HEADER) {
              region_head = region;
              region_head_index = i;
            }
          }
          if ((region_tool && region_head) && (region_head_index > region_tool_index)) {
            BLI_listbase_swaplinks(regionbase, region_tool, region_head);
          }
        }
      }
    }

    /* Set strip color tags to STRIP_COLOR_NONE. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        blender::seq::foreach_strip(
            &scene->ed->seqbase, do_versions_sequencer_color_tags, nullptr);
      }
    }

    /* Show sequencer color tags by default. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->timeline_overlay.flag |= SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG;
          }
        }
      }
    }

    /* Set defaults for new color balance modifier parameters. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        blender::seq::foreach_strip(
            &scene->ed->seqbase, do_versions_sequencer_color_balance_sop, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 33)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_SEQ: {
              SpaceSeq *sseq = (SpaceSeq *)sl;
              enum { SEQ_DRAW_SEQUENCE = 0 };
              if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
                sseq->mainb = SEQ_DRAW_IMG_IMBUF;
              }
              break;
            }
            case SPACE_TEXT: {
              SpaceText *st = (SpaceText *)sl;
              st->flags &= ~ST_FLAG_UNUSED_4;
              break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 36)) {
    /* Update the `idnames` for renamed geometry and function nodes. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_node_id(ntree, FN_NODE_COMPARE, "FunctionNodeCompareFloats");
      version_node_id(ntree, GEO_NODE_CAPTURE_ATTRIBUTE, "GeometryNodeCaptureAttribute");
      version_node_id(ntree, GEO_NODE_MESH_BOOLEAN, "GeometryNodeMeshBoolean");
      version_node_id(ntree, GEO_NODE_FILL_CURVE, "GeometryNodeFillCurve");
      version_node_id(ntree, GEO_NODE_FILLET_CURVE, "GeometryNodeFilletCurve");
      version_node_id(ntree, GEO_NODE_REVERSE_CURVE, "GeometryNodeReverseCurve");
      version_node_id(ntree, GEO_NODE_SAMPLE_CURVE, "GeometryNodeSampleCurve");
      version_node_id(ntree, GEO_NODE_RESAMPLE_CURVE, "GeometryNodeResampleCurve");
      version_node_id(ntree, GEO_NODE_SUBDIVIDE_CURVE, "GeometryNodeSubdivideCurve");
      version_node_id(ntree, GEO_NODE_TRIM_CURVE, "GeometryNodeTrimCurve");
      version_node_id(ntree, GEO_NODE_REPLACE_MATERIAL, "GeometryNodeReplaceMaterial");
      version_node_id(ntree, GEO_NODE_SUBDIVIDE_MESH, "GeometryNodeSubdivideMesh");
      version_node_id(ntree, GEO_NODE_SET_MATERIAL, "GeometryNodeSetMaterial");
      version_node_id(ntree, GEO_NODE_SPLIT_EDGES, "GeometryNodeSplitEdges");
    }

    /* Update bone roll after a fix to vec_roll_to_mat3_normalized. */
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      do_version_bones_roll(&arm->bonebase);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 37)) {
    /* Node Editor: toggle overlays on. */
    if (!DNA_struct_exists(fd->filesdna, "SpaceNodeOverlay")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_NODE) {
              SpaceNode *snode = (SpaceNode *)space;
              snode->overlay.flag |= SN_OVERLAY_SHOW_OVERLAYS;
              snode->overlay.flag |= SN_OVERLAY_SHOW_WIRE_COLORS;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 38)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_FILE) {
            SpaceFile *sfile = (SpaceFile *)space;
            FileAssetSelectParams *asset_params = sfile->asset_params;
            if (asset_params) {
              asset_params->base_params.filter_id = FILTER_ID_ALL;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 39)) {
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      wm->xr.session_settings.base_scale = 1.0f;
      wm->xr.session_settings.draw_flags |= (V3D_OFSDRAW_SHOW_SELECTION |
                                             V3D_OFSDRAW_XR_SHOW_CONTROLLERS |
                                             V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 40)) {
    /* Update the `idnames` for renamed geometry and function nodes. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_node_id(ntree, FN_NODE_SLICE_STRING, "FunctionNodeSliceString");
      version_geometry_nodes_set_position_node_offset(ntree);
    }

    /* Add storage to viewer node. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == GEO_NODE_VIEWER) {
          if (node->storage == nullptr) {
            NodeGeometryViewer *data = MEM_callocN<NodeGeometryViewer>(__func__);
            data->data_type_legacy = CD_PROP_FLOAT;
            node->storage = data;
          }
        }
      }
    }

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_input_socket_name(
            ntree, GEO_NODE_DISTRIBUTE_POINTS_ON_FACES, "Geometry", "Mesh");
        version_node_input_socket_name(ntree, GEO_NODE_POINTS_TO_VOLUME, "Geometry", "Points");
        version_node_output_socket_name(ntree, GEO_NODE_POINTS_TO_VOLUME, "Geometry", "Volume");
        version_node_socket_name(ntree, GEO_NODE_SUBDIVISION_SURFACE, "Geometry", "Mesh");
        version_node_socket_name(ntree, GEO_NODE_RESAMPLE_CURVE, "Geometry", "Curve");
        version_node_socket_name(ntree, GEO_NODE_SUBDIVIDE_CURVE, "Geometry", "Curve");
        version_node_socket_name(ntree, GEO_NODE_SET_CURVE_RADIUS, "Geometry", "Curve");
        version_node_socket_name(ntree, GEO_NODE_SET_CURVE_TILT, "Geometry", "Curve");
        version_node_socket_name(ntree, GEO_NODE_SET_CURVE_HANDLES, "Geometry", "Curve");
        version_node_socket_name(ntree, GEO_NODE_TRANSLATE_INSTANCES, "Geometry", "Instances");
        version_node_socket_name(ntree, GEO_NODE_ROTATE_INSTANCES, "Geometry", "Instances");
        version_node_socket_name(ntree, GEO_NODE_SCALE_INSTANCES, "Geometry", "Instances");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_BOOLEAN, "Geometry", "Mesh");
        version_node_input_socket_name(ntree, GEO_NODE_MESH_BOOLEAN, "Geometry 1", "Mesh 1");
        version_node_input_socket_name(ntree, GEO_NODE_MESH_BOOLEAN, "Geometry 2", "Mesh 2");
        version_node_socket_name(ntree, GEO_NODE_SUBDIVIDE_MESH, "Geometry", "Mesh");
        version_node_socket_name(ntree, GEO_NODE_TRIANGULATE, "Geometry", "Mesh");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_PRIMITIVE_CONE, "Geometry", "Mesh");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_PRIMITIVE_CUBE, "Geometry", "Mesh");
        version_node_output_socket_name(
            ntree, GEO_NODE_MESH_PRIMITIVE_CYLINDER, "Geometry", "Mesh");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_PRIMITIVE_GRID, "Geometry", "Mesh");
        version_node_output_socket_name(
            ntree, GEO_NODE_MESH_PRIMITIVE_ICO_SPHERE, "Geometry", "Mesh");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_PRIMITIVE_CIRCLE, "Geometry", "Mesh");
        version_node_output_socket_name(ntree, GEO_NODE_MESH_PRIMITIVE_LINE, "Geometry", "Mesh");
        version_node_output_socket_name(
            ntree, GEO_NODE_MESH_PRIMITIVE_UV_SPHERE, "Geometry", "Mesh");
        version_node_socket_name(ntree, GEO_NODE_SET_POINT_RADIUS, "Geometry", "Points");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 42)) {
    /* Use consistent socket identifiers for the math node.
     * The code to make unique identifiers from the names was inconsistent. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        version_node_tree_socket_id_delim(ntree);
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.min[1] = 1.0f;
              }
            }
          }
        }
      }
    }

    /* Change minimum zoom to 0.05f in the node editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_NODE) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                region->v2d.minzoom = std::min(region->v2d.minzoom, 0.05f);
              }
            }
          }
        }
      }
    }
  }

  /* Special case to handle older in-development 3.1 files, before change from 3.0 branch gets
   * merged in master. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 42) ||
      (bmain->versionfile == 301 && !MAIN_VERSION_FILE_ATLEAST(bmain, 301, 3)))
  {
    /* Update LibOverride operations regarding insertions in RNA collections (i.e. modifiers,
     * constraints and NLA tracks). */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id_iter)) {
        version_liboverride_rnacollections_insertion_animdata(id_iter);
        if (GS(id_iter->name) == ID_OB) {
          version_liboverride_rnacollections_insertion_object((Object *)id_iter);
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 301, 4)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_node_id(ntree, GEO_NODE_CURVE_SPLINE_PARAMETER, "GeometryNodeSplineParameter");
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == GEO_NODE_CURVE_SPLINE_PARAMETER) {
          version_node_add_socket_if_not_exist(
              ntree, node, SOCK_OUT, SOCK_INT, PROP_NONE, "Index", "Index");
        }

        /* Convert float compare into a more general compare node. */
        if (node->type_legacy == FN_NODE_COMPARE) {
          if (node->storage == nullptr) {
            NodeFunctionCompare *data = MEM_callocN<NodeFunctionCompare>(__func__);
            data->data_type = SOCK_FLOAT;
            data->operation = node->custom1;
            STRNCPY_UTF8(node->idname, "FunctionNodeCompare");
            node->storage = data;
          }
        }
      }
    }

    /* Add a toggle for the breadcrumbs overlay in the node editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)space;
            snode->overlay.flag |= SN_OVERLAY_SHOW_PATH;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 301, 6)) {
    /* Add node storage for map range node. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == SH_NODE_MAP_RANGE) {
          if (node->storage == nullptr) {
            NodeMapRange *data = MEM_callocN<NodeMapRange>(__func__);
            data->clamp = node->custom1;
            data->data_type = CD_PROP_FLOAT;
            data->interpolation_type = node->custom2;
            node->storage = data;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    /* Update spreadsheet data set region type. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SPREADSHEET) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_CHANNELS) {
                region->regiontype = RGN_TYPE_TOOLS;
              }
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Curve *, curve, &bmain->curves) {
      LISTBASE_FOREACH (Nurb *, nurb, &curve->nurb) {
        /* Previously other flags were ignored if CU_NURB_CYCLIC is set. */
        if (nurb->flagu & CU_NURB_CYCLIC) {
          nurb->flagu = CU_NURB_CYCLIC;
          BKE_nurb_knot_calc_u(nurb);
        }
        /* Previously other flags were ignored if CU_NURB_CYCLIC is set. */
        if (nurb->flagv & CU_NURB_CYCLIC) {
          nurb->flagv = CU_NURB_CYCLIC;
          BKE_nurb_knot_calc_v(nurb);
        }
      }
    }

    /* Initialize the bone wireframe opacity setting. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "bone_wire_alpha")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.bone_wire_alpha = 1.0f;
            }
          }
        }
      }
    }

    /* Rename sockets on multiple nodes */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_output_socket_name(
            ntree, GEO_NODE_STRING_TO_CURVES, "Curves", "Curve Instances");
        version_node_output_socket_name(
            ntree, GEO_NODE_INPUT_MESH_EDGE_ANGLE, "Angle", "Unsigned Angle");
        version_node_output_socket_name(
            ntree, GEO_NODE_INPUT_MESH_ISLAND, "Index", "Island Index");
        version_node_input_socket_name(
            ntree, GEO_NODE_TRANSFER_ATTRIBUTE_DEPRECATED, "Target", "Source");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 301, 7) ||
      (bmain->versionfile == 302 && !MAIN_VERSION_FILE_ATLEAST(bmain, 302, 4)))
  {
    /* Duplicate value for two flags that mistakenly had the same numeric value. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_WeightVGProximity) {
          WeightVGProximityModifierData *wpmd = (WeightVGProximityModifierData *)md;
          if (wpmd->proximity_flags & MOD_WVG_PROXIMITY_INVERT_VGROUP_MASK) {
            wpmd->proximity_flags |= MOD_WVG_PROXIMITY_WEIGHTS_NORMALIZE;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 2)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_transform_filter_set, nullptr);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 6)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag_seq = tool_settings->snap_flag &
                                     ~(short(SCE_SNAP) | short(SCE_SNAP_SEQ));
      if (tool_settings->snap_flag & SCE_SNAP_SEQ) {
        tool_settings->snap_flag_seq |= SCE_SNAP;
        tool_settings->snap_flag &= ~SCE_SNAP_SEQ;
      }

      tool_settings->snap_flag_node = tool_settings->snap_flag;
      tool_settings->snap_uv_flag |= tool_settings->snap_flag & SCE_SNAP;
    }

    /* Alter NURBS knot mode flags to fit new modes. */
    LISTBASE_FOREACH (Curve *, curve, &bmain->curves) {
      LISTBASE_FOREACH (Nurb *, nurb, &curve->nurb) {
        /* CU_NURB_BEZIER and CU_NURB_ENDPOINT were ignored if combined. */
        if (nurb->flagu & CU_NURB_BEZIER && nurb->flagu & CU_NURB_ENDPOINT) {
          nurb->flagu &= ~(CU_NURB_BEZIER | CU_NURB_ENDPOINT);
          BKE_nurb_knot_calc_u(nurb);
        }
        else if (nurb->flagu & CU_NURB_CYCLIC) {
          /* In 45d038181ae2 cyclic bezier support is added, but CU_NURB_ENDPOINT still ignored. */
          nurb->flagu = CU_NURB_CYCLIC | (nurb->flagu & CU_NURB_BEZIER);
          BKE_nurb_knot_calc_u(nurb);
        }
        /* Bezier NURBS of order 3 were clamped to first control point. */
        if (nurb->orderu == 3 && (nurb->flagu & CU_NURB_BEZIER)) {
          nurb->flagu |= CU_NURB_ENDPOINT;
          BKE_nurb_knot_calc_u(nurb);
        }
        /* CU_NURB_BEZIER and CU_NURB_ENDPOINT were ignored if combined. */
        if (nurb->flagv & CU_NURB_BEZIER && nurb->flagv & CU_NURB_ENDPOINT) {
          nurb->flagv &= ~(CU_NURB_BEZIER | CU_NURB_ENDPOINT);
          BKE_nurb_knot_calc_v(nurb);
        }
        else if (nurb->flagv & CU_NURB_CYCLIC) {
          /* In 45d038181ae2 cyclic bezier support is added, but CU_NURB_ENDPOINT still ignored. */
          nurb->flagv = CU_NURB_CYCLIC | (nurb->flagv & CU_NURB_BEZIER);
          BKE_nurb_knot_calc_v(nurb);
        }
        /* Bezier NURBS of order 3 were clamped to first control point. */
        if (nurb->orderv == 3 && (nurb->flagv & CU_NURB_BEZIER)) {
          nurb->flagv |= CU_NURB_ENDPOINT;
          BKE_nurb_knot_calc_v(nurb);
        }
      }
    }

    /* Change grease pencil smooth iterations to match old results with new algorithm. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
        if (md->type == eGpencilModifierType_Smooth) {
          SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
          if (gpmd->step == 1 && gpmd->factor <= 0.5f) {
            gpmd->factor *= 2.0f;
          }
          else {
            gpmd->step = 1 + int(gpmd->factor * max_ff(0.0f,
                                                       min_ff(5.1f * sqrtf(gpmd->step) - 3.0f,
                                                              gpmd->step + 2.0f)));
            gpmd->factor = 1.0f;
          }
        }
      }
    }
  }

  /* Rebuild active/render color attribute references. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 6)) {
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      /* Buggy code in wm_toolsystem broke smear in old files,
       * reset to defaults. */
      if (br->sculpt_brush_type == SCULPT_BRUSH_TYPE_SMEAR) {
        br->alpha = 1.0f;
        br->spacing = 5;
        br->flag &= ~BRUSH_ALPHA_PRESSURE;
        br->flag &= ~BRUSH_SPACE_ATTEN;
        br->curve_distance_falloff_preset = BRUSH_CURVE_SPHERE;
      }
    }

    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (int step = 0; step < 2; step++) {
        CustomDataLayer *actlayer = nullptr;

        int vact1, vact2;

        if (step) {
          vact1 = CustomData_get_render_layer_index(&me->vert_data, CD_PROP_COLOR);
          vact2 = CustomData_get_render_layer_index(&me->corner_data, CD_PROP_BYTE_COLOR);
        }
        else {
          vact1 = CustomData_get_active_layer_index(&me->vert_data, CD_PROP_COLOR);
          vact2 = CustomData_get_active_layer_index(&me->corner_data, CD_PROP_BYTE_COLOR);
        }

        if (vact1 != -1) {
          actlayer = me->vert_data.layers + vact1;
        }
        else if (vact2 != -1) {
          actlayer = me->corner_data.layers + vact2;
        }

        if (actlayer) {
          if (step) {
            BKE_id_attributes_default_color_set(&me->id, actlayer->name);
          }
          else {
            BKE_id_attributes_active_color_set(&me->id, actlayer->name);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 7)) {
    /* Generate 'system' liboverrides IDs.
     * NOTE: This is a fairly rough process, based on very basic heuristics. Should be enough for a
     * do_version code though, this is a new optional feature, not a critical conversion. */
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) || ID_IS_LINKED(id)) {
        /* Ignore non-real liboverrides, and linked ones. */
        continue;
      }
      if (GS(id->name) == ID_OB) {
        /* Never 'lock' an object into a system override for now. */
        continue;
      }
      if (BKE_lib_override_library_is_user_edited(id)) {
        /* Do not 'lock' an ID already edited by the user. */
        continue;
      }
      id->override_library->flag |= LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
    }
    FOREACH_MAIN_ID_END;

    /* Initialize brush curves sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode != OB_MODE_SCULPT_CURVES) {
        continue;
      }
      if (brush->curves_sculpt_settings != nullptr) {
        continue;
      }
      brush->curves_sculpt_settings = MEM_callocN<BrushCurvesSculptSettings>(__func__);
      brush->curves_sculpt_settings->add_amount = 1;
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
            space_outliner->filter &= ~SO_FILTER_CLEARED_1;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 9)) {
    /* Sequencer channels region. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_SEQ) {
            continue;
          }
          if (ELEM(((SpaceSeq *)sl)->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
            continue;
          }

          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *region = BKE_region_find_in_listbase_by_type(regionbase, RGN_TYPE_CHANNELS);
          if (!region) {
            /* Find sequencer tools region. */
            ARegion *tools_region = BKE_region_find_in_listbase_by_type(regionbase,
                                                                        RGN_TYPE_TOOLS);
            region = do_versions_add_region(RGN_TYPE_CHANNELS, "channels region");
            BLI_insertlinkafter(regionbase, tools_region, region);
            region->alignment = RGN_ALIGN_LEFT;
            region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
          }

          ARegion *timeline_region = BKE_region_find_in_listbase_by_type(regionbase,
                                                                         RGN_TYPE_WINDOW);
          if (timeline_region != nullptr) {
            timeline_region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
          }
        }
      }
    }

    /* Initialize channels. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed == nullptr) {
        continue;
      }
      blender::seq::channels_ensure(&ed->channels);
      blender::seq::foreach_strip(&scene->ed->seqbase, strip_meta_channels_ensure, nullptr);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 10)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_FILE) {
            continue;
          }
          SpaceFile *sfile = (SpaceFile *)sl;
          if (sfile->browse_mode != FILE_BROWSE_MODE_ASSETS) {
            continue;
          }
          sfile->asset_params->base_params.filter_id |= FILTER_ID_GR;
        }
      }
    }

    /* While vertex-colors were experimental the smear tool became corrupt due
     * to bugs in the wm_toolsystem API (auto-creation of sculpt brushes
     * was broken).  Go through and reset all smear brushes. */
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->sculpt_brush_type == SCULPT_BRUSH_TYPE_SMEAR) {
        br->alpha = 1.0f;
        br->spacing = 5;
        br->flag &= ~BRUSH_ALPHA_PRESSURE;
        br->flag &= ~BRUSH_SPACE_ATTEN;
        br->curve_distance_falloff_preset = BRUSH_CURVE_SPHERE;
      }
    }

    /* Rebuild active/render color attribute references. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (int step = 0; step < 2; step++) {
        CustomDataLayer *actlayer = nullptr;

        int vact1, vact2;

        if (step) {
          vact1 = CustomData_get_render_layer_index(&me->vert_data, CD_PROP_COLOR);
          vact2 = CustomData_get_render_layer_index(&me->corner_data, CD_PROP_BYTE_COLOR);
        }
        else {
          vact1 = CustomData_get_active_layer_index(&me->vert_data, CD_PROP_COLOR);
          vact2 = CustomData_get_active_layer_index(&me->corner_data, CD_PROP_BYTE_COLOR);
        }

        if (vact1 != -1) {
          actlayer = me->vert_data.layers + vact1;
        }
        else if (vact2 != -1) {
          actlayer = me->corner_data.layers + vact2;
        }

        if (actlayer) {
          if (step) {
            BKE_id_attributes_default_color_set(&me->id, actlayer->name);
          }
          else {
            BKE_id_attributes_active_color_set(&me->id, actlayer->name);
          }
        }
      }
    }

    /* Update data transfer modifiers */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_DataTransfer) {
          DataTransferModifierData *dtmd = (DataTransferModifierData *)md;

          for (int i = 0; i < DT_MULTILAYER_INDEX_MAX; i++) {
            if (dtmd->layers_select_src[i] == 0) {
              dtmd->layers_select_src[i] = DT_LAYERS_ALL_SRC;
            }

            if (dtmd->layers_select_dst[i] == 0) {
              dtmd->layers_select_dst[i] = DT_LAYERS_NAME_DST;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 12)) {
    /* UV/Image show background grid option. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)space;
            sima->overlay.flag |= SI_OVERLAY_SHOW_GRID_BACKGROUND;
          }
        }
      }
    }

    /* Add node storage for the merge by distance node. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == GEO_NODE_MERGE_BY_DISTANCE) {
            if (node->storage == nullptr) {
              NodeGeometryMergeByDistance *data = MEM_callocN<NodeGeometryMergeByDistance>(
                  __func__);
              data->mode = GEO_NODE_MERGE_BY_DISTANCE_MODE_ALL;
              node->storage = data;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_input_socket_name(
            ntree, GEO_NODE_SUBDIVISION_SURFACE, "Crease", "Edge Crease");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 13)) {
    /* Enable named attributes overlay in node editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_NODE) {
            SpaceNode *snode = (SpaceNode *)space;
            snode->overlay.flag |= SN_OVERLAY_SHOW_NAMED_ATTRIBUTES;
          }
        }
      }
    }

    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      BrushCurvesSculptSettings *settings = brush->curves_sculpt_settings;
      if (settings == nullptr) {
        continue;
      }
      if (settings->curve_length == 0.0f) {
        settings->curve_length = 0.3f;
      }
    }
  }

  if (!DNA_struct_member_exists(fd->filesdna, "Sculpt", "float", "automasking_cavity_factor")) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->toolsettings && scene->toolsettings->sculpt) {
        scene->toolsettings->sculpt->automasking_cavity_factor = 0.5f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 302, 14)) {
    /* Compensate for previously wrong squared distance. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.bake.max_ray_distance = safe_sqrtf(scene->r.bake.max_ray_distance);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 1)) {
    /* Initialize brush curves sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode != OB_MODE_SCULPT_CURVES) {
        continue;
      }
      if (brush->curves_sculpt_settings->points_per_curve == 0) {
        brush->curves_sculpt_settings->points_per_curve = 8;
      }
    }

    /* UDIM Packing. */
    if (!DNA_struct_member_exists(fd->filesdna, "ImagePackedFile", "int", "tile_number")) {
      LISTBASE_FOREACH (Image *, ima, &bmain->images) {
        int view;
        LISTBASE_FOREACH_INDEX (ImagePackedFile *, imapf, &ima->packedfiles, view) {
          imapf->view = view;
          imapf->tile_number = 1001;
        }
      }
    }

    /* Merge still offsets into start/end offsets. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, version_merge_still_offsets, nullptr);
      }
    }

    /* Use the curves type enum for the set spline type node, instead of a special one. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == GEO_NODE_CURVE_SPLINE_TYPE) {
            NodeGeometryCurveSplineType *storage = (NodeGeometryCurveSplineType *)node->storage;
            switch (storage->spline_type) {
              case 0: /* GEO_NODE_SPLINE_TYPE_BEZIER */
                storage->spline_type = CURVE_TYPE_BEZIER;
                break;
              case 1: /* GEO_NODE_SPLINE_TYPE_NURBS */
                storage->spline_type = CURVE_TYPE_NURBS;
                break;
              case 2: /* GEO_NODE_SPLINE_TYPE_POLY */
                storage->spline_type = CURVE_TYPE_POLY;
                break;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (GpencilModifierData *, gpd, &ob->greasepencil_modifiers) {
        if (gpd->type == eGpencilModifierType_Lineart) {
          LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)gpd;
          lmd->shadow_camera_near = 0.1f;
          lmd->shadow_camera_far = 200.0f;
          lmd->shadow_camera_size = 200.0f;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 2)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_CLIP) {
            ((SpaceClip *)sl)->mask_info.blend_factor = 1.0;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 3)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_CLIP) {
            ((SpaceClip *)sl)->mask_info.draw_flag |= MASK_DRAWFLAG_SPLINE;
          }
          else if (sl->spacetype == SPACE_IMAGE) {
            ((SpaceImage *)sl)->mask_info.draw_flag |= MASK_DRAWFLAG_SPLINE;
          }
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      /* Zero isn't a valid value, use for versioning. */
      if (tool_settings->snap_face_nearest_steps == 0) {
        /* Minimum of snap steps for face nearest is 1. */
        tool_settings->snap_face_nearest_steps = 1;
        /* Set snap to edited and non-edited as default. */
        tool_settings->snap_flag |= SCE_SNAP_TO_INCLUDE_EDITED | SCE_SNAP_TO_INCLUDE_NONEDITED;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_OUTPUT_FILE) {
            LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
              if (sock->storage) {
                NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)sock->storage;
                version_fix_image_format_copy(bmain, &sockdata->format);
              }
            }

            if (node->storage) {
              NodeCompositorFileOutput *nimf = (NodeCompositorFileOutput *)node->storage;
              version_fix_image_format_copy(bmain, &nimf->format);
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      version_fix_image_format_copy(bmain, &scene->r.im_format);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 5)) {
    /* Fix for #98925 - remove channels region, that was initialized in incorrect editor types. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_CLIP, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ)) {
            continue;
          }

          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *channels_region = BKE_region_find_in_listbase_by_type(regionbase,
                                                                         RGN_TYPE_CHANNELS);
          if (channels_region) {
            MEM_delete(channels_region->runtime);
            BLI_freelinkN(regionbase, channels_region);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 303, 6)) {
    /* Initialize brush curves sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode != OB_MODE_SCULPT_CURVES) {
        continue;
      }
      brush->curves_sculpt_settings->density_add_attempts = 100;
    }

    /* Disable 'show_bounds' option of curve objects. Option was set as there was no object mode
     * outline implementation. See #95933. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->type == OB_CURVES) {
        ob->dtx &= ~OB_DRAWBOUNDOX;
      }
    }

    BKE_main_namemap_validate_and_fix(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 1)) {
    /* Image generation information transferred to tiles. */
    if (!DNA_struct_member_exists(fd->filesdna, "ImageTile", "int", "gen_x")) {
      LISTBASE_FOREACH (Image *, ima, &bmain->images) {
        LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
          tile->gen_x = ima->gen_x;
          tile->gen_y = ima->gen_y;
          tile->gen_type = ima->gen_type;
          tile->gen_flag = ima->gen_flag;
          tile->gen_depth = ima->gen_depth;
          copy_v4_v4(tile->gen_color, ima->gen_color);
        }
      }
    }

    /* Convert mix rgb node to new mix node and add storage. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_replace_legacy_mix_rgb_node(ntree);
    }
    FOREACH_NODETREE_END;

    /* Face sets no longer store whether the corresponding face is hidden. */
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      int *face_sets = (int *)CustomData_get_layer(&mesh->face_data, CD_SCULPT_FACE_SETS);
      if (face_sets) {
        for (int i = 0; i < mesh->faces_num; i++) {
          face_sets[i] = abs(face_sets[i]);
        }
      }
    }

    /* Custom grids in UV Editor have separate X and Y divisions. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->custom_grid_subdiv[0] = 10;
              sima->custom_grid_subdiv[1] = 10;
              break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 2)) {
    /* Initialize brush curves sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      brush->automasking_cavity_factor = 0.5f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 3)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->flag2 |= V3D_SHOW_VIEWER;
            v3d->overlay.flag |= V3D_OVERLAY_VIEWER_ATTRIBUTE;
            v3d->overlay.viewer_attribute_opacity = 1.0f;
          }
          if (sl->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)sl;
            if (sima->flag & SI_FLAG_UNUSED_18) { /* Was #SI_CUSTOM_GRID. */
              sima->grid_shape_source = SI_GRID_SHAPE_FIXED;
              sima->flag &= ~SI_FLAG_UNUSED_18;
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_node_id(ntree, GEO_NODE_OFFSET_POINT_IN_CURVE, "GeometryNodeOffsetPointInCurve");
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 4)) {
    /* Update brush sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      brush->automasking_cavity_factor = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 5)) {
    /* Fix for #101622 - update flags of sequence editor regions that were not initialized
     * properly. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          if (sl->spacetype == SPACE_SEQ) {
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_TOOLS) {
                region->v2d.flag &= ~V2D_VIEWSYNC_AREA_VERTICAL;
              }
              if (region->regiontype == RGN_TYPE_CHANNELS) {
                region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 304, 6)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != GEO_NODE_SAMPLE_CURVE) {
          continue;
        }
        static_cast<NodeGeometryCurveSample *>(node->storage)->use_all_curves = true;
        static_cast<NodeGeometryCurveSample *>(node->storage)->data_type = CD_PROP_FLOAT;
        bNodeSocket *curve_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Curve");
        BLI_assert(curve_socket != nullptr);
        STRNCPY_UTF8(curve_socket->name, "Curves");
        STRNCPY_UTF8(curve_socket->identifier, "Curves");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 2)) {
    LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
      MovieTracking *tracking = &clip->tracking;

      const float frame_center_x = float(clip->lastsize[0]) / 2;
      const float frame_center_y = float(clip->lastsize[1]) / 2;

      tracking->camera.principal_point[0] = (tracking->camera.principal_legacy[0] -
                                             frame_center_x) /
                                            frame_center_x;
      tracking->camera.principal_point[1] = (tracking->camera.principal_legacy[1] -
                                             frame_center_y) /
                                            frame_center_y;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 4)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_COLLECTION_INFO, "Geometry", "Instances");
      }
    }

    /* UVSeam fixing distance. */
    if (!DNA_struct_member_exists(fd->filesdna, "Image", "short", "seam_margin")) {
      LISTBASE_FOREACH (Image *, image, &bmain->images) {
        image->seam_margin = 8;
      }
    }

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_primitive_uv_maps(*ntree);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 6)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.flag |= int(V3D_OVERLAY_SCULPT_SHOW_MASK |
                                     V3D_OVERLAY_SCULPT_SHOW_FACE_SETS);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 7)) {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->radius = light->area_size;
    }
    /* Grease Pencil Build modifier:
     * Set default value for new natural draw-speed factor and maximum gap. */
    if (!DNA_struct_member_exists(
            fd->filesdna, "BuildGpencilModifierData", "float", "speed_fac") ||
        !DNA_struct_member_exists(
            fd->filesdna, "BuildGpencilModifierData", "float", "speed_maxgap"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
          if (md->type == eGpencilModifierType_Build) {
            BuildGpencilModifierData *mmd = (BuildGpencilModifierData *)md;
            mmd->speed_fac = 1.2f;
            mmd->speed_maxgap = 0.5f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 8)) {
    const int CV_SCULPT_SELECTION_ENABLED = (1 << 1);
    LISTBASE_FOREACH (Curves *, curves_id, &bmain->hair_curves) {
      curves_id->flag &= ~CV_SCULPT_SELECTION_ENABLED;
    }
    LISTBASE_FOREACH (Curves *, curves_id, &bmain->hair_curves) {
      AttributeOwner owner = AttributeOwner::from_id(&curves_id->id);
      BKE_attribute_rename(owner, ".selection_point_float", ".selection", nullptr);
      BKE_attribute_rename(owner, ".selection_curve_float", ".selection", nullptr);
    }

    /* Toggle the Invert Vertex Group flag on Armature modifiers in some cases. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      bool after_armature = false;
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Armature) {
          ArmatureModifierData *amd = (ArmatureModifierData *)md;
          if (amd->multi) {
            /* Toggle the invert vertex group flag on operational Multi Modifier entries. */
            if (after_armature && amd->defgrp_name[0]) {
              amd->deformflag ^= ARM_DEF_INVERT_VGROUP;
            }
          }
          else {
            /* Disabled multi modifiers don't reset propagation, but non-multi ones do. */
            after_armature = false;
          }
          /* Multi Modifier is only valid and operational after an active Armature modifier. */
          if (md->mode & (eModifierMode_Realtime | eModifierMode_Render)) {
            after_armature = true;
          }
        }
        else if (ELEM(md->type, eModifierType_Lattice, eModifierType_MeshDeform)) {
          /* These modifiers will also allow a following Multi Modifier to work. */
          after_armature = (md->mode & (eModifierMode_Realtime | eModifierMode_Render)) != 0;
        }
        else {
          after_armature = false;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 9)) {
    /* Enable legacy normal and rotation outputs in Distribute Points on Faces node. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != GEO_NODE_DISTRIBUTE_POINTS_ON_FACES) {
          continue;
        }
        node->custom2 = true;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 305, 10)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_FILE) {
            continue;
          }
          SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
          if (!sfile->asset_params) {
            continue;
          }

          /* When an asset browser uses the default import method, make it follow the new
           * preference setting. This means no effective default behavior change. */
          if (sfile->asset_params->import_method == FILE_ASSET_IMPORT_APPEND_REUSE) {
            sfile->asset_params->import_method = FILE_ASSET_IMPORT_FOLLOW_PREFS;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "int", "shadow_pool_size")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.flag |= SCE_EEVEE_SHADOW_ENABLED;
        scene->eevee.shadow_pool_size = 512;
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.flag |= V3D_OVERLAY_SCULPT_CURVES_CAGE;
            v3d->overlay.sculpt_curves_cage_opacity = 0.5f;
          }
        }
      }
    }

    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode == OB_MODE_SCULPT_CURVES) {
        if (brush->curves_sculpt_settings->curve_parameter_falloff == nullptr) {
          brush->curves_sculpt_settings->curve_parameter_falloff = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 3)) {
    /* Z bias for retopology overlay. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "retopology_offset")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.retopology_offset = 0.2f;
            }
          }
        }
      }
    }

    /* Use `SEQ_SINGLE_FRAME_CONTENT` flag instead of weird function to check if strip has multiple
     * frames. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, version_set_seq_single_frame_content, nullptr);
      }
    }

    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_extrude_smooth_propagation(*ntree);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 5)) {
    /* Some regions used to be added/removed dynamically. Ensure they are always there, there is a
     * `ARegionType.poll()` now. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          version_ensure_missing_regions(area, sl);

          /* Ensure expected region state. Previously this was modified to hide/unhide regions. */

          const ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                       &sl->regionbase;
          if (sl->spacetype == SPACE_SEQ) {
            ARegion *region_main = BKE_region_find_in_listbase_by_type(regionbase,
                                                                       RGN_TYPE_WINDOW);
            region_main->flag &= ~RGN_FLAG_HIDDEN;
            region_main->alignment = RGN_ALIGN_NONE;

            ARegion *region_preview = BKE_region_find_in_listbase_by_type(regionbase,
                                                                          RGN_TYPE_PREVIEW);
            region_preview->flag &= ~RGN_FLAG_HIDDEN;
            region_preview->alignment = RGN_ALIGN_NONE;

            ARegion *region_channels = BKE_region_find_in_listbase_by_type(regionbase,
                                                                           RGN_TYPE_CHANNELS);
            region_channels->alignment = RGN_ALIGN_LEFT;
          }
        }
      }

      /* Replace old hard coded names with brush names, see: #106057. */
      const char *tool_replace_table[][2] = {
          {"selection_paint", "Paint Selection"},
          {"add", "Add"},
          {"delete", "Delete"},
          {"density", "Density"},
          {"comb", "Comb"},
          {"snake_hook", "Snake Hook"},
          {"grow_shrink", "Grow / Shrink"},
          {"pinch", "Pinch"},
          {"puff", "Puff"},
          {"smooth", "Comb"},
          {"slide", "Slide"},
      };
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        BKE_workspace_tool_id_replace_table(workspace,
                                            SPACE_VIEW3D,
                                            CTX_MODE_SCULPT_CURVES,
                                            "builtin_brush.",
                                            tool_replace_table,
                                            ARRAY_SIZE(tool_replace_table));
      }
    }

    /* Rename Grease Pencil weight draw brush. */
    do_versions_rename_id(bmain, ID_BR, "Draw Weight", "Weight Draw");
  }

  /* `fcm->name` was never used to store modifier name so it has always been an empty string.
   * Now this property supports name editing. So assign value to name variable of F-modifier
   * otherwise modifier interface would show an empty name field.
   * Also ensure uniqueness when opening old files. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 7)) {
    LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
      LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
        LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
          BKE_fmodifier_name_set(fcm, "");
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 8)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      ob->flag |= OB_FLAG_USE_SIMULATION_CACHE;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 9)) {
    /* Fix sound strips with speed factor set to 0. See #107289. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = blender::seq::editing_get(scene);
      if (ed != nullptr) {
        blender::seq::foreach_strip(&ed->seqbase, version_seq_fix_broken_sound_strips, nullptr);
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);
            saction->cache_display |= TIME_CACHE_SIMULATION_NODES;
          }
        }
      }
    }

    /* Enable the iTaSC ITASC_TRANSLATE_ROOT_BONES flag for backward compatibility.
     * See #104606. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->type != OB_ARMATURE || ob->pose == nullptr) {
        continue;
      }
      bPose *pose = ob->pose;
      if (pose->iksolver != IKSOLVER_ITASC || pose->ikparam == nullptr) {
        continue;
      }
      bItasc *ikparam = (bItasc *)pose->ikparam;
      ikparam->flag |= ITASC_TRANSLATE_ROOT_BONES;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 10)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Set default values for new members. */
      short snap_mode_geom = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 5);
      scene->toolsettings->snap_mode_tools = snap_mode_geom;
      scene->toolsettings->plane_axis = 2;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 306, 11)) {
    BKE_animdata_main_cb(bmain, version_liboverride_nla_frame_start_end);

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          /* #107870: Movie Clip Editor hangs in "Clip" view */
          if (sl->spacetype == SPACE_CLIP) {
            const ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                         &sl->regionbase;
            ARegion *region_main = BKE_region_find_in_listbase_by_type(regionbase,
                                                                       RGN_TYPE_WINDOW);
            region_main->flag &= ~RGN_FLAG_HIDDEN;
            ARegion *region_tools = BKE_region_find_in_listbase_by_type(regionbase,
                                                                        RGN_TYPE_TOOLS);
            region_tools->alignment = RGN_ALIGN_LEFT;
            if (!(region_tools->flag & RGN_FLAG_HIDDEN_BY_USER)) {
              region_tools->flag &= ~RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        version_node_socket_name(ntree, CMP_NODE_LENSDIST, "Distort", "Distortion");
      }
    }
    FOREACH_NODETREE_END;
  }

  {
    /* Keep this block, even when empty. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->toolsettings->uvcalc_iterations = 10;
      scene->toolsettings->uvcalc_weight_factor = 1.0f;
      STRNCPY_UTF8(scene->toolsettings->uvcalc_weight_group, "uv_importance");
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}
