/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_key_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_rotation.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_types.hh"

#include "rna_internal.hh"

const EnumPropertyItem rna_enum_keyblock_type_items[] = {
    {KEY_LINEAR, "KEY_LINEAR", 0, "Linear", ""},
    {KEY_CARDINAL, "KEY_CARDINAL", 0, "Cardinal", ""},
    {KEY_CATMULL_ROM, "KEY_CATMULL_ROM", 0, "Catmull-Rom", ""},
    {KEY_BSPLINE, "KEY_BSPLINE", 0, "BSpline", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>
#  include <cstddef>
#  include <fmt/format.h>

#  include "BLT_translation.hh"

#  include "DNA_curve_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_object_types.h"

#  include "BLI_listbase.h"
#  include "BLI_string.h"
#  include "BLI_string_utf8.h"
#  include "BLI_string_utils.hh"

#  include "BKE_animsys.h"
#  include "BKE_key.hh"
#  include "BKE_main.hh"

#  include "DEG_depsgraph.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"

static Key *rna_ShapeKey_find_key(ID *id)
{
  switch (GS(id->name)) {
    case ID_CU_LEGACY:
      return ((Curve *)id)->key;
    case ID_KE:
      return (Key *)id;
    case ID_LT:
      return ((Lattice *)id)->key;
    case ID_ME:
      return ((Mesh *)id)->key;
    case ID_OB:
      return BKE_key_from_object((Object *)id);
    default:
      return nullptr;
  }
}

static void rna_ShapeKey_name_set(PointerRNA *ptr, const char *value)
{
  KeyBlock *kb = static_cast<KeyBlock *>(ptr->data);
  char oldname[sizeof(kb->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, kb->name);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(kb->name, value);

  /* make sure the name is truly unique */
  if (ptr->owner_id) {
    Key *key = rna_ShapeKey_find_key(ptr->owner_id);
    BLI_uniquename(&key->block,
                   kb,
                   CTX_DATA_(BLT_I18NCONTEXT_ID_SHAPEKEY, "Key"),
                   '.',
                   offsetof(KeyBlock, name),
                   sizeof(kb->name));
  }

  /* fix all the animation data which may link to this */
  BKE_animdata_fix_paths_rename_all(nullptr, "key_blocks", oldname, kb->name);
}

static float rna_ShapeKey_frame_get(PointerRNA *ptr)
{
  KeyBlock *kb = (KeyBlock *)ptr->data;
  return kb->pos * 100.0f; /* Because pos is ctime/100... */
}

static void rna_ShapeKey_value_set(PointerRNA *ptr, float value)
{
  KeyBlock *data = (KeyBlock *)ptr->data;
  CLAMP(value, data->slidermin, data->slidermax);
  data->curval = value;
}

static void rna_ShapeKey_value_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  KeyBlock *data = (KeyBlock *)ptr->data;

  *min = data->slidermin;
  *max = data->slidermax;
}

/* epsilon for how close one end of shapekey range can get to the other */
#  define SHAPEKEY_SLIDER_TOL 0.001f

static void rna_ShapeKey_slider_min_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  KeyBlock *data = (KeyBlock *)ptr->data;

  *min = -10.0f;
  *max = data->slidermax - SHAPEKEY_SLIDER_TOL;
}

static void rna_ShapeKey_slider_min_set(PointerRNA *ptr, float value)
{
  KeyBlock *data = (KeyBlock *)ptr->data;
  float min, max, softmin, softmax;

  rna_ShapeKey_slider_min_range(ptr, &min, &max, &softmin, &softmax);
  CLAMP(value, min, max);
  data->slidermin = value;
}

static void rna_ShapeKey_slider_max_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  KeyBlock *data = (KeyBlock *)ptr->data;

  *min = data->slidermin + SHAPEKEY_SLIDER_TOL;
  *max = 10.0f;
}

static void rna_ShapeKey_slider_max_set(PointerRNA *ptr, float value)
{
  KeyBlock *data = (KeyBlock *)ptr->data;
  float min, max, softmin, softmax;

  rna_ShapeKey_slider_max_range(ptr, &min, &max, &softmin, &softmax);
  CLAMP(value, min, max);
  data->slidermax = value;
}

