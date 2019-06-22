# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public LicenseS
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
# ***** END GPL LICENSE BLOCK *****

set(INC

)

set(INC_SYS

)

# blender and player
set(SRC

)

set(LIB
)

# Order matches "UI_icons.h", final name will be formatted: "icons{size}_{name}.dat"
set(ICON_NAMES
  question
  error
  cancel
  tria_right
  tria_down
  tria_left
  tria_up
  arrow_leftright
  plus
  disclosure_tri_right
  disclosure_tri_down
  radiobut_off
  radiobut_on
  menu_panel
  blender
  grip
  dot
  collapsemenu
  x
  duplicate
  node
  node_sel
  window
  workspace
  rightarrow_thin
  bordermove
  viewzoom
  add
  remove
  panel_close
  copy_id
  eyedropper
  auto
  checkbox_dehlt
  checkbox_hlt
  unlocked
  locked
  unpinned
  pinned
  screen_back
  rightarrow
  downarrow_hlt
  plugin
  help
  ghost_enabled
  color
  unlinked
  linked
  hand
  zoom_all
  zoom_selected
  zoom_previous
  zoom_in
  zoom_out
  driver_distance
  driver_rotational_difference
  driver_transform
  freeze
  stylus_pressure
  ghost_disabled
  file_new
  file_tick
  quit
  url
  recover_last
  three_dots
  fullscreen_enter
  fullscreen_exit
  light
  material
  texture
  anim
  world
  scene
  output
  script
  particles
  physics
  speaker
  tool_settings
  shaderfx
  fake_user_off
  fake_user_on
  view3d
  graph
  outliner
  properties
  filebrowser
  image
  info
  sequence
  text
  sound
  action
  nla
  preferences
  time
  nodetree
  console
  clip
  asset_manager
  node_compositing
  node_texture
  node_material
  object_datamode
  editmode_hlt
  uv
  vpaint_hlt
  tpaint_hlt
  wpaint_hlt
  sculptmode_hlt
  pose_hlt
  particlemode
  tracking
  tracking_backwards
  tracking_forwards
  tracking_backwards_single
  tracking_forwards_single
  tracking_clear_backwards
  tracking_clear_forwards
  tracking_refine_backwards
  tracking_refine_forwards
  scene_data
  renderlayers
  world_data
  object_data
  mesh_data
  curve_data
  meta_data
  lattice_data
  light_data
  material_data
  texture_data
  anim_data
  camera_data
  particle_data
  library_data_direct
  group
  armature_data
  community
  bone_data
  constraint
  shapekey_data
  constraint_bone
  camera_stereo
  package
  uglypackage
  experimental
  brush_data
  image_data
  file
  fcurve
  font_data
  render_result
  surface_data
  empty_data
  preset
  render_animation
  render_still
  library_data_broken
  boids
  strands
  library_data_indirect
  greasepencil
  line_data
  library_data_override
  group_bone
  group_vertex
  group_vcol
  group_uvs
  face_maps
  rna
  rna_add
  mouse_lmb
  mouse_mmb
  mouse_rmb
  mouse_move
  mouse_lmb_drag
  mouse_mmb_drag
  mouse_rmb_drag
  preset_new
  decorate
  decorate_keyframe
  decorate_animate
  decorate_driver
  decorate_linked
  decorate_library_override
  decorate_unlocked
  decorate_locked
  decorate_override
  sealed
  heart
  orphan_data
  user
  system
  settings
  outliner_ob_empty
  outliner_ob_mesh
  outliner_ob_curve
  outliner_ob_lattice
  outliner_ob_meta
  outliner_ob_light
  outliner_ob_camera
  outliner_ob_armature
  outliner_ob_font
  outliner_ob_surface
  outliner_ob_speaker
  outliner_ob_force_field
  outliner_ob_group_instance
  outliner_ob_greasepencil
  outliner_ob_lightprobe
  outliner_ob_image
  restrict_color_off
  restrict_color_on
  hide_on
  hide_off
  restrict_select_on
  restrict_select_off
  restrict_render_on
  restrict_render_off
  outliner_data_empty
  outliner_data_mesh
  outliner_data_curve
  outliner_data_lattice
  outliner_data_meta
  outliner_data_light
  outliner_data_camera
  outliner_data_armature
  outliner_data_font
  outliner_data_surface
  outliner_data_speaker
  outliner_data_greasepencil
  gp_select_points
  gp_select_strokes
  gp_multiframe_editing
  gp_only_selected
  modifier_off
  modifier_on
  onionskin_off
  onionskin_on
  restrict_view_on
  restrict_view_off
  mesh_plane
  mesh_cube
  mesh_circle
  mesh_uvsphere
  mesh_icosphere
  mesh_grid
  mesh_monkey
  mesh_cylinder
  mesh_torus
  mesh_cone
  mesh_capsule
  empty_single_arrow
  light_point
  light_sun
  light_spot
  light_hemi
  light_area
  cube
  sphere
  cone
  meta_plane
  meta_cube
  meta_ball
  meta_ellipsoid
  meta_capsule
  surface_ncurve
  surface_ncircle
  surface_nsurface
  surface_ncylinder
  surface_nsphere
  surface_ntorus
  empty_axis
  stroke
  empty_arrows
  curve_bezcurve
  curve_bezcircle
  curve_ncurve
  curve_ncircle
  curve_path
  lightprobe_cubemap
  lightprobe_planar
  lightprobe_grid
  color_red
  color_green
  color_blue
  tria_right_bar
  tria_down_bar
  tria_left_bar
  tria_up_bar
  force_force
  force_wind
  force_vortex
  force_magnetic
  force_harmonic
  force_charge
  force_lennardjones
  force_texture
  force_curve
  force_boid
  force_turbulence
  force_drag
  force_smokeflow
  image_plane
  image_background
  image_reference
  node_insert_on
  node_insert_off
  node_top
  node_side
  node_corner
  align_left
  align_center
  align_right
  align_justify
  align_flush
  align_top
  align_middle
  align_bottom
  bold
  italic
  underline
  small_caps
  modifier
  mod_wave
  mod_build
  mod_decim
  mod_mirror
  mod_soft
  mod_subsurf
  hook
  mod_physics
  mod_particles
  mod_boolean
  mod_edgesplit
  mod_array
  mod_uvproject
  mod_displace
  mod_curve
  mod_lattice
  mod_tint
  mod_armature
  mod_shrinkwrap
  mod_cast
  mod_meshdeform
  mod_bevel
  mod_smooth
  mod_simpledeform
  mod_mask
  mod_cloth
  mod_explode
  mod_fluidsim
  mod_multires
  mod_smoke
  mod_solidify
  mod_screw
  mod_vertex_weight
  mod_dynamicpaint
  mod_remesh
  mod_ocean
  mod_warp
  mod_skin
  mod_triangulate
  mod_wireframe
  mod_data_transfer
  mod_normaledit
  mod_particle_instance
  mod_hue_saturation
  mod_noise
  mod_offset
  mod_simplify
  mod_thickness
  mod_instance
  mod_time
  mod_opacity
  rec
  play
  ff
  rew
  pause
  prev_keyframe
  next_keyframe
  play_reverse
  preview_range
  action_tweak
  pmarker_act
  pmarker_sel
  pmarker
  marker_hlt
  marker
  keyframe_hlt
  keyframe
  keyingset
  key_dehlt
  key_hlt
  mute_ipo_off
  mute_ipo_on
  driver
  solo_off
  solo_on
  frame_prev
  frame_next
  nla_pushdown
  ipo_constant
  ipo_linear
  ipo_bezier
  ipo_sine
  ipo_quad
  ipo_cubic
  ipo_quart
  ipo_quint
  ipo_expo
  ipo_circ
  ipo_bounce
  ipo_elastic
  ipo_back
  ipo_ease_in
  ipo_ease_out
  ipo_ease_in_out
  normalize_fcurves
  vertexsel
  edgesel
  facesel
  pivot_boundbox
  pivot_cursor
  pivot_individual
  pivot_median
  pivot_active
  center_only
  smoothcurve
  spherecurve
  rootcurve
  sharpcurve
  lincurve
  nocurve
  rndcurve
  prop_off
  prop_on
  prop_con
  sculpt_dyntopo
  particle_point
  particle_tip
  particle_path
  snap_off
  snap_on
  snap_normal
  snap_grid
  snap_vertex
  snap_edge
  snap_face
  snap_volume
  snap_increment
  sticky_uvs_loc
  sticky_uvs_disable
  sticky_uvs_vert
  clipuv_dehlt
  clipuv_hlt
  snap_peel_object
  grid
  object_origin
  orientation_global
  orientation_gimbal
  orientation_local
  orientation_normal
  orientation_view
  copydown
  pastedown
  pasteflipup
  pasteflipdown
  vis_sel_11
  vis_sel_10
  vis_sel_01
  vis_sel_00
  automerge_on
  automerge_off
  uv_vertexsel
  uv_edgesel
  uv_facesel
  uv_islandsel
  uv_sync_select
  normals_vertex
  normals_face
  normals_vertex_face
  shading_bbox
  shading_wire
  shading_solid
  shading_rendered
  shading_texture
  overlay
  xray
  lockview_off
  lockview_on
  axis_side
  axis_front
  axis_top
  ndof_dom
  ndof_turn
  ndof_fly
  ndof_trans
  layer_used
  layer_active
  sortalpha
  sortbyext
  sorttime
  sortsize
  shortdisplay
  longdisplay
  imgdisplay
  bookmarks
  fontpreview
  filter
  newfolder
  file_parent
  file_refresh
  file_folder
  file_blank
  file_blend
  file_image
  file_movie
  file_script
  file_sound
  file_font
  file_text
  sort_desc
  sort_asc
  link_blend
  append_blend
  import
  export
  loop_back
  loop_forwards
  back
  forward
  file_cache
  file_volume
  file_3d
  file_hidden
  file_backup
  disk_drive
  matplane
  matsphere
  matcube
  monkey
  hair
  aliased
  antialiased
  mat_sphere_sky
  matshaderball
  matcloth
  matfluid
  wordwrap_off
  wordwrap_on
  syntax_off
  syntax_on
  linenumbers_off
  linenumbers_on
  scriptplugins
  seq_sequencer
  seq_preview
  seq_luma_waveform
  seq_chroma_scope
  seq_histogram
  seq_splitview
  image_rgb
  image_rgb_alpha
  image_alpha
  image_zdepth
  view_perspective
  view_ortho
  view_camera
  view_pan
  view_zoom
)

