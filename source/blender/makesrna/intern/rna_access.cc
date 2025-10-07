/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_mutex.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_report.hh"

#include "CLG_log.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_types.hh"

#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_message.hh"

/* flush updates */
#include "WM_types.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "rna_access_internal.hh"
#include "rna_internal.hh"

static CLG_LogRef LOG = {"rna.access"};

/**
 * The boolean IDProperty type isn't supported in old versions. In order to keep forward
 * compatibility for a period of time (until 4.0), save boolean RNA properties as integer
 * IDProperties.
 */
#define USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP

/* Init/Exit */

/* NOTE: Initializing this object here is fine for now, as it should not allocate any memory. */
extern const PointerRNA PointerRNA_NULL = {};

void RNA_init()
{
  StructRNA *srna;

  BLENDER_RNA.structs_map = BLI_ghash_str_new_ex(__func__, 2048);
  BLENDER_RNA.structs_len = 0;

  for (srna = static_cast<StructRNA *>(BLENDER_RNA.structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    if (!srna->cont.prop_lookup_set) {
      srna->cont.prop_lookup_set =
          MEM_new<blender::CustomIDVectorSet<PropertyRNA *, PropertyRNAIdentifierGetter>>(
              __func__);

      LISTBASE_FOREACH (PropertyRNA *, prop, &srna->cont.properties) {
        if (!(prop->flag_internal & PROP_INTERN_BUILTIN)) {
          srna->cont.prop_lookup_set->add(prop);
        }
      }
    }
    BLI_assert(srna->flag & STRUCT_PUBLIC_NAMESPACE);
    BLI_ghash_insert(BLENDER_RNA.structs_map, (void *)srna->identifier, srna);
    BLENDER_RNA.structs_len += 1;
  }
}

void RNA_bpy_exit()
{
#ifdef WITH_PYTHON
  StructRNA *srna;

  for (srna = static_cast<StructRNA *>(BLENDER_RNA.structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    /* NOTE(@ideasman42): each call locks the Python's GIL. Only locking/unlocking once
     * is possible but gives barely measurable speedup (< ~1millisecond) so leave as-is. */
    BPY_free_srna_pytype(srna);
  }
#endif
}

void RNA_exit()
{
  StructRNA *srna;

  for (srna = static_cast<StructRNA *>(BLENDER_RNA.structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    MEM_SAFE_DELETE(srna->cont.prop_lookup_set);
  }

  RNA_free(&BLENDER_RNA);
}

/* Pointer */

BLI_INLINE void rna_pointer_refine(PointerRNA &r_ptr)
{
  while (r_ptr.type->refine) {
    StructRNA *type = r_ptr.type->refine(&r_ptr);
    if (type == r_ptr.type) {
      break;
    }
    r_ptr.type = type;
  }
}

void rna_pointer_create_with_ancestors(const PointerRNA &parent,
                                       StructRNA *type,
                                       void *data,
                                       PointerRNA &r_ptr)
{
  if (data) {
    if (type && type->flag & STRUCT_ID) {
      /* Currently, assume that an ID PointerRNA never has an ancestor.
       * NOTE: This may become an issue for embedded IDs in the future, see also
       * #PointerRNA::ancestors docs. */
      r_ptr = {static_cast<ID *>(data), type, data};
    }
    else {
      r_ptr = {parent.owner_id, type, data, parent};
    }
    rna_pointer_refine(r_ptr);
  }
  else {
    r_ptr = {};
  }
}

PointerRNA RNA_main_pointer_create(Main *main)
{
  return {nullptr, &RNA_BlendData, main};
}

PointerRNA RNA_id_pointer_create(ID *id)
{
  if (id) {
    PointerRNA ptr{id, ID_code_to_RNA_type(GS(id->name)), id};
    rna_pointer_refine(ptr);
    return ptr;
  }

  return PointerRNA_NULL;
}

PointerRNA RNA_pointer_create_discrete(ID *id, StructRNA *type, void *data)
{
  PointerRNA ptr{id, type, data};

  if (data) {
    rna_pointer_refine(ptr);
  }

  return ptr;
}

PointerRNA RNA_pointer_create_with_parent(const PointerRNA &parent, StructRNA *type, void *data)
{
  PointerRNA result;
  rna_pointer_create_with_ancestors(parent, type, data, result);
  return result;
}

PointerRNA RNA_pointer_create_id_subdata(ID &id, StructRNA *type, void *data)
{
  PointerRNA parent = RNA_id_pointer_create(&id);
  PointerRNA result;
  rna_pointer_create_with_ancestors(parent, type, data, result);
  return result;
}

PointerRNA RNA_pointer_create_from_ancestor(const PointerRNA &ptr, const int ancestor_idx)
{
  if (ancestor_idx >= ptr.ancestors.size()) {
    BLI_assert_unreachable();
    return {};
  }

  /* NOTE: No call to `rna_pointer_refine` should be needed here, as ancestors info should have
   * been created from already refined PointerRNA data. */
  PointerRNA ancestor_ptr{ptr.owner_id,
                          ptr.ancestors[ancestor_idx].type,
                          ptr.ancestors[ancestor_idx].data,
                          ptr.ancestors.as_span().slice(0, ancestor_idx)};
#ifndef NDEBUG
  StructRNA *type = ancestor_ptr.type;
  rna_pointer_refine(ancestor_ptr);
  BLI_assert(type == ancestor_ptr.type);
#endif

  return ancestor_ptr;
}

bool RNA_pointer_is_null(const PointerRNA *ptr)
{
  return (ptr->data == nullptr) || (ptr->type == nullptr);
}

PointerRNA RNA_blender_rna_pointer_create()
{
  PointerRNA ptr = {};
  ptr.owner_id = nullptr;
  ptr.type = &RNA_BlenderRNA;
  ptr.data = &BLENDER_RNA;
  return ptr;
}

PointerRNA RNA_pointer_recast(PointerRNA *ptr)
{
#if 0 /* works but this case if covered by more general code below. */
  if (RNA_struct_is_ID(ptr->type)) {
    /* simple case */
    return RNA_id_pointer_create(ptr->owner_id);
  }
  else
#endif
  {
    PointerRNA ptr_result{*ptr};
    PointerRNA t_ptr{*ptr};
    StructRNA *base;

    for (base = ptr->type->base; base; base = base->base) {
      t_ptr.type = base;
      rna_pointer_refine(t_ptr);
      if (t_ptr.type && t_ptr.type != ptr->type) {
        ptr_result = t_ptr;
      }
    }
    return ptr_result;
  }
}

/* ID Properties */

void rna_idproperty_touch(IDProperty *idprop)
{
  /* so the property is seen as 'set' by rna */
  idprop->flag &= ~IDP_FLAG_GHOST;
}

IDProperty **RNA_struct_idprops_p(PointerRNA *ptr)
{
  StructRNA *type = ptr->type;
  if (type == nullptr) {
    return nullptr;
  }
  if (type->idproperties == nullptr) {
    return nullptr;
  }

  return type->idproperties(ptr);
}

IDProperty *RNA_struct_idprops(PointerRNA *ptr, bool create)
{
  IDProperty **property_ptr = RNA_struct_idprops_p(ptr);
  if (property_ptr == nullptr) {
    return nullptr;
  }

  if (create && *property_ptr == nullptr) {
    *property_ptr =
        blender::bke::idprop::create_group("user_properties", IDP_FLAG_STATIC_TYPE).release();
  }

  return *property_ptr;
}

bool RNA_struct_idprops_check(const StructRNA *srna)
{
  return (srna && srna->idproperties);
}

IDProperty *rna_idproperty_find(PointerRNA *ptr, const char *name)
{
  IDProperty *group = RNA_struct_idprops(ptr, false);

  if (!group) {
    return nullptr;
  }
  if (group->type == IDP_GROUP) {
    return IDP_GetPropertyFromGroup(group, name);
  }

  /* Not sure why that happens sometimes, with nested properties... */
  /* Seems to be actually array prop, name is usually "0"... To be sorted out later. */
#if 0
  printf("Got unexpected IDProp container when trying to retrieve %s: %d\n", name, group->type);
#endif

  return nullptr;
}

IDProperty **RNA_struct_system_idprops_p(PointerRNA *ptr)
{
  StructRNA *type = ptr->type;
  if (type == nullptr) {
    return nullptr;
  }
  if (type->system_idproperties == nullptr) {
    return nullptr;
  }

  return type->system_idproperties(ptr);
}

IDProperty *RNA_struct_system_idprops(PointerRNA *ptr, bool create)
{
  IDProperty **property_ptr = RNA_struct_system_idprops_p(ptr);
  if (property_ptr == nullptr) {
    return nullptr;
  }

  if (create && *property_ptr == nullptr) {
    *property_ptr =
        blender::bke::idprop::create_group("system_properties", IDP_FLAG_STATIC_TYPE).release();
  }

  return *property_ptr;
}

bool RNA_struct_system_idprops_check(StructRNA *srna)
{
  return (srna && srna->system_idproperties);
}

IDProperty *rna_system_idproperty_find(PointerRNA *ptr, const char *name)
{
  IDProperty *group = RNA_struct_system_idprops(ptr, false);

  if (!group) {
    return nullptr;
  }

  if (group->type == IDP_GROUP) {
    return IDP_GetPropertyFromGroup(group, name);
  }

  /* Not sure why that happens sometimes, with nested properties... */
  /* Seems to be actually array prop, name is usually "0"... To be sorted out later. */
#if 0
  printf("Got unexpected IDProp container when trying to retrieve %s: %d\n", name, group->type);
#endif

  return nullptr;
}

static void rna_system_idproperty_free(PointerRNA *ptr, const char *name)
{
  IDProperty *group = RNA_struct_system_idprops(ptr, false);

  if (group) {
    IDProperty *idprop = IDP_GetPropertyFromGroup(group, name);
    if (idprop) {
      IDP_FreeFromGroup(group, idprop);
    }
  }
}

static int rna_ensure_property_array_length(PointerRNA *ptr, PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    int arraylen[RNA_MAX_ARRAY_DIMENSION];
    return (prop->getlength && ptr->data) ? prop->getlength(ptr, arraylen) :
                                            int(prop->totarraylength);
  }
  IDProperty *idprop = (IDProperty *)prop;

  if (idprop->type == IDP_ARRAY) {
    return idprop->len;
  }
  return 0;
}

static bool rna_ensure_property_array_check(PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    return (prop->getlength || prop->totarraylength);
  }
  IDProperty *idprop = (IDProperty *)prop;

  return (idprop->type == IDP_ARRAY);
}

static void rna_ensure_property_multi_array_length(const PointerRNA *ptr,
                                                   PropertyRNA *prop,
                                                   int length[])
{
  if (prop->magic == RNA_MAGIC) {
    if (prop->getlength) {
      prop->getlength(ptr, length);
    }
    else {
      memcpy(length, prop->arraylength, prop->arraydimension * sizeof(int));
    }
  }
  else {
    IDProperty *idprop = (IDProperty *)prop;

    if (idprop->type == IDP_ARRAY) {
      length[0] = idprop->len;
    }
    else {
      length[0] = 0;
    }
  }
}

static bool rna_idproperty_verify_valid(PointerRNA *ptr, PropertyRNA *prop, IDProperty *idprop)
{
  /* this verifies if the idproperty actually matches the property
   * description and otherwise removes it. this is to ensure that
   * rna property access is type safe, e.g. if you defined the rna
   * to have a certain array length you can count on that staying so */

  switch (idprop->type) {
    case IDP_IDPARRAY:
      if (prop->type != PROP_COLLECTION) {
        return false;
      }
      break;
    case IDP_ARRAY:
      if (rna_ensure_property_array_length(ptr, prop) != idprop->len) {
        return false;
      }

      if (idprop->subtype == IDP_FLOAT && prop->type != PROP_FLOAT) {
        return false;
      }
      if (idprop->subtype == IDP_BOOLEAN && prop->type != PROP_BOOLEAN) {
        return false;
      }
      if (idprop->subtype == IDP_INT && !ELEM(prop->type, PROP_BOOLEAN, PROP_INT, PROP_ENUM)) {
        return false;
      }

      break;
    case IDP_INT:
      if (!ELEM(prop->type, PROP_BOOLEAN, PROP_INT, PROP_ENUM)) {
        return false;
      }
      break;
    case IDP_BOOLEAN:
      if (prop->type != PROP_BOOLEAN) {
        return false;
      }
      break;
    case IDP_FLOAT:
    case IDP_DOUBLE:
      if (prop->type != PROP_FLOAT) {
        return false;
      }
      break;
    case IDP_STRING:
      if (prop->type != PROP_STRING) {
        return false;
      }
      break;
    case IDP_GROUP:
    case IDP_ID:
      if (prop->type != PROP_POINTER) {
        return false;
      }
      break;
    default:
      return false;
  }

  return true;
}

static PropertyRNA *typemap[IDP_NUMTYPES] = {
    &rna_PropertyGroupItem_string,
    &rna_PropertyGroupItem_int,
    &rna_PropertyGroupItem_float,
    nullptr,
    nullptr,
    nullptr,
    &rna_PropertyGroupItem_group,
    &rna_PropertyGroupItem_id,
    &rna_PropertyGroupItem_double,
    &rna_PropertyGroupItem_idp_array,
    &rna_PropertyGroupItem_bool,
};

static PropertyRNA *arraytypemap[IDP_NUMTYPES] = {
    nullptr,
    &rna_PropertyGroupItem_int_array,
    &rna_PropertyGroupItem_float_array,
    nullptr,
    nullptr,
    nullptr,
    &rna_PropertyGroupItem_collection,
    nullptr,
    &rna_PropertyGroupItem_double_array,
    nullptr,
    &rna_PropertyGroupItem_bool_array,
};

void rna_property_rna_or_id_get(PropertyRNA *prop,
                                PointerRNA *ptr,
                                PropertyRNAOrID *r_prop_rna_or_id)
{
  /* This is quite a hack, but avoids some complexity in the API. we
   * pass IDProperty structs as PropertyRNA pointers to the outside.
   * We store some bytes in PropertyRNA structs that allows us to
   * distinguish it from IDProperty structs. If it is an ID property,
   * we look up an IDP PropertyRNA based on the type, and set the data
   * pointer to the IDProperty. */
  *r_prop_rna_or_id = {};

  r_prop_rna_or_id->ptr = ptr;
  r_prop_rna_or_id->rawprop = prop;

  if (prop->magic == RNA_MAGIC) {
    r_prop_rna_or_id->rnaprop = prop;
    r_prop_rna_or_id->identifier = prop->identifier;

    r_prop_rna_or_id->is_array = prop->getlength || prop->totarraylength;
    if (r_prop_rna_or_id->is_array) {
      int arraylen[RNA_MAX_ARRAY_DIMENSION];
      r_prop_rna_or_id->array_len = (prop->getlength && ptr->data) ?
                                        uint(prop->getlength(ptr, arraylen)) :
                                        prop->totarraylength;
    }

    if (prop->flag & PROP_IDPROPERTY) {
      IDProperty *idprop = rna_system_idproperty_find(ptr, prop->identifier);

      if (idprop != nullptr && !rna_idproperty_verify_valid(ptr, prop, idprop)) {
        IDProperty *group = RNA_struct_system_idprops(ptr, false);

        IDP_FreeFromGroup(group, idprop);
        idprop = nullptr;
      }

      r_prop_rna_or_id->idprop = idprop;
      r_prop_rna_or_id->is_rna_storage_idprop = true;
      r_prop_rna_or_id->is_set = idprop != nullptr && (idprop->flag & IDP_FLAG_GHOST) == 0;
    }
    else {
      /* Full static RNA properties are always set. */
      r_prop_rna_or_id->is_set = true;
    }
  }
  else {
    IDProperty *idprop = (IDProperty *)prop;
    /* Given prop may come from the custom properties of another data, ensure we get the one from
     * given data ptr. */
    IDProperty *idprop_evaluated = rna_idproperty_find(ptr, idprop->name);
    if (idprop_evaluated != nullptr && idprop->type != idprop_evaluated->type) {
      idprop_evaluated = nullptr;
    }

    r_prop_rna_or_id->idprop = idprop_evaluated;
    r_prop_rna_or_id->is_idprop = true;
    /* Full IDProperties are always set, if it exists. */
    r_prop_rna_or_id->is_set = (idprop_evaluated != nullptr);

    r_prop_rna_or_id->identifier = idprop->name;
    if (idprop->type == IDP_ARRAY) {
      r_prop_rna_or_id->rnaprop = arraytypemap[int(idprop->subtype)];
      r_prop_rna_or_id->is_array = true;
      r_prop_rna_or_id->array_len = idprop_evaluated != nullptr ? uint(idprop_evaluated->len) : 0;
    }
    else {
      /* Special case for int properties with enum items, these are displayed as a PROP_ENUM. */
      if (idprop->type == IDP_INT) {
        const IDPropertyUIDataInt *ui_data_int = reinterpret_cast<IDPropertyUIDataInt *>(
            idprop->ui_data);
        if (ui_data_int && ui_data_int->enum_items_num > 0) {
          r_prop_rna_or_id->rnaprop = &rna_PropertyGroupItem_enum;
          return;
        }
      }
      r_prop_rna_or_id->rnaprop = typemap[int(idprop->type)];
    }
  }
}

IDProperty *rna_idproperty_check(PropertyRNA **prop, PointerRNA *ptr)
{
  PropertyRNAOrID prop_rna_or_id;

  rna_property_rna_or_id_get(*prop, ptr, &prop_rna_or_id);

  *prop = prop_rna_or_id.rnaprop;
  return prop_rna_or_id.idprop;
}

PropertyRNA *rna_ensure_property(PropertyRNA *prop)
{
  /* the quick version if we don't need the idproperty */

  if (prop->magic == RNA_MAGIC) {
    return prop;
  }

  {
    IDProperty *idprop = (IDProperty *)prop;

    if (idprop->type == IDP_ARRAY) {
      return arraytypemap[int(idprop->subtype)];
    }
    /* Special case for int properties with enum items, these are displayed as a PROP_ENUM. */
    if (idprop->type == IDP_INT) {
      const IDPropertyUIDataInt *ui_data_int = reinterpret_cast<IDPropertyUIDataInt *>(
          idprop->ui_data);
      if (ui_data_int && ui_data_int->enum_items_num > 0) {
        return &rna_PropertyGroupItem_enum;
      }
    }
    return typemap[int(idprop->type)];
  }
}

static const char *rna_ensure_property_identifier(const PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    return prop->identifier;
  }
  return ((const IDProperty *)prop)->name;
}

static const char *rna_ensure_property_description(const PropertyRNA *prop)
{
  if (prop->magic == RNA_MAGIC) {
    return prop->description;
  }

  const IDProperty *idprop = (const IDProperty *)prop;
  if (idprop->ui_data) {
    const IDPropertyUIData *ui_data = idprop->ui_data;
    return ui_data->description;
  }

  return "";
}

static const char *rna_ensure_property_name(const PropertyRNA *prop)
{
  const char *name;

  if (prop->magic == RNA_MAGIC) {
    name = prop->name;
  }
  else {
    name = ((const IDProperty *)prop)->name;
  }

  return name;
}

/* Structs */

StructRNA *RNA_struct_find(const char *identifier)
{
  return static_cast<StructRNA *>(BLI_ghash_lookup(BLENDER_RNA.structs_map, identifier));
}

const char *RNA_struct_identifier(const StructRNA *type)
{
  return type->identifier;
}

const char *RNA_struct_ui_name(const StructRNA *type)
{
  return CTX_IFACE_(type->translation_context, type->name);
}

const char *RNA_struct_ui_name_raw(const StructRNA *type)
{
  return type->name;
}

int RNA_struct_ui_icon(const StructRNA *type)
{
  if (type) {
    return type->icon;
  }
  return ICON_DOT;
}

const char *RNA_struct_ui_description(const StructRNA *type)
{
  return TIP_(type->description);
}

const char *RNA_struct_ui_description_raw(const StructRNA *type)
{
  return type->description;
}

const char *RNA_struct_translation_context(const StructRNA *type)
{
  return type->translation_context;
}

PropertyRNA *RNA_struct_name_property(const StructRNA *type)
{
  return type->nameproperty;
}

const EnumPropertyItem *RNA_struct_property_tag_defines(const StructRNA *type)
{
  return type->prop_tag_defines;
}

PropertyRNA *RNA_struct_iterator_property(StructRNA *type)
{
  return type->iteratorproperty;
}

StructRNA *RNA_struct_base(StructRNA *type)
{
  return type->base;
}

const StructRNA *RNA_struct_base_child_of(const StructRNA *type, const StructRNA *parent_type)
{
  while (type) {
    if (type->base == parent_type) {
      return type;
    }
    type = type->base;
  }
  return nullptr;
}

bool RNA_struct_is_ID(const StructRNA *type)
{
  return (type->flag & STRUCT_ID) != 0;
}

bool RNA_struct_undo_check(const StructRNA *type)
{
  return (type->flag & STRUCT_UNDO) != 0;
}

bool RNA_struct_idprops_datablock_allowed(const StructRNA *type)
{
  return (type->flag & (STRUCT_NO_DATABLOCK_IDPROPERTIES | STRUCT_NO_IDPROPERTIES)) == 0;
}

bool RNA_struct_idprops_contains_datablock(const StructRNA *type)
{
  return (type->flag & (STRUCT_CONTAINS_DATABLOCK_IDPROPERTIES | STRUCT_ID)) != 0;
}

bool RNA_struct_system_idprops_register_check(const StructRNA *type)
{
  return (type->flag & STRUCT_NO_IDPROPERTIES) == 0 && type->system_idproperties != nullptr;
}

bool RNA_struct_system_idprops_unset(PointerRNA *ptr, const char *identifier)
{
  IDProperty *group = RNA_struct_system_idprops(ptr, false);

  if (group) {
    IDProperty *idp = IDP_GetPropertyFromGroup(group, identifier);
    if (idp) {
      IDP_FreeFromGroup(group, idp);

      return true;
    }
  }
  return false;
}

bool RNA_struct_is_a(const StructRNA *type, const StructRNA *srna)
{
  const StructRNA *base;

  if (srna == &RNA_AnyType) {
    return true;
  }

  if (!type) {
    return false;
  }

  /* ptr->type is always maximally refined */
  for (base = type; base; base = base->base) {
    if (base == srna) {
      return true;
    }
  }

  return false;
}

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier)
{
  if (identifier[0] == '[' && identifier[1] == '"') {
    /* id prop lookup, not so common */
    PropertyRNA *prop_test = nullptr;
    PointerRNA ptr_test; /* only support single level props */
    if (RNA_path_resolve_property(ptr, identifier, &ptr_test, &prop_test) &&
        (ptr_test.type == ptr->type) && (ptr_test.data == ptr->data))
    {
      return prop_test;
    }
  }
  else {
    /* most common case */
    PropertyRNA *iterprop = RNA_struct_iterator_property(ptr->type);
    PointerRNA propptr;

    if (RNA_property_collection_lookup_string(ptr, iterprop, identifier, &propptr)) {
      return static_cast<PropertyRNA *>(propptr.data);
    }
  }

  return nullptr;
}

static const char *rna_property_type_identifier(PropertyType prop_type)
{
  switch (prop_type) {
    case PROP_BOOLEAN:
      return RNA_struct_identifier(&RNA_BoolProperty);
    case PROP_INT:
      return RNA_struct_identifier(&RNA_IntProperty);
    case PROP_FLOAT:
      return RNA_struct_identifier(&RNA_FloatProperty);
    case PROP_STRING:
      return RNA_struct_identifier(&RNA_StringProperty);
    case PROP_ENUM:
      return RNA_struct_identifier(&RNA_EnumProperty);
    case PROP_POINTER:
      return RNA_struct_identifier(&RNA_PointerProperty);
    case PROP_COLLECTION:
      return RNA_struct_identifier(&RNA_CollectionProperty);
    default:
      return RNA_struct_identifier(&RNA_Property);
  }
}

