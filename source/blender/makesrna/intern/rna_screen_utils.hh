/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

struct ARegion;
struct EnumPropertyItem;

#ifdef RNA_RUNTIME

int rna_region_active_panel_category_get(ARegion *region);
void rna_region_active_panel_category_set(ARegion *region, int value);
const EnumPropertyItem *rna_region_active_panel_category_itemf(const ARegion *region,
                                                               bool *r_free);

#endif
