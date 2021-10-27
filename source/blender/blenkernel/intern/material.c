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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_customdata_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BLI_array_utils.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_brush.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_vfont.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "GPU_material.h"

#include "NOD_shader.h"

#include "BLO_read_write.h"

static CLG_LogRef LOG = {"bke.material"};

static void material_init_data(ID *id)
{
  Material *material = (Material *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(material, id));

  MEMCPY_STRUCT_AFTER(material, DNA_struct_default_get(Material), id);
}

static void material_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Material *material_dst = (Material *)id_dst;
  const Material *material_src = (const Material *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (material_src->nodetree != NULL) {
    if (is_localized) {
      material_dst->nodetree = ntreeLocalize(material_src->nodetree);
    }
    else {
      BKE_id_copy_ex(bmain,
                     (ID *)material_src->nodetree,
                     (ID **)&material_dst->nodetree,
                     flag_private_id_data);
    }
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&material_dst->id, &material_src->id);
  }
  else {
    material_dst->preview = NULL;
  }

  if (material_src->texpaintslot != NULL) {
    /* TODO: Think we can also skip copying this data in the more generic `NO_MAIN` case? */
    material_dst->texpaintslot = is_localized ? NULL : MEM_dupallocN(material_src->texpaintslot);
  }

  if (material_src->gp_style != NULL) {
    material_dst->gp_style = MEM_dupallocN(material_src->gp_style);
  }

  BLI_listbase_clear(&material_dst->gpumaterial);

  /* TODO: Duplicate Engine Settings and set runtime to NULL. */
}

static void material_free_data(ID *id)
{
  Material *material = (Material *)id;

  /* Free gpu material before the ntree */
  GPU_material_free(&material->gpumaterial);

  /* is no lib link block, but material extension */
  if (material->nodetree) {
    ntreeFreeEmbeddedTree(material->nodetree);
    MEM_freeN(material->nodetree);
    material->nodetree = NULL;
  }

  MEM_SAFE_FREE(material->texpaintslot);

  MEM_SAFE_FREE(material->gp_style);

  BKE_icon_id_delete((ID *)material);
  BKE_previewimg_free(&material->preview);
}

static void material_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Material *material = (Material *)id;
  /* Nodetrees **are owned by IDs**, treat them as mere sub-data and not real ID! */
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data, BKE_library_foreach_ID_embedded(data, (ID **)&material->nodetree));
  if (material->texpaintslot != NULL) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->texpaintslot->ima, IDWALK_CB_NOP);
  }
  if (material->gp_style != NULL) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->gp_style->sima, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->gp_style->ima, IDWALK_CB_USER);
  }
}

static void material_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Material *ma = (Material *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  ma->texpaintslot = NULL;
  BLI_listbase_clear(&ma->gpumaterial);

  /* write LibData */
  BLO_write_id_struct(writer, Material, id_address, &ma->id);
  BKE_id_blend_write(writer, &ma->id);

  if (ma->adt) {
    BKE_animdata_blend_write(writer, ma->adt);
  }

  /* nodetree is integral part of material, no libdata */
  if (ma->nodetree) {
    BLO_write_struct(writer, bNodeTree, ma->nodetree);
    ntreeBlendWrite(writer, ma->nodetree);
  }

  BKE_previewimg_blend_write(writer, ma->preview);

  /* grease pencil settings */
  if (ma->gp_style) {
    BLO_write_struct(writer, MaterialGPencilStyle, ma->gp_style);
  }
}

static void material_blend_read_data(BlendDataReader *reader, ID *id)
{
  Material *ma = (Material *)id;
  BLO_read_data_address(reader, &ma->adt);
  BKE_animdata_blend_read_data(reader, ma->adt);

  ma->texpaintslot = NULL;

  BLO_read_data_address(reader, &ma->preview);
  BKE_previewimg_blend_read(reader, ma->preview);

  BLI_listbase_clear(&ma->gpumaterial);

  BLO_read_data_address(reader, &ma->gp_style);
}

static void material_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Material *ma = (Material *)id;
  BLO_read_id_address(reader, ma->id.lib, &ma->ipo); /* XXX deprecated - old animation system */

  /* relink grease pencil settings */
  if (ma->gp_style != NULL) {
    MaterialGPencilStyle *gp_style = ma->gp_style;
    if (gp_style->sima != NULL) {
      BLO_read_id_address(reader, ma->id.lib, &gp_style->sima);
    }
    if (gp_style->ima != NULL) {
      BLO_read_id_address(reader, ma->id.lib, &gp_style->ima);
    }
  }
}

static void material_blend_read_expand(BlendExpander *expander, ID *id)
{
  Material *ma = (Material *)id;
  BLO_expand(expander, ma->ipo); /* XXX deprecated - old animation system */

  if (ma->gp_style) {
    MaterialGPencilStyle *gp_style = ma->gp_style;
    BLO_expand(expander, gp_style->sima);
    BLO_expand(expander, gp_style->ima);
  }
}

IDTypeInfo IDType_ID_MA = {
    .id_code = ID_MA,
    .id_filter = FILTER_ID_MA,
    .main_listbase_index = INDEX_ID_MA,
    .struct_size = sizeof(Material),
    .name = "Material",
    .name_plural = "materials",
    .translation_context = BLT_I18NCONTEXT_ID_MATERIAL,
    .flags = IDTYPE_FLAGS_APPEND_IS_REUSABLE,

    .init_data = material_init_data,
    .copy_data = material_copy_data,
    .free_data = material_free_data,
    .make_local = NULL,
    .foreach_id = material_foreach_id,
    .foreach_cache = NULL,
    .owner_get = NULL,

    .blend_write = material_blend_write,
    .blend_read_data = material_blend_read_data,
    .blend_read_lib = material_blend_read_lib,
    .blend_read_expand = material_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

void BKE_gpencil_material_attr_init(Material *ma)
{
  if ((ma) && (ma->gp_style == NULL)) {
    ma->gp_style = MEM_callocN(sizeof(MaterialGPencilStyle), "Grease Pencil Material Settings");

    MaterialGPencilStyle *gp_style = ma->gp_style;
    /* set basic settings */
    gp_style->stroke_rgba[3] = 1.0f;
    gp_style->fill_rgba[3] = 1.0f;
    ARRAY_SET_ITEMS(gp_style->mix_rgba, 1.0f, 1.0f, 1.0f, 1.0f);
    ARRAY_SET_ITEMS(gp_style->texture_scale, 1.0f, 1.0f);
    gp_style->texture_offset[0] = -0.5f;
    gp_style->texture_pixsize = 100.0f;
    gp_style->mix_factor = 0.5f;

    gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
  }
}

Material *BKE_material_add(Main *bmain, const char *name)
{
  Material *ma;

  ma = BKE_id_new(bmain, ID_MA, name);

  return ma;
}

Material *BKE_gpencil_material_add(Main *bmain, const char *name)
{
  Material *ma;

  ma = BKE_material_add(bmain, name);

  /* grease pencil settings */
  if (ma != NULL) {
    BKE_gpencil_material_attr_init(ma);
  }
  return ma;
}

Material ***BKE_object_material_array_p(Object *ob)
{
  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;
    return &(me->mat);
  }
  if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
    Curve *cu = ob->data;
    return &(cu->mat);
  }
  if (ob->type == OB_MBALL) {
    MetaBall *mb = ob->data;
    return &(mb->mat);
  }
  if (ob->type == OB_GPENCIL) {
    bGPdata *gpd = ob->data;
    return &(gpd->mat);
  }
  if (ob->type == OB_HAIR) {
    Hair *hair = ob->data;
    return &(hair->mat);
  }
  if (ob->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = ob->data;
    return &(pointcloud->mat);
  }
  if (ob->type == OB_VOLUME) {
    Volume *volume = ob->data;
    return &(volume->mat);
  }
  return NULL;
}

