/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#else

void RNA_api_material(StructRNA * /*srna*/)
{
  // FunctionRNA *func;
  // PropertyRNA *parm;
}

#endif
