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
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>
#include <stdio.h>

#include "DNA_ID.h"
#include "DNA_vfont_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_types.h"

#include "rna_internal.h"

/* enum of ID-block types
 * NOTE: need to keep this in line with the other defines for these
 */
const EnumPropertyItem rna_enum_id_type_items[] = {
    {ID_AC, "ACTION", ICON_ACTION, "Action", ""},
    {ID_AR, "ARMATURE", ICON_ARMATURE_DATA, "Armature", ""},
    {ID_BR, "BRUSH", ICON_BRUSH_DATA, "Brush", ""},
    {ID_CA, "CAMERA", ICON_CAMERA_DATA, "Camera", ""},
    {ID_CF, "CACHEFILE", ICON_FILE, "Cache File", ""},
    {ID_CU, "CURVE", ICON_CURVE_DATA, "Curve", ""},
    {ID_VF, "FONT", ICON_FONT_DATA, "Font", ""},
    {ID_GD, "GREASEPENCIL", ICON_GREASEPENCIL, "Grease Pencil", ""},
    {ID_GR, "COLLECTION", ICON_GROUP, "Collection", ""},
    {ID_IM, "IMAGE", ICON_IMAGE_DATA, "Image", ""},
    {ID_KE, "KEY", ICON_SHAPEKEY_DATA, "Key", ""},
    {ID_LA, "LIGHT", ICON_LIGHT_DATA, "Light", ""},
    {ID_LI, "LIBRARY", ICON_LIBRARY_DATA_DIRECT, "Library", ""},
    {ID_LS, "LINESTYLE", ICON_LINE_DATA, "Line Style", ""},
    {ID_LT, "LATTICE", ICON_LATTICE_DATA, "Lattice", ""},
    {ID_MSK, "MASK", ICON_MOD_MASK, "Mask", ""},
    {ID_MA, "MATERIAL", ICON_MATERIAL_DATA, "Material", ""},
    {ID_MB, "META", ICON_META_DATA, "Metaball", ""},
    {ID_ME, "MESH", ICON_MESH_DATA, "Mesh", ""},
    {ID_MC, "MOVIECLIP", ICON_TRACKER, "Movie Clip", ""},
    {ID_NT, "NODETREE", ICON_NODETREE, "Node Tree", ""},
    {ID_OB, "OBJECT", ICON_OBJECT_DATA, "Object", ""},
    {ID_PC, "PAINTCURVE", ICON_CURVE_BEZCURVE, "Paint Curve", ""},
    {ID_PAL, "PALETTE", ICON_COLOR, "Palette", ""},
    {ID_PA, "PARTICLE", ICON_PARTICLE_DATA, "Particle", ""},
    {ID_LP, "LIGHT_PROBE", ICON_LIGHTPROBE_CUBEMAP, "Light Probe", ""},
    {ID_SCE, "SCENE", ICON_SCENE_DATA, "Scene", ""},
    {ID_SO, "SOUND", ICON_SOUND, "Sound", ""},
    {ID_SPK, "SPEAKER", ICON_SPEAKER, "Speaker", ""},
    {ID_TXT, "TEXT", ICON_TEXT, "Text", ""},
    {ID_TE, "TEXTURE", ICON_TEXTURE_DATA, "Texture", ""},
    {ID_WM, "WINDOWMANAGER", ICON_WINDOW, "Window Manager", ""},
    {ID_WO, "WORLD", ICON_WORLD_DATA, "World", ""},
    {ID_WS, "WORKSPACE", ICON_WORKSPACE, "Workspace", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "DNA_anim_types.h"

#  include "BLI_listbase.h"
#  include "BLI_math_base.h"

#  include "BKE_font.h"
#  include "BKE_idprop.h"
#  include "BKE_library_query.h"
#  include "BKE_library_override.h"
#  include "BKE_library_remap.h"
#  include "BKE_animsys.h"
#  include "BKE_material.h"
#  include "BKE_global.h" /* XXX, remove me */

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"
#  include "DEG_depsgraph_query.h"

#  include "WM_api.h"

void rna_ID_override_static_property_operation_refname_get(PointerRNA *ptr, char *value)
{
  IDOverrideStaticPropertyOperation *opop = ptr->data;
  strcpy(value, (opop->subitem_reference_name == NULL) ? "" : opop->subitem_reference_name);
}

int rna_ID_override_static_property_operation_refname_length(PointerRNA *ptr)
{
  IDOverrideStaticPropertyOperation *opop = ptr->data;
  return (opop->subitem_reference_name == NULL) ? 0 : strlen(opop->subitem_reference_name);
}

void rna_ID_override_static_property_operation_locname_get(PointerRNA *ptr, char *value)
{
  IDOverrideStaticPropertyOperation *opop = ptr->data;
  strcpy(value, (opop->subitem_local_name == NULL) ? "" : opop->subitem_local_name);
}

int rna_ID_override_static_property_operation_locname_length(PointerRNA *ptr)
{
  IDOverrideStaticPropertyOperation *opop = ptr->data;
  return (opop->subitem_local_name == NULL) ? 0 : strlen(opop->subitem_local_name);
}

/* name functions that ignore the first two ID characters */
void rna_ID_name_get(PointerRNA *ptr, char *value)
{
  ID *id = (ID *)ptr->data;
  BLI_strncpy(value, id->name + 2, sizeof(id->name) - 2);
}

int rna_ID_name_length(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;
  return strlen(id->name + 2);
}

void rna_ID_name_set(PointerRNA *ptr, const char *value)
{
  ID *id = (ID *)ptr->data;
  BLI_strncpy_utf8(id->name + 2, value, sizeof(id->name) - 2);
  BLI_assert(BKE_id_is_in_global_main(id));
  BLI_libblock_ensure_unique_name(G_MAIN, id->name);

  if (GS(id->name) == ID_OB) {
    Object *ob = (Object *)id;
    if (ob->type == OB_MBALL) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
}

static int rna_ID_name_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  ID *id = (ID *)ptr->data;

  if (GS(id->name) == ID_VF) {
    VFont *vfont = (VFont *)id;
    if (BKE_vfont_is_builtin(vfont))
      return 0;
  }
  else if (!BKE_id_is_in_global_main(id)) {
    return 0;
  }

  return PROP_EDITABLE;
}

void rna_ID_name_full_get(PointerRNA *ptr, char *value)
{
  ID *id = (ID *)ptr->data;
  BKE_id_full_name_get(value, id);
}

int rna_ID_name_full_length(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;
  char name[MAX_ID_FULL_NAME];
  BKE_id_full_name_get(name, id);
  return strlen(name);
}

static int rna_ID_is_evaluated_get(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;

  return (DEG_get_original_id(id) != id);
}

static PointerRNA rna_ID_original_get(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;

  return rna_pointer_inherit_refine(ptr, &RNA_ID, DEG_get_original_id(id));
}

short RNA_type_to_ID_code(const StructRNA *type)
{
  const StructRNA *base_type = RNA_struct_base_child_of(type, &RNA_ID);
  if (UNLIKELY(base_type == NULL)) {
    return 0;
  }
  if (base_type == &RNA_Action)
    return ID_AC;
  if (base_type == &RNA_Armature)
    return ID_AR;
  if (base_type == &RNA_Brush)
    return ID_BR;
  if (base_type == &RNA_CacheFile)
    return ID_CF;
  if (base_type == &RNA_Camera)
    return ID_CA;
  if (base_type == &RNA_Curve)
    return ID_CU;
  if (base_type == &RNA_GreasePencil)
    return ID_GD;
  if (base_type == &RNA_Collection)
    return ID_GR;
  if (base_type == &RNA_Image)
    return ID_IM;
  if (base_type == &RNA_Key)
    return ID_KE;
  if (base_type == &RNA_Light)
    return ID_LA;
  if (base_type == &RNA_Library)
    return ID_LI;
  if (base_type == &RNA_FreestyleLineStyle)
    return ID_LS;
  if (base_type == &RNA_Lattice)
    return ID_LT;
  if (base_type == &RNA_Material)
    return ID_MA;
  if (base_type == &RNA_MetaBall)
    return ID_MB;
  if (base_type == &RNA_MovieClip)
    return ID_MC;
  if (base_type == &RNA_Mesh)
    return ID_ME;
  if (base_type == &RNA_Mask)
    return ID_MSK;
  if (base_type == &RNA_NodeTree)
    return ID_NT;
  if (base_type == &RNA_Object)
    return ID_OB;
  if (base_type == &RNA_ParticleSettings)
    return ID_PA;
  if (base_type == &RNA_Palette)
    return ID_PAL;
  if (base_type == &RNA_PaintCurve)
    return ID_PC;
  if (base_type == &RNA_LightProbe)
    return ID_LP;
  if (base_type == &RNA_Scene)
    return ID_SCE;
  if (base_type == &RNA_Screen)
    return ID_SCR;
  if (base_type == &RNA_Sound)
    return ID_SO;
  if (base_type == &RNA_Speaker)
    return ID_SPK;
  if (base_type == &RNA_Texture)
    return ID_TE;
  if (base_type == &RNA_Text)
    return ID_TXT;
  if (base_type == &RNA_VectorFont)
    return ID_VF;
  if (base_type == &RNA_WorkSpace)
    return ID_WS;
  if (base_type == &RNA_World)
    return ID_WO;
  if (base_type == &RNA_WindowManager)
    return ID_WM;

  return 0;
}

StructRNA *ID_code_to_RNA_type(short idcode)
{
  /* Note, this switch doesn't use a 'default',
   * so adding new ID's causes a warning. */
  switch ((ID_Type)idcode) {
    case ID_AC:
      return &RNA_Action;
    case ID_AR:
      return &RNA_Armature;
    case ID_BR:
      return &RNA_Brush;
    case ID_CA:
      return &RNA_Camera;
    case ID_CF:
      return &RNA_CacheFile;
    case ID_CU:
      return &RNA_Curve;
    case ID_GD:
      return &RNA_GreasePencil;
    case ID_GR:
      return &RNA_Collection;
    case ID_IM:
      return &RNA_Image;
    case ID_KE:
      return &RNA_Key;
    case ID_LA:
      return &RNA_Light;
    case ID_LI:
      return &RNA_Library;
    case ID_LS:
      return &RNA_FreestyleLineStyle;
    case ID_LT:
      return &RNA_Lattice;
    case ID_MA:
      return &RNA_Material;
    case ID_MB:
      return &RNA_MetaBall;
    case ID_MC:
      return &RNA_MovieClip;
    case ID_ME:
      return &RNA_Mesh;
    case ID_MSK:
      return &RNA_Mask;
    case ID_NT:
      return &RNA_NodeTree;
    case ID_OB:
      return &RNA_Object;
    case ID_PA:
      return &RNA_ParticleSettings;
    case ID_PAL:
      return &RNA_Palette;
    case ID_PC:
      return &RNA_PaintCurve;
    case ID_LP:
      return &RNA_LightProbe;
    case ID_SCE:
      return &RNA_Scene;
    case ID_SCR:
      return &RNA_Screen;
    case ID_SO:
      return &RNA_Sound;
    case ID_SPK:
      return &RNA_Speaker;
    case ID_TE:
      return &RNA_Texture;
    case ID_TXT:
      return &RNA_Text;
    case ID_VF:
      return &RNA_VectorFont;
    case ID_WM:
      return &RNA_WindowManager;
    case ID_WO:
      return &RNA_World;
    case ID_WS:
      return &RNA_WorkSpace;

    /* deprecated */
    case ID_IP:
      break;
  }

  return &RNA_ID;
}

StructRNA *rna_ID_refine(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;

  return ID_code_to_RNA_type(GS(id->name));
}

IDProperty *rna_ID_idprops(PointerRNA *ptr, bool create)
{
  return IDP_GetProperties(ptr->data, create);
}

void rna_ID_fake_user_set(PointerRNA *ptr, bool value)
{
  ID *id = (ID *)ptr->data;

  if (value) {
    id_fake_user_set(id);
  }
  else {
    id_fake_user_clear(id);
  }
}

IDProperty *rna_PropertyGroup_idprops(PointerRNA *ptr, bool UNUSED(create))
{
  return ptr->data;
}

void rna_PropertyGroup_unregister(Main *UNUSED(bmain), StructRNA *type)
{
  RNA_struct_free(&BLENDER_RNA, type);
}

StructRNA *rna_PropertyGroup_register(Main *UNUSED(bmain),
                                      ReportList *reports,
                                      void *data,
                                      const char *identifier,
                                      StructValidateFunc validate,
                                      StructCallbackFunc UNUSED(call),
                                      StructFreeFunc UNUSED(free))
{
  PointerRNA dummyptr;

  /* create dummy pointer */
  RNA_pointer_create(NULL, &RNA_PropertyGroup, NULL, &dummyptr);

  /* validate the python class */
  if (validate(&dummyptr, data, NULL) != 0)
    return NULL;

  /* note: it looks like there is no length limit on the srna id since its
   * just a char pointer, but take care here, also be careful that python
   * owns the string pointer which it could potentially free while blender
   * is running. */
  if (BLI_strnlen(identifier, MAX_IDPROP_NAME) == MAX_IDPROP_NAME) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering id property class: '%s' is too long, maximum length is %d",
                identifier,
                MAX_IDPROP_NAME);
    return NULL;
  }

  return RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_PropertyGroup); /* XXX */
}