PropertyRNA *RNA_struct_find_property_check(PointerRNA &props,
                                            const char *name,
                                            const PropertyType property_type_check)
{
  PropertyRNA *prop = RNA_struct_find_property(&props, name);
  if (!prop) {
    return nullptr;
  }
  const PropertyType prop_type = RNA_property_type(prop);
  if (prop_type == property_type_check) {
    return prop;
  }
  CLOG_WARN(&LOG,
            "'%s : %s()' expected, got '%s : %s()'",
            name,
            rna_property_type_identifier(property_type_check),
            name,
            rna_property_type_identifier(prop_type));
  return nullptr;
}

PropertyRNA *RNA_struct_find_collection_property_check(PointerRNA &props,
                                                       const char *name,
                                                       const StructRNA *struct_type_check)
{
  PropertyRNA *prop = RNA_struct_find_property(&props, name);
  if (!prop) {
    return nullptr;
  }

  const PropertyType prop_type = RNA_property_type(prop);
  const StructRNA *prop_struct_type = RNA_property_pointer_type(&props, prop);
  if (prop_type == PROP_COLLECTION && prop_struct_type == struct_type_check) {
    return prop;
  }

  if (prop_type != PROP_COLLECTION) {
    CLOG_WARN(&LOG,
              "'%s : %s(type = %s)' expected, got '%s : %s()'",
              name,
              rna_property_type_identifier(PROP_COLLECTION),
              RNA_struct_identifier(struct_type_check),
              name,
              rna_property_type_identifier(prop_type));
    return nullptr;
  }

  CLOG_WARN(&LOG,
            "'%s : %s(type = %s)' expected, got '%s : %s(type = %s)'.",
            name,
            rna_property_type_identifier(PROP_COLLECTION),
            RNA_struct_identifier(struct_type_check),
            name,
            rna_property_type_identifier(PROP_COLLECTION),
            RNA_struct_identifier(prop_struct_type));
  return nullptr;
}

PropertyRNA *rna_struct_find_nested(PointerRNA *ptr, StructRNA *srna)
{
  PropertyRNA *prop = nullptr;

  RNA_STRUCT_BEGIN (ptr, iprop) {
    /* This assumes that there can only be one user of this nested struct */
    if (RNA_property_pointer_type(ptr, iprop) == srna) {
      prop = iprop;
      break;
    }
  }
  RNA_PROP_END;

  return prop;
}

bool RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test)
{
  /* NOTE: prop_test could be freed memory, only use for comparison. */

  /* validate the RNA is ok */
  PropertyRNA *iterprop;
  bool found = false;

  iterprop = RNA_struct_iterator_property(ptr->type);

  RNA_PROP_BEGIN (ptr, itemptr, iterprop) {
    // PropertyRNA *prop = itemptr.data;
    if (prop_test == (PropertyRNA *)itemptr.data) {
      found = true;
      break;
    }
  }
  RNA_PROP_END;

  return found;
}

uint RNA_struct_count_properties(StructRNA *srna)
{
  uint counter = 0;

  PointerRNA struct_ptr = RNA_pointer_create_discrete(nullptr, srna, nullptr);

  RNA_STRUCT_BEGIN (&struct_ptr, prop) {
    counter++;
    UNUSED_VARS(prop);
  }
  RNA_STRUCT_END;

  return counter;
}

std::optional<AncestorPointerRNA> RNA_struct_search_closest_ancestor_by_type(PointerRNA *ptr,
                                                                             const StructRNA *srna)
{
  if (RNA_struct_is_a(ptr->type, srna)) {
    return {{ptr->type, ptr->data}};
  }
  else {
    for (int i = ptr->ancestors.size() - 1; i >= 0; i--) {
      const AncestorPointerRNA &ancestor = ptr->ancestors[i];
      if (RNA_struct_is_a(ancestor.type, srna)) {
        return ancestor;
      }
    }
  }

  return std::nullopt;
}

const ListBase *RNA_struct_type_properties(StructRNA *srna)
{
  return &srna->cont.properties;
}

PropertyRNA *RNA_struct_type_find_property_no_base(StructRNA *srna, const char *identifier)
{
  return static_cast<PropertyRNA *>(
      BLI_findstring_ptr(&srna->cont.properties, identifier, offsetof(PropertyRNA, identifier)));
}

PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier)
{
  for (; srna; srna = srna->base) {
    PropertyRNA *prop = RNA_struct_type_find_property_no_base(srna, identifier);
    if (prop != nullptr) {
      return prop;
    }
  }
  return nullptr;
}

FunctionRNA *RNA_struct_find_function(StructRNA *srna, const char *identifier)
{
#if 1
  FunctionRNA *func;
  for (; srna; srna = srna->base) {
    func = (FunctionRNA *)BLI_findstring_ptr(
        &srna->functions, identifier, offsetof(FunctionRNA, identifier));
    if (func) {
      return func;
    }
  }
  return nullptr;

  /* functional but slow */
#else
  PropertyRNA *iterprop;
  FunctionRNA *func;

  PointerRNA tptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, srna);
  iterprop = RNA_struct_find_property(&tptr, "functions");

  func = nullptr;

  RNA_PROP_BEGIN (&tptr, funcptr, iterprop) {
    if (STREQ(identifier, RNA_function_identifier(funcptr.data))) {
      func = funcptr.data;
      break;
    }
  }
  RNA_PROP_END;

  return func;
#endif
}

const ListBase *RNA_struct_type_functions(StructRNA *srna)
{
  return &srna->functions;
}

StructRegisterFunc RNA_struct_register(StructRNA *type)
{
  return type->reg;
}

StructUnregisterFunc RNA_struct_unregister(StructRNA *type)
{
  do {
    if (type->unreg) {
      return type->unreg;
    }
  } while ((type = type->base));

  return nullptr;
}

void **RNA_struct_instance(PointerRNA *ptr)
{
  StructRNA *type = ptr->type;

  do {
    if (type->instance) {
      return type->instance(ptr);
    }
  } while ((type = type->base));

  return nullptr;
}

void *RNA_struct_py_type_get(StructRNA *srna)
{
  return srna->py_type;
}

void RNA_struct_py_type_set(StructRNA *srna, void *py_type)
{
  srna->py_type = py_type;
}

void *RNA_struct_blender_type_get(StructRNA *srna)
{
  return srna->blender_type;
}

void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type)
{
  srna->blender_type = blender_type;
}

char *RNA_struct_name_get_alloc_ex(
    PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len, PropertyRNA **r_nameprop)
{
  if (ptr->data) {
    if (PropertyRNA *nameprop = RNA_struct_name_property(ptr->type)) {
      *r_nameprop = nameprop;
      return RNA_property_string_get_alloc(ptr, nameprop, fixedbuf, fixedlen, r_len);
    }
  }
  return nullptr;
}

char *RNA_struct_name_get_alloc(PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len)
{
  if (ptr->data) {
    if (PropertyRNA *nameprop = RNA_struct_name_property(ptr->type)) {
      return RNA_property_string_get_alloc(ptr, nameprop, fixedbuf, fixedlen, r_len);
    }
  }
  return nullptr;
}

bool RNA_struct_available_or_report(ReportList *reports, const char *identifier)
{
  const StructRNA *srna_exists = RNA_struct_find(identifier);
  if (UNLIKELY(srna_exists != nullptr)) {
    /* Use comprehensive string construction since this is such a rare occurrence
     * and information here may cut down time troubleshooting. */
    DynStr *dynstr = BLI_dynstr_new();
    BLI_dynstr_appendf(dynstr, "Type identifier '%s' is already in use: '", identifier);
    BLI_dynstr_append(dynstr, srna_exists->identifier);
    int i = 0;
    if (srna_exists->base) {
      for (const StructRNA *base = srna_exists->base; base; base = base->base) {
        BLI_dynstr_append(dynstr, "(");
        BLI_dynstr_append(dynstr, base->identifier);
        i += 1;
      }
      while (i--) {
        BLI_dynstr_append(dynstr, ")");
      }
    }
    BLI_dynstr_append(dynstr, "'.");
    char *result = BLI_dynstr_get_cstring(dynstr);
    BLI_dynstr_free(dynstr);
    BKE_report(reports, RPT_ERROR, result);
    MEM_freeN(result);
    return false;
  }
  return true;
}

bool RNA_struct_bl_idname_ok_or_report(ReportList *reports,
                                       const char *identifier,
                                       const char *sep)
{
  const int len_sep = strlen(sep);
  const int len_id = strlen(identifier);
  const char *p = strstr(identifier, sep);
  /* TODO: make error, for now warning until add-ons update. */
#if 1
  const int report_level = RPT_WARNING;
  const bool failure = true;
#else
  const int report_level = RPT_ERROR;
  const bool failure = false;
#endif
  if (p == nullptr || p == identifier || p + len_sep >= identifier + len_id) {
    BKE_reportf(reports,
                eReportType(report_level),
                "'%s' does not contain '%s' with prefix and suffix",
                identifier,
                sep);
    return failure;
  }

  const char *c, *start, *end, *last;
  start = identifier;
  end = p;
  last = end - 1;
  for (c = start; c != end; c++) {
    if (((*c >= 'A' && *c <= 'Z') || ((c != start) && (*c >= '0' && *c <= '9')) ||
         ((c != start) && (c != last) && (*c == '_'))) == 0)
    {
      BKE_reportf(reports,
                  eReportType(report_level),
                  "'%s' does not have upper case alphanumeric prefix",
                  identifier);
      return failure;
    }
  }

  start = p + len_sep;
  end = identifier + len_id;
  last = end - 1;
  for (c = start; c != end; c++) {
    if (((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
         ((c != start) && (c != last) && (*c == '_'))) == 0)
    {
      BKE_reportf(reports,
                  eReportType(report_level),
                  "'%s' does not have an alphanumeric suffix",
                  identifier);
      return failure;
    }
  }
  return true;
}

/* Property Information */

const char *RNA_property_identifier(const PropertyRNA *prop)
{
  return rna_ensure_property_identifier(prop);
}

const char *RNA_property_description(PropertyRNA *prop)
{
  return TIP_(rna_ensure_property_description(prop));
}

const DeprecatedRNA *RNA_property_deprecated(const PropertyRNA *prop)
{
  return prop->deprecated;
}

PropertyType RNA_property_type(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->type;
}

PropertySubType RNA_property_subtype(PropertyRNA *prop)
{
  PropertyRNA *rna_prop = rna_ensure_property(prop);

  /* For custom properties, find and parse the 'subtype' metadata field. */
  if (prop->magic != RNA_MAGIC) {
    IDProperty *idprop = (IDProperty *)prop;

    if (idprop->type == IDP_STRING && idprop->subtype == IDP_STRING_SUB_BYTE) {
      return PROP_BYTESTRING;
    }

    if (idprop->ui_data) {
      IDPropertyUIData *ui_data = idprop->ui_data;
      return (PropertySubType)ui_data->rna_subtype;
    }
  }

  return rna_prop->subtype;
}

PropertyUnit RNA_property_unit(PropertyRNA *prop)
{
  return PropertyUnit(RNA_SUBTYPE_UNIT(RNA_property_subtype(prop)));
}

PropertyScaleType RNA_property_ui_scale(PropertyRNA *prop)
{
  PropertyRNA *rna_prop = rna_ensure_property(prop);

  switch (rna_prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)rna_prop;
      return iprop->ui_scale_type;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_prop;
      return fprop->ui_scale_type;
    }
    default:
      return PROP_SCALE_LINEAR;
  }
}

int RNA_property_flag(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->flag;
}

int RNA_property_tags(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->tags;
}

PropertyPathTemplateType RNA_property_path_template_type(PropertyRNA *prop)
{
  return rna_ensure_property(prop)->path_template_type;
}

bool RNA_property_builtin(PropertyRNA *prop)
{
  return (rna_ensure_property(prop)->flag_internal & PROP_INTERN_BUILTIN) != 0;
}

void *RNA_property_py_data_get(PropertyRNA *prop)
{
  /* This is only called in isolated situations (mainly by the Python API),
   * so skip check for ID properties. Callers must use #RNA_property_is_idprop
   * when it's not known if the property might be an ID Property. */
  BLI_assert(prop->magic == RNA_MAGIC);
  return prop->py_data;
}

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop)
{
  return rna_ensure_property_array_length(ptr, prop);
}

bool RNA_property_array_check(PropertyRNA *prop)
{
  return rna_ensure_property_array_check(prop);
}

int RNA_property_array_dimension(const PointerRNA *ptr, PropertyRNA *prop, int length[])
{
  PropertyRNA *rprop = rna_ensure_property(prop);

  if (length) {
    rna_ensure_property_multi_array_length(ptr, prop, length);
  }

  return rprop->arraydimension;
}

int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dimension)
{
  int len[RNA_MAX_ARRAY_DIMENSION];

  rna_ensure_property_multi_array_length(ptr, prop, len);

  return len[dimension];
}

char RNA_property_array_item_char(PropertyRNA *prop, int index)
{
  const char *vectoritem = "XYZW";
  const char *quatitem = "WXYZ";
  const char *coloritem = "RGBA";
  PropertySubType subtype = RNA_property_subtype(prop);

  BLI_assert(index >= 0);

  /* get string to use for array index */
  if ((index < 4) && ELEM(subtype, PROP_QUATERNION, PROP_AXISANGLE)) {
    return quatitem[index];
  }
  if ((index < 4) && ELEM(subtype,
                          PROP_TRANSLATION,
                          PROP_DIRECTION,
                          PROP_XYZ,
                          PROP_XYZ_LENGTH,
                          PROP_EULER,
                          PROP_VELOCITY,
                          PROP_ACCELERATION,
                          PROP_COORDS))
  {
    return vectoritem[index];
  }
  if ((index < 4) && ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
    return coloritem[index];
  }

  return '\0';
}

int RNA_property_array_item_index(PropertyRNA *prop, char name)
{
  /* Don't use custom property sub-types in RNA path lookup. */
  PropertySubType subtype = rna_ensure_property(prop)->subtype;

  /* get index based on string name/alias */
  /* maybe a function to find char index in string would be better than all the switches */
  if (ELEM(subtype, PROP_QUATERNION, PROP_AXISANGLE)) {
    switch (name) {
      case 'w':
        return 0;
      case 'x':
        return 1;
      case 'y':
        return 2;
      case 'z':
        return 3;
    }
  }
  else if (ELEM(subtype,
                PROP_TRANSLATION,
                PROP_DIRECTION,
                PROP_XYZ,
                PROP_XYZ_LENGTH,
                PROP_EULER,
                PROP_VELOCITY,
                PROP_ACCELERATION))
  {
    switch (name) {
      case 'x':
        return 0;
      case 'y':
        return 1;
      case 'z':
        return 2;
      case 'w':
        return 3;
    }
  }
  else if (ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
    switch (name) {
      case 'r':
        return 0;
      case 'g':
        return 1;
      case 'b':
        return 2;
      case 'a':
        return 3;
    }
  }

  return -1;
}

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
  int softmin, softmax;

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->ui_data) {
      IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)idprop->ui_data;
      *hardmin = ui_data->min;
      *hardmax = ui_data->max;
    }
    else {
      *hardmin = INT_MIN;
      *hardmax = INT_MAX;
    }
    return;
  }

  if (iprop->range) {
    *hardmin = INT_MIN;
    *hardmax = INT_MAX;

    iprop->range(ptr, hardmin, hardmax, &softmin, &softmax);
  }
  else if (iprop->range_ex) {
    *hardmin = INT_MIN;
    *hardmax = INT_MAX;

    iprop->range_ex(ptr, prop, hardmin, hardmax, &softmin, &softmax);
  }
  else {
    *hardmin = iprop->hardmin;
    *hardmax = iprop->hardmax;
  }
}

void RNA_property_int_ui_range(
    PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);
  int hardmin, hardmax;

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->ui_data) {
      IDPropertyUIDataInt *ui_data_int = (IDPropertyUIDataInt *)idprop->ui_data;
      *softmin = ui_data_int->soft_min;
      *softmax = ui_data_int->soft_max;
      *step = ui_data_int->step;
    }
    else {
      *softmin = INT_MIN;
      *softmax = INT_MAX;
      *step = 1;
    }
    return;
  }

  *softmin = iprop->softmin;
  *softmax = iprop->softmax;

  if (iprop->range) {
    hardmin = INT_MIN;
    hardmax = INT_MAX;

    iprop->range(ptr, &hardmin, &hardmax, softmin, softmax);

    *softmin = max_ii(*softmin, hardmin);
    *softmax = min_ii(*softmax, hardmax);
  }
  else if (iprop->range_ex) {
    hardmin = INT_MIN;
    hardmax = INT_MAX;

    iprop->range_ex(ptr, prop, &hardmin, &hardmax, softmin, softmax);

    *softmin = max_ii(*softmin, hardmin);
    *softmax = min_ii(*softmax, hardmax);
  }

  *step = iprop->step;
}

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);
  float softmin, softmax;

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->ui_data) {
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)idprop->ui_data;
      *hardmin = float(ui_data->min);
      *hardmax = float(ui_data->max);
    }
    else {
      *hardmin = -FLT_MAX;
      *hardmax = FLT_MAX;
    }
    return;
  }

  if (fprop->range) {
    *hardmin = -FLT_MAX;
    *hardmax = FLT_MAX;

    fprop->range(ptr, hardmin, hardmax, &softmin, &softmax);
  }
  else if (fprop->range_ex) {
    *hardmin = -FLT_MAX;
    *hardmax = FLT_MAX;

    fprop->range_ex(ptr, prop, hardmin, hardmax, &softmin, &softmax);
  }
  else {
    *hardmin = fprop->hardmin;
    *hardmax = fprop->hardmax;
  }
}

void RNA_property_float_ui_range(PointerRNA *ptr,
                                 PropertyRNA *prop,
                                 float *softmin,
                                 float *softmax,
                                 float *step,
                                 float *precision)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);
  float hardmin, hardmax;

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->ui_data) {
      IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)idprop->ui_data;
      *softmin = float(ui_data->soft_min);
      *softmax = float(ui_data->soft_max);
      *step = ui_data->step;
      *precision = float(ui_data->precision);
    }
    else {
      *softmin = -FLT_MAX;
      *softmax = FLT_MAX;
      *step = 1.0f;
      *precision = 3.0f;
    }
    return;
  }

  *softmin = fprop->softmin;
  *softmax = fprop->softmax;

  if (fprop->range) {
    hardmin = -FLT_MAX;
    hardmax = FLT_MAX;

    fprop->range(ptr, &hardmin, &hardmax, softmin, softmax);

    *softmin = max_ff(*softmin, hardmin);
    *softmax = min_ff(*softmax, hardmax);
  }
  else if (fprop->range_ex) {
    hardmin = -FLT_MAX;
    hardmax = FLT_MAX;

    fprop->range_ex(ptr, prop, &hardmin, &hardmax, softmin, softmax);

    *softmin = max_ff(*softmin, hardmin);
    *softmax = min_ff(*softmax, hardmax);
  }

  *step = fprop->step;
  *precision = float(fprop->precision);
}

int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value)
{
  float min, max;

  RNA_property_float_range(ptr, prop, &min, &max);

  if (*value < min) {
    *value = min;
    return -1;
  }
  if (*value > max) {
    *value = max;
    return 1;
  }
  return 0;
}

int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value)
{
  int min, max;

  RNA_property_int_range(ptr, prop, &min, &max);

  if (*value < min) {
    *value = min;
    return -1;
  }
  if (*value > max) {
    *value = max;
    return 1;
  }
  return 0;
}

int RNA_property_string_maxlength(PropertyRNA *prop)
{
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);
  return sprop->maxlength;
}

StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop)
{
  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->type == IDP_ID) {
      const IDPropertyUIDataID *ui_data = (const IDPropertyUIDataID *)idprop->ui_data;
      if (ui_data) {
        return ID_code_to_RNA_type(ui_data->id_type);
      }
    }
  }

  prop = rna_ensure_property(prop);

  if (prop->type == PROP_POINTER) {
    PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

    if (pprop->type_fn) {
      return pprop->type_fn(ptr);
    }
    if (pprop->type) {
      return pprop->type;
    }
  }
  else if (prop->type == PROP_COLLECTION) {
    CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

    if (cprop->item_type) {
      return cprop->item_type;
    }
  }
  /* ignore other types, rna_struct_find_nested calls with unchecked props */

  return &RNA_UnknownType;
}

bool RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value)
{
  prop = rna_ensure_property(prop);

  if (prop->type != PROP_POINTER) {
    printf("%s: %s is not a pointer property.\n", __func__, prop->identifier);
    return false;
  }

  PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

  /* Can't point from linked to local datablock. */
  if (ptr->owner_id && value->owner_id && !BKE_id_can_use_id(*ptr->owner_id, *value->owner_id)) {
    return false;
  }

  /* Check custom poll function. */
  if (!pprop->poll) {
    return true;
  }

  if (rna_idproperty_check(&prop, ptr)) {
    return reinterpret_cast<PropPointerPollFuncPy>(reinterpret_cast<void *>(pprop->poll))(
        ptr, *value, prop);
  }
  return pprop->poll(ptr, *value);
}

void RNA_property_enum_items_ex(bContext *C,
                                PointerRNA *ptr,
                                PropertyRNA *prop,
                                const bool use_static,
                                const EnumPropertyItem **r_item,
                                int *r_totitem,
                                bool *r_free)
{
  if (!use_static && prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (IDProperty *)prop;
    if (idprop->type == IDP_INT) {
      IDPropertyUIDataInt *ui_data = reinterpret_cast<IDPropertyUIDataInt *>(idprop->ui_data);

      int totitem = 0;
      EnumPropertyItem *result = nullptr;
      if (ui_data) {
        for (const IDPropertyUIDataEnumItem &idprop_item :
             blender::Span(ui_data->enum_items, ui_data->enum_items_num))
        {
          BLI_assert(idprop_item.identifier != nullptr);
          BLI_assert(idprop_item.name != nullptr);
          const EnumPropertyItem item = {idprop_item.value,
                                         idprop_item.identifier,
                                         idprop_item.icon,
                                         idprop_item.name,
                                         idprop_item.description ? idprop_item.description : ""};
          RNA_enum_item_add(&result, &totitem, &item);
        }
      }

      RNA_enum_item_end(&result, &totitem);
      *r_item = result;
      if (r_totitem) {
        /* Exclude the terminator item. */
        *r_totitem = totitem - 1;
      }
      *r_free = true;
      return;
    }
  }

  EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);

  *r_free = false;

  if (!use_static && (eprop->item_fn != nullptr)) {
    const bool no_context = (prop->flag & PROP_ENUM_NO_CONTEXT) ||
                            ((ptr->type->flag & STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID) &&
                             (ptr->owner_id == nullptr));
    if (C != nullptr || no_context) {
      const EnumPropertyItem *item;

      item = eprop->item_fn(no_context ? nullptr : C, ptr, prop, r_free);

      /* any callbacks returning nullptr should be fixed */
      BLI_assert(item != nullptr);

      if (r_totitem) {
        int tot;
        for (tot = 0; item[tot].identifier; tot++) {
          /* pass */
        }
        *r_totitem = tot;
      }

      *r_item = item;
      return;
    }
  }

  *r_item = eprop->item;
  if (r_totitem) {
    *r_totitem = eprop->totitem;
  }
}

void RNA_property_enum_items(bContext *C,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             const EnumPropertyItem **r_item,
                             int *r_totitem,
                             bool *r_free)
{
  RNA_property_enum_items_ex(C, ptr, prop, false, r_item, r_totitem, r_free);
}

#ifdef WITH_INTERNATIONAL
static void property_enum_translate(PropertyRNA *prop,
                                    EnumPropertyItem **r_item,
                                    const int *totitem,
                                    bool *r_free)
{
  if (!(RNA_property_flag(prop) & PROP_ENUM_NO_TRANSLATE)) {
    int i;

    /* NOTE: Only do those tests once, and then use BLT_pgettext. */
    bool do_iface = BLT_translate_iface();
    bool do_tooltip = BLT_translate_tooltips();
    EnumPropertyItem *nitem;

    if (!(do_iface || do_tooltip)) {
      return;
    }

    if (*r_free) {
      nitem = *r_item;
    }
    else {
      const EnumPropertyItem *item = *r_item;
      int tot;

      if (totitem) {
        tot = *totitem;
      }
      else {
        /* count */
        for (tot = 0; item[tot].identifier; tot++) {
          /* pass */
        }
      }

      nitem = MEM_malloc_arrayN<EnumPropertyItem>(size_t(tot) + 1, __func__);
      memcpy(nitem, item, sizeof(EnumPropertyItem) * (tot + 1));

      *r_free = true;
    }

    const char *translation_context = RNA_property_translation_context(prop);

    for (i = 0; nitem[i].identifier; i++) {
      if (nitem[i].name && do_iface) {
        nitem[i].name = BLT_pgettext(translation_context, nitem[i].name);
      }
      if (nitem[i].description && do_tooltip) {
        nitem[i].description = BLT_pgettext(nullptr, nitem[i].description);
      }
    }

    *r_item = nitem;
  }
}
#endif

