/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"

using blender::StringRefNull;

/* -------------------------------------------------------------------- */
/** \name Histogram Template
 * \{ */

void uiTemplateHistogram(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Histogram)) {
    return;
  }
  Histogram *hist = (Histogram *)cptr.data;

  if (hist->height < UI_UNIT_Y) {
    hist->height = UI_UNIT_Y;
  }
  else if (hist->height > UI_UNIT_Y * 20) {
    hist->height = UI_UNIT_Y * 20;
  }

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  uiDefBut(block, UI_BTYPE_HISTOGRAM, 0, "", 0, 0, UI_UNIT_X * 10, hist->height, hist, 0, 0, "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                short(UI_UNIT_Y * 0.3f),
                &hist->height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Waveform Template
 * \{ */

void uiTemplateWaveform(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes)) {
    return;
  }
  Scopes *scopes = (Scopes *)cptr.data;

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  if (scopes->wavefrm_height < UI_UNIT_Y) {
    scopes->wavefrm_height = UI_UNIT_Y;
  }
  else if (scopes->wavefrm_height > UI_UNIT_Y * 20) {
    scopes->wavefrm_height = UI_UNIT_Y * 20;
  }

  uiDefBut(block,
           UI_BTYPE_WAVEFORM,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           scopes->wavefrm_height,
           scopes,
           0,
           0,
           "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                short(UI_UNIT_Y * 0.3f),
                &scopes->wavefrm_height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vector-Scope Template
 * \{ */

void uiTemplateVectorscope(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop || RNA_property_type(prop) != PROP_POINTER) {
    return;
  }

  const PointerRNA cptr = RNA_property_pointer_get(ptr, prop);
  if (!cptr.data || !RNA_struct_is_a(cptr.type, &RNA_Scopes)) {
    return;
  }
  Scopes *scopes = (Scopes *)cptr.data;

  if (scopes->vecscope_height < UI_UNIT_Y) {
    scopes->vecscope_height = UI_UNIT_Y;
  }
  else if (scopes->vecscope_height > UI_UNIT_Y * 20) {
    scopes->vecscope_height = UI_UNIT_Y * 20;
  }

  uiLayout *col = uiLayoutColumn(layout, true);
  uiBlock *block = uiLayoutGetBlock(col);

  uiDefBut(block,
           UI_BTYPE_VECTORSCOPE,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           scopes->vecscope_height,
           scopes,
           0,
           0,
           "");

  /* Resize grip. */
  uiDefIconButI(block,
                UI_BTYPE_GRIP,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                short(UI_UNIT_Y * 0.3f),
                &scopes->vecscope_height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                "");
}

/** \} */