short *BKE_object_material_len_p(Object *ob)
{
  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;
    return &(me->totcol);
  }
  if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
    Curve *cu = ob->data;
    return &(cu->totcol);
  }
  if (ob->type == OB_MBALL) {
    MetaBall *mb = ob->data;
    return &(mb->totcol);
  }
  if (ob->type == OB_GPENCIL) {
    bGPdata *gpd = ob->data;
    return &(gpd->totcol);
  }
  if (ob->type == OB_HAIR) {
    Hair *hair = ob->data;
    return &(hair->totcol);
  }
  if (ob->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = ob->data;
    return &(pointcloud->totcol);
  }
  if (ob->type == OB_VOLUME) {
    Volume *volume = ob->data;
    return &(volume->totcol);
  }
  return NULL;
}

/* same as above but for ID's */
Material ***BKE_id_material_array_p(ID *id)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      return &(((Mesh *)id)->mat);
    case ID_CU:
      return &(((Curve *)id)->mat);
    case ID_MB:
      return &(((MetaBall *)id)->mat);
    case ID_GD:
      return &(((bGPdata *)id)->mat);
    case ID_HA:
      return &(((Hair *)id)->mat);
    case ID_PT:
      return &(((PointCloud *)id)->mat);
    case ID_VO:
      return &(((Volume *)id)->mat);
    default:
      break;
  }
  return NULL;
}

short *BKE_id_material_len_p(ID *id)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      return &(((Mesh *)id)->totcol);
    case ID_CU:
      return &(((Curve *)id)->totcol);
    case ID_MB:
      return &(((MetaBall *)id)->totcol);
    case ID_GD:
      return &(((bGPdata *)id)->totcol);
    case ID_HA:
      return &(((Hair *)id)->totcol);
    case ID_PT:
      return &(((PointCloud *)id)->totcol);
    case ID_VO:
      return &(((Volume *)id)->totcol);
    default:
      break;
  }
  return NULL;
}

static void material_data_index_remove_id(ID *id, short index)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      BKE_mesh_material_index_remove((Mesh *)id, index);
      break;
    case ID_CU:
      BKE_curve_material_index_remove((Curve *)id, index);
      break;
    case ID_MB:
    case ID_HA:
    case ID_PT:
    case ID_VO:
      /* No material indices for these object data types. */
      break;
    default:
      break;
  }
}

bool BKE_object_material_slot_used(Object *object, short actcol)
{
  if (!BKE_object_supports_material_slots(object)) {
    return false;
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
    if (psys->part->omat == actcol) {
      return true;
    }
  }

  ID *ob_data = object->data;
  if (ob_data == NULL || !OB_DATA_SUPPORT_ID(GS(ob_data->name))) {
    return false;
  }

  switch (GS(ob_data->name)) {
    case ID_ME:
      return BKE_mesh_material_index_used((Mesh *)ob_data, actcol - 1);
    case ID_CU:
      return BKE_curve_material_index_used((Curve *)ob_data, actcol - 1);
    case ID_MB:
      /* Meta-elements don't support materials at the moment. */
      return false;
    case ID_GD:
      return BKE_gpencil_material_index_used((bGPdata *)ob_data, actcol - 1);
    default:
      return false;
  }
}

static void material_data_index_clear_id(ID *id)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      BKE_mesh_material_index_clear((Mesh *)id);
      break;
    case ID_CU:
      BKE_curve_material_index_clear((Curve *)id);
      break;
    case ID_MB:
    case ID_HA:
    case ID_PT:
    case ID_VO:
      /* No material indices for these object data types. */
      break;
    default:
      break;
  }
}

void BKE_id_materials_copy(Main *bmain, ID *id_src, ID *id_dst)
{
  Material ***matar_src = BKE_id_material_array_p(id_src);
  const short *materials_len_p_src = BKE_id_material_len_p(id_src);

  Material ***matar_dst = BKE_id_material_array_p(id_dst);
  short *materials_len_p_dst = BKE_id_material_len_p(id_dst);

  *materials_len_p_dst = *materials_len_p_src;
  if (*materials_len_p_src != 0) {
    (*matar_dst) = MEM_dupallocN(*matar_src);

    for (int a = 0; a < *materials_len_p_src; a++) {
      id_us_plus((ID *)(*matar_dst)[a]);
    }

    DEG_id_tag_update(id_dst, ID_RECALC_COPY_ON_WRITE);
    DEG_relations_tag_update(bmain);
  }
}

