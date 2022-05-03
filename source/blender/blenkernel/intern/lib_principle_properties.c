/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"

#include "BKE_lib_id.h"
#include "BKE_lib_principle_properties.h"
#include "BKE_report.h"

#include "BLO_readfile.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

static CLG_LogRef LOG = {"bke.idprincipleprops"};

IDPrincipleProperties *BKE_lib_principleprop_init(ID *id)
{
  BLI_assert(id->principle_properties == NULL);

  /* Else, generate new empty override. */
  id->principle_properties = MEM_callocN(sizeof(*id->principle_properties), __func__);

  return id->principle_properties;
}

void BKE_lib_principleprop_clear(IDPrincipleProperties *principle_props, bool UNUSED(do_id_user))
{
  LISTBASE_FOREACH_MUTABLE (IDPrincipleProperty *, pprop, &principle_props->properties) {
    BLI_assert(pprop->rna_path != NULL);
    MEM_freeN(pprop->rna_path);
    MEM_freeN(pprop);
  }
  BLI_listbase_clear(&principle_props->properties);
  principle_props->flag = 0;
}

void BKE_lib_principleprop_free(IDPrincipleProperties **principle_props, bool do_id_user)
{
  BLI_assert(*principle_props != NULL);

  BKE_lib_principleprop_clear(*principle_props, do_id_user);
  MEM_freeN(*principle_props);
  *principle_props = NULL;
}

IDPrincipleProperty *BKE_lib_principleprop_find(IDPrincipleProperties *principle_props,
                                                const char *rna_path)
{
  return BLI_findstring_ptr(
      &principle_props->properties, rna_path, offsetof(IDPrincipleProperty, rna_path));
}

IDPrincipleProperty *BKE_lib_principleprop_get(IDPrincipleProperties *principle_props,
                                               const char *rna_path,
                                               bool *r_created)
{
  IDPrincipleProperty *pprop = BKE_lib_principleprop_find(principle_props, rna_path);

  if (pprop == NULL) {
    pprop = MEM_callocN(sizeof(*pprop), __func__);
    pprop->rna_path = BLI_strdup(rna_path);
    BLI_addtail(&principle_props->properties, pprop);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return pprop;
}

void BKE_lib_principleprop_delete(IDPrincipleProperties *principle_props,
                                  IDPrincipleProperty *principle_prop)
{
  BLI_remlink(&principle_props->properties, principle_prop);
}

bool BKE_lib_principleprop_rna_property_find(struct PointerRNA *idpoin,
                                             const struct IDPrincipleProperty *principle_prop,
                                             struct PointerRNA *r_principle_poin,
                                             struct PropertyRNA **r_principle_prop)
{
  BLI_assert(RNA_struct_is_ID(idpoin->type));
  return RNA_path_resolve_property(
      idpoin, principle_prop->rna_path, r_principle_poin, r_principle_prop);
}
