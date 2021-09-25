/* Builtin brush channels are defined in this file.

When adding a new channel, the following other
places in rna_engine_codebase are relevent:

- brush_settings_map: use to convert to/from settings structures in Brush
- dyntopo_settings_map: same as above but for DynTopoSettings
- brush_flags_map: used to convert bitflags
- BKE_brush_builtin_patch(): adds missing channels to channelsets
- BKE_brush_builtin_create(): adds channels to brushes based on brush type (Brush->sculpt_tool)
- BKE_brush_check_toolsettings: adds missing channels to tool settings
- BKE_brush_channelset_ui_init: Configures UI visibility for channels based on brush type

*/

/* static name checking stuff */
#if defined(BRUSH_CHANNEL_DEFINE_TYPES) || defined(BRUSH_CHANNEL_DEFINE_EXTERNAL)
#  ifdef MAKE_FLOAT
#    undef MAKE_FLOAT
#  endif
#  ifdef MAKE_FLOAT_EX
#    undef MAKE_FLOAT_EX
#  endif
#  ifdef MAKE_FLOAT_EX_EX
#    undef MAKE_FLOAT_EX_EX
#  endif
#  ifdef MAKE_FLOAT_EX_INV
#    undef MAKE_FLOAT_EX_INV
#  endif
#  ifdef MAKE_FLOAT3
#    undef MAKE_FLOAT3
#  endif
#  ifdef MAKE_FLOAT3_EX
#    undef MAKE_FLOAT3_EX
#  endif
#  ifdef MAKE_INT
#    undef MAKE_INT
#  endif
#  ifdef MAKE_INT_EX
#    undef MAKE_INT_EX
#  endif
#  ifdef MAKE_COLOR3
#    undef MAKE_COLOR3
#  endif
#  ifdef MAKE_COLOR4
#    undef MAKE_COLOR4
#  endif
#  ifdef MAKE_BOOL
#    undef MAKE_BOOL
#  endif
#  ifdef MAKE_BOOL_EX
#    undef MAKE_BOOL_EX
#  endif
#  ifdef MAKE_ENUM
#    undef MAKE_ENUM
#  endif
#  ifdef MAKE_FLAGS
#    undef MAKE_FLAGS
#  endif
#  ifdef MAKE_ENUM_EX
#    undef MAKE_ENUM_EX
#  endif
#  ifdef MAKE_FLAGS_EX
#    undef MAKE_FLAGS_EX
#  endif
#  ifdef MAKE_CURVE
#    undef MAKE_CURVE
#  endif

#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif
#  ifdef MAKE_FLOAT_EX_FLAG
#    undef MAKE_FLOAT_EX_FLAG
#  endif

#  ifdef BRUSH_CHANNEL_DEFINE_TYPES
#    define MAKE_BUILTIN_CH_DEF(idname) const char *BRUSH_BUILTIN_##idname = #    idname;
#  else
#    define MAKE_BUILTIN_CH_DEF(idname) extern const char *BRUSH_BUILTIN_##idname;
#  endif

#  define MAKE_FLOAT_EX(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT_EX_FLAG( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, flag) \
    MAKE_BUILTIN_CH_DEF(idname)

#  define MAKE_FLOAT_EX_INV(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT_EX_EX( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, inv, flag) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_INT_EX(idname, name, tooltip, val, min, max, smin, smax) \
    MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_INT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_BOOL(idname, name, tooltip, val) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_BOOL_EX(idname, name, tooltip, val, flag) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_COLOR3(idname, name, tooltip, r, g, b) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_COLOR4(idname, name, tooltip, r, g, b, a) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_FLOAT3(idname, name, tooltip, x, y, z, min, max) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_FLOAT3_EX(idname, name, tooltip, x, y, z, min, max, smin, smax, flag) \
    MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1) MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, enumdef1, flag) \
    MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1) MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, enumdef1, flag1) \
    MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_CURVE(idname1, name1, tooltip1, preset1) MAKE_BUILTIN_CH_DEF(idname1);
#else
#endif

// annoying trick to pass array initializers through macro arguments
#define _(...) __VA_ARGS__

