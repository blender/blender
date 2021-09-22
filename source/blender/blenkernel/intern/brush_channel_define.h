// static name checking stuff
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

#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif

#  ifdef BRUSH_CHANNEL_DEFINE_TYPES
#    define MAKE_BUILTIN_CH_DEF(idname) const char *BRUSH_BUILTIN_##idname = #    idname;
#  else
#    define MAKE_BUILTIN_CH_DEF(idname) extern const char *BRUSH_BUILTIN_##idname;
#  endif

#  define MAKE_FLOAT_EX(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT_EX_INV(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT_EX_EX( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, inv) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_INT_EX(idname, name, tooltip, val, min, max, smin, smax) \
    MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_INT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_BOOL(idname, name, tooltip, val) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_BOOL_EX(idname, name, tooltip, val, flag) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_COLOR3(idname, name, tooltip, r, g, b) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_COLOR4(idname, name, tooltip, r, g, b, a) MAKE_BUILTIN_CH_DEF(idname);
#  define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1) MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, enumdef1, flag) \
    MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1) MAKE_BUILTIN_CH_DEF(idname1);
#  define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, enumdef1, flag1) \
    MAKE_BUILTIN_CH_DEF(idname1);

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
  MAKE_FLOAT_EX(dyntopo_radius_scale, "Radius Scale", "Ratio between the brush radius and the radius that is going to be "
                           "used for DynTopo", 1.0f, 0.001f, 5.0f, 0.01f, 2.0f, false)
  MAKE_FLOAT_EX(projection, "Projection", "Amount of volume preserving projection", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(autosmooth_projection, "Projection", "Amount of volume preserving projection", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT_EX(topology_rake_projection, "Projection", "Amount of volume preserving projection", 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, false)
  MAKE_FLOAT(fset_slide, "Face Set Projection", "Stick face set boundaries to surface of mesh", 1.0f, 0.0f, 1.0f)
  MAKE_FLOAT(boundary_smooth, "Boundary Smooth", "Smooth hard boundaries", 1.0f, 0.0f, 1.0f)
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
         {BRUSH_AUTOMASKING_CONCAVITY, "CONCAVITY", ICON_NONE, "Concave", ""},
         {BRUSH_AUTOMASKING_INVERT_CONCAVITY, "INVERT_CONCAVITY", ICON_NONE, "Invert Concave", "Invert Concave Map"},
         {BRUSH_AUTOMASKING_FACE_SETS, "FACE_SETS", ICON_NONE, "Face Sets", ""},
         {BRUSH_AUTOMASKING_TOPOLOGY, "TOPOLOGY", ICON_NONE, "Topology", ""},
         {-1},
    }), BRUSH_CHANNEL_INHERIT_IF_UNSET)

  MAKE_BOOL_EX(dyntopo_disabled, "Disable Dyntopo", "", false, BRUSH_CHANNEL_NO_MAPPINGS)
  MAKE_FLAGS_EX(dyntopo_mode, "Dyntopo Operators", "s", DYNTOPO_COLLAPSE|DYNTOPO_CLEANUP|DYNTOPO_SUBDIVIDE, _({\
        {DYNTOPO_COLLAPSE, "COLLAPSE", ICON_NONE, "Collapse", ""},
        {DYNTOPO_SUBDIVIDE, "SUBDIVIDE", ICON_NONE, "Subdivide", ""},
        {DYNTOPO_CLEANUP, "CLEANUP", ICON_NONE, "Cleanup", ""},
        {DYNTOPO_LOCAL_COLLAPSE, "LOCAL_COLLAPSE", ICON_NONE, "Local Collapse", ""},
        {DYNTOPO_LOCAL_SUBDIVIDE, "LOCAL_SUBDIVIDE", ICON_NONE, "Local Subdivide", ""},
        {-1}
      }), BRUSH_CHANNEL_INHERIT)
  MAKE_ENUM(slide_deform_type, "Slide Deform Type", "", BRUSH_SLIDE_DEFORM_DRAG, _({\
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
  MAKE_ENUM(direction, "Direction", "", 0, _({\
        {0, "ADD", "ADD", "Add", "Add effect of brush"},
        {1, "SUBTRACT", "REMOVE", "Subtract", "Subtract effect of brush"},
        {-1}
     }))

MAKE_FLOAT(normal_weight, "Normal Weight", "", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(rake_factor, "Rake Factor",  "How much grab will follow cursor rotation", 0.0f, 0.0f, 10.0f)
MAKE_FLOAT(weight, "Weight", "", 0.5f, 0.0f, 1.0f)
MAKE_FLOAT(jitter, "Jitter",  "Jitter the position of the brush while painting", 0.0f, 0.0f, 1.0f)
MAKE_INT(jitter_absolute, "Absolute Jitter", "", 0, 0.0f, 1000.0f)
MAKE_FLOAT(smooth_stroke_radius, "Smooth Stroke Radius", "Minimum distance from last point before stroke continues", 10.0f, 10.0f, 200.0f)
MAKE_FLOAT(smooth_stroke_factor, "Smooth Stroke Factor", "", 0.5f, 0.5f, 0.99f)
MAKE_FLOAT_EX(rate, "Rate", "", 0.5, 0.0001f, 10000.0f, 0.01f, 1.0f, false)
MAKE_FLOAT(flow, "Flow", "Amount of paint that is applied per stroke sample", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(wet_mix, "Wet Mix", "Amount of paint that is picked from the surface into the brush color", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(wet_persistence, "Wet Persistence", "Amount of wet paint that stays in the brush after applying paint to the surface", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(density, "Density", "Amount of random elements that are going to be affected by the brush", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(tip_scale_x, "Tip Scale X", "Scale of the brush tip in the X axis", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT(dash_ratio, "Dash Ratio", "Ratio of samples in a cycle that the brush is enabled", 0.0f, 0.0f, 1.0f)
MAKE_FLOAT_EX(plane_offset, "Plane Offset", "Adjust plane on which the brush acts towards or away from the object surface", 0.0f, -2.0f, 2.0f, -0.5f, 0.5f, false)
MAKE_BOOL(original_normal, "Original Normal", "When locked keep using normal of surface where stroke was initiated", false)
MAKE_BOOL(original_plane, "Original Plane", "When locked keep using the plane origin of surface where stroke was initiated", false)
MAKE_BOOL(use_weighted_smooth, "Weight By Area", "Weight by face area to get a smoother result", true)
MAKE_BOOL(preserve_faceset_boundary, "Keep FSet Boundary", "Preserve face set boundaries", true)
MAKE_BOOL(hard_edge_mode, "Hard Edge Mode", "Forces all brushes into hard edge face set mode (sets face set slide to 0)", false)
MAKE_BOOL(grab_silhouette, "Grab Silhouette", "Grabs trying to automask the silhouette of the object", false)
MAKE_FLOAT(dyntopo_detail_percent, "Detail Percent", "Detail Percent", 25.0f, 0.0f, 1000.0f)
MAKE_FLOAT(dyntopo_detail_range, "Detail Range", "Detail Range", 0.45f, 0.01f, 0.99f)
MAKE_FLOAT_EX(dyntopo_detail_size, "Detail Size", "Detail Size", 8.0f, 0.1f, 100.0f, 0.001f, 500.0f, false)
MAKE_FLOAT_EX(dyntopo_constant_detail, "Constaint Detail", "", 3.0f, 0.001f, 1000.0f, 0.0001, FLT_MAX, false)
MAKE_FLOAT_EX(dyntopo_spacing, "Spacing", "Dyntopo Spacing", 35.0f, 0.01f, 300.0f, 0.001f, 50000.0f, false)
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

#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif
#endif
