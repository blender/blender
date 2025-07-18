/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 * SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Adapted from the Blender Alembic importer implementation. */

#include "usd_reader_prim.hh"
#include "usd_reader_utils.hh"

#include "usd.hh"

#include "DNA_object_types.h"

#include <pxr/usd/usd/prim.h>

#include "BLI_assert.h"

namespace blender::io::usd {

void USDPrimReader::set_props(const bool merge_with_parent, const pxr::UsdTimeCode time)
{
  if (!prim_ || !object_) {
    return;
  }

  eUSDPropertyImportMode property_import_mode = this->import_params_.property_import_mode;

  if (property_import_mode == USD_ATTR_IMPORT_NONE) {
    return;
  }

  if (merge_with_parent) {
    /* This object represents a parent Xform merged with its child prim.
     * Set the parent prim's custom properties on the Object ID. */
    if (const pxr::UsdPrim parent_prim = prim_.GetParent()) {
      set_id_props_from_prim(&object_->id, parent_prim, property_import_mode, time);
    }
  }
  if (!object_->data) {
    /* If the object has no data, set the prim's custom properties on the object.
     * This applies to Xforms that have been converted to Empty objects. */
    set_id_props_from_prim(&object_->id, prim_, property_import_mode, time);
  }

  if (object_->data) {
    /* If the object has data, the data represents the USD prim, so set the prim's custom
     * properties on the data directly. */
    set_id_props_from_prim(static_cast<ID *>(object_->data), prim_, property_import_mode, time);
  }
}

USDPrimReader::USDPrimReader(const pxr::UsdPrim &prim,
                             const USDImportParams &import_params,
                             const ImportSettings &settings)
    : name_(prim.GetName().GetString()),
      object_(nullptr),
      prim_(prim),
      parent_reader_(nullptr),
      import_params_(import_params),
      settings_(&settings),
      refcount_(0),
      is_in_instancer_proto_(false)
{
}

USDPrimReader::~USDPrimReader() = default;

const pxr::UsdPrim &USDPrimReader::prim() const
{
  return prim_;
}

Object *USDPrimReader::object() const
{
  return object_;
}

void USDPrimReader::object(Object *ob)
{
  object_ = ob;
}

bool USDPrimReader::valid() const
{
  return prim_.IsValid();
}

int USDPrimReader::refcount() const
{
  return refcount_;
}

void USDPrimReader::incref()
{
  refcount_++;
}

void USDPrimReader::decref()
{
  refcount_--;
  BLI_assert(refcount_ >= 0);
}

bool USDPrimReader::is_in_proto() const
{
  return prim_ && (prim_.IsInPrototype() || is_in_instancer_proto_);
}

}  // namespace blender::io::usd
