/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstddef>
#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_customdata_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_grease_pencil_types.h"
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
#include "BKE_attribute.h"
#include "BKE_brush.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
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

  *((short *)id->name) = ID_MA;
}

static void material_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Material *material_dst = (Material *)id_dst;
  const Material *material_src = (const Material *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (material_src->nodetree != nullptr) {
    if (is_localized) {
      material_dst->nodetree = ntreeLocalize(material_src->nodetree);
    }
    else {
      BKE_id_copy_ex(bmain,
                     (ID *)material_src->nodetree,
                     (ID **)&material_dst->nodetree,
                     flag_private_id_data);
    }
    material_dst->nodetree->owner_id = &material_dst->id;
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&material_dst->id, &material_src->id);
  }
  else {
    material_dst->preview = nullptr;
  }

  if (material_src->texpaintslot != nullptr) {
    /* TODO: Think we can also skip copying this data in the more generic `NO_MAIN` case? */
    material_dst->texpaintslot = is_localized ? nullptr :
                                                static_cast<TexPaintSlot *>(
                                                    MEM_dupallocN(material_src->texpaintslot));
  }

  if (material_src->gp_style != nullptr) {
    material_dst->gp_style = static_cast<MaterialGPencilStyle *>(
        MEM_dupallocN(material_src->gp_style));
  }

  BLI_listbase_clear(&material_dst->gpumaterial);

  /* TODO: Duplicate Engine Settings and set runtime to nullptr. */
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
    material->nodetree = nullptr;
  }

  MEM_SAFE_FREE(material->texpaintslot);

  MEM_SAFE_FREE(material->gp_style);

  BKE_previewimg_free(&material->preview);

  BKE_icon_id_delete((ID *)material);
}

static void material_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Material *material = (Material *)id;
  /* Nodetrees **are owned by IDs**, treat them as mere sub-data and not real ID! */
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
      data, BKE_library_foreach_ID_embedded(data, (ID **)&material->nodetree));
  if (material->texpaintslot != nullptr) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->texpaintslot->ima, IDWALK_CB_NOP);
  }
  if (material->gp_style != nullptr) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->gp_style->sima, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, material->gp_style->ima, IDWALK_CB_USER);
  }
}

static void material_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Material *ma = (Material *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  ma->texpaintslot = nullptr;
  BLI_listbase_clear(&ma->gpumaterial);

  /* write LibData */
  BLO_write_id_struct(writer, Material, id_address, &ma->id);
  BKE_id_blend_write(writer, &ma->id);

  if (ma->adt) {
    BKE_animdata_blend_write(writer, ma->adt);
  }

  /* nodetree is integral part of material, no libdata */
  if (ma->nodetree) {
    BLO_Write_IDBuffer *temp_embedded_id_buffer = BLO_write_allocate_id_buffer();
    BLO_write_init_id_buffer_from_id(
        temp_embedded_id_buffer, &ma->nodetree->id, BLO_write_is_undo(writer));
    BLO_write_struct_at_address(
        writer, bNodeTree, ma->nodetree, BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer));
    ntreeBlendWrite(
        writer,
        reinterpret_cast<bNodeTree *>(BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer)));
    BLO_write_destroy_id_buffer(&temp_embedded_id_buffer);
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

  ma->texpaintslot = nullptr;

  BLO_read_data_address(reader, &ma->preview);
  BKE_previewimg_blend_read(reader, ma->preview);

  BLI_listbase_clear(&ma->gpumaterial);

  BLO_read_data_address(reader, &ma->gp_style);
}