void RNA_property_enum_items_gettexted(bContext *C,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const EnumPropertyItem **r_item,
                                       int *r_totitem,
                                       bool *r_free)
{
  RNA_property_enum_items(C, ptr, prop, r_item, r_totitem, r_free);

#ifdef WITH_INTERNATIONAL
  /* Normally dropping 'const' is _not_ ok, in this case it's only modified if we own the memory
   * so allow the exception (callers are creating new arrays in this case). */
  property_enum_translate(prop, (EnumPropertyItem **)r_item, r_totitem, r_free);
#endif
}

void RNA_property_enum_items_gettexted_all(bContext *C,
                                           PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const EnumPropertyItem **r_item,
                                           int *r_totitem,
                                           bool *r_free)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);
  size_t mem_size = sizeof(EnumPropertyItem) * (size_t(eprop->totitem) + 1);
  /* first return all items */
  EnumPropertyItem *item_array = MEM_malloc_arrayN<EnumPropertyItem>(size_t(eprop->totitem) + 1,
                                                                     "enum_gettext_all");
  *r_free = true;
  memcpy(item_array, eprop->item, mem_size);

  if (r_totitem) {
    *r_totitem = eprop->totitem;
  }

  if (eprop->item_fn != nullptr) {
    const bool no_context = (eprop->property.flag & PROP_ENUM_NO_CONTEXT) ||
                            ((ptr->type->flag & STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID) &&
                             (ptr->owner_id == nullptr));
    if (C != nullptr || no_context) {
      const EnumPropertyItem *item;
      int i;
      bool free = false;

      item = eprop->item_fn(no_context ? nullptr : nullptr, ptr, prop, &free);

      /* any callbacks returning nullptr should be fixed */
      BLI_assert(item != nullptr);

      for (i = 0; i < eprop->totitem; i++) {
        bool exists = false;
        int i_fixed;

        /* Items that do not exist on list are returned,
         * but have their names/identifiers null'ed out. */
        for (i_fixed = 0; item[i_fixed].identifier; i_fixed++) {
          if (STREQ(item[i_fixed].identifier, item_array[i].identifier)) {
            exists = true;
            break;
          }
        }

        if (!exists) {
          item_array[i].name = nullptr;
          item_array[i].identifier = "";
        }
      }

      if (free) {
        MEM_freeN(item);
      }
    }
  }

#ifdef WITH_INTERNATIONAL
  property_enum_translate(prop, &item_array, r_totitem, r_free);
#endif
  *r_item = item_array;
}

bool RNA_property_enum_value(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *r_value)
{
  const EnumPropertyItem *item;
  bool free;
  bool found;

  RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);

  if (item) {
    const int i = RNA_enum_from_identifier(item, identifier);
    if (i != -1) {
      *r_value = item[i].value;
      found = true;
    }
    else {
      found = false;
    }

    if (free) {
      MEM_freeN(item);
    }
  }
  else {
    found = false;
  }
  return found;
}

bool RNA_enum_identifier(const EnumPropertyItem *item, const int value, const char **r_identifier)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_identifier = item[i].identifier;
    return true;
  }
  return false;
}

int RNA_enum_bitflag_identifiers(const EnumPropertyItem *item,
                                 const int value,
                                 const char **r_identifier)
{
  int index = 0;
  for (; item->identifier; item++) {
    if (item->identifier[0] && item->value & value) {
      r_identifier[index++] = item->identifier;
    }
  }
  r_identifier[index] = nullptr;
  return index;
}

bool RNA_enum_name(const EnumPropertyItem *item, const int value, const char **r_name)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_name = item[i].name;
    return true;
  }
  return false;
}

bool RNA_enum_name_gettexted(const EnumPropertyItem *item,
                             int value,
                             const char *translation_context,
                             const char **r_name)
{
  bool result;

  result = RNA_enum_name(item, value, r_name);

  if (result) {
    *r_name = BLT_translate_do_iface(translation_context, *r_name);
  }

  return result;
};

bool RNA_enum_description(const EnumPropertyItem *item,
                          const int value,
                          const char **r_description)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_description = item[i].description;
    return true;
  }
  return false;
}

int RNA_enum_from_identifier(const EnumPropertyItem *item, const char *identifier)
{
  int i = 0;
  for (; item->identifier; item++, i++) {
    if (item->identifier[0] && STREQ(item->identifier, identifier)) {
      return i;
    }
  }
  return -1;
}

bool RNA_enum_value_from_identifier(const EnumPropertyItem *item,
                                    const char *identifier,
                                    int *r_value)
{
  const int i = RNA_enum_from_identifier(item, identifier);
  if (i == -1) {
    return false;
  }
  *r_value = item[i].value;
  return true;
}

int RNA_enum_from_name(const EnumPropertyItem *item, const char *name)
{
  int i = 0;
  for (; item->identifier; item++, i++) {
    if (item->identifier[0] && STREQ(item->name, name)) {
      return i;
    }
  }
  return -1;
}

int RNA_enum_from_value(const EnumPropertyItem *item, const int value)
{
  int i = 0;
  for (; item->identifier; item++, i++) {
    if (item->identifier[0] && item->value == value) {
      return i;
    }
  }
  return -1;
}

uint RNA_enum_items_count(const EnumPropertyItem *item)
{
  uint i = 0;

  while (item->identifier) {
    item++;
    i++;
  }

  return i;
}

bool RNA_property_enum_identifier(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **r_identifier)
{
  const EnumPropertyItem *item = nullptr;
  bool free;

  RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);
  if (item) {
    bool result;
    result = RNA_enum_identifier(item, value, r_identifier);
    if (free) {
      MEM_freeN(item);
    }
    return result;
  }
  return false;
}

bool RNA_property_enum_name(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **r_name)
{
  const EnumPropertyItem *item = nullptr;
  bool free;

  RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);
  if (item) {
    bool result;
    result = RNA_enum_name(item, value, r_name);
    if (free) {
      MEM_freeN(item);
    }

    return result;
  }
  return false;
}

bool RNA_property_enum_name_gettexted(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **r_name)
{
  bool result;

  result = RNA_property_enum_name(C, ptr, prop, value, r_name);

  if (result) {
    if (!(prop->flag & PROP_ENUM_NO_TRANSLATE)) {
      *r_name = BLT_translate_do_iface(RNA_property_translation_context(prop), *r_name);
    }
  }

  return result;
}

bool RNA_property_enum_item_from_value(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, EnumPropertyItem *r_item)
{
  const EnumPropertyItem *item = nullptr;
  bool free;

  RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);
  if (item) {
    const int i = RNA_enum_from_value(item, value);
    bool result;

    if (i != -1) {
      *r_item = item[i];
      result = true;
    }
    else {
      result = false;
    }

    if (free) {
      MEM_freeN(item);
    }

    return result;
  }
  return false;
}

bool RNA_property_enum_item_from_value_gettexted(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, EnumPropertyItem *r_item)
{
  const bool result = RNA_property_enum_item_from_value(C, ptr, prop, value, r_item);

  if (result && !(RNA_property_flag(prop) & PROP_ENUM_NO_TRANSLATE)) {
    r_item->name = BLT_translate_do_iface(RNA_property_translation_context(prop), r_item->name);
    r_item->description = BLT_translate_do_tooltip(nullptr, r_item->description);
  }

  return result;
}

int RNA_property_enum_bitflag_identifiers(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **r_identifier)
{
  const EnumPropertyItem *item = nullptr;
  bool free;

  RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);
  if (item) {
    int result;
    result = RNA_enum_bitflag_identifiers(item, value, r_identifier);
    if (free) {
      MEM_freeN(item);
    }

    return result;
  }
  return 0;
}

const char *RNA_property_ui_name(const PropertyRNA *prop, const PointerRNA *ptr)
{
  if (ptr && prop->magic == RNA_MAGIC && prop->ui_name_func) {
    if (const char *name = prop->ui_name_func(ptr, prop, true)) {
      return name;
    }
  }
  return CTX_IFACE_(RNA_property_translation_context(prop), rna_ensure_property_name(prop));
}

const char *RNA_property_ui_name_raw(const PropertyRNA *prop, const PointerRNA *ptr)
{
  if (ptr && prop->magic == RNA_MAGIC && prop->ui_name_func) {
    if (const char *name = prop->ui_name_func(ptr, prop, false)) {
      return name;
    }
  }
  return rna_ensure_property_name(prop);
}

const char *RNA_property_ui_description(const PropertyRNA *prop)
{
  return TIP_(rna_ensure_property_description(prop));
}

const char *RNA_property_ui_description_raw(const PropertyRNA *prop)
{
  return rna_ensure_property_description(prop);
}

const char *RNA_property_translation_context(const PropertyRNA *prop)
{
  return rna_ensure_property((PropertyRNA *)prop)->translation_context;
}

int RNA_property_ui_icon(const PropertyRNA *prop)
{
  return rna_ensure_property((PropertyRNA *)prop)->icon;
}

static bool rna_property_editable_do(const PointerRNA *ptr,
                                     PropertyRNA *prop_orig,
                                     const int index,
                                     const char **r_info)
{
  ID *id = ptr->owner_id;

  PropertyRNA *prop = rna_ensure_property(prop_orig);

  const char *info = "";
  const int flag = (prop->itemeditable != nullptr && index >= 0) ?
                       prop->itemeditable(ptr, index) :
                       (prop->editable != nullptr ? prop->editable(ptr, &info) : prop->flag);
  if (r_info != nullptr) {
    *r_info = info;
  }

  /* Early return if the property itself is not editable. */
  if ((flag & PROP_EDITABLE) == 0) {
    return false;
  }
  /* Only considered registerable properties "internal"
   * because regular properties may not be editable and still be displayed. */
  if (flag & PROP_REGISTER) {
    if (r_info != nullptr && (*r_info)[0] == '\0') {
      *r_info = N_("This property is for internal use only and cannot be edited");
    }
    return false;
  }

  /* If there is no owning ID, the property is editable at this point. */
  if (id == nullptr) {
    return true;
  }

  /* Handle linked or liboverride ID cases. */
  const bool is_linked_prop_exception = (prop->flag & PROP_LIB_EXCEPTION) != 0;
  if (!ID_IS_EDITABLE(id)) {
    if (is_linked_prop_exception) {
      return true;
    }
    if (r_info != nullptr && (*r_info)[0] == '\0') {
      *r_info = N_("Cannot edit this property from a linked data-block");
    }
    return false;
  }
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    const bool is_liboverride_system = BKE_lib_override_library_is_system_defined(G_MAIN, id);
    if (!RNA_property_overridable_get(ptr, prop_orig)) {
      if (r_info != nullptr && (*r_info)[0] == '\0') {
        *r_info = N_("Cannot edit this property from an override data-block");
      }
      return false;
    }
    if (is_liboverride_system && !is_linked_prop_exception) {
      if (r_info != nullptr && (*r_info)[0] == '\0') {
        *r_info = N_("Cannot edit this property from a system override data-block");
      }
      return false;
    }
  }

  /* At this point, property is owned by a local ID and therefore fully editable. */
  return true;
}

bool RNA_property_is_runtime(const PropertyRNA *prop)
{
  return prop->flag_internal & PROP_INTERN_RUNTIME;
}

bool RNA_property_editable(const PointerRNA *ptr, PropertyRNA *prop)
{
  return rna_property_editable_do(ptr, prop, -1, nullptr);
}

bool RNA_property_editable_info(const PointerRNA *ptr, PropertyRNA *prop, const char **r_info)
{
  return rna_property_editable_do(ptr, prop, -1, r_info);
}

bool RNA_property_editable_flag(const PointerRNA *ptr, PropertyRNA *prop)
{
  int flag;
  const char *dummy_info;

  prop = rna_ensure_property(prop);
  flag = prop->editable ? prop->editable(ptr, &dummy_info) : prop->flag;
  return (flag & PROP_EDITABLE) != 0;
}

bool RNA_property_editable_index(const PointerRNA *ptr, PropertyRNA *prop, const int index)
{
  BLI_assert(index >= 0);

  return rna_property_editable_do(ptr, prop, index, nullptr);
}

bool RNA_property_animateable(const PointerRNA *ptr, PropertyRNA *prop_orig)
{
  /* check that base ID-block can support animation data */
  if (!id_can_have_animdata(ptr->owner_id)) {
    return false;
  }

  PropertyRNA *prop_ensured = rna_ensure_property(prop_orig);

  if (!(prop_ensured->flag & PROP_ANIMATABLE)) {
    return false;
  }

  return true;
}

bool RNA_property_anim_editable(const PointerRNA *ptr, PropertyRNA *prop_orig)
{
  /* check that base ID-block can support animation data */
  if (!RNA_property_animateable(ptr, prop_orig)) {
    return false;
  }

  /* Linked or LibOverride Action IDs are not editable at the FCurve level. */
  if (ptr->owner_id) {
    AnimData *anim_data = BKE_animdata_from_id(ptr->owner_id);
    if (anim_data && anim_data->action &&
        (!ID_IS_EDITABLE(anim_data->action) || ID_IS_OVERRIDE_LIBRARY(anim_data->action)))
    {
      return false;
    }
  }

  return rna_property_editable_do(ptr, prop_orig, -1, nullptr);
}

bool RNA_property_driver_editable(const PointerRNA *ptr, PropertyRNA *prop)
{
  if (!RNA_property_anim_editable(ptr, prop)) {
    return false;
  }

  /* LibOverrides can only get drivers if their animdata (if any) was created for the local
   * liboverride, and there is none in the linked reference.
   *
   * See also #rna_AnimaData_override_apply. */
  if (ptr->owner_id && ID_IS_OVERRIDE_LIBRARY(ptr->owner_id)) {
    IDOverrideLibrary *liboverride = BKE_lib_override_library_get(
        nullptr, ptr->owner_id, nullptr, nullptr);
    AnimData *linked_reference_anim_data = BKE_animdata_from_id(liboverride->reference);
    if (linked_reference_anim_data) {
      return false;
    }
  }

  return true;
}

bool RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop)
{
  int len = 1, index;
  bool driven, special;

  if (!prop) {
    return false;
  }

  if (RNA_property_array_check(prop)) {
    len = RNA_property_array_length(ptr, prop);
  }

  for (index = 0; index < len; index++) {
    if (BKE_fcurve_find_by_rna(ptr, prop, index, nullptr, nullptr, &driven, &special)) {
      return true;
    }
  }

  return false;
}
bool RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop)
{
  const std::optional<std::string> path = RNA_path_from_ID_to_property(ptr, prop);
  bool ret = false;

  if (path) {
    PointerRNA ptr_test;
    PropertyRNA *prop_test;

    PointerRNA id_ptr = RNA_id_pointer_create(ptr->owner_id);
    if (RNA_path_resolve(&id_ptr, path->c_str(), &ptr_test, &prop_test) == true) {
      ret = (prop == prop_test);
    }
  }

  return ret;
}

static void rna_property_update(
    bContext *C, Main *bmain, Scene *scene, PointerRNA *ptr, PropertyRNA *prop)
{
  const bool is_rna = (prop->magic == RNA_MAGIC);
  prop = rna_ensure_property(prop);

  if (is_rna) {
    if (prop->update) {
      /* ideally no context would be needed for update, but there's some
       * parts of the code that need it still, so we have this exception */
      if (prop->flag & PROP_CONTEXT_UPDATE) {
        if (C) {
          if ((prop->flag & PROP_CONTEXT_PROPERTY_UPDATE) == PROP_CONTEXT_PROPERTY_UPDATE) {
            ((ContextPropUpdateFunc)prop->update)(C, ptr, prop);
          }
          else {
            reinterpret_cast<ContextUpdateFunc>(reinterpret_cast<void *>(prop->update))(C, ptr);
          }
        }
      }
      else {
        prop->update(bmain, scene, ptr);
      }
    }

#if 1
    /* TODO(@ideasman42): Should eventually be replaced entirely by message bus (below)
     * for now keep since copy-on-eval, bugs are hard to track when we have other missing updates.
     */
    if (prop->noteflag) {
      WM_main_add_notifier(prop->noteflag, ptr->owner_id);
    }
#endif

    /* if C is nullptr, we're updating from animation.
     * avoid slow-down from f-curves by not publishing (for now). */
    if (C != nullptr) {
      wmMsgBus *mbus = CTX_wm_message_bus(C);
      /* we could add nullptr check, for now don't */
      WM_msg_publish_rna(mbus, ptr, prop);
    }
    if (ptr->owner_id != nullptr && ((prop->flag & PROP_NO_DEG_UPDATE) == 0)) {
      const short id_type = GS(ptr->owner_id->name);
      if (ID_TYPE_USE_COPY_ON_EVAL(id_type)) {
        if (prop->flag & PROP_DEG_SYNC_ONLY) {
          DEG_id_tag_update(ptr->owner_id, ID_RECALC_SYNC_TO_EVAL);
        }
        else {
          DEG_id_tag_update(ptr->owner_id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_PARAMETERS);
        }
      }
    }
    /* End message bus. */
  }

  if (!is_rna || (prop->flag & PROP_IDPROPERTY)) {

    /* Disclaimer: this logic is not applied consistently, causing some confusing behavior.
     *
     * - When animated (which skips update functions).
     * - When ID-properties are edited via Python (since RNA properties aren't used in this case).
     *
     * Adding updates will add a lot of overhead in the case of animation.
     * For Python it may cause unexpected slow-downs for developers using ID-properties
     * for data storage. Further, the root ID isn't available with nested data-structures.
     *
     * So editing custom properties only causes updates in the UI,
     * keep this exception because it happens to be useful for driving settings.
     * Python developers on the other hand will need to manually 'update_tag', see: #74000. */
    DEG_id_tag_update(ptr->owner_id,
                      ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_PARAMETERS);

    /* When updating an ID pointer property, tag depsgraph for update. */
    if (prop->type == PROP_POINTER && RNA_struct_is_ID(RNA_property_pointer_type(ptr, prop))) {
      DEG_relations_tag_update(bmain);
    }

    WM_main_add_notifier(NC_WINDOW, nullptr);
    if (ptr->owner_id) {
      WM_main_add_notifier(NC_ID | NA_EDITED, nullptr);
    }
    /* Not nice as well, but the only way to make sure material preview
     * is updated with custom nodes.
     */
    if ((prop->flag & PROP_IDPROPERTY) != 0 && (ptr->owner_id != nullptr) &&
        (GS(ptr->owner_id->name) == ID_NT))
    {
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, nullptr);
    }
  }
}

bool RNA_property_update_check(PropertyRNA *prop)
{
  /* NOTE: must keep in sync with #rna_property_update. */
  return (prop->magic != RNA_MAGIC || prop->update || prop->noteflag);
}

void RNA_property_update(bContext *C, PointerRNA *ptr, PropertyRNA *prop)
{
  rna_property_update(C, CTX_data_main(C), CTX_data_scene(C), ptr, prop);
}

void RNA_property_update_main(Main *bmain, Scene *scene, PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(bmain != nullptr);
  rna_property_update(nullptr, bmain, scene, ptr, prop);
}

/* ---------------------------------------------------------------------- */

/* Property Data */

static bool property_boolean_get(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
#ifdef USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP
    return IDP_int_or_bool_get(prop_rna_or_id.idprop);
#else
    return IDP_bool_get(prop_rna_or_id.idprop);
#endif
  }
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (bprop->get) {
    return bprop->get(ptr);
  }
  if (bprop->get_ex) {
    return bprop->get_ex(ptr, &bprop->property);
  }
  return bprop->defaultvalue;
}

bool RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop_rna_or_id.rnaprop);

  bool value = property_boolean_get(ptr, prop_rna_or_id);
  if (bprop->get_transform) {
    value = bprop->get_transform(ptr, &bprop->property, value, prop_rna_or_id.is_set);
  }

  return value;
}

void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, bool value)
{
  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop_rna_or_id.rnaprop);

  if (bprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const bool curr_value = property_boolean_get(ptr, prop_rna_or_id);
    value = bprop->set_transform(ptr, &bprop->property, value, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
#ifdef USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP
    IDP_int_or_bool_set(idprop, value);
#else
    IDP_bool_set(idprop, value);
#endif
    rna_idproperty_touch(idprop);
  }
  else if (bprop->set) {
    bprop->set(ptr, value);
  }
  else if (bprop->set_ex) {
    bprop->set_ex(ptr, &bprop->property, value);
  }
  else if (bprop->property.flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
#ifdef USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create(prop_rna_or_id.identifier, int(value), IDP_FLAG_STATIC_TYPE)
              .release());
#else
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create_bool(prop_rna_or_id.identifier, value, IDP_FLAG_STATIC_TYPE)
              .release());
#endif
    }
  }
}

static void rna_property_boolean_fill_default_array_values(
    const bool *defarr, int defarr_length, bool defvalue, int out_length, bool *r_values)
{
  if (defarr && defarr_length > 0) {
    defarr_length = std::min(defarr_length, out_length);
    memcpy(r_values, defarr, sizeof(bool) * defarr_length);
  }
  else {
    defarr_length = 0;
  }

  for (int i = defarr_length; i < out_length; i++) {
    r_values[i] = defvalue;
  }
}

static void rna_property_boolean_fill_default_array_values_from_ints(
    const int *defarr, int defarr_length, bool defvalue, int out_length, bool *r_values)
{
  if (defarr && defarr_length > 0) {
    defarr_length = std::min(defarr_length, out_length);
    for (int i = 0; i < defarr_length; i++) {
      r_values[i] = defarr[i] != 0;
    }
  }
  else {
    defarr_length = 0;
  }

  for (int i = defarr_length; i < out_length; i++) {
    r_values[i] = defvalue;
  }
}

static void rna_property_boolean_get_default_array_values(PointerRNA *ptr,
                                                          BoolPropertyRNA *bprop,
                                                          bool *r_values)
{
  if (ptr->data && bprop->get_default_array) {
    bprop->get_default_array(ptr, &bprop->property, r_values);
    return;
  }

  int length = bprop->property.totarraylength;
  int out_length = RNA_property_array_length(ptr, (PropertyRNA *)bprop);

  rna_property_boolean_fill_default_array_values(
      bprop->defaultarray, length, bprop->defaultvalue, out_length, r_values);
}

static void property_boolean_get_array(PointerRNA *ptr,
                                       PropertyRNAOrID &prop_rna_or_id,
                                       blender::MutableSpan<bool> r_values)
{
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(rna_prop);
  if (prop_rna_or_id.idprop) {
    IDProperty *idprop = prop_rna_or_id.idprop;
    BLI_assert(idprop->len == RNA_property_array_length(ptr, rna_prop) ||
               (rna_prop->flag & PROP_IDPROPERTY));
    if (rna_prop->arraydimension == 0) {
      r_values[0] = RNA_property_boolean_get(ptr, rna_prop);
    }
    else if (idprop->subtype == IDP_INT) {
      /* Some boolean IDProperty arrays might be saved in files as an integer
       * array property, since the boolean IDProperty type was added later. */
      const int *values_src = IDP_array_int_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        r_values[i] = bool(values_src[i]);
      }
    }
    else if (idprop->subtype == IDP_BOOLEAN) {
      int8_t *values_src = IDP_array_bool_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        r_values[i] = values_src[i];
      }
    }
  }
  else if (rna_prop->arraydimension == 0) {
    r_values[0] = RNA_property_boolean_get(ptr, rna_prop);
  }
  else if (bprop->getarray) {
    bprop->getarray(ptr, r_values.data());
  }
  else if (bprop->getarray_ex) {
    bprop->getarray_ex(ptr, rna_prop, r_values.data());
  }
  else {
    rna_property_boolean_get_default_array_values(ptr, bprop, r_values.data());
  }
}

