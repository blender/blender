/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

/* so modifier types match their defines */
#include "MOD_modifiertypes.hh"

#include "DEG_depsgraph_build.h"

struct ARegionType;
struct Panel;
struct PanelType;
struct PointerRNA;
struct bContext;
struct uiLayout;

using PanelDrawFn = void (*)(const bContext *, Panel *);

/**
 * Helper function for modifier layouts to draw vertex group settings.
 */
void modifier_vgroup_ui(uiLayout *layout,
                        PointerRNA *ptr,
                        PointerRNA *ob_ptr,
                        const char *vgroup_prop,
                        const char *invert_vgroup_prop,
                        const char *text);

/**
 * Draw modifier error message.
 */
void modifier_panel_end(uiLayout *layout, PointerRNA *ptr);

PointerRNA *modifier_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr);

/**
 * Create a panel in the context's region
 */
PanelType *modifier_panel_register(ARegionType *region_type, ModifierType type, PanelDrawFn draw);

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's #PanelType.idname,
 * it appends the \a name argument to the \a parent's `idname`.
 */
PanelType *modifier_subpanel_register(ARegionType *region_type,
                                      const char *name,
                                      const char *label,
                                      PanelDrawFn draw_header,
                                      PanelDrawFn draw,
                                      PanelType *parent);