void BKE_id_material_resize(Main *bmain, ID *id, short totcol, bool do_id_user)
{
  Material ***matar = BKE_id_material_array_p(id);
  short *totcolp = BKE_id_material_len_p(id);

  if (matar == NULL) {
    return;
  }

  if (do_id_user && totcol < (*totcolp)) {
    short i;
    for (i = totcol; i < (*totcolp); i++) {
      id_us_min((ID *)(*matar)[i]);
    }
  }

  if (totcol == 0) {
    if (*totcolp) {
      MEM_freeN(*matar);
      *matar = NULL;
    }
  }
  else {
    *matar = MEM_recallocN(*matar, sizeof(void *) * totcol);
  }
  *totcolp = totcol;

  DEG_id_tag_update(id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
}

void BKE_id_material_append(Main *bmain, ID *id, Material *ma)
{
  Material ***matar;
  if ((matar = BKE_id_material_array_p(id))) {
    short *totcol = BKE_id_material_len_p(id);
    Material **mat = MEM_callocN(sizeof(void *) * ((*totcol) + 1), "newmatar");
    if (*totcol) {
      memcpy(mat, *matar, sizeof(void *) * (*totcol));
    }
    if (*matar) {
      MEM_freeN(*matar);
    }

    *matar = mat;
    (*matar)[(*totcol)++] = ma;

    id_us_plus((ID *)ma);
    BKE_objects_materials_test_all(bmain, id);

    DEG_id_tag_update(id, ID_RECALC_COPY_ON_WRITE);
    DEG_relations_tag_update(bmain);
  }
}

Material *BKE_id_material_pop(Main *bmain, ID *id, int index_i)
{
  short index = (short)index_i;
  Material *ret = NULL;
  Material ***matar;
  if ((matar = BKE_id_material_array_p(id))) {
    short *totcol = BKE_id_material_len_p(id);
    if (index >= 0 && index < (*totcol)) {
      ret = (*matar)[index];
      id_us_min((ID *)ret);

      if (*totcol <= 1) {
        *totcol = 0;
        MEM_freeN(*matar);
        *matar = NULL;
      }
      else {
        if (index + 1 != (*totcol)) {
          memmove((*matar) + index,
                  (*matar) + (index + 1),
                  sizeof(void *) * ((*totcol) - (index + 1)));
        }

        (*totcol)--;
        *matar = MEM_reallocN(*matar, sizeof(void *) * (*totcol));
        BKE_objects_materials_test_all(bmain, id);
      }

      material_data_index_remove_id(id, index);

      DEG_id_tag_update(id, ID_RECALC_COPY_ON_WRITE);
      DEG_relations_tag_update(bmain);
    }
  }

  return ret;
}

void BKE_id_material_clear(Main *bmain, ID *id)
{
  Material ***matar;
  if ((matar = BKE_id_material_array_p(id))) {
    short *totcol = BKE_id_material_len_p(id);

    while ((*totcol)--) {
      id_us_min((ID *)((*matar)[*totcol]));
    }
    *totcol = 0;
    if (*matar) {
      MEM_freeN(*matar);
      *matar = NULL;
    }

    BKE_objects_materials_test_all(bmain, id);
    material_data_index_clear_id(id);

    DEG_id_tag_update(id, ID_RECALC_COPY_ON_WRITE);
    DEG_relations_tag_update(bmain);
  }
}

Material **BKE_object_material_get_p(Object *ob, short act)
{
  Material ***matarar, **ma_p;
  const short *totcolp;

  if (ob == NULL) {
    return NULL;
  }

  /* if object cannot have material, (totcolp == NULL) */
  totcolp = BKE_object_material_len_p(ob);
  if (totcolp == NULL || ob->totcol == 0) {
    return NULL;
  }

  /* return NULL for invalid 'act', can happen for mesh face indices */
  if (act > ob->totcol) {
    return NULL;
  }
  if (act <= 0) {
    if (act < 0) {
      CLOG_ERROR(&LOG, "Negative material index!");
    }
    return NULL;
  }

  if (ob->matbits && ob->matbits[act - 1]) { /* in object */
    ma_p = &ob->mat[act - 1];
  }
  else { /* in data */

    /* check for inconsistency */
    if (*totcolp < ob->totcol) {
      ob->totcol = *totcolp;
    }
    if (act > ob->totcol) {
      act = ob->totcol;
    }

    matarar = BKE_object_material_array_p(ob);

    if (matarar && *matarar) {
      ma_p = &(*matarar)[act - 1];
    }
    else {
      ma_p = NULL;
    }
  }

  return ma_p;
}

Material *BKE_object_material_get(Object *ob, short act)
{
  Material **ma_p = BKE_object_material_get_p(ob, act);
  return ma_p ? *ma_p : NULL;
}

static ID *get_evaluated_object_data_with_materials(Object *ob)
{
  ID *data = ob->data;
  /* Meshes in edit mode need special handling. */
  if (ob->type == OB_MESH && ob->mode == OB_MODE_EDIT) {
    Mesh *mesh = ob->data;
    if (mesh->edit_mesh && mesh->edit_mesh->mesh_eval_final) {
      data = &mesh->edit_mesh->mesh_eval_final->id;
    }
  }
  return data;
}

/**
 * On evaluated objects the number of materials on an object and its data might go out of sync.
 * This is because during evaluation materials can be added/removed on the object data.
 *
 * For rendering or exporting we generally use the materials on the object data. However, some
 * material indices might be overwritten by the object.
 */
Material *BKE_object_material_get_eval(Object *ob, short act)
{
  BLI_assert(DEG_is_evaluated_object(ob));
  const int slot_index = act - 1;

  if (slot_index < 0) {
    return NULL;
  }
  ID *data = get_evaluated_object_data_with_materials(ob);
  const short *tot_slots_data_ptr = BKE_id_material_len_p(data);
  const int tot_slots_data = tot_slots_data_ptr ? *tot_slots_data_ptr : 0;
  if (slot_index >= tot_slots_data) {
    return NULL;
  }
  const int tot_slots_object = ob->totcol;

  Material ***materials_data_ptr = BKE_id_material_array_p(data);
  Material **materials_data = materials_data_ptr ? *materials_data_ptr : NULL;
  Material **materials_object = ob->mat;

  /* Check if slot is overwritten by object. */
  if (slot_index < tot_slots_object) {
    if (ob->matbits) {
      if (ob->matbits[slot_index]) {
        Material *material = materials_object[slot_index];
        if (material != NULL) {
          return material;
        }
      }
    }
  }
  /* Otherwise use data from object-data. */
  if (slot_index < tot_slots_data) {
    Material *material = materials_data[slot_index];
    return material;
  }
  return NULL;
}

int BKE_object_material_count_eval(Object *ob)
{
  BLI_assert(DEG_is_evaluated_object(ob));
  ID *id = get_evaluated_object_data_with_materials(ob);
  const short *len_p = BKE_id_material_len_p(id);
  return len_p ? *len_p : 0;
}

void BKE_id_material_eval_assign(ID *id, int slot, Material *material)
{
  BLI_assert(slot >= 1);
  Material ***materials_ptr = BKE_id_material_array_p(id);
  short *len_ptr = BKE_id_material_len_p(id);
  if (ELEM(NULL, materials_ptr, len_ptr)) {
    BLI_assert_unreachable();
    return;
  }

  const int slot_index = slot - 1;
  const int old_length = *len_ptr;

  if (slot_index >= old_length) {
    /* Need to grow slots array. */
    const int new_length = slot_index + 1;
    *materials_ptr = MEM_reallocN(*materials_ptr, sizeof(void *) * new_length);
    *len_ptr = new_length;
    for (int i = old_length; i < new_length; i++) {
      (*materials_ptr)[i] = NULL;
    }
  }

  (*materials_ptr)[slot_index] = material;
}

/**
 * Add an empty material slot if the id has no material slots. This material slot allows the
 * material to be overwritten by object-linked materials.
 */
void BKE_id_material_eval_ensure_default_slot(ID *id)
{
  short *len_ptr = BKE_id_material_len_p(id);
  if (len_ptr == NULL) {
    return;
  }
  if (*len_ptr == 0) {
    BKE_id_material_eval_assign(id, 1, NULL);
  }
}

Material *BKE_gpencil_material(Object *ob, short act)
{
  Material *ma = BKE_object_material_get(ob, act);
  if (ma != NULL) {
    return ma;
  }

  return BKE_material_default_gpencil();
}

MaterialGPencilStyle *BKE_gpencil_material_settings(Object *ob, short act)
{
  Material *ma = BKE_object_material_get(ob, act);
  if (ma != NULL) {
    if (ma->gp_style == NULL) {
      BKE_gpencil_material_attr_init(ma);
    }

    return ma->gp_style;
  }

  return BKE_material_default_gpencil()->gp_style;
}

void BKE_object_material_resize(Main *bmain, Object *ob, const short totcol, bool do_id_user)
{
  Material **newmatar;
  char *newmatbits;

  if (do_id_user && totcol < ob->totcol) {
    for (int i = totcol; i < ob->totcol; i++) {
      id_us_min((ID *)ob->mat[i]);
    }
  }

  if (totcol == 0) {
    if (ob->totcol) {
      MEM_freeN(ob->mat);
      MEM_freeN(ob->matbits);
      ob->mat = NULL;
      ob->matbits = NULL;
    }
  }
  else if (ob->totcol < totcol) {
    newmatar = MEM_callocN(sizeof(void *) * totcol, "newmatar");
    newmatbits = MEM_callocN(sizeof(char) * totcol, "newmatbits");
    if (ob->totcol) {
      memcpy(newmatar, ob->mat, sizeof(void *) * ob->totcol);
      memcpy(newmatbits, ob->matbits, sizeof(char) * ob->totcol);
      MEM_freeN(ob->mat);
      MEM_freeN(ob->matbits);
    }
    ob->mat = newmatar;
    ob->matbits = newmatbits;
  }
  /* XXX(campbell): why not realloc on shrink? */

  ob->totcol = totcol;
  if (ob->totcol && ob->actcol == 0) {
    ob->actcol = 1;
  }
  if (ob->actcol > ob->totcol) {
    ob->actcol = ob->totcol;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
}

void BKE_object_materials_test(Main *bmain, Object *ob, ID *id)
{
  /* make the ob mat-array same size as 'ob->data' mat-array */
  const short *totcol;

  if (id == NULL || (totcol = BKE_id_material_len_p(id)) == NULL) {
    return;
  }

  BKE_object_material_resize(bmain, ob, *totcol, false);
}

void BKE_objects_materials_test_all(Main *bmain, ID *id)
{
  /* make the ob mat-array same size as 'ob->data' mat-array */
  Object *ob;
  const short *totcol;

  if (id == NULL || (totcol = BKE_id_material_len_p(id)) == NULL) {
    return;
  }

  BKE_main_lock(bmain);
  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->data == id) {
      BKE_object_material_resize(bmain, ob, *totcol, false);
    }
  }
  BKE_main_unlock(bmain);
}