static void material_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Material *ma = (Material *)id;
  BLO_read_id_address(reader, id, &ma->ipo); /* XXX deprecated - old animation system */

  /* relink grease pencil settings */
  if (ma->gp_style != nullptr) {
    MaterialGPencilStyle *gp_style = ma->gp_style;
    if (gp_style->sima != nullptr) {
      BLO_read_id_address(reader, id, &gp_style->sima);
    }
    if (gp_style->ima != nullptr) {
      BLO_read_id_address(reader, id, &gp_style->ima);
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
    /*id_code*/ ID_MA,
    /*id_filter*/ FILTER_ID_MA,
    /*main_listbase_index*/ INDEX_ID_MA,
    /*struct_size*/ sizeof(Material),
    /*name*/ "Material",
    /*name_plural*/ "materials",
    /*translation_context*/ BLT_I18NCONTEXT_ID_MATERIAL,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ material_init_data,
    /*copy_data*/ material_copy_data,
    /*free_data*/ material_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ material_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ material_blend_write,
    /*blend_read_data*/ material_blend_read_data,
    /*blend_read_lib*/ material_blend_read_lib,
    /*blend_read_expand*/ material_blend_read_expand,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

void BKE_gpencil_material_attr_init(Material *ma)
{
  if ((ma) && (ma->gp_style == nullptr)) {
    ma->gp_style = static_cast<MaterialGPencilStyle *>(
        MEM_callocN(sizeof(MaterialGPencilStyle), "Grease Pencil Material Settings"));

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

  ma = static_cast<Material *>(BKE_id_new(bmain, ID_MA, name));

  return ma;
}

Material *BKE_gpencil_material_add(Main *bmain, const char *name)
{
  Material *ma;

  ma = BKE_material_add(bmain, name);

  /* grease pencil settings */
  if (ma != nullptr) {
    BKE_gpencil_material_attr_init(ma);
  }
  return ma;
}

Material ***BKE_object_material_array_p(Object *ob)
{
  if (ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    return &(me->mat);
  }
  if (ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(ob->data);
    return &(cu->mat);
  }
  if (ob->type == OB_MBALL) {
    MetaBall *mb = static_cast<MetaBall *>(ob->data);
    return &(mb->mat);
  }
  if (ob->type == OB_GPENCIL_LEGACY) {
    bGPdata *gpd = static_cast<bGPdata *>(ob->data);
    return &(gpd->mat);
  }
  if (ob->type == OB_CURVES) {
    Curves *curves = static_cast<Curves *>(ob->data);
    return &(curves->mat);
  }
  if (ob->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(ob->data);
    return &(pointcloud->mat);
  }
  if (ob->type == OB_VOLUME) {
    Volume *volume = static_cast<Volume *>(ob->data);
    return &(volume->mat);
  }
  if (ob->type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
    return &(grease_pencil->material_array);
  }
  return nullptr;
}

short *BKE_object_material_len_p(Object *ob)
{
  if (ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    return &(me->totcol);
  }
  if (ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(ob->data);
    return &(cu->totcol);
  }
  if (ob->type == OB_MBALL) {
    MetaBall *mb = static_cast<MetaBall *>(ob->data);
    return &(mb->totcol);
  }
  if (ob->type == OB_GPENCIL_LEGACY) {
    bGPdata *gpd = static_cast<bGPdata *>(ob->data);
    return &(gpd->totcol);
  }
  if (ob->type == OB_CURVES) {
    Curves *curves = static_cast<Curves *>(ob->data);
    return &(curves->totcol);
  }
  if (ob->type == OB_POINTCLOUD) {
    PointCloud *pointcloud = static_cast<PointCloud *>(ob->data);
    return &(pointcloud->totcol);
  }
  if (ob->type == OB_VOLUME) {
    Volume *volume = static_cast<Volume *>(ob->data);
    return &(volume->totcol);
  }
  if (ob->type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
    return &(grease_pencil->material_array_size);
  }
  return nullptr;
}

Material ***BKE_id_material_array_p(ID *id)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      return &(((Mesh *)id)->mat);
    case ID_CU_LEGACY:
      return &(((Curve *)id)->mat);
    case ID_MB:
      return &(((MetaBall *)id)->mat);
    case ID_GD_LEGACY:
      return &(((bGPdata *)id)->mat);
    case ID_CV:
      return &(((Curves *)id)->mat);
    case ID_PT:
      return &(((PointCloud *)id)->mat);
    case ID_VO:
      return &(((Volume *)id)->mat);
    case ID_GP:
      return &(((GreasePencil *)id)->material_array);
    default:
      break;
  }
  return nullptr;
}

short *BKE_id_material_len_p(ID *id)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      return &(((Mesh *)id)->totcol);
    case ID_CU_LEGACY:
      return &(((Curve *)id)->totcol);
    case ID_MB:
      return &(((MetaBall *)id)->totcol);
    case ID_GD_LEGACY:
      return &(((bGPdata *)id)->totcol);
    case ID_CV:
      return &(((Curves *)id)->totcol);
    case ID_PT:
      return &(((PointCloud *)id)->totcol);
    case ID_VO:
      return &(((Volume *)id)->totcol);
    case ID_GP:
      return &(((GreasePencil *)id)->material_array_size);
    default:
      break;
  }
  return nullptr;
}