void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, bool *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop_rna_or_id.rnaprop);

  blender::MutableSpan<bool> r_values(values, int64_t(prop_rna_or_id.array_len));
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  property_boolean_get_array(ptr, prop_rna_or_id, r_values);
  if (bprop->getarray_transform) {
    /* NOTE: Given current implementation, it would _probably_ be safe to use `values` for both
     * input 'current values' and output 'final values', since python will make a copy of the input
     * anyways. Think it's better to keep it clean and make a copy here to avoid any potential
     * issues in the future though. */
    blender::Array<bool, RNA_STACK_ARRAY> curr_values(r_values.as_span());
    bprop->getarray_transform(
        ptr, &bprop->property, curr_values.data(), prop_rna_or_id.is_set, r_values.data());
  }
}

void RNA_property_boolean_get_array_at_most(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            bool *values,
                                            int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_boolean_get_array(ptr, prop, values);
    return;
  }

  blender::Array<bool, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_boolean_get_array(ptr, prop, value_buf.data());
  memcpy(values, value_buf.data(), sizeof(*values) * values_num);
}

bool RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  bool tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);
  bool value;

  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_boolean_get_array(ptr, prop, tmp);
    value = tmp[index];
  }
  else {
    bool *tmparray;

    tmparray = MEM_malloc_arrayN<bool>(size_t(len), __func__);
    RNA_property_boolean_get_array(ptr, prop, tmparray);
    value = tmparray[index];
    MEM_freeN(tmparray);
  }

  BLI_assert(ELEM(value, false, true));

  return value;
}

void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const bool *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(rna_prop);

  const int64_t values_num = int64_t(prop_rna_or_id.array_len);
  blender::Span<bool> final_values(values, values_num);
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  /* Default init does not allocate anything, so it's cheap. This is only reinitialized with actual
   * `values_num` items if `setarray_transform` is called. */
  blender::Array<bool, RNA_STACK_ARRAY> final_values_storage{};
  if (bprop->setarray_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    blender::Array<bool, RNA_STACK_ARRAY> curr_values(values_num);
    property_boolean_get_array(ptr, prop_rna_or_id, curr_values.as_mutable_span());

    final_values_storage.reinitialize(values_num);
    bprop->setarray_transform(ptr,
                              rna_prop,
                              final_values.data(),
                              curr_values.data(),
                              prop_rna_or_id.is_set,
                              final_values_storage.data());
    final_values = final_values_storage.as_span();
  }

  if (idprop) {
    /* Support writing to integer and boolean IDProperties, since boolean
     * RNA properties used to be stored with integer IDProperties. */
    if (rna_prop->arraydimension == 0) {
      if (idprop->subtype == IDP_BOOLEAN) {
        IDP_bool_set(idprop, final_values[0]);
      }
      else {
        BLI_assert(idprop->subtype == IDP_INT);
        IDP_int_set(idprop, final_values[0]);
      }
    }
    else {
      BLI_assert(idprop->type = IDP_ARRAY);
      BLI_assert(idprop->len == values_num);
      if (idprop->subtype == IDP_BOOLEAN) {
        memcpy(IDP_array_bool_get(idprop),
               final_values.data(),
               sizeof(decltype(final_values)::value_type) * idprop->len);
      }
      else {
        BLI_assert(idprop->subtype == IDP_INT);
        int *values_dst = IDP_array_int_get(idprop);
        for (int i = 0; i < idprop->len; i++) {
          values_dst[i] = int(final_values[i]);
        }
      }
    }
    rna_idproperty_touch(idprop);
  }
  else if (rna_prop->arraydimension == 0) {
    RNA_property_boolean_set(ptr, rna_prop, final_values[0]);
  }
  else if (bprop->setarray) {
    bprop->setarray(ptr, final_values.data());
  }
  else if (bprop->setarray_ex) {
    bprop->setarray_ex(ptr, rna_prop, final_values.data());
  }
  else if (rna_prop->flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDPropertyTemplate val = {0};
      val.array.len = rna_prop->totarraylength;
#ifdef USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP
      val.array.type = IDP_INT;
#else
      val.array.type = IDP_BOOLEAN;
#endif

      idprop = IDP_New(IDP_ARRAY, &val, prop_rna_or_id.identifier, IDP_FLAG_STATIC_TYPE);
      IDP_AddToGroup(group, idprop);
#ifdef USE_INT_IDPROPS_FOR_BOOLEAN_RNA_PROP
      int *values_dst = IDP_array_int_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        values_dst[i] = int(final_values[i]);
      }
#else
      int8_t *values_dst = IDP_array_bool_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        values_dst[i] = final_values[i];
      }
#endif
    }
  }
}

void RNA_property_boolean_set_array_at_most(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            const bool *values,
                                            int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_boolean_set_array(ptr, prop, values);
    return;
  }

  blender::Array<bool, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_boolean_get_array(ptr, prop, value_buf.data());
  memcpy(value_buf.data(), values, sizeof(*values) * values_num);
  RNA_property_boolean_set_array(ptr, prop, value_buf.data());
}

void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, bool value)
{
  bool tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);
  BLI_assert(ELEM(value, false, true));

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_boolean_get_array(ptr, prop, tmp);
    tmp[index] = value;
    RNA_property_boolean_set_array(ptr, prop, tmp);
  }
  else {
    bool *tmparray;

    tmparray = MEM_malloc_arrayN<bool>(size_t(len), __func__);
    RNA_property_boolean_get_array(ptr, prop, tmparray);
    tmparray[index] = value;
    RNA_property_boolean_set_array(ptr, prop, tmparray);
    MEM_freeN(tmparray);
  }
}

bool RNA_property_boolean_get_default(PointerRNA *ptr, PropertyRNA *prop)
{
  /* TODO: Make defaults work for IDProperties. */
  BoolPropertyRNA *bprop = (BoolPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
  BLI_assert(RNA_property_array_check(prop) == false);
  BLI_assert(ELEM(bprop->defaultvalue, false, true));

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      switch (IDP_ui_data_type(idprop)) {
        case IDP_UI_DATA_TYPE_BOOLEAN: {
          const IDPropertyUIDataBool *ui_data = (const IDPropertyUIDataBool *)idprop->ui_data;
          return ui_data->default_value;
        }
        case IDP_UI_DATA_TYPE_INT: {
          const IDPropertyUIDataInt *ui_data = (const IDPropertyUIDataInt *)idprop->ui_data;
          return ui_data->default_value != 0;
        }
        default:
          BLI_assert_unreachable();
      }
    }
    return false;
  }
  if (bprop->get_default) {
    return bprop->get_default(ptr, prop);
  }

  return bprop->defaultvalue;
}

void RNA_property_boolean_get_default_array(PointerRNA *ptr, PropertyRNA *prop, bool *values)
{
  BoolPropertyRNA *bprop = (BoolPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
  BLI_assert(RNA_property_array_check(prop) != false);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_ARRAY);
      switch (IDP_ui_data_type(idprop)) {
        case IDP_UI_DATA_TYPE_BOOLEAN: {
          const IDPropertyUIDataBool *ui_data = (const IDPropertyUIDataBool *)idprop->ui_data;
          if (ui_data->default_array) {
            rna_property_boolean_fill_default_array_values((const bool *)ui_data->default_array,
                                                           ui_data->default_array_len,
                                                           ui_data->default_value,
                                                           idprop->len,
                                                           values);
          }
          else {
            rna_property_boolean_fill_default_array_values(
                nullptr, 0, ui_data->default_value, idprop->len, values);
          }
          break;
        }
        case IDP_UI_DATA_TYPE_INT: {
          const IDPropertyUIDataInt *ui_data = (const IDPropertyUIDataInt *)idprop->ui_data;
          if (ui_data->default_array) {
            rna_property_boolean_fill_default_array_values_from_ints(ui_data->default_array,
                                                                     ui_data->default_array_len,
                                                                     ui_data->default_value,
                                                                     idprop->len,
                                                                     values);
          }
          else {
            rna_property_boolean_fill_default_array_values(
                nullptr, 0, ui_data->default_value, idprop->len, values);
          }
          break;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  }
  else if (prop->arraydimension == 0) {
    values[0] = bprop->defaultvalue;
  }
  else {
    rna_property_boolean_get_default_array_values(ptr, bprop, values);
  }
}

bool RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  bool tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_BOOLEAN);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_boolean_get_default_array(ptr, prop, tmp);
    return tmp[index];
  }
  bool *tmparray, value;

  tmparray = MEM_malloc_arrayN<bool>(size_t(len), __func__);
  RNA_property_boolean_get_default_array(ptr, prop, tmparray);
  value = tmparray[index];
  MEM_freeN(tmparray);

  return value;
}

static int property_int_get(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
    return IDP_int_get(prop_rna_or_id.idprop);
  }
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (iprop->get) {
    return iprop->get(ptr);
  }
  if (iprop->get_ex) {
    return iprop->get_ex(ptr, &iprop->property);
  }
  return iprop->defaultvalue;
}

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_INT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop_rna_or_id.rnaprop);

  int value = property_int_get(ptr, prop_rna_or_id);
  if (iprop->get_transform) {
    value = iprop->get_transform(ptr, &iprop->property, value, prop_rna_or_id.is_set);
  }

  return value;
}

void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value)
{
  BLI_assert(RNA_property_type(prop) == PROP_INT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop_rna_or_id.rnaprop);

  if (iprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const int curr_value = property_int_get(ptr, prop_rna_or_id);
    value = iprop->set_transform(ptr, &iprop->property, value, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
    RNA_property_int_clamp(ptr, &iprop->property, &value);
    IDP_int_set(idprop, value);
    rna_idproperty_touch(idprop);
  }
  else if (iprop->set) {
    iprop->set(ptr, value);
  }
  else if (iprop->set_ex) {
    iprop->set_ex(ptr, &iprop->property, value);
  }
  else if (iprop->property.flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      RNA_property_int_clamp(ptr, &iprop->property, &value);
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create(prop_rna_or_id.identifier, value, IDP_FLAG_STATIC_TYPE)
              .release());
    }
  }
}

static void rna_property_int_fill_default_array_values(
    const int *defarr, int defarr_length, int defvalue, int out_length, int *r_values)
{
  if (defarr && defarr_length > 0) {
    defarr_length = std::min(defarr_length, out_length);
    memcpy(r_values, defarr, sizeof(int) * defarr_length);
  }
  else {
    defarr_length = 0;
  }

  for (int i = defarr_length; i < out_length; i++) {
    r_values[i] = defvalue;
  }
}

static void rna_property_int_get_default_array_values(PointerRNA *ptr,
                                                      IntPropertyRNA *iprop,
                                                      int *r_values)
{
  if (ptr->data && iprop->get_default_array) {
    iprop->get_default_array(ptr, &iprop->property, r_values);
    return;
  }

  int length = iprop->property.totarraylength;
  int out_length = RNA_property_array_length(ptr, (PropertyRNA *)iprop);

  rna_property_int_fill_default_array_values(
      iprop->defaultarray, length, iprop->defaultvalue, out_length, r_values);
}

static void property_int_get_array(PointerRNA *ptr,
                                   PropertyRNAOrID &prop_rna_or_id,
                                   blender::MutableSpan<int> r_values)
{
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(rna_prop);
  if (prop_rna_or_id.idprop) {
    IDProperty *idprop = prop_rna_or_id.idprop;
    BLI_assert(idprop->len == RNA_property_array_length(ptr, rna_prop) ||
               (rna_prop->flag & PROP_IDPROPERTY));
    if (rna_prop->arraydimension == 0) {
      r_values[0] = RNA_property_int_get(ptr, rna_prop);
    }
    else {
      memcpy(r_values.data(),
             IDP_array_int_get(idprop),
             sizeof(decltype(r_values)::value_type) * idprop->len);
    }
  }
  else if (rna_prop->arraydimension == 0) {
    r_values[0] = RNA_property_int_get(ptr, rna_prop);
  }
  else if (iprop->getarray) {
    iprop->getarray(ptr, r_values.data());
  }
  else if (iprop->getarray_ex) {
    iprop->getarray_ex(ptr, rna_prop, r_values.data());
  }
  else {
    rna_property_int_get_default_array_values(ptr, iprop, r_values.data());
  }
}

void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_INT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop_rna_or_id.rnaprop);

  blender::MutableSpan<int> r_values(values, int64_t(prop_rna_or_id.array_len));
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  property_int_get_array(ptr, prop_rna_or_id, r_values);
  if (iprop->getarray_transform) {
    /* NOTE: Given current implementation, it would _probably_ be safe to use `values` for both
     * input 'current values' and output 'final values', since python will make a copy of the input
     * anyways. Think it's better to keep it clean and make a copy here to avoid any potential
     * issues in the future though. */
    blender::Array<int, RNA_STACK_ARRAY> curr_values(r_values.as_span());
    iprop->getarray_transform(
        ptr, &iprop->property, curr_values.data(), prop_rna_or_id.is_set, r_values.data());
  }
}

void RNA_property_int_get_array_at_most(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        int *values,
                                        int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_int_get_array(ptr, prop, values);
    return;
  }

  blender::Array<int, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_int_get_array(ptr, prop, value_buf.data());
  memcpy(values, value_buf.data(), sizeof(*values) * values_num);
}

void RNA_property_int_get_array_range(PointerRNA *ptr, PropertyRNA *prop, int values[2])
{
  const int array_len = RNA_property_array_length(ptr, prop);

  if (array_len <= 0) {
    values[0] = 0;
    values[1] = 0;
  }
  else if (array_len == 1) {
    RNA_property_int_get_array(ptr, prop, values);
    values[1] = values[0];
  }
  else {
    int arr_stack[32];
    int *arr;
    int i;

    if (array_len > 32) {
      arr = MEM_malloc_arrayN<int>(size_t(array_len), __func__);
    }
    else {
      arr = arr_stack;
    }

    RNA_property_int_get_array(ptr, prop, arr);
    values[0] = values[1] = arr[0];
    for (i = 1; i < array_len; i++) {
      values[0] = std::min(values[0], arr[i]);
      values[1] = std::max(values[1], arr[i]);
    }

    if (arr != arr_stack) {
      MEM_freeN(arr);
    }
  }
}

int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  int tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_INT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_int_get_array(ptr, prop, tmp);
    return tmp[index];
  }
  int *tmparray, value;

  tmparray = MEM_malloc_arrayN<int>(size_t(len), __func__);
  RNA_property_int_get_array(ptr, prop, tmparray);
  value = tmparray[index];
  MEM_freeN(tmparray);

  return value;
}

void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_INT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(rna_prop);

  const int64_t values_num = int64_t(prop_rna_or_id.array_len);
  blender::Span<int> final_values(values, values_num);
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  /* Default init does not allocate anything, so it's cheap. This is only reinitialized with actual
   * `values_num` items if `setarray_transform` is called. */
  blender::Array<int, RNA_STACK_ARRAY> final_values_storage{};
  if (iprop->setarray_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    blender::Array<int, RNA_STACK_ARRAY> curr_values(values_num);
    property_int_get_array(ptr, prop_rna_or_id, curr_values.as_mutable_span());

    final_values_storage.reinitialize(values_num);
    iprop->setarray_transform(ptr,
                              rna_prop,
                              final_values.data(),
                              curr_values.data(),
                              prop_rna_or_id.is_set,
                              final_values_storage.data());
    final_values = final_values_storage.as_span();
  }

  if (idprop) {
    BLI_assert(idprop->len == values_num);
    if (rna_prop->arraydimension == 0) {
      IDP_int_set(idprop, final_values[0]);
    }
    else {
      memcpy(IDP_array_int_get(idprop),
             final_values.data(),
             sizeof(decltype(final_values)::value_type) * idprop->len);
    }

    rna_idproperty_touch(idprop);
  }
  else if (rna_prop->arraydimension == 0) {
    RNA_property_int_set(ptr, rna_prop, final_values[0]);
  }
  else if (iprop->setarray) {
    iprop->setarray(ptr, final_values.data());
  }
  else if (iprop->setarray_ex) {
    iprop->setarray_ex(ptr, rna_prop, final_values.data());
  }
  else if (rna_prop->flag & PROP_EDITABLE) {
    // RNA_property_int_clamp_array(ptr, prop, &value); /* TODO. */
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_AddToGroup(group,
                     blender::bke::idprop::create(
                         prop_rna_or_id.identifier, final_values, IDP_FLAG_STATIC_TYPE)
                         .release());
    }
  }
}

void RNA_property_int_set_array_at_most(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        const int *values,
                                        int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_int_set_array(ptr, prop, values);
    return;
  }

  blender::Array<int, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_int_get_array(ptr, prop, value_buf.data());
  memcpy(value_buf.data(), values, sizeof(*values) * values_num);
  RNA_property_int_set_array(ptr, prop, value_buf.data());
}

void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value)
{
  int tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_INT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_int_get_array(ptr, prop, tmp);
    tmp[index] = value;
    RNA_property_int_set_array(ptr, prop, tmp);
  }
  else {
    int *tmparray;

    tmparray = MEM_malloc_arrayN<int>(size_t(len), __func__);
    RNA_property_int_get_array(ptr, prop, tmparray);
    tmparray[index] = value;
    RNA_property_int_set_array(ptr, prop, tmparray);
    MEM_freeN(tmparray);
  }
}

int RNA_property_int_get_default(PointerRNA *ptr, PropertyRNA *prop)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      const IDPropertyUIDataInt *ui_data = (const IDPropertyUIDataInt *)idprop->ui_data;
      return ui_data->default_value;
    }
  }
  if (iprop->get_default) {
    return iprop->get_default(ptr, prop);
  }

  return iprop->defaultvalue;
}

bool RNA_property_int_set_default(PropertyRNA *prop, int value)
{
  if (prop->magic == RNA_MAGIC) {
    return false;
  }

  IDProperty *idprop = (IDProperty *)prop;
  BLI_assert(idprop->type == IDP_INT);

  IDPropertyUIDataInt *ui_data = (IDPropertyUIDataInt *)IDP_ui_data_ensure(idprop);
  ui_data->default_value = value;
  return true;
}

void RNA_property_int_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_INT);
  BLI_assert(RNA_property_array_check(prop) != false);

  if (prop->magic != RNA_MAGIC) {
    int length = rna_ensure_property_array_length(ptr, prop);

    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_ARRAY);
      BLI_assert(idprop->subtype == IDP_INT);
      const IDPropertyUIDataInt *ui_data = (const IDPropertyUIDataInt *)idprop->ui_data;
      if (ui_data->default_array) {
        rna_property_int_fill_default_array_values(ui_data->default_array,
                                                   ui_data->default_array_len,
                                                   ui_data->default_value,
                                                   length,
                                                   values);
      }
      else {
        rna_property_int_fill_default_array_values(
            nullptr, 0, ui_data->default_value, length, values);
      }
    }
  }
  else if (prop->arraydimension == 0) {
    values[0] = iprop->defaultvalue;
  }
  else {
    rna_property_int_get_default_array_values(ptr, iprop, values);
  }
}

int RNA_property_int_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  int tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_INT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_int_get_default_array(ptr, prop, tmp);
    return tmp[index];
  }
  int *tmparray, value;

  tmparray = MEM_malloc_arrayN<int>(size_t(len), __func__);
  RNA_property_int_get_default_array(ptr, prop, tmparray);
  value = tmparray[index];
  MEM_freeN(tmparray);

  return value;
}

static float property_float_get(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
    IDProperty *idprop = prop_rna_or_id.idprop;
    if (idprop->type == IDP_FLOAT) {
      return IDP_float_get(idprop);
    }
    return float(IDP_double_get(idprop));
  }
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (fprop->get) {
    return fprop->get(ptr);
  }
  if (fprop->get_ex) {
    return fprop->get_ex(ptr, &fprop->property);
  }
  return fprop->defaultvalue;
}

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop_rna_or_id.rnaprop);

  float value = property_float_get(ptr, prop_rna_or_id);
  if (fprop->get_transform) {
    value = fprop->get_transform(ptr, &fprop->property, value, prop_rna_or_id.is_set);
  }

  return value;
}

void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value)
{
  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop_rna_or_id.rnaprop);

  if (fprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const float curr_value = property_float_get(ptr, prop_rna_or_id);
    value = fprop->set_transform(ptr, &fprop->property, value, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
    RNA_property_float_clamp(ptr, &fprop->property, &value);
    if (idprop->type == IDP_FLOAT) {
      IDP_float_set(idprop, value);
    }
    else {
      IDP_double_set(idprop, double(value));
    }
    rna_idproperty_touch(idprop);
  }
  else if (fprop->set) {
    fprop->set(ptr, value);
  }
  else if (fprop->set_ex) {
    fprop->set_ex(ptr, &fprop->property, value);
  }
  else if (fprop->property.flag & PROP_EDITABLE) {
    RNA_property_float_clamp(ptr, &fprop->property, &value);
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create(prop_rna_or_id.identifier, value, IDP_FLAG_STATIC_TYPE)
              .release());
    }
  }
}

static void rna_property_float_fill_default_array_values(
    const float *defarr, int defarr_length, float defvalue, int out_length, float *r_values)
{
  if (defarr && defarr_length > 0) {
    defarr_length = std::min(defarr_length, out_length);
    memcpy(r_values, defarr, sizeof(float) * defarr_length);
  }
  else {
    defarr_length = 0;
  }

  for (int i = defarr_length; i < out_length; i++) {
    r_values[i] = defvalue;
  }
}

/**
 * The same logic as #rna_property_float_fill_default_array_values for a double array.
 */
static void rna_property_float_fill_default_array_values_double(const double *default_array,
                                                                const int default_array_len,
                                                                const double default_value,
                                                                const int out_length,
                                                                float *r_values)
{
  const int array_copy_len = std::min(out_length, default_array_len);

  for (int i = 0; i < array_copy_len; i++) {
    r_values[i] = float(default_array[i]);
  }

  for (int i = array_copy_len; i < out_length; i++) {
    r_values[i] = float(default_value);
  }
}

static void rna_property_float_get_default_array_values(PointerRNA *ptr,
                                                        FloatPropertyRNA *fprop,
                                                        float *r_values)
{
  if (ptr->data && fprop->get_default_array) {
    fprop->get_default_array(ptr, &fprop->property, r_values);
    return;
  }

  int length = fprop->property.totarraylength;
  int out_length = RNA_property_array_length(ptr, &fprop->property);

  rna_property_float_fill_default_array_values(
      fprop->defaultarray, length, fprop->defaultvalue, out_length, r_values);
}

static void property_float_get_array(PointerRNA *ptr,
                                     PropertyRNAOrID &prop_rna_or_id,
                                     blender::MutableSpan<float> r_values)
{
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(rna_prop);
  if (prop_rna_or_id.idprop) {
    IDProperty *idprop = prop_rna_or_id.idprop;
    BLI_assert(idprop->len == RNA_property_array_length(ptr, rna_prop) ||
               (rna_prop->flag & PROP_IDPROPERTY));
    if (rna_prop->arraydimension == 0) {
      r_values[0] = RNA_property_float_get(ptr, rna_prop);
    }
    else if (idprop->subtype == IDP_FLOAT) {
      memcpy(r_values.data(),
             IDP_array_float_get(idprop),
             sizeof(decltype(r_values)::value_type) * idprop->len);
    }
    else {
      double *src_values = IDP_array_double_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        r_values[i] = float(src_values[i]);
      }
    }
  }
  else if (rna_prop->arraydimension == 0) {
    r_values[0] = RNA_property_float_get(ptr, rna_prop);
  }
  else if (fprop->getarray) {
    fprop->getarray(ptr, r_values.data());
  }
  else if (fprop->getarray_ex) {
    fprop->getarray_ex(ptr, rna_prop, r_values.data());
  }
  else {
    rna_property_float_get_default_array_values(ptr, fprop, r_values.data());
  }
}

void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) != false);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop_rna_or_id.rnaprop);

  blender::MutableSpan<float> r_values(values, int64_t(prop_rna_or_id.array_len));
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  property_float_get_array(ptr, prop_rna_or_id, r_values);
  if (fprop->getarray_transform) {
    /* NOTE: Given current implementation, it would _probably_ be safe to use `values` for both
     * input 'current values' and output 'final values', since python will make a copy of the input
     * anyways. Think it's better to keep it clean and make a copy here to avoid any potential
     * issues in the future though. */
    blender::Array<float, RNA_STACK_ARRAY> curr_values(r_values.as_span());
    fprop->getarray_transform(
        ptr, &fprop->property, curr_values.data(), prop_rna_or_id.is_set, r_values.data());
  }
}

