/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "action_intern.hh" /* own include */

/* ******************* action editor space & buttons ************** */

/* ******************* general ******************************** */

void action_buttons_register(ARegionType * /*art*/)
{
#if 0
  PanelType *pt;

  /* TODO: AnimData / Actions List */

  pt = MEM_cnew<PanelType>("spacetype action panel properties");
  STRNCPY(pt->idname, "ACTION_PT_properties");
  STRNCPY(pt->label, N_("Active F-Curve"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_properties;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_cnew<PanelType>("spacetype action panel properties");
  STRNCPY(pt->idname, "ACTION_PT_key_properties");
  STRNCPY(pt->label, N_("Active Keyframe"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_key_properties;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype action panel modifiers");
  STRNCPY(pt->idname, "ACTION_PT_modifiers");
  STRNCPY(pt->label, N_("Modifiers"));
  STRNCPY(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_modifiers;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);
#endif
}
