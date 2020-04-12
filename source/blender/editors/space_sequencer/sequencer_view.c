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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include "ED_util_imbuf.h"

#include "RNA_define.h"

#include "WM_types.h"

/* Own include. */
#include "sequencer_intern.h"

/******************** sample backdrop operator ********************/
void SEQUENCER_OT_sample(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sample Color";
  ot->idname = "SEQUENCER_OT_sample";
  ot->description = "Use mouse to sample color in current frame";

  /* Api callbacks. */
  ot->invoke = ED_imbuf_sample_invoke;
  ot->modal = ED_imbuf_sample_modal;
  ot->cancel = ED_imbuf_sample_cancel;
  ot->poll = ED_imbuf_sample_poll;

  /* Flags. */
  ot->flag = OPTYPE_BLOCKING;

  /* Not implemented. */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna, "size", 1, 1, 128, "Sample Size", "", 1, 64);
  RNA_def_property_subtype(prop, PROP_PIXEL);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}