StructRNA *rna_PropertyGroup_refine(PointerRNA *ptr)
{
  return ptr->type;
}

static ID *rna_ID_evaluated_get(ID *id, struct Depsgraph *depsgraph)
{
  return DEG_get_evaluated_id(depsgraph, id);
}

static ID *rna_ID_copy(ID *id, Main *bmain)
{
  ID *newid;

  if (BKE_id_copy(bmain, id, &newid)) {
    if (newid != NULL) {
      id_us_min(newid);
    }
    return newid;
  }

  return NULL;
}

static ID *rna_ID_override_create(ID *id, Main *bmain)
{
  if (!BKE_override_static_is_enabled() || id->lib == NULL) {
    return NULL;
  }

  return BKE_override_static_create_from_id(bmain, id);
}

static void rna_ID_update_tag(ID *id, ReportList *reports, int flag)
{
  /* XXX, new function for this! */
#  if 0
  if (ob->type == OB_FONT) {
    Curve *cu = ob->data;
    freedisplist(&cu->disp);
    BKE_vfont_to_curve(bmain, sce, ob, FO_EDIT, NULL);
  }
#  endif

  if (flag == 0) {
    /* pass */
  }
  else {
    /* ensure flag us correct for the type */
    switch (GS(id->name)) {
      case ID_OB:
        /* TODO(sergey): This is kind of difficult to predict since different
         * object types supports different flags. Maybe does not worth checking
         * for this at all. Or maybe let dependency graph to return whether
         * the tag was valid or not. */
        if (flag & ~(ID_RECALC_ALL)) {
          BKE_report(reports, RPT_ERROR, "'Refresh' incompatible with Object ID type");
          return;
        }
        break;
        /* Could add particle updates later */
#  if 0
      case ID_PA:
        if (flag & ~(OB_RECALC_ALL | PSYS_RECALC)) {
          BKE_report(reports, RPT_ERROR, "'Refresh' incompatible with ParticleSettings ID type");
          return;
        }
        break;
#  endif
      default:
        BKE_report(
            reports, RPT_ERROR, "This ID type is not compatible with any 'refresh' options");
        return;
    }
  }

  DEG_id_tag_update(id, flag);
}

