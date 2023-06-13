/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "DNA_material_types.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#else

void RNA_api_material(StructRNA *UNUSED(srna))
{
  // FunctionRNA *func;
  // PropertyRNA *parm;
}

#endif