#  undef SHAPEKEY_SLIDER_TOL

/* ***** Normals accessors for shape-keys. ***** */
/* NOTE: with this we may recompute several times the same data, should we want to access verts,
 *       then faces, then loops normals... However,
 *       such case looks rather unlikely - and not worth adding some kind of caching in key-blocks.
 */

static Mesh *rna_KeyBlock_normals_get_mesh(const PointerRNA *ptr, ID *id)
{
  Key *key = rna_ShapeKey_find_key((id == nullptr && ptr != nullptr) ? ptr->owner_id : id);
  id = key ? key->from : nullptr;

  if (id != nullptr) {
    switch (GS(id->name)) {
      case ID_ME:
        return (Mesh *)id;
      case ID_OB: {
        Object *ob = (Object *)id;
        if (ob->type == OB_MESH) {
          return static_cast<Mesh *>(ob->data);
        }
      }
      default:
        break;
    }
  }

  return nullptr;
}

static int rna_KeyBlock_normals_vert_len(const PointerRNA *ptr,
                                         int length[RNA_MAX_ARRAY_DIMENSION])
{
  const Mesh *mesh = rna_KeyBlock_normals_get_mesh(ptr, nullptr);

  length[0] = mesh ? mesh->verts_num : 0;
  length[1] = 3;

  return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_vert_calc(ID *id,
                                           KeyBlock *data,
                                           float **normals,
                                           int *normals_num)
{
  Mesh *mesh = rna_KeyBlock_normals_get_mesh(nullptr, id);

  *normals_num = (mesh ? mesh->verts_num : 0) * 3;

  if (ELEM(nullptr, mesh, data) || (mesh->verts_num == 0)) {
    *normals = nullptr;
    return;
  }

  *normals = MEM_malloc_arrayN<float>(size_t(*normals_num), __func__);

  BKE_keyblock_mesh_calc_normals(data, mesh, (float (*)[3])(*normals), nullptr, nullptr);
}

static int rna_KeyBlock_normals_poly_len(const PointerRNA *ptr,
                                         int length[RNA_MAX_ARRAY_DIMENSION])
{
  const Mesh *mesh = rna_KeyBlock_normals_get_mesh(ptr, nullptr);

  length[0] = mesh ? mesh->faces_num : 0;
  length[1] = 3;

  return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_poly_calc(ID *id,
                                           KeyBlock *data,
                                           float **normals,
                                           int *normals_num)
{
  Mesh *mesh = rna_KeyBlock_normals_get_mesh(nullptr, id);

  *normals_num = (mesh ? mesh->faces_num : 0) * 3;

  if (ELEM(nullptr, mesh, data) || (mesh->faces_num == 0)) {
    *normals = nullptr;
    return;
  }

  *normals = MEM_malloc_arrayN<float>(size_t(*normals_num), __func__);

  BKE_keyblock_mesh_calc_normals(data, mesh, nullptr, (float (*)[3])(*normals), nullptr);
}

static int rna_KeyBlock_normals_loop_len(const PointerRNA *ptr,
                                         int length[RNA_MAX_ARRAY_DIMENSION])
{
  const Mesh *mesh = rna_KeyBlock_normals_get_mesh(ptr, nullptr);

  length[0] = mesh ? mesh->corners_num : 0;
  length[1] = 3;

  return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_loop_calc(ID *id,
                                           KeyBlock *data,
                                           float **normals,
                                           int *normals_num)
{
  Mesh *mesh = rna_KeyBlock_normals_get_mesh(nullptr, id);

  *normals_num = (mesh ? mesh->corners_num : 0) * 3;

  if (ELEM(nullptr, mesh, data) || (mesh->corners_num == 0)) {
    *normals = nullptr;
    return;
  }

  *normals = MEM_malloc_arrayN<float>(size_t(*normals_num), __func__);

  BKE_keyblock_mesh_calc_normals(data, mesh, nullptr, nullptr, (float (*)[3])(*normals));
}

PointerRNA rna_object_shapekey_index_get(ID *id, int value)
{
  Key *key = rna_ShapeKey_find_key(id);
  KeyBlock *kb = nullptr;

  if (key && value < key->totkey) {
    kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, value));
  }

  PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_ShapeKey, kb);
  return ptr;
}