static void rna_ID_user_clear(ID *id)
{
  id_fake_user_clear(id);
  id->us = 0; /* don't save */
}

static void rna_ID_user_remap(ID *id, Main *bmain, ID *new_id)
{
  if ((GS(id->name) == GS(new_id->name)) && (id != new_id)) {
    /* For now, do not allow remapping data in linked data from here... */
    BKE_libblock_remap(
        bmain, id, new_id, ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);
  }
}

static struct ID *rna_ID_make_local(struct ID *self, Main *bmain, bool clear_proxy)
{
  /* Special case, as we can't rely on id_make_local(); it clears proxies. */
  if (!clear_proxy && GS(self->name) == ID_OB) {
    BKE_object_make_local_ex(bmain, (Object *)self, false, clear_proxy);
  }
  else {
    id_make_local(bmain, self, false, false);
  }

  ID *ret_id = self->newid ? self->newid : self;
  BKE_id_clear_newpoin(self);
  return ret_id;
}

static AnimData *rna_ID_animation_data_create(ID *id, Main *bmain)
{
  AnimData *adt = BKE_animdata_add_id(id);
  DEG_relations_tag_update(bmain);
  return adt;
}

static void rna_ID_animation_data_free(ID *id, Main *bmain)
{
  BKE_animdata_free(id, true);
  DEG_relations_tag_update(bmain);
}

#  ifdef WITH_PYTHON
void **rna_ID_instance(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;
  return &id->py_instance;
}
#  endif

static void rna_IDPArray_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  IDProperty *prop = (IDProperty *)ptr->data;
  rna_iterator_array_begin(iter, IDP_IDPArray(prop), sizeof(IDProperty), prop->len, 0, NULL);
}

static int rna_IDPArray_length(PointerRNA *ptr)
{
  IDProperty *prop = (IDProperty *)ptr->data;
  return prop->len;
}

int rna_IDMaterials_assign_int(PointerRNA *ptr, int key, const PointerRNA *assign_ptr)
{
  ID *id = ptr->id.data;
  short *totcol = give_totcolp_id(id);
  Material *mat_id = assign_ptr->id.data;
  if (totcol && (key >= 0 && key < *totcol)) {
    BLI_assert(BKE_id_is_in_global_main(id));
    BLI_assert(BKE_id_is_in_global_main(&mat_id->id));
    assign_material_id(G_MAIN, id, mat_id, key + 1);
    return 1;
  }
  else {
    return 0;
  }
}