void RNA_property_float_get_array_at_most(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          float *values,
                                          int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_float_get_array(ptr, prop, values);
    return;
  }

  blender::Array<float, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_float_get_array(ptr, prop, value_buf.data());
  memcpy(values, value_buf.data(), sizeof(*values) * values_num);
}

void RNA_property_float_get_array_range(PointerRNA *ptr, PropertyRNA *prop, float values[2])
{
  const int array_len = RNA_property_array_length(ptr, prop);

  if (array_len <= 0) {
    values[0] = 0.0f;
    values[1] = 0.0f;
  }
  else if (array_len == 1) {
    RNA_property_float_get_array(ptr, prop, values);
    values[1] = values[0];
  }
  else {
    float arr_stack[32];
    float *arr;
    int i;

    if (array_len > 32) {
      arr = MEM_malloc_arrayN<float>(size_t(array_len), __func__);
    }
    else {
      arr = arr_stack;
    }

    RNA_property_float_get_array(ptr, prop, arr);
    values[0] = values[1] = arr[0];
    for (i = 1; i < array_len; i++) {
      values[0] = std::min(values[0], arr[i]);
      values[1] = std::max(values[1], arr[i]);
    }

    if (arr != arr_stack) {
      MEM_freeN(arr);
    }
  }
}

float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  float tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_float_get_array(ptr, prop, tmp);
    return tmp[index];
  }
  float *tmparray, value;

  tmparray = MEM_malloc_arrayN<float>(size_t(len), __func__);
  RNA_property_float_get_array(ptr, prop, tmparray);
  value = tmparray[index];
  MEM_freeN(tmparray);

  return value;
}

void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values)
{
  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  PropertyRNA *rna_prop = prop_rna_or_id.rnaprop;
  FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(rna_prop);

  const int64_t values_num = int64_t(prop_rna_or_id.array_len);
  blender::Span<float> final_values(values, values_num);
  values = nullptr; /* Do not access this 'raw' pointer anymore in code below. */
  /* Default init does not allocate anything, so it's cheap. This is only reinitialized with actual
   * `values_num` items if `setarray_transform` is called. */
  blender::Array<float, RNA_STACK_ARRAY> final_values_storage{};
  if (fprop->setarray_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    blender::Array<float, RNA_STACK_ARRAY> curr_values(values_num);
    property_float_get_array(ptr, prop_rna_or_id, curr_values.as_mutable_span());

    final_values_storage.reinitialize(values_num);
    fprop->setarray_transform(ptr,
                              rna_prop,
                              final_values.data(),
                              curr_values.data(),
                              prop_rna_or_id.is_set,
                              final_values_storage.data());
    final_values = final_values_storage.as_span();
  }

  if (idprop) {
    BLI_assert(idprop->len == values_num);
    if (rna_prop->arraydimension == 0) {
      if (idprop->type == IDP_FLOAT) {
        IDP_float_set(idprop, final_values[0]);
      }
      else {
        IDP_double_set(idprop, double(final_values[0]));
      }
    }
    else if (idprop->subtype == IDP_FLOAT) {
      memcpy(IDP_array_float_get(idprop),
             final_values.data(),
             sizeof(decltype(final_values)::value_type) * idprop->len);
    }
    else {
      double *dst_values = IDP_array_double_get(idprop);
      for (int i = 0; i < idprop->len; i++) {
        dst_values[i] = double(final_values[i]);
      }
    }

    rna_idproperty_touch(idprop);
  }
  else if (rna_prop->arraydimension == 0) {
    RNA_property_float_set(ptr, rna_prop, final_values[0]);
  }
  else if (fprop->setarray) {
    fprop->setarray(ptr, final_values.data());
  }
  else if (fprop->setarray_ex) {
    fprop->setarray_ex(ptr, rna_prop, final_values.data());
  }
  else if (rna_prop->flag & PROP_EDITABLE) {
    // RNA_property_float_clamp_array(ptr, prop, &value); /* TODO. */
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_AddToGroup(group,
                     blender::bke::idprop::create(
                         prop_rna_or_id.identifier, final_values, IDP_FLAG_STATIC_TYPE)
                         .release());
    }
  }
}

void RNA_property_float_set_array_at_most(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          const float *values,
                                          int values_num)
{
  BLI_assert(values_num >= 0);
  const int array_num = RNA_property_array_length(ptr, prop);
  if (values_num >= array_num) {
    RNA_property_float_set_array(ptr, prop, values);
    return;
  }

  blender::Array<float, RNA_STACK_ARRAY> value_buf(array_num);
  RNA_property_float_get_array(ptr, prop, value_buf.data());
  memcpy(value_buf.data(), values, sizeof(*values) * values_num);
  RNA_property_float_set_array(ptr, prop, value_buf.data());
}

void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value)
{
  float tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_float_get_array(ptr, prop, tmp);
    tmp[index] = value;
    RNA_property_float_set_array(ptr, prop, tmp);
  }
  else {
    float *tmparray;

    tmparray = MEM_malloc_arrayN<float>(size_t(len), __func__);
    RNA_property_float_get_array(ptr, prop, tmparray);
    tmparray[index] = value;
    RNA_property_float_set_array(ptr, prop, tmparray);
    MEM_freeN(tmparray);
  }
}

float RNA_property_float_get_default(PointerRNA *ptr, PropertyRNA *prop)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) == false);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(ELEM(idprop->type, IDP_FLOAT, IDP_DOUBLE));
      const IDPropertyUIDataFloat *ui_data = (const IDPropertyUIDataFloat *)idprop->ui_data;
      return float(ui_data->default_value);
    }
  }
  if (fprop->get_default) {
    return fprop->get_default(ptr, prop);
  }

  return fprop->defaultvalue;
}

bool RNA_property_float_set_default(PropertyRNA *prop, float value)
{
  if (prop->magic == RNA_MAGIC) {
    return false;
  }

  IDProperty *idprop = (IDProperty *)prop;
  BLI_assert(idprop->type == IDP_FLOAT);

  IDPropertyUIDataFloat *ui_data = (IDPropertyUIDataFloat *)IDP_ui_data_ensure(idprop);
  ui_data->default_value = double(value);
  return true;
}

void RNA_property_float_get_default_array(PointerRNA *ptr, PropertyRNA *prop, float *values)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) != false);

  if (prop->magic != RNA_MAGIC) {
    int length = rna_ensure_property_array_length(ptr, prop);

    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_ARRAY);
      BLI_assert(ELEM(idprop->subtype, IDP_FLOAT, IDP_DOUBLE));
      const IDPropertyUIDataFloat *ui_data = (const IDPropertyUIDataFloat *)idprop->ui_data;
      rna_property_float_fill_default_array_values_double(ui_data->default_array,
                                                          ui_data->default_array_len,
                                                          ui_data->default_value,
                                                          length,
                                                          values);
    }
  }
  else if (prop->arraydimension == 0) {
    values[0] = fprop->defaultvalue;
  }
  else {
    rna_property_float_get_default_array_values(ptr, fprop, values);
  }
}

float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  float tmp[RNA_MAX_ARRAY_LENGTH];
  int len = rna_ensure_property_array_length(ptr, prop);

  BLI_assert(RNA_property_type(prop) == PROP_FLOAT);
  BLI_assert(RNA_property_array_check(prop) != false);
  BLI_assert(index >= 0);
  BLI_assert(index < len);

  if (len <= RNA_MAX_ARRAY_LENGTH) {
    RNA_property_float_get_default_array(ptr, prop, tmp);
    return tmp[index];
  }
  float *tmparray, value;

  tmparray = MEM_malloc_arrayN<float>(size_t(len), __func__);
  RNA_property_float_get_default_array(ptr, prop, tmparray);
  value = tmparray[index];
  MEM_freeN(tmparray);

  return value;
}

static size_t property_string_length_storage(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
    IDProperty *idprop = prop_rna_or_id.idprop;
    if (idprop->subtype == IDP_STRING_SUB_BYTE) {
      return size_t(idprop->len);
    }
    /* these _must_ stay in sync */
    if (strlen(IDP_string_get(idprop)) != idprop->len - 1) {
      printf("%zu vs. %d\n", strlen(IDP_string_get(idprop)), idprop->len - 1);
    }
    BLI_assert(strlen(IDP_string_get(idprop)) == idprop->len - 1);
    return size_t(idprop->len - 1);
  }

  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (sprop->length) {
    return sprop->length(ptr);
  }
  if (sprop->length_ex) {
    return size_t(sprop->length_ex(ptr, &sprop->property));
  }
  return strlen(sprop->defaultvalue);
}

static std::string property_string_get(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
    const size_t length = property_string_length_storage(ptr, prop_rna_or_id);
    return std::string{IDP_string_get(prop_rna_or_id.idprop), length};
  }
  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (sprop->get) {
    const size_t length = property_string_length_storage(ptr, prop_rna_or_id);
    /* Note: after `resize()` the underlying buffer is actually at least
     * `length + 1` bytes long, because (since C++11) `std::string` guarantees
     * a terminating null byte, but that is not considered part of the length. */
    std::string string_ret;
    string_ret.resize(length);

    sprop->get(ptr, string_ret.data());
    return string_ret;
  }
  if (sprop->get_ex) {
    return sprop->get_ex(ptr, &sprop->property);
  }
  return sprop->defaultvalue;
}

std::string RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);

  std::string string_ret = property_string_get(ptr, prop_rna_or_id);
  if (sprop->get_transform) {
    string_ret = sprop->get_transform(ptr, &sprop->property, string_ret, prop_rna_or_id.is_set);
  }

  BLI_assert_msg(!sprop->maxlength || string_ret.size() < sprop->maxlength,
                 "Returned string exceeds the property's max length");

  return string_ret;
}

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value)
{
  const std::string string_ret = RNA_property_string_get(ptr, prop);

  memcpy(value, string_ret.c_str(), string_ret.size() + 1);
}

char *RNA_property_string_get_alloc(
    PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen, int *r_len)
{
  if (fixedbuf) {
    BLI_string_debug_size(fixedbuf, fixedlen);
  }

  const std::string string_ret = RNA_property_string_get(ptr, prop);

  char *buf;
  if (string_ret.size() < fixedlen) {
    buf = fixedbuf;
  }
  else {
    buf = MEM_malloc_arrayN<char>(string_ret.size() + 1, __func__);
  }

  memcpy(buf, string_ret.c_str(), string_ret.size() + 1);
  if (r_len) {
    *r_len = int(string_ret.size());
  }

  return buf;
}

int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  prop = nullptr;
  /* NOTE: `prop` is kept unchanged, to allow e.g. call to `RNA_property_string_get` without
   * further complications.
   * `sprop->property` should be used when access to an actual RNA property is required.
   */
  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);

  /* If there is a `get_transform` callback, no choice but get that final string to find out its
   * length. Otherwise, get the 'storage length', which is typically more efficient to compute. */
  if (sprop->get_transform) {
    std::string string_final = property_string_get(ptr, prop_rna_or_id);
    return int(string_final.size());
  }
  return int(property_string_length_storage(ptr, prop_rna_or_id));
}

void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value)
{
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);

  /* Value can be nullptr, see #145562. */
  /* FIXME: unclear if this function is supposed to accept nullptr values? */
  std::string value_set = value ? value : "";
  if (sprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const std::string curr_value = property_string_get(ptr, prop_rna_or_id);
    value_set = sprop->set_transform(
        ptr, &sprop->property, value_set, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
    /* both IDP_STRING_SUB_BYTE / IDP_STRING_SUB_UTF8 */
    IDP_AssignStringMaxSize(
        idprop, value_set.c_str(), RNA_property_string_maxlength(&sprop->property));
    rna_idproperty_touch(idprop);
  }
  else if (sprop->set) {
    sprop->set(ptr, value_set.c_str()); /* set function needs to clamp itself */
  }
  else if (sprop->set_ex) {
    sprop->set_ex(ptr, &sprop->property, value_set); /* set function needs to clamp itself */
  }
  else if (sprop->property.flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_AddToGroup(group,
                     IDP_NewStringMaxSize(value_set.c_str(),
                                          RNA_property_string_maxlength(&sprop->property),
                                          prop_rna_or_id.identifier,
                                          IDP_FLAG_STATIC_TYPE));
    }
  }
}

void RNA_property_string_set_bytes(PointerRNA *ptr, PropertyRNA *prop, const char *value, int len)
{
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  BLI_assert(RNA_property_subtype(prop) == PROP_BYTESTRING);
  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop_rna_or_id.rnaprop);

  std::string value_set = {value, size_t(len)};
  if (sprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const std::string curr_value = property_string_get(ptr, prop_rna_or_id);
    value_set = sprop->set_transform(
        ptr, &sprop->property, value_set, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
    IDP_ResizeArray(idprop, value_set.size() + 1);
    memcpy(idprop->data.pointer, value, value_set.size() + 1);
    rna_idproperty_touch(idprop);
  }
  else if (sprop->set) {
    sprop->set(ptr, value_set.c_str()); /* set function needs to clamp itself */
  }
  else if (sprop->set_ex) {
    sprop->set_ex(ptr, &sprop->property, value_set); /* set function needs to clamp itself */
  }
  else if (sprop->property.flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDPropertyTemplate val = {0};
      val.string.str = value_set.c_str();
      val.string.len = value_set.size() + 1;
      val.string.subtype = IDP_STRING_SUB_BYTE;
      IDP_AddToGroup(group,
                     IDP_New(IDP_STRING, &val, prop_rna_or_id.identifier, IDP_FLAG_STATIC_TYPE));
    }
  }
}

void RNA_property_string_get_default(PropertyRNA *prop, char *value, const int value_maxncpy)
{
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_STRING);
      const IDPropertyUIDataString *ui_data = (const IDPropertyUIDataString *)idprop->ui_data;
      BLI_strncpy(value, ui_data->default_value, value_maxncpy);
      return;
    }

    strcpy(value, "");
    return;
  }

  BLI_assert(RNA_property_type(prop) == PROP_STRING);

  strcpy(value, sprop->defaultvalue);
}

char *RNA_property_string_get_default_alloc(
    PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen, int *r_len)
{
  char *buf;
  int length;

  BLI_assert(RNA_property_type(prop) == PROP_STRING);

  length = RNA_property_string_default_length(ptr, prop);

  if (length + 1 < fixedlen) {
    buf = fixedbuf;
  }
  else {
    buf = MEM_calloc_arrayN<char>(size_t(length) + 1, __func__);
  }

  RNA_property_string_get_default(prop, buf, length + 1);

  if (r_len) {
    *r_len = length;
  }

  return buf;
}

int RNA_property_string_default_length(PointerRNA * /*ptr*/, PropertyRNA *prop)
{
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = (const IDProperty *)prop;
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_STRING);
      const IDPropertyUIDataString *ui_data = (const IDPropertyUIDataString *)idprop->ui_data;
      if (ui_data->default_value != nullptr) {
        return strlen(ui_data->default_value);
      }
    }

    return 0;
  }

  BLI_assert(RNA_property_type(prop) == PROP_STRING);

  return strlen(sprop->defaultvalue);
}

eStringPropertySearchFlag RNA_property_string_search_flag(PropertyRNA *prop)
{
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);
  if (prop->magic != RNA_MAGIC) {
    return eStringPropertySearchFlag(0);
  }
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  if (sprop->search) {
    BLI_assert(sprop->search_flag & PROP_STRING_SEARCH_SUPPORTED);
  }
  else {
    BLI_assert(sprop->search_flag == 0);
  }
  return sprop->search_flag;
}

void RNA_property_string_search(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *edit_text,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  BLI_assert(RNA_property_string_search_flag(prop) & PROP_STRING_SEARCH_SUPPORTED);
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_ensure_property(prop);
  sprop->search(C, ptr, prop, edit_text, visit_fn);
}

std::optional<std::string> RNA_property_string_path_filter(const bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_STRING);
  PropertyRNA *rna_prop = rna_ensure_property(prop);
  StringPropertyRNA *sprop = (StringPropertyRNA *)rna_prop;
  if (!sprop->path_filter) {
    return std::nullopt;
  }
  return sprop->path_filter(C, ptr, rna_prop);
}

static int property_enum_get(PointerRNA *ptr, PropertyRNAOrID &prop_rna_or_id)
{
  if (prop_rna_or_id.idprop) {
    return IDP_int_get(prop_rna_or_id.idprop);
  }
  EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop_rna_or_id.rnaprop);
  if (eprop->get) {
    return eprop->get(ptr);
  }
  if (eprop->get_ex) {
    return eprop->get_ex(ptr, &eprop->property);
  }
  return eprop->defaultvalue;
}

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_ENUM);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop_rna_or_id.rnaprop);

  int value = property_enum_get(ptr, prop_rna_or_id);
  if (eprop->get_transform) {
    value = eprop->get_transform(ptr, &eprop->property, value, prop_rna_or_id.is_set);
  }

  return value;
}

void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value)
{
  BLI_assert(RNA_property_type(prop) == PROP_ENUM);

  PropertyRNAOrID prop_rna_or_id;
  rna_property_rna_or_id_get(prop, ptr, &prop_rna_or_id);
  BLI_assert(!prop_rna_or_id.is_array);
  /* Make initial `prop` pointer invalid, to ensure that it is not used anywhere below. */
  prop = nullptr;
  IDProperty *idprop = prop_rna_or_id.idprop;
  EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop_rna_or_id.rnaprop);

  if (eprop->set_transform) {
    /* Get raw, untransformed (aka 'storage') value. */
    const int curr_value = property_enum_get(ptr, prop_rna_or_id);
    value = eprop->set_transform(ptr, &eprop->property, value, curr_value, prop_rna_or_id.is_set);
  }

  if (idprop) {
    IDP_int_set(idprop, value);
    rna_idproperty_touch(idprop);
  }
  else if (eprop->set) {
    eprop->set(ptr, value);
  }
  else if (eprop->set_ex) {
    eprop->set_ex(ptr, &eprop->property, value);
  }
  else if (eprop->property.flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create(prop_rna_or_id.identifier, value, IDP_FLAG_STATIC_TYPE)
              .release());
    }
  }
}

int RNA_property_enum_get_default(PointerRNA *ptr, PropertyRNA *prop)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)rna_ensure_property(prop);
  BLI_assert(RNA_property_type(prop) == PROP_ENUM);

  if (prop->magic != RNA_MAGIC) {
    const IDProperty *idprop = reinterpret_cast<const IDProperty *>(prop);
    if (idprop->ui_data) {
      BLI_assert(idprop->type == IDP_INT);
      const IDPropertyUIDataInt *ui_data = reinterpret_cast<const IDPropertyUIDataInt *>(
          idprop->ui_data);
      return ui_data->default_value;
    }
  }
  if (eprop->get_default) {
    return eprop->get_default(ptr, prop);
  }

  return eprop->defaultvalue;
}

int RNA_property_enum_step(
    const bContext *C, PointerRNA *ptr, PropertyRNA *prop, int from_value, int step)
{
  const EnumPropertyItem *item_array;
  int totitem;
  bool free;
  int result_value = from_value;
  int i, i_init;
  int single_step = (step < 0) ? -1 : 1;
  int step_tot = 0;

  RNA_property_enum_items((bContext *)C, ptr, prop, &item_array, &totitem, &free);
  i = RNA_enum_from_value(item_array, from_value);
  i_init = i;

  do {
    i = mod_i(i + single_step, totitem);
    if (item_array[i].identifier[0]) {
      step_tot += single_step;
    }
  } while ((i != i_init) && (step_tot != step));

  if (i != i_init) {
    result_value = item_array[i].value;
  }

  if (free) {
    MEM_freeN(item_array);
  }

  return result_value;
}

static PointerRNA property_pointer_get(PointerRNA *ptr, PropertyRNA *prop, const bool do_create)
{
  PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
  IDProperty *idprop;

  static blender::Mutex mutex;

  BLI_assert(RNA_property_type(prop) == PROP_POINTER);

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    pprop = (PointerPropertyRNA *)prop;

    if (RNA_struct_is_ID(pprop->type)) {
      /* ID PointerRNA should not have ancestors currently. */
      return RNA_id_pointer_create(idprop->type == IDP_GROUP ? nullptr : IDP_ID_get(idprop));
    }

    /* for groups, data is idprop itself */
    if (pprop->type_fn) {
      return RNA_pointer_create_with_parent(*ptr, pprop->type_fn(ptr), idprop);
    }
    return RNA_pointer_create_with_parent(*ptr, pprop->type, idprop);
  }
  if (pprop->get) {
    return pprop->get(ptr);
  }
  if (prop->flag & PROP_IDPROPERTY && do_create) {
    /* NOTE: While creating/writing data in an accessor is really bad design-wise, this is
     * currently very difficult to avoid in that case. So a global mutex is used to keep ensuring
     * thread safety. */
    std::scoped_lock lock(mutex);
    /* NOTE: We do not need to check again for existence of the pointer after locking here, since
     * this is also done in #RNA_property_pointer_add itself. */
    RNA_property_pointer_add(ptr, prop);
    return RNA_property_pointer_get(ptr, prop);
  }
  return PointerRNA_NULL;
}

PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop)
{
  return property_pointer_get(ptr, prop, true);
}

PointerRNA RNA_property_pointer_get_never_create(PointerRNA *ptr, PropertyRNA *prop)
{
  return property_pointer_get(ptr, prop, false);
}

void RNA_property_pointer_set(PointerRNA *ptr,
                              PropertyRNA *prop,
                              PointerRNA ptr_value,
                              ReportList *reports)
{
  /* Detect IDProperty and retrieve the actual PropertyRNA pointer before cast. */
  IDProperty *idprop = rna_idproperty_check(&prop, ptr);

  PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
  BLI_assert(RNA_property_type(prop) == PROP_POINTER);

  /* This is a 'real' RNA property, not an IDProperty or a dynamic RNA property using an IDProperty
   * as backend storage. */
  if (pprop->set) {
    if (ptr_value.type != nullptr && !RNA_struct_is_a(ptr_value.type, pprop->type)) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s: expected %s type, not %s",
                  __func__,
                  pprop->type->identifier,
                  ptr_value.type->identifier);
      return;
    }

#ifndef NDEBUG
    /* NOTE: By design, it can be safely assumed that both old and new ID pointers are valid when
     * accessed through RNA. Handling of invalid ID pointers, e.g. freed ones etc., should never
     * happen through RNA code, but directly on underlying (DNA) data.
     *
     * Setters are also not expected to free or otherwise invalidate ID pointers. So storing them
     * here should be safe. */
    BLI_assert(pprop->get);
    const bool is_id_refcounting = (prop->flag & PROP_ID_REFCOUNT) != 0;

    const PointerRNA old_id_ptr = pprop->get(ptr);
    BLI_assert_msg(!is_id_refcounting || !old_id_ptr.data || RNA_struct_is_ID(old_id_ptr.type),
                   "If the property is tagged with ID reference-counting, "
                   "its current value should be null or an ID");
    const ID *old_id = (old_id_ptr.type && RNA_struct_is_ID(old_id_ptr.type)) ?
                           old_id_ptr.data_as<ID>() :
                           nullptr;
    const int old_id_old_refcount = old_id ? ID_REFCOUNTING_USERS(old_id) : 0;

    const ID *new_id = (ptr_value.type && RNA_struct_is_ID(ptr_value.type)) ?
                           ptr_value.data_as<ID>() :
                           nullptr;
    const int new_id_old_refcount = new_id ? ID_REFCOUNTING_USERS(new_id) : 0;
#endif

    if (!((prop->flag & PROP_NEVER_NULL) && ptr_value.data == nullptr) &&
        !((prop->flag & PROP_ID_SELF_CHECK) && ptr->owner_id == ptr_value.owner_id))
    {
      pprop->set(ptr, ptr_value, reports);
    }

