/* check if we need to undef various macros */

#if defined(BRUSH_CHANNEL_MAKE_CPP_LOOKUPS) || defined(BRUSH_CHANNEL_DEFINE_TYPES) || \
    defined(BRUSH_CHANNEL_DEFINE_EXTERNAL) || defined(BRUSH_CHANNEL_MAKE_NAMES)
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
#  ifdef MAKE_CURVE_EX
#    undef MAKE_CURVE_EX
#  endif

#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif
#  ifdef MAKE_FLOAT_EX_FLAG
#    undef MAKE_FLOAT_EX_FLAG
#  endif

#  ifdef MAKE_BUILTIN_CH_DEF
#    undef MAKE_BUILTIN_CH_DEF
#  endif
#endif

#ifdef BRUSH_CHANNEL_MAKE_CPP_LOOKUPS

#  define MAKE_BUILTIN_CH_DEF(idname, type) \
    BrushChannelIF<type> idname() \
    { \
      return lookup<type>(#idname); \
    }

//      BrushChannel *ch = lookup(#idname); \
//      return BrushChannelIF<type>(ch); \
//    }

#  define MAKE_FLOAT_EX(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname, float)
#  define MAKE_FLOAT_EX_FLAG( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, flag) \
    MAKE_BUILTIN_CH_DEF(idname, float)

#  define MAKE_FLOAT_EX_INV(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled) \
    MAKE_BUILTIN_CH_DEF(idname, float)
#  define MAKE_FLOAT_EX_EX( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, inv, flag) \
    MAKE_BUILTIN_CH_DEF(idname, float)
#  define MAKE_FLOAT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname, float);

#  define MAKE_INT_EX(idname, name, tooltip, val, min, max, smin, smax) \
    MAKE_BUILTIN_CH_DEF(idname, int);
#  define MAKE_INT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname, int);
#  define MAKE_BOOL(idname, name, tooltip, val) MAKE_BUILTIN_CH_DEF(idname, bool);
#  define MAKE_BOOL_EX(idname, name, tooltip, val, flag) MAKE_BUILTIN_CH_DEF(idname, bool);
#  define MAKE_COLOR3(idname, name, tooltip, r, g, b) MAKE_BUILTIN_CH_DEF(idname, float[3]);
#  define MAKE_COLOR4(idname, name, tooltip, r, g, b, a) MAKE_BUILTIN_CH_DEF(idname, float[4]);
#  define MAKE_FLOAT3(idname, name, tooltip, x, y, z, min, max) \
    MAKE_BUILTIN_CH_DEF(idname, float[3]);
#  define MAKE_FLOAT3_EX(idname, name, tooltip, x, y, z, min, max, smin, smax, flag) \
    MAKE_BUILTIN_CH_DEF(idname, float[3]);
#  define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1, int);
#  define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1, int);
#  define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1, int);
#  define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1, int);
#  define MAKE_CURVE(idname1, name1, tooltip1, preset1) MAKE_BUILTIN_CH_DEF(idname1, BrushCurve);
#  define MAKE_CURVE_EX(idname1, name1, tooltip1, preset1, flag, preset_slope_neg) \
    MAKE_BUILTIN_CH_DEF(idname1, BrushCurve);

/* static name checking stuff */
#elif defined(BRUSH_CHANNEL_DEFINE_TYPES) || defined(BRUSH_CHANNEL_DEFINE_EXTERNAL) || \
    defined(BRUSH_CHANNEL_MAKE_NAMES)

#  ifdef BRUSH_CHANNEL_DEFINE_TYPES
#    define MAKE_BUILTIN_CH_DEF(idname) const char *BRUSH_BUILTIN_##idname = #    idname;
#  elif defined(BRUSH_CHANNEL_MAKE_NAMES)
#    define MAKE_BUILTIN_CH_DEF(idname) #    idname,
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
#  define MAKE_FLOAT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_INT_EX(idname, name, tooltip, val, min, max, smin, smax) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_INT(idname, name, tooltip, val, min, max) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_BOOL(idname, name, tooltip, val) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_BOOL_EX(idname, name, tooltip, val, flag) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_COLOR3(idname, name, tooltip, r, g, b) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_COLOR4(idname, name, tooltip, r, g, b, a) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT3(idname, name, tooltip, x, y, z, min, max) MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_FLOAT3_EX(idname, name, tooltip, x, y, z, min, max, smin, smax, flag) \
    MAKE_BUILTIN_CH_DEF(idname)
#  define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1, ...) MAKE_BUILTIN_CH_DEF(idname1)
#  define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1)
#  define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1, ...) MAKE_BUILTIN_CH_DEF(idname1)
#  define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...) \
    MAKE_BUILTIN_CH_DEF(idname1)
#  define MAKE_CURVE(idname1, name1, tooltip1, preset1) MAKE_BUILTIN_CH_DEF(idname1)
#  define MAKE_CURVE_EX(idname1, name1, tooltip1, preset1, flag, preset_slope_neg) \
    MAKE_BUILTIN_CH_DEF(idname1)
#else
#endif
