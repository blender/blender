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
 * The Original Code is Copyright (C) 2005 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup imbuf
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_idprop.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_metadata.h"

#define METADATA_MAX_VALUE_LENGTH 1024

void IMB_metadata_ensure(struct IDProperty **metadata)
{
  if (*metadata != NULL) {
    return;
  }

  IDPropertyTemplate val;
  *metadata = IDP_New(IDP_GROUP, &val, "metadata");
}

void IMB_metadata_free(struct IDProperty *metadata)
{
  if (metadata == NULL) {
    return;
  }

  IDP_FreeProperty(metadata);
}

bool IMB_metadata_get_field(struct IDProperty *metadata,
                            const char *key,
                            char *field,
                            const size_t len)
{
  IDProperty *prop;

  if (metadata == NULL) {
    return false;
  }

  prop = IDP_GetPropertyFromGroup(metadata, key);

  if (prop && prop->type == IDP_STRING) {
    BLI_strncpy(field, IDP_String(prop), len);
    return true;
  }
  return false;
}

void IMB_metadata_copy(struct ImBuf *dimb, struct ImBuf *simb)
{
  BLI_assert(dimb != simb);
  if (simb->metadata) {
    IMB_metadata_free(dimb->metadata);
    dimb->metadata = IDP_CopyProperty(simb->metadata);
  }
}

void IMB_metadata_set_field(struct IDProperty *metadata, const char *key, const char *value)
{
  BLI_assert(metadata);
  IDProperty *prop = IDP_GetPropertyFromGroup(metadata, key);

  if (prop != NULL && prop->type != IDP_STRING) {
    IDP_FreeFromGroup(metadata, prop);
    prop = NULL;
  }

  if (prop == NULL) {
    prop = IDP_NewString(value, key, METADATA_MAX_VALUE_LENGTH);
    IDP_AddToGroup(metadata, prop);
  }

  IDP_AssignString(prop, value, METADATA_MAX_VALUE_LENGTH);
}

void IMB_metadata_foreach(struct ImBuf *ibuf, IMBMetadataForeachCb callback, void *userdata)
{
  if (ibuf->metadata == NULL) {
    return;
  }
  for (IDProperty *prop = ibuf->metadata->data.group.first; prop != NULL; prop = prop->next) {
    callback(prop->name, IDP_String(prop), userdata);
  }
}