#ifndef NDEBUG
    /* NOTE: Current checks are relatively flexible, but do expect 'reasonable' behavior (ID
     * handling) from custom setters.
     *
     * Should there be some very uncommon setter behavior, e.g. unassigning an ID from the property
     * automatically assigning it to several other reference-counting usages, this will have to be
     * tweaked, e.g. by adding a special 'skip checks' flag to such RNA properties. */
    PointerRNA current_id_ptr = pprop->get(ptr);
    BLI_assert_msg(
        !is_id_refcounting || !current_id_ptr.data || RNA_struct_is_ID(current_id_ptr.type),
        "If the property is tagged with ID reference-counting, its current value should be "
        "null or an ID");
    ID *current_id = (current_id_ptr.type && RNA_struct_is_ID(current_id_ptr.type)) ?
                         static_cast<ID *>(current_id_ptr.data) :
                         nullptr;

    if (old_id) {
      const int old_id_new_refcount = ID_REFCOUNTING_USERS(old_id);
      if (ELEM(old_id, new_id, current_id)) {
        BLI_assert_msg(old_id_new_refcount == old_id_old_refcount,
                       "Reassigning the same ID to a RNA pointer property, or assignment failure, "
                       "should not modify the original ID user-count");
      }
      else if (is_id_refcounting) {
        BLI_assert_msg(
            old_id_new_refcount < old_id_old_refcount,
            "Unassigning an ID from a reference-counting RNA pointer property should decrease "
            "its user-count");
      }
      else {
        BLI_assert_msg(
            old_id_new_refcount == old_id_old_refcount,
            "Unassigning an ID from a non-reference-counting RNA pointer property should not "
            "modify its user-count");
      }
    }
    if (new_id && new_id != old_id) {
      const int new_id_new_refcount = ID_REFCOUNTING_USERS(new_id);
      if (current_id == old_id) {
        BLI_assert_msg(new_id_new_refcount == new_id_old_refcount,
                       "Failed assigning a new ID to a RNA pointer property, should not modify "
                       "the new ID user-count");
      }
      else if (is_id_refcounting) {
        BLI_assert_msg(
            new_id_new_refcount > new_id_old_refcount,
            "Assigning an ID to a reference-counting RNA pointer property should increase "
            "its user-count");
      }
      else {
        BLI_assert_msg(
            new_id_new_refcount == new_id_old_refcount,
            "Assigning an ID to a non-reference-counting RNA pointer property should not "
            "modify its user-count");
      }
    }
#endif

    return;
  }

  /* Assigning to an IDProperty. */
  ID *value = static_cast<ID *>(ptr_value.data);

  if (ptr_value.type != nullptr && !RNA_struct_is_a(ptr_value.type, &RNA_ID)) {
    BKE_reportf(
        reports, RPT_ERROR, "%s: expected ID type, not %s", __func__, ptr_value.type->identifier);
    return;
  }
  if (value && (value->flag & ID_FLAG_EMBEDDED_DATA) != 0) {
    BKE_reportf(reports, RPT_ERROR, "%s: cannot assign an embedded ID to an IDProperty", __func__);
    return;
  }

  /* We got an existing IDProperty. */
  if (idprop != nullptr) {
    /* Not-yet-defined ID IDProps have an IDP_GROUP type, not an IDP_ID one - because of reasons?
     * XXX This has to be investigated fully - there might be a good reason for it, but off hands
     * this seems really weird... */
    if (idprop->type == IDP_ID) {
      IDP_AssignID(idprop, value, 0);
      rna_idproperty_touch(idprop);
    }
    else {
      BLI_assert(idprop->type == IDP_GROUP);
      IDProperty *group = RNA_struct_system_idprops(ptr, true);
      BLI_assert(group != nullptr);

      IDP_ReplaceInGroup_ex(
          group,
          blender::bke::idprop::create(idprop->name, value, IDP_FLAG_STATIC_TYPE).release(),
          idprop,
          0);
    }
  }
  /* IDProperty disguised as RNA property (and not yet defined in ptr). */
  else if (prop->flag & PROP_EDITABLE) {
    if (IDProperty *group = RNA_struct_system_idprops(ptr, true)) {
      IDP_ReplaceInGroup(
          group,
          blender::bke::idprop::create(prop->identifier, value, IDP_FLAG_STATIC_TYPE).release());
    }
  }
}

PointerRNA RNA_property_pointer_get_default(PointerRNA * /*ptr*/, PropertyRNA * /*prop*/)
{
  // PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

  // BLI_assert(RNA_property_type(prop) == PROP_POINTER);

  return PointerRNA_NULL; /* FIXME: there has to be a way... */
}

void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop)
{
  // IDProperty *idprop;

  BLI_assert(RNA_property_type(prop) == PROP_POINTER);

  if (/*idprop=*/rna_idproperty_check(&prop, ptr)) {
    /* already exists */
  }
  else if (prop->flag & PROP_IDPROPERTY) {
    IDProperty *group;

    group = RNA_struct_system_idprops(ptr, true);
    if (group) {
      IDP_AddToGroup(
          group,
          blender::bke::idprop::create_group(prop->identifier, IDP_FLAG_STATIC_TYPE).release());
    }
  }
  else {
    printf("%s %s.%s: only supported for id properties.\n",
           __func__,
           ptr->type->identifier,
           prop->identifier);
  }
}

void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop)
{
  IDProperty *idprop, *group;

  BLI_assert(RNA_property_type(prop) == PROP_POINTER);

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    group = RNA_struct_system_idprops(ptr, false);

    if (group) {
      IDP_FreeFromGroup(group, idprop);
    }
  }
  else {
    printf("%s %s.%s: only supported for id properties.\n",
           __func__,
           ptr->type->identifier,
           prop->identifier);
  }
}

static void rna_property_collection_get_idp(CollectionPropertyIterator *iter)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)iter->prop;

  rna_pointer_create_with_ancestors(
      iter->parent, cprop->item_type, rna_iterator_array_get(iter), iter->ptr);
}

void RNA_property_collection_begin(PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   CollectionPropertyIterator *iter)
{
  IDProperty *idprop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  *iter = {};

  if ((idprop = rna_idproperty_check(&prop, ptr)) || (prop->flag & PROP_IDPROPERTY)) {
    iter->parent = *ptr;
    iter->prop = prop;

    if (idprop) {
      rna_iterator_array_begin(iter,
                               ptr,
                               IDP_property_array_get(idprop),
                               sizeof(IDProperty),
                               idprop->len,
                               false,
                               nullptr);
    }
    else {
      rna_iterator_array_begin(iter, ptr, nullptr, sizeof(IDProperty), 0, false, nullptr);
    }

    if (iter->valid) {
      rna_property_collection_get_idp(iter);
    }

    iter->idprop = 1;
  }
  else {
    CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
    cprop->begin(iter, ptr);
  }
}

void RNA_property_collection_next(CollectionPropertyIterator *iter)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);

  if (iter->idprop) {
    rna_iterator_array_next(iter);

    if (iter->valid) {
      rna_property_collection_get_idp(iter);
    }
  }
  else {
    cprop->next(iter);
  }
}

void RNA_property_collection_skip(CollectionPropertyIterator *iter, int num)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);
  int i;

  if (num > 1 && (iter->idprop || (cprop->property.flag_internal & PROP_INTERN_RAW_ARRAY))) {
    /* fast skip for array */
    ArrayIterator *internal = &iter->internal.array;

    if (!internal->skip) {
      internal->ptr += internal->itemsize * (num - 1);
      iter->valid = (internal->ptr < internal->endptr);
      if (iter->valid) {
        RNA_property_collection_next(iter);
      }
      return;
    }
  }

  /* slow iteration otherwise */
  for (i = 0; i < num && iter->valid; i++) {
    RNA_property_collection_next(iter);
  }
}

void RNA_property_collection_end(CollectionPropertyIterator *iter)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(iter->prop);

  if (iter->idprop) {
    rna_iterator_array_end(iter);
  }
  else {
    cprop->end(iter);
  }
}

int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
  IDProperty *idprop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    return idprop->len;
  }
  if (cprop->length) {
    return cprop->length(ptr);
  }
  CollectionPropertyIterator iter;
  int length = 0;

  RNA_property_collection_begin(ptr, prop, &iter);
  for (; iter.valid; RNA_property_collection_next(&iter)) {
    length++;
  }
  RNA_property_collection_end(&iter);

  return length;
}

bool RNA_property_collection_is_empty(PointerRNA *ptr, PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);
  CollectionPropertyIterator iter;
  RNA_property_collection_begin(ptr, prop, &iter);
  bool test = iter.valid;
  RNA_property_collection_end(&iter);
  return !test;
}

/* This helper checks whether given collection property itself is editable (we only currently
 * support a limited set of operations, insertion of new items, and re-ordering of those new items
 * exclusively). */
static bool property_collection_liboverride_editable(PointerRNA *ptr,
                                                     PropertyRNA *prop,
                                                     bool *r_is_liboverride)
{
  ID *id = ptr->owner_id;
  if (id == nullptr) {
    *r_is_liboverride = false;
    return true;
  }

  const bool is_liboverride = *r_is_liboverride = ID_IS_OVERRIDE_LIBRARY(id);

  if (!is_liboverride) {
    /* We return True also for linked data, as it allows tricks like py scripts 'overriding' data
     * of those. */
    return true;
  }

  if (!RNA_property_overridable_get(ptr, prop)) {
    return false;
  }

  if (prop->magic != RNA_MAGIC || (prop->flag & PROP_IDPROPERTY) == 0) {
    /* Insertion and such not supported for pure IDProperties for now, nor for pure RNA/DNA ones.
     */
    return false;
  }
  if ((prop->flag_override & PROPOVERRIDE_LIBRARY_INSERTION) == 0) {
    return false;
  }

  /* No more checks to do, this collections is overridable. */
  return true;
}

void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr)
{
  IDProperty *idprop;
  // CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  bool is_liboverride;
  if (!property_collection_liboverride_editable(ptr, prop, &is_liboverride)) {
    if (r_ptr) {
      *r_ptr = {};
    }
    return;
  }

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    IDProperty *item;

    item = blender::bke::idprop::create_group("", IDP_FLAG_STATIC_TYPE).release();
    if (is_liboverride) {
      item->flag |= IDP_FLAG_OVERRIDELIBRARY_LOCAL;
    }
    IDP_AppendArray(idprop, item);
    /* IDP_AppendArray does a shallow copy (memcpy), only free memory. */
    // IDP_FreePropertyContent(item);
    MEM_freeN(item);
    rna_idproperty_touch(idprop);
  }
  else if (prop->flag & PROP_IDPROPERTY) {
    IDProperty *group, *item;

    group = RNA_struct_system_idprops(ptr, true);
    if (group) {
      idprop = IDP_NewIDPArray(prop->identifier);
      IDP_AddToGroup(group, idprop);

      item = blender::bke::idprop::create_group("", IDP_FLAG_STATIC_TYPE).release();
      if (is_liboverride) {
        item->flag |= IDP_FLAG_OVERRIDELIBRARY_LOCAL;
      }
      IDP_AppendArray(idprop, item);
      /* #IDP_AppendArray does a shallow copy (memcpy), only free memory. */
      // IDP_FreePropertyContent(item);
      MEM_freeN(item);
    }
  }

  if (r_ptr) {
    if (idprop) {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

      rna_pointer_create_with_ancestors(
          *ptr, cprop->item_type, IDP_GetIndexArray(idprop, idprop->len - 1), *r_ptr);
    }
    else {
      *r_ptr = {};
    }
  }
}

bool RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key)
{
  IDProperty *idprop;
  // CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  bool is_liboverride;
  if (!property_collection_liboverride_editable(ptr, prop, &is_liboverride)) {
    return false;
  }

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    IDProperty tmp, *array;
    int len;

    len = idprop->len;
    array = IDP_property_array_get(idprop);

    if (key >= 0 && key < len) {
      if (is_liboverride && (array[key].flag & IDP_FLAG_OVERRIDELIBRARY_LOCAL) == 0) {
        /* We can only remove items that we actually inserted in the local override. */
        return false;
      }

      if (key + 1 < len) {
        /* move element to be removed to the back */
        memcpy(&tmp, &array[key], sizeof(IDProperty));
        memmove(array + key, array + key + 1, sizeof(IDProperty) * (len - (key + 1)));
        memcpy(&array[len - 1], &tmp, sizeof(IDProperty));
      }

      IDP_ResizeIDPArray(idprop, len - 1);
    }

    return true;
  }
  if (prop->flag & PROP_IDPROPERTY) {
    return true;
  }

  return false;
}

bool RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos)
{
  IDProperty *idprop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  bool is_liboverride;
  if (!property_collection_liboverride_editable(ptr, prop, &is_liboverride)) {
    return false;
  }

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    IDProperty tmp, *array;
    int len;

    len = idprop->len;
    array = IDP_property_array_get(idprop);

    if (key >= 0 && key < len && pos >= 0 && pos < len && key != pos) {
      if (is_liboverride && (array[key].flag & IDP_FLAG_OVERRIDELIBRARY_LOCAL) == 0) {
        /* We can only move items that we actually inserted in the local override. */
        return false;
      }

      memcpy(&tmp, &array[key], sizeof(IDProperty));
      if (pos < key) {
        memmove(array + pos + 1, array + pos, sizeof(IDProperty) * (key - pos));
      }
      else {
        memmove(array + key, array + key + 1, sizeof(IDProperty) * (pos - key));
      }
      memcpy(&array[pos], &tmp, sizeof(IDProperty));
    }

    return true;
  }
  if (prop->flag & PROP_IDPROPERTY) {
    return true;
  }

  return false;
}

void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop)
{
  IDProperty *idprop;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  bool is_liboverride;
  if (!property_collection_liboverride_editable(ptr, prop, &is_liboverride)) {
    return;
  }

  if ((idprop = rna_idproperty_check(&prop, ptr))) {
    if (is_liboverride) {
      /* We can only move items that we actually inserted in the local override. */
      int len = idprop->len;
      IDProperty tmp, *array = IDP_property_array_get(idprop);
      for (int i = 0; i < len; i++) {
        if ((array[i].flag & IDP_FLAG_OVERRIDELIBRARY_LOCAL) != 0) {
          memcpy(&tmp, &array[i], sizeof(IDProperty));
          memmove(array + i, array + i + 1, sizeof(IDProperty) * (len - (i + 1)));
          memcpy(&array[len - 1], &tmp, sizeof(IDProperty));
          IDP_ResizeIDPArray(idprop, --len);
          i--;
        }
      }
    }
    else {
      IDP_ResizeIDPArray(idprop, 0);
    }
    rna_idproperty_touch(idprop);
  }
}

int RNA_property_collection_lookup_index(PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         const PointerRNA *t_ptr)
{
  CollectionPropertyIterator iter;
  int index = 0;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  RNA_property_collection_begin(ptr, prop, &iter);
  for (index = 0; iter.valid; RNA_property_collection_next(&iter), index++) {
    if (iter.ptr.data == t_ptr->data) {
      break;
    }
  }
  RNA_property_collection_end(&iter);

  /* did we find it? */
  if (iter.valid) {
    return index;
  }
  return -1;
}

bool RNA_property_collection_lookup_int_has_fn(PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);
  return cprop->lookupint != nullptr;
}

bool RNA_property_collection_lookup_string_has_fn(PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);
  return cprop->lookupstring != nullptr;
}

bool RNA_property_collection_lookup_string_has_nameprop(PropertyRNA *prop)
{
  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);
  return (cprop->item_type && cprop->item_type->nameproperty);
}

bool RNA_property_collection_lookup_string_supported(PropertyRNA *prop)
{
  return (RNA_property_collection_lookup_string_has_fn(prop) ||
          RNA_property_collection_lookup_string_has_nameprop(prop));
}

bool RNA_property_collection_lookup_int(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        int key,
                                        PointerRNA *r_ptr)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  if (cprop->lookupint) {
    /* we have a callback defined, use it */
    return cprop->lookupint(ptr, key, r_ptr);
  }
  /* no callback defined, just iterate and find the nth item */
  CollectionPropertyIterator iter;
  int i;

  RNA_property_collection_begin(ptr, prop, &iter);
  for (i = 0; iter.valid; RNA_property_collection_next(&iter), i++) {
    if (i == key) {
      *r_ptr = iter.ptr;
      break;
    }
  }
  RNA_property_collection_end(&iter);

  if (!iter.valid) {
    *r_ptr = {};
  }

  return iter.valid;
}

bool RNA_property_collection_lookup_string_index(
    PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr, int *r_index)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  if (!key) {
    *r_index = -1;
    *r_ptr = PointerRNA_NULL;
    return false;
  }

  if (cprop->lookupstring) {
    /* we have a callback defined, use it */
    return cprop->lookupstring(ptr, key, r_ptr);
  }
  /* no callback defined, compare with name properties if they exist */
  CollectionPropertyIterator iter;
  PropertyRNA *nameprop;
  char name_buf[256], *name;
  bool found = false;
  int keylen = strlen(key);
  int namelen;
  int index = 0;

  RNA_property_collection_begin(ptr, prop, &iter);
  for (; iter.valid; RNA_property_collection_next(&iter), index++) {
    if (iter.ptr.data && iter.ptr.type->nameproperty) {
      nameprop = iter.ptr.type->nameproperty;

      name = RNA_property_string_get_alloc(
          &iter.ptr, nameprop, name_buf, sizeof(name_buf), &namelen);

      if ((keylen == namelen) && STREQ(name, key)) {
        *r_ptr = iter.ptr;
        found = true;
      }

      if (name != name_buf) {
        MEM_freeN(name);
      }

      if (found) {
        break;
      }
    }
  }
  RNA_property_collection_end(&iter);

  if (!iter.valid) {
    *r_ptr = {};
    *r_index = -1;
  }
  else {
    *r_index = index;
  }

  return iter.valid;
}

bool RNA_property_collection_lookup_string(PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const char *key,
                                           PointerRNA *r_ptr)
{
  int index;
  return RNA_property_collection_lookup_string_index(ptr, prop, key, r_ptr, &index);
}

bool RNA_property_collection_assign_int(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        const int key,
                                        const PointerRNA *assign_ptr)
{
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)rna_ensure_property(prop);

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  if (cprop->assignint) {
    /* we have a callback defined, use it */
    return cprop->assignint(ptr, key, assign_ptr);
  }

  return false;
}

bool RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr)
{
  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  *r_ptr = *ptr;
  return ((r_ptr->type = rna_ensure_property(prop)->srna) ? 1 : 0);
}

int RNA_property_collection_raw_array(
    PointerRNA *ptr, PropertyRNA *prop, PropertyRNA *itemprop, bool set, RawArray *array)
{
  CollectionPropertyIterator iter;
  ArrayIterator *internal;
  char *arrayp;

  BLI_assert(RNA_property_type(prop) == PROP_COLLECTION);

  if (!(prop->flag_internal & PROP_INTERN_RAW_ARRAY) ||
      !(itemprop->flag_internal & PROP_INTERN_RAW_ACCESS))
  {
    return 0;
  }

  RNA_property_collection_begin(ptr, prop, &iter);

  if (iter.valid) {
    /* get data from array iterator and item property */
    internal = &iter.internal.array;
    arrayp = (iter.valid) ? static_cast<char *>(iter.ptr.data) : nullptr;

    if (internal->skip || (set && !RNA_property_editable(&iter.ptr, itemprop))) {
      /* we might skip some items, so it's not a proper array */
      RNA_property_collection_end(&iter);
      return 0;
    }

    array->array = arrayp + itemprop->rawoffset;
    array->stride = internal->itemsize;
    array->len = ((char *)internal->endptr - arrayp) / internal->itemsize;
    array->type = itemprop->rawtype;
  }
  else {
    memset(array, 0, sizeof(RawArray));
  }

  RNA_property_collection_end(&iter);

  return 1;
}

#define RAW_GET(dtype, var, raw, a) \
  { \
    switch (raw.type) { \
      case PROP_RAW_CHAR: \
        var = (dtype)((char *)raw.array)[a]; \
        break; \
      case PROP_RAW_INT8: \
        var = (dtype)((int8_t *)raw.array)[a]; \
        break; \
      case PROP_RAW_UINT8: \
        var = (dtype)((uint8_t *)raw.array)[a]; \
        break; \
      case PROP_RAW_SHORT: \
        var = (dtype)((short *)raw.array)[a]; \
        break; \
      case PROP_RAW_UINT16: \
        var = (dtype)((uint16_t *)raw.array)[a]; \
        break; \
      case PROP_RAW_INT: \
        var = (dtype)((int *)raw.array)[a]; \
        break; \
      case PROP_RAW_BOOLEAN: \
        var = (dtype)((bool *)raw.array)[a]; \
        break; \
      case PROP_RAW_FLOAT: \
        var = (dtype)((float *)raw.array)[a]; \
        break; \
      case PROP_RAW_DOUBLE: \
        var = (dtype)((double *)raw.array)[a]; \
        break; \
      case PROP_RAW_INT64: \
        var = (dtype)((int64_t *)raw.array)[a]; \
        break; \
      case PROP_RAW_UINT64: \
        var = (dtype)((uint64_t *)raw.array)[a]; \
        break; \
      default: \
        var = (dtype)0; \
    } \
  } \
  (void)0

#define RAW_SET(dtype, raw, a, var) \
  { \
    switch (raw.type) { \
      case PROP_RAW_CHAR: \
        ((char *)raw.array)[a] = char(var); \
        break; \
      case PROP_RAW_INT8: \
        ((int8_t *)raw.array)[a] = int8_t(var); \
        break; \
      case PROP_RAW_UINT8: \
        ((uint8_t *)raw.array)[a] = uint8_t(var); \
        break; \
      case PROP_RAW_SHORT: \
        ((short *)raw.array)[a] = short(var); \
        break; \
      case PROP_RAW_UINT16: \
        ((uint16_t *)raw.array)[a] = uint16_t(var); \
        break; \
      case PROP_RAW_INT: \
        ((int *)raw.array)[a] = int(var); \
        break; \
      case PROP_RAW_BOOLEAN: \
        ((bool *)raw.array)[a] = bool(var); \
        break; \
      case PROP_RAW_FLOAT: \
        ((float *)raw.array)[a] = float(var); \
        break; \
      case PROP_RAW_DOUBLE: \
        ((double *)raw.array)[a] = double(var); \
        break; \
      case PROP_RAW_INT64: \
        ((int64_t *)raw.array)[a] = int64_t(var); \
        break; \
      case PROP_RAW_UINT64: \
        ((uint64_t *)raw.array)[a] = uint64_t(var); \
        break; \
      default: \
        break; \
    } \
  } \
  (void)0

size_t RNA_raw_type_sizeof(RawPropertyType type)
{
  switch (type) {
    case PROP_RAW_CHAR:
      return sizeof(char);
    case PROP_RAW_INT8:
      return sizeof(int8_t);
    case PROP_RAW_UINT8:
      return sizeof(uint8_t);
    case PROP_RAW_SHORT:
      return sizeof(short);
    case PROP_RAW_UINT16:
      return sizeof(uint16_t);
    case PROP_RAW_INT:
      return sizeof(int);
    case PROP_RAW_BOOLEAN:
      return sizeof(bool);
    case PROP_RAW_FLOAT:
      return sizeof(float);
    case PROP_RAW_DOUBLE:
      return sizeof(double);
    case PROP_RAW_INT64:
      return sizeof(int64_t);
    case PROP_RAW_UINT64:
      return sizeof(uint64_t);
    default:
      return 0;
  }
}

static int rna_property_array_length_all_dimensions(PointerRNA *ptr, PropertyRNA *prop)
{
  int i, len[RNA_MAX_ARRAY_DIMENSION];
  const int dim = RNA_property_array_dimension(ptr, prop, len);
  int size;

  if (dim == 0) {
    return 0;
  }

  for (size = 1, i = 0; i < dim; i++) {
    size *= len[i];
  }

  return size;
}