int rna_object_shapekey_index_set(ID *id, PointerRNA value, int current)
{
  Key *key = rna_ShapeKey_find_key(id);

  if (key) {
    int a = BLI_findindex(&key->block, value.data);
    if (a != -1) {
      return a;
    }
  }

  return current;
}

static PointerRNA rna_ShapeKey_relative_key_get(PointerRNA *ptr)
{
  KeyBlock *kb = (KeyBlock *)ptr->data;

  return rna_object_shapekey_index_get(ptr->owner_id, kb->relative);
}

static void rna_ShapeKey_relative_key_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  KeyBlock *kb = (KeyBlock *)ptr->data;

  kb->relative = rna_object_shapekey_index_set(ptr->owner_id, value, kb->relative);
}

static void rna_ShapeKeyPoint_co_get(PointerRNA *ptr, float *values)
{
  float *vec = (float *)ptr->data;

  values[0] = vec[0];
  values[1] = vec[1];
  values[2] = vec[2];
}

static void rna_ShapeKeyPoint_co_set(PointerRNA *ptr, const float *values)
{
  float *vec = (float *)ptr->data;

  vec[0] = values[0];
  vec[1] = values[1];
  vec[2] = values[2];
}

static float rna_ShapeKeyCurvePoint_tilt_get(PointerRNA *ptr)
{
  float *vec = (float *)ptr->data;
  return vec[3];
}

static void rna_ShapeKeyCurvePoint_tilt_set(PointerRNA *ptr, float value)
{
  float *vec = (float *)ptr->data;
  vec[3] = value;
}

static float rna_ShapeKeyCurvePoint_radius_get(PointerRNA *ptr)
{
  float *vec = (float *)ptr->data;
  return vec[4];
}

static void rna_ShapeKeyCurvePoint_radius_set(PointerRNA *ptr, float value)
{
  float *vec = (float *)ptr->data;
  CLAMP_MIN(value, 0.0f);
  vec[4] = value;
}

static void rna_ShapeKeyBezierPoint_co_get(PointerRNA *ptr, float *values)
{
  float *vec = (float *)ptr->data;

  values[0] = vec[0 + 3];
  values[1] = vec[1 + 3];
  values[2] = vec[2 + 3];
}