static void rna_IDMaterials_append_id(ID *id, Main *bmain, Material *ma)
{
  BKE_material_append_id(bmain, id, ma);

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, id);
  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, id);
}

static Material *rna_IDMaterials_pop_id(
    ID *id, Main *bmain, ReportList *reports, int index_i, bool remove_material_slot)
{
  Material *ma;
  short *totcol = give_totcolp_id(id);
  const short totcol_orig = *totcol;
  if (index_i < 0) {
    index_i += (*totcol);
  }

  if ((index_i < 0) || (index_i >= (*totcol))) {
    BKE_report(reports, RPT_ERROR, "Index out of range");
    return NULL;
  }

  ma = BKE_material_pop_id(bmain, id, index_i, remove_material_slot);

  if (*totcol == totcol_orig) {
    BKE_report(reports, RPT_ERROR, "No material to removed");
    return NULL;
  }

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, id);
  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, id);

  return ma;
}

static void rna_IDMaterials_clear_id(ID *id, Main *bmain, bool remove_material_slot)
{
  BKE_material_clear_id(bmain, id, remove_material_slot);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, id);
  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, id);
}

static void rna_Library_filepath_set(PointerRNA *ptr, const char *value)
{
  Library *lib = (Library *)ptr->data;
  BLI_assert(BKE_id_is_in_global_main(&lib->id));
  BKE_library_filepath_set(G_MAIN, lib, value);
}

/* ***** ImagePreview ***** */

static void rna_ImagePreview_is_custom_set(PointerRNA *ptr, int value, enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  if ((value && (prv_img->flag[size] & PRV_USER_EDITED)) ||
      (!value && !(prv_img->flag[size] & PRV_USER_EDITED))) {
    return;
  }

  if (value)
    prv_img->flag[size] |= PRV_USER_EDITED;
  else
    prv_img->flag[size] &= ~PRV_USER_EDITED;

  prv_img->flag[size] |= PRV_CHANGED;

  BKE_previewimg_clear_single(prv_img, size);
}

static void rna_ImagePreview_size_get(PointerRNA *ptr, int *values, enum eIconSizes size)
{
  ID *id = (ID *)ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_ensure(prv_img, size);

  values[0] = prv_img->w[size];
  values[1] = prv_img->h[size];
}

static void rna_ImagePreview_size_set(PointerRNA *ptr, const int *values, enum eIconSizes size)
{
  ID *id = (ID *)ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_clear_single(prv_img, size);

  if (values[0] && values[1]) {
    prv_img->rect[size] = MEM_callocN(values[0] * values[1] * sizeof(unsigned int), "prv_rect");

    prv_img->w[size] = values[0];
    prv_img->h[size] = values[1];
  }

  prv_img->flag[size] |= (PRV_CHANGED | PRV_USER_EDITED);
}

static int rna_ImagePreview_pixels_get_length(PointerRNA *ptr,
                                              int length[RNA_MAX_ARRAY_DIMENSION],
                                              enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_ensure(prv_img, size);

  length[0] = prv_img->w[size] * prv_img->h[size];

  return length[0];
}

static void rna_ImagePreview_pixels_get(PointerRNA *ptr, int *values, enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_ensure(prv_img, size);

  memcpy(values, prv_img->rect[size], prv_img->w[size] * prv_img->h[size] * sizeof(unsigned int));
}

static void rna_ImagePreview_pixels_set(PointerRNA *ptr, const int *values, enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  memcpy(prv_img->rect[size], values, prv_img->w[size] * prv_img->h[size] * sizeof(unsigned int));
  prv_img->flag[size] |= PRV_USER_EDITED;
}

static int rna_ImagePreview_pixels_float_get_length(PointerRNA *ptr,
                                                    int length[RNA_MAX_ARRAY_DIMENSION],
                                                    enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  BLI_assert(sizeof(unsigned int) == 4);

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_ensure(prv_img, size);

  length[0] = prv_img->w[size] * prv_img->h[size] * 4;

  return length[0];
}

static void rna_ImagePreview_pixels_float_get(PointerRNA *ptr, float *values, enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  unsigned char *data = (unsigned char *)prv_img->rect[size];
  const size_t len = prv_img->w[size] * prv_img->h[size] * 4;
  size_t i;

  BLI_assert(sizeof(unsigned int) == 4);

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  BKE_previewimg_ensure(prv_img, size);

  for (i = 0; i < len; i++) {
    values[i] = data[i] * (1.0f / 255.0f);
  }
}

static void rna_ImagePreview_pixels_float_set(PointerRNA *ptr,
                                              const float *values,
                                              enum eIconSizes size)
{
  ID *id = ptr->id.data;
  PreviewImage *prv_img = (PreviewImage *)ptr->data;

  unsigned char *data = (unsigned char *)prv_img->rect[size];
  const size_t len = prv_img->w[size] * prv_img->h[size] * 4;
  size_t i;

  BLI_assert(sizeof(unsigned int) == 4);

  if (id != NULL) {
    BLI_assert(prv_img == BKE_previewimg_id_ensure(id));
  }

  for (i = 0; i < len; i++) {
    data[i] = unit_float_to_uchar_clamp(values[i]);
  }
  prv_img->flag[size] |= PRV_USER_EDITED;
}

