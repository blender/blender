/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#pragma once

struct MDeformVert;
struct Material;
struct Object;
struct bGPDlayer;
struct bGPDstroke;

/**
 * Verify if valid layer, material and pass index.
 */
bool is_stroke_affected_by_modifier(struct Object *ob,
                                    char *mlayername,
                                    struct Material *material,
                                    int mpassindex,
                                    int gpl_passindex,
                                    int minpoints,
                                    bGPDlayer *gpl,
                                    bGPDstroke *gps,
                                    bool inv1,
                                    bool inv2,
                                    bool inv3,
                                    bool inv4);

/**
 * Verify if valid vertex group *and return weight.
 */
float get_modifier_point_weight(struct MDeformVert *dvert, bool inverse, int def_nr);