static void rna_ShapeKeyBezierPoint_co_set(PointerRNA *ptr, const float *values)
{
  float *vec = (float *)ptr->data;

  vec[0 + 3] = values[0];
  vec[1 + 3] = values[1];
  vec[2 + 3] = values[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_get(PointerRNA *ptr, float *values)
{
  float *vec = (float *)ptr->data;

  values[0] = vec[0];
  values[1] = vec[1];
  values[2] = vec[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_set(PointerRNA *ptr, const float *values)
{
  float *vec = (float *)ptr->data;

  vec[0] = values[0];
  vec[1] = values[1];
  vec[2] = values[2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_get(PointerRNA *ptr, float *values)
{
  float *vec = (float *)ptr->data;

  values[0] = vec[6 + 0];
  values[1] = vec[6 + 1];
  values[2] = vec[6 + 2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_set(PointerRNA *ptr, const float *values)
{
  float *vec = (float *)ptr->data;

  vec[6 + 0] = values[0];
  vec[6 + 1] = values[1];
  vec[6 + 2] = values[2];
}

static float rna_ShapeKeyBezierPoint_tilt_get(PointerRNA *ptr)
{
  float *vec = (float *)ptr->data;
  return vec[9];
}

static void rna_ShapeKeyBezierPoint_tilt_set(PointerRNA *ptr, float value)
{
  float *vec = (float *)ptr->data;
  vec[9] = value;
}

static float rna_ShapeKeyBezierPoint_radius_get(PointerRNA *ptr)
{
  float *vec = (float *)ptr->data;
  return vec[10];
}

static void rna_ShapeKeyBezierPoint_radius_set(PointerRNA *ptr, float value)
{
  float *vec = (float *)ptr->data;
  CLAMP_MIN(value, 0.0f);
  vec[10] = value;
}

/* Indexing and iteration of Curve points through sub-curves. */
struct NurbInfo {
  Nurb *nu;
  int nurb_size, nurb_elem_step;

  /* Current index in the Nurb */
  int nurb_index;

  /* Total index as item and element. */
  int item_index, elem_index;
};

StructRNA *rna_ShapeKey_curve_point_type(Nurb *nu)
{
  if (nu->bezt) {
    return &RNA_ShapeKeyBezierPoint;
  }
  return &RNA_ShapeKeyCurvePoint;
}

static void rna_ShapeKey_NurbInfo_init(NurbInfo *r_info, Nurb *nu)
{
  r_info->nu = nu;

  if (nu->bezt) {
    r_info->nurb_size = nu->pntsu;
    r_info->nurb_elem_step = KEYELEM_ELEM_LEN_BEZTRIPLE;
  }
  else {
    r_info->nurb_size = nu->pntsu * nu->pntsv;
    r_info->nurb_elem_step = KEYELEM_ELEM_LEN_BPOINT;
  }
}

static void rna_ShapeKey_NurbInfo_step(NurbInfo *r_info,
                                       Nurb *nu,
                                       int *p_raw_index,
                                       bool input_elem)
{
  rna_ShapeKey_NurbInfo_init(r_info, nu);

  if (input_elem) {
    r_info->nurb_index = std::min(r_info->nurb_size, *p_raw_index / r_info->nurb_elem_step);
    *p_raw_index -= r_info->nurb_size * r_info->nurb_elem_step;
  }
  else {
    r_info->nurb_index = std::min(r_info->nurb_size, *p_raw_index);
    *p_raw_index -= r_info->nurb_size;
  }

  r_info->item_index += r_info->nurb_index;
  r_info->elem_index += r_info->nurb_index * r_info->nurb_elem_step;
}

static void rna_ShapeKey_NurbInfo_find_index(Key *key,
                                             int raw_index,
                                             bool input_elem,
                                             NurbInfo *r_info)
{
  Curve *cu = (Curve *)key->from;

  memset(r_info, 0, sizeof(*r_info));

  for (Nurb *nu = static_cast<Nurb *>(cu->nurb.first); nu && raw_index >= 0; nu = nu->next) {
    rna_ShapeKey_NurbInfo_step(r_info, nu, &raw_index, input_elem);
  }
}

static int rna_ShapeKey_curve_find_index(Key *key, int elem_index)
{
  NurbInfo info;
  rna_ShapeKey_NurbInfo_find_index(key, elem_index, true, &info);
  return info.item_index;
}

struct ShapeKeyCurvePoint {
  StructRNA *type;
  void *data;
};

/* Build a mapping array for Curve objects with mixed sub-curve types. */
static void rna_ShapeKey_data_begin_mixed(
    CollectionPropertyIterator *iter, PointerRNA *ptr, Key *key, KeyBlock *kb, Curve *cu)
{
  int point_count = rna_ShapeKey_curve_find_index(key, kb->totelem);

  ShapeKeyCurvePoint *points = MEM_malloc_arrayN<ShapeKeyCurvePoint>(size_t(point_count),
                                                                     __func__);

  char *databuf = static_cast<char *>(kb->data);
  int items_left = point_count;
  NurbInfo info = {nullptr};

  for (Nurb *nu = static_cast<Nurb *>(cu->nurb.first); nu && items_left > 0; nu = nu->next) {
    ShapeKeyCurvePoint *nurb_points = points + info.item_index;
    char *nurb_data = databuf + info.elem_index * key->elemsize;

    rna_ShapeKey_NurbInfo_step(&info, nu, &items_left, false);

    StructRNA *type = rna_ShapeKey_curve_point_type(nu);

    for (int i = 0; i < info.nurb_index; i++) {
      nurb_points[i].type = type;
      nurb_points[i].data = nurb_data + i * info.nurb_elem_step * key->elemsize;
    }
  }

  rna_iterator_array_begin(iter, ptr, points, sizeof(*points), point_count, true, nullptr);
}

static void rna_ShapeKey_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int tot = kb->totelem, size = key->elemsize;

  if (GS(key->from->name) == ID_CU_LEGACY && tot > 0) {
    Curve *cu = (Curve *)key->from;
    StructRNA *type = nullptr;
    NurbInfo info = {nullptr};

    /* Check if all sub-curves have the same type. */
    LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
      if (type == nullptr) {
        type = rna_ShapeKey_curve_point_type(nu);
        rna_ShapeKey_NurbInfo_init(&info, nu);
      }
      else if (type != rna_ShapeKey_curve_point_type(nu)) {
        type = nullptr;
        break;
      }
    }

    /* If types are mixed, build a mapping array. */
    if (type == nullptr) {
      rna_ShapeKey_data_begin_mixed(iter, ptr, key, kb, cu);
      return;
    }
    tot /= info.nurb_elem_step;
    size *= info.nurb_elem_step;
  }

  rna_iterator_array_begin(iter, ptr, kb->data, size, tot, false, nullptr);
}

static int rna_ShapeKey_data_length(PointerRNA *ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int tot = kb->totelem;

  if (GS(key->from->name) == ID_CU_LEGACY) {
    tot = rna_ShapeKey_curve_find_index(key, tot);
  }

  return tot;
}

static PointerRNA rna_ShapeKey_data_get(CollectionPropertyIterator *iter)
{
  Key *key = rna_ShapeKey_find_key(iter->parent.owner_id);
  void *ptr = rna_iterator_array_get(iter);
  StructRNA *type = &RNA_ShapeKeyPoint;

  /* If data_begin allocated a mapping array, access it. */
  if (iter->internal.array.free_ptr) {
    ShapeKeyCurvePoint *point = static_cast<ShapeKeyCurvePoint *>(ptr);

    return RNA_pointer_create_with_parent(iter->parent, point->type, point->data);
  }

  if (GS(key->from->name) == ID_CU_LEGACY) {
    Curve *cu = (Curve *)key->from;

    type = rna_ShapeKey_curve_point_type(static_cast<Nurb *>(cu->nurb.first));
  }

  return RNA_pointer_create_with_parent(iter->parent, type, ptr);
}

bool rna_ShapeKey_data_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int elemsize = key->elemsize;
  char *databuf = static_cast<char *>(kb->data);

  *r_ptr = {};

  if (index < 0) {
    return false;
  }

  if (GS(key->from->name) == ID_CU_LEGACY) {
    NurbInfo info;
    rna_ShapeKey_NurbInfo_find_index(key, index, false, &info);

    if (info.nu && info.nurb_index < info.nurb_size) {
      StructRNA *type = rna_ShapeKey_curve_point_type(info.nu);

      rna_pointer_create_with_ancestors(*ptr, type, databuf + elemsize * info.elem_index, *r_ptr);
      return true;
    }
  }
  else {
    if (index < kb->totelem) {
      rna_pointer_create_with_ancestors(
          *ptr, &RNA_ShapeKeyPoint, databuf + elemsize * index, *r_ptr);
      return true;
    }
  }

  return false;
}

static void rna_ShapeKey_points_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int tot = kb->totelem;

  if (GS(key->from->name) == ID_CU_LEGACY) {
    /* Legacy curves have only curve points and bezier points. */
    tot = 0;
  }
  rna_iterator_array_begin(iter, ptr, kb->data, key->elemsize, tot, false, nullptr);
}

static int rna_ShapeKey_points_length(PointerRNA *ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int tot = kb->totelem;

  if (GS(key->from->name) == ID_CU_LEGACY) {
    /* Legacy curves have only curve points and bezier points. */
    tot = 0;
  }

  return tot;
}

bool rna_ShapeKey_points_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb = (KeyBlock *)ptr->data;
  int elemsize = key->elemsize;
  char *databuf = static_cast<char *>(kb->data);

  *r_ptr = {};

  if (index < 0) {
    return false;
  }

  if (GS(key->from->name) == ID_CU_LEGACY) {
    /* Legacy curves have only curve points and bezier points. */
    return false;
  }
  else {
    if (index < kb->totelem) {
      rna_pointer_create_with_ancestors(
          *ptr, &RNA_ShapeKeyPoint, databuf + elemsize * index, *r_ptr);
      return true;
    }
  }

  return false;
}

static std::optional<std::string> rna_ShapeKey_path(const PointerRNA *ptr)
{
  const KeyBlock *kb = (KeyBlock *)ptr->data;
  const ID *id = ptr->owner_id;
  char name_esc[sizeof(kb->name) * 2];

  BLI_str_escape(name_esc, kb->name, sizeof(name_esc));

  if ((id) && (GS(id->name) != ID_KE)) {
    return fmt::format("shape_keys.key_blocks[\"{}\"]", name_esc);
  }
  return fmt::format("key_blocks[\"{}\"]", name_esc);
}

static void rna_Key_update_data(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Key *key = (Key *)ptr->owner_id;
  Object *ob;

  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    if (BKE_key_from_object(ob) == key) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
    }
  }
}

static void rna_ShapeKey_update_minmax(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  KeyBlock *data = (KeyBlock *)ptr->data;
  if (IN_RANGE_INCL(data->curval, data->slidermin, data->slidermax)) {
    return;
  }
  CLAMP(data->curval, data->slidermin, data->slidermax);
  rna_Key_update_data(bmain, scene, ptr);
}

static KeyBlock *rna_ShapeKeyData_find_keyblock(Key *key, const float *point)
{
  KeyBlock *kb;

  /* sanity checks */
  if (ELEM(nullptr, key, point)) {
    return nullptr;
  }

  /* We'll need to manually search through the key-blocks and check
   * if the point is somewhere in the middle of each block's data. */
  for (kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next) {
    if (kb->data) {
      float *start = (float *)kb->data;
      float *end;

      /* easy cases first */
      if ((start == nullptr) || (start > point)) {
        /* there's no chance point is in array */
        continue;
      }
      if (start == point) {
        /* exact match - point is first in array */
        return kb;
      }

      /* determine where end of array is
       * - elemsize is in bytes, so use (char *) cast to get array in terms of bytes
       */
      end = (float *)((char *)start + (key->elemsize * kb->totelem));

      /* If point's address is less than the end,
       * then it is somewhere between start and end, so in array. */
      if (end > point) {
        /* we've found the owner of the point data */
        return kb;
      }
    }
  }

  return nullptr;
}

static int rna_ShapeKeyPoint_get_index(Key *key, KeyBlock *kb, float *point)
{
  /* if we frame the data array and point pointers as (char *), then the difference between
   * them will be in bytes. Thus, dividing through by key->elemsize (number of bytes per point)
   * gives us the offset of point from start of array.
   */
  char *start = (char *)kb->data;
  char *pt = (char *)point;

  return int(pt - start) / key->elemsize;
}

static std::optional<std::string> rna_ShapeKeyPoint_path(const PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  Key *key = rna_ShapeKey_find_key(ptr->owner_id);
  KeyBlock *kb;
  float *point = (float *)ptr->data;

  /* if we can get a key block, we can construct a path */
  kb = rna_ShapeKeyData_find_keyblock(key, point);

  if (kb) {
    char name_esc_kb[sizeof(kb->name) * 2];
    int index;

    index = rna_ShapeKeyPoint_get_index(key, kb, point);

    if (ELEM(ptr->type, &RNA_ShapeKeyBezierPoint, &RNA_ShapeKeyCurvePoint)) {
      index = rna_ShapeKey_curve_find_index(key, index);
    }

    BLI_str_escape(name_esc_kb, kb->name, sizeof(name_esc_kb));

    if (GS(id->name) == ID_KE) {
      return fmt::format("key_blocks[\"{}\"].data[{}]", name_esc_kb, index);
    }
    return fmt::format("shape_keys.key_blocks[\"{}\"].data[{}]", name_esc_kb, index);
  }
  return std::nullopt; /* XXX: there's really no way to resolve this... */
}

#else

static const float tilt_limit = DEG2RADF(21600.0f);

static void rna_def_keydata(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ShapeKeyPoint", nullptr);
  RNA_def_struct_sdna(srna, "vec3f");
  RNA_def_struct_ui_text(srna, "Shape Key Point", "Point in a shape key");
  RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  srna = RNA_def_struct(brna, "ShapeKeyCurvePoint", nullptr);
  RNA_def_struct_ui_text(srna, "Shape Key Curve Point", "Point in a shape key for curves");
  /* there's nothing type specific here, so this is fine for now */
  RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyPoint_co_get", "rna_ShapeKeyPoint_co_set", nullptr);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyCurvePoint_tilt_get", "rna_ShapeKeyCurvePoint_tilt_set", nullptr);
  RNA_def_property_range(prop, -tilt_limit, tilt_limit);
  RNA_def_property_ui_range(prop, -tilt_limit, tilt_limit, 10, 3);
  RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3D View");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyCurvePoint_radius_get", "rna_ShapeKeyCurvePoint_radius_set", nullptr);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Radius", "Radius for beveling");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  srna = RNA_def_struct(brna, "ShapeKeyBezierPoint", nullptr);
  RNA_def_struct_ui_text(srna, "Shape Key Bézier Point", "Point in a shape key for Bézier curves");
  /* there's nothing type specific here, so this is fine for now */
  RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyBezierPoint_co_get", "rna_ShapeKeyBezierPoint_co_set", nullptr);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_ShapeKeyBezierPoint_handle_1_co_get",
                               "rna_ShapeKeyBezierPoint_handle_1_co_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "Handle 1 Location", "");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_ShapeKeyBezierPoint_handle_2_co_get",
                               "rna_ShapeKeyBezierPoint_handle_2_co_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "Handle 2 Location", "");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyBezierPoint_tilt_get", "rna_ShapeKeyBezierPoint_tilt_set", nullptr);
  RNA_def_property_range(prop, -tilt_limit, tilt_limit);
  RNA_def_property_ui_range(prop, -tilt_limit, tilt_limit, 10, 3);
  RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3D View");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_funcs(
      prop, "rna_ShapeKeyBezierPoint_radius_get", "rna_ShapeKeyBezierPoint_radius_set", nullptr);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop, "Radius", "Radius for beveling");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");
}

