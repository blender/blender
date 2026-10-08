/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Compile shader files as C++ inside one compilation unit to lint syntax and get IDE integration.
 */

#include "draw_aabb.bsl.hh"                    /* IWYU pragma: export */
#include "draw_colormanagement.bsl.hh"         /* IWYU pragma: export */
#include "draw_curves_interpolation.bsl.hh"    /* IWYU pragma: export */
#include "draw_curves_length_intercept.bsl.hh" /* IWYU pragma: export */
#include "draw_curves_topology.bsl.hh"         /* IWYU pragma: export */
#include "draw_debug_draw_display.bsl.hh"      /* IWYU pragma: export */
#include "draw_gsplat.bsl.hh"                  /* IWYU pragma: export */
#include "draw_gsplat_lib.bsl.hh"              /* IWYU pragma: export */
#include "draw_math_geom.bsl.hh"               /* IWYU pragma: export */
#include "draw_mesh_to_corner.bsl.hh"          /* IWYU pragma: export */
#include "draw_model.bsl.hh"                   /* IWYU pragma: export */
#include "draw_resource_finalize.bsl.hh"       /* IWYU pragma: export */
#include "draw_shape.bsl.hh"                   /* IWYU pragma: export */
#include "draw_view.bsl.hh"                    /* IWYU pragma: export */
#include "draw_view_finalize.bsl.hh"           /* IWYU pragma: export */
#include "draw_visibility.bsl.hh"              /* IWYU pragma: export */
#include "subdiv_attrib_interp.bsl.hh"         /* IWYU pragma: export */
#include "subdiv_common.bsl.hh"                /* IWYU pragma: export */
#include "subdiv_ibo_generate.bsl.hh"          /* IWYU pragma: export */
#include "subdiv_patch_evaluation.bsl.hh"      /* IWYU pragma: export */
#include "subdiv_vbo_generate.bsl.hh"          /* IWYU pragma: export */

void main() {}
