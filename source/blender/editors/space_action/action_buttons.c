/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spaction
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "action_intern.h" /* own include */

/* ******************* action editor space & buttons ************** */

/* ******************* general ******************************** */

void action_buttons_register(ARegionType *UNUSED(art))
{
#if 0
  PanelType *pt;

  /* TODO: AnimData / Actions List */

  pt = MEM_callocN(sizeof(PanelType), "spacetype action panel properties");
  strcpy(pt->idname, "ACTION_PT_properties");
  strcpy(pt->label, N_("Active F-Curve"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_properties;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype action panel properties");
  strcpy(pt->idname, "ACTION_PT_key_properties");
  strcpy(pt->label, N_("Active Keyframe"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_key_properties;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);

  pt = MEM_callocN(sizeof(PanelType), "spacetype action panel modifiers");
  strcpy(pt->idname, "ACTION_PT_modifiers");
  strcpy(pt->label, N_("Modifiers"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw = action_anim_panel_modifiers;
  pt->poll = action_anim_panel_poll;
  BLI_addtail(&art->paneltypes, pt);
#endif
}