static void rna_ImagePreview_is_image_custom_set(PointerRNA *ptr, bool value)
{
  rna_ImagePreview_is_custom_set(ptr, value, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_size_get(PointerRNA *ptr, int *values)
{
  rna_ImagePreview_size_get(ptr, values, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_size_set(PointerRNA *ptr, const int *values)
{
  rna_ImagePreview_size_set(ptr, values, ICON_SIZE_PREVIEW);
}

static int rna_ImagePreview_image_pixels_get_length(PointerRNA *ptr,
                                                    int length[RNA_MAX_ARRAY_DIMENSION])
{
  return rna_ImagePreview_pixels_get_length(ptr, length, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_pixels_get(PointerRNA *ptr, int *values)
{
  rna_ImagePreview_pixels_get(ptr, values, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_pixels_set(PointerRNA *ptr, const int *values)
{
  rna_ImagePreview_pixels_set(ptr, values, ICON_SIZE_PREVIEW);
}

static int rna_ImagePreview_image_pixels_float_get_length(PointerRNA *ptr,
                                                          int length[RNA_MAX_ARRAY_DIMENSION])
{
  return rna_ImagePreview_pixels_float_get_length(ptr, length, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_pixels_float_get(PointerRNA *ptr, float *values)
{
  rna_ImagePreview_pixels_float_get(ptr, values, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_image_pixels_float_set(PointerRNA *ptr, const float *values)
{
  rna_ImagePreview_pixels_float_set(ptr, values, ICON_SIZE_PREVIEW);
}

static void rna_ImagePreview_is_icon_custom_set(PointerRNA *ptr, bool value)
{
  rna_ImagePreview_is_custom_set(ptr, value, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_size_get(PointerRNA *ptr, int *values)
{
  rna_ImagePreview_size_get(ptr, values, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_size_set(PointerRNA *ptr, const int *values)
{
  rna_ImagePreview_size_set(ptr, values, ICON_SIZE_ICON);
}

static int rna_ImagePreview_icon_pixels_get_length(PointerRNA *ptr,
                                                   int length[RNA_MAX_ARRAY_DIMENSION])
{
  return rna_ImagePreview_pixels_get_length(ptr, length, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_pixels_get(PointerRNA *ptr, int *values)
{
  rna_ImagePreview_pixels_get(ptr, values, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_pixels_set(PointerRNA *ptr, const int *values)
{
  rna_ImagePreview_pixels_set(ptr, values, ICON_SIZE_ICON);
}

static int rna_ImagePreview_icon_pixels_float_get_length(PointerRNA *ptr,
                                                         int length[RNA_MAX_ARRAY_DIMENSION])
{
  return rna_ImagePreview_pixels_float_get_length(ptr, length, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_pixels_float_get(PointerRNA *ptr, float *values)
{
  rna_ImagePreview_pixels_float_get(ptr, values, ICON_SIZE_ICON);
}

static void rna_ImagePreview_icon_pixels_float_set(PointerRNA *ptr, const float *values)
{
  rna_ImagePreview_pixels_float_set(ptr, values, ICON_SIZE_ICON);
}

static int rna_ImagePreview_icon_id_get(PointerRNA *ptr)
{
  /* Using a callback here allows us to only generate icon matching
   * that preview when icon_id is requested. */
  return BKE_icon_preview_ensure(ptr->id.data, (PreviewImage *)(ptr->data));
}
static void rna_ImagePreview_icon_reload(PreviewImage *prv)
{
  /* will lazy load on next use, but only in case icon is not user-modified! */
  if (!(prv->flag[ICON_SIZE_ICON] & PRV_USER_EDITED) &&
      !(prv->flag[ICON_SIZE_PREVIEW] & PRV_USER_EDITED)) {
    BKE_previewimg_clear(prv);
  }
}

static PointerRNA rna_IDPreview_get(PointerRNA *ptr)
{
  ID *id = (ID *)ptr->data;
  PreviewImage *prv_img = BKE_previewimg_id_ensure(id);

  return rna_pointer_inherit_refine(ptr, &RNA_ImagePreview, prv_img);
}

static IDProperty *rna_IDPropertyWrapPtr_idprops(PointerRNA *ptr, bool UNUSED(create))
{
  if (ptr == NULL) {
    return NULL;
  }
  return ptr->data;
}

static void rna_Library_version_get(PointerRNA *ptr, int *value)
{
  Library *lib = (Library *)ptr->data;
  value[0] = lib->versionfile / 100;
  value[1] = lib->versionfile % 100;
  value[2] = lib->subversionfile;
}

#else

static void rna_def_ID_properties(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* this is struct is used for holding the virtual
   * PropertyRNA's for ID properties */
  srna = RNA_def_struct(brna, "PropertyGroupItem", NULL);
  RNA_def_struct_sdna(srna, "IDProperty");
  RNA_def_struct_ui_text(
      srna, "ID Property", "Property that stores arbitrary, user defined properties");

  /* IDP_STRING */
  prop = RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);

  /* IDP_INT */
  prop = RNA_def_property(srna, "int", PROP_INT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);

  prop = RNA_def_property(srna, "int_array", PROP_INT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 1);

  /* IDP_FLOAT */
  prop = RNA_def_property(srna, "float", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);

  prop = RNA_def_property(srna, "float_array", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 1);

  /* IDP_DOUBLE */
  prop = RNA_def_property(srna, "double", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);

  prop = RNA_def_property(srna, "double_array", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_array(prop, 1);

  /* IDP_GROUP */
  prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "PropertyGroup");

  prop = RNA_def_property(srna, "collection", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_struct_type(prop, "PropertyGroup");

  prop = RNA_def_property(srna, "idp_array", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "PropertyGroup");
  RNA_def_property_collection_funcs(prop,
                                    "rna_IDPArray_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_IDPArray_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);

  /* never tested, maybe its useful to have this? */
#  if 0
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Name", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);
#  endif

  /* IDP_ID */
  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY | PROP_NEVER_UNLINK);
  RNA_def_property_struct_type(prop, "ID");

  /* ID property groups > level 0, since level 0 group is merged
   * with native RNA properties. the builtin_properties will take
   * care of the properties here */
  srna = RNA_def_struct(brna, "PropertyGroup", NULL);
  RNA_def_struct_sdna(srna, "IDPropertyGroup");
  RNA_def_struct_ui_text(srna, "ID Property Group", "Group of ID properties");
  RNA_def_struct_idprops_func(srna, "rna_PropertyGroup_idprops");
  RNA_def_struct_register_funcs(
      srna, "rna_PropertyGroup_register", "rna_PropertyGroup_unregister", NULL);
  RNA_def_struct_refine_func(srna, "rna_PropertyGroup_refine");

  /* important so python types can have their name used in list views
   * however this isn't prefect because it overrides how python would set the name
   * when we only really want this so RNA_def_struct_name_property() is set to something useful */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  /*RNA_def_property_clear_flag(prop, PROP_EDITABLE); */
  RNA_def_property_ui_text(prop, "Name", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);
}

static void rna_def_ID_materials(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  /* for mesh/mball/curve materials */
  srna = RNA_def_struct(brna, "IDMaterials", NULL);
  RNA_def_struct_sdna(srna, "ID");
  RNA_def_struct_ui_text(srna, "ID Materials", "Collection of materials");

  func = RNA_def_function(srna, "append", "rna_IDMaterials_append_id");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new material to the data-block");
  parm = RNA_def_pointer(func, "material", "Material", "", "Material to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "pop", "rna_IDMaterials_pop_id");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove a material from the data-block");
  parm = RNA_def_int(
      func, "index", -1, -MAXMAT, MAXMAT, "", "Index of material to remove", 0, MAXMAT);
  RNA_def_boolean(
      func, "update_data", 0, "", "Update data by re-adjusting the material slots assigned");
  parm = RNA_def_pointer(func, "material", "Material", "", "Material to remove");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "clear", "rna_IDMaterials_clear_id");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove all materials from the data-block");
  RNA_def_boolean(
      func, "update_data", 0, "", "Update data by re-adjusting the material slots assigned");
}

static void rna_def_image_preview(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImagePreview", NULL);
  RNA_def_struct_sdna(srna, "PreviewImage");
  RNA_def_struct_ui_text(srna, "Image Preview", "Preview image and icon");

  prop = RNA_def_property(srna, "is_image_custom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag[ICON_SIZE_PREVIEW]", PRV_USER_EDITED);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_ImagePreview_is_image_custom_set");
  RNA_def_property_ui_text(prop,
                           "Custom Image",
                           "True if this preview image has been modified by py script,"
                           "and is no more auto-generated by Blender");

  prop = RNA_def_int_vector(
      srna, "image_size", 2, NULL, 0, 0, "Image Size", "Width and height in pixels", 0, 0);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_int_funcs(
      prop, "rna_ImagePreview_image_size_get", "rna_ImagePreview_image_size_set", NULL);

  prop = RNA_def_property(srna, "image_pixels", PROP_INT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 1, NULL);
  RNA_def_property_ui_text(prop, "Image Pixels", "Image pixels, as bytes (always RGBA 32bits)");
  RNA_def_property_dynamic_array_funcs(prop, "rna_ImagePreview_image_pixels_get_length");
  RNA_def_property_int_funcs(
      prop, "rna_ImagePreview_image_pixels_get", "rna_ImagePreview_image_pixels_set", NULL);

  prop = RNA_def_property(srna, "image_pixels_float", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 1, NULL);
  RNA_def_property_ui_text(
      prop, "Float Image Pixels", "Image pixels components, as floats (RGBA concatenated values)");
  RNA_def_property_dynamic_array_funcs(prop, "rna_ImagePreview_image_pixels_float_get_length");
  RNA_def_property_float_funcs(prop,
                               "rna_ImagePreview_image_pixels_float_get",
                               "rna_ImagePreview_image_pixels_float_set",
                               NULL);

  prop = RNA_def_property(srna, "is_icon_custom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag[ICON_SIZE_ICON]", PRV_USER_EDITED);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_ImagePreview_is_icon_custom_set");
  RNA_def_property_ui_text(prop,
                           "Custom Icon",
                           "True if this preview icon has been modified by py script,"
                           "and is no more auto-generated by Blender");

  prop = RNA_def_int_vector(
      srna, "icon_size", 2, NULL, 0, 0, "Icon Size", "Width and height in pixels", 0, 0);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_int_funcs(
      prop, "rna_ImagePreview_icon_size_get", "rna_ImagePreview_icon_size_set", NULL);

  prop = RNA_def_property(srna, "icon_pixels", PROP_INT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 1, NULL);
  RNA_def_property_ui_text(prop, "Icon Pixels", "Icon pixels, as bytes (always RGBA 32bits)");
  RNA_def_property_dynamic_array_funcs(prop, "rna_ImagePreview_icon_pixels_get_length");
  RNA_def_property_int_funcs(
      prop, "rna_ImagePreview_icon_pixels_get", "rna_ImagePreview_icon_pixels_set", NULL);

  prop = RNA_def_property(srna, "icon_pixels_float", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_multi_array(prop, 1, NULL);
  RNA_def_property_ui_text(
      prop, "Float Icon Pixels", "Icon pixels components, as floats (RGBA concatenated values)");
  RNA_def_property_dynamic_array_funcs(prop, "rna_ImagePreview_icon_pixels_float_get_length");
  RNA_def_property_float_funcs(prop,
                               "rna_ImagePreview_icon_pixels_float_get",
                               "rna_ImagePreview_icon_pixels_float_set",
                               NULL);

  prop = RNA_def_int(srna,
                     "icon_id",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "Icon ID",
                     "Unique integer identifying this preview as an icon (zero means invalid)",
                     INT_MIN,
                     INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_ImagePreview_icon_id_get", NULL, NULL);

  func = RNA_def_function(srna, "reload", "rna_ImagePreview_icon_reload");
  RNA_def_function_ui_description(func, "Reload the preview from its source path");
}

static void rna_def_ID_override_static_property_operation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem static_override_property_operation_items[] = {
      {IDOVERRIDESTATIC_OP_NOOP,
       "NOOP",
       0,
       "No-Op",
       "Does nothing, prevents adding actual overrides (NOT USED)"},
      {IDOVERRIDESTATIC_OP_REPLACE,
       "REPLACE",
       0,
       "Replace",
       "Replace value of reference by overriding one"},
      {IDOVERRIDESTATIC_OP_ADD,
       "DIFF_ADD",
       0,
       "Differential",
       "Stores and apply difference between reference and local value (NOT USED)"},
      {IDOVERRIDESTATIC_OP_SUBTRACT,
       "DIFF_SUB",
       0,
       "Differential",
       "Stores and apply difference between reference and local value (NOT USED)"},
      {IDOVERRIDESTATIC_OP_MULTIPLY,
       "FACT_MULTIPLY",
       0,
       "Factor",
       "Stores and apply multiplication factor between reference and local value (NOT USED)"},
      {IDOVERRIDESTATIC_OP_INSERT_AFTER,
       "INSERT_AFTER",
       0,
       "Insert After",
       "Insert a new item into collection after the one referenced in subitem_reference_name or "
       "_index"},
      {IDOVERRIDESTATIC_OP_INSERT_BEFORE,
       "INSERT_BEFORE",
       0,
       "Insert Before",
       "Insert a new item into collection after the one referenced in subitem_reference_name or "
       "_index (NOT USED)"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem static_override_property_flag_items[] = {
      {IDOVERRIDESTATIC_FLAG_MANDATORY,
       "MANDATORY",
       0,
       "Mandatory",
       "For templates, prevents the user from removing pre-defined operation (NOT USED)"},
      {IDOVERRIDESTATIC_FLAG_LOCKED,
       "LOCKED",
       0,
       "Locked",
       "Prevents the user from modifying that override operation (NOT USED)"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "IDOverrideStaticPropertyOperation", NULL);
  RNA_def_struct_ui_text(srna,
                         "ID Static Override Property Operation",
                         "Description of an override operation over an overridden property");

  prop = RNA_def_enum(srna,
                      "operation",
                      static_override_property_operation_items,
                      IDOVERRIDESTATIC_OP_REPLACE,
                      "Operation",
                      "What override operation is performed");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */

  prop = RNA_def_enum(
      srna, "flag", static_override_property_flag_items, 0, "Flags", "Optional flags (NOT USED)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */

  prop = RNA_def_string(srna,
                        "subitem_reference_name",
                        NULL,
                        INT_MAX,
                        "Subitem Reference Name",
                        "Used to handle insertions into collection");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */
  RNA_def_property_string_funcs(prop,
                                "rna_ID_override_static_property_operation_refname_get",
                                "rna_ID_override_static_property_operation_refname_length",
                                NULL);

  prop = RNA_def_string(srna,
                        "subitem_local_name",
                        NULL,
                        INT_MAX,
                        "Subitem Local Name",
                        "Used to handle insertions into collection");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */
  RNA_def_property_string_funcs(prop,
                                "rna_ID_override_static_property_operation_locname_get",
                                "rna_ID_override_static_property_operation_locname_length",
                                NULL);

  prop = RNA_def_int(srna,
                     "subitem_reference_index",
                     -1,
                     -1,
                     INT_MAX,
                     "Subitem Reference Index",
                     "Used to handle insertions into collection",
                     -1,
                     INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */

  prop = RNA_def_int(srna,
                     "subitem_local_index",
                     -1,
                     -1,
                     INT_MAX,
                     "Subitem Local Index",
                     "Used to handle insertions into collection",
                     -1,
                     INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */
}

static void rna_def_ID_override_static_property(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "IDOverrideStaticProperty", NULL);
  RNA_def_struct_ui_text(
      srna, "ID Static Override Property", "Description of an overridden property");

  /* String pointer, we *should* add get/set/etc.
   * But NULL rna_path would be a nasty bug anyway. */
  prop = RNA_def_string(srna,
                        "rna_path",
                        NULL,
                        INT_MAX,
                        "RNA Path",
                        "RNA path leading to that property, from owning ID");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* For now. */

  RNA_def_collection(srna,
                     "operations",
                     "IDOverrideStaticPropertyOperation",
                     "Operations",
                     "List of overriding operations for a property");

  rna_def_ID_override_static_property_operation(brna);
}

static void rna_def_ID_override_static(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "IDOverrideStatic", NULL);
  RNA_def_struct_ui_text(
      srna, "ID Static Override", "Struct gathering all data needed by statically overridden IDs");

  RNA_def_pointer(
      srna, "reference", "ID", "Reference ID", "Linked ID used as reference by this override");

  prop = RNA_def_boolean(
      srna,
      "auto_generate",
      true,
      "Auto Generate Override",
      "Automatically generate overriding operations by detecting changes in properties");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", STATICOVERRIDE_AUTO);

  RNA_def_collection(srna,
                     "properties",
                     "IDOverrideStaticProperty",
                     "Properties",
                     "List of overridden properties");

  rna_def_ID_override_static_property(brna);
}

static void rna_def_ID(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop, *parm;

  static const EnumPropertyItem update_flag_items[] = {
      {ID_RECALC_TRANSFORM, "OBJECT", 0, "Object", ""},
      {ID_RECALC_GEOMETRY, "DATA", 0, "Data", ""},
      {ID_RECALC_ANIMATION, "TIME", 0, "Time", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "ID", NULL);
  RNA_def_struct_ui_text(
      srna,
      "ID",
      "Base type for data-blocks, defining a unique name, linking from other libraries "
      "and garbage collection");
  RNA_def_struct_flag(srna, STRUCT_ID | STRUCT_ID_REFCOUNT);
  RNA_def_struct_refine_func(srna, "rna_ID_refine");
  RNA_def_struct_idprops_func(srna, "rna_ID_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Unique data-block ID name");
  RNA_def_property_string_funcs(prop, "rna_ID_name_get", "rna_ID_name_length", "rna_ID_name_set");
  RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
  RNA_def_property_editable_func(prop, "rna_ID_name_editable");
  RNA_def_property_update(prop, NC_ID | NA_RENAME, NULL);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "name_full", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Full Name", "Unique data-block ID name, including library one is any");
  RNA_def_property_string_funcs(prop, "rna_ID_name_full_get", "rna_ID_name_full_length", NULL);
  RNA_def_property_string_maxlength(prop, MAX_ID_FULL_NAME);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_evaluated", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop,
      "Is Evaluated",
      "Whether this ID is runtime-only, evaluated data-block, or actual data from .blend file");
  RNA_def_property_boolean_funcs(prop, "rna_ID_is_evaluated_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "original", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_ui_text(
      prop,
      "Original ID",
      "Actual data-block from .blend file (Main database) that generated that evaluated one");
  RNA_def_property_pointer_funcs(prop, "rna_ID_original_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_PTR_NO_OWNERSHIP);

  prop = RNA_def_property(srna, "users", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "us");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Users", "Number of times this data-block is referenced");

  prop = RNA_def_property(srna, "use_fake_user", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LIB_FAKEUSER);
  RNA_def_property_ui_text(prop, "Fake User", "Save this data-block even if it has no users");
  RNA_def_property_ui_icon(prop, ICON_FAKE_USER_OFF, true);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_ID_fake_user_set");

  prop = RNA_def_property(srna, "tag", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "tag", LIB_TAG_DOIT);
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_ui_text(prop,
                           "Tag",
                           "Tools can use this to tag data for their own purposes "
                           "(initial state is undefined)");

  prop = RNA_def_property(srna, "is_library_indirect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "tag", LIB_TAG_INDIRECT);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Indirect", "Is this ID block linked indirectly");

  prop = RNA_def_property(srna, "library", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "lib");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Library", "Library file the data-block is linked from");

  prop = RNA_def_pointer(
      srna, "override_static", "IDOverrideStatic", "Static Override", "Static override data");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_pointer(
      srna,
      "preview",
      "ImagePreview",
      "Preview",
      "Preview image and icon of this data-block (None if not supported for this type of data)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, "rna_IDPreview_get", NULL, NULL, NULL);

  /* functions */
  func = RNA_def_function(srna, "evaluated_get", "rna_ID_evaluated_get");
  RNA_def_function_ui_description(
      func, "Get corresponding evaluated ID from the given dependency graph");
  parm = RNA_def_pointer(
      func, "depsgraph", "Depsgraph", "", "Dependency graph to perform lookup in");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "id", "ID", "", "New copy of the ID");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "copy", "rna_ID_copy");
  RNA_def_function_ui_description(
      func, "Create a copy of this data-block (not supported for all data-blocks)");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "id", "ID", "", "New copy of the ID");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "override_create", "rna_ID_override_create");
  RNA_def_function_ui_description(func,
                                  "Create an overridden local copy of this linked data-block (not "
                                  "supported for all data-blocks)");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "id", "ID", "", "New overridden local copy of the ID");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "user_clear", "rna_ID_user_clear");
  RNA_def_function_ui_description(func,
                                  "Clear the user count of a data-block so its not saved, "
                                  "on reload the data will be removed");

  func = RNA_def_function(srna, "user_remap", "rna_ID_user_remap");
  RNA_def_function_ui_description(
      func, "Replace all usage in the .blend file of this ID by new given one");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "new_id", "ID", "", "New ID to use");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "make_local", "rna_ID_make_local");
  RNA_def_function_ui_description(
      func,
      "Make this datablock local, return local one "
      "(may be a copy of the original, in case it is also indirectly used)");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_boolean(
      func,
      "clear_proxy",
      true,
      "",
      "Whether to clear proxies (the default behavior, "
      "note that if object has to be duplicated to be made local, proxies are always cleared)");
  parm = RNA_def_pointer(func, "id", "ID", "", "This ID, or the new ID if it was copied");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "user_of_id", "BKE_library_ID_use_ID");
  RNA_def_function_ui_description(func,
                                  "Count the number of times that ID uses/references given one");
  parm = RNA_def_pointer(func, "id", "ID", "", "ID to count usages");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "count",
                     0,
                     0,
                     INT_MAX,
                     "",
                     "Number of usages/references of given id by current data-block",
                     0,
                     INT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "animation_data_create", "rna_ID_animation_data_create");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(
      func, "Create animation data to this ID, note that not all ID types support this");
  parm = RNA_def_pointer(func, "anim_data", "AnimData", "", "New animation data or NULL");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "animation_data_clear", "rna_ID_animation_data_free");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Clear animation on this this ID");

  func = RNA_def_function(srna, "update_tag", "rna_ID_update_tag");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func,
                                  "Tag the ID to update its display data, "
                                  "e.g. when calling :class:`bpy.types.Scene.update`");
  RNA_def_enum_flag(func, "refresh", update_flag_items, 0, "", "Type of updates to perform");

#  ifdef WITH_PYTHON
  RNA_def_struct_register_funcs(srna, NULL, NULL, "rna_ID_instance");
#  endif
}

static void rna_def_library(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Library", "ID");
  RNA_def_struct_ui_text(srna, "Library", "External .blend file from which data is linked");
  RNA_def_struct_ui_icon(srna, ICON_LIBRARY_DATA_DIRECT);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(prop, "File Path", "Path to the library .blend file");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Library_filepath_set");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Library");
  RNA_def_property_ui_text(prop, "Parent", "");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");

  prop = RNA_def_int_vector(srna,
                            "version",
                            3,
                            NULL,
                            0,
                            INT_MAX,
                            "Version",
                            "Version of Blender the library .blend was saved with",
                            0,
                            INT_MAX);
  RNA_def_property_int_funcs(prop, "rna_Library_version_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_THICK_WRAP);

  func = RNA_def_function(srna, "reload", "WM_lib_reload");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Reload this library and all its linked data-blocks");
}

/**
 * \attention This is separate from the above. It allows for RNA functions to
 * return an IDProperty *. See MovieClip.metadata for a usage example.
 */
static void rna_def_idproperty_wrap_ptr(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "IDPropertyWrapPtr", NULL);
  RNA_def_struct_idprops_func(srna, "rna_IDPropertyWrapPtr_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);
}

void RNA_def_ID(BlenderRNA *brna)
{
  StructRNA *srna;

  /* built-in unknown type */
  srna = RNA_def_struct(brna, "UnknownType", NULL);
  RNA_def_struct_ui_text(
      srna, "Unknown Type", "Stub RNA type used for pointers to unknown or internal data");

  /* built-in any type */
  srna = RNA_def_struct(brna, "AnyType", NULL);
  RNA_def_struct_ui_text(srna, "Any Type", "RNA type used for pointers to any possible data");

  rna_def_ID(brna);
  rna_def_ID_override_static(brna);
  rna_def_image_preview(brna);
  rna_def_ID_properties(brna);
  rna_def_ID_materials(brna);
  rna_def_library(brna);
  rna_def_idproperty_wrap_ptr(brna);
}

#endif
