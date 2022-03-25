/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#pragma once

struct Depsgraph;
struct GpencilModifierData;
struct MDeformVert;
struct Material;
struct Object;
struct bGPDlayer;
struct bGPDframe;
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
/**
 * Generic bake function for deformStroke.
 */
typedef void (*gpBakeCb)(struct GpencilModifierData *md_,
                         struct Depsgraph *depsgraph_,
                         struct Object *ob_,
                         struct bGPDlayer *gpl_,
                         struct bGPDframe *gpf_,
                         struct bGPDstroke *gps_);

void generic_bake_deform_stroke(struct Depsgraph *depsgraph,
                                struct GpencilModifierData *md,
                                struct Object *ob,
                                const bool retime,
                                gpBakeCb bake_cb);
