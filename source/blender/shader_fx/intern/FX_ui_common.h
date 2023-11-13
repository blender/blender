/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "FX_shader_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct PanelType;
struct bContext;
struct uiLayout;
typedef void (*PanelDrawFn)(const bContext *, Panel *);

/**
 * Draw shaderfx error message.
 */
void shaderfx_panel_end(struct uiLayout *layout, PointerRNA *ptr);

/**
 * Gets RNA pointers for the active object and the panel's shaderfx data.
 */
struct PointerRNA *shaderfx_panel_get_property_pointers(struct Panel *panel,
                                                        struct PointerRNA *r_ob_ptr);

/**
 * Create a panel in the context's region
 */
PanelType *shaderfx_panel_register(ARegionType *region_type, ShaderFxType type, PanelDrawFn draw);

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's idname, it appends the \a name argument to the \a parent's
 * idname.
 */
struct PanelType *shaderfx_subpanel_register(struct ARegionType *region_type,
                                             const char *name,
                                             const char *label,
                                             PanelDrawFn draw_header,
                                             PanelDrawFn draw,
                                             struct PanelType *parent);

#ifdef __cplusplus
}
#endif
