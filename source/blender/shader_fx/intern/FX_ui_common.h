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