void BKE_id_material_assign(Main *bmain, ID *id, Material *ma, short act)
{
  Material *mao, **matar, ***matarar;
  short *totcolp;

  if (act > MAXMAT) {
    return;
  }
  if (act < 1) {
    act = 1;
  }

  /* test arraylens */

  totcolp = BKE_id_material_len_p(id);
  matarar = BKE_id_material_array_p(id);

  if (totcolp == NULL || matarar == NULL) {
    return;
  }

  if (act > *totcolp) {
    matar = MEM_callocN(sizeof(void *) * act, "matarray1");

    if (*totcolp) {
      memcpy(matar, *matarar, sizeof(void *) * (*totcolp));
      MEM_freeN(*matarar);
    }

    *matarar = matar;
    *totcolp = act;
  }

  /* in data */
  mao = (*matarar)[act - 1];
  if (mao) {
    id_us_min(&mao->id);
  }
  (*matarar)[act - 1] = ma;

  if (ma) {
    id_us_plus(&ma->id);
  }

  BKE_objects_materials_test_all(bmain, id);
}

void BKE_object_material_assign(Main *bmain, Object *ob, Material *ma, short act, int assign_type)
{
  Material *mao, **matar, ***matarar;
  short *totcolp;
  char bit = 0;

  if (act > MAXMAT) {
    return;
  }
  if (act < 1) {
    act = 1;
  }

  /* test arraylens */

  totcolp = BKE_object_material_len_p(ob);
  matarar = BKE_object_material_array_p(ob);

  if (totcolp == NULL || matarar == NULL) {
    return;
  }

  if (act > *totcolp) {
    matar = MEM_callocN(sizeof(void *) * act, "matarray1");

    if (*totcolp) {
      memcpy(matar, *matarar, sizeof(void *) * (*totcolp));
      MEM_freeN(*matarar);
    }

    *matarar = matar;
    *totcolp = act;
  }

  if (act > ob->totcol) {
    /* Need more space in the material arrays */
    ob->mat = MEM_recallocN_id(ob->mat, sizeof(void *) * act, "matarray2");
    ob->matbits = MEM_recallocN_id(ob->matbits, sizeof(char) * act, "matbits1");
    ob->totcol = act;
  }

  /* Determine the object/mesh linking */
  if (assign_type == BKE_MAT_ASSIGN_EXISTING) {
    /* keep existing option (avoid confusion in scripts),
     * intentionally ignore userpref (default to obdata). */
    bit = ob->matbits[act - 1];
  }
  else if (assign_type == BKE_MAT_ASSIGN_USERPREF && ob->totcol && ob->actcol) {
    /* copy from previous material */
    bit = ob->matbits[ob->actcol - 1];
  }
  else {
    switch (assign_type) {
      case BKE_MAT_ASSIGN_OBDATA:
        bit = 0;
        break;
      case BKE_MAT_ASSIGN_OBJECT:
        bit = 1;
        break;
      case BKE_MAT_ASSIGN_USERPREF:
      default:
        bit = (U.flag & USER_MAT_ON_OB) ? 1 : 0;
        break;
    }
  }

  /* do it */

  ob->matbits[act - 1] = bit;
  if (bit == 1) { /* in object */
    mao = ob->mat[act - 1];
    if (mao) {
      id_us_min(&mao->id);
    }
    ob->mat[act - 1] = ma;
    BKE_object_materials_test(bmain, ob, ob->data);
  }
  else { /* in data */
    mao = (*matarar)[act - 1];
    if (mao) {
      id_us_min(&mao->id);
    }
    (*matarar)[act - 1] = ma;
    BKE_objects_materials_test_all(bmain, ob->data); /* Data may be used by several objects... */
  }

  if (ma) {
    id_us_plus(&ma->id);
  }
}

void BKE_object_material_remap(Object *ob, const unsigned int *remap)
{
  Material ***matar = BKE_object_material_array_p(ob);
  const short *totcol_p = BKE_object_material_len_p(ob);

  BLI_array_permute(ob->mat, ob->totcol, remap);

  if (ob->matbits) {
    BLI_array_permute(ob->matbits, ob->totcol, remap);
  }

  if (matar) {
    BLI_array_permute(*matar, *totcol_p, remap);
  }

  if (ob->type == OB_MESH) {
    BKE_mesh_material_remap(ob->data, remap, ob->totcol);
  }
  else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
    BKE_curve_material_remap(ob->data, remap, ob->totcol);
  }
  else if (ob->type == OB_GPENCIL) {
    BKE_gpencil_material_remap(ob->data, remap, ob->totcol);
  }
  else {
    /* add support for this object data! */
    BLI_assert(matar == NULL);
  }
}

/**
 * Calculate a material remapping from \a ob_src to \a ob_dst.
 *
 * \param remap_src_to_dst: An array the size of `ob_src->totcol`
 * where index values are filled in which map to \a ob_dst materials.
 */
void BKE_object_material_remap_calc(Object *ob_dst, Object *ob_src, short *remap_src_to_dst)
{
  if (ob_src->totcol == 0) {
    return;
  }

  GHash *gh_mat_map = BLI_ghash_ptr_new_ex(__func__, ob_src->totcol);

  for (int i = 0; i < ob_dst->totcol; i++) {
    Material *ma_src = BKE_object_material_get(ob_dst, i + 1);
    BLI_ghash_reinsert(gh_mat_map, ma_src, POINTER_FROM_INT(i), NULL, NULL);
  }

  /* setup default mapping (when materials don't match) */
  {
    int i = 0;
    if (ob_dst->totcol >= ob_src->totcol) {
      for (; i < ob_src->totcol; i++) {
        remap_src_to_dst[i] = i;
      }
    }
    else {
      for (; i < ob_dst->totcol; i++) {
        remap_src_to_dst[i] = i;
      }
      for (; i < ob_src->totcol; i++) {
        remap_src_to_dst[i] = 0;
      }
    }
  }

  for (int i = 0; i < ob_src->totcol; i++) {
    Material *ma_src = BKE_object_material_get(ob_src, i + 1);

    if ((i < ob_dst->totcol) && (ma_src == BKE_object_material_get(ob_dst, i + 1))) {
      /* when objects have exact matching materials - keep existing index */
    }
    else {
      void **index_src_p = BLI_ghash_lookup_p(gh_mat_map, ma_src);
      if (index_src_p) {
        remap_src_to_dst[i] = POINTER_AS_INT(*index_src_p);
      }
    }
  }

  BLI_ghash_free(gh_mat_map, NULL, NULL);
}

