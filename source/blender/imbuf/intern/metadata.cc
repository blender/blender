/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>
#include <cstring>
#include <utility>

#include "BLI_implicit_sharing.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_listbase.hh"
#include "BLI_string.hh"

#include "BKE_idprop.hh"

#include "DNA_ID.h" /* ID property definitions. */

#include "IMB_imbuf_types.hh"

#include "IMB_metadata.hh"

namespace blender {

/** Implicit sharing wrapper for IDProperty. */
class MetaDataImplicitSharing : public ImplicitSharingInfo {
 public:
  IDProperty *idprop;

  MetaDataImplicitSharing(IDProperty *idprop) : idprop(idprop)
  {
    BLI_assert(idprop != nullptr);
  }

 private:
  void delete_self_with_data() override
  {
    IDP_FreeProperty(idprop);
    MEM_delete(this);
  }
};

void IMB_metadata_ensure(IDProperty **metadata)
{
  if (*metadata != nullptr) {
    return;
  }

  *metadata = bke::idprop::create_group("metadata").release();
}

const IDProperty *ImBuf::metadata() const
{
  return this->metadata_ptr;
}

IDProperty *ImBuf::metadata_for_write()
{
  BLI_assert((this->metadata_ptr == nullptr) == (this->metadata_sharing_info == nullptr));

  if (this->metadata_ptr == nullptr) {
    /* Allocate on demand. */
    this->metadata_ptr = bke::idprop::create_group("metadata").release();
    this->metadata_sharing_info = ImplicitSharingPtr<>(
        MEM_new<MetaDataImplicitSharing>(__func__, this->metadata_ptr));
  }
  else if (!this->metadata_sharing_info->is_mutable()) {
    /* Copy on write. */
    this->metadata_ptr = IDP_CopyProperty(this->metadata_ptr);
    this->metadata_sharing_info = ImplicitSharingPtr<>(
        MEM_new<MetaDataImplicitSharing>(__func__, this->metadata_ptr));
  }

  return this->metadata_ptr;
}

void ImBuf::assign_metadata(const IDProperty *metadata, ImplicitSharingPtr<> sharing_info)
{
  BLI_assert(metadata != nullptr);
  BLI_assert(sharing_info.get() != nullptr);

  this->metadata_ptr = const_cast<IDProperty *>(metadata);
  this->metadata_sharing_info = std::move(sharing_info);
}

void IMB_metadata_free(IDProperty *metadata)
{
  if (metadata == nullptr) {
    return;
  }

  IDP_FreeProperty(metadata);
}

bool IMB_metadata_get_field(const IDProperty *metadata,
                            const char *key,
                            char *value,
                            const size_t value_maxncpy)
{
  if (metadata == nullptr) {
    return false;
  }

  IDProperty *prop = IDP_GetPropertyFromGroup(metadata, key);

  if (prop && prop->type == IDP_STRING) {
    BLI_strncpy(value, IDP_string_get(prop), value_maxncpy);
    return true;
  }
  return false;
}

void IMB_metadata_copy(ImBuf *ibuf_dst, const ImBuf *ibuf_src)
{
  BLI_assert(ibuf_dst != ibuf_src);
  ibuf_dst->metadata_ptr = ibuf_src->metadata_ptr;
  ibuf_dst->metadata_sharing_info = ibuf_src->metadata_sharing_info;
}

void IMB_metadata_set_field(IDProperty *metadata, const char *key, const char *value)
{
  BLI_assert(metadata);
  IDProperty *prop = IDP_GetPropertyFromGroup(metadata, key);

  if (prop != nullptr && prop->type != IDP_STRING) {
    IDP_FreeFromGroup(metadata, prop);
    prop = nullptr;
  }

  if (prop) {
    IDP_AssignString(prop, value);
  }
  else {
    prop = bke::idprop::create(key, value).release();
    IDP_AddToGroup(metadata, prop);
  }
}

void IMB_metadata_foreach(const ImBuf *ibuf, IMBMetadataForeachCb callback, void *userdata)
{
  const IDProperty *metadata = ibuf->metadata();
  if (metadata == nullptr) {
    return;
  }
  for (const IDProperty &prop : metadata->data.group) {
    callback(prop.name, IDP_string_get(&prop), userdata);
  }
}

}  // namespace blender
