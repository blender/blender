/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compile shader files as C++ inside one compilation unit to lint syntax and get IDE integration.
 */

#include "eevee_bxdf_lut.bsl.hh"                     /* IWYU pragma: export */
#include "eevee_bxdf_lut_lib.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_camera_lib.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_colorspace_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_deferred_eval.bsl.hh"                /* IWYU pragma: export */
#include "eevee_deferred_thickness_amend.bsl.hh"     /* IWYU pragma: export */
#include "eevee_fast_gi.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_light_culling.bsl.hh"                /* IWYU pragma: export */
#include "eevee_light_eval.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_iter.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_shadow_setup.bsl.hh"           /* IWYU pragma: export */
#include "eevee_light_shape_display.bsl.hh"          /* IWYU pragma: export */
#include "eevee_lightprobe_display.bsl.hh"           /* IWYU pragma: export */
#include "eevee_ltc_lib.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_ltc_lut_lib.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_motion_blur.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_occupancy_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_octahedron_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_ray_denoise.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_ray_trace.bsl.hh"                    /* IWYU pragma: export */
#include "eevee_ray_types_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_reverse_z_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_shadow.bsl.hh"                       /* IWYU pragma: export */
#include "eevee_shadow_page_allocate.bsl.hh"         /* IWYU pragma: export */
#include "eevee_shadow_page_clear.bsl.hh"            /* IWYU pragma: export */
#include "eevee_shadow_page_defrag.bsl.hh"           /* IWYU pragma: export */
#include "eevee_shadow_page_free.bsl.hh"             /* IWYU pragma: export */
#include "eevee_shadow_page_mask.bsl.hh"             /* IWYU pragma: export */
#include "eevee_shadow_page_ops.bsl.hh"              /* IWYU pragma: export */
#include "eevee_shadow_tag_update.bsl.hh"            /* IWYU pragma: export */
#include "eevee_shadow_tag_usage.bsl.hh"             /* IWYU pragma: export */
#include "eevee_shadow_tag_usage_transparent.bsl.hh" /* IWYU pragma: export */
#include "eevee_shadow_tilemap_amend.bsl.hh"         /* IWYU pragma: export */
#include "eevee_shadow_tilemap_bounds.bsl.hh"        /* IWYU pragma: export */
#include "eevee_shadow_tilemap_finalize.bsl.hh"      /* IWYU pragma: export */
#include "eevee_shadow_tilemap_init.bsl.hh"          /* IWYU pragma: export */
#include "eevee_shadow_tilemap_lib.bsl.hh"           /* IWYU pragma: export */
#include "eevee_shadow_tracing.bsl.hh"               /* IWYU pragma: export */
#include "eevee_shadow_visibility.bsl.hh"            /* IWYU pragma: export */
#include "eevee_subsurface.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_surf_capture.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surf_deferred.bsl.hh"                /* IWYU pragma: export */
#include "eevee_surf_depth.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_surf_forward.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surf_hybrid.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_occupancy.bsl.hh"               /* IWYU pragma: export */
#include "eevee_surf_shadow.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_volume.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_world.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_surfel_light.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surfel_list.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_thickness_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_volume.bsl.hh"                       /* IWYU pragma: export */
#include "eevee_volume_lib.bsl.hh"                   /* IWYU pragma: export */

void main() {}
