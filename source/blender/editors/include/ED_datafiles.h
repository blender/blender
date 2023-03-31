/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Datafiles embedded in Blender */

extern int datatoc_startup_blend_size;
extern const char datatoc_startup_blend[];

extern int datatoc_preview_blend_size;
extern const char datatoc_preview_blend[];

extern int datatoc_preview_grease_pencil_blend_size;
extern const char datatoc_preview_grease_pencil_blend[];

extern int datatoc_blender_icons16_png_size;
extern const char datatoc_blender_icons16_png[];

extern int datatoc_blender_icons32_png_size;
extern const char datatoc_blender_icons32_png[];

extern int datatoc_prvicons_png_size;
extern const char datatoc_prvicons_png[];

extern int datatoc_alert_icons_png_size;
extern const char datatoc_alert_icons_png[];

extern int datatoc_blender_logo_png_size;
extern const char datatoc_blender_logo_png[];

extern int datatoc_splash_png_size;
extern const char datatoc_splash_png[];

extern int datatoc_bfont_pfb_size;
extern const char datatoc_bfont_pfb[];

/* Brush icon datafiles */
/* TODO: this could be simplified by putting all
 * the brush icons in one file */
extern int datatoc_add_png_size;
extern const char datatoc_add_png[];

extern int datatoc_blob_png_size;
extern const char datatoc_blob_png[];

extern int datatoc_blur_png_size;
extern const char datatoc_blur_png[];

extern int datatoc_clay_png_size;
extern const char datatoc_clay_png[];

extern int datatoc_claystrips_png_size;
extern const char datatoc_claystrips_png[];

extern int datatoc_clone_png_size;
extern const char datatoc_clone_png[];

extern int datatoc_crease_png_size;
extern const char datatoc_crease_png[];

extern int datatoc_darken_png_size;
extern const char datatoc_darken_png[];

extern int datatoc_draw_png_size;
extern const char datatoc_draw_png[];

extern int datatoc_fill_png_size;
extern const char datatoc_fill_png[];

extern int datatoc_flatten_png_size;
extern const char datatoc_flatten_png[];

extern int datatoc_grab_png_size;
extern const char datatoc_grab_png[];

extern int datatoc_inflate_png_size;
extern const char datatoc_inflate_png[];

extern int datatoc_layer_png_size;
extern const char datatoc_layer_png[];

extern int datatoc_lighten_png_size;
extern const char datatoc_lighten_png[];

extern int datatoc_mask_png_size;
extern const char datatoc_mask_png[];

extern int datatoc_mix_png_size;
extern const char datatoc_mix_png[];

extern int datatoc_multiply_png_size;
extern const char datatoc_multiply_png[];

extern int datatoc_nudge_png_size;
extern const char datatoc_nudge_png[];

extern int datatoc_paint_select_png_size;
extern const char datatoc_paint_select_png[];

extern int datatoc_pinch_png_size;
extern const char datatoc_pinch_png[];

extern int datatoc_scrape_png_size;
extern const char datatoc_scrape_png[];

extern int datatoc_smear_png_size;
extern const char datatoc_smear_png[];

extern int datatoc_smooth_png_size;
extern const char datatoc_smooth_png[];

extern int datatoc_snake_hook_png_size;
extern const char datatoc_snake_hook_png[];

extern int datatoc_soften_png_size;
extern const char datatoc_soften_png[];

extern int datatoc_subtract_png_size;
extern const char datatoc_subtract_png[];

extern int datatoc_texdraw_png_size;
extern const char datatoc_texdraw_png[];

extern int datatoc_texfill_png_size;
extern const char datatoc_texfill_png[];

extern int datatoc_texmask_png_size;
extern const char datatoc_texmask_png[];

extern int datatoc_thumb_png_size;
extern const char datatoc_thumb_png[];

extern int datatoc_twist_png_size;
extern const char datatoc_twist_png[];

extern int datatoc_vertexdraw_png_size;
extern const char datatoc_vertexdraw_png[];

/* Matcap files */

extern int datatoc_mc01_jpg_size;
extern const char datatoc_mc01_jpg[];

extern int datatoc_mc02_jpg_size;
extern const char datatoc_mc02_jpg[];

extern int datatoc_mc03_jpg_size;
extern const char datatoc_mc03_jpg[];

extern int datatoc_mc04_jpg_size;
extern const char datatoc_mc04_jpg[];

extern int datatoc_mc05_jpg_size;
extern const char datatoc_mc05_jpg[];

extern int datatoc_mc06_jpg_size;
extern const char datatoc_mc06_jpg[];

extern int datatoc_mc07_jpg_size;
extern const char datatoc_mc07_jpg[];

extern int datatoc_mc08_jpg_size;
extern const char datatoc_mc08_jpg[];

extern int datatoc_mc09_jpg_size;
extern const char datatoc_mc09_jpg[];

extern int datatoc_mc10_jpg_size;
extern const char datatoc_mc10_jpg[];

extern int datatoc_mc11_jpg_size;
extern const char datatoc_mc11_jpg[];

extern int datatoc_mc12_jpg_size;
extern const char datatoc_mc12_jpg[];