/**
 * Copy materials from evaluated geometry to the original geometry of an object.
 */
void BKE_object_material_from_eval_data(Main *bmain, Object *ob_orig, ID *data_eval)
{
  ID *data_orig = ob_orig->data;

  short *orig_totcol = BKE_id_material_len_p(data_orig);
  Material ***orig_mat = BKE_id_material_array_p(data_orig);

  short *eval_totcol = BKE_id_material_len_p(data_eval);
  Material ***eval_mat = BKE_id_material_array_p(data_eval);

  if (ELEM(NULL, orig_totcol, orig_mat, eval_totcol, eval_mat)) {
    return;
  }

  /* Remove old materials from original geometry. */
  for (int i = 0; i < *orig_totcol; i++) {
    id_us_min(&(*orig_mat)[i]->id);
  }
  MEM_SAFE_FREE(*orig_mat);

  /* Create new material slots based on materials on evaluated geometry. */
  *orig_totcol = *eval_totcol;
  *orig_mat = MEM_callocN(sizeof(void *) * (*eval_totcol), __func__);
  for (int i = 0; i < *eval_totcol; i++) {
    Material *material_eval = (*eval_mat)[i];
    if (material_eval != NULL) {
      Material *material_orig = (Material *)DEG_get_original_id(&material_eval->id);
      (*orig_mat)[i] = material_orig;
      id_us_plus(&material_orig->id);
    }
  }
  BKE_object_materials_test(bmain, ob_orig, data_orig);
}

/* XXX: this calls many more update calls per object then are needed, could be optimized. */
void BKE_object_material_array_assign(Main *bmain,
                                      struct Object *ob,
                                      struct Material ***matar,
                                      int totcol,
                                      const bool to_object_only)
{
  int actcol_orig = ob->actcol;

  while ((ob->totcol > totcol) && BKE_object_material_slot_remove(bmain, ob)) {
    /* pass */
  }

  /* now we have the right number of slots */
  for (int i = 0; i < totcol; i++) {
    if (to_object_only && ob->matbits[i] == 0) {
      /* If we only assign to object, and that slot uses obdata material, do nothing. */
      continue;
    }
    BKE_object_material_assign(bmain,
                               ob,
                               (*matar)[i],
                               i + 1,
                               to_object_only ? BKE_MAT_ASSIGN_OBJECT : BKE_MAT_ASSIGN_USERPREF);
  }

  if (actcol_orig > ob->totcol) {
    actcol_orig = ob->totcol;
  }

  ob->actcol = actcol_orig;
}

short BKE_object_material_slot_find_index(Object *ob, Material *ma)
{
  Material ***matarar;
  short a, *totcolp;

  if (ma == NULL) {
    return 0;
  }

  totcolp = BKE_object_material_len_p(ob);
  matarar = BKE_object_material_array_p(ob);

  if (totcolp == NULL || matarar == NULL) {
    return 0;
  }

  for (a = 0; a < *totcolp; a++) {
    if ((*matarar)[a] == ma) {
      break;
    }
  }
  if (a < *totcolp) {
    return a + 1;
  }
  return 0;
}

bool BKE_object_material_slot_add(Main *bmain, Object *ob)
{
  if (ob == NULL) {
    return false;
  }
  if (ob->totcol >= MAXMAT) {
    return false;
  }

  BKE_object_material_assign(bmain, ob, NULL, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);
  ob->actcol = ob->totcol;
  return true;
}

/* ****************** */

bool BKE_object_material_slot_remove(Main *bmain, Object *ob)
{
  Material *mao, ***matarar;
  short *totcolp;

  if (ob == NULL || ob->totcol == 0) {
    return false;
  }

  /* this should never happen and used to crash */
  if (ob->actcol <= 0) {
    CLOG_ERROR(&LOG, "invalid material index %d, report a bug!", ob->actcol);
    BLI_assert(0);
    return false;
  }

  /* take a mesh/curve/mball as starting point, remove 1 index,
   * AND with all objects that share the ob->data
   *
   * after that check indices in mesh/curve/mball!!!
   */

  totcolp = BKE_object_material_len_p(ob);
  matarar = BKE_object_material_array_p(ob);

  if (ELEM(NULL, matarar, *matarar)) {
    return false;
  }

  /* can happen on face selection in editmode */
  if (ob->actcol > ob->totcol) {
    ob->actcol = ob->totcol;
  }

  /* we delete the actcol */
  mao = (*matarar)[ob->actcol - 1];
  if (mao) {
    id_us_min(&mao->id);
  }

  for (int a = ob->actcol; a < ob->totcol; a++) {
    (*matarar)[a - 1] = (*matarar)[a];
  }
  (*totcolp)--;

  if (*totcolp == 0) {
    MEM_freeN(*matarar);
    *matarar = NULL;
  }

  const int actcol = ob->actcol;

  for (Object *obt = bmain->objects.first; obt; obt = obt->id.next) {
    if (obt->data == ob->data) {
      /* Can happen when object material lists are used, see: T52953 */
      if (actcol > obt->totcol) {
        continue;
      }
      /* WATCH IT: do not use actcol from ob or from obt (can become zero) */
      mao = obt->mat[actcol - 1];
      if (mao) {
        id_us_min(&mao->id);
      }

      for (int a = actcol; a < obt->totcol; a++) {
        obt->mat[a - 1] = obt->mat[a];
        obt->matbits[a - 1] = obt->matbits[a];
      }
      obt->totcol--;
      if (obt->actcol > obt->totcol) {
        obt->actcol = obt->totcol;
      }

      if (obt->totcol == 0) {
        MEM_freeN(obt->mat);
        MEM_freeN(obt->matbits);
        obt->mat = NULL;
        obt->matbits = NULL;
      }
    }
  }

  /* check indices from mesh */
  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT)) {
    material_data_index_remove_id((ID *)ob->data, actcol - 1);
    if (ob->runtime.curve_cache) {
      BKE_displist_free(&ob->runtime.curve_cache->disp);
    }
  }
  /* check indices from gpencil */
  else if (ob->type == OB_GPENCIL) {
    BKE_gpencil_material_index_reassign((bGPdata *)ob->data, ob->totcol, actcol - 1);
  }

  return true;
}

static bNode *nodetree_uv_node_recursive(bNode *node)
{
  bNode *inode;
  bNodeSocket *sock;

  for (sock = node->inputs.first; sock; sock = sock->next) {
    if (sock->link) {
      inode = sock->link->fromnode;
      if (inode->typeinfo->nclass == NODE_CLASS_INPUT && inode->typeinfo->type == SH_NODE_UVMAP) {
        return inode;
      }

      return nodetree_uv_node_recursive(inode);
    }
  }

  return NULL;
}

