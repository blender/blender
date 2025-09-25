/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_idprop.hh"

#include "DNA_ID.h" /* ID property definitions. */

#include "IMB_imbuf_types.hh"

#include "IMB_metadata.hh"

void IMB_metadata_ensure(IDProperty **metadata)
{
  if (*metadata != nullptr) {
    return;
  }

  *metadata = blender::bke::idprop::create_group("metadata").release();
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
  if (ibuf_src->metadata) {
    IMB_metadata_free(ibuf_dst->metadata);
    ibuf_dst->metadata = IDP_CopyProperty(ibuf_src->metadata);
  }
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
    prop = blender::bke::idprop::create(key, value).release();
    IDP_AddToGroup(metadata, prop);
  }
}

void IMB_metadata_foreach(const ImBuf *ibuf, IMBMetadataForeachCb callback, void *userdata)
{
  if (ibuf->metadata == nullptr) {
    return;
  }
  LISTBASE_FOREACH (IDProperty *, prop, &ibuf->metadata->data.group) {
    callback(prop->name, IDP_string_get(prop), userdata);
  }
}
