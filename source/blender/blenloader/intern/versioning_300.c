/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */
/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_lineart_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_workspace_types.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_asset.h"
#include "BKE_attribute.h"
#include "BKE_collection.h"
#include "BKE_curve.h"
#include "BKE_data_transfer.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "BLO_readfile.h"

#include "readfile.h"

#include "SEQ_channels.h"
#include "SEQ_iterator.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "versioning_common.h"

static CLG_LogRef LOG = {"blo.readfile.doversion"};

static IDProperty *idproperty_find_ui_container(IDProperty *idprop_group)
{
  LISTBASE_FOREACH (IDProperty *, prop, &idprop_group->data.group) {
    if (prop->type == IDP_GROUP && STREQ(prop->name, "_RNA_UI")) {
      return prop;
    }
  }
  return NULL;
}

static void version_idproperty_move_data_int(IDPropertyUIDataInt *ui_data,
                                             const IDProperty *prop_ui_data)
{
  IDProperty *min = IDP_GetPropertyFromGroup(prop_ui_data, "min");
  if (min != NULL) {
    ui_data->min = ui_data->soft_min = IDP_coerce_to_int_or_zero(min);
  }
  IDProperty *max = IDP_GetPropertyFromGroup(prop_ui_data, "max");
  if (max != NULL) {
    ui_data->max = ui_data->soft_max = IDP_coerce_to_int_or_zero(max);
  }
  IDProperty *soft_min = IDP_GetPropertyFromGroup(prop_ui_data, "soft_min");
  if (soft_min != NULL) {
    ui_data->soft_min = IDP_coerce_to_int_or_zero(soft_min);
    ui_data->soft_min = MIN2(ui_data->soft_min, ui_data->min);
  }
  IDProperty *soft_max = IDP_GetPropertyFromGroup(prop_ui_data, "soft_max");
  if (soft_max != NULL) {
    ui_data->soft_max = IDP_coerce_to_int_or_zero(soft_max);
    ui_data->soft_max = MAX2(ui_data->soft_max, ui_data->max);
  }
  IDProperty *step = IDP_GetPropertyFromGroup(prop_ui_data, "step");
  if (step != NULL) {
    ui_data->step = IDP_coerce_to_int_or_zero(soft_max);
  }
  IDProperty *default_value = IDP_GetPropertyFromGroup(prop_ui_data, "default");
  if (default_value != NULL) {
    if (default_value->type == IDP_ARRAY) {
      if (default_value->subtype == IDP_INT) {
        ui_data->default_array = MEM_malloc_arrayN(default_value->len, sizeof(int), __func__);
        memcpy(ui_data->default_array, IDP_Array(default_value), sizeof(int) * default_value->len);
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
  if (min != NULL) {
    ui_data->min = ui_data->soft_min = IDP_coerce_to_double_or_zero(min);
  }
  IDProperty *max = IDP_GetPropertyFromGroup(prop_ui_data, "max");
  if (max != NULL) {
    ui_data->max = ui_data->soft_max = IDP_coerce_to_double_or_zero(max);
  }
  IDProperty *soft_min = IDP_GetPropertyFromGroup(prop_ui_data, "soft_min");
  if (soft_min != NULL) {
    ui_data->soft_min = IDP_coerce_to_double_or_zero(soft_min);
    ui_data->soft_min = MAX2(ui_data->soft_min, ui_data->min);
  }
  IDProperty *soft_max = IDP_GetPropertyFromGroup(prop_ui_data, "soft_max");
  if (soft_max != NULL) {
    ui_data->soft_max = IDP_coerce_to_double_or_zero(soft_max);
    ui_data->soft_max = MIN2(ui_data->soft_max, ui_data->max);
  }
  IDProperty *step = IDP_GetPropertyFromGroup(prop_ui_data, "step");
  if (step != NULL) {
    ui_data->step = IDP_coerce_to_float_or_zero(step);
  }
  IDProperty *precision = IDP_GetPropertyFromGroup(prop_ui_data, "precision");
  if (precision != NULL) {
    ui_data->precision = IDP_coerce_to_int_or_zero(precision);
  }
  IDProperty *default_value = IDP_GetPropertyFromGroup(prop_ui_data, "default");
  if (default_value != NULL) {
    if (default_value->type == IDP_ARRAY) {
      const int array_len = default_value->len;
      ui_data->default_array_len = array_len;
      if (default_value->subtype == IDP_FLOAT) {
        ui_data->default_array = MEM_malloc_arrayN(array_len, sizeof(double), __func__);
        const float *old_default_array = IDP_Array(default_value);
        for (int i = 0; i < ui_data->default_array_len; i++) {
          ui_data->default_array[i] = (double)old_default_array[i];
        }
      }
      else if (default_value->subtype == IDP_DOUBLE) {
        ui_data->default_array = MEM_malloc_arrayN(array_len, sizeof(double), __func__);
        memcpy(ui_data->default_array, IDP_Array(default_value), sizeof(double) * array_len);
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
  if (default_value != NULL && default_value->type == IDP_STRING) {
    ui_data->default_value = BLI_strdup(IDP_String(default_value));
  }
}

static void version_idproperty_ui_data(IDProperty *idprop_group)
{
  if (idprop_group == NULL) { /* NULL check here to reduce verbosity of calls to this function. */
    return;
  }

  IDProperty *ui_container = idproperty_find_ui_container(idprop_group);
  if (ui_container == NULL) {
    return;
  }

  LISTBASE_FOREACH (IDProperty *, prop, &idprop_group->data.group) {
    IDProperty *prop_ui_data = IDP_GetPropertyFromGroup(ui_container, prop->name);
    if (prop_ui_data == NULL) {
      continue;
    }

    if (!IDP_ui_data_supported(prop)) {
      continue;
    }

    IDPropertyUIData *ui_data = IDP_ui_data_ensure(prop);

    IDProperty *subtype = IDP_GetPropertyFromGroup(prop_ui_data, "subtype");
    if (subtype != NULL && subtype->type == IDP_STRING) {
      const char *subtype_string = IDP_String(subtype);
      int result = PROP_NONE;
      RNA_enum_value_from_id(rna_enum_property_subtype_items, subtype_string, &result);
      ui_data->rna_subtype = result;
    }

    IDProperty *description = IDP_GetPropertyFromGroup(prop_ui_data, "description");
    if (description != NULL && description->type == IDP_STRING) {
      ui_data->description = BLI_strdup(IDP_String(description));
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
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    version_idproperty_ui_data(seq->prop);
    if (seq->type == SEQ_TYPE_META) {
      do_versions_idproperty_seq_recursive(&seq->seqbase);
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
    IDProperty *idprop_group = IDP_GetProperties(id, false);
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
    LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->inputs) {
      version_idproperty_ui_data(socket->prop);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &ntree->outputs) {
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
    if (ob->type == OB_ARMATURE && ob->pose != NULL) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        version_idproperty_ui_data(pchan->prop);
      }
    }
  }

  /* Sequences. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != NULL) {
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
        id_sort_by_name(&temp_list, id, NULL);
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
    ID *id_prev = NULL;
    LISTBASE_FOREACH (ID *, id, lb) {
      if (id_prev == NULL) {
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
    if (ELEM(object->type, OB_MESH, OB_LATTICE, OB_GPENCIL)) {
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
#define SEQ_SPEED_INTEGRATE (1 << 0)
#define SEQ_SPEED_COMPRESS_IPO_Y (1 << 2)

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type == SEQ_TYPE_SPEED) {
      SpeedControlVars *v = (SpeedControlVars *)seq->effectdata;
      const char *substr = NULL;
      float globalSpeed = v->globalSpeed;
      if (seq->flag & SEQ_USE_EFFECT_DEFAULT_FADE) {
        if (globalSpeed == 1.0f) {
          v->speed_control_type = SEQ_SPEED_STRETCH;
        }
        else {
          v->speed_control_type = SEQ_SPEED_MULTIPLY;
          v->speed_fader = globalSpeed *
                           ((float)seq->seq1->len /
                            max_ff((float)(SEQ_time_right_handle_frame_get(seq->seq1) -
                                           seq->seq1->start),
                                   1.0f));
        }
      }
      else if (v->flags & SEQ_SPEED_INTEGRATE) {
        v->speed_control_type = SEQ_SPEED_MULTIPLY;
        v->speed_fader = seq->speed_fader * globalSpeed;
      }
      else if (v->flags & SEQ_SPEED_COMPRESS_IPO_Y) {
        globalSpeed *= 100.0f;
        v->speed_control_type = SEQ_SPEED_LENGTH;
        v->speed_fader_length = seq->speed_fader * globalSpeed;
        substr = "speed_length";
      }
      else {
        v->speed_control_type = SEQ_SPEED_FRAME_NUMBER;
        v->speed_fader_frame_number = (int)(seq->speed_fader * globalSpeed);
        substr = "speed_frame_number";
      }

      v->flags &= ~(SEQ_SPEED_INTEGRATE | SEQ_SPEED_COMPRESS_IPO_Y);

      if (substr || globalSpeed != 1.0f) {
        FCurve *fcu = id_data_find_fcurve(&scene->id, seq, &RNA_Sequence, "speed_factor", 0, NULL);
        if (fcu) {
          if (globalSpeed != 1.0f) {
            for (int i = 0; i < fcu->totvert; i++) {
              BezTriple *bezt = &fcu->bezt[i];
              bezt->vec[0][1] *= globalSpeed;
              bezt->vec[1][1] *= globalSpeed;
              bezt->vec[2][1] *= globalSpeed;
            }
          }
          if (substr) {
            char *new_path = BLI_str_replaceN(fcu->rna_path, "speed_factor", substr);
            MEM_freeN(fcu->rna_path);
            fcu->rna_path = new_path;
          }
        }
      }
    }
    else if (seq->type == SEQ_TYPE_META) {
      do_versions_sequencer_speed_effect_recursive(scene, &seq->seqbase);
    }
  }

#undef SEQ_SPEED_INTEGRATE
#undef SEQ_SPEED_COMPRESS_IPO_Y
}

static bool do_versions_sequencer_color_tags(Sequence *seq, void *UNUSED(user_data))
{
  seq->color_tag = SEQUENCE_COLOR_NONE;
  return true;
}

static bool do_versions_sequencer_color_balance_sop(Sequence *seq, void *UNUSED(user_data))
{
  LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
    if (smd->type == seqModifierType_ColorBalance) {
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

static bNodeLink *find_connected_link(bNodeTree *ntree, bNodeSocket *in_socket)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tosock == in_socket) {
      return link;
    }
  }
  return NULL;
}

static void add_realize_instances_before_socket(bNodeTree *ntree,
                                                bNode *node,
                                                bNodeSocket *geometry_socket)
{
  BLI_assert(geometry_socket->type == SOCK_GEOMETRY);
  bNodeLink *link = find_connected_link(ntree, geometry_socket);
  if (link == NULL) {
    return;
  }

  /* If the realize instances node is already before this socket, no need to continue. */
  if (link->fromnode->type == GEO_NODE_REALIZE_INSTANCES) {
    return;
  }

  bNode *realize_node = nodeAddStaticNode(NULL, ntree, GEO_NODE_REALIZE_INSTANCES);
  realize_node->parent = node->parent;
  realize_node->locx = node->locx - 100;
  realize_node->locy = node->locy;
  nodeAddLink(ntree, link->fromnode, link->fromsock, realize_node, realize_node->inputs.first);
  link->fromnode = realize_node;
  link->fromsock = realize_node->outputs.first;
}

/**
 * If a node used to realize instances implicitly and will no longer do so in 3.0, add a "Realize
 * Instances" node in front of it to avoid changing behavior. Don't do this if the node will be
 * replaced anyway though.
 */
static void version_geometry_nodes_add_realize_instance_nodes(bNodeTree *ntree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
    if (ELEM(node->type,
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
             GEO_NODE_TRIANGULATE)) {
      bNodeSocket *geometry_socket = node->inputs.first;
      add_realize_instances_before_socket(ntree, node, geometry_socket);
    }
    /* Also realize instances for the profile input of the curve to mesh node. */
    if (node->type == GEO_NODE_CURVE_TO_MESH) {
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
  bNodeTree *node_tree = ntreeAddTree(bmain, "Realize Instances 2.93 Legacy", "GeometryNodeTree");

  ntreeAddSocketInterface(node_tree, SOCK_IN, "NodeSocketGeometry", "Geometry");
  ntreeAddSocketInterface(node_tree, SOCK_OUT, "NodeSocketGeometry", "Geometry");

  bNode *group_input = nodeAddStaticNode(NULL, node_tree, NODE_GROUP_INPUT);
  group_input->locx = -400.0f;
  bNode *group_output = nodeAddStaticNode(NULL, node_tree, NODE_GROUP_OUTPUT);
  group_output->locx = 500.0f;
  group_output->flag |= NODE_DO_OUTPUT;

  bNode *join = nodeAddStaticNode(NULL, node_tree, GEO_NODE_JOIN_GEOMETRY);
  join->locx = group_output->locx - 175.0f;
  join->locy = group_output->locy;
  bNode *conv = nodeAddStaticNode(NULL, node_tree, GEO_NODE_POINTS_TO_VERTICES);
  conv->locx = join->locx - 175.0f;
  conv->locy = join->locy - 70.0;
  bNode *separate = nodeAddStaticNode(NULL, node_tree, GEO_NODE_SEPARATE_COMPONENTS);
  separate->locx = join->locx - 350.0f;
  separate->locy = join->locy + 50.0f;
  bNode *realize = nodeAddStaticNode(NULL, node_tree, GEO_NODE_REALIZE_INSTANCES);
  realize->locx = separate->locx - 200.0f;
  realize->locy = join->locy;

  nodeAddLink(node_tree, group_input, group_input->outputs.first, realize, realize->inputs.first);
  nodeAddLink(node_tree, realize, realize->outputs.first, separate, separate->inputs.first);
  nodeAddLink(node_tree, conv, conv->outputs.first, join, join->inputs.first);
  nodeAddLink(node_tree, separate, BLI_findlink(&separate->outputs, 3), join, join->inputs.first);
  nodeAddLink(node_tree, separate, BLI_findlink(&separate->outputs, 1), conv, conv->inputs.first);
  nodeAddLink(node_tree, separate, BLI_findlink(&separate->outputs, 2), join, join->inputs.first);
  nodeAddLink(node_tree, separate, separate->outputs.first, join, join->inputs.first);
  nodeAddLink(node_tree, join, join->outputs.first, group_output, group_output->inputs.first);

  LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
    nodeSetSelected(node, false);
  }

  version_socket_update_is_used(node_tree);
  return node_tree;
}

void do_versions_after_linking_300(Main *bmain, ReportList *UNUSED(reports))
{
  if (MAIN_VERSION_ATLEAST(bmain, 300, 0) && !MAIN_VERSION_ATLEAST(bmain, 300, 1)) {
    /* Set zero user text objects to have a fake user. */
    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      if (text->id.us == 0) {
        id_fake_user_set(&text->id);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 3)) {
    sort_linked_ids(bmain);
    assert_sorted_ids(bmain);
  }

  if (MAIN_VERSION_ATLEAST(bmain, 300, 3)) {
    assert_sorted_ids(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 11)) {
    move_vertex_group_names_to_object_data(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 13)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        do_versions_sequencer_speed_effect_recursive(scene, &scene->ed->seqbase);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 25)) {
    version_node_socket_index_animdata(bmain, NTREE_SHADER, SH_NODE_BSDF_PRINCIPLED, 4, 2, 25);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 26)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      ImagePaintSettings *imapaint = &tool_settings->imapaint;
      if (imapaint->canvas != NULL &&
          ELEM(imapaint->canvas->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
        imapaint->canvas = NULL;
      }
      if (imapaint->stencil != NULL &&
          ELEM(imapaint->stencil->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
        imapaint->stencil = NULL;
      }
      if (imapaint->clone != NULL &&
          ELEM(imapaint->clone->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
        imapaint->clone = NULL;
      }
    }

    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->clone.image != NULL &&
          ELEM(brush->clone.image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
        brush->clone.image = NULL;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 28)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_geometry_nodes_add_realize_instance_nodes(ntree);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 30)) {
    do_versions_idproperty_ui_data(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 32)) {
    /* Update Switch Node Non-Fields switch input to Switch_001. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }

      LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
        if (link->tonode->type == GEO_NODE_SWITCH) {
          if (STREQ(link->tosock->identifier, "Switch")) {
            bNode *to_node = link->tonode;

            uint8_t mode = ((NodeSwitch *)to_node->storage)->input_type;
            if (ELEM(mode,
                     SOCK_GEOMETRY,
                     SOCK_OBJECT,
                     SOCK_COLLECTION,
                     SOCK_TEXTURE,
                     SOCK_MATERIAL)) {
              link->tosock = link->tosock->next;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 33)) {
    /* This was missing from #move_vertex_group_names_to_object_data. */
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      if (ELEM(object->type, OB_MESH, OB_LATTICE, OB_GPENCIL)) {
        /* This uses the fact that the active vertex group index starts counting at 1. */
        if (BKE_object_defgroup_active_index_get(object) == 0) {
          BKE_object_defgroup_active_index_set(object, object->actdef);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 35)) {
    /* Add a new modifier to realize instances from previous modifiers.
     * Previously that was done automatically by geometry nodes. */
    bNodeTree *realize_instances_node_tree = NULL;
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH_MUTABLE (ModifierData *, md, &ob->modifiers) {
        if (md->type != eModifierType_Nodes) {
          continue;
        }
        if (md->next == NULL) {
          break;
        }
        if (md->next->type == eModifierType_Nodes) {
          continue;
        }
        NodesModifierData *nmd = (NodesModifierData *)md;
        if (nmd->node_group == NULL) {
          continue;
        }

        NodesModifierData *new_nmd = (NodesModifierData *)BKE_modifier_new(eModifierType_Nodes);
        STRNCPY(new_nmd->modifier.name, "Realize Instances 2.93 Legacy");
        BKE_modifier_unique_name(&ob->modifiers, &new_nmd->modifier);
        BLI_insertlinkafter(&ob->modifiers, md, new_nmd);
        if (realize_instances_node_tree == NULL) {
          realize_instances_node_tree = add_realize_node_tree(bmain);
        }
        new_nmd->node_group = realize_instances_node_tree;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 37)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
          if (node->type == GEO_NODE_BOUNDING_BOX) {
            bNodeSocket *geometry_socket = node->inputs.first;
            add_realize_instances_before_socket(ntree, node, geometry_socket);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 301, 6)) {
    { /* Ensure driver variable names are unique within the driver. */
      ID *id;
      FOREACH_MAIN_ID_BEGIN (bmain, id) {
        AnimData *adt = BKE_animdata_from_id(id);
        if (adt == NULL) {
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
        char *filename = (char *)BLI_path_basename(ima->filepath);
        BKE_image_ensure_tile_token(filename);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 14)) {
    /* Sequencer channels region. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_SEQ) {
            continue;
          }
          SpaceSeq *sseq = (SpaceSeq *)sl;
          sseq->flag |= SEQ_CLAMP_VIEW;

          if (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
            continue;
          }

          ARegion *timeline_region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

          if (timeline_region == NULL) {
            continue;
          }

          timeline_region->v2d.cur.ymax = 8.5f;
          timeline_region->v2d.align &= ~V2D_ALIGN_NO_NEG_Y;
        }
      }
    }
  }
  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_300 in this file.
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}

static void version_switch_node_input_prefix(Main *bmain)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    if (ntree->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_SWITCH) {
          LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
            /* Skip the "switch" socket. */
            if (socket == node->inputs.first) {
              continue;
            }
            strcpy(socket->name, socket->name[0] == 'A' ? "False" : "True");

            /* Replace "A" and "B", but keep the unique number suffix at the end. */
            char number_suffix[8];
            BLI_strncpy(number_suffix, socket->identifier + 1, sizeof(number_suffix));
            strcpy(socket->identifier, socket->name);
            strcat(socket->identifier, number_suffix);
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

  if (old_path == NULL) {
    return false;
  }

  int len = strlen(old_path);

  if (BLI_str_endswith(old_path, ".bbone_curveiny") ||
      BLI_str_endswith(old_path, ".bbone_curveouty")) {
    old_path[len - 1] = 'z';
    return true;
  }

  if (BLI_str_endswith(old_path, ".bbone_scaleinx") ||
      BLI_str_endswith(old_path, ".bbone_scaleiny") ||
      BLI_str_endswith(old_path, ".bbone_scaleoutx") ||
      BLI_str_endswith(old_path, ".bbone_scaleouty")) {
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
        replace_bbone_len_scale_rnapath(&dtar->rna_path, NULL);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  /* Update F-Curve's path. */
  replace_bbone_len_scale_rnapath(&fcu->rna_path, &fcu->array_index);
}

static void do_version_bbone_len_scale_animdata_cb(ID *UNUSED(id),
                                                   AnimData *adt,
                                                   void *UNUSED(wrapper_data))
{
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &adt->drivers) {
    do_version_bbone_len_scale_fcurve_fix(fcu);
  }
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
      if (data->points == NULL) {
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
  nodeRemoveSocket(ntree, node, socket);
  bNodeSocket *new_socket = nodeAddSocket(
      ntree, node, SOCK_IN, nodeStaticSocketType(SOCK_VECTOR, PROP_TRANSLATION), "Size", "Size");
  bNodeSocketValueVector *value_vector = (bNodeSocketValueVector *)new_socket->default_value;
  copy_v3_fl(value_vector->value, old_value);
  return new_socket;
}

static bool seq_transform_origin_set(Sequence *seq, void *UNUSED(user_data))
{
  StripTransform *transform = seq->strip->transform;
  if (seq->strip->transform != NULL) {
    transform->origin[0] = transform->origin[1] = 0.5f;
  }
  return true;
}

static bool seq_transform_filter_set(Sequence *seq, void *UNUSED(user_data))
{
  StripTransform *transform = seq->strip->transform;
  if (seq->strip->transform != NULL) {
    transform->filter = SEQ_TRANSFORM_FILTER_BILINEAR;
  }
  return true;
}

static bool seq_meta_channels_ensure(Sequence *seq, void *UNUSED(user_data))
{
  if (seq->type == SEQ_TYPE_META) {
    SEQ_channels_ensure(&seq->channels);
  }
  return true;
}

static void do_version_subsurface_methods(bNode *node)
{
  if (node->type == SH_NODE_SUBSURFACE_SCATTERING) {
    if (!ELEM(node->custom1, SHD_SUBSURFACE_BURLEY, SHD_SUBSURFACE_RANDOM_WALK)) {
      node->custom1 = SHD_SUBSURFACE_RANDOM_WALK_FIXED_RADIUS;
    }
  }
  else if (node->type == SH_NODE_BSDF_PRINCIPLED) {
    if (!ELEM(node->custom2, SHD_SUBSURFACE_BURLEY, SHD_SUBSURFACE_RANDOM_WALK)) {
      node->custom2 = SHD_SUBSURFACE_RANDOM_WALK_FIXED_RADIUS;
    }
  }
}

static void version_geometry_nodes_add_attribute_input_settings(NodesModifierData *nmd)
{
  if (nmd->settings.properties == NULL) {
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
    BLI_snprintf(use_attribute_prop_name,
                 sizeof(use_attribute_prop_name),
                 "%s%s",
                 property->name,
                 "_use_attribute");

    IDPropertyTemplate idprop = {0};
    IDProperty *use_attribute_prop = IDP_New(IDP_INT, &idprop, use_attribute_prop_name);
    IDP_AddToGroup(nmd->settings.properties, use_attribute_prop);

    char attribute_name_prop_name[MAX_IDPROP_NAME];
    BLI_snprintf(attribute_name_prop_name,
                 sizeof(attribute_name_prop_name),
                 "%s%s",
                 property->name,
                 "_attribute_name");

    IDProperty *attribute_prop = IDP_New(IDP_STRING, &idprop, attribute_name_prop_name);
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
    if (node->type != GEO_NODE_SET_POSITION) {
      continue;
    }
    if (BLI_listbase_count(&node->inputs) < 4) {
      /* The offset socket didn't exist in the file yet. */
      return;
    }
    bNodeSocket *old_offset_socket = BLI_findlink(&node->inputs, 3);
    if (old_offset_socket->type == SOCK_VECTOR) {
      /* Versioning happened already. */
      return;
    }
    /* Change identifier of old socket, so that the there is no name collision. */
    STRNCPY(old_offset_socket->identifier, "Offset_old");
    nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_VECTOR, PROP_TRANSLATION, "Offset", "Offset");
  }

  /* Relink links that were connected to Position while Offset was enabled. */
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tonode->type != GEO_NODE_SET_POSITION) {
      continue;
    }
    if (!STREQ(link->tosock->identifier, "Position")) {
      continue;
    }
    bNodeSocket *old_offset_socket = BLI_findlink(&link->tonode->inputs, 3);
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
    if (node->type != GEO_NODE_SET_POSITION) {
      continue;
    }
    bNodeSocket *old_offset_socket = BLI_findlink(&node->inputs, 3);
    nodeRemoveSocket(ntree, node, old_offset_socket);
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

static bool version_fix_seq_meta_range(Sequence *seq, void *user_data)
{
  Scene *scene = (Scene *)user_data;
  if (seq->type == SEQ_TYPE_META) {
    SEQ_time_update_meta_strip_range(scene, seq);
  }
  return true;
}

static bool version_merge_still_offsets(Sequence *seq, void *UNUSED(user_data))
{
  seq->startofs -= seq->startstill;
  seq->endofs -= seq->endstill;
  seq->startstill = 0;
  seq->endstill = 0;
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
    if (opop->operation != IDOVERRIDE_LIBRARY_OP_INSERT_AFTER) {
      continue;
    }
    bConstraint *constraint_anchor = BLI_listbase_string_or_index_find(constraints,
                                                                       opop->subitem_local_name,
                                                                       offsetof(bConstraint, name),
                                                                       opop->subitem_local_index);
    bConstraint *constraint_src = constraint_anchor != NULL ? constraint_anchor->next :
                                                              constraints->first;

    if (constraint_src == NULL) {
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
  if (op != NULL) {
    LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != IDOVERRIDE_LIBRARY_OP_INSERT_AFTER) {
        continue;
      }
      ModifierData *mod_anchor = BLI_listbase_string_or_index_find(&object->modifiers,
                                                                   opop->subitem_local_name,
                                                                   offsetof(ModifierData, name),
                                                                   opop->subitem_local_index);
      ModifierData *mod_src = mod_anchor != NULL ? mod_anchor->next : object->modifiers.first;

      if (mod_src == NULL) {
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
  if (op != NULL) {
    LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != IDOVERRIDE_LIBRARY_OP_INSERT_AFTER) {
        continue;
      }
      GpencilModifierData *gp_mod_anchor = BLI_listbase_string_or_index_find(
          &object->greasepencil_modifiers,
          opop->subitem_local_name,
          offsetof(GpencilModifierData, name),
          opop->subitem_local_index);
      GpencilModifierData *gp_mod_src = gp_mod_anchor != NULL ?
                                            gp_mod_anchor->next :
                                            object->greasepencil_modifiers.first;

      if (gp_mod_src == NULL) {
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
  if (op != NULL) {
    version_liboverride_rnacollections_insertion_object_constraints(&object->constraints, op);
  }

  if (object->pose != NULL) {
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      char rna_path[26 + (sizeof(pchan->name) * 2) + 1];
      char name_esc[sizeof(pchan->name) * 2];
      BLI_str_escape(name_esc, pchan->name, sizeof(name_esc));
      SNPRINTF(rna_path, "pose.bones[\"%s\"].constraints", name_esc);
      op = BKE_lib_override_library_property_find(liboverride, rna_path);
      if (op != NULL) {
        version_liboverride_rnacollections_insertion_object_constraints(&pchan->constraints, op);
      }
    }
  }
}

static void version_liboverride_rnacollections_insertion_animdata(ID *id)
{
  AnimData *anim_data = BKE_animdata_from_id(id);
  if (anim_data == NULL) {
    return;
  }

  IDOverrideLibrary *liboverride = id->override_library;
  IDOverrideLibraryProperty *op;

  op = BKE_lib_override_library_property_find(liboverride, "animation_data.nla_tracks");
  if (op != NULL) {
    LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if (opop->operation != IDOVERRIDE_LIBRARY_OP_INSERT_AFTER) {
        continue;
      }
      /* NLA tracks are only referenced by index, which limits possibilities, basically they are
       * always added at the end of the list, see #rna_NLA_tracks_override_apply.
       *
       * This makes things simple here. */
      opop->subitem_reference_name = opop->subitem_local_name;
      opop->subitem_local_name = NULL;
      opop->subitem_reference_index = opop->subitem_local_index;
      opop->subitem_local_index++;
    }
  }
}

static void versioning_replace_legacy_combined_and_separate_color_nodes(bNodeTree *ntree)
{
  /* In geometry nodes, replace shader combine/separate color nodes with function nodes */
  if (ntree->type == NTREE_GEOMETRY) {
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "Image", "Color");

    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "Image", "Color");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type) {
        case SH_NODE_COMBRGB_LEGACY: {
          node->type = FN_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "FunctionNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPRGB_LEGACY: {
          node->type = FN_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "FunctionNodeSeparateColor");
          node->storage = storage;
          break;
        }
      }
    }
  }

  /* In compositing nodes, replace combine/separate RGBA/HSVA/YCbCrA/YCCA nodes with
   * combine/separate color */
  if (ntree->type == NTREE_COMPOSIT) {
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "H", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "S", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "V", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Y", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Cb", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Cr", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "Y", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "U", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "V", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "H", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "S", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "V", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Y", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Cb", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Cr", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "Y", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "U", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "V", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "A", "Alpha");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type) {
        case CMP_NODE_COMBRGBA_LEGACY: {
          node->type = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBHSVA_LEGACY: {
          node->type = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_HSV;
          strcpy(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBYCCA_LEGACY: {
          node->type = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YCC;
          storage->ycc_mode = node->custom1;
          strcpy(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBYUVA_LEGACY: {
          node->type = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YUV;
          strcpy(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPRGBA_LEGACY: {
          node->type = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPHSVA_LEGACY: {
          node->type = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_HSV;
          strcpy(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPYCCA_LEGACY: {
          node->type = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YCC;
          storage->ycc_mode = node->custom1;
          strcpy(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPYUVA_LEGACY: {
          node->type = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)MEM_callocN(
              sizeof(NodeCMPCombSepColor), __func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YUV;
          strcpy(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
      }
    }
  }

  /* In texture nodes, replace combine/separate RGBA with combine/separate color */
  if (ntree->type == NTREE_TEXTURE) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type) {
        case TEX_NODE_COMPOSE_LEGACY: {
          node->type = TEX_NODE_COMBINE_COLOR;
          node->custom1 = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "TextureNodeCombineColor");
          break;
        }
        case TEX_NODE_DECOMPOSE_LEGACY: {
          node->type = TEX_NODE_SEPARATE_COLOR;
          node->custom1 = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "TextureNodeSeparateColor");
          break;
        }
      }
    }
  }

  /* In shader nodes, replace combine/separate RGB/HSV with combine/separate color */
  if (ntree->type == NTREE_SHADER) {
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "Image", "Color");

    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "H", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "S", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "V", "Blue");

    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "Image", "Color");

    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "H", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "S", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "V", "Blue");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type) {
        case SH_NODE_COMBRGB_LEGACY: {
          node->type = SH_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "ShaderNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_COMBHSV_LEGACY: {
          node->type = SH_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_HSV;
          strcpy(node->idname, "ShaderNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPRGB_LEGACY: {
          node->type = SH_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          strcpy(node->idname, "ShaderNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPHSV_LEGACY: {
          node->type = SH_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = (NodeCombSepColor *)MEM_callocN(sizeof(NodeCombSepColor),
                                                                      __func__);
          storage->mode = NODE_COMBSEP_COLOR_HSV;
          strcpy(node->idname, "ShaderNodeSeparateColor");
          node->storage = storage;
          break;
        }
      }
    }
  }
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_300(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  /* The #SCE_SNAP_SEQ flag has been removed in favor of the #SCE_SNAP which can be used for each
   * snap_flag member individually. */
  enum { SCE_SNAP_SEQ = (1 << 7) };

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 1)) {
    /* Set default value for the new bisect_threshold parameter in the mirror modifier. */
    if (!DNA_struct_elem_find(fd->filesdna, "MirrorModifierData", "float", "bisect_threshold")) {
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
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "int", "dilate_pixels")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->gpencil_settings) {
          brush->gpencil_settings->dilate_pixels = 1;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 2)) {
    version_switch_node_input_prefix(bmain);

    if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "custom_scale_xyz[3]")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose == NULL) {
          continue;
        }
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          copy_v3_fl(pchan->custom_scale_xyz, pchan->custom_scale);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 4)) {
    /* Add a properties sidebar to the spreadsheet editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SPREADSHEET) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *new_sidebar = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_UI, "sidebar for spreadsheet", RGN_TYPE_FOOTER);
            if (new_sidebar != NULL) {
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

    if (!DNA_struct_elem_find(fd->filesdna, "FileAssetSelectParams", "short", "import_type")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_FILE) {
              SpaceFile *sfile = (SpaceFile *)sl;
              if (sfile->asset_params) {
                sfile->asset_params->import_type = FILE_ASSET_IMPORT_APPEND;
              }
            }
          }
        }
      }
    }

    /* Initialize length-wise scale B-Bone settings. */
    if (!DNA_struct_elem_find(fd->filesdna, "Bone", "int", "bbone_flag")) {
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

      BKE_animdata_main_cb(bmain, do_version_bbone_len_scale_animdata_cb, NULL);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 5)) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 6)) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 7)) {
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
        tool_settings->snap_mode |= (1 << 6); /* SCE_SNAP_MODE_INCREMENT */
      }
      if (snap_mode & (1 << 5)) {
        tool_settings->snap_mode |= (1 << 4); /* SCE_SNAP_MODE_EDGE_MIDPOINT */
      }
      if (snap_mode & (1 << 6)) {
        tool_settings->snap_mode |= (1 << 5); /* SCE_SNAP_MODE_EDGE_PERPENDICULAR */
      }
      if (snap_node_mode & (1 << 5)) {
        tool_settings->snap_node_mode |= (1 << 0); /* SCE_SNAP_MODE_NODE_X */
      }
      if (snap_node_mode & (1 << 6)) {
        tool_settings->snap_node_mode |= (1 << 1); /* SCE_SNAP_MODE_NODE_Y */
      }
      if (snap_uv_mode & (1 << 4)) {
        tool_settings->snap_uv_mode |= (1 << 6); /* SCE_SNAP_MODE_INCREMENT */
      }

      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode = SEQ_SNAP_TO_STRIPS | SEQ_SNAP_TO_CURRENT_FRAME |
                                           SEQ_SNAP_TO_STRIP_HOLD;
      sequencer_tool_settings->snap_distance = 15;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 8)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->master_collection != NULL) {
        BLI_strncpy(scene->master_collection->id.name + 2,
                    BKE_SCENE_COLLECTION_NAME,
                    sizeof(scene->master_collection->id.name) - 2);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 9)) {
    /* Fix a bug where reordering FCurves and bActionGroups could cause some corruption. Just
     * reconstruct all the action groups & ensure that the FCurves of a group are continuously
     * stored (i.e. not mixed with other groups) to be sure. See T89435. */
    LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
      BKE_action_groups_reconstruct(act);
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == GEO_NODE_SUBDIVIDE_MESH) {
            strcpy(node->idname, "GeometryNodeMeshSubdivide");
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 10)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      if (tool_settings->snap_uv_mode & (1 << 4)) {
        tool_settings->snap_uv_mode |= (1 << 6); /* SCE_SNAP_MODE_INCREMENT */
        tool_settings->snap_uv_mode &= ~(1 << 4);
      }
    }
    LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
      if (!(mat->lineart.flags & LRT_MATERIAL_CUSTOM_OCCLUSION_EFFECTIVENESS)) {
        mat->lineart.mat_occlusion = 1;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 13)) {
    /* Convert Surface Deform to sparse-capable bind structure. */
    if (!DNA_struct_elem_find(
            fd->filesdna, "SurfaceDeformModifierData", "int", "num_mesh_verts")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SurfaceDeform) {
            SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
            if (smd->bind_verts_num && smd->verts) {
              smd->mesh_verts_num = smd->bind_verts_num;

              for (unsigned int i = 0; i < smd->bind_verts_num; i++) {
                smd->verts[i].vertex_idx = i;
              }
            }
          }
        }
        if (ob->type == OB_GPENCIL) {
          LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
            if (md->type == eGpencilModifierType_Lineart) {
              LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
              lmd->flags |= LRT_GPENCIL_USE_CACHE;
              lmd->chain_smooth_tolerance = 0.2f;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "WorkSpace", "AssetLibraryReference", "asset_library")) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        BKE_asset_library_reference_init_default(&workspace->asset_library_ref);
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "FileAssetSelectParams", "AssetLibraryReference", "asset_library_ref")) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 14)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag &= ~SCE_SNAP_SEQ;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 15)) {
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

  /* Font names were copied directly into ID names, see: T90417. */
  if (!MAIN_VERSION_ATLEAST(bmain, 300, 16)) {
    ListBase *lb = which_libbase(bmain, ID_VF);
    BKE_main_id_repair_duplicate_names_listbase(lb);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 17)) {
    if (!DNA_struct_elem_find(
            fd->filesdna, "View3DOverlay", "float", "normals_constant_screen_size")) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 18)) {
    if (!DNA_struct_elem_find(
            fd->filesdna, "WorkSpace", "AssetLibraryReference", "asset_library_ref")) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        BKE_asset_library_reference_init_default(&workspace->asset_library_ref);
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "FileAssetSelectParams", "AssetLibraryReference", "asset_library_ref")) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 19)) {
    /* Disable Fade Inactive Overlay by default as it is redundant after introducing flash on
     * mode transfer. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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
      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->overlap_mode = SEQ_OVERLAP_SHUFFLE;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 20)) {
    /* Use new vector Size socket in Cube Mesh Primitive node. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }

      LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
        if (link->tonode->type == GEO_NODE_MESH_PRIMITIVE_CUBE) {
          bNode *node = link->tonode;
          if (STREQ(link->tosock->identifier, "Size") && link->tosock->type == SOCK_FLOAT) {
            bNode *link_fromnode = link->fromnode;
            bNodeSocket *link_fromsock = link->fromsock;
            bNodeSocket *socket = link->tosock;
            BLI_assert(socket);

            bNodeSocket *new_socket = do_version_replace_float_size_with_vector(
                ntree, node, socket);
            nodeAddLink(ntree, link_fromnode, link_fromsock, node, new_socket);
          }
        }
      }

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type != GEO_NODE_MESH_PRIMITIVE_CUBE) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 22)) {
    if (!DNA_struct_elem_find(
            fd->filesdna, "LineartGpencilModifierData", "bool", "use_crease_on_smooth")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->type == OB_GPENCIL) {
          LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
            if (md->type == eGpencilModifierType_Lineart) {
              LineartGpencilModifierData *lmd = (LineartGpencilModifierData *)md;
              lmd->calculation_flags |= LRT_USE_CREASE_ON_SMOOTH_SURFACES;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 23)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 24)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = SEQ_tool_settings_ensure(scene);
      sequencer_tool_settings->pivot_point = V3D_AROUND_CENTER_MEDIAN;

      if (scene->ed != NULL) {
        SEQ_for_each_callback(&scene->ed->seqbase, seq_transform_origin_set, NULL);
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 25)) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 26)) {
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

              /* New default import type: Append with reuse. */
              if (sfile->asset_params) {
                sfile->asset_params->import_type = FILE_ASSET_IMPORT_APPEND_REUSE;
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 29)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_SEQ: {
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              LISTBASE_FOREACH (ARegion *, region, regionbase) {
                if (region->regiontype == RGN_TYPE_WINDOW) {
                  region->v2d.max[1] = MAXSEQ;
                }
              }
              break;
            }
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->custom_grid_subdiv = 10;
              break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 31)) {
    /* Swap header with the tool header so the regular header is always on the edge. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *region_tool = NULL, *region_head = NULL;
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

    /* Set strip color tags to SEQUENCE_COLOR_NONE. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        SEQ_for_each_callback(&scene->ed->seqbase, do_versions_sequencer_color_tags, NULL);
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
      if (scene->ed != NULL) {
        SEQ_for_each_callback(&scene->ed->seqbase, do_versions_sequencer_color_balance_sop, NULL);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 33)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 36)) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 37)) {
    /* Node Editor: toggle overlays on. */
    if (!DNA_struct_find(fd->filesdna, "SpaceNodeOverlay")) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 38)) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 39)) {
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      wm->xr.session_settings.base_scale = 1.0f;
      wm->xr.session_settings.draw_flags |= (V3D_OFSDRAW_SHOW_SELECTION |
                                             V3D_OFSDRAW_XR_SHOW_CONTROLLERS |
                                             V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 40)) {
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
        if (node->type == GEO_NODE_VIEWER) {
          if (node->storage == NULL) {
            NodeGeometryViewer *data = (NodeGeometryViewer *)MEM_callocN(
                sizeof(NodeGeometryViewer), __func__);
            data->data_type = CD_PROP_FLOAT;
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

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 42)) {
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
                if (region->v2d.minzoom > 0.05f) {
                  region->v2d.minzoom = 0.05f;
                }
              }
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene);
      /* Make sure range of meta strips is correct.
       * It was possible to save .blend file with incorrect state of meta strip
       * range. The root cause is expected to be fixed, but need to ensure files
       * with invalid meta strip range are corrected. */
      if (ed != NULL) {
        SEQ_for_each_callback(&ed->seqbase, version_fix_seq_meta_range, scene);
      }
    }
  }

  /* Special case to handle older in-development 3.1 files, before change from 3.0 branch gets
   * merged in master. */
  if (!MAIN_VERSION_ATLEAST(bmain, 300, 42) ||
      (bmain->versionfile == 301 && !MAIN_VERSION_ATLEAST(bmain, 301, 3))) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 301, 4)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_node_id(ntree, GEO_NODE_CURVE_SPLINE_PARAMETER, "GeometryNodeSplineParameter");
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_CURVE_SPLINE_PARAMETER) {
          version_node_add_socket_if_not_exist(
              ntree, node, SOCK_OUT, SOCK_INT, PROP_NONE, "Index", "Index");
        }

        /* Convert float compare into a more general compare node. */
        if (node->type == FN_NODE_COMPARE) {
          if (node->storage == NULL) {
            NodeFunctionCompare *data = (NodeFunctionCompare *)MEM_callocN(
                sizeof(NodeFunctionCompare), __func__);
            data->data_type = SOCK_FLOAT;
            data->operation = node->custom1;
            strcpy(node->idname, "FunctionNodeCompare");
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

  if (!MAIN_VERSION_ATLEAST(bmain, 301, 5)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type != GEO_NODE_REALIZE_INSTANCES) {
          continue;
        }
        node->custom1 |= GEO_NODE_REALIZE_INSTANCES_LEGACY_BEHAVIOR;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 301, 6)) {
    /* Add node storage for map range node. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == SH_NODE_MAP_RANGE) {
          if (node->storage == NULL) {
            NodeMapRange *data = MEM_callocN(sizeof(NodeMapRange), __func__);
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

    /* Initialize the bone wireframe opacity setting. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "bone_wire_alpha")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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
        version_node_input_socket_name(ntree, GEO_NODE_TRANSFER_ATTRIBUTE, "Target", "Source");
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 301, 7) ||
      (bmain->versionfile == 302 && !MAIN_VERSION_ATLEAST(bmain, 302, 4))) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 2)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        SEQ_for_each_callback(&scene->ed->seqbase, seq_transform_filter_set, NULL);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 6)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (ts->uv_relax_method == 0) {
        ts->uv_relax_method = UV_SCULPT_TOOL_RELAX_LAPLACIAN;
      }
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *tool_settings = scene->toolsettings;
      tool_settings->snap_flag_seq = tool_settings->snap_flag & ~(SCE_SNAP | SCE_SNAP_SEQ);
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
        /* Previously other flags were ignored if CU_NURB_CYCLIC is set. */
        if (nurb->flagu & CU_NURB_CYCLIC) {
          nurb->flagu = CU_NURB_CYCLIC;
        }
        /* CU_NURB_BEZIER and CU_NURB_ENDPOINT were ignored if combined. */
        else if (nurb->flagu & CU_NURB_BEZIER && nurb->flagu & CU_NURB_ENDPOINT) {
          nurb->flagu &= ~(CU_NURB_BEZIER | CU_NURB_ENDPOINT);
          BKE_nurb_knot_calc_u(nurb);
        }
        /* Bezier NURBS of order 3 were clamped to first control point. */
        else if (nurb->orderu == 3 && (nurb->flagu & CU_NURB_BEZIER)) {
          nurb->flagu |= CU_NURB_ENDPOINT;
        }

        /* Previously other flags were ignored if CU_NURB_CYCLIC is set. */
        if (nurb->flagv & CU_NURB_CYCLIC) {
          nurb->flagv = CU_NURB_CYCLIC;
        }
        /* CU_NURB_BEZIER and CU_NURB_ENDPOINT were ignored if used together. */
        else if (nurb->flagv & CU_NURB_BEZIER && nurb->flagv & CU_NURB_ENDPOINT) {
          nurb->flagv &= ~(CU_NURB_BEZIER | CU_NURB_ENDPOINT);
          BKE_nurb_knot_calc_v(nurb);
        }
        /* Bezier NURBS of order 3 were clamped to first control point. */
        else if (nurb->orderv == 3 && (nurb->flagv & CU_NURB_BEZIER)) {
          nurb->flagv |= CU_NURB_ENDPOINT;
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
            gpmd->step = 1 + (int)(gpmd->factor * max_ff(0.0f,
                                                         min_ff(5.1f * sqrtf(gpmd->step) - 3.0f,
                                                                gpmd->step + 2.0f)));
            gpmd->factor = 1.0f;
          }
        }
      }
    }
  }

  /* Rebuild active/render color attribute references. */
  if (!MAIN_VERSION_ATLEAST(bmain, 302, 6)) {
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      /* Buggy code in wm_toolsystem broke smear in old files,
       * reset to defaults. */
      if (br->sculpt_tool == SCULPT_TOOL_SMEAR) {
        br->alpha = 1.0f;
        br->spacing = 5;
        br->flag &= ~BRUSH_ALPHA_PRESSURE;
        br->flag &= ~BRUSH_SPACE_ATTEN;
        br->curve_preset = BRUSH_CURVE_SPHERE;
      }
    }

    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (int step = 0; step < 2; step++) {
        CustomDataLayer *actlayer = NULL;

        int vact1, vact2;

        if (step) {
          vact1 = CustomData_get_render_layer_index(&me->vdata, CD_PROP_COLOR);
          vact2 = CustomData_get_render_layer_index(&me->ldata, CD_PROP_BYTE_COLOR);
        }
        else {
          vact1 = CustomData_get_active_layer_index(&me->vdata, CD_PROP_COLOR);
          vact2 = CustomData_get_active_layer_index(&me->ldata, CD_PROP_BYTE_COLOR);
        }

        if (vact1 != -1) {
          actlayer = me->vdata.layers + vact1;
        }
        else if (vact2 != -1) {
          actlayer = me->ldata.layers + vact2;
        }

        if (actlayer) {
          if (step) {
            BKE_id_attributes_render_color_set(&me->id, actlayer);
          }
          else {
            BKE_id_attributes_active_color_set(&me->id, actlayer);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 7)) {
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
      id->override_library->flag |= IDOVERRIDE_LIBRARY_FLAG_SYSTEM_DEFINED;
    }
    FOREACH_MAIN_ID_END;

    /* Initialize brush curves sculpt settings. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode != OB_MODE_SCULPT_CURVES) {
        continue;
      }
      if (brush->curves_sculpt_settings != NULL) {
        continue;
      }
      brush->curves_sculpt_settings = MEM_callocN(sizeof(BrushCurvesSculptSettings), __func__);
      brush->curves_sculpt_settings->add_amount = 1;
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 9)) {
    /* Sequencer channels region. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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
          ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
          if (!region) {
            ARegion *tools_region = BKE_area_find_region_type(area, RGN_TYPE_TOOLS);
            region = do_versions_add_region(RGN_TYPE_CHANNELS, "channels region");
            BLI_insertlinkafter(regionbase, tools_region, region);
            region->alignment = RGN_ALIGN_LEFT;
            region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
          }

          ARegion *timeline_region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
          if (timeline_region != NULL) {
            timeline_region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;
          }
        }
      }
    }

    /* Initialize channels. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene);
      if (ed == NULL) {
        continue;
      }
      SEQ_channels_ensure(&ed->channels);
      SEQ_for_each_callback(&scene->ed->seqbase, seq_meta_channels_ensure, NULL);

      ed->displayed_channels = &ed->channels;

      ListBase *previous_channels = &ed->channels;
      LISTBASE_FOREACH (MetaStack *, ms, &ed->metastack) {
        ms->old_channels = previous_channels;
        previous_channels = &ms->parseq->channels;
        /* If `MetaStack` exists, active channels must point to last link. */
        ed->displayed_channels = &ms->parseq->channels;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 10)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
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
      if (br->sculpt_tool == SCULPT_TOOL_SMEAR) {
        br->alpha = 1.0f;
        br->spacing = 5;
        br->flag &= ~BRUSH_ALPHA_PRESSURE;
        br->flag &= ~BRUSH_SPACE_ATTEN;
        br->curve_preset = BRUSH_CURVE_SPHERE;
      }
    }

    /* Rebuild active/render color attribute references. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (int step = 0; step < 2; step++) {
        CustomDataLayer *actlayer = NULL;

        int vact1, vact2;

        if (step) {
          vact1 = CustomData_get_render_layer_index(&me->vdata, CD_PROP_COLOR);
          vact2 = CustomData_get_render_layer_index(&me->ldata, CD_PROP_BYTE_COLOR);
        }
        else {
          vact1 = CustomData_get_active_layer_index(&me->vdata, CD_PROP_COLOR);
          vact2 = CustomData_get_active_layer_index(&me->ldata, CD_PROP_BYTE_COLOR);
        }

        if (vact1 != -1) {
          actlayer = me->vdata.layers + vact1;
        }
        else if (vact2 != -1) {
          actlayer = me->ldata.layers + vact2;
        }

        if (actlayer) {
          if (step) {
            BKE_id_attributes_render_color_set(&me->id, actlayer);
          }
          else {
            BKE_id_attributes_active_color_set(&me->id, actlayer);
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

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 12)) {
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
          if (node->type == GEO_NODE_MERGE_BY_DISTANCE) {
            if (node->storage == NULL) {
              NodeGeometryMergeByDistance *data = MEM_callocN(sizeof(NodeGeometryMergeByDistance),
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

  if (!MAIN_VERSION_ATLEAST(bmain, 302, 13)) {
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
      if (settings == NULL) {
        continue;
      }
      if (settings->curve_length == 0.0f) {
        settings->curve_length = 0.3f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 303, 1)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_replace_legacy_combined_and_separate_color_nodes(ntree);
    }
    FOREACH_NODETREE_END;

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
    if (!DNA_struct_elem_find(fd->filesdna, "ImagePackedFile", "int", "tile_number")) {
      for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
        int view;
        LISTBASE_FOREACH_INDEX (ImagePackedFile *, imapf, &ima->packedfiles, view) {
          imapf->view = view;
          imapf->tile_number = 1001;
        }
      }
    }

    /* Merge still offsets into start/end offsets. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene);
      if (ed != NULL) {
        SEQ_for_each_callback(&ed->seqbase, version_merge_still_offsets, NULL);
      }
    }

    /* Use the curves type enum for the set spline type node, instead of a special one. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == GEO_NODE_CURVE_SPLINE_TYPE) {
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
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
