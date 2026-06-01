/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compile shader files as C++ inside one compilation unit to lint syntax and get IDE integration.
 */

#include "eevee_bxdf.bsl.hh"                         /* IWYU pragma: export */
#include "eevee_bxdf_diffuse.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_bxdf_lut.bsl.hh"                     /* IWYU pragma: export */
#include "eevee_bxdf_lut_lib.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_bxdf_microfacet.bsl.hh"              /* IWYU pragma: export */
#include "eevee_bxdf_types.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_camera_lib.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_closure.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_colorspace_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_cryptomatte.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_cryptomatte_sort.bsl.hh"             /* IWYU pragma: export */
#include "eevee_debug.bsl.hh"                        /* IWYU pragma: export */
#include "eevee_deferred_combine.bsl.hh"             /* IWYU pragma: export */
#include "eevee_deferred_eval.bsl.hh"                /* IWYU pragma: export */
#include "eevee_deferred_thickness_amend.bsl.hh"     /* IWYU pragma: export */
#include "eevee_deferred_tile_classify.bsl.hh"       /* IWYU pragma: export */
#include "eevee_depth_of_field_accumulator.bsl.hh"   /* IWYU pragma: export */
#include "eevee_depth_of_field_bokeh_lut.bsl.hh"     /* IWYU pragma: export */
#include "eevee_depth_of_field_filter.bsl.hh"        /* IWYU pragma: export */
#include "eevee_depth_of_field_gather.bsl.hh"        /* IWYU pragma: export */
#include "eevee_depth_of_field_lib.bsl.hh"           /* IWYU pragma: export */
#include "eevee_depth_of_field_resolve.bsl.hh"       /* IWYU pragma: export */
#include "eevee_depth_of_field_scatter.bsl.hh"       /* IWYU pragma: export */
#include "eevee_depth_of_field_setup.bsl.hh"         /* IWYU pragma: export */
#include "eevee_depth_of_field_tiles.bsl.hh"         /* IWYU pragma: export */
#include "eevee_fast_gi.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_film.bsl.hh"                         /* IWYU pragma: export */
#include "eevee_filter.bsl.hh"                       /* IWYU pragma: export */
#include "eevee_forward_lib.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_forward_resolve.bsl.hh"              /* IWYU pragma: export */
#include "eevee_gbuffer_read.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_gbuffer_types.bsl.hh"                /* IWYU pragma: export */
#include "eevee_gbuffer_write.bsl.hh"                /* IWYU pragma: export */
#include "eevee_geom_curves.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_geom_mesh.bsl.hh"                    /* IWYU pragma: export */
#include "eevee_geom_pointcloud.bsl.hh"              /* IWYU pragma: export */
#include "eevee_geom_types_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_geom_volume.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_geom_world.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_hiz.bsl.hh"                          /* IWYU pragma: export */
#include "eevee_hiz_update.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_culling.bsl.hh"                /* IWYU pragma: export */
#include "eevee_light_data.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_eval.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_iter.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_light_lib.bsl.hh"                    /* IWYU pragma: export */
#include "eevee_light_shadow_setup.bsl.hh"           /* IWYU pragma: export */
#include "eevee_light_shape_display.bsl.hh"          /* IWYU pragma: export */
#include "eevee_lightprobe.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_lightprobe_display.bsl.hh"           /* IWYU pragma: export */
#include "eevee_lightprobe_plane.bsl.hh"             /* IWYU pragma: export */
#include "eevee_lightprobe_sphere.bsl.hh"            /* IWYU pragma: export */
#include "eevee_lightprobe_sphere_bake.bsl.hh"       /* IWYU pragma: export */
#include "eevee_lightprobe_sphere_culling.bsl.hh"    /* IWYU pragma: export */
#include "eevee_lightprobe_volume.bsl.hh"            /* IWYU pragma: export */
#include "eevee_lightprobe_volume_bake.bsl.hh"       /* IWYU pragma: export */
#include "eevee_lightprobe_volume_load.bsl.hh"       /* IWYU pragma: export */
#include "eevee_lookdev_copy_world.bsl.hh"           /* IWYU pragma: export */
#include "eevee_lookdev_display.bsl.hh"              /* IWYU pragma: export */
#include "eevee_ltc_lib.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_ltc_lut_lib.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_material_variants.bsl.hh"            /* IWYU pragma: export */
#include "eevee_motion_blur.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_nodetree_closures_lib.glsl"          /* IWYU pragma: export */
#include "eevee_nodetree_frag_lib.glsl"              /* IWYU pragma: export */
#include "eevee_nodetree_lib.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_nodetree_type_lib.glsl"              /* IWYU pragma: export */
#include "eevee_nodetree_vert_lib.glsl"              /* IWYU pragma: export */
#include "eevee_occupancy_convert.bsl.hh"            /* IWYU pragma: export */
#include "eevee_occupancy_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_octahedron_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_pipeline.bsl.hh"                     /* IWYU pragma: export */
#include "eevee_ray_denoise.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_ray_generate.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_ray_tile.bsl.hh"                     /* IWYU pragma: export */
#include "eevee_ray_trace.bsl.hh"                    /* IWYU pragma: export */
#include "eevee_ray_trace_screen_lib.bsl.hh"         /* IWYU pragma: export */
#include "eevee_ray_types_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_renderpass.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_renderpass_clear.bsl.hh"             /* IWYU pragma: export */
#include "eevee_reverse_z_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_sampling_lib.bsl.hh"                 /* IWYU pragma: export */
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
#include "eevee_spherical_harmonics.bsl.hh"          /* IWYU pragma: export */
#include "eevee_subsurface.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_subsurface_lib.bsl.hh"               /* IWYU pragma: export */
#include "eevee_surf_capture.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surf_common.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_deferred.bsl.hh"                /* IWYU pragma: export */
#include "eevee_surf_depth.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_surf_forward.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surf_hybrid.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_occupancy.bsl.hh"               /* IWYU pragma: export */
#include "eevee_surf_shadow.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_volume.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surf_world.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_surfel.bsl.hh"                       /* IWYU pragma: export */
#include "eevee_surfel_cluster.bsl.hh"               /* IWYU pragma: export */
#include "eevee_surfel_light.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_surfel_list.bsl.hh"                  /* IWYU pragma: export */
#include "eevee_surfel_ray.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_thickness_lib.bsl.hh"                /* IWYU pragma: export */
#include "eevee_transparency.bsl.hh"                 /* IWYU pragma: export */
#include "eevee_uniform.bsl.hh"                      /* IWYU pragma: export */
#include "eevee_utility_tx.bsl.hh"                   /* IWYU pragma: export */
#include "eevee_velocity.bsl.hh"                     /* IWYU pragma: export */
#include "eevee_volume.bsl.hh"                       /* IWYU pragma: export */
#include "eevee_volume_lib.bsl.hh"                   /* IWYU pragma: export */
// #include "eevee_test_fast_gi.bsl.hh"                 /* IWYU pragma: export */
// #include "eevee_test_gbuffer_closure.bsl.hh"         /* IWYU pragma: export */
// #include "eevee_test_gbuffer_normal.bsl.hh"          /* IWYU pragma: export */
// #include "eevee_test_occupancy.bsl.hh"               /* IWYU pragma: export */
// #include "eevee_test_shadow.bsl.hh"                  /* IWYU pragma: export */

void main() {}
