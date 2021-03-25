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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edgeometry
 */

#include "BKE_attribute.h"
#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"

#include "geometry_intern.h"

/*********************** Attribute Operators ************************/

static bool geometry_attributes_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  ID *data = (ob) ? ob->data : NULL;
  return (ob && !ID_IS_LINKED(ob) && data && !ID_IS_LINKED(data)) &&
         BKE_id_attributes_supported(data);
}

static const EnumPropertyItem *geometry_attribute_domain_itemf(bContext *C,
                                                               PointerRNA *UNUSED(ptr),
                                                               PropertyRNA *UNUSED(prop),
                                                               bool *r_free)
{
  Object *ob = ED_object_context(C);
  if (ob != NULL) {
    return rna_enum_attribute_domain_itemf(ob->data, r_free);
  }

  return DummyRNA_NULL_items;
}

static int geometry_attribute_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = ob->data;

  char name[MAX_NAME];
  RNA_string_get(op->ptr, "name", name);
  CustomDataType type = (CustomDataType)RNA_enum_get(op->ptr, "data_type");
  AttributeDomain domain = (AttributeDomain)RNA_enum_get(op->ptr, "domain");
  CustomDataLayer *layer = BKE_id_attribute_new(id, name, type, domain, op->reports);

  if (layer == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_id_attributes_active_set(id, layer);

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_attribute_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Geometry Attribute";
  ot->description = "Add attribute to geometry";
  ot->idname = "GEOMETRY_OT_attribute_add";

  /* api callbacks */
  ot->poll = geometry_attributes_poll;
  ot->exec = geometry_attribute_add_exec;
  ot->invoke = WM_operator_props_popup_confirm;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", "Attribute", MAX_NAME, "Name", "Name of new attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "data_type",
                      rna_enum_attribute_type_items,
                      CD_PROP_FLOAT,
                      "Data Type",
                      "Type of data stored in attribute");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "domain",
                      rna_enum_attribute_domain_items,
                      ATTR_DOMAIN_POINT,
                      "Domain",
                      "Type of element that attribute is stored on");
  RNA_def_enum_funcs(prop, geometry_attribute_domain_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int geometry_attribute_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  ID *id = ob->data;
  CustomDataLayer *layer = BKE_id_attributes_active_get(id);

  if (layer == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (!BKE_id_attribute_remove(id, layer, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);

  return OPERATOR_FINISHED;
}

void GEOMETRY_OT_attribute_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Geometry Attribute";
  ot->description = "Remove attribute from geometry";
  ot->idname = "GEOMETRY_OT_attribute_remove";

  /* api callbacks */
  ot->exec = geometry_attribute_remove_exec;
  ot->poll = geometry_attributes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