# This section is maintained by the updating script, keep BEGIN/END comments.
set_property(GLOBAL PROPERTY ICON_GEOM_NAMES
  # BEGIN ICON_GEOM_NAMES
  brush.gpencil_draw.draw
  brush.gpencil_draw.erase
  brush.gpencil_draw.fill
  brush.paint_texture.airbrush
  brush.paint_texture.clone
  brush.paint_texture.draw
  brush.paint_texture.fill
  brush.paint_texture.mask
  brush.paint_texture.masklort
  brush.paint_texture.multiply
  brush.paint_texture.smear
  brush.paint_texture.soften
  brush.paint_vertex.alpha
  brush.paint_vertex.average
  brush.paint_vertex.blur
  brush.paint_vertex.draw
  brush.paint_vertex.smear
  brush.paint_weight.average
  brush.paint_weight.blur
  brush.paint_weight.draw
  brush.paint_weight.mix
  brush.paint_weight.smear
  brush.particle.add
  brush.particle.comb
  brush.particle.cut
  brush.particle.length
  brush.particle.puff
  brush.particle.smooth
  brush.particle.weight
  brush.sculpt.blob
  brush.sculpt.clay
  brush.sculpt.clay_strips
  brush.sculpt.crease
  brush.sculpt.draw
  brush.sculpt.fill
  brush.sculpt.flatten
  brush.sculpt.grab
  brush.sculpt.inflate
  brush.sculpt.layer
  brush.sculpt.mask
  brush.sculpt.nudge
  brush.sculpt.pinch
  brush.sculpt.rotate
  brush.sculpt.scrape
  brush.sculpt.simplify
  brush.sculpt.smooth
  brush.sculpt.snake_hook
  brush.sculpt.thumb
  brush.uv_sculpt.grab
  brush.uv_sculpt.pinch
  brush.uv_sculpt.relax
  none
  ops.armature.bone.roll
  ops.armature.extrude_cursor
  ops.armature.extrude_move
  ops.curve.draw
  ops.curve.extrude_cursor
  ops.curve.extrude_move
  ops.curve.radius
  ops.curve.vertex_random
  ops.generic.cursor
  ops.generic.select
  ops.generic.select_box
  ops.generic.select_circle
  ops.generic.select_lasso
  ops.gpencil.draw
  ops.gpencil.draw.eraser
  ops.gpencil.draw.line
  ops.gpencil.draw.poly
  ops.gpencil.edit_bend
  ops.gpencil.edit_mirror
  ops.gpencil.edit_shear
  ops.gpencil.edit_to_sphere
  ops.gpencil.extrude_move
  ops.gpencil.primitive_arc
  ops.gpencil.primitive_box
  ops.gpencil.primitive_circle
  ops.gpencil.primitive_curve
  ops.gpencil.primitive_line
  ops.gpencil.radius
  ops.gpencil.sculpt_clone
  ops.gpencil.sculpt_grab
  ops.gpencil.sculpt_pinch
  ops.gpencil.sculpt_push
  ops.gpencil.sculpt_randomize
  ops.gpencil.sculpt_smooth
  ops.gpencil.sculpt_strength
  ops.gpencil.sculpt_thickness
  ops.gpencil.sculpt_twist
  ops.gpencil.sculpt_weight
  ops.gpencil.stroke_cutter
  ops.mesh.bevel
  ops.mesh.bisect
  ops.mesh.dupli_extrude_cursor
  ops.mesh.extrude_faces_move
  ops.mesh.extrude_region_move
  ops.mesh.extrude_region_shrink_fatten
  ops.mesh.inset
  ops.mesh.knife_tool
  ops.mesh.loopcut_slide
  ops.mesh.offset_edge_loops_slide
  ops.mesh.polybuild_hover
  ops.mesh.primitive_cone_add_gizmo
  ops.mesh.primitive_cube_add_gizmo
  ops.mesh.primitive_cylinder_add_gizmo
  ops.mesh.primitive_grid_add_gizmo
  ops.mesh.primitive_sphere_add_gizmo
  ops.mesh.primitive_torus_add_gizmo
  ops.mesh.rip
  ops.mesh.rip_edge
  ops.mesh.spin
  ops.mesh.spin.duplicate
  ops.mesh.vertices_smooth
  ops.node.links_cut
  ops.paint.vertex_color_fill
  ops.paint.weight_fill
  ops.paint.weight_gradient
  ops.paint.weight_sample
  ops.paint.weight_sample_group
  ops.pose.breakdowner
  ops.pose.push
  ops.pose.relax
  ops.sculpt.border_hide
  ops.sculpt.border_mask
  ops.sculpt.lasso_mask
  ops.transform.bone_envelope
  ops.transform.bone_size
  ops.transform.edge_slide
  ops.transform.push_pull
  ops.transform.resize.cage
  ops.transform.resize
  ops.transform.rotate
  ops.transform.shear
  ops.transform.shrink_fatten
  ops.transform.tilt
  ops.transform.tosphere
  ops.transform.transform
  ops.transform.translate
  ops.transform.vert_slide
  ops.transform.vertex_random
  ops.view3d.ruler
  # END ICON_GEOM_NAMES
)

