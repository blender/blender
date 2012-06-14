/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_nodetree_types.h
 *  \ingroup RNA
 */

/* intentionally no include guard */

/* Empty definitions for undefined macros to avoid warnings */
#ifndef DefNode
#define DefNode(Category, ID, DefFunc, EnumName, StructName, UIName, UIDesc)
#endif

/*       Tree type       Node ID                  RNA def function        Enum name         Struct name       UI Name              UI Description */
DefNode( ShaderNode,     SH_NODE_OUTPUT,          0,                      "OUTPUT",         Output,           "Output",            ""              )
DefNode( ShaderNode,     SH_NODE_MATERIAL,        def_sh_material,        "MATERIAL",       Material,         "Material",          ""              )
DefNode( ShaderNode,     SH_NODE_RGB,             0,                      "RGB",            RGB,              "RGB",               ""              )
DefNode( ShaderNode,     SH_NODE_VALUE,           0,                      "VALUE",          Value,            "Value",             ""              )
DefNode( ShaderNode,     SH_NODE_MIX_RGB,         def_mix_rgb,            "MIX_RGB",        MixRGB,           "MixRGB",            ""              )
DefNode( ShaderNode,     SH_NODE_VALTORGB,        def_colorramp,          "VALTORGB",       ValToRGB,         "ColorRamp",      ""              )
DefNode( ShaderNode,     SH_NODE_RGBTOBW,         0,                      "RGBTOBW",        RGBToBW,          "RGB to BW",         ""              )
DefNode( ShaderNode,     SH_NODE_TEXTURE,         def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( ShaderNode,     SH_NODE_NORMAL,          0,                      "NORMAL",         Normal,           "Normal",            ""              )
DefNode( ShaderNode,     SH_NODE_GAMMA,           0,                      "GAMMA",          Gamma,            "Gamma",             ""              )
DefNode( ShaderNode,     SH_NODE_BRIGHTCONTRAST,  0,                      "BRIGHTCONTRAST", BrightContrast,   "Bright Contrast",   ""              )
DefNode( ShaderNode,     SH_NODE_GEOMETRY,        def_sh_geometry,        "GEOMETRY",       Geometry,         "Geometry",          ""              )
DefNode( ShaderNode,     SH_NODE_MAPPING,         def_sh_mapping,         "MAPPING",        Mapping,          "Mapping",           ""              )
DefNode( ShaderNode,     SH_NODE_CURVE_VEC,       def_vector_curve,       "CURVE_VEC",      VectorCurve,      "Vector Curve",      ""              )
DefNode( ShaderNode,     SH_NODE_CURVE_RGB,       def_rgb_curve,          "CURVE_RGB",      RGBCurve,         "RGB Curve",         ""              )
DefNode( ShaderNode,     SH_NODE_CAMERA,          0,                      "CAMERA",         CameraData,       "Camera Data",       ""              )
DefNode( ShaderNode,     SH_NODE_MATH,            def_math,               "MATH",           Math,             "Math",              ""              )
DefNode( ShaderNode,     SH_NODE_VECT_MATH,       def_vector_math,        "VECT_MATH",      VectorMath,       "Vector Math",       ""              )
DefNode( ShaderNode,     SH_NODE_SQUEEZE,         0,                      "SQUEEZE",        Squeeze,          "Squeeze",           ""              )
DefNode( ShaderNode,     SH_NODE_MATERIAL_EXT,    def_sh_material,        "MATERIAL_EXT",   ExtendedMaterial, "Extended Material", ""              )
DefNode( ShaderNode,     SH_NODE_INVERT,          0,                      "INVERT",         Invert,           "Invert",            ""              )
DefNode( ShaderNode,     SH_NODE_SEPRGB,          0,                      "SEPRGB",         SeparateRGB,      "Separate RGB",      ""              )
DefNode( ShaderNode,     SH_NODE_COMBRGB,         0,                      "COMBRGB",        CombineRGB,       "Combine RGB",       ""              )
DefNode( ShaderNode,     SH_NODE_HUE_SAT,         0,                      "HUE_SAT",        HueSaturation,    "Hue/Saturation",    ""              )

DefNode( ShaderNode,     SH_NODE_OUTPUT_MATERIAL,    0,                      "OUTPUT_MATERIAL",    OutputMaterial,   "Material Output",   ""       )
DefNode( ShaderNode,     SH_NODE_OUTPUT_LAMP,        0,                      "OUTPUT_LAMP",        OutputLamp,       "Lamp Output",       ""       )
DefNode( ShaderNode,     SH_NODE_OUTPUT_WORLD,       0,                      "OUTPUT_WORLD",       OutputWorld,      "World Output",      ""       )
DefNode( ShaderNode,     SH_NODE_FRESNEL,            0,                      "FRESNEL",            Fresnel,          "Fresnel",           ""       )
DefNode( ShaderNode,     SH_NODE_LAYER_WEIGHT,       0,                      "LAYER_WEIGHT",       LayerWeight,      "LayerWeight",       ""       )
DefNode( ShaderNode,     SH_NODE_MIX_SHADER,         0,                      "MIX_SHADER",         MixShader,        "Mix Shader",        ""       )
DefNode( ShaderNode,     SH_NODE_ADD_SHADER,         0,                      "ADD_SHADER",         AddShader,        "Add Shader",        ""       )
DefNode( ShaderNode,     SH_NODE_ATTRIBUTE,          def_sh_attribute,       "ATTRIBUTE",          Attribute,        "Attribute",         ""       )
DefNode( ShaderNode,     SH_NODE_BACKGROUND,         0,                      "BACKGROUND",         Background,       "Background",        ""       )
DefNode( ShaderNode,     SH_NODE_HOLDOUT,            0,                      "HOLDOUT",            Holdout,          "Holdout",           ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_DIFFUSE,       0,                      "BSDF_DIFFUSE",       BsdfDiffuse,      "Diffuse Bsdf",      ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_GLOSSY,        def_glossy,             "BSDF_GLOSSY",        BsdfGlossy,       "Glossy Bsdf",       ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_GLASS,         def_glossy,             "BSDF_GLASS",         BsdfGlass,        "Glass Bsdf",        ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_TRANSLUCENT,   0,                      "BSDF_TRANSLUCENT",   BsdfTranslucent,  "Translucent Bsdf",  ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_TRANSPARENT,   0,                      "BSDF_TRANSPARENT",   BsdfTransparent,  "Transparent Bsdf",  ""       )
DefNode( ShaderNode,     SH_NODE_BSDF_VELVET,        0,                      "BSDF_VELVET",        BsdfVelvet,       "Velvet Bsdf",       ""       )
DefNode( ShaderNode,     SH_NODE_VOLUME_TRANSPARENT, 0,                      "VOLUME_TRANSPARENT", VolumeTransparent,"Transparent Volume",""       )
DefNode( ShaderNode,     SH_NODE_VOLUME_ISOTROPIC,   0,                      "VOLUME_ISOTROPIC",   VolumeIsotropic,  "Isotropic Volume",  ""       )
DefNode( ShaderNode,     SH_NODE_EMISSION,           0,                      "EMISSION",           Emission,         "Emission",          ""       )
DefNode( ShaderNode,     SH_NODE_NEW_GEOMETRY,       0,                      "NEW_GEOMETRY",       NewGeometry,      "Geometry",          ""       )
DefNode( ShaderNode,     SH_NODE_LIGHT_PATH,         0,                      "LIGHT_PATH",         LightPath,        "Light Path",        ""       )
DefNode( ShaderNode,     SH_NODE_LIGHT_FALLOFF,      0,                      "LIGHT_FALLOFF",      LightFalloff,     "Light Falloff",     ""       )
DefNode( ShaderNode,     SH_NODE_OBJECT_INFO,        0,                      "OBJECT_INFO",        ObjectInfo,       "Object Info",       ""       )
DefNode( ShaderNode,     SH_NODE_PARTICLE_INFO,      0,                      "PARTICLE_INFO",      ParticleInfo,     "Particle Info",     ""       )
DefNode( ShaderNode,     SH_NODE_TEX_IMAGE,          def_sh_tex_image,       "TEX_IMAGE",          TexImage,         "Image Texture",     ""       )
DefNode( ShaderNode,     SH_NODE_TEX_ENVIRONMENT,    def_sh_tex_environment, "TEX_ENVIRONMENT",    TexEnvironment,   "Environment Texture",""      )
DefNode( ShaderNode,     SH_NODE_TEX_SKY,            def_sh_tex_sky,         "TEX_SKY",            TexSky,           "Sky Texture",       ""       )
DefNode( ShaderNode,     SH_NODE_TEX_GRADIENT,       def_sh_tex_gradient,    "TEX_GRADIENT",       TexGradient,      "Gradient Texture",  ""       )
DefNode( ShaderNode,     SH_NODE_TEX_NOISE,          def_sh_tex_noise,       "TEX_NOISE",          TexNoise,         "Noise Texture",     ""       )
DefNode( ShaderNode,     SH_NODE_TEX_MAGIC,          def_sh_tex_magic,       "TEX_MAGIC",          TexMagic,         "Magic Texture",     ""       )
DefNode( ShaderNode,     SH_NODE_TEX_WAVE,           def_sh_tex_wave,        "TEX_WAVE",           TexWave,          "Wave Texture",      ""       )
DefNode( ShaderNode,     SH_NODE_TEX_MUSGRAVE,       def_sh_tex_musgrave,    "TEX_MUSGRAVE",       TexMusgrave,      "Musgrave Texture",  ""       )
DefNode( ShaderNode,     SH_NODE_TEX_VORONOI,        def_sh_tex_voronoi,     "TEX_VORONOI",        TexVoronoi,       "Voronoi Texture",   ""       )
DefNode( ShaderNode,     SH_NODE_TEX_CHECKER,        def_sh_tex_checker,     "TEX_CHECKER",        TexChecker,       "Checker Texture",   ""       )
DefNode( ShaderNode,     SH_NODE_TEX_COORD,          0,                      "TEX_COORD",          TexCoord,         "Texture Coordinate","")

DefNode( CompositorNode, CMP_NODE_VIEWER,         def_cmp_viewer,         "VIEWER",         Viewer,           "Viewer",            ""              )
DefNode( CompositorNode, CMP_NODE_RGB,            0,                      "RGB",            RGB,              "RGB",               ""              )
DefNode( CompositorNode, CMP_NODE_VALUE,          0,                      "VALUE",          Value,            "Value",             ""              )
DefNode( CompositorNode, CMP_NODE_MIX_RGB,        def_mix_rgb,            "MIX_RGB",        MixRGB,           "Mix RGB",           ""              )
DefNode( CompositorNode, CMP_NODE_VALTORGB,       def_colorramp,          "VALTORGB",       ValToRGB,         "ColorRamp",      ""              )
DefNode( CompositorNode, CMP_NODE_RGBTOBW,        0,                      "RGBTOBW",        RGBToBW,          "RGB to BW",         ""              )
DefNode( CompositorNode, CMP_NODE_NORMAL,         0,                      "NORMAL",         Normal,           "Normal",            ""              )
DefNode( CompositorNode, CMP_NODE_CURVE_VEC,      def_vector_curve,       "CURVE_VEC",      CurveVec,         "Vector Curve",      ""              )
DefNode( CompositorNode, CMP_NODE_CURVE_RGB,      def_rgb_curve,          "CURVE_RGB",      CurveRGB,         "RGB Curve",         ""              )
DefNode( CompositorNode, CMP_NODE_ALPHAOVER,      def_cmp_alpha_over,     "ALPHAOVER",      AlphaOver,        "Alpha Over",        ""              )
DefNode( CompositorNode, CMP_NODE_BLUR,           def_cmp_blur,           "BLUR",           Blur,             "Blur",              ""              )
DefNode( CompositorNode, CMP_NODE_FILTER,         def_cmp_filter,         "FILTER",         Filter,           "Filter",            ""              )
DefNode( CompositorNode, CMP_NODE_MAP_VALUE,      def_cmp_map_value,      "MAP_VALUE",      MapValue,         "Map Value",         ""              )
DefNode( CompositorNode, CMP_NODE_TIME,           def_time,               "TIME",           Time,             "Time",              ""              )
DefNode( CompositorNode, CMP_NODE_VECBLUR,        def_cmp_vector_blur,    "VECBLUR",        VecBlur,          "Vector Blur",       ""              )
DefNode( CompositorNode, CMP_NODE_SEPRGBA,        0,                      "SEPRGBA",        SepRGBA,          "Separate RGBA",     ""              )
DefNode( CompositorNode, CMP_NODE_SEPHSVA,        0,                      "SEPHSVA",        SepHSVA,          "Separate HSVA",     ""              )
DefNode( CompositorNode, CMP_NODE_SETALPHA,       0,                      "SETALPHA",       SetAlpha,         "Set Alpha",         ""              )
DefNode( CompositorNode, CMP_NODE_HUE_SAT,        def_cmp_hue_saturation, "HUE_SAT",        HueSat,           "Hue/Saturation",    ""              )
DefNode( CompositorNode, CMP_NODE_IMAGE,          def_cmp_image,          "IMAGE",          Image,            "Image",             ""              )
DefNode( CompositorNode, CMP_NODE_R_LAYERS,       def_cmp_render_layers,  "R_LAYERS",       RLayers,          "Render Layers",     ""              )
DefNode( CompositorNode, CMP_NODE_COMPOSITE,      0,                      "COMPOSITE",      Composite,        "Composite",         ""              )
DefNode( CompositorNode, CMP_NODE_OUTPUT_FILE,    def_cmp_output_file,    "OUTPUT_FILE",    OutputFile,       "Output File",       ""              )
DefNode( CompositorNode, CMP_NODE_TEXTURE,        def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( CompositorNode, CMP_NODE_TRANSLATE,      0,                      "TRANSLATE",      Translate,        "Translate",         ""              )
DefNode( CompositorNode, CMP_NODE_ZCOMBINE,       def_cmp_zcombine,		  "ZCOMBINE",       Zcombine,         "Z Combine",         ""              )
DefNode( CompositorNode, CMP_NODE_COMBRGBA,       0,                      "COMBRGBA",       CombRGBA,         "Combine RGBA",      ""              )
DefNode( CompositorNode, CMP_NODE_DILATEERODE,    def_cmp_dilate_erode,   "DILATEERODE",    DilateErode,      "Dilate/Erode",      ""              )
DefNode( CompositorNode, CMP_NODE_ROTATE,         def_cmp_rotate,         "ROTATE",         Rotate,           "Rotate",            ""              )
DefNode( CompositorNode, CMP_NODE_SCALE,          def_cmp_scale,          "SCALE",          Scale,            "Scale",             ""              )
DefNode( CompositorNode, CMP_NODE_SEPYCCA,        def_cmp_ycc,            "SEPYCCA",        SepYCCA,          "Separate YCCA",     ""              )
DefNode( CompositorNode, CMP_NODE_COMBYCCA,       def_cmp_ycc,            "COMBYCCA",       CombYCCA,         "Combine YCCA",      ""              )
DefNode( CompositorNode, CMP_NODE_SEPYUVA,        0,                      "SEPYUVA",        SepYUVA,          "Separate YUVA",     ""              )
DefNode( CompositorNode, CMP_NODE_COMBYUVA,       0,                      "COMBYUVA",       CombYUVA,         "Combine YUVA",      ""              )
DefNode( CompositorNode, CMP_NODE_DIFF_MATTE,     def_cmp_diff_matte,     "DIFF_MATTE",     DiffMatte,        "Difference Key",    ""              )
DefNode( CompositorNode, CMP_NODE_COLOR_SPILL,    def_cmp_color_spill,    "COLOR_SPILL",    ColorSpill,       "Color Spill",       ""              )
DefNode( CompositorNode, CMP_NODE_CHROMA_MATTE,   def_cmp_chroma_matte,   "CHROMA_MATTE",   ChromaMatte,      "Chroma Key",        ""              )
DefNode( CompositorNode, CMP_NODE_CHANNEL_MATTE,  def_cmp_channel_matte,  "CHANNEL_MATTE",  ChannelMatte,     "Channel Key",       ""              )
DefNode( CompositorNode, CMP_NODE_FLIP,           def_cmp_flip,           "FLIP",           Flip,             "Flip",              ""              )
DefNode( CompositorNode, CMP_NODE_SPLITVIEWER,    def_cmp_splitviewer,    "SPLITVIEWER",    SplitViewer,      "Split Viewer",      ""              )
DefNode( CompositorNode, CMP_NODE_MAP_UV,         def_cmp_map_uv,         "MAP_UV",         MapUV,            "Map UV",            ""              )
DefNode( CompositorNode, CMP_NODE_ID_MASK,        def_cmp_id_mask,        "ID_MASK",        IDMask,           "ID Mask",           ""              )
DefNode( CompositorNode, CMP_NODE_DOUBLEEDGEMASK, def_cmp_double_edge_mask,"DOUBLEEDGEMASK", DoubleEdgeMask, "Double Edge Mask",   ""              )
DefNode( CompositorNode, CMP_NODE_DEFOCUS,        def_cmp_defocus,        "DEFOCUS",        Defocus,          "Defocus",           ""              )
DefNode( CompositorNode, CMP_NODE_DISPLACE,       0,                      "DISPLACE",       Displace,         "Displace",          ""              )
DefNode( CompositorNode, CMP_NODE_COMBHSVA,       0,                      "COMBHSVA",       CombHSVA,         "Combine HSVA",      ""              )
DefNode( CompositorNode, CMP_NODE_MATH,           def_math,               "MATH",           Math,             "Math",              ""              )
DefNode( CompositorNode, CMP_NODE_LUMA_MATTE,     def_cmp_luma_matte,     "LUMA_MATTE",     LumaMatte,        "Luma Matte",        ""              )
DefNode( CompositorNode, CMP_NODE_BRIGHTCONTRAST, 0,                      "BRIGHTCONTRAST", BrightContrast,   "Bright Contrast",   ""              )
DefNode( CompositorNode, CMP_NODE_GAMMA,          0,                      "GAMMA",          Gamma,            "Gamma",             ""              )
DefNode( CompositorNode, CMP_NODE_INVERT,         def_cmp_invert,         "INVERT",         Invert,           "Invert",            ""              )
DefNode( CompositorNode, CMP_NODE_NORMALIZE,      0,                      "NORMALIZE",      Normalize,        "Normalize",         ""              )
DefNode( CompositorNode, CMP_NODE_CROP,           def_cmp_crop,           "CROP",           Crop,             "Crop",              ""              )
DefNode( CompositorNode, CMP_NODE_DBLUR,          def_cmp_dblur,          "DBLUR",          DBlur,            "Directional Blur",  ""              )
DefNode( CompositorNode, CMP_NODE_BILATERALBLUR,  def_cmp_bilateral_blur, "BILATERALBLUR",  Bilateralblur,    "Bilateral Blur",    ""              )
DefNode( CompositorNode, CMP_NODE_PREMULKEY,      def_cmp_premul_key,     "PREMULKEY",      PremulKey,        "Premul Key",        ""              )
DefNode( CompositorNode, CMP_NODE_GLARE,          def_cmp_glare,          "GLARE",          Glare,            "Glare",             ""              )
DefNode( CompositorNode, CMP_NODE_TONEMAP,        def_cmp_tonemap,        "TONEMAP",        Tonemap,          "Tonemap",           ""              )
DefNode( CompositorNode, CMP_NODE_LENSDIST,       def_cmp_lensdist,       "LENSDIST",       Lensdist,         "Lensdist",          ""              )
DefNode( CompositorNode, CMP_NODE_VIEW_LEVELS,    def_cmp_levels,         "LEVELS",         Levels,           "Levels",            ""              )
DefNode( CompositorNode, CMP_NODE_COLOR_MATTE,    def_cmp_color_matte,    "COLOR_MATTE",    ColorMatte,       "Color Matte",       ""              )
DefNode( CompositorNode, CMP_NODE_DIST_MATTE,     def_cmp_distance_matte, "DISTANCE_MATTE", DistanceMatte,    "Distance Matte",    ""              )
DefNode( CompositorNode, CMP_NODE_COLORBALANCE,   def_cmp_colorbalance,   "COLORBALANCE",   ColorBalance,     "Color Balance",     ""              )
DefNode( CompositorNode, CMP_NODE_HUECORRECT,     def_cmp_huecorrect,     "HUECORRECT",     HueCorrect,       "Hue Correct",       ""              )
DefNode( CompositorNode, CMP_NODE_MOVIECLIP,      def_cmp_movieclip,      "MOVIECLIP",      MovieClip,        "MovieClip",         ""              )
DefNode( CompositorNode, CMP_NODE_TRANSFORM,      dev_cmd_transform,      "TRANSFORM",      Transform,        "Transform",         ""              )
DefNode( CompositorNode, CMP_NODE_STABILIZE2D,    def_cmp_stabilize2d,    "STABILIZE2D",    Stabilize,        "Stabilize 2D",      ""              )
DefNode( CompositorNode, CMP_NODE_MOVIEDISTORTION,def_cmp_moviedistortion,"MOVIEDISTORTION",MovieDistortion,  "Movie Distortion",  ""              )
DefNode( CompositorNode, CMP_NODE_MASK_BOX,       def_cmp_boxmask,        "BOXMASK"        ,BoxMask,          "Box mask",          ""              )
DefNode( CompositorNode, CMP_NODE_MASK_ELLIPSE,   def_cmp_ellipsemask,    "ELLIPSEMASK"    ,EllipseMask,      "Ellipse mask",      ""              )
DefNode( CompositorNode, CMP_NODE_BOKEHIMAGE,     def_cmp_bokehimage,     "BOKEHIMAGE"     ,BokehImage,       "Bokeh image",       ""              )
DefNode( CompositorNode, CMP_NODE_BOKEHBLUR,      def_cmp_bokehblur,      "BOKEHBLUR"      ,BokehBlur,        "Bokeh Blur",        ""              )
DefNode( CompositorNode, CMP_NODE_SWITCH,         def_cmp_switch,         "SWITCH"         ,Switch,           "Switch",            ""              )
DefNode( CompositorNode, CMP_NODE_COLORCORRECTION,def_cmp_colorcorrection,"COLORCORRECTION",ColorCorrection,  "ColorCorrection",   ""              )
DefNode( CompositorNode, CMP_NODE_MASK,           def_cmp_mask,           "MASK",           Mask,             "Mask",              ""              )
DefNode( CompositorNode, CMP_NODE_KEYINGSCREEN,   def_cmp_keyingscreen,   "KEYINGSCREEN",   KeyingScreen,     "KeyingScreen",      ""              )
                                                                                                                                                   
DefNode( TextureNode,    TEX_NODE_OUTPUT,         def_tex_output,         "OUTPUT",         Output,           "Output",            ""              )
DefNode( TextureNode,    TEX_NODE_CHECKER,        0,                      "CHECKER",        Checker,          "Checker",           ""              )
DefNode( TextureNode,    TEX_NODE_TEXTURE,        def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( TextureNode,    TEX_NODE_BRICKS,         def_tex_bricks,         "BRICKS",         Bricks,           "Bricks",            ""              )
DefNode( TextureNode,    TEX_NODE_MATH,           def_math,               "MATH",           Math,             "Math",              ""              )
DefNode( TextureNode,    TEX_NODE_MIX_RGB,        def_mix_rgb,            "MIX_RGB",        MixRGB,           "Mix RGB",           ""              )
DefNode( TextureNode,    TEX_NODE_RGBTOBW,        0,                      "RGBTOBW",        RGBToBW,          "RGB to BW",         ""              )
DefNode( TextureNode,    TEX_NODE_VALTORGB,       def_colorramp,          "VALTORGB",       ValToRGB,         "ColorRamp",      ""              )
DefNode( TextureNode,    TEX_NODE_IMAGE,          def_tex_image,          "IMAGE",          Image,            "Image",             ""              )
DefNode( TextureNode,    TEX_NODE_CURVE_RGB,      def_rgb_curve,          "CURVE_RGB",      CurveRGB,         "RGB Curve",         ""              )
DefNode( TextureNode,    TEX_NODE_INVERT,         0,                      "INVERT",         Invert,           "Invert",            ""              )
DefNode( TextureNode,    TEX_NODE_HUE_SAT,        0,                      "HUE_SAT",        HueSaturation,    "Hue/Saturation",    ""              )
DefNode( TextureNode,    TEX_NODE_CURVE_TIME,     def_time,               "CURVE_TIME",     CurveTime,        "Curve Time",        ""              )
DefNode( TextureNode,    TEX_NODE_ROTATE,         0,                      "ROTATE",         Rotate,           "Rotate",            ""              )
DefNode( TextureNode,    TEX_NODE_VIEWER,         0,                      "VIEWER",         Viewer,           "Viewer",            ""              )
DefNode( TextureNode,    TEX_NODE_TRANSLATE,      0,                      "TRANSLATE",      Translate,        "Translate",         ""              )
DefNode( TextureNode,    TEX_NODE_COORD,          0,                      "COORD",          Coordinates,      "Coordinates",       ""              )
DefNode( TextureNode,    TEX_NODE_DISTANCE,       0,                      "DISTANCE",       Distance,         "Distance",          ""              )
DefNode( TextureNode,    TEX_NODE_COMPOSE,        0,                      "COMPOSE",        Compose,          "Compose",           ""              )
DefNode( TextureNode,    TEX_NODE_DECOMPOSE,      0,                      "DECOMPOSE",      Decompose,        "Decompose",         ""              )
DefNode( TextureNode,    TEX_NODE_VALTONOR,       0,                      "VALTONOR",       ValToNor,         "Value to Normal",   ""              )
DefNode( TextureNode,    TEX_NODE_SCALE,          0,                      "SCALE",          Scale,            "Scale",             ""              )


/* undefine macros */
#undef DefNode