extern int datatoc_mc13_jpg_size;
extern const char datatoc_mc13_jpg[];

extern int datatoc_mc14_jpg_size;
extern const char datatoc_mc14_jpg[];

extern int datatoc_mc15_jpg_size;
extern const char datatoc_mc15_jpg[];

extern int datatoc_mc16_jpg_size;
extern const char datatoc_mc16_jpg[];

extern int datatoc_mc17_jpg_size;
extern const char datatoc_mc17_jpg[];

extern int datatoc_mc18_jpg_size;
extern const char datatoc_mc18_jpg[];

extern int datatoc_mc19_jpg_size;
extern const char datatoc_mc19_jpg[];

extern int datatoc_mc20_jpg_size;
extern const char datatoc_mc20_jpg[];

extern int datatoc_mc21_jpg_size;
extern const char datatoc_mc21_jpg[];

extern int datatoc_mc22_jpg_size;
extern const char datatoc_mc22_jpg[];

extern int datatoc_mc23_jpg_size;
extern const char datatoc_mc23_jpg[];

extern int datatoc_mc24_jpg_size;
extern const char datatoc_mc24_jpg[];

/* grease pencil sculpt brushes files */

extern int datatoc_gp_brush_smooth_png_size;
extern const char datatoc_gp_brush_smooth_png[];

extern int datatoc_gp_brush_thickness_png_size;
extern const char datatoc_gp_brush_thickness_png[];

extern int datatoc_gp_brush_strength_png_size;
extern const char datatoc_gp_brush_strength_png[];

extern int datatoc_gp_brush_grab_png_size;
extern const char datatoc_gp_brush_grab_png[];

extern int datatoc_gp_brush_push_png_size;
extern const char datatoc_gp_brush_push_png[];

extern int datatoc_gp_brush_twist_png_size;
extern const char datatoc_gp_brush_twist_png[];

extern int datatoc_gp_brush_pinch_png_size;
extern const char datatoc_gp_brush_pinch_png[];

extern int datatoc_gp_brush_randomize_png_size;
extern const char datatoc_gp_brush_randomize_png[];

extern int datatoc_gp_brush_clone_png_size;
extern const char datatoc_gp_brush_clone_png[];

extern int datatoc_gp_brush_weight_png_size;
extern const char datatoc_gp_brush_weight_png[];

extern int datatoc_gp_brush_pencil_png_size;
extern const char datatoc_gp_brush_pencil_png[];

extern int datatoc_gp_brush_pen_png_size;
extern const char datatoc_gp_brush_pen_png[];

extern int datatoc_gp_brush_ink_png_size;
extern const char datatoc_gp_brush_ink_png[];

extern int datatoc_gp_brush_inknoise_png_size;
extern const char datatoc_gp_brush_inknoise_png[];

extern int datatoc_gp_brush_block_png_size;
extern const char datatoc_gp_brush_block_png[];

extern int datatoc_gp_brush_marker_png_size;
extern const char datatoc_gp_brush_marker_png[];

extern int datatoc_gp_brush_fill_png_size;
extern const char datatoc_gp_brush_fill_png[];

extern int datatoc_gp_brush_airbrush_png_size;
extern const char datatoc_gp_brush_airbrush_png[];

extern int datatoc_gp_brush_chisel_png_size;
extern const char datatoc_gp_brush_chisel_png[];

extern int datatoc_gp_brush_erase_soft_png_size;
extern const char datatoc_gp_brush_erase_soft_png[];

extern int datatoc_gp_brush_erase_hard_png_size;
extern const char datatoc_gp_brush_erase_hard_png[];

extern int datatoc_gp_brush_erase_stroke_png_size;
extern const char datatoc_gp_brush_erase_stroke_png[];

/* curves sculpt brushes files */

extern int datatoc_curves_sculpt_add_png_size;
extern const char datatoc_curves_sculpt_add_png[];

extern int datatoc_curves_sculpt_comb_png_size;
extern const char datatoc_curves_sculpt_comb_png[];

extern int datatoc_curves_sculpt_cut_png_size;
extern const char datatoc_curves_sculpt_cut_png[];

extern int datatoc_curves_sculpt_delete_png_size;
extern const char datatoc_curves_sculpt_delete_png[];

extern int datatoc_curves_sculpt_density_png_size;
extern const char datatoc_curves_sculpt_density_png[];

extern int datatoc_curves_sculpt_grow_shrink_png_size;
extern const char datatoc_curves_sculpt_grow_shrink_png[];

extern int datatoc_curves_sculpt_pinch_png_size;
extern const char datatoc_curves_sculpt_pinch_png[];

extern int datatoc_curves_sculpt_puff_png_size;
extern const char datatoc_curves_sculpt_puff_png[];

extern int datatoc_curves_sculpt_slide_png_size;
extern const char datatoc_curves_sculpt_slide_png[];

extern int datatoc_curves_sculpt_smooth_png_size;
extern const char datatoc_curves_sculpt_smooth_png[];

extern int datatoc_curves_sculpt_snake_hook_png_size;
extern const char datatoc_curves_sculpt_snake_hook_png[];

#ifdef __cplusplus
}
#endif
