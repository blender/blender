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

#ifndef __FX_UI_COMMON_H__
#define __FX_UI_COMMON_H__

#include "FX_shader_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct bContext;
struct PanelType;
struct uiLayout;
typedef void (*PanelDrawFn)(const bContext *, Panel *);

void shaderfx_panel_end(struct uiLayout *layout, PointerRNA *ptr);

void shaderfx_panel_get_property_pointers(const bContext *C,
                                          struct Panel *panel,
                                          struct PointerRNA *r_ob_ptr,
                                          struct PointerRNA *r_ptr);

PanelType *shaderfx_panel_register(ARegionType *region_type, ShaderFxType type, PanelDrawFn draw);

struct PanelType *shaderfx_subpanel_register(struct ARegionType *region_type,
                                             const char *name,
                                             const char *label,
                                             PanelDrawFn draw_header,
                                             PanelDrawFn draw,
                                             struct PanelType *parent);

#ifdef __cplusplus
}
#endif

#endif /* __FX_UI_COMMON_H__ */