static int rna_raw_access(ReportList *reports,
                          PointerRNA *ptr,
                          PropertyRNA *prop,
                          const char *propname,
                          void *inarray,
                          RawPropertyType intype,
                          int inlen,
                          int set)
{
  StructRNA *ptype;
  PropertyRNA *itemprop, *iprop;
  PropertyType itemtype = PropertyType(0);
  RawArray in;
  /* Actual array length. Will always be `0` for non-array properties. */
  int array_len = 0;
  /* Item length. Will always be `1` for non-array properties. */
  int item_len = 0;
  /* Whether the accessed property is an array or not. */
  bool is_array;

  /* initialize in array, stride assumed 0 in following code */
  in.array = inarray;
  in.type = intype;
  in.len = inlen;
  in.stride = 0;

  ptype = RNA_property_pointer_type(ptr, prop);

  /* try to get item property pointer */
  PointerRNA itemptr_base = RNA_pointer_create_discrete(nullptr, ptype, nullptr);
  itemprop = RNA_struct_find_property(&itemptr_base, propname);

  if (itemprop) {
    /* we have item property pointer */
    RawArray out;

    /* check type */
    itemtype = RNA_property_type(itemprop);
    is_array = RNA_property_array_check(itemprop);

    if (!ELEM(itemtype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT, PROP_ENUM)) {
      BKE_report(reports, RPT_ERROR, "Only boolean, int, float, and enum properties supported");
      return 0;
    }

    /* check item array */
    array_len = RNA_property_array_length(&itemptr_base, itemprop);
    item_len = is_array ? array_len : 1;

    /* dynamic array? need to get length per item */
    if (itemprop->getlength) {
      itemprop = nullptr;
    }
    /* try to access as raw array */
    else if (RNA_property_collection_raw_array(ptr, prop, itemprop, set, &out)) {
      if (in.len != item_len * out.len) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Array length mismatch (expected %d, got %d)",
                    out.len * item_len,
                    in.len);
        return 0;
      }

      /* matching raw types */
      if (out.type == in.type) {
        void *inp = in.array;
        void *outp = out.array;
        size_t size;

        size = RNA_raw_type_sizeof(out.type) * item_len;

        if (size == out.stride) {
          /* The property is stored contiguously so the entire array can be copied at once. */
          if (set) {
            memcpy(outp, inp, size * out.len);
          }
          else {
            memcpy(inp, outp, size * out.len);
          }
        }
        else {
          for (int a = 0; a < out.len; a++) {
            if (set) {
              memcpy(outp, inp, size);
            }
            else {
              memcpy(inp, outp, size);
            }

            inp = (char *)inp + size;
            outp = (char *)outp + out.stride;
          }
        }

        return 1;
      }

      /* Could also be faster with non-matching types,
       * for now we just do slower loop. */
    }
    BLI_assert_msg(array_len == 0 || itemtype != PROP_ENUM,
                   "Enum array properties should not exist");
  }

  {
    void *tmparray = nullptr;
    int tmplen = 0;
    int err = 0, j, a = 0;
    int needconv = 1;

    if (((itemtype == PROP_INT) && (in.type == PROP_RAW_INT)) ||
        ((itemtype == PROP_BOOLEAN) && (in.type == PROP_RAW_BOOLEAN)) ||
        ((itemtype == PROP_FLOAT) && (in.type == PROP_RAW_FLOAT)))
    {
      /* avoid creating temporary buffer if the data type match */
      needconv = 0;
    }
    /* no item property pointer, can still be id property, or
     * property of a type derived from the collection pointer type */
    RNA_PROP_BEGIN (ptr, itemptr, prop) {
      if (itemptr.data) {
        if (itemprop) {
          /* we got the property already */
          iprop = itemprop;
        }
        else {
          /* not yet, look it up and verify if it is valid */
          iprop = RNA_struct_find_property(&itemptr, propname);

          if (iprop) {
            is_array = RNA_property_array_check(iprop);
            array_len = rna_property_array_length_all_dimensions(&itemptr, iprop);
            item_len = is_array ? array_len : 1;
            itemtype = RNA_property_type(iprop);
          }
          else {
            BKE_reportf(reports, RPT_ERROR, "Property named '%s' not found", propname);
            err = 1;
            break;
          }

          if (!ELEM(itemtype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT, PROP_ENUM)) {
            BKE_report(
                reports, RPT_ERROR, "Only boolean, int, float and enum properties supported");
            err = 1;
            break;
          }
          BLI_assert_msg(array_len == 0 || itemtype != PROP_ENUM,
                         "Enum array properties should not exist");
        }

        /* editable check */
        if (!set || RNA_property_editable(&itemptr, iprop)) {
          if (a + item_len > in.len) {
            BKE_reportf(
                reports, RPT_ERROR, "Array length mismatch (got %d, expected more)", in.len);
            err = 1;
            break;
          }

          if (array_len == 0) {
            /* handle conversions */
            if (set) {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  int b;
                  RAW_GET(bool, b, in, a);
                  RNA_property_boolean_set(&itemptr, iprop, b);
                  break;
                }
                case PROP_INT: {
                  int i;
                  RAW_GET(int, i, in, a);
                  RNA_property_int_set(&itemptr, iprop, i);
                  break;
                }
                case PROP_FLOAT: {
                  float f;
                  RAW_GET(float, f, in, a);
                  RNA_property_float_set(&itemptr, iprop, f);
                  break;
                }
                case PROP_ENUM: {
                  int i;
                  RAW_GET(int, i, in, a);
                  RNA_property_enum_set(&itemptr, iprop, i);
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
            else {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  int b = RNA_property_boolean_get(&itemptr, iprop);
                  RAW_SET(bool, in, a, b);
                  break;
                }
                case PROP_INT: {
                  int i = RNA_property_int_get(&itemptr, iprop);
                  RAW_SET(int, in, a, i);
                  break;
                }
                case PROP_FLOAT: {
                  float f = RNA_property_float_get(&itemptr, iprop);
                  RAW_SET(float, in, a, f);
                  break;
                }
                case PROP_ENUM: {
                  int i = RNA_property_enum_get(&itemptr, iprop);
                  RAW_SET(int, in, a, i);
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
            a++;
          }
          else if (needconv == 1) {
            /* allocate temporary array if needed */
            if (tmparray && tmplen != array_len) {
              MEM_freeN(tmparray);
              tmparray = nullptr;
            }
            if (!tmparray) {
              tmparray = MEM_calloc_arrayN<float>(array_len, "RNA tmparray");
              tmplen = array_len;
            }

            /* handle conversions */
            if (set) {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  bool *array = static_cast<bool *>(tmparray);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_GET(bool, array[j], in, a);
                  }
                  RNA_property_boolean_set_array(&itemptr, iprop, array);
                  break;
                }
                case PROP_INT: {
                  int *array = static_cast<int *>(tmparray);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_GET(int, array[j], in, a);
                  }
                  RNA_property_int_set_array(&itemptr, iprop, array);
                  break;
                }
                case PROP_FLOAT: {
                  float *array = static_cast<float *>(tmparray);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_GET(float, array[j], in, a);
                  }
                  RNA_property_float_set_array(&itemptr, iprop, array);
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
            else {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  bool *array = static_cast<bool *>(tmparray);
                  RNA_property_boolean_get_array(&itemptr, iprop, array);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_SET(int, in, a, ((bool *)tmparray)[j]);
                  }
                  break;
                }
                case PROP_INT: {
                  int *array = static_cast<int *>(tmparray);
                  RNA_property_int_get_array(&itemptr, iprop, array);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_SET(int, in, a, array[j]);
                  }
                  break;
                }
                case PROP_FLOAT: {
                  float *array = static_cast<float *>(tmparray);
                  RNA_property_float_get_array(&itemptr, iprop, array);
                  for (j = 0; j < array_len; j++, a++) {
                    RAW_SET(float, in, a, array[j]);
                  }
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
          }
          else {
            if (set) {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  RNA_property_boolean_set_array(&itemptr, iprop, &((bool *)in.array)[a]);
                  a += array_len;
                  break;
                }
                case PROP_INT: {
                  RNA_property_int_set_array(&itemptr, iprop, &((int *)in.array)[a]);
                  a += array_len;
                  break;
                }
                case PROP_FLOAT: {
                  RNA_property_float_set_array(&itemptr, iprop, &((float *)in.array)[a]);
                  a += array_len;
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
            else {
              switch (itemtype) {
                case PROP_BOOLEAN: {
                  RNA_property_boolean_get_array(&itemptr, iprop, &((bool *)in.array)[a]);
                  a += array_len;
                  break;
                }
                case PROP_INT: {
                  RNA_property_int_get_array(&itemptr, iprop, &((int *)in.array)[a]);
                  a += array_len;
                  break;
                }
                case PROP_FLOAT: {
                  RNA_property_float_get_array(&itemptr, iprop, &((float *)in.array)[a]);
                  a += array_len;
                  break;
                }
                default:
                  BLI_assert_unreachable();
                  break;
              }
            }
          }
        }
      }
    }
    RNA_PROP_END;

    if (tmparray) {
      MEM_freeN(tmparray);
    }

    return !err;
  }
}

RawPropertyType RNA_property_raw_type(PropertyRNA *prop)
{
  if (prop->rawtype == PROP_RAW_UNSET) {
    /* this property has no raw access,
     * yet we try to provide a raw type to help building the array. */
    switch (prop->type) {
      case PROP_BOOLEAN:
        return PROP_RAW_BOOLEAN;
      case PROP_INT:
        return PROP_RAW_INT;
      case PROP_FLOAT:
        return PROP_RAW_FLOAT;
      case PROP_ENUM:
        return PROP_RAW_INT;
      default:
        break;
    }
  }
  return prop->rawtype;
}

int RNA_property_collection_raw_get(ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len)
{
  return rna_raw_access(reports, ptr, prop, propname, array, type, len, 0);
}

int RNA_property_collection_raw_set(ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len)
{
  return rna_raw_access(reports, ptr, prop, propname, array, type, len, 1);
}

/* Standard iterator functions */

void rna_iterator_listbase_begin(CollectionPropertyIterator *iter,
                                 PointerRNA *ptr,
                                 ListBase *lb,
                                 IteratorSkipFunc skip)
{
  iter->parent = *ptr;

  ListBaseIterator *internal = &iter->internal.listbase;

  internal->link = (lb) ? static_cast<Link *>(lb->first) : nullptr;
  internal->skip = skip;

  iter->valid = (internal->link != nullptr);

  if (skip && iter->valid && skip(iter, internal->link)) {
    rna_iterator_listbase_next(iter);
  }
}

void rna_iterator_listbase_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  if (internal->skip) {
    do {
      internal->link = internal->link->next;
      iter->valid = (internal->link != nullptr);
    } while (iter->valid && internal->skip(iter, internal->link));
  }
  else {
    internal->link = internal->link->next;
    iter->valid = (internal->link != nullptr);
  }
}

void *rna_iterator_listbase_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  return internal->link;
}

void rna_iterator_listbase_end(CollectionPropertyIterator * /*iter*/) {}

PointerRNA rna_listbase_lookup_int(PointerRNA *ptr, StructRNA *type, ListBase *lb, int index)
{
  void *data = BLI_findlink(lb, index);
  return RNA_pointer_create_with_parent(*ptr, type, data);
}

void rna_iterator_array_begin(CollectionPropertyIterator *iter,
                              PointerRNA *ptr,
                              void *data,
                              size_t itemsize,
                              int64_t length,
                              bool free_ptr,
                              IteratorSkipFunc skip)
{
  iter->parent = *ptr;

  ArrayIterator *internal;

  if (data == nullptr) {
    length = 0;
  }
  else if (length == 0) {
    data = nullptr;
    itemsize = 0;
  }
  else if (UNLIKELY(length < 0 || length > std::numeric_limits<uint64_t>::max() / itemsize)) {
    /* This path is never expected to execute. Assert and trace if it ever does. */
    BLI_assert_unreachable();
    data = nullptr;
    length = 0;
  }

  internal = &iter->internal.array;
  internal->ptr = static_cast<char *>(data);
  internal->free_ptr = free_ptr ? data : nullptr;
  internal->endptr = ((char *)data) + itemsize * length;
  internal->itemsize = itemsize;
  internal->skip = skip;
  internal->length = length;

  iter->valid = (internal->ptr != internal->endptr);

  if (skip && iter->valid && skip(iter, internal->ptr)) {
    rna_iterator_array_next(iter);
  }
}

void rna_iterator_array_next(CollectionPropertyIterator *iter)
{
  ArrayIterator *internal = &iter->internal.array;

  if (internal->skip) {
    do {
      internal->ptr += internal->itemsize;
      iter->valid = (internal->ptr != internal->endptr);
    } while (iter->valid && internal->skip(iter, internal->ptr));
  }
  else {
    internal->ptr += internal->itemsize;
    iter->valid = (internal->ptr != internal->endptr);
  }
}

void *rna_iterator_array_get(CollectionPropertyIterator *iter)
{
  ArrayIterator *internal = &iter->internal.array;

  return internal->ptr;
}

void *rna_iterator_array_dereference_get(CollectionPropertyIterator *iter)
{
  ArrayIterator *internal = &iter->internal.array;

  /* for ** arrays */
  return *(void **)(internal->ptr);
}

void rna_iterator_array_end(CollectionPropertyIterator *iter)
{
  ArrayIterator *internal = &iter->internal.array;

  MEM_SAFE_FREE(internal->free_ptr);
}

PointerRNA rna_array_lookup_int(
    PointerRNA *ptr, StructRNA *type, void *data, size_t itemsize, int64_t length, int64_t index)
{
  if (index < 0 || index >= length) {
    return PointerRNA_NULL;
  }
  if (UNLIKELY(index > std::numeric_limits<uint64_t>::max() / itemsize)) {
    /* This path is never expected to execute. Assert and trace if it ever does. */
    BLI_assert_unreachable();
    return PointerRNA_NULL;
  }

  return RNA_pointer_create_with_parent(*ptr, type, ((char *)data) + itemsize * index);
}

/* Quick name based property access */

bool RNA_boolean_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_boolean_get(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return false;
}

void RNA_boolean_set(PointerRNA *ptr, const char *name, bool value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_boolean_set(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_boolean_get_array(PointerRNA *ptr, const char *name, bool *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_boolean_get_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const bool *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_boolean_set_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

int RNA_int_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_int_get(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return 0;
}

void RNA_int_set(PointerRNA *ptr, const char *name, int value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_int_set(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_int_get_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_int_set_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

float RNA_float_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_float_get(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return 0;
}

void RNA_float_set(PointerRNA *ptr, const char *name, float value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_float_set(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_float_get_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_float_set_array(ptr, prop, values);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

int RNA_enum_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_enum_get(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return 0;
}

void RNA_enum_set(PointerRNA *ptr, const char *name, int value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_enum_set(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_enum_set_identifier(bContext *C, PointerRNA *ptr, const char *name, const char *id)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    int value;
    if (RNA_property_enum_value(C, ptr, prop, id, &value)) {
      RNA_property_enum_set(ptr, prop, value);
    }
    else {
      printf("%s: %s.%s has no enum id '%s'.\n", __func__, ptr->type->identifier, name, id);
    }
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

bool RNA_enum_is_equal(bContext *C, PointerRNA *ptr, const char *name, const char *enumname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);
  const EnumPropertyItem *item;
  bool free;

  if (prop) {
    int i;
    bool cmp = false;

    RNA_property_enum_items(C, ptr, prop, &item, nullptr, &free);
    i = RNA_enum_from_identifier(item, enumname);
    if (i != -1) {
      cmp = (item[i].value == RNA_property_enum_get(ptr, prop));
    }

    if (free) {
      MEM_freeN(item);
    }

    if (i != -1) {
      return cmp;
    }

    printf("%s: %s.%s item %s not found.\n", __func__, ptr->type->identifier, name, enumname);
    return false;
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return false;
}

bool RNA_enum_value_from_id(const EnumPropertyItem *item, const char *identifier, int *r_value)
{
  const int i = RNA_enum_from_identifier(item, identifier);
  if (i != -1) {
    *r_value = item[i].value;
    return true;
  }
  return false;
}

bool RNA_enum_id_from_value(const EnumPropertyItem *item, int value, const char **r_identifier)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_identifier = item[i].identifier;
    return true;
  }
  return false;
}

bool RNA_enum_icon_from_value(const EnumPropertyItem *item, int value, int *r_icon)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_icon = item[i].icon;
    return true;
  }
  return false;
}

bool RNA_enum_name_from_value(const EnumPropertyItem *item, int value, const char **r_name)
{
  const int i = RNA_enum_from_value(item, value);
  if (i != -1) {
    *r_name = item[i].name;
    return true;
  }
  return false;
}

std::string RNA_string_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);
  if (!prop) {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
    return {};
  }
  return RNA_property_string_get(ptr, prop);
}

void RNA_string_get(PointerRNA *ptr, const char *name, char *value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_string_get(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
    value[0] = '\0';
  }
}

char *RNA_string_get_alloc(
    PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen, int *r_len)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_string_get_alloc(ptr, prop, fixedbuf, fixedlen, r_len);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  if (r_len != nullptr) {
    *r_len = 0;
  }
  return nullptr;
}

int RNA_string_length(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_string_length(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return 0;
}

void RNA_string_set(PointerRNA *ptr, const char *name, const char *value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_string_set(ptr, prop, value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_pointer_get(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);

  return PointerRNA_NULL;
}

void RNA_pointer_set(PointerRNA *ptr, const char *name, PointerRNA ptr_value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_pointer_set(ptr, prop, ptr_value, nullptr);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_pointer_add(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_pointer_add(ptr, prop);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_collection_begin(ptr, prop, iter);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_collection_add(ptr, prop, r_value);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

void RNA_collection_clear(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    RNA_property_collection_clear(ptr, prop);
  }
  else {
    printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  }
}

int RNA_collection_length(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_collection_length(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return 0;
}

bool RNA_collection_is_empty(PointerRNA *ptr, const char *name)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);

  if (prop) {
    return RNA_property_collection_is_empty(ptr, prop);
  }
  printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return false;
}

bool RNA_property_is_set_ex(PointerRNA *ptr, PropertyRNA *prop, bool use_ghost)
{
  prop = rna_ensure_property(prop);
  if (prop->flag & PROP_IDPROPERTY) {
    IDProperty *idprop = rna_system_idproperty_find(ptr, prop->identifier);
    return ((idprop != nullptr) && (use_ghost == false || !(idprop->flag & IDP_FLAG_GHOST)));
  }
  return true;
}

bool RNA_property_is_set(PointerRNA *ptr, PropertyRNA *prop)
{
  prop = rna_ensure_property(prop);
  if (prop->flag & PROP_IDPROPERTY) {
    IDProperty *idprop = rna_system_idproperty_find(ptr, prop->identifier);
    return ((idprop != nullptr) && !(idprop->flag & IDP_FLAG_GHOST));
  }
  return true;
}

void RNA_property_unset(PointerRNA *ptr, PropertyRNA *prop)
{
  prop = rna_ensure_property(prop);
  if (prop->flag & PROP_IDPROPERTY) {
    rna_system_idproperty_free(ptr, prop->identifier);
  }
}

bool RNA_struct_property_is_set_ex(PointerRNA *ptr, const char *identifier, bool use_ghost)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

  if (prop) {
    return RNA_property_is_set_ex(ptr, prop, use_ghost);
  }
  /* python raises an error */
  // printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return false;
}

bool RNA_struct_property_is_set(PointerRNA *ptr, const char *identifier)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

  if (prop) {
    return RNA_property_is_set(ptr, prop);
  }
  /* python raises an error */
  // printf("%s: %s.%s not found.\n", __func__, ptr->type->identifier, name);
  return false;
}

void RNA_struct_property_unset(PointerRNA *ptr, const char *identifier)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);

  if (prop) {
    RNA_property_unset(ptr, prop);
  }
}

bool RNA_property_is_idprop(const PropertyRNA *prop)
{
  return (prop->magic != RNA_MAGIC);
}

bool RNA_property_is_unlink(PropertyRNA *prop)
{
  const int flag = RNA_property_flag(prop);
  if (RNA_property_type(prop) == PROP_STRING) {
    return (flag & PROP_NEVER_UNLINK) == 0;
  }
  return (flag & (PROP_NEVER_UNLINK | PROP_NEVER_NULL)) == 0;
}

std::string RNA_pointer_as_string_id(bContext *C, PointerRNA *ptr)
{
  std::stringstream ss;

  const char *propname;
  int first_time = 1;

  ss << '{';

  RNA_STRUCT_BEGIN (ptr, prop) {
    propname = RNA_property_identifier(prop);

    if (STREQ(propname, "rna_type")) {
      continue;
    }

    if (first_time == 0) {
      ss << ", ";
    }
    first_time = 0;

    const std::string str = RNA_property_as_string(C, ptr, prop, -1, INT_MAX);
    ss << fmt::format("\"{}\":{}", propname, str);
  }
  RNA_STRUCT_END;

  ss << '}';

  return ss.str();
}

static std::optional<std::string> rna_pointer_as_string__bldata(PointerRNA *ptr)
{
  if (ptr->type == nullptr || ptr->owner_id == nullptr) {
    return "None";
  }
  if (RNA_struct_is_ID(ptr->type)) {
    return RNA_path_full_ID_py(ptr->owner_id);
  }
  return RNA_path_full_struct_py(ptr);
}

std::optional<std::string> RNA_pointer_as_string(bContext *C,
                                                 PointerRNA *ptr,
                                                 PropertyRNA *prop_ptr,
                                                 PointerRNA *ptr_prop)
{
  IDProperty *prop;
  if (ptr_prop->data == nullptr) {
    return "None";
  }
  if ((prop = rna_idproperty_check(&prop_ptr, ptr)) && prop->type != IDP_ID) {
    return RNA_pointer_as_string_id(C, ptr_prop);
  }
  return rna_pointer_as_string__bldata(ptr_prop);
}

std::string RNA_pointer_as_string_keywords_ex(bContext *C,
                                              PointerRNA *ptr,
                                              const bool as_function,
                                              const bool all_args,
                                              const bool nested_args,
                                              const int max_prop_length,
                                              PropertyRNA *iterprop)
{
  const char *arg_name = nullptr;

  PropertyRNA *prop;

  std::stringstream ss;

  bool first_iter = true;
  int flag, flag_parameter;

  RNA_PROP_BEGIN (ptr, propptr, iterprop) {
    prop = static_cast<PropertyRNA *>(propptr.data);

    flag = RNA_property_flag(prop);
    flag_parameter = RNA_parameter_flag(prop);

    if (as_function && (flag_parameter & PARM_OUTPUT)) {
      continue;
    }

    arg_name = RNA_property_identifier(prop);

    if (STREQ(arg_name, "rna_type")) {
      continue;
    }

    if ((nested_args == false) && (RNA_property_type(prop) == PROP_POINTER)) {
      continue;
    }

    if (as_function && (prop->flag_parameter & PARM_REQUIRED)) {
      /* required args don't have useful defaults */
      ss << fmt::format(fmt::runtime(first_iter ? "{}" : ", {}"), arg_name);
      first_iter = false;
    }
    else {
      bool ok = true;

      if (all_args == true) {
        /* pass */
      }
      else if (RNA_struct_system_idprops_check(ptr->type)) {
        ok = RNA_property_is_set(ptr, prop);
      }

      if (ok) {
        std::string buf;
        if (as_function && RNA_property_type(prop) == PROP_POINTER) {
          /* don't expand pointers for functions */
          if (flag & PROP_NEVER_NULL) {
            /* we can't really do the right thing here. arg=arg?, hrmf! */
            buf = arg_name;
          }
          else {
            buf = "None";
          }
        }
        else {
          buf = RNA_property_as_string(C, ptr, prop, -1, max_prop_length);
        }

        ss << fmt::format(fmt::runtime(first_iter ? "{}={}" : ", {}={}"), arg_name, buf);
        first_iter = false;
      }
    }
  }
  RNA_PROP_END;

  return ss.str();
}

std::string RNA_pointer_as_string_keywords(bContext *C,
                                           PointerRNA *ptr,
                                           const bool as_function,
                                           const bool all_args,
                                           const bool nested_args,
                                           const int max_prop_length)
{
  PropertyRNA *iterprop;

  iterprop = RNA_struct_iterator_property(ptr->type);

  return RNA_pointer_as_string_keywords_ex(
      C, ptr, as_function, all_args, nested_args, max_prop_length, iterprop);
}

std::string RNA_function_as_string_keywords(bContext *C,
                                            FunctionRNA *func,
                                            const bool as_function,
                                            const bool all_args,
                                            const int max_prop_length)
{
  PointerRNA funcptr = RNA_pointer_create_discrete(nullptr, &RNA_Function, func);

  PropertyRNA *iterprop = RNA_struct_find_property(&funcptr, "parameters");

  RNA_struct_iterator_property(funcptr.type);

  return RNA_pointer_as_string_keywords_ex(
      C, &funcptr, as_function, all_args, true, max_prop_length, iterprop);
}