static void rna_def_keyblock(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "ShapeKey", nullptr);
  RNA_def_struct_ui_text(srna, "Shape Key", "Shape key in a shape keys data-block");
  RNA_def_struct_sdna(srna, "KeyBlock");
  RNA_def_struct_path_func(srna, "rna_ShapeKey_path");
  RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of Shape Key");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_ShapeKey_name_set");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");
  RNA_def_struct_name_property(srna, prop);

  /* keys need to be sorted to edit this */
  prop = RNA_def_property(srna, "frame", PROP_FLOAT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_sdna(prop, nullptr, "pos");
  RNA_def_property_float_funcs(prop, "rna_ShapeKey_frame_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Frame", "Frame for absolute keys");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  /* for now, this is editable directly, as users can set this even if they're not animating them
   * (to test results) */
  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "curval");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ShapeKey_value_set", "rna_ShapeKey_value_range");
  RNA_def_property_ui_range(prop, -10.0f, 10.0f, 10, 3);
  RNA_def_property_ui_text(prop, "Value", "Value of shape key at the current frame");
  RNA_def_property_update(prop, ND_KEYS, "rna_Key_update_data");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_keyblock_type_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Interpolation type for absolute shape keys");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "vgroup");
  RNA_def_property_ui_text(prop, "Vertex Group", "Vertex weight group, to blend with basis shape");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "relative_key", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ShapeKey");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL | PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_pointer_funcs(
      prop, "rna_ShapeKey_relative_key_get", "rna_ShapeKey_relative_key_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Relative Key", "Shape used as a relative key");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", KEYBLOCK_MUTE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Mute", "Toggle this shape key");
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, -1);
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "lock_shape", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", KEYBLOCK_LOCKED_SHAPE);
  RNA_def_property_ui_text(
      prop, "Lock Shape", "Protect the shape key from accidental sculpting and editing");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "slider_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "slidermin");
  RNA_def_property_range(prop, -10.0f, 10.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ShapeKey_slider_min_set", "rna_ShapeKey_slider_min_range");
  RNA_def_property_ui_text(prop, "Slider Min", "Minimum for slider");
  RNA_def_property_update(prop, 0, "rna_ShapeKey_update_minmax");

  prop = RNA_def_property(srna, "slider_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "slidermax");
  RNA_def_property_range(prop, -10.0f, 10.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_ShapeKey_slider_max_set", "rna_ShapeKey_slider_max_range");
  RNA_def_property_ui_text(prop, "Slider Max", "Maximum for slider");
  RNA_def_property_update(prop, 0, "rna_ShapeKey_update_minmax");

  prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "data", "totelem");
  RNA_def_property_struct_type(prop, "UnknownType");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop, "Data", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_ShapeKey_data_begin",
                                    nullptr,
                                    nullptr,
                                    "rna_ShapeKey_data_get",
                                    "rna_ShapeKey_data_length",
                                    "rna_ShapeKey_data_lookup_int",
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "data", nullptr);
  RNA_def_property_struct_type(prop, "ShapeKeyPoint");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_ui_text(prop,
                           "Points",
                           "Optimized access to shape keys point data, when using "
                           "foreach_get/foreach_set accessors. "
                           "Warning: Does not support legacy Curve shape keys.");
  RNA_def_property_collection_funcs(prop,
                                    "rna_ShapeKey_points_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_ShapeKey_points_length",
                                    "rna_ShapeKey_points_lookup_int",
                                    nullptr,
                                    nullptr);

  /* XXX multi-dim dynamic arrays are very badly supported by (py)rna currently,
   *     those are defined for the day it works better, for now user will get a 1D tuple.
   */
  func = RNA_def_function(srna, "normals_vertex_get", "rna_KeyBlock_normals_vert_calc");
  RNA_def_function_ui_description(func,
                                  "Compute local space vertices' normals for this shape key");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_property(func, "normals", PROP_FLOAT, /*PROP_DIRECTION*/ PROP_NONE);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
  RNA_def_property_multi_array(parm, 2, nullptr);
  RNA_def_property_range(parm, -1.0f, 1.0f);
  RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_vert_len");

  func = RNA_def_function(srna, "normals_polygon_get", "rna_KeyBlock_normals_poly_calc");
  RNA_def_function_ui_description(func, "Compute local space faces' normals for this shape key");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_property(func, "normals", PROP_FLOAT, /*PROP_DIRECTION*/ PROP_NONE);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
  RNA_def_property_multi_array(parm, 2, nullptr);
  RNA_def_property_range(parm, -1.0f, 1.0f);
  RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_poly_len");

  func = RNA_def_function(srna, "normals_split_get", "rna_KeyBlock_normals_loop_calc");
  RNA_def_function_ui_description(func,
                                  "Compute local space face corners' normals for this shape key");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_property(func, "normals", PROP_FLOAT, /*PROP_DIRECTION*/ PROP_NONE);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
  RNA_def_property_multi_array(parm, 2, nullptr);
  RNA_def_property_range(parm, -1.0f, 1.0f);
  RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_loop_len");
}

