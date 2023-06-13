/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "MOD_gpencil_legacy_modifiertypes.h"

struct ARegionType;
struct PanelType;
struct bContext;
struct uiLayout;
typedef void (*PanelDrawFn)(const bContext *, Panel *);

void gpencil_modifier_masking_panel_draw(Panel *panel, bool use_material, bool use_vertex);

void gpencil_modifier_curve_header_draw(const bContext *C, Panel *panel);
void gpencil_modifier_curve_panel_draw(const bContext *C, Panel *panel);

/**
 * Draw modifier error message.
 */
void gpencil_modifier_panel_end(struct uiLayout *layout, PointerRNA *ptr);

struct PointerRNA *gpencil_modifier_panel_get_property_pointers(struct Panel *panel,
                                                                struct PointerRNA *r_ob_ptr);

/**
 * Create a panel in the context's region
 */
PanelType *gpencil_modifier_panel_register(struct ARegionType *region_type,
                                           GpencilModifierType type,
                                           PanelDrawFn draw);

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's #PanelType.idname,
 * it appends the \a name argument to the \a parent's `idname`.
 */
struct PanelType *gpencil_modifier_subpanel_register(struct ARegionType *region_type,
                                                     const char *name,
                                                     const char *label,
                                                     PanelDrawFn draw_header,
                                                     PanelDrawFn draw,
                                                     struct PanelType *parent);

#ifdef __cplusplus
}
#endif
