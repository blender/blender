/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdio>
#include <cstdlib>

#include "BLI_utildefines.h"

#include "RNA_define.hh"

#include "DNA_material_types.h"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#else

void RNA_api_material(StructRNA * /*srna*/)
{
  // FunctionRNA *func;
  // PropertyRNA *parm;
}

#endif
