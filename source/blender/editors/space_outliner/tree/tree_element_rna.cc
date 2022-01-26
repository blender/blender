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
 * \ingroup spoutliner
 */

#include <climits>
#include <iostream>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_outliner_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "../outliner_intern.hh"

#include "tree_element_rna.hh"

namespace blender::ed::outliner {

/* Don't display arrays larger, weak but index is stored as a short,
 * also the outliner isn't intended for editing such large data-sets. */
BLI_STATIC_ASSERT(sizeof(TreeElement::index) == 2, "Index is no longer short!")

/* -------------------------------------------------------------------- */
/* Common functionality (#TreeElementRNACommon Base Class) */

TreeElementRNACommon::TreeElementRNACommon(TreeElement &legacy_te, PointerRNA &rna_ptr)
    : AbstractTreeElement(legacy_te), rna_ptr_(rna_ptr)
{
  /* Create an empty tree-element. */
  if (!isRNAValid()) {
    legacy_te_.name = IFACE_("(empty)");
    return;
  }
}

bool TreeElementRNACommon::isExpandValid() const
{
  return true;
}

bool TreeElementRNACommon::isRNAValid() const
{
  return rna_ptr_.data != nullptr;
}

bool TreeElementRNACommon::expandPoll(const SpaceOutliner &) const
{
  return isRNAValid();
}

const PointerRNA &TreeElementRNACommon::getPointerRNA() const
{
  return rna_ptr_;
}

PropertyRNA *TreeElementRNACommon::getPropertyRNA() const
{
  return nullptr;
}

/* -------------------------------------------------------------------- */
/* RNA Struct */

TreeElementRNAStruct::TreeElementRNAStruct(TreeElement &legacy_te, PointerRNA &rna_ptr)
    : TreeElementRNACommon(legacy_te, rna_ptr)
{
  BLI_assert(legacy_te.store_elem->type == TSE_RNA_STRUCT);

  if (!isRNAValid()) {
    return;
  }

  legacy_te_.name = RNA_struct_name_get_alloc(&rna_ptr, nullptr, 0, nullptr);
  if (legacy_te_.name) {
    legacy_te_.flag |= TE_FREE_NAME;
  }
  else {
    legacy_te_.name = RNA_struct_ui_name(rna_ptr.type);
  }
}

void TreeElementRNAStruct::expand(SpaceOutliner &space_outliner) const
{
  TreeStoreElem &tselem = *TREESTORE(&legacy_te_);
  PointerRNA ptr = rna_ptr_;

  /* If searching don't expand RNA entries */
  if (SEARCHING_OUTLINER(&space_outliner) && BLI_strcasecmp("RNA", legacy_te_.name) == 0) {
    tselem.flag &= ~TSE_CHILDSEARCH;
  }

  PropertyRNA *iterprop = RNA_struct_iterator_property(ptr.type);
  int tot = RNA_property_collection_length(&ptr, iterprop);
  CLAMP_MAX(tot, max_index);

  TreeElementRNAProperty *parent_prop_te = legacy_te_.parent ?
                                               tree_element_cast<TreeElementRNAProperty>(
                                                   legacy_te_.parent) :
                                               nullptr;
  /* auto open these cases */
  if (!parent_prop_te || (RNA_property_type(parent_prop_te->getPropertyRNA()) == PROP_POINTER)) {
    if (!tselem.used) {
      tselem.flag &= ~TSE_CLOSED;
    }
  }

  if (TSELEM_OPEN(&tselem, &space_outliner)) {
    for (int index = 0; index < tot; index++) {
      PointerRNA propptr;
      RNA_property_collection_lookup_int(&ptr, iterprop, index, &propptr);
      if (!(RNA_property_flag(reinterpret_cast<PropertyRNA *>(propptr.data)) & PROP_HIDDEN)) {
        outliner_add_element(
            &space_outliner, &legacy_te_.subtree, &ptr, &legacy_te_, TSE_RNA_PROPERTY, index);
      }
    }
  }
  else if (tot) {
    legacy_te_.flag |= TE_LAZY_CLOSED;
  }
}

/* -------------------------------------------------------------------- */
/* RNA Property */

TreeElementRNAProperty::TreeElementRNAProperty(TreeElement &legacy_te,
                                               PointerRNA &rna_ptr,
                                               const int index)
    : TreeElementRNACommon(legacy_te, rna_ptr)
{
  BLI_assert(legacy_te.store_elem->type == TSE_RNA_PROPERTY);

  if (!isRNAValid()) {
    return;
  }

  PointerRNA propptr;
  PropertyRNA *iterprop = RNA_struct_iterator_property(rna_ptr.type);
  RNA_property_collection_lookup_int(&rna_ptr, iterprop, index, &propptr);

  PropertyRNA *prop = reinterpret_cast<PropertyRNA *>(propptr.data);

  legacy_te_.name = RNA_property_ui_name(prop);
  rna_prop_ = prop;
}

void TreeElementRNAProperty::expand(SpaceOutliner &space_outliner) const
{
  TreeStoreElem &tselem = *TREESTORE(&legacy_te_);
  PointerRNA rna_ptr = rna_ptr_;
  PropertyType proptype = RNA_property_type(rna_prop_);

  /* If searching don't expand RNA entries */
  if (SEARCHING_OUTLINER(&space_outliner) && BLI_strcasecmp("RNA", legacy_te_.name) == 0) {
    tselem.flag &= ~TSE_CHILDSEARCH;
  }

  if (proptype == PROP_POINTER) {
    PointerRNA pptr = RNA_property_pointer_get(&rna_ptr, rna_prop_);

    if (pptr.data) {
      if (TSELEM_OPEN(&tselem, &space_outliner)) {
        outliner_add_element(
            &space_outliner, &legacy_te_.subtree, &pptr, &legacy_te_, TSE_RNA_STRUCT, -1);
      }
      else {
        legacy_te_.flag |= TE_LAZY_CLOSED;
      }
    }
  }
  else if (proptype == PROP_COLLECTION) {
    int tot = RNA_property_collection_length(&rna_ptr, rna_prop_);
    CLAMP_MAX(tot, max_index);

    if (TSELEM_OPEN(&tselem, &space_outliner)) {
      for (int index = 0; index < tot; index++) {
        PointerRNA pptr;
        RNA_property_collection_lookup_int(&rna_ptr, rna_prop_, index, &pptr);
        outliner_add_element(
            &space_outliner, &legacy_te_.subtree, &pptr, &legacy_te_, TSE_RNA_STRUCT, index);
      }
    }
    else if (tot) {
      legacy_te_.flag |= TE_LAZY_CLOSED;
    }
  }
  else if (ELEM(proptype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
    int tot = RNA_property_array_length(&rna_ptr, rna_prop_);
    CLAMP_MAX(tot, max_index);

    if (TSELEM_OPEN(&tselem, &space_outliner)) {
      for (int index = 0; index < tot; index++) {
        outliner_add_element(&space_outliner,
                             &legacy_te_.subtree,
                             &rna_ptr,
                             &legacy_te_,
                             TSE_RNA_ARRAY_ELEM,
                             index);
      }
    }
    else if (tot) {
      legacy_te_.flag |= TE_LAZY_CLOSED;
    }
  }
}

PropertyRNA *TreeElementRNAProperty::getPropertyRNA() const
{
  return rna_prop_;
}

/* -------------------------------------------------------------------- */
/* RNA Array Element */

TreeElementRNAArrayElement::TreeElementRNAArrayElement(TreeElement &legacy_te,
                                                       PointerRNA &rna_ptr,
                                                       const int index)
    : TreeElementRNACommon(legacy_te, rna_ptr)
{
  BLI_assert(legacy_te.store_elem->type == TSE_RNA_ARRAY_ELEM);

  BLI_assert(legacy_te.parent && (legacy_te.parent->store_elem->type == TSE_RNA_PROPERTY));
  legacy_te_.index = index;

  char c = RNA_property_array_item_char(TreeElementRNAArrayElement::getPropertyRNA(), index);

  legacy_te_.name = reinterpret_cast<char *>(
      MEM_callocN(sizeof(char[20]), "OutlinerRNAArrayName"));
  if (c) {
    sprintf((char *)legacy_te_.name, "  %c", c);
  }
  else {
    sprintf((char *)legacy_te_.name, "  %d", index + 1);
  }
  legacy_te_.flag |= TE_FREE_NAME;
}

PropertyRNA *TreeElementRNAArrayElement::getPropertyRNA() const
{
  /* Forward query to the parent (which is expected to be a #TreeElementRNAProperty). */
  const TreeElementRNAProperty *parent_prop_te = tree_element_cast<TreeElementRNAProperty>(
      legacy_te_.parent);
  return parent_prop_te ? parent_prop_te->getPropertyRNA() : nullptr;
}

}  // namespace blender::ed::outliner
