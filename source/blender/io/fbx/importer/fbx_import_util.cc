/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BKE_idprop.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "DNA_object_types.h"

#include "fbx_import_util.hh"

namespace blender::io::fbx {

const char *get_fbx_name(const ufbx_string &name, const char *def)
{
  return name.length > 0 ? name.data : def;
}

void matrix_to_m44(const ufbx_matrix &src, float dst[4][4])
{
  dst[0][0] = src.m00;
  dst[1][0] = src.m01;
  dst[2][0] = src.m02;
  dst[3][0] = src.m03;
  dst[0][1] = src.m10;
  dst[1][1] = src.m11;
  dst[2][1] = src.m12;
  dst[3][1] = src.m13;
  dst[0][2] = src.m20;
  dst[1][2] = src.m21;
  dst[2][2] = src.m22;
  dst[3][2] = src.m23;
  dst[0][3] = 0.0f;
  dst[1][3] = 0.0f;
  dst[2][3] = 0.0f;
  dst[3][3] = 1.0f;
}

ufbx_matrix calc_bone_pose_matrix(const ufbx_transform &local_xform,
                                  const ufbx_node &node,
                                  const ufbx_matrix &local_bind_inv_matrix)
{
  ufbx_transform xform = local_xform;

  /* For bones that have "ignore parent scale" on them, ufbx helpfully applies global scale to
   * the evaluated transform. However we really need to get local transform without global
   * scale, so undo that. */
  if (node.adjust_post_scale != 1.0) {
    xform.scale.x /= node.adjust_post_scale;
    xform.scale.y /= node.adjust_post_scale;
    xform.scale.z /= node.adjust_post_scale;
  }

  /* Transformed to the bind transform in joint-local space. */
  ufbx_matrix matrix = ufbx_transform_to_matrix(&xform);
  matrix = ufbx_matrix_mul(&local_bind_inv_matrix, &matrix);
  return matrix;
}

void ufbx_matrix_to_obj(const ufbx_matrix &mtx, Object *obj)
{
#ifdef FBX_DEBUG_PRINT
  fprintf(g_debug_file, "init NODE %s self.matrix:\n", obj->id.name + 2);
  print_matrix(mtx);
#endif

  float obmat[4][4];
  matrix_to_m44(mtx, obmat);
  BKE_object_apply_mat4(obj, obmat, true, false);
  BKE_object_to_mat4(obj, obj->runtime->object_to_world.ptr());
}

void node_matrix_to_obj(const ufbx_node *node, Object *obj, const FbxElementMapping &mapping)
{
  ufbx_matrix mtx = ufbx_matrix_mul(node->node_depth < 2 ? &node->node_to_world :
                                                           &node->node_to_parent,
                                    &node->geometry_to_node);

  /* Handle case of an object parented to a bone: need to set
   * bone as parent, and make transform be at the end of the bone. */
  const ufbx_node *parbone = node->parent;
  if (obj->parent == nullptr && parbone && mapping.node_is_blender_bone.contains(parbone)) {
    Object *arm = mapping.bone_to_armature.lookup_default(parbone, nullptr);
    if (arm != nullptr) {
      ufbx_matrix offset_mtx = ufbx_identity_matrix;
      offset_mtx.cols[3].y = -mapping.bone_to_length.lookup_default(parbone, 0.0);
      if (mapping.node_is_blender_bone.contains(node)) {
        /* The node itself is a "fake bone", in which case parent it to the matching
         * fake bone, and matrix is just what puts transform at the bone tail. */
        parbone = node;
        mtx = offset_mtx;
      }
      else {
        mtx = ufbx_matrix_mul(&offset_mtx, &mtx);
      }

      obj->parent = arm;
      obj->partype = PARBONE;
      STRNCPY_UTF8(obj->parsubstr, mapping.node_to_name.lookup_default(parbone, "").c_str());

#ifdef FBX_DEBUG_PRINT
      fprintf(g_debug_file,
              "parent CHILD %s to ARM %s BONE %s bone_child_mtx:\n",
              node->name.data,
              arm->id.name + 2,
              parbone->name.data);
      print_matrix(offset_mtx);
      fprintf(g_debug_file, "- child matrix:\n");
      print_matrix(mtx);
#endif
    }
  }

  ufbx_matrix_to_obj(mtx, obj);
}

static void read_ufbx_property(const ufbx_prop &prop, IDProperty *idgroup, bool enums_as_strings)
{
  IDProperty *idprop = nullptr;
  IDPropertyTemplate val = {0};
  //@TODO: validate_blend_names on the property name
  const char *name = prop.name.data;

  switch (prop.type) {
    case UFBX_PROP_BOOLEAN:
      val.i = prop.value_int;
      idprop = IDP_New(IDP_BOOLEAN, &val, name);
      break;
    case UFBX_PROP_INTEGER: {
      bool parsed_as_enum = false;
      if (enums_as_strings && (prop.flags & UFBX_PROP_FLAG_VALUE_STR) &&
          (prop.value_str.length > 0))
      {
        /* "Enum" property with integer value, and enum names as `~` separated string. */
        const char *tilde = prop.value_str.data;
        int enum_index = -1;
        while (true) {
          const char *tilde_start = tilde;
          tilde = BLI_strchr_or_end(tilde_start, '~');
          if (tilde == tilde_start) {
            break;
          }
          /* We have an enum value string. */
          enum_index++;
          if (enum_index == prop.value_int) {
            /* Found the needed one. */
            parsed_as_enum = true;
            std::string str_val = StringRef(tilde_start, tilde).trim();
            val.string.str = str_val.c_str();
            val.string.len = str_val.size() + 1; /* .len needs to include null terminator. */
            val.string.subtype = IDP_STRING_SUB_UTF8;
            idprop = IDP_New(IDP_STRING, &val, name);
            break;
          }
          if (tilde[0] == 0) {
            break;
          }
          tilde++;
        }
      }

      if (!parsed_as_enum) {
        val.i = prop.value_int;
        idprop = IDP_New(IDP_INT, &val, name);
      }

    } break;
    case UFBX_PROP_NUMBER:
      val.d = prop.value_real;
      idprop = IDP_New(IDP_DOUBLE, &val, name);
      break;
    case UFBX_PROP_STRING:
      if (STREQ(name, "UDP3DSMAX")) {
        /* 3dsmax user properties are coming as `UDP3DSMAX` property. Parse them
         * as multi-line text, splitting across `=` within each line. */
        const char *line = prop.value_str.data;
        while (true) {
          const char *line_start = line;
          line = BLI_strchr_or_end(line_start, '\n');
          if (line == line_start) {
            break;
          }

          /* We have a line, split it by '=' and trim name/value. */
          const char *eq_pos = line_start;
          while (eq_pos != line && eq_pos[0] != '=') {
            eq_pos++;
          }
          if (eq_pos[0] == '=') {
            std::string str_name = StringRef(line_start, eq_pos).trim();
            std::string str_val = StringRef(eq_pos + 1, line).trim();
            //@TODO validate_blend_names on str_name
            val.string.str = str_val.c_str();
            val.string.len = str_val.size() + 1; /* .len needs to include null terminator. */
            val.string.subtype = IDP_STRING_SUB_UTF8;
            IDProperty *str_prop = IDP_New(IDP_STRING, &val, str_name.c_str());
            IDP_AddToGroup(idgroup, str_prop);
          }

          if (line[0] == 0) {
            break;
          }
          line++;
        }
      }
      else {
        val.string.str = prop.value_str.data;
        val.string.len = prop.value_str.length + 1; /* .len needs to include null terminator. */
        val.string.subtype = IDP_STRING_SUB_UTF8;
        idprop = IDP_New(IDP_STRING, &val, name);
      }
      break;
    case UFBX_PROP_VECTOR:
    case UFBX_PROP_COLOR:
      val.array.len = 3;
      val.array.type = IDP_DOUBLE;
      idprop = IDP_New(IDP_ARRAY, &val, name);
      {
        double *dst = static_cast<double *>(idprop->data.pointer);
        dst[0] = prop.value_vec3.x;
        dst[1] = prop.value_vec3.y;
        dst[2] = prop.value_vec3.z;
      }
      break;
    case UFBX_PROP_COLOR_WITH_ALPHA:
      val.array.len = 4;
      val.array.type = IDP_DOUBLE;
      idprop = IDP_New(IDP_ARRAY, &val, name);
      {
        double *dst = static_cast<double *>(idprop->data.pointer);
        dst[0] = prop.value_vec4.x;
        dst[1] = prop.value_vec4.y;
        dst[2] = prop.value_vec4.z;
        dst[3] = prop.value_vec4.z;
      }
      break;
    default:
      break;
  }

  if (idprop != nullptr) {
    IDP_AddToGroup(idgroup, idprop);
  }
}

void read_custom_properties(const ufbx_props &props, ID &id, bool enums_as_strings)
{
  for (const ufbx_prop &prop : props.props) {
    if ((prop.flags & UFBX_PROP_FLAG_USER_DEFINED) == 0) {
      continue;
    }
    IDProperty *idgroup = IDP_EnsureProperties(&id);
    read_ufbx_property(prop, idgroup, enums_as_strings);
  }
}

static IDProperty *pchan_EnsureProperties(bPoseChannel &pchan)
{
  if (pchan.prop == nullptr) {
    pchan.prop = bke::idprop::create_group("").release();
  }
  return pchan.prop;
}

void read_custom_properties(const ufbx_props &props, bPoseChannel &pchan, bool enums_as_strings)
{
  for (const ufbx_prop &prop : props.props) {
    if ((prop.flags & UFBX_PROP_FLAG_USER_DEFINED) == 0) {
      continue;
    }
    IDProperty *idgroup = pchan_EnsureProperties(pchan);
    read_ufbx_property(prop, idgroup, enums_as_strings);
  }
}

#ifdef FBX_DEBUG_PRINT
FILE *g_debug_file;
void print_matrix(const ufbx_matrix &m)
{
  fprintf(g_debug_file,
          "    (%.3f %.3f %.3f %.3f)\n",
          adjf(m.cols[0].x),
          adjf(m.cols[1].x),
          adjf(m.cols[2].x),
          adjf(m.cols[3].x));
  fprintf(g_debug_file,
          "    (%.3f %.3f %.3f %.3f)\n",
          adjf(m.cols[0].y),
          adjf(m.cols[1].y),
          adjf(m.cols[2].y),
          adjf(m.cols[3].y));
  fprintf(g_debug_file,
          "    (%.3f %.3f %.3f %.3f)\n",
          adjf(m.cols[0].z),
          adjf(m.cols[1].z),
          adjf(m.cols[2].z),
          adjf(m.cols[3].z));
}
#endif

}  // namespace blender::io::fbx