/* clang-format off */
  MAKE_FLOAT_EX(radius, "Radius", "Radius of the brush in pixels", 50.0f, 0.5f, MAX_BRUSH_PIXEL_RADIUS*10, 0.5, MAX_BRUSH_PIXEL_RADIUS, false)
  MAKE_FLOAT_EX(strength, "Strength", "How powerful the effect of the brush is when applied", 0.5f, 0.0f, 10.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(spacing, "Spacing", "", 10.0f, 0.25f, 1000.0f, 1.0f, 500.0f, false)
  MAKE_FLOAT_EX(topology_rake, "Topology Rake", "Automatically align edges to the brush direction to "
                           "generate cleaner topology and define sharp features. "
                           "Best used on low-poly meshes as it has a performance impact", 0.0f, 0.0f, 5.0f, 0.0f, 2.0f, false)
  MAKE_FLOAT_EX_INV(autosmooth, "Auto-Smooth",  "Amount of smoothing to automatically apply to each stroke", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(autosmooth_radius_scale, "Radius Scale", "Ratio between the brush radius and the radius that is going to be "
                           "used for smoothing", 1.0f, 0.001f, 5.0f, 0.01f, 2.0f, false)
  MAKE_FLOAT_EX(topology_rake_radius_scale, "Radius Scale", "Ratio between the brush radius and the radius that is going to be "
                           "used for topology rake", 1.0f, 0.001f, 5.0f, 0.01f, 2.0f, false)
  MAKE_FLOAT_EX_FLAG(dyntopo_radius_scale, "Radius Scale", "Ratio between the brush radius and the radius that is going to be "
                           "used for DynTopo", 1.0f, 0.001f, 5.0f, 0.01f, 2.0f, false, BRUSH_CHANNEL_INHERIT)
  MAKE_FLOAT_EX(projection, "Projection", "Amount of volume preserving projection", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(autosmooth_projection, "Projection", "Amount of volume preserving projection", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(topology_rake_projection, "Projection", "Amount of volume preserving projection", 0.975f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT(fset_slide, "Face Set Projection", "Stick face set boundaries to surface of mesh", 1.0f, 0.0f, 1.0f)
  MAKE_FLOAT(boundary_smooth, "Boundary Smooth", "Smooth hard boundaries", 0.0f, 0.0f, 1.0f)
  MAKE_BOOL(topology_rake_use_spacing, "Use Spacing", "Use custom spacing for topology rake", false)
  MAKE_BOOL(autosmooth_use_spacing, "Use Spacing", "Use custom spacing for autosmooth", false)
  MAKE_FLOAT_EX(topology_rake_spacing, "Spacing", "Topology rake stroke spacing", 13.0f, 0.05f, 1000.0f, 0.1f, 300.0f, false)
  MAKE_FLOAT_EX(autosmooth_spacing, "Spacing", "Autosmooth stroke spacing", 13.0f, 0.05f, 1000.0f, 0.1f, 300.0f, false)
  MAKE_ENUM(topology_rake_mode, "Topology Rake Mode", "", 1, _({
      {0, "BRUSH_DIRECTION", ICON_NONE, "Stroke", "Stroke Direction"},
      {1, "CURVATURE", ICON_NONE, "Curvature", "Follow mesh curvature"},
      {-1},
   }))

  MAKE_FLAGS_EX(automasking, "Automasking", "", 0, _({
         {BRUSH_AUTOMASKING_BOUNDARY_EDGES, "BOUNDARY_EDGE", ICON_NONE, "Boundary Edges", ""},
         {BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS, "BOUNDARY_FACE_SETS", ICON_NONE, "Boundary Face Sets", ""},
         {BRUSH_AUTOMASKING_CONCAVITY, "CONCAVITY", ICON_NONE, "Cavity", ""},
         {BRUSH_AUTOMASKING_INVERT_CONCAVITY, "INVERT_CONCAVITY", ICON_NONE, "Invert Cavity", "Invert Cavity Map"},
         {BRUSH_AUTOMASKING_FACE_SETS, "FACE_SETS", ICON_NONE, "Face Sets", ""},
         {BRUSH_AUTOMASKING_TOPOLOGY, "TOPOLOGY", ICON_NONE, "Topology", ""},
         {-1},
    }), BRUSH_CHANNEL_INHERIT_IF_UNSET)

  MAKE_BOOL_EX(dyntopo_disabled, "Disable Dyntopo", "", false, BRUSH_CHANNEL_NO_MAPPINGS)
  MAKE_FLAGS_EX(dyntopo_mode, "Dyntopo Operators", "", DYNTOPO_COLLAPSE|DYNTOPO_CLEANUP|DYNTOPO_SUBDIVIDE, _({
        {DYNTOPO_COLLAPSE, "COLLAPSE", ICON_NONE, "Collapse", ""},
        {DYNTOPO_SUBDIVIDE, "SUBDIVIDE", ICON_NONE, "Subdivide", ""},
        {DYNTOPO_CLEANUP, "CLEANUP", ICON_NONE, "Cleanup", ""},
        {DYNTOPO_LOCAL_COLLAPSE, "LOCAL_COLLAPSE", ICON_NONE, "Local Collapse", ""},
        {DYNTOPO_LOCAL_SUBDIVIDE, "LOCAL_SUBDIVIDE", ICON_NONE, "Local Subdivide", ""},
        {-1}
      }), BRUSH_CHANNEL_INHERIT)
  MAKE_ENUM(slide_deform_type, "Slide Deform Type", "", BRUSH_SLIDE_DEFORM_DRAG, _({
       {BRUSH_SLIDE_DEFORM_DRAG, "DRAG", ICON_NONE, "Drag", ""},
       {BRUSH_SLIDE_DEFORM_PINCH, "PINCH", ICON_NONE, "Pinch", ""},
       {BRUSH_SLIDE_DEFORM_EXPAND, "EXPAND", ICON_NONE, "Expand", ""},
       {-1}
    }))
  MAKE_FLOAT(normal_radius_factor, "Normal Radius", "Ratio between the brush radius and the radius that is going to be "
                            "used to sample the normal", 0.5f, 0.0f, 1.0f)
  MAKE_FLOAT(hardness, "Hardness", "Brush falloff hardness", 0.0f, 0.0f, 1.0f)
  MAKE_FLOAT(tip_roundness, "Tip Roundness", "", 0.0f, 0.0f, 1.0f)
  MAKE_BOOL(accumulate, "Accumulate", "", false)
  MAKE_ENUM(direction, "Direction", "", 0, _({
        {0, "ADD", "ADD", "Add", "Add effect of brush"},
        {1, "SUBTRACT", "REMOVE", "Subtract", "Subtract effect of brush"},
        {-1}
     }))

MAKE_FLOAT(normal_weight, "Normal Weight", "", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(weight, "Weight", "", 0.5f, 0.0f, 1.0f)
MAKE_FLOAT(jitter, "Jitter",  "Jitter the position of the brush while painting", 0.0f, 0.0f, 1.0f)
MAKE_INT(jitter_absolute, "Absolute Jitter", "", 0, 0.0f, 1000.0f)
MAKE_FLOAT(smooth_stroke_radius, "Smooth Stroke Radius", "Minimum distance from last point before stroke continues", 10.0f, 10.0f, 200.0f)
MAKE_FLOAT(smooth_stroke_factor, "Smooth Stroke Factor", "", 0.5f, 0.5f, 0.99f)
MAKE_FLOAT_EX(rate, "Rate", "", 0.5, 0.0001f, 10000.0f, 0.01f, 1.0f, false)
MAKE_FLOAT(flow, "Flow", "Amount of paint that is applied per stroke sample", 1.0f, 0.0f, 1.0f)
MAKE_FLOAT(wet_mix, "Wet Mix", "Amount of paint that is picked from the surface into the brush color", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(wet_persistence, "Wet Persistence", "Amount of wet paint that stays in the brush after applying paint to the surface", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(density, "Density", "Amount of random elements that are going to be affected by the brush", 1.0f, 0.0f, 1.0f)
MAKE_FLOAT(tip_scale_x, "Tip Scale X", "Scale of the brush tip in the X axis", 1.0f, 0.0f, 1.0f)
MAKE_FLOAT(dash_ratio, "Dash Ratio", "Ratio of samples in a cycle that the brush is enabled", 1.0f, 0.0f, 1.0f)
MAKE_FLOAT_EX(plane_offset, "Plane Offset", "Adjust plane on which the brush acts towards or away from the object surface", 0.0f, -2.0f, 2.0f, -0.5f, 0.5f, false)
MAKE_BOOL(original_normal, "Original Normal", "When locked keep using normal of surface where stroke was initiated", false)
MAKE_BOOL(original_plane, "Original Plane", "When locked keep using the plane origin of surface where stroke was initiated", false)
MAKE_BOOL(use_weighted_smooth, "Weight By Area", "Weight by face area to get a smoother result", true)
MAKE_BOOL(preserve_faceset_boundary, "Preserve Faceset Boundary", "Preserve face set boundaries", true)
MAKE_BOOL(hard_edge_mode, "Hard Edge Mode", "Forces all brushes into hard edge face set mode (sets face set slide to 0)", false)
MAKE_BOOL(grab_silhouette, "Grab Silhouette", "Grabs trying to automask the silhouette of the object", false)
MAKE_FLOAT_EX_FLAG(dyntopo_detail_percent, "Detail Percent", "Detail Percent", 25.0f, 0.0f, 1000.0f, 0.0f, 1000.0f, false, BRUSH_CHANNEL_INHERIT)
MAKE_FLOAT_EX_FLAG(dyntopo_detail_range, "Detail Range", "Detail Range", 0.45f, 0.01f, 0.99f, 0.01f, 0.99f, false, BRUSH_CHANNEL_INHERIT)
MAKE_FLOAT_EX_FLAG(dyntopo_detail_size, "Detail Size", "Detail Size", 8.0f, 0.1f, 100.0f, 0.001f, 500.0f, false, BRUSH_CHANNEL_INHERIT)
MAKE_FLOAT_EX_FLAG(dyntopo_constant_detail, "Constaint Detail", "", 3.0f, 0.001f, 1000.0f, 0.0001, FLT_MAX, false, BRUSH_CHANNEL_INHERIT)
MAKE_FLOAT_EX_FLAG(dyntopo_spacing, "Spacing", "Dyntopo Spacing", 35.0f, 0.01f, 300.0f, 0.001f, 50000.0f, false, BRUSH_CHANNEL_INHERIT)
MAKE_ENUM_EX(dyntopo_detail_mode, "Detail Mode", "", DYNTOPO_DETAIL_RELATIVE, _({
    {DYNTOPO_DETAIL_RELATIVE, "RELATIVE", "NONE", "Relative", ""},
    {DYNTOPO_DETAIL_CONSTANT, "CONSTANT", "NONE", "Constant", ""},
    {DYNTOPO_DETAIL_MANUAL, "MANUAL", "NONE", "Manual", ""},
    {DYNTOPO_DETAIL_BRUSH, "BRUSH", "NONE", "Brush", ""},
    {-1}
}), BRUSH_CHANNEL_INHERIT)

MAKE_FLOAT(concave_mask_factor, "Cavity Factor", "", 0.35f, 0.0f, 1.0f)
MAKE_INT_EX(automasking_boundary_edges_propagation_steps, "Propagation Steps",
  "Distance where boundary edge automasking is going to protect vertices "
                        "from the fully masked edge", 1, 1, 20, 1, 3)
MAKE_COLOR4(cursor_color_add, "Add Color", "Color of cursor when adding", 1.0f, 0.39f, 0.39f, 1.0f)
MAKE_COLOR4(cursor_color_sub, "Subtract Color", "Color of cursor when subtracting", 0.39f, 0.39f, 1.0f, 1.0f)
MAKE_COLOR3(color, "Color", "", 1.0f, 1.0f, 1.0f)
MAKE_COLOR3(secondary_color, "Secondary Color", "", 0.0f, 0.0f, 0.0f)
MAKE_FLOAT(vcol_boundary_factor, "Boundary Hardening", "Automatically align edges on color boundaries"
                        "to generate sharper features. ", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT_EX(vcol_boundary_exponent, "Exponent", "Hardening exponent (smaller values make smoother edges)",
                1.0, 0.001f, 6.0f, 0.001, 3.0f, false)
MAKE_FLOAT_EX(vcol_boundary_radius_scale, "Radius Scale",
  "Scale brush radius for vcol boundary hardening",
  1.0f, 0.0001f, 100.0f, 0.001f, 3.0f, false)
MAKE_FLOAT_EX(vcol_boundary_spacing, "Spacing", "Spacing for vcol boundary hardening", 15, 0.25, 5000, 0.5, 300, false)
MAKE_BOOL(invert_to_scrape_fill,"Invert to Scrape or Fill",
                        "Use Scrape or Fill tool when inverting this brush instead of "
                        "inverting its displacement direction", true)
MAKE_FLOAT(area_radius_factor, "Area Radius", "Ratio between the brush radius and the radius that is going to be "
                        "used to sample the area center", 0.5f, 0.0f, 2.0f)
MAKE_BOOL(use_multiplane_scrape_dynamic, "Dynamic Mode",  "The angle between the planes changes during the stroke to fit the "
                        "surface under the cursor", true)
MAKE_BOOL(show_multiplane_scrape_planes_preview, "Show Cursor Preview", "Preview the scrape planes in the cursor during the stroke", true)
MAKE_FLOAT(multiplane_scrape_angle, "Plane Angle", "Angle between the planes of the crease", 60.0f, 0.0f, 160.0f)

MAKE_BOOL(use_persistent, "Persistent", "Sculpt on a persistent layer of the mesh", false)
MAKE_ENUM(cloth_deform_type, "Deformation", "Deformation type that is used in the brush", BRUSH_CLOTH_DEFORM_DRAG, _({
      {BRUSH_CLOTH_DEFORM_DRAG, "DRAG", "NONE", "Drag", ""},
      {BRUSH_CLOTH_DEFORM_PUSH, "PUSH", "NONE", "Push", ""},
      {BRUSH_CLOTH_DEFORM_PINCH_POINT, "PINCH_POINT", "NONE", "Pinch Point", ""},
      {BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR, "PINCH_PERPENDICULAR", "NONE", "Pinch Perpendicular", ""},
      {BRUSH_CLOTH_DEFORM_INFLATE, "INFLATE", "NONE", "Inflate", ""},
      {BRUSH_CLOTH_DEFORM_GRAB, "GRAB", "NONE", "Grab", ""},
      {BRUSH_CLOTH_DEFORM_EXPAND, "EXPAND", "NONE", "Expand", ""},
      {BRUSH_CLOTH_DEFORM_SNAKE_HOOK, "SNAKE_HOOK", "NONE", "Snake Hook", ""},
      {BRUSH_CLOTH_DEFORM_ELASTIC_DRAG, "ELASTIC", "NONE", "Elastic Drag", ""},
      {-1}
}))

MAKE_ENUM(cloth_simulation_area_type, "Simulation Area", "Part of the mesh that is going to be simulated when the stroke is active", BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC, _({
  {BRUSH_CLOTH_SIMULATION_AREA_LOCAL,
    "LOCAL",
    "NONE",
    "Local",
    "Simulates only a specific area around the brush limited by a fixed radius"},
  {BRUSH_CLOTH_SIMULATION_AREA_GLOBAL, "GLOBAL", "NONE", "Global", "Simulates the entire mesh"},
  {BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC,
    "DYNAMIC",
    "NONE",
    "Dynamic",
    "The active simulation area moves with the brush"},
  {-1}
}))

MAKE_ENUM(cloth_force_falloff_type, "Force Falloff", "Shape used in the brush to apply force to the cloth",
  BRUSH_CLOTH_FORCE_FALLOFF_RADIAL, _({
      {BRUSH_CLOTH_FORCE_FALLOFF_RADIAL, "RADIAL", "NONE", "Radial", ""},
      {BRUSH_CLOTH_FORCE_FALLOFF_PLANE, "PLANE", "NONE", "Plane", ""},
      {-1}
}))

MAKE_FLOAT(cloth_mass, "Cloth Mass", "Mass of each simulation particle", 1.0f, 0.0f, 2.0f)
MAKE_FLOAT(cloth_damping, "Cloth Damping", "How much the applied forces are propagated through the cloth", 0.01f, 0.01f, 1.0f)
MAKE_FLOAT(cloth_sim_limit, "Simulation Limit",
      "Factor added relative to the size of the radius to limit the cloth simulation effects", 2.5f, 0.1f, 10.0f)
MAKE_FLOAT(cloth_sim_falloff, "Simulation Falloff",
                           "Area to apply deformation falloff to the effects of the simulation", 0.75f, 0.0f, 1.0f)
MAKE_FLOAT(cloth_constraint_softbody_strength,  "Soft Body Plasticity",
      "How much the cloth preserves the original shape, acting as a soft body", 0.0f, 0.0f, 1.0f)
MAKE_BOOL(cloth_use_collision,  "Enable Collision", "Collide with objects during the simulation", false)

MAKE_BOOL(use_frontface, "Use Front-Face", "Brush only affects vertexes that face the viewer", false)
MAKE_BOOL(cloth_pin_simulation_boundary, "Pin Simulation Boundary",
      "Lock the position of the vertices in the simulation falloff area to avoid artifacts and "
      "create a softer transition with unaffected areas", false)

MAKE_FLOAT(boundary_offset, "Boundary Origin Offset",
                           "Offset of the boundary origin in relation to the brush radius", 0.05f, 0.0f, 1.0f)
MAKE_ENUM(boundary_deform_type, "Deformation", "Deformation type that is used in the brush", BRUSH_BOUNDARY_DEFORM_BEND, _({
      {BRUSH_BOUNDARY_DEFORM_BEND, "BEND", "NONE", "Bend", ""},
      {BRUSH_BOUNDARY_DEFORM_EXPAND, "EXPAND", "NONE", "Expand", ""},
      {BRUSH_BOUNDARY_DEFORM_INFLATE, "INFLATE", "NONE", "Inflate", ""},
      {BRUSH_BOUNDARY_DEFORM_GRAB, "GRAB", "NONE", "Grab", ""},
      {BRUSH_BOUNDARY_DEFORM_TWIST, "TWIST", "NONE", "Twist", ""},
      {BRUSH_BOUNDARY_DEFORM_SMOOTH, "SMOOTH", "NONE", "Smooth", ""},
      {BRUSH_BOUNDARY_DEFORM_CIRCLE, "CIRCLE", "NONE", "Circle", ""},
      {-1}
}))

MAKE_ENUM(boundary_falloff_type, "Boundary Falloff", "How the brush falloff is applied across the boundary", BRUSH_BOUNDARY_FALLOFF_CONSTANT, _({
      {BRUSH_BOUNDARY_FALLOFF_CONSTANT,
       "CONSTANT",
       "NONE",
       "Constant",
       "Applies the same deformation in the entire boundary"},
      {BRUSH_BOUNDARY_FALLOFF_RADIUS,
       "RADIUS",
       "NONE",
       "Brush Radius",
       "Applies the deformation in a localized area limited by the brush radius"},
      {BRUSH_BOUNDARY_FALLOFF_LOOP,
       "LOOP",
       "NONE",
       "Loop",
       "Applies the brush falloff in a loop pattern"},
      {BRUSH_BOUNDARY_FALLOFF_LOOP_INVERT,
       "LOOP_INVERT",
       "NONE",
       "Loop and Invert",
       "Applies the falloff radius in a loop pattern, inverting the displacement direction in "
       "each pattern repetition"},
      {-1}
}))

MAKE_ENUM(deform_target, "Deformation Target", "How the deformation of the brush will affect the object", BRUSH_DEFORM_TARGET_GEOMETRY, _({
{BRUSH_DEFORM_TARGET_GEOMETRY,
    "GEOMETRY",
    "NONE",
    "Geometry",
    "Brush deformation displaces the vertices of the mesh"},
    {BRUSH_DEFORM_TARGET_CLOTH_SIM,
    "CLOTH_SIM",
    "NONE",
    "Cloth Simulation",
    "Brush deforms the mesh by deforming the constraints of a cloth simulation"},
    {-1}
}))

MAKE_CURVE(autosmooth_falloff_curve, "Falloff", "Custom curve for autosmooth", BRUSH_CURVE_SMOOTH)
MAKE_CURVE(topology_rake_falloff_curve, "Falloff", "Custom curve for topolgoy rake", BRUSH_CURVE_SMOOTH)
MAKE_CURVE(falloff_curve, "Falloff", "Falloff curve", BRUSH_CURVE_SMOOTH)
MAKE_FLOAT_EX(unprojected_radius, "Unprojected Radius", "Radius of brush in Blender units", 0.1f, 0.001, FLT_MAX, 0.001, 1.0f, false)
MAKE_ENUM_EX(radius_unit,  "Radius Unit", "Measure brush size relative to the view or the scene", 0, _({
  {0, "VIEW", "NONE", "View", "Measure brush size relative to the view"},
  {BRUSH_LOCK_SIZE, "SCENE", "NONE", "Scene", "Measure brush size relative to the scene"},
  {-1}
}), BRUSH_CHANNEL_SHOW_IN_WORKSPACE)
MAKE_FLOAT(tilt_strength_factor, "Tilt Strength", "How much the tilt of the pen will affect the brush", 0.0f, 0.0f, 1.0f)

MAKE_FLOAT_EX(rake_factor, "Rake", "How much grab will follow cursor rotation", 0.5f, 0.0f, 10.0f, 0.0f, 1.0f, false)
MAKE_FLOAT(pose_offset, "Pose Origin Offset", "Offset of the pose origin in relation to the brush radius", 0.0f, 0.0f, 2.0f)
MAKE_FLOAT(disconnected_distance_max, "Max Element Distance",
                           "Maximum distance to search for disconnected loose parts in the mesh", 0.1f, 0.0f, 10.0f)
MAKE_INT(pose_smooth_iterations,  "Smooth Iterations",
      "Smooth iterations applied after calculating the pose factor of each vertex", 4.0f, 0.0f, 100.0f)
MAKE_INT(pose_ik_segments, "Pose IK Segments",
      "Number of segments of the inverse kinematics chain that will deform the mesh", 1, 1, 20)
MAKE_FLOAT(surface_smooth_shape_preservation, "Shape Preservation", "How much of the original shape is preserved when smoothing", 0.5f, 0.0f, 1.0f)
MAKE_FLOAT(surface_smooth_current_vertex, "Per Vertex Displacement",
      "How much the position of each individual vertex influences the final result", 0.5f, 0.0f, 1.0f)
MAKE_INT(surface_smooth_iterations, "Iterations", "Number of smoothing iterations per brush step", 4, 1, 10)
MAKE_BOOL(use_connected_only,  "Connected Only", "Affect only topologically connected elements", true)
MAKE_BOOL(use_pose_ik_anchored,  "Keep Anchor Point", "Keep the position of the last segment in the IK chain fixed", true)
MAKE_BOOL(use_pose_lock_rotation,  "Lock Rotation When Scaling",
                           "Do not rotate the segment when using the scale deform mode", false)
MAKE_ENUM(pose_deform_type, "Deformation", "Deformation type that is used in the brush", 0, _({
  {BRUSH_POSE_DEFORM_ROTATE_TWIST, "ROTATE_TWIST", "NONE", "Rotate/Twist", ""},
  {BRUSH_POSE_DEFORM_SCALE_TRASLATE, "SCALE_TRANSLATE", "NONE", "Scale/Translate", ""},
  {BRUSH_POSE_DEFORM_SQUASH_STRETCH, "SQUASH_STRETCH", "NONE", "Squash & Stretch", ""},
  {BRUSH_POSE_DEFORM_BEND, "BEND", "NONE", "Bend", ""},
  {-1}
}))
MAKE_ENUM(pose_origin_type, "Rotation Origins",
                           "Method to set the rotation origins for the segments of the brush", 0, _({

  {BRUSH_POSE_ORIGIN_TOPOLOGY,
    "TOPOLOGY",
    "NONE",
    "Topology",
    "Sets the rotation origin automatically using the topology and shape of the mesh as a "
    "guide"},
  {BRUSH_POSE_ORIGIN_FACE_SETS,
    "FACE_SETS",
    "NONE",
    "Face Sets",
    "Creates a pose segment per face sets, starting from the active face set"},
  {BRUSH_POSE_ORIGIN_FACE_SETS_FK,
    "FACE_SETS_FK",
    "NONE",
    "Face Sets FK",
    "Simulates an FK deformation using the Face Set under the cursor as control"},
  {-1}
}))

MAKE_FLOAT(crease_pinch_factor, "Crease Brush Pinch Factor", "How much the crease brush pinches", 0.0f, 0.0f, 1.0f)

MAKE_ENUM(snake_hook_deform_type, "Deformation", "Deformation type that is used in the brush", BRUSH_SNAKE_HOOK_DEFORM_FALLOFF, _({
  {BRUSH_SNAKE_HOOK_DEFORM_FALLOFF,
    "FALLOFF",
    "NONE",
    "Radius Falloff",
    "Applies the brush falloff in the tip of the brush"},
  {BRUSH_SNAKE_HOOK_DEFORM_ELASTIC,
    "ELASTIC",
    "NONE",
    "Elastic",
    "Modifies the entire mesh using elastic deform"},
  {-1}
}))

/*   MTex paramters (not stored directly inside of brushes)  */
MAKE_FLOAT3(mtex_offset, "Offset", "Fine tune of the texture mapping X, Y and Z locations", 0.0f, 0.0f, 0.0f, -10.0f, 10.0f)
MAKE_FLOAT3(mtex_scale, "Size", "Set scaling for the texture's X, Y and Z sizes", 1.0f, 1.0f, 1.0f, -100.0f, 100.0f)
MAKE_FLOAT3_EX(mtex_color,  "Color", "Default color for textures that don't return RGB or when RGB to intensity is enabled", 1, 1, 1, 0, 1, -5, 5, BRUSH_CHANNEL_COLOR)
MAKE_ENUM(mtex_map_mode, "Mode", "", MTEX_MAP_MODE_TILED, _({
    {MTEX_MAP_MODE_VIEW, "VIEW_PLANE", "NONE", "View Plane", ""},
    {MTEX_MAP_MODE_AREA, "AREA_PLANE", "NONE", "Area Plane", ""},
    {MTEX_MAP_MODE_TILED, "TILED", "NONE", "Tiled", ""},
    {MTEX_MAP_MODE_3D, "3D", "NONE", "3D", ""},
    {MTEX_MAP_MODE_RANDOM, "RANDOM", "NONE", "Random", ""},
    {MTEX_MAP_MODE_STENCIL, "STENCIL", "NONE", "Stencil", ""},
    {-1}
}))
MAKE_BOOL(mtex_use_rake, "Rake", "", false)
MAKE_BOOL(mtex_use_random, "Random", "", false)
MAKE_FLOAT(mtex_random_angle, "Random Angle", "Brush texture random angle", 0.0f, 0.0f, M_PI*2.0f)
MAKE_FLOAT(mtex_angle, "Angle", "", 0.0f, 0.0f, M_PI*2.0f)

//MAKE_FLOAT3_EX
/* clang-format on */
#if defined(BRUSH_CHANNEL_DEFINE_TYPES) || defined(BRUSH_CHANNEL_DEFINE_EXTERNAL)
#  ifdef MAKE_FLOAT
#    undef MAKE_FLOAT
#  endif
#  ifdef MAKE_FLOAT_EX
#    undef MAKE_FLOAT_EX
#  endif
#  ifdef MAKE_FLOAT_EX_EX
#    undef MAKE_FLOAT_EX_EX
#  endif
#  ifdef MAKE_FLOAT_EX_INV
#    undef MAKE_FLOAT_EX_INV
#  endif

#  ifdef MAKE_INT
#    undef MAKE_INT
#  endif
#  ifdef MAKE_INT_EX
#    undef MAKE_INT_EX
#  endif
#  ifdef MAKE_COLOR3
#    undef MAKE_COLOR3
#  endif
#  ifdef MAKE_COLOR4
#    undef MAKE_COLOR4
#  endif
#  ifdef MAKE_FLOAT3
#    undef MAKE_FLOAT3
#  endif
#  ifdef MAKE_FLOAT3_EX
#    undef MAKE_FLOAT3_EX
#  endif
#  ifdef MAKE_BOOL
#    undef MAKE_BOOL
#  endif
#  ifdef MAKE_BOOL_EX
#    undef MAKE_BOOL_EX
#  endif
#  ifdef MAKE_ENUM
#    undef MAKE_ENUM
#  endif
#  ifdef MAKE_FLAGS
#    undef MAKE_FLAGS
#  endif
#  ifdef MAKE_ENUM_EX
#    undef MAKE_ENUM_EX
#  endif
#  ifdef MAKE_FLAGS_EX
#    undef MAKE_FLAGS_EX
#  endif
#  ifdef MAKE_FLOAT_EX_FLAG
#    undef MAKE_FLOAT_EX_FLAG
#  endif
#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif

#  ifdef MAKE_CURVE
#    undef MAKE_CURVE
#  endif
#endif