static const char *bool_as_py_string(const int var)
{
  return var ? "True" : "False";
}

static void *rna_array_as_string_alloc(
    int type, int len, PointerRNA *ptr, PropertyRNA *prop, void **r_buf_end)
{
  switch (type) {
    case PROP_BOOLEAN: {
      bool *buf = MEM_malloc_arrayN<bool>(size_t(len), __func__);
      RNA_property_boolean_get_array(ptr, prop, buf);
      *r_buf_end = buf + len;
      return buf;
    }
    case PROP_INT: {
      int *buf = MEM_malloc_arrayN<int>(size_t(len), __func__);
      RNA_property_int_get_array(ptr, prop, buf);
      *r_buf_end = buf + len;
      return buf;
    }
    case PROP_FLOAT: {
      float *buf = MEM_malloc_arrayN<float>(size_t(len), __func__);
      RNA_property_float_get_array(ptr, prop, buf);
      *r_buf_end = buf + len;
      return buf;
    }
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

static void rna_array_as_string_elem(int type, void **buf_p, int len, std::stringstream &ss)
{
  /* This will print a comma separated string of the array elements from
   * buf start to len. We will add a comma if len == 1 to preserve tuples. */
  const int end = len - 1;
  switch (type) {
    case PROP_BOOLEAN: {
      bool *buf = static_cast<bool *>(*buf_p);
      for (int i = 0; i < len; i++, buf++) {
        ss << fmt::format(fmt::runtime((i < end || !end) ? "{}, " : "{}"),
                          bool_as_py_string(*buf));
      }
      *buf_p = buf;
      break;
    }
    case PROP_INT: {
      int *buf = static_cast<int *>(*buf_p);
      for (int i = 0; i < len; i++, buf++) {
        ss << fmt::format(fmt::runtime((i < end || !end) ? "{}, " : "{}"), *buf);
      }
      *buf_p = buf;
      break;
    }
    case PROP_FLOAT: {
      float *buf = static_cast<float *>(*buf_p);
      for (int i = 0; i < len; i++, buf++) {
        ss << fmt::format(fmt::runtime((i < end || !end) ? "{:g}, " : "{:g}"), *buf);
      }
      *buf_p = buf;
      break;
    }
    default:
      BLI_assert_unreachable();
  }
}

static void rna_array_as_string_recursive(
    int type, void **buf_p, int totdim, const int *dim_size, std::stringstream &ss)
{
  ss << '(';
  if (totdim > 1) {
    totdim--;
    const int end = dim_size[totdim] - 1;
    for (int i = 0; i <= end; i++) {
      rna_array_as_string_recursive(type, buf_p, totdim, dim_size, ss);
      if (i < end || !end) {
        ss << ", ";
      }
    }
  }
  else {
    rna_array_as_string_elem(type, buf_p, dim_size[0], ss);
  }
  ss << ')';
}

static void rna_array_as_string(
    int type, int len, PointerRNA *ptr, PropertyRNA *prop, std::stringstream &ss)
{
  void *buf_end;
  void *buf = rna_array_as_string_alloc(type, len, ptr, prop, &buf_end);
  void *buf_step = buf;
  int totdim, dim_size[RNA_MAX_ARRAY_DIMENSION];

  totdim = RNA_property_array_dimension(ptr, prop, dim_size);

  rna_array_as_string_recursive(type, &buf_step, totdim, dim_size, ss);
  BLI_assert(buf_step == buf_end);
  MEM_freeN(buf);
}

std::string RNA_property_as_string(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index, int max_prop_length)
{
  int type = RNA_property_type(prop);
  int len = RNA_property_array_length(ptr, prop);

  std::stringstream ss;

  /* see if we can coerce into a python type - PropertyType */
  switch (type) {
    case PROP_BOOLEAN:
      if (len == 0) {
        ss << bool_as_py_string(RNA_property_boolean_get(ptr, prop));
      }
      else {
        if (index != -1) {
          ss << bool_as_py_string(RNA_property_boolean_get_index(ptr, prop, index));
        }
        else {
          rna_array_as_string(type, len, ptr, prop, ss);
        }
      }
      break;
    case PROP_INT:
      if (len == 0) {
        ss << RNA_property_int_get(ptr, prop);
      }
      else {
        if (index != -1) {
          ss << RNA_property_int_get_index(ptr, prop, index);
        }
        else {
          rna_array_as_string(type, len, ptr, prop, ss);
        }
      }
      break;
    case PROP_FLOAT:
      if (len == 0) {
        ss << fmt::format("{:g}", RNA_property_float_get(ptr, prop));
      }
      else {
        if (index != -1) {
          ss << fmt::format("{:g}", RNA_property_float_get_index(ptr, prop, index));
        }
        else {
          rna_array_as_string(type, len, ptr, prop, ss);
        }
      }
      break;
    case PROP_STRING: {
      char *buf_esc;
      char *buf;
      int length;

      length = RNA_property_string_length(ptr, prop);
      buf = MEM_malloc_arrayN<char>(size_t(length) + 1, "RNA_property_as_string");
      buf_esc = MEM_malloc_arrayN<char>(size_t(length) * 2 + 1, "RNA_property_as_string esc");
      RNA_property_string_get(ptr, prop, buf);
      BLI_str_escape(buf_esc, buf, length * 2 + 1);
      MEM_freeN(buf);
      ss << fmt::format("\"{}\"", buf_esc);
      MEM_freeN(buf_esc);
      break;
    }
    case PROP_ENUM: {
      /* string arrays don't exist */
      const char *identifier;
      int val = RNA_property_enum_get(ptr, prop);

      if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
        /* represent as a python set */
        if (val) {
          const EnumPropertyItem *item_array;
          bool free;

          ss << "{";

          RNA_property_enum_items(C, ptr, prop, &item_array, nullptr, &free);
          if (item_array) {
            const EnumPropertyItem *item = item_array;
            bool is_first = true;
            for (; item->identifier; item++) {
              if (item->identifier[0] && item->value & val) {
                ss << fmt::format(fmt::runtime(is_first ? "'{}'" : ", '{}'"), item->identifier);
                is_first = false;
              }
            }

            if (free) {
              MEM_freeN(item_array);
            }
          }

          ss << "}";
        }
        else {
          /* annoying exception, don't confuse with dictionary syntax above: {} */
          ss << "set()";
        }
      }
      else if (RNA_property_enum_identifier(C, ptr, prop, val, &identifier)) {
        ss << fmt::format("'{}'", identifier);
      }
      else {
        return "'<UNKNOWN ENUM>'";
      }
      break;
    }
    case PROP_POINTER: {
      PointerRNA tptr = RNA_property_pointer_get(ptr, prop);
      ss << RNA_pointer_as_string(C, ptr, prop, &tptr).value_or("");
      break;
    }
    case PROP_COLLECTION: {
      int i = 0;
      CollectionPropertyIterator collect_iter;
      ss << "[";

      for (RNA_property_collection_begin(ptr, prop, &collect_iter);
           (i < max_prop_length) && collect_iter.valid;
           RNA_property_collection_next(&collect_iter), i++)
      {
        PointerRNA itemptr = collect_iter.ptr;

        if (i != 0) {
          ss << ", ";
        }

        /* now get every prop of the collection */
        ss << RNA_pointer_as_string(C, ptr, prop, &itemptr).value_or("");
      }

      RNA_property_collection_end(&collect_iter);
      ss << "]";
      break;
    }
    default:
      return "'<UNKNOWN TYPE>'"; /* TODO */
  }

  return ss.str();
}

/* Function */

const char *RNA_function_identifier(FunctionRNA *func)
{
  return func->identifier;
}

const char *RNA_function_ui_description(FunctionRNA *func)
{
  return TIP_(func->description);
}

const char *RNA_function_ui_description_raw(FunctionRNA *func)
{
  return func->description;
}

int RNA_function_flag(FunctionRNA *func)
{
  return func->flag;
}

int RNA_function_defined(FunctionRNA *func)
{
  return func->call != nullptr;
}

PropertyRNA *RNA_function_get_parameter(PointerRNA * /*ptr*/, FunctionRNA *func, int index)
{
  return static_cast<PropertyRNA *>(BLI_findlink(&func->cont.properties, index));
}

PropertyRNA *RNA_function_find_parameter(PointerRNA * /*ptr*/,
                                         FunctionRNA *func,
                                         const char *identifier)
{
  PropertyRNA *parm;

  parm = static_cast<PropertyRNA *>(func->cont.properties.first);
  for (; parm; parm = parm->next) {
    if (STREQ(RNA_property_identifier(parm), identifier)) {
      break;
    }
  }

  return parm;
}

const ListBase *RNA_function_defined_parameters(FunctionRNA *func)
{
  return &func->cont.properties;
}

/* Utility */

int RNA_parameter_flag(PropertyRNA *prop)
{
  return int(rna_ensure_property(prop)->flag_parameter);
}

ParameterList *RNA_parameter_list_create(ParameterList *parms,
                                         PointerRNA * /*ptr*/,
                                         FunctionRNA *func)
{
  PointerRNA null_ptr = PointerRNA_NULL;
  void *data;
  int alloc_size = 0, size;

  parms->arg_count = 0;
  parms->ret_count = 0;

  /* allocate data */
  LISTBASE_FOREACH (PropertyRNA *, parm, &func->cont.properties) {
    alloc_size += rna_parameter_size_pad(rna_parameter_size(parm));

    if (parm->flag_parameter & PARM_OUTPUT) {
      parms->ret_count++;
    }
    else {
      parms->arg_count++;
    }
  }

  parms->data = MEM_callocN(alloc_size, "RNA_parameter_list_create");
  parms->func = func;
  parms->alloc_size = alloc_size;

  /* set default values */
  data = parms->data;

  LISTBASE_FOREACH (PropertyRNA *, parm, &func->cont.properties) {
    size = rna_parameter_size(parm);

    /* set length to 0, these need to be set later, see bpy_array.c's py_to_array */
    if (parm->flag & PROP_DYNAMIC) {
      ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(data);
      data_alloc->array_tot = 0;
      data_alloc->array = nullptr;
    }
    else if ((parm->flag_parameter & PARM_RNAPTR) && (parm->flag & PROP_THICK_WRAP)) {
      BLI_assert(parm->type == PROP_POINTER);
      new (static_cast<PointerRNA *>(data)) PointerRNA();
    }

    if (!(parm->flag_parameter & PARM_REQUIRED) && !(parm->flag & PROP_DYNAMIC)) {
      switch (parm->type) {
        case PROP_BOOLEAN:
          if (parm->arraydimension) {
            rna_property_boolean_get_default_array_values(
                &null_ptr, (BoolPropertyRNA *)parm, static_cast<bool *>(data));
          }
          else {
            memcpy(data, &((BoolPropertyRNA *)parm)->defaultvalue, size);
          }
          break;
        case PROP_INT:
          if (parm->arraydimension) {
            rna_property_int_get_default_array_values(
                &null_ptr, (IntPropertyRNA *)parm, static_cast<int *>(data));
          }
          else {
            memcpy(data, &((IntPropertyRNA *)parm)->defaultvalue, size);
          }
          break;
        case PROP_FLOAT:
          if (parm->arraydimension) {
            rna_property_float_get_default_array_values(
                &null_ptr, (FloatPropertyRNA *)parm, static_cast<float *>(data));
          }
          else {
            memcpy(data, &((FloatPropertyRNA *)parm)->defaultvalue, size);
          }
          break;
        case PROP_ENUM:
          memcpy(data, &((EnumPropertyRNA *)parm)->defaultvalue, size);
          break;
        case PROP_STRING: {
          const char *defvalue = ((StringPropertyRNA *)parm)->defaultvalue;
          if (defvalue && defvalue[0]) {
            /* Causes bug #29988, possibly this is only correct for thick wrapped
             * need to look further into it - campbell */
#if 0
            BLI_strncpy(data, defvalue, size);
#else
            memcpy(data, &defvalue, size);
#endif
          }
          break;
        }
        case PROP_POINTER:
        case PROP_COLLECTION:
          break;
      }
    }

    data = ((char *)data) + rna_parameter_size_pad(size);
  }

  return parms;
}

void RNA_parameter_list_free(ParameterList *parms)
{
  PropertyRNA *parm;

  parm = static_cast<PropertyRNA *>(parms->func->cont.properties.first);
  void *data = parms->data;
  for (; parm; parm = parm->next) {
    if (parm->type == PROP_COLLECTION) {
      CollectionVector *vector = static_cast<CollectionVector *>(data);
      vector->~CollectionVector();
    }
    else if ((parm->flag_parameter & PARM_RNAPTR) && (parm->flag & PROP_THICK_WRAP)) {
      BLI_assert(parm->type == PROP_POINTER);
      PointerRNA *ptr = static_cast<PointerRNA *>(data);
      /* #RNA_parameter_list_create ensures that 'thick wrap' PointerRNA parameters are
       * constructed. */
      ptr->~PointerRNA();
    }
    else if (parm->flag & PROP_DYNAMIC) {
      /* for dynamic arrays and strings, data is a pointer to an array */
      ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(data);
      if (data_alloc->array) {
        MEM_freeN(data_alloc->array);
      }
    }
    data = static_cast<char *>(data) + rna_parameter_size_pad(rna_parameter_size(parm));
  }

  MEM_freeN(parms->data);
  parms->data = nullptr;

  parms->func = nullptr;
}

int RNA_parameter_list_size(const ParameterList *parms)
{
  return parms->alloc_size;
}

int RNA_parameter_list_arg_count(const ParameterList *parms)
{
  return parms->arg_count;
}

int RNA_parameter_list_ret_count(const ParameterList *parms)
{
  return parms->ret_count;
}

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter)
{
  /* may be useful but unused now */
  // RNA_pointer_create_discrete(nullptr, &RNA_Function, parms->func, &iter->funcptr); /* UNUSED */

  iter->parms = parms;
  iter->parm = static_cast<PropertyRNA *>(parms->func->cont.properties.first);
  iter->valid = iter->parm != nullptr;
  iter->offset = 0;

  if (iter->valid) {
    iter->size = rna_parameter_size(iter->parm);
    iter->data = ((char *)iter->parms->data); /* +iter->offset, always 0 */
  }
}

void RNA_parameter_list_next(ParameterIterator *iter)
{
  iter->offset += rna_parameter_size_pad(iter->size);
  iter->parm = iter->parm->next;
  iter->valid = iter->parm != nullptr;

  if (iter->valid) {
    iter->size = rna_parameter_size(iter->parm);
    iter->data = (((char *)iter->parms->data) + iter->offset);
  }
}

void RNA_parameter_list_end(ParameterIterator * /*iter*/)
{
  /* nothing to do */
}

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **r_value)
{
  ParameterIterator iter;

  RNA_parameter_list_begin(parms, &iter);

  for (; iter.valid; RNA_parameter_list_next(&iter)) {
    if (iter.parm == parm) {
      break;
    }
  }

  if (iter.valid) {
    if (parm->flag & PROP_DYNAMIC) {
      /* for dynamic arrays and strings, data is a pointer to an array */
      ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(iter.data);
      *r_value = data_alloc->array;
    }
    else {
      *r_value = iter.data;
    }
  }
  else {
    *r_value = nullptr;
  }

  RNA_parameter_list_end(&iter);
}

void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **r_value)
{
  PropertyRNA *parm;

  parm = static_cast<PropertyRNA *>(parms->func->cont.properties.first);
  for (; parm; parm = parm->next) {
    if (STREQ(RNA_property_identifier(parm), identifier)) {
      break;
    }
  }

  if (parm) {
    RNA_parameter_get(parms, parm, r_value);
  }
}

void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value)
{
  ParameterIterator iter;

  RNA_parameter_list_begin(parms, &iter);

  for (; iter.valid; RNA_parameter_list_next(&iter)) {
    if (iter.parm == parm) {
      break;
    }
  }

  if (iter.valid) {
    if (parm->flag & PROP_DYNAMIC) {
      /* for dynamic arrays and strings, data is a pointer to an array */
      ParameterDynAlloc *data_alloc = static_cast<ParameterDynAlloc *>(iter.data);
      size_t size = 0;
      switch (parm->type) {
        case PROP_STRING:
          size = sizeof(char);
          break;
        case PROP_INT:
        case PROP_BOOLEAN:
          size = sizeof(int);
          break;
        case PROP_FLOAT:
          size = sizeof(float);
          break;
        default:
          break;
      }
      size *= data_alloc->array_tot;
      if (data_alloc->array) {
        MEM_freeN(data_alloc->array);
      }
      data_alloc->array = MEM_mallocN(size, __func__);
      memcpy(data_alloc->array, value, size);
    }
    else if ((parm->flag_parameter & PARM_RNAPTR) && (parm->flag & PROP_THICK_WRAP)) {
      BLI_assert(parm->type == PROP_POINTER);
      BLI_assert(iter.size == sizeof(PointerRNA));
      PointerRNA *ptr = static_cast<PointerRNA *>(iter.data);
      /* #RNA_parameter_list_create ensures that 'thick wrap' PointerRNA parameters are
       * constructed. */
      *ptr = PointerRNA(*static_cast<const PointerRNA *>(value));
    }
    else {
      memcpy(iter.data, value, iter.size);
    }
  }

  RNA_parameter_list_end(&iter);
}

void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value)
{
  PropertyRNA *parm;

  parm = static_cast<PropertyRNA *>(parms->func->cont.properties.first);
  for (; parm; parm = parm->next) {
    if (STREQ(RNA_property_identifier(parm), identifier)) {
      break;
    }
  }

  if (parm) {
    RNA_parameter_set(parms, parm, value);
  }
}

int RNA_parameter_dynamic_length_get(ParameterList *parms, PropertyRNA *parm)
{
  ParameterIterator iter;
  int len = 0;

  RNA_parameter_list_begin(parms, &iter);

  for (; iter.valid; RNA_parameter_list_next(&iter)) {
    if (iter.parm == parm) {
      break;
    }
  }

  if (iter.valid) {
    len = RNA_parameter_dynamic_length_get_data(parms, parm, iter.data);
  }

  RNA_parameter_list_end(&iter);

  return len;
}

void RNA_parameter_dynamic_length_set(ParameterList *parms, PropertyRNA *parm, int length)
{
  ParameterIterator iter;

  RNA_parameter_list_begin(parms, &iter);

  for (; iter.valid; RNA_parameter_list_next(&iter)) {
    if (iter.parm == parm) {
      break;
    }
  }

  if (iter.valid) {
    RNA_parameter_dynamic_length_set_data(parms, parm, iter.data, length);
  }

  RNA_parameter_list_end(&iter);
}

int RNA_parameter_dynamic_length_get_data(ParameterList * /*parms*/, PropertyRNA *parm, void *data)
{
  if (parm->flag & PROP_DYNAMIC) {
    return int(((ParameterDynAlloc *)data)->array_tot);
  }
  return 0;
}

void RNA_parameter_dynamic_length_set_data(ParameterList * /*parms*/,
                                           PropertyRNA *parm,
                                           void *data,
                                           int length)
{
  if (parm->flag & PROP_DYNAMIC) {
    ((ParameterDynAlloc *)data)->array_tot = intptr_t(length);
  }
}

int RNA_function_call(
    bContext *C, ReportList *reports, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
  if (func->call) {
    func->call(C, reports, ptr, parms);

    return 0;
  }

  return -1;
}

std::optional<blender::StringRefNull> RNA_translate_ui_text(
    const char *text, const char *text_ctxt, StructRNA *type, PropertyRNA *prop, int translate)
{
  return rna_translate_ui_text(text, text_ctxt, type, prop, translate);
}

bool RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index)
{
  int len;

  /* get the length of the array to work with */
  len = RNA_property_array_length(ptr, prop);

  /* get and set the default values as appropriate for the various types */
  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
      if (len) {
        if (index == -1) {
          bool *tmparray = MEM_calloc_arrayN<bool>(len, __func__);

          RNA_property_boolean_get_default_array(ptr, prop, tmparray);
          RNA_property_boolean_set_array(ptr, prop, tmparray);

          MEM_freeN(tmparray);
        }
        else {
          int value = RNA_property_boolean_get_default_index(ptr, prop, index);
          RNA_property_boolean_set_index(ptr, prop, index, value);
        }
      }
      else {
        int value = RNA_property_boolean_get_default(ptr, prop);
        RNA_property_boolean_set(ptr, prop, value);
      }
      return true;
    case PROP_INT:
      if (len) {
        if (index == -1) {
          int *tmparray = MEM_calloc_arrayN<int>(len, __func__);

          RNA_property_int_get_default_array(ptr, prop, tmparray);
          RNA_property_int_set_array(ptr, prop, tmparray);

          MEM_freeN(tmparray);
        }
        else {
          int value = RNA_property_int_get_default_index(ptr, prop, index);
          RNA_property_int_set_index(ptr, prop, index, value);
        }
      }
      else {
        int value = RNA_property_int_get_default(ptr, prop);
        RNA_property_int_set(ptr, prop, value);
      }
      return true;
    case PROP_FLOAT:
      if (len) {
        if (index == -1) {
          float *tmparray = MEM_calloc_arrayN<float>(len, __func__);

          RNA_property_float_get_default_array(ptr, prop, tmparray);
          RNA_property_float_set_array(ptr, prop, tmparray);

          MEM_freeN(tmparray);
        }
        else {
          float value = RNA_property_float_get_default_index(ptr, prop, index);
          RNA_property_float_set_index(ptr, prop, index, value);
        }
      }
      else {
        float value = RNA_property_float_get_default(ptr, prop);
        RNA_property_float_set(ptr, prop, value);
      }
      return true;
    case PROP_ENUM: {
      int value = RNA_property_enum_get_default(ptr, prop);
      RNA_property_enum_set(ptr, prop, value);
      return true;
    }

    case PROP_STRING: {
      char *value = RNA_property_string_get_default_alloc(ptr, prop, nullptr, 0, nullptr);
      RNA_property_string_set(ptr, prop, value);
      MEM_freeN(value);
      return true;
    }

    case PROP_POINTER: {
      PointerRNA value = RNA_property_pointer_get_default(ptr, prop);
      RNA_property_pointer_set(ptr, prop, value, nullptr);
      return true;
    }

    default:
      /* FIXME: are there still any cases that haven't been handled?
       * comment out "default" block to check :) */
      return false;
  }
}

bool RNA_property_assign_default(PointerRNA *ptr, PropertyRNA *prop)
{
  if (!RNA_property_is_idprop(prop) || RNA_property_array_check(prop)) {
    return false;
  }

  /* get and set the default values as appropriate for the various types */
  switch (RNA_property_type(prop)) {
    case PROP_INT: {
      int value = RNA_property_int_get(ptr, prop);
      return RNA_property_int_set_default(prop, value);
    }

    case PROP_FLOAT: {
      float value = RNA_property_float_get(ptr, prop);
      return RNA_property_float_set_default(prop, value);
    }

    default:
      return false;
  }
}

#ifdef WITH_PYTHON
extern void PyC_LineSpit();
#endif

void _RNA_warning(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  /* gcc macro adds '\n', but can't use for other compilers */
#ifndef __GNUC__
  fputc('\n', stdout);
#endif

#ifdef WITH_PYTHON
  {
    PyC_LineSpit();
  }
#endif
}

bool RNA_path_resolved_create(PointerRNA *ptr,
                              PropertyRNA *prop,
                              const int prop_index,
                              PathResolvedRNA *r_anim_rna)
{
  int array_len = RNA_property_array_length(ptr, prop);

  if ((array_len == 0) || (prop_index < array_len)) {
    r_anim_rna->ptr = *ptr;
    r_anim_rna->prop = prop;
    r_anim_rna->prop_index = array_len ? prop_index : -1;

    return true;
  }
  return false;
}

static char rna_struct_state_owner[128];
void RNA_struct_state_owner_set(const char *name)
{
  if (name) {
    STRNCPY(rna_struct_state_owner, name);
  }
  else {
    rna_struct_state_owner[0] = '\0';
  }
}

const char *RNA_struct_state_owner_get()
{
  if (rna_struct_state_owner[0]) {
    return rna_struct_state_owner;
  }
  return nullptr;
}