data_to_c_simple(../../../../release/datafiles/bfont.pfb SRC)
data_to_c_simple(../../../../release/datafiles/bfont.ttf SRC)
data_to_c_simple(../../../../release/datafiles/bmonofont.ttf SRC)

if(WITH_BLENDER)
  # blender only (not player)

  if(NOT WITH_HEADLESS)
    # blender UI only

    # blends
    data_to_c_simple(../../../../release/datafiles/preview.blend SRC)
    data_to_c_simple(../../../../release/datafiles/preview_grease_pencil.blend SRC)

    # images
    data_to_c_simple(../../../../release/datafiles/splash.png SRC)
    data_to_c_simple(../../../../release/datafiles/splash_2x.png SRC)
    # XXX These are handy, but give nasty "false changes" in svn :/
    #svg_to_png(../../../../release/datafiles/blender_icons.svg
               #../../../../release/datafiles/blender_icons16.png
               #90 SRC)

    data_to_c_simple_icons(../../../../release/datafiles/blender_icons16 "icon16_" "${ICON_NAMES}" SRC)
    #data_to_c_simple(../../../../release/datafiles/blender_icons16.png SRC)
    #svg_to_png(../../../../release/datafiles/blender_icons.svg
               #../../../../release/datafiles/blender_icons32.png
               #180 SRC)
    data_to_c_simple_icons(../../../../release/datafiles/blender_icons32 "icon32_" "${ICON_NAMES}" SRC)
    #data_to_c_simple(../../../../release/datafiles/blender_icons32.png SRC)
    #svg_to_png(../../../../release/datafiles/prvicons.svg
               #../../../../release/datafiles/prvicons.png
               #90 SRC)
    data_to_c_simple(../../../../release/datafiles/prvicons.png SRC)

    # brushes
    data_to_c_simple(../../../../release/datafiles/brushicons/blob.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/blur.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/clay.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/claystrips.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/clone.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/crease.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/draw.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/fill.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/flatten.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/grab.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/inflate.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/layer.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/mask.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/mix.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/nudge.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/pinch.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/scrape.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/smear.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/smooth.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/snake_hook.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/soften.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/texdraw.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/texfill.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/texmask.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/thumb.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/twist.png SRC)

    # grease pencil sculpt
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_smooth.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_thickness.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_strength.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_grab.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_push.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_twist.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_pinch.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_randomize.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_clone.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_weight.png SRC)

    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_pencil.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_pen.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_ink.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_inknoise.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_block.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_marker.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_fill.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_erase_soft.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_erase_hard.png SRC)
    data_to_c_simple(../../../../release/datafiles/brushicons/gp_brush_erase_stroke.png SRC)

  endif()

  data_to_c_simple(../../../../release/datafiles/startup.blend SRC)
endif()

unset(ICON_NAMES)

blender_add_lib(bf_editor_datafiles "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
