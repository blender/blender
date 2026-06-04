/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Material pipeline definitions.
 * Due to combinatorial explosion, we cannot realistically include all of them here.
 * Instead, the pipelines are defined at runtime by assembling vertex and fragment functions.
 * Moreover, using these pipelines at runtime would pull all sources from all different geometry
 * types and fragment shader types for all shaders. Doing the runtime definition avoids this.
 * The other way would be to generate one file per pipeline variant, but that is just too
 * cumbersome.
 */

#pragma once

#include "eevee_geom_curves.bsl.hh"
#include "eevee_geom_mesh.bsl.hh"
#include "eevee_geom_pointcloud.bsl.hh"
#include "eevee_geom_volume.bsl.hh"
#include "eevee_geom_world.bsl.hh"
#include "eevee_surf_capture.bsl.hh"
#include "eevee_surf_deferred.bsl.hh"
#include "eevee_surf_depth.bsl.hh"
#include "eevee_surf_forward.bsl.hh"
#include "eevee_surf_hybrid.bsl.hh"
#include "eevee_surf_occupancy.bsl.hh"
#include "eevee_surf_shadow.bsl.hh"
#include "eevee_surf_volume.bsl.hh"
#include "eevee_surf_world.bsl.hh"

namespace eevee {

/* clang-format off */
PipelineGraphic eevee_surface_world_world(         geom_world,      surf_world,       PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_world_curves(        geom_curves,     surf_world,       PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_world_mesh(          geom_mesh,       surf_world,       PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_world_pointcloud(    geom_pointcloud, surf_world,       PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_world_volume(        geom_volume,     surf_world,       PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_depth_world(         geom_world,      surf_depth<true>, PipelineConstants{.use_velocity = true,  .use_transparency = true,  .use_clip_plane = true,  .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_depth_curves(        geom_curves,     surf_depth<true>, PipelineConstants{.use_velocity = true,  .use_transparency = true,  .use_clip_plane = true,  .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_depth_mesh(          geom_mesh,       surf_depth<true>, PipelineConstants{.use_velocity = true,  .use_transparency = true,  .use_clip_plane = true,  .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_depth_pointcloud(    geom_pointcloud, surf_depth<true>, PipelineConstants{.use_velocity = true,  .use_transparency = true,  .use_clip_plane = true,  .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_depth_volume(        geom_volume,     surf_depth<true>, PipelineConstants{.use_velocity = true,  .use_transparency = true,  .use_clip_plane = true,  .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_deferred_world(      geom_world,      surf_deferred,    PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true});
PipelineGraphic eevee_surface_deferred_curves(     geom_curves,     surf_deferred,    PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true});
PipelineGraphic eevee_surface_deferred_mesh(       geom_mesh,       surf_deferred,    PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true});
PipelineGraphic eevee_surface_deferred_pointcloud( geom_pointcloud, surf_deferred,    PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true});
PipelineGraphic eevee_surface_deferred_volume(     geom_volume,     surf_deferred,    PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true});
PipelineGraphic eevee_surface_hybrid_world(        geom_world,      surf_hybrid,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_hybrid_curves(       geom_curves,     surf_hybrid,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_hybrid_mesh(         geom_mesh,       surf_hybrid,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_hybrid_pointcloud(   geom_pointcloud, surf_hybrid,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_hybrid_volume(       geom_volume,     surf_hybrid,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, gbuffer::PackParameters{.gbuffer_has_reflection = true, .gbuffer_has_refraction = true, .gbuffer_has_subsurface = true, .gbuffer_has_translucent = true, .gbuffer_reflection_colorless = true, .gbuffer_refraction_colorless = true, .gbuffer_layer_max = 3, .gbuffer_simple_layout = true}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_forward_world(       geom_world,      surf_forward,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_forward_curves(      geom_curves,     surf_forward,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_forward_mesh(        geom_mesh,       surf_forward,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_forward_pointcloud(  geom_pointcloud, surf_forward,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_forward_volume(      geom_volume,     surf_forward,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3}, eevee::ShadowRenderData{.shadow_random = true});
PipelineGraphic eevee_surface_capture_world(       geom_world,      surf_capture,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_capture_curves(      geom_curves,     surf_capture,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_capture_mesh(        geom_mesh,       surf_capture,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_capture_pointcloud(  geom_pointcloud, surf_capture,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_capture_volume(      geom_volume,     surf_capture,     PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_volume_world(        geom_world,      surf_volume,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_volume_curves(       geom_curves,     surf_volume,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_volume_mesh(         geom_mesh,       surf_volume,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_volume_pointcloud(   geom_pointcloud, surf_volume,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_volume_volume(       geom_volume,     surf_volume,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_occupancy_world(     geom_world,      surf_occupancy,   PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_occupancy_curves(    geom_curves,     surf_occupancy,   PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_occupancy_mesh(      geom_mesh,       surf_occupancy,   PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_occupancy_pointcloud(geom_pointcloud, surf_occupancy,   PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
PipelineGraphic eevee_surface_occupancy_volume(    geom_volume,     surf_occupancy,   PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = false, .closure_bin_count = 3});
//PipelineGraphic eevee_surface_shadow_world(      geom_world,      surf_shadow); /* N/A */
PipelineGraphic eevee_surface_shadow_curves(       geom_curves,     surf_shadow,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = true,  .closure_bin_count = 3});
PipelineGraphic eevee_surface_shadow_mesh(         geom_mesh,       surf_shadow,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = true,  .closure_bin_count = 3});
PipelineGraphic eevee_surface_shadow_pointcloud(   geom_pointcloud, surf_shadow,      PipelineConstants{.use_velocity = false, .use_transparency = false, .use_clip_plane = false, .use_sss = true, .is_shadow_pipe = true,  .closure_bin_count = 3});
//PipelineGraphic eevee_surface_shadow_volume(     geom_volume,     surf_shadow); /* N/A */
/* clang-format on */

}  // namespace eevee
