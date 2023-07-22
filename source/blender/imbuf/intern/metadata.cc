/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.h"

#include "DNA_ID.h" /* ID property definitions. */

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_metadata.h"

void IMB_metadata_ensure(IDProperty **metadata)
{
  if (*metadata != nullptr) {
    return;
  }

  IDPropertyTemplate val = {0};
  *metadata = IDP_New(IDP_GROUP, &val, "metadata");
}

void IMB_metadata_free(IDProperty *metadata)
{
  if (metadata == nullptr) {
    return;
  }

  IDP_FreeProperty(metadata);
}

bool IMB_metadata_get_field(IDProperty *metadata,
                            const char *key,
                            char *value,
                            const size_t value_maxncpy)
{
  IDProperty *prop;

  if (metadata == nullptr) {
    return false;
  }

  prop = IDP_GetPropertyFromGroup(metadata, key);

  if (prop && prop->type == IDP_STRING) {
    BLI_strncpy(value, IDP_String(prop), value_maxncpy);
    return true;
  }
  return false;
}

void IMB_metadata_copy(ImBuf *dimb, ImBuf *simb)
{
  BLI_assert(dimb != simb);
  if (simb->metadata) {
    IMB_metadata_free(dimb->metadata);
    dimb->metadata = IDP_CopyProperty(simb->metadata);
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
  else if (prop == nullptr) {
    prop = IDP_NewString(value, key);
    IDP_AddToGroup(metadata, prop);
  }
}

void IMB_metadata_foreach(ImBuf *ibuf, IMBMetadataForeachCb callback, void *userdata)
{
  if (ibuf->metadata == nullptr) {
    return;
  }
  LISTBASE_FOREACH (IDProperty *, prop, &ibuf->metadata->data.group) {
    callback(prop->name, IDP_String(prop), userdata);
  }
}