typedef bool (*ForEachTexNodeCallback)(bNode *node, void *userdata);
static bool ntree_foreach_texnode_recursive(bNodeTree *nodetree,
                                            ForEachTexNodeCallback callback,
                                            void *userdata)
{
  LISTBASE_FOREACH (bNode *, node, &nodetree->nodes) {
    if (node->typeinfo->nclass == NODE_CLASS_TEXTURE &&
        node->typeinfo->type == SH_NODE_TEX_IMAGE && node->id) {
      if (!callback(node, userdata)) {
        return false;
      }
    }
    else if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id) {
      /* recurse into the node group and see if it contains any textures */
      if (!ntree_foreach_texnode_recursive((bNodeTree *)node->id, callback, userdata)) {
        return false;
      }
    }
  }
  return true;
}

static bool count_texture_nodes_cb(bNode *UNUSED(node), void *userdata)
{
  (*((int *)userdata))++;
  return true;
}

static int count_texture_nodes_recursive(bNodeTree *nodetree)
{
  int tex_nodes = 0;
  ntree_foreach_texnode_recursive(nodetree, count_texture_nodes_cb, &tex_nodes);

  return tex_nodes;
}

struct FillTexPaintSlotsData {
  bNode *active_node;
  Material *ma;
  int index;
  int slot_len;
};

static bool fill_texpaint_slots_cb(bNode *node, void *userdata)
{
  struct FillTexPaintSlotsData *fill_data = userdata;

  Material *ma = fill_data->ma;
  int index = fill_data->index;
  fill_data->index++;

  if (fill_data->active_node == node) {
    ma->paint_active_slot = index;
  }

  ma->texpaintslot[index].ima = (Image *)node->id;
  ma->texpaintslot[index].interp = ((NodeTexImage *)node->storage)->interpolation;

  /* for new renderer, we need to traverse the treeback in search of a UV node */
  bNode *uvnode = nodetree_uv_node_recursive(node);

  if (uvnode) {
    NodeShaderUVMap *storage = (NodeShaderUVMap *)uvnode->storage;
    ma->texpaintslot[index].uvname = storage->uv_map;
    /* set a value to index so UI knows that we have a valid pointer for the mesh */
    ma->texpaintslot[index].valid = true;
  }
  else {
    /* just invalidate the index here so UV map does not get displayed on the UI */
    ma->texpaintslot[index].valid = false;
  }

  return fill_data->index != fill_data->slot_len;
}

static void fill_texpaint_slots_recursive(bNodeTree *nodetree,
                                          bNode *active_node,
                                          Material *ma,
                                          int slot_len)
{
  struct FillTexPaintSlotsData fill_data = {active_node, ma, 0, slot_len};
  ntree_foreach_texnode_recursive(nodetree, fill_texpaint_slots_cb, &fill_data);
}

void BKE_texpaint_slot_refresh_cache(Scene *scene, Material *ma)
{
  int count = 0;

  if (!ma) {
    return;
  }

  /* COW needed when adding texture slot on an object with no materials. */
  DEG_id_tag_update(&ma->id, ID_RECALC_SHADING | ID_RECALC_COPY_ON_WRITE);

  if (ma->texpaintslot) {
    MEM_freeN(ma->texpaintslot);
    ma->tot_slots = 0;
    ma->texpaintslot = NULL;
  }

  if (scene->toolsettings->imapaint.mode == IMAGEPAINT_MODE_IMAGE) {
    ma->paint_active_slot = 0;
    ma->paint_clone_slot = 0;
    return;
  }

  if (!(ma->nodetree)) {
    ma->paint_active_slot = 0;
    ma->paint_clone_slot = 0;
    return;
  }

  count = count_texture_nodes_recursive(ma->nodetree);

  if (count == 0) {
    ma->paint_active_slot = 0;
    ma->paint_clone_slot = 0;
    return;
  }

  ma->texpaintslot = MEM_callocN(sizeof(*ma->texpaintslot) * count, "texpaint_slots");

  bNode *active_node = nodeGetActiveTexture(ma->nodetree);

  fill_texpaint_slots_recursive(ma->nodetree, active_node, ma, count);

  ma->tot_slots = count;

  if (ma->paint_active_slot >= count) {
    ma->paint_active_slot = count - 1;
  }

  if (ma->paint_clone_slot >= count) {
    ma->paint_clone_slot = count - 1;
  }
}

void BKE_texpaint_slots_refresh_object(Scene *scene, struct Object *ob)
{
  for (int i = 1; i < ob->totcol + 1; i++) {
    Material *ma = BKE_object_material_get(ob, i);
    BKE_texpaint_slot_refresh_cache(scene, ma);
  }
}

struct FindTexPaintNodeData {
  Image *ima;
  bNode *r_node;
};

static bool texpaint_slot_node_find_cb(bNode *node, void *userdata)
{
  struct FindTexPaintNodeData *find_data = userdata;
  Image *ima = (Image *)node->id;
  if (find_data->ima == ima) {
    find_data->r_node = node;
    return false;
  }

  return true;
}

bNode *BKE_texpaint_slot_material_find_node(Material *ma, short texpaint_slot)
{
  struct FindTexPaintNodeData find_data = {ma->texpaintslot[texpaint_slot].ima, NULL};
  ntree_foreach_texnode_recursive(ma->nodetree, texpaint_slot_node_find_cb, &find_data);

  return find_data.r_node;
}