static void rna_def_key(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Key", "ID");
  RNA_def_struct_ui_text(
      srna, "Key", "Shape keys data-block containing different shapes of geometric data-blocks");
  RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

  prop = RNA_def_property(srna, "reference_key", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_sdna(prop, nullptr, "refkey");
  RNA_def_property_ui_text(prop, "Reference Key", "");

  prop = RNA_def_property(srna, "key_blocks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "block", nullptr);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_struct_type(prop, "ShapeKey");
  RNA_def_property_ui_text(prop, "Key Blocks", "Shape keys");

  rna_def_animdata_common(srna);

  prop = RNA_def_property(srna, "user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_pointer_sdna(prop, nullptr, "from");
  RNA_def_property_ui_text(prop, "User", "Data-block using these shape keys");

  prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "type", KEY_RELATIVE);
  RNA_def_property_ui_text(
      prop,
      "Relative",
      "Make shape keys relative, "
      "otherwise play through shapes as a sequence using the evaluation time");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");

  prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ctime");
  RNA_def_property_range(prop, MINFRAME, MAXFRAME);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Evaluation Time", "Evaluation time for absolute shape keys");
  RNA_def_property_update(prop, 0, "rna_Key_update_data");
}

void RNA_def_key(BlenderRNA *brna)
{
  rna_def_key(brna);
  rna_def_keyblock(brna);
  rna_def_keydata(brna);
}

#endif
