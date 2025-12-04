/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "FX_shader_types.hh"  // IWYU pragma: export

struct PointerRNA;
struct Panel;
struct ARegionType;
struct PanelType;
struct bContext;

namespace blender::ui {
struct Layout;
}  // namespace blender::ui

using PanelDrawFn = void (*)(const bContext *, Panel *);

/**
 * Draw shaderfx error message.
 */
void shaderfx_panel_end(blender::ui::Layout &layout, PointerRNA *ptr);

/**
 * Gets RNA pointers for the active object and the panel's shaderfx data.
 */
PointerRNA *shaderfx_panel_get_property_pointers(Panel *panel, PointerRNA *r_ob_ptr);

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
PanelType *shaderfx_subpanel_register(ARegionType *region_type,
                                      const char *name,
                                      const char *label,
                                      PanelDrawFn draw_header,
                                      PanelDrawFn draw,
                                      PanelType *parent);