/* r_col = current value, col = new value, (fac == 0) is no change */
void ramp_blend(int type, float r_col[3], const float fac, const float col[3])
{
  float tmp, facm = 1.0f - fac;

  switch (type) {
    case MA_RAMP_BLEND:
      r_col[0] = facm * (r_col[0]) + fac * col[0];
      r_col[1] = facm * (r_col[1]) + fac * col[1];
      r_col[2] = facm * (r_col[2]) + fac * col[2];
      break;
    case MA_RAMP_ADD:
      r_col[0] += fac * col[0];
      r_col[1] += fac * col[1];
      r_col[2] += fac * col[2];
      break;
    case MA_RAMP_MULT:
      r_col[0] *= (facm + fac * col[0]);
      r_col[1] *= (facm + fac * col[1]);
      r_col[2] *= (facm + fac * col[2]);
      break;
    case MA_RAMP_SCREEN:
      r_col[0] = 1.0f - (facm + fac * (1.0f - col[0])) * (1.0f - r_col[0]);
      r_col[1] = 1.0f - (facm + fac * (1.0f - col[1])) * (1.0f - r_col[1]);
      r_col[2] = 1.0f - (facm + fac * (1.0f - col[2])) * (1.0f - r_col[2]);
      break;
    case MA_RAMP_OVERLAY:
      if (r_col[0] < 0.5f) {
        r_col[0] *= (facm + 2.0f * fac * col[0]);
      }
      else {
        r_col[0] = 1.0f - (facm + 2.0f * fac * (1.0f - col[0])) * (1.0f - r_col[0]);
      }
      if (r_col[1] < 0.5f) {
        r_col[1] *= (facm + 2.0f * fac * col[1]);
      }
      else {
        r_col[1] = 1.0f - (facm + 2.0f * fac * (1.0f - col[1])) * (1.0f - r_col[1]);
      }
      if (r_col[2] < 0.5f) {
        r_col[2] *= (facm + 2.0f * fac * col[2]);
      }
      else {
        r_col[2] = 1.0f - (facm + 2.0f * fac * (1.0f - col[2])) * (1.0f - r_col[2]);
      }
      break;
    case MA_RAMP_SUB:
      r_col[0] -= fac * col[0];
      r_col[1] -= fac * col[1];
      r_col[2] -= fac * col[2];
      break;
    case MA_RAMP_DIV:
      if (col[0] != 0.0f) {
        r_col[0] = facm * (r_col[0]) + fac * (r_col[0]) / col[0];
      }
      if (col[1] != 0.0f) {
        r_col[1] = facm * (r_col[1]) + fac * (r_col[1]) / col[1];
      }
      if (col[2] != 0.0f) {
        r_col[2] = facm * (r_col[2]) + fac * (r_col[2]) / col[2];
      }
      break;
    case MA_RAMP_DIFF:
      r_col[0] = facm * (r_col[0]) + fac * fabsf(r_col[0] - col[0]);
      r_col[1] = facm * (r_col[1]) + fac * fabsf(r_col[1] - col[1]);
      r_col[2] = facm * (r_col[2]) + fac * fabsf(r_col[2] - col[2]);
      break;
    case MA_RAMP_DARK:
      r_col[0] = min_ff(r_col[0], col[0]) * fac + r_col[0] * facm;
      r_col[1] = min_ff(r_col[1], col[1]) * fac + r_col[1] * facm;
      r_col[2] = min_ff(r_col[2], col[2]) * fac + r_col[2] * facm;
      break;
    case MA_RAMP_LIGHT:
      tmp = fac * col[0];
      if (tmp > r_col[0]) {
        r_col[0] = tmp;
      }
      tmp = fac * col[1];
      if (tmp > r_col[1]) {
        r_col[1] = tmp;
      }
      tmp = fac * col[2];
      if (tmp > r_col[2]) {
        r_col[2] = tmp;
      }
      break;
    case MA_RAMP_DODGE:
      if (r_col[0] != 0.0f) {
        tmp = 1.0f - fac * col[0];
        if (tmp <= 0.0f) {
          r_col[0] = 1.0f;
        }
        else if ((tmp = (r_col[0]) / tmp) > 1.0f) {
          r_col[0] = 1.0f;
        }
        else {
          r_col[0] = tmp;
        }
      }
      if (r_col[1] != 0.0f) {
        tmp = 1.0f - fac * col[1];
        if (tmp <= 0.0f) {
          r_col[1] = 1.0f;
        }
        else if ((tmp = (r_col[1]) / tmp) > 1.0f) {
          r_col[1] = 1.0f;
        }
        else {
          r_col[1] = tmp;
        }
      }
      if (r_col[2] != 0.0f) {
        tmp = 1.0f - fac * col[2];
        if (tmp <= 0.0f) {
          r_col[2] = 1.0f;
        }
        else if ((tmp = (r_col[2]) / tmp) > 1.0f) {
          r_col[2] = 1.0f;
        }
        else {
          r_col[2] = tmp;
        }
      }
      break;
    case MA_RAMP_BURN:
      tmp = facm + fac * col[0];

      if (tmp <= 0.0f) {
        r_col[0] = 0.0f;
      }
      else if ((tmp = (1.0f - (1.0f - (r_col[0])) / tmp)) < 0.0f) {
        r_col[0] = 0.0f;
      }
      else if (tmp > 1.0f) {
        r_col[0] = 1.0f;
      }
      else {
        r_col[0] = tmp;
      }

      tmp = facm + fac * col[1];
      if (tmp <= 0.0f) {
        r_col[1] = 0.0f;
      }
      else if ((tmp = (1.0f - (1.0f - (r_col[1])) / tmp)) < 0.0f) {
        r_col[1] = 0.0f;
      }
      else if (tmp > 1.0f) {
        r_col[1] = 1.0f;
      }
      else {
        r_col[1] = tmp;
      }

      tmp = facm + fac * col[2];
      if (tmp <= 0.0f) {
        r_col[2] = 0.0f;
      }
      else if ((tmp = (1.0f - (1.0f - (r_col[2])) / tmp)) < 0.0f) {
        r_col[2] = 0.0f;
      }
      else if (tmp > 1.0f) {
        r_col[2] = 1.0f;
      }
      else {
        r_col[2] = tmp;
      }
      break;
    case MA_RAMP_HUE: {
      float rH, rS, rV;
      float colH, colS, colV;
      float tmpr, tmpg, tmpb;
      rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
      if (colS != 0) {
        rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
        hsv_to_rgb(colH, rS, rV, &tmpr, &tmpg, &tmpb);
        r_col[0] = facm * (r_col[0]) + fac * tmpr;
        r_col[1] = facm * (r_col[1]) + fac * tmpg;
        r_col[2] = facm * (r_col[2]) + fac * tmpb;
      }
      break;
    }
    case MA_RAMP_SAT: {
      float rH, rS, rV;
      float colH, colS, colV;
      rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
      if (rS != 0) {
        rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
        hsv_to_rgb(rH, (facm * rS + fac * colS), rV, r_col + 0, r_col + 1, r_col + 2);
      }
      break;
    }
    case MA_RAMP_VAL: {
      float rH, rS, rV;
      float colH, colS, colV;
      rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
      rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
      hsv_to_rgb(rH, rS, (facm * rV + fac * colV), r_col + 0, r_col + 1, r_col + 2);
      break;
    }
    case MA_RAMP_COLOR: {
      float rH, rS, rV;
      float colH, colS, colV;
      float tmpr, tmpg, tmpb;
      rgb_to_hsv(col[0], col[1], col[2], &colH, &colS, &colV);
      if (colS != 0) {
        rgb_to_hsv(r_col[0], r_col[1], r_col[2], &rH, &rS, &rV);
        hsv_to_rgb(colH, colS, rV, &tmpr, &tmpg, &tmpb);
        r_col[0] = facm * (r_col[0]) + fac * tmpr;
        r_col[1] = facm * (r_col[1]) + fac * tmpg;
        r_col[2] = facm * (r_col[2]) + fac * tmpb;
      }
      break;
    }
    case MA_RAMP_SOFT: {
      float scr, scg, scb;

      /* first calculate non-fac based Screen mix */
      scr = 1.0f - (1.0f - col[0]) * (1.0f - r_col[0]);
      scg = 1.0f - (1.0f - col[1]) * (1.0f - r_col[1]);
      scb = 1.0f - (1.0f - col[2]) * (1.0f - r_col[2]);

      r_col[0] = facm * (r_col[0]) +
                 fac * (((1.0f - r_col[0]) * col[0] * (r_col[0])) + (r_col[0] * scr));
      r_col[1] = facm * (r_col[1]) +
                 fac * (((1.0f - r_col[1]) * col[1] * (r_col[1])) + (r_col[1] * scg));
      r_col[2] = facm * (r_col[2]) +
                 fac * (((1.0f - r_col[2]) * col[2] * (r_col[2])) + (r_col[2] * scb));
      break;
    }
    case MA_RAMP_LINEAR:
      if (col[0] > 0.5f) {
        r_col[0] = r_col[0] + fac * (2.0f * (col[0] - 0.5f));
      }
      else {
        r_col[0] = r_col[0] + fac * (2.0f * (col[0]) - 1.0f);
      }
      if (col[1] > 0.5f) {
        r_col[1] = r_col[1] + fac * (2.0f * (col[1] - 0.5f));
      }
      else {
        r_col[1] = r_col[1] + fac * (2.0f * (col[1]) - 1.0f);
      }
      if (col[2] > 0.5f) {
        r_col[2] = r_col[2] + fac * (2.0f * (col[2] - 0.5f));
      }
      else {
        r_col[2] = r_col[2] + fac * (2.0f * (col[2]) - 1.0f);
      }
      break;
  }
}