static void material_data_index_remove_id(ID *id, short index)
{
  /* ensure we don't try get materials from non-obdata */
  BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));

  switch (GS(id->name)) {
    case ID_ME:
      BKE_mesh_material_index_remove((Mesh *)id, index);
      break;
    case ID_CU_LEGACY:
      BKE_curve_material_index_remove((Curve *)id, index);
      break;
    case ID_MB:
    case ID_CV:
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

  ID *ob_data = static_cast<ID *>(object->data);
  if (ob_data == nullptr || !OB_DATA_SUPPORT_ID(GS(ob_data->name))) {
    return false;
  }

  switch (GS(ob_data->name)) {
    case ID_ME:
      return BKE_mesh_material_index_used((Mesh *)ob_data, actcol - 1);
    case ID_CU_LEGACY:
      return BKE_curve_material_index_used((Curve *)ob_data, actcol - 1);
    case ID_MB:
      /* Meta-elements don't support materials at the moment. */
      return false;
    case ID_GD_LEGACY:
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
    case ID_CU_LEGACY:
      BKE_curve_material_index_clear((Curve *)id);
      break;
    case ID_MB:
    case ID_CV:
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
    (*matar_dst) = static_cast<Material **>(MEM_dupallocN(*matar_src));

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

  if (matar == nullptr) {
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
      *matar = nullptr;
    }
  }
  else {
    *matar = static_cast<Material **>(MEM_recallocN(*matar, sizeof(void *) * totcol));
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
    Material **mat = MEM_cnew_array<Material *>((*totcol) + 1, "newmatar");
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
  short index = short(index_i);
  Material *ret = nullptr;
  Material ***matar;
  if ((matar = BKE_id_material_array_p(id))) {
    short *totcol = BKE_id_material_len_p(id);
    if (index >= 0 && index < (*totcol)) {
      ret = (*matar)[index];
      id_us_min((ID *)ret);

      if (*totcol <= 1) {
        *totcol = 0;
        MEM_freeN(*matar);
        *matar = nullptr;
      }
      else {
        if (index + 1 != (*totcol)) {
          memmove((*matar) + index,
                  (*matar) + (index + 1),
                  sizeof(void *) * ((*totcol) - (index + 1)));
        }

        (*totcol)--;
        *matar = static_cast<Material **>(MEM_reallocN(*matar, sizeof(void *) * (*totcol)));
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
      *matar = nullptr;
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

  if (ob == nullptr) {
    return nullptr;
  }

  /* if object cannot have material, (totcolp == nullptr) */
  totcolp = BKE_object_material_len_p(ob);
  if (totcolp == nullptr || *totcolp == 0) {
    return nullptr;
  }

  /* Clamp to number of slots if index is out of range, same convention as used for rendering. */
  const int slot_index = clamp_i(act - 1, 0, *totcolp - 1);

  /* Fix inconsistency which may happen when library linked data reduces the number of
   * slots but object was not updated. Ideally should be fixed elsewhere. */
  if (*totcolp < ob->totcol) {
    ob->totcol = *totcolp;
  }

  if (slot_index < ob->totcol && ob->matbits && ob->matbits[slot_index]) {
    /* Use object material slot. */
    ma_p = &ob->mat[slot_index];
  }
  else {
    /* Use data material slot. */
    matarar = BKE_object_material_array_p(ob);

    if (matarar && *matarar) {
      ma_p = &(*matarar)[slot_index];
    }
    else {
      ma_p = nullptr;
    }
  }

  return ma_p;
}

Material *BKE_object_material_get(Object *ob, short act)
{
  Material **ma_p = BKE_object_material_get_p(ob, act);
  return ma_p ? *ma_p : nullptr;
}

static ID *get_evaluated_object_data_with_materials(Object *ob)
{
  ID *data = static_cast<ID *>(ob->data);
  /* Meshes in edit mode need special handling. */
  if (ob->type == OB_MESH && ob->mode == OB_MODE_EDIT) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(ob);
    if (mesh->edit_mesh && editmesh_eval_final) {
      data = &editmesh_eval_final->id;
    }
  }
  return data;
}

Material *BKE_object_material_get_eval(Object *ob, short act)
{
  BLI_assert(DEG_is_evaluated_object(ob));

  ID *data = get_evaluated_object_data_with_materials(ob);
  const short *tot_slots_data_ptr = BKE_id_material_len_p(data);
  const int tot_slots_data = tot_slots_data_ptr ? *tot_slots_data_ptr : 0;

  if (tot_slots_data == 0) {
    return nullptr;
  }

  /* Clamp to number of slots if index is out of range, same convention as used for rendering. */
  const int slot_index = clamp_i(act - 1, 0, tot_slots_data - 1);
  const int tot_slots_object = ob->totcol;

  Material ***materials_data_ptr = BKE_id_material_array_p(data);
  Material **materials_data = materials_data_ptr ? *materials_data_ptr : nullptr;
  Material **materials_object = ob->mat;

  /* Check if slot is overwritten by object. */
  if (slot_index < tot_slots_object) {
    if (ob->matbits) {
      if (ob->matbits[slot_index]) {
        Material *material = materials_object[slot_index];
        if (material != nullptr) {
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
  return nullptr;
}

int BKE_object_material_count_eval(Object *ob)
{
  BLI_assert(DEG_is_evaluated_object(ob));
  if (ob->type == OB_EMPTY) {
    return 0;
  }
  BLI_assert(ob->data != nullptr);
  ID *id = get_evaluated_object_data_with_materials(ob);
  const short *len_p = BKE_id_material_len_p(id);
  return len_p ? *len_p : 0;
}

void BKE_id_material_eval_assign(ID *id, int slot, Material *material)
{
  BLI_assert(slot >= 1);
  Material ***materials_ptr = BKE_id_material_array_p(id);
  short *len_ptr = BKE_id_material_len_p(id);
  if (ELEM(nullptr, materials_ptr, len_ptr)) {
    BLI_assert_unreachable();
    return;
  }

  const int slot_index = slot - 1;
  const int old_length = *len_ptr;

  if (slot_index >= old_length) {
    /* Need to grow slots array. */
    const int new_length = slot_index + 1;
    *materials_ptr = static_cast<Material **>(
        MEM_reallocN(*materials_ptr, sizeof(void *) * new_length));
    *len_ptr = new_length;
    for (int i = old_length; i < new_length; i++) {
      (*materials_ptr)[i] = nullptr;
    }
  }

  (*materials_ptr)[slot_index] = material;
}

void BKE_id_material_eval_ensure_default_slot(ID *id)
{
  short *len_ptr = BKE_id_material_len_p(id);
  if (len_ptr == nullptr) {
    return;
  }
  if (*len_ptr == 0) {
    BKE_id_material_eval_assign(id, 1, nullptr);
  }
}

Material *BKE_gpencil_material(Object *ob, short act)
{
  Material *ma = BKE_object_material_get(ob, act);
  if (ma != nullptr) {
    return ma;
  }

  return BKE_material_default_gpencil();
}

MaterialGPencilStyle *BKE_gpencil_material_settings(Object *ob, short act)
{
  Material *ma = BKE_object_material_get(ob, act);
  if (ma != nullptr) {
    if (ma->gp_style == nullptr) {
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
      ob->mat = nullptr;
      ob->matbits = nullptr;
    }
  }
  else if (ob->totcol < totcol) {
    newmatar = MEM_cnew_array<Material *>(totcol, "newmatar");
    newmatbits = MEM_cnew_array<char>(totcol, "newmatbits");
    if (ob->totcol) {
      memcpy(newmatar, ob->mat, sizeof(void *) * ob->totcol);
      memcpy(newmatbits, ob->matbits, sizeof(char) * ob->totcol);
      MEM_freeN(ob->mat);
      MEM_freeN(ob->matbits);
    }
    ob->mat = newmatar;
    ob->matbits = newmatbits;
  }
  /* XXX(@ideasman42): why not realloc on shrink? */

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

  if (id == nullptr || (totcol = BKE_id_material_len_p(id)) == nullptr) {
    return;
  }

  if ((ob->id.tag & LIB_TAG_MISSING) == 0 && (id->tag & LIB_TAG_MISSING) != 0) {
    /* Exception: In case the object is a valid data, but its obdata is an empty place-holder,
     * use object's material slots amount as reference.
     * This avoids losing materials in a local object when its linked obdata goes missing.
     * See #92780. */
    BKE_id_material_resize(bmain, id, short(ob->totcol), false);
  }
  else {
    /* Normal case: the use the obdata amount of materials slots to update the object's one. */
    BKE_object_material_resize(bmain, ob, *totcol, false);
  }
}

void BKE_objects_materials_test_all(Main *bmain, ID *id)
{
  /* make the ob mat-array same size as 'ob->data' mat-array */
  Object *ob;
  const short *totcol;

  if (id == nullptr || (totcol = BKE_id_material_len_p(id)) == nullptr) {
    return;
  }

  BKE_main_lock(bmain);
  int processed_objects = 0;
  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next)) {
    if (ob->data == id) {
      BKE_object_material_resize(bmain, ob, *totcol, false);
      processed_objects++;
      BLI_assert(processed_objects <= id->us && processed_objects > 0);
      if (processed_objects == id->us) {
        break;
      }
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

  if (totcolp == nullptr || matarar == nullptr) {
    return;
  }

  if (act > *totcolp) {
    matar = MEM_cnew_array<Material *>(act, "matarray1");

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

static void object_material_assign(
    Main *bmain, Object *ob, Material *ma, short act, int assign_type, bool do_test_all)
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

  if (totcolp == nullptr || matarar == nullptr) {
    return;
  }

  if (act > *totcolp) {
    matar = MEM_cnew_array<Material *>(act, "matarray1");

    if (*totcolp) {
      memcpy(matar, *matarar, sizeof(void *) * (*totcolp));
      MEM_freeN(*matarar);
    }

    *matarar = matar;
    *totcolp = act;
  }

  if (act > ob->totcol) {
    /* Need more space in the material arrays */
    ob->mat = static_cast<Material **>(
        MEM_recallocN_id(ob->mat, sizeof(void *) * act, "matarray2"));
    ob->matbits = static_cast<char *>(
        MEM_recallocN_id(ob->matbits, sizeof(char) * act, "matbits1"));
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
    BKE_object_materials_test(bmain, ob, static_cast<ID *>(ob->data));
  }
  else { /* in data */
    mao = (*matarar)[act - 1];
    if (mao) {
      id_us_min(&mao->id);
    }
    (*matarar)[act - 1] = ma;
    /* Data may be used by several objects. */
    if (do_test_all) {
      BKE_objects_materials_test_all(bmain, static_cast<ID *>(ob->data));
    }
  }

  if (ma) {
    id_us_plus(&ma->id);
  }
}

void BKE_object_material_assign(Main *bmain, Object *ob, Material *ma, short act, int assign_type)
{
  object_material_assign(bmain, ob, ma, act, assign_type, true);
}

void BKE_object_material_assign_single_obdata(Main *bmain, Object *ob, Material *ma, short act)
{
  object_material_assign(bmain, ob, ma, act, BKE_MAT_ASSIGN_OBDATA, false);
}

void BKE_object_material_remap(Object *ob, const uint *remap)
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
    BKE_mesh_material_remap(static_cast<Mesh *>(ob->data), remap, ob->totcol);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    BKE_curve_material_remap(static_cast<Curve *>(ob->data), remap, ob->totcol);
  }
  else if (ob->type == OB_GPENCIL_LEGACY) {
    BKE_gpencil_material_remap(static_cast<bGPdata *>(ob->data), remap, ob->totcol);
  }
  else {
    /* add support for this object data! */
    BLI_assert(matar == nullptr);
  }
}

void BKE_object_material_remap_calc(Object *ob_dst, Object *ob_src, short *remap_src_to_dst)
{
  if (ob_src->totcol == 0) {
    return;
  }

  GHash *gh_mat_map = BLI_ghash_ptr_new_ex(__func__, ob_src->totcol);

  for (int i = 0; i < ob_dst->totcol; i++) {
    Material *ma_src = BKE_object_material_get(ob_dst, i + 1);
    BLI_ghash_reinsert(gh_mat_map, ma_src, POINTER_FROM_INT(i), nullptr, nullptr);
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

  BLI_ghash_free(gh_mat_map, nullptr, nullptr);
}

void BKE_object_material_from_eval_data(Main *bmain, Object *ob_orig, const ID *data_eval)
{
  ID *data_orig = static_cast<ID *>(ob_orig->data);

  short *orig_totcol = BKE_id_material_len_p(data_orig);
  Material ***orig_mat = BKE_id_material_array_p(data_orig);

  /* Can cast away const, because the data is not changed. */
  const short *eval_totcol = BKE_id_material_len_p((ID *)data_eval);
  Material ***eval_mat = BKE_id_material_array_p((ID *)data_eval);

  if (ELEM(nullptr, orig_totcol, orig_mat, eval_totcol, eval_mat)) {
    return;
  }

  /* Remove old materials from original geometry. */
  for (int i = 0; i < *orig_totcol; i++) {
    id_us_min(&(*orig_mat)[i]->id);
  }
  MEM_SAFE_FREE(*orig_mat);

  /* Create new material slots based on materials on evaluated geometry. */
  *orig_totcol = *eval_totcol;
  *orig_mat = MEM_cnew_array<Material *>(*eval_totcol, __func__);
  for (int i = 0; i < *eval_totcol; i++) {
    Material *material_eval = (*eval_mat)[i];
    if (material_eval != nullptr) {
      Material *material_orig = (Material *)DEG_get_original_id(&material_eval->id);
      (*orig_mat)[i] = material_orig;
      id_us_plus(&material_orig->id);
    }
  }
  BKE_object_materials_test(bmain, ob_orig, data_orig);
}

void BKE_object_material_array_assign(
    Main *bmain, Object *ob, Material ***matar, int totcol, const bool to_object_only)
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

  if (ma == nullptr) {
    return 0;
  }

  totcolp = BKE_object_material_len_p(ob);
  matarar = BKE_object_material_array_p(ob);

  if (totcolp == nullptr || matarar == nullptr) {
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
  if (ob == nullptr) {
    return false;
  }
  if (ob->totcol >= MAXMAT) {
    return false;
  }

  BKE_object_material_assign(bmain, ob, nullptr, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);
  ob->actcol = ob->totcol;
  return true;
}

/* ****************** */

bool BKE_object_material_slot_remove(Main *bmain, Object *ob)
{
  Material *mao, ***matarar;
  short *totcolp;

  if (ob == nullptr || ob->totcol == 0) {
    return false;
  }

  /* this should never happen and used to crash */
  if (ob->actcol <= 0) {
    CLOG_ERROR(&LOG, "invalid material index %d, report a bug!", ob->actcol);
    return false;
  }

  /* Take a mesh/curve/meta-ball as starting point, remove 1 index,
   * AND with all objects that share the `ob->data`.
   * After that check indices in mesh/curve/meta-ball! */

  totcolp = BKE_object_material_len_p(ob);
  matarar = BKE_object_material_array_p(ob);

  if (ELEM(nullptr, matarar, *matarar)) {
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
    *matarar = nullptr;
  }

  const int actcol = ob->actcol;

  for (Object *obt = static_cast<Object *>(bmain->objects.first); obt;
       obt = static_cast<Object *>(obt->id.next))
  {
    if (obt->data == ob->data) {
      /* Can happen when object material lists are used, see: #52953 */
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
        obt->mat = nullptr;
        obt->matbits = nullptr;
      }
    }
  }

  /* check indices from mesh */
  if (ELEM(ob->type, OB_MESH, OB_CURVES_LEGACY, OB_SURF, OB_FONT)) {
    material_data_index_remove_id((ID *)ob->data, actcol - 1);
    if (ob->runtime.curve_cache) {
      BKE_displist_free(&ob->runtime.curve_cache->disp);
    }
  }
  /* check indices from gpencil */
  else if (ob->type == OB_GPENCIL_LEGACY) {
    BKE_gpencil_material_index_reassign((bGPdata *)ob->data, ob->totcol, actcol - 1);
  }

  return true;
}

static bNode *nodetree_uv_node_recursive(bNode *node)
{
  bNode *inode;
  bNodeSocket *sock;

  for (sock = static_cast<bNodeSocket *>(node->inputs.first); sock; sock = sock->next) {
    if (sock->link) {
      inode = sock->link->fromnode;
      if (inode->typeinfo->nclass == NODE_CLASS_INPUT && inode->typeinfo->type == SH_NODE_UVMAP) {
        return inode;
      }

      return nodetree_uv_node_recursive(inode);
    }
  }

  return nullptr;
}

/** Bitwise filter for updating paint slots. */
enum ePaintSlotFilter {
  PAINT_SLOT_IMAGE = 1 << 0,
  PAINT_SLOT_COLOR_ATTRIBUTE = 1 << 1,
};
ENUM_OPERATORS(ePaintSlotFilter, PAINT_SLOT_COLOR_ATTRIBUTE)

using ForEachTexNodeCallback = bool (*)(bNode *node, void *userdata);
static bool ntree_foreach_texnode_recursive(bNodeTree *nodetree,
                                            ForEachTexNodeCallback callback,
                                            void *userdata,
                                            ePaintSlotFilter slot_filter)
{
  const bool do_image_nodes = (slot_filter & PAINT_SLOT_IMAGE) != 0;
  const bool do_color_attributes = (slot_filter & PAINT_SLOT_COLOR_ATTRIBUTE) != 0;
  for (bNode *node : nodetree->all_nodes()) {
    if (do_image_nodes && node->typeinfo->nclass == NODE_CLASS_TEXTURE &&
        node->typeinfo->type == SH_NODE_TEX_IMAGE && node->id)
    {
      if (!callback(node, userdata)) {
        return false;
      }
    }
    if (do_color_attributes && node->typeinfo->type == SH_NODE_ATTRIBUTE) {
      if (!callback(node, userdata)) {
        return false;
      }
    }
    else if (ELEM(node->type, NODE_GROUP, NODE_CUSTOM_GROUP) && node->id) {
      /* recurse into the node group and see if it contains any textures */
      if (!ntree_foreach_texnode_recursive((bNodeTree *)node->id, callback, userdata, slot_filter))
      {
        return false;
      }
    }
  }
  return true;
}

static bool count_texture_nodes_cb(bNode * /*node*/, void *userdata)
{
  (*((int *)userdata))++;
  return true;
}

static int count_texture_nodes_recursive(bNodeTree *nodetree, ePaintSlotFilter slot_filter)
{
  int tex_nodes = 0;
  ntree_foreach_texnode_recursive(nodetree, count_texture_nodes_cb, &tex_nodes, slot_filter);

  return tex_nodes;
}

struct FillTexPaintSlotsData {
  bNode *active_node;
  const Object *ob;
  Material *ma;
  int index;
  int slot_len;
};

static bool fill_texpaint_slots_cb(bNode *node, void *userdata)
{
  FillTexPaintSlotsData *fill_data = static_cast<FillTexPaintSlotsData *>(userdata);

  Material *ma = fill_data->ma;
  int index = fill_data->index;
  fill_data->index++;

  if (fill_data->active_node == node) {
    ma->paint_active_slot = index;
  }

  switch (node->type) {
    case SH_NODE_TEX_IMAGE: {
      TexPaintSlot *slot = &ma->texpaintslot[index];
      slot->ima = (Image *)node->id;
      NodeTexImage *storage = (NodeTexImage *)node->storage;
      slot->interp = storage->interpolation;
      slot->image_user = &storage->iuser;
      /* For new renderer, we need to traverse the tree back in search of a UV node. */
      bNode *uvnode = nodetree_uv_node_recursive(node);

      if (uvnode) {
        NodeShaderUVMap *uv_storage = (NodeShaderUVMap *)uvnode->storage;
        slot->uvname = uv_storage->uv_map;
        /* set a value to index so UI knows that we have a valid pointer for the mesh */
        slot->valid = true;
      }
      else {
        /* just invalidate the index here so UV map does not get displayed on the UI */
        slot->valid = false;
      }
      break;
    }

    case SH_NODE_ATTRIBUTE: {
      TexPaintSlot *slot = &ma->texpaintslot[index];
      NodeShaderAttribute *storage = static_cast<NodeShaderAttribute *>(node->storage);
      slot->attribute_name = storage->name;
      if (storage->type == SHD_ATTRIBUTE_GEOMETRY) {
        const Mesh *mesh = (const Mesh *)fill_data->ob->data;
        const CustomDataLayer *layer = BKE_id_attributes_color_find(&mesh->id, storage->name);
        slot->valid = layer != nullptr;
      }

      /* Do not show unsupported attributes. */
      if (!slot->valid) {
        slot->attribute_name = nullptr;
        fill_data->index--;
      }

      break;
    }
  }

  return fill_data->index != fill_data->slot_len;
}

static void fill_texpaint_slots_recursive(bNodeTree *nodetree,
                                          bNode *active_node,
                                          const Object *ob,
                                          Material *ma,
                                          int slot_len,
                                          ePaintSlotFilter slot_filter)
{
  FillTexPaintSlotsData fill_data = {active_node, ob, ma, 0, slot_len};
  ntree_foreach_texnode_recursive(nodetree, fill_texpaint_slots_cb, &fill_data, slot_filter);
}

/** Check which type of paint slots should be filled for the given object. */
static ePaintSlotFilter material_paint_slot_filter(const Object *ob)
{
  ePaintSlotFilter slot_filter = PAINT_SLOT_IMAGE;
  if (ob->mode == OB_MODE_SCULPT && U.experimental.use_sculpt_texture_paint) {
    slot_filter |= PAINT_SLOT_COLOR_ATTRIBUTE;
  }
  return slot_filter;
}

void BKE_texpaint_slot_refresh_cache(Scene *scene, Material *ma, const Object *ob)
{
  if (!ma) {
    return;
  }

  const ePaintSlotFilter slot_filter = material_paint_slot_filter(ob);

  const TexPaintSlot *prev_texpaintslot = ma->texpaintslot;
  const int prev_paint_active_slot = ma->paint_active_slot;
  const int prev_paint_clone_slot = ma->paint_clone_slot;
  const int prev_tot_slots = ma->tot_slots;

  ma->texpaintslot = nullptr;
  ma->tot_slots = 0;

  if (scene->toolsettings->imapaint.mode == IMAGEPAINT_MODE_IMAGE) {
    ma->paint_active_slot = 0;
    ma->paint_clone_slot = 0;
  }
  else if (!(ma->nodetree)) {
    ma->paint_active_slot = 0;
    ma->paint_clone_slot = 0;
  }
  else {
    int count = count_texture_nodes_recursive(ma->nodetree, slot_filter);

    if (count == 0) {
      ma->paint_active_slot = 0;
      ma->paint_clone_slot = 0;
    }
    else {
      ma->texpaintslot = static_cast<TexPaintSlot *>(
          MEM_callocN(sizeof(TexPaintSlot) * count, "texpaint_slots"));

      bNode *active_node = blender::bke::nodeGetActivePaintCanvas(ma->nodetree);

      fill_texpaint_slots_recursive(ma->nodetree, active_node, ob, ma, count, slot_filter);

      ma->tot_slots = count;

      if (ma->paint_active_slot >= count) {
        ma->paint_active_slot = count - 1;
      }

      if (ma->paint_clone_slot >= count) {
        ma->paint_clone_slot = count - 1;
      }
    }
  }

  /* COW needed when adding texture slot on an object with no materials.
   * But do it only when slots actually change to avoid continuous depsgraph updates. */
  if (ma->tot_slots != prev_tot_slots || ma->paint_active_slot != prev_paint_active_slot ||
      ma->paint_clone_slot != prev_paint_clone_slot ||
      (ma->texpaintslot && prev_texpaintslot &&
       memcmp(ma->texpaintslot, prev_texpaintslot, sizeof(*ma->texpaintslot) * ma->tot_slots) !=
           0))
  {
    DEG_id_tag_update(&ma->id, ID_RECALC_SHADING | ID_RECALC_COPY_ON_WRITE);
  }

  MEM_SAFE_FREE(prev_texpaintslot);
}

void BKE_texpaint_slots_refresh_object(Scene *scene, Object *ob)
{
  for (int i = 1; i < ob->totcol + 1; i++) {
    Material *ma = BKE_object_material_get(ob, i);
    BKE_texpaint_slot_refresh_cache(scene, ma, ob);
  }
}

struct FindTexPaintNodeData {
  TexPaintSlot *slot;
  bNode *r_node;
};

static bool texpaint_slot_node_find_cb(bNode *node, void *userdata)
{
  FindTexPaintNodeData *find_data = static_cast<FindTexPaintNodeData *>(userdata);
  if (find_data->slot->ima && node->type == SH_NODE_TEX_IMAGE) {
    Image *node_ima = (Image *)node->id;
    if (find_data->slot->ima == node_ima) {
      find_data->r_node = node;
      return false;
    }
  }

  if (find_data->slot->attribute_name && node->type == SH_NODE_ATTRIBUTE) {
    NodeShaderAttribute *storage = static_cast<NodeShaderAttribute *>(node->storage);
    if (STREQLEN(find_data->slot->attribute_name, storage->name, sizeof(storage->name))) {
      find_data->r_node = node;
      return false;
    }
  }

  return true;
}

bNode *BKE_texpaint_slot_material_find_node(Material *ma, short texpaint_slot)
{
  TexPaintSlot *slot = &ma->texpaintslot[texpaint_slot];
  FindTexPaintNodeData find_data = {slot, nullptr};
  ntree_foreach_texnode_recursive(ma->nodetree,
                                  texpaint_slot_node_find_cb,
                                  &find_data,
                                  PAINT_SLOT_IMAGE | PAINT_SLOT_COLOR_ATTRIBUTE);

  return find_data.r_node;
}

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
    case MA_RAMP_EXCLUSION:
      r_col[0] = max_ff(facm * (r_col[0]) + fac * (r_col[0] + col[0] - 2.0f * r_col[0] * col[0]),
                        0.0f);
      r_col[1] = max_ff(facm * (r_col[1]) + fac * (r_col[1] + col[1] - 2.0f * r_col[1] * col[1]),
                        0.0f);
      r_col[2] = max_ff(facm * (r_col[2]) + fac * (r_col[2] + col[2] - 2.0f * r_col[2] * col[2]),
                        0.0f);
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

void BKE_material_eval(Depsgraph *depsgraph, Material *material)
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
                                        nullptr};

static void material_default_gpencil_init(Material *ma)
{
  BLI_strncpy(ma->id.name + 2, "Default GPencil", MAX_NAME);
  BKE_gpencil_material_attr_init(ma);
  add_v3_fl(&ma->gp_style->stroke_rgba[0], 0.6f);
}

static void material_default_surface_init(Material *ma)
{
  BLI_strncpy(ma->id.name + 2, "Default Surface", MAX_NAME);

  bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
  ma->use_nodes = true;

  bNode *principled = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_PRINCIPLED);
  bNodeSocket *base_color = nodeFindSocket(principled, SOCK_IN, "Base Color");
  copy_v3_v3(((bNodeSocketValueRGBA *)base_color->default_value)->value, &ma->r);

  bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

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
  BLI_strncpy(ma->id.name + 2, "Default Volume", MAX_NAME);

  bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
  ma->use_nodes = true;

  bNode *principled = nodeAddStaticNode(nullptr, ntree, SH_NODE_VOLUME_PRINCIPLED);
  bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

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
  BLI_strncpy(ma->id.name + 2, "Default Holdout", MAX_NAME);

  bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
      nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
  ma->use_nodes = true;

  bNode *holdout = nodeAddStaticNode(nullptr, ntree, SH_NODE_HOLDOUT);
  bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

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
