/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLI_listbase.hh"
#include "BLI_utildefines.hh"

#include "BKE_undo_system.hh"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.hh"

namespace blender {

#ifdef RNA_RUNTIME

static int rna_UndoStack_steps_length(PointerRNA *ptr)
{
  UndoStack *ustack = static_cast<UndoStack *>(ptr->data);
  return BLI_listbase_count(&ustack->steps);
}

static PointerRNA rna_UndoStack_steps_get(CollectionPropertyIterator *iter)
{
  UndoStep *us = static_cast<UndoStep *>(rna_iterator_listbase_get(iter));
  return RNA_pointer_create_with_parent(iter->parent, RNA_UndoStep, us);
}

static void rna_UndoStack_steps_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  UndoStack *ustack = static_cast<UndoStack *>(ptr->data);
  rna_iterator_listbase_begin(iter, ptr, &ustack->steps, nullptr);
}

static int rna_UndoStack_active_index_get(PointerRNA *ptr)
{
  UndoStack *ustack = static_cast<UndoStack *>(ptr->data);
  return BLI_findindex(&ustack->steps, ustack->step_active);
}

static PointerRNA rna_UndoStack_active_get(PointerRNA *ptr)
{
  UndoStack *ustack = static_cast<UndoStack *>(ptr->data);
  return RNA_pointer_create_with_parent(*ptr, RNA_UndoStep, ustack->step_active);
}

static void rna_UndoStep_name_get(PointerRNA *ptr, char *value)
{
  UndoStep *us = static_cast<UndoStep *>(ptr->data);
  BLI_strncpy(value, us->name, sizeof(us->name));
}

static int rna_UndoStep_name_length(PointerRNA *ptr)
{
  UndoStep *us = static_cast<UndoStep *>(ptr->data);
  return strlen(us->name);
}

static int rna_UndoStep_type_get(PointerRNA *ptr)
{
  const UndoStep *us = static_cast<const UndoStep *>(ptr->data);
  if (!us->type) {
    return 0;
  }
  int index = 0, type_index = 0;
  BKE_undosys_type_foreach([&](const UndoType *ut) {
    if (ut == us->type) {
      type_index = index;
      return false;
    }
    index++;
    return true;
  });
  return type_index;
}

static const EnumPropertyItem *rna_UndoStep_type_itemf(bContext * /*C*/,
                                                       PointerRNA * /*ptr*/,
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0, index = 0;

  BKE_undosys_type_foreach([&](const UndoType *ut) {
    EnumPropertyItem tmp = {0, "", 0, "", ""};
    tmp.value = index++;
    tmp.identifier = ut->identifier;
    tmp.name = ut->identifier;
    RNA_enum_item_add(&item, &totitem, &tmp);
    return true;
  });

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

static bool rna_UndoStep_is_substep_get(PointerRNA *ptr)
{
  UndoStep *us = static_cast<UndoStep *>(ptr->data);
  return us->skip;
}

#else /* !RNA_RUNTIME */

void RNA_def_undo(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UndoStep", nullptr);
  RNA_def_struct_ui_text(srna, "Undo Step", "A single step in the undo history");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_UndoStep_name_get", "rna_UndoStep_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Label of the undo step");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop, "rna_UndoStep_type_get", nullptr, "rna_UndoStep_type_itemf");
  RNA_def_property_ui_text(prop, "Type", "Type of the undo step");

  prop = RNA_def_property(srna, "is_substep", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_UndoStep_is_substep_get", nullptr);
  RNA_def_property_ui_text(prop,
                           "Is Substep",
                           "If true, this is a sub-step and should not be shown to the user "
                           "for undo/redo selection as it is not accessible on its own");

  srna = RNA_def_struct(brna, "UndoStack", nullptr);
  RNA_def_struct_ui_text(srna, "Undo Stack", "Read-only access to the undo stack");

  prop = RNA_def_property(srna, "steps", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "UndoStep");
  RNA_def_property_ui_text(prop, "Steps", "List of undo steps");
  RNA_def_property_collection_funcs(prop,
                                    "rna_UndoStack_steps_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_UndoStack_steps_get",
                                    "rna_UndoStack_steps_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_UndoStack_active_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Index", "Index of currently active undo step");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "UndoStep");
  RNA_def_property_pointer_funcs(prop, "rna_UndoStack_active_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active", "Currently active undo step");
}

#endif /* RNA_RUNTIME */

}  // namespace blender