/**
 * \brief copy/paste buffer, if we had a proper py api that would be better
 * \note matcopybuf.nodetree does _NOT_ use ID's
 * \todo matcopybuf.nodetree's  node->id's are NOT validated, this will crash!
 */
static Material matcopybuf;
static short matcopied = 0;

void BKE_material_copybuf_clear(void)
{
  memset(&matcopybuf, 0, sizeof(Material));
  matcopied = 0;
}

void BKE_material_copybuf_free(void)
{
  if (matcopybuf.nodetree) {
    ntreeFreeLocalTree(matcopybuf.nodetree);
    BLI_assert(!matcopybuf.nodetree->id.py_instance); /* Or call #BKE_libblock_free_data_py. */
    MEM_freeN(matcopybuf.nodetree);
    matcopybuf.nodetree = NULL;
  }

  matcopied = 0;
}

void BKE_material_copybuf_copy(Main *bmain, Material *ma)
{
  if (matcopied) {
    BKE_material_copybuf_free();
  }

  memcpy(&matcopybuf, ma, sizeof(Material));

  if (ma->nodetree != NULL) {
    matcopybuf.nodetree = ntreeCopyTree_ex(ma->nodetree, bmain, false);
  }

  matcopybuf.preview = NULL;
  BLI_listbase_clear(&matcopybuf.gpumaterial);
  /* TODO: Duplicate Engine Settings and set runtime to NULL. */
  matcopied = 1;
}

void BKE_material_copybuf_paste(Main *bmain, Material *ma)
{
  ID id;

  if (matcopied == 0) {
    return;
  }

  /* Free gpu material before the ntree */
  GPU_material_free(&ma->gpumaterial);

  if (ma->nodetree) {
    ntreeFreeEmbeddedTree(ma->nodetree);
    MEM_freeN(ma->nodetree);
  }

  id = (ma->id);
  memcpy(ma, &matcopybuf, sizeof(Material));
  (ma->id) = id;

  if (matcopybuf.nodetree != NULL) {
    ma->nodetree = ntreeCopyTree_ex(matcopybuf.nodetree, bmain, false);
  }
}

void BKE_material_eval(struct Depsgraph *depsgraph, Material *material)
{
  DEG_debug_print_eval(depsgraph, __func__, material->id.name, material);
  GPU_material_free(&material->gpumaterial);
}

/* Default Materials
 *
 * Used for rendering when objects have no materials assigned, and initializing
 * default shader nodes. */

static Material default_material_empty;
static Material default_material_holdout;
static Material default_material_surface;
static Material default_material_volume;
static Material default_material_gpencil;

static Material *default_materials[] = {&default_material_empty,
                                        &default_material_holdout,
                                        &default_material_surface,
                                        &default_material_volume,
                                        &default_material_gpencil,
                                        NULL};

static void material_default_gpencil_init(Material *ma)
{
  strcpy(ma->id.name, "MADefault GPencil");
  BKE_gpencil_material_attr_init(ma);
  add_v3_fl(&ma->gp_style->stroke_rgba[0], 0.6f);
}

static void material_default_surface_init(Material *ma)
{
  bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
  ma->nodetree = ntree;
  ma->use_nodes = true;

  bNode *principled = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_PRINCIPLED);
  bNodeSocket *base_color = nodeFindSocket(principled, SOCK_IN, "Base Color");
  copy_v3_v3(((bNodeSocketValueRGBA *)base_color->default_value)->value, &ma->r);

  bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

  nodeAddLink(ntree,
              principled,
              nodeFindSocket(principled, SOCK_OUT, "BSDF"),
              output,
              nodeFindSocket(output, SOCK_IN, "Surface"));

  principled->locx = 10.0f;
  principled->locy = 300.0f;
  output->locx = 300.0f;
  output->locy = 300.0f;

  nodeSetActive(ntree, output);
}

static void material_default_volume_init(Material *ma)
{
  bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
  ma->nodetree = ntree;
  ma->use_nodes = true;

  bNode *principled = nodeAddStaticNode(NULL, ntree, SH_NODE_VOLUME_PRINCIPLED);
  bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

  nodeAddLink(ntree,
              principled,
              nodeFindSocket(principled, SOCK_OUT, "Volume"),
              output,
              nodeFindSocket(output, SOCK_IN, "Volume"));

  principled->locx = 10.0f;
  principled->locy = 300.0f;
  output->locx = 300.0f;
  output->locy = 300.0f;

  nodeSetActive(ntree, output);
}

static void material_default_holdout_init(Material *ma)
{
  bNodeTree *ntree = ntreeAddTree(NULL, "Shader Nodetree", ntreeType_Shader->idname);
  ma->nodetree = ntree;
  ma->use_nodes = true;

  bNode *holdout = nodeAddStaticNode(NULL, ntree, SH_NODE_HOLDOUT);
  bNode *output = nodeAddStaticNode(NULL, ntree, SH_NODE_OUTPUT_MATERIAL);

  nodeAddLink(ntree,
              holdout,
              nodeFindSocket(holdout, SOCK_OUT, "Holdout"),
              output,
              nodeFindSocket(output, SOCK_IN, "Surface"));

  holdout->locx = 10.0f;
  holdout->locy = 300.0f;
  output->locx = 300.0f;
  output->locy = 300.0f;

  nodeSetActive(ntree, output);
}

Material *BKE_material_default_empty(void)
{
  return &default_material_empty;
}

Material *BKE_material_default_holdout(void)
{
  return &default_material_holdout;
}

Material *BKE_material_default_surface(void)
{
  return &default_material_surface;
}

Material *BKE_material_default_volume(void)
{
  return &default_material_volume;
}

Material *BKE_material_default_gpencil(void)
{
  return &default_material_gpencil;
}

void BKE_material_defaults_free_gpu(void)
{
  for (int i = 0; default_materials[i]; i++) {
    Material *ma = default_materials[i];
    if (ma->gpumaterial.first) {
      GPU_material_free(&ma->gpumaterial);
    }
  }
}

/* Module functions called on startup and exit. */

void BKE_materials_init(void)
{
  for (int i = 0; default_materials[i]; i++) {
    material_init_data(&default_materials[i]->id);
  }

  material_default_surface_init(&default_material_surface);
  material_default_volume_init(&default_material_volume);
  material_default_holdout_init(&default_material_holdout);
  material_default_gpencil_init(&default_material_gpencil);
}

void BKE_materials_exit(void)
{
  for (int i = 0; default_materials[i]; i++) {
    material_free_data(&default_materials[i]->id);
  }
}
