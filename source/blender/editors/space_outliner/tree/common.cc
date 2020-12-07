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
 *
 * Functions and helpers shared between tree-display types or other tree related code.
 */

#include "BKE_idtype.h"

#include "RNA_access.h"

#include "tree_display.hh"

/* -------------------------------------------------------------------- */
/** \name ID Helpers.
 *
 * \{ */

const char *outliner_idcode_to_plural(short idcode)
{
  const char *propname = BKE_idtype_idcode_to_name_plural(idcode);
  PropertyRNA *prop = RNA_struct_type_find_property(&RNA_BlendData, propname);
  return (prop) ? RNA_property_ui_name(prop) : "UNKNOWN";
}

/** \} */
