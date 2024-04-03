/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief Blender kernel freestyle line style functionality.
 */

#include <optional>
#include <string>

#include "DNA_linestyle_types.h"

#define LS_MODIFIER_TYPE_COLOR 1
#define LS_MODIFIER_TYPE_ALPHA 2
#define LS_MODIFIER_TYPE_THICKNESS 3
#define LS_MODIFIER_TYPE_GEOMETRY 4

struct ColorBand;
struct Main;
struct ViewLayer;
struct bContext;

void BKE_linestyle_init(struct FreestyleLineStyle *linestyle);
FreestyleLineStyle *BKE_linestyle_new(struct Main *bmain, const char *name);

FreestyleLineStyle *BKE_linestyle_active_from_view_layer(struct ViewLayer *view_layer);

LineStyleModifier *BKE_linestyle_color_modifier_add(FreestyleLineStyle *linestyle,
                                                    const char *name,
                                                    int type);
LineStyleModifier *BKE_linestyle_alpha_modifier_add(FreestyleLineStyle *linestyle,
                                                    const char *name,
                                                    int type);
LineStyleModifier *BKE_linestyle_thickness_modifier_add(FreestyleLineStyle *linestyle,
                                                        const char *name,
                                                        int type);
LineStyleModifier *BKE_linestyle_geometry_modifier_add(FreestyleLineStyle *linestyle,
                                                       const char *name,
                                                       int type);

LineStyleModifier *BKE_linestyle_color_modifier_copy(FreestyleLineStyle *linestyle,
                                                     const LineStyleModifier *m,
                                                     int flag);
LineStyleModifier *BKE_linestyle_alpha_modifier_copy(FreestyleLineStyle *linestyle,
                                                     const LineStyleModifier *m,
                                                     int flag);
LineStyleModifier *BKE_linestyle_thickness_modifier_copy(FreestyleLineStyle *linestyle,
                                                         const LineStyleModifier *m,
                                                         int flag);
LineStyleModifier *BKE_linestyle_geometry_modifier_copy(FreestyleLineStyle *linestyle,
                                                        const LineStyleModifier *m,
                                                        int flag);

int BKE_linestyle_color_modifier_remove(FreestyleLineStyle *linestyle,
                                        LineStyleModifier *modifier);
int BKE_linestyle_alpha_modifier_remove(FreestyleLineStyle *linestyle,
                                        LineStyleModifier *modifier);
int BKE_linestyle_thickness_modifier_remove(FreestyleLineStyle *linestyle,
                                            LineStyleModifier *modifier);
int BKE_linestyle_geometry_modifier_remove(FreestyleLineStyle *linestyle,
                                           LineStyleModifier *modifier);

/**
 * Reinsert \a modifier in modifier list with an offset of \a direction.
 * \return if position of \a modifier has changed.
 */
bool BKE_linestyle_color_modifier_move(FreestyleLineStyle *linestyle,
                                       LineStyleModifier *modifier,
                                       int direction);
bool BKE_linestyle_alpha_modifier_move(FreestyleLineStyle *linestyle,
                                       LineStyleModifier *modifier,
                                       int direction);
bool BKE_linestyle_thickness_modifier_move(FreestyleLineStyle *linestyle,
                                           LineStyleModifier *modifier,
                                           int direction);
bool BKE_linestyle_geometry_modifier_move(FreestyleLineStyle *linestyle,
                                          LineStyleModifier *modifier,
                                          int direction);

void BKE_linestyle_modifier_list_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase);
std::optional<std::string> BKE_linestyle_path_to_color_ramp(FreestyleLineStyle *linestyle,
                                                            const struct ColorBand *color_ramp);

bool BKE_linestyle_use_textures(FreestyleLineStyle *linestyle, bool use_shading_nodes);

void BKE_linestyle_default_shader(const struct bContext *C, FreestyleLineStyle *linestyle);
