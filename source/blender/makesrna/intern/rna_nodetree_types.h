/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
/*       Tree type       Node ID                  RNA def function        Enum name         Struct name       UI Name              UI Description */
DefNode( ShaderNode,     SH_NODE_OUTPUT,          0,                      "OUTPUT",         Output,           "Output",            ""              )
DefNode( ShaderNode,     SH_NODE_MATERIAL,        def_sh_material,        "MATERIAL",       Material,         "Material",          ""              )
DefNode( ShaderNode,     SH_NODE_RGB,             0,                      "RGB",            RGB,              "RGB",               ""              )
DefNode( ShaderNode,     SH_NODE_VALUE,           0,                      "VALUE",          Value,            "Value",             ""              )
DefNode( ShaderNode,     SH_NODE_MIX_RGB,         def_mix_rgb,            "MIX_RGB",        MixRGB,           "MixRGB",            ""              )
DefNode( ShaderNode,     SH_NODE_VALTORGB,        def_val_to_rgb,         "VALTORGB",       ValToRGB,         "Value to RGB",      ""              )
DefNode( ShaderNode,     SH_NODE_RGBTOBW,         0,                      "RGBTOBW",        RGBToBW,          "RGB to BW",         ""              )
DefNode( ShaderNode,     SH_NODE_TEXTURE,         def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( ShaderNode,     SH_NODE_NORMAL,          0,                      "NORMAL",         Normal,           "Normal",            ""              )
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
                                                                                                                                                   
DefNode( CompositorNode, CMP_NODE_VIEWER,         0,                      "VIEWER",         Viewer,           "Viewer",            ""              )
DefNode( CompositorNode, CMP_NODE_RGB,            0,                      "RGB",            RGB,              "RGB",               ""              )
DefNode( CompositorNode, CMP_NODE_VALUE,          0,                      "VALUE",          Value,            "Value",             ""              )
DefNode( CompositorNode, CMP_NODE_MIX_RGB,        def_mix_rgb,            "MIX_RGB",        MixRGB,           "Mix RGB",           ""              )
DefNode( CompositorNode, CMP_NODE_VALTORGB,       def_val_to_rgb,         "VALTORGB",       ValToRGB,         "Val to RGB",        ""              )
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
DefNode( CompositorNode, CMP_NODE_HUE_SAT,        0,                      "HUE_SAT",        HueSat,           "Hue/Saturation",    ""              )
DefNode( CompositorNode, CMP_NODE_IMAGE,          def_cmp_image,          "IMAGE",          Image,            "Image",             ""              )
DefNode( CompositorNode, CMP_NODE_R_LAYERS,       def_cmp_render_layers,  "R_LAYERS",       RLayers,          "Render Layers",     ""              )
DefNode( CompositorNode, CMP_NODE_COMPOSITE,      0,                      "COMPOSITE",      Composite,        "Composite",         ""              )
DefNode( CompositorNode, CMP_NODE_OUTPUT_FILE,    def_cmp_output_file,    "OUTPUT_FILE",    OutputFile,       "Output File",       ""              )
DefNode( CompositorNode, CMP_NODE_TEXTURE,        def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( CompositorNode, CMP_NODE_TRANSLATE,      0,                      "TRANSLATE",      Translate,        "Translate",         ""              )
DefNode( CompositorNode, CMP_NODE_ZCOMBINE,       0,                      "ZCOMBINE",       Zcombine,         "Z Combine",         ""              )
DefNode( CompositorNode, CMP_NODE_COMBRGBA,       0,                      "COMBRGBA",       CombRGBA,         "Combine RGBA",      ""              )
DefNode( CompositorNode, CMP_NODE_DILATEERODE,    def_cmp_dilate_erode,   "DILATEERODE",    DilateErode,      "Dilate/Erode",      ""              )
DefNode( CompositorNode, CMP_NODE_ROTATE,         0,                      "ROTATE",         Rotate,           "Rotate",            ""              )
DefNode( CompositorNode, CMP_NODE_SCALE,          def_cmp_scale,          "SCALE",          Scale,            "Scale",             ""              )
DefNode( CompositorNode, CMP_NODE_SEPYCCA,        0,                      "SEPYCCA",        SepYCCA,          "Separate YCCA",     ""              )
DefNode( CompositorNode, CMP_NODE_COMBYCCA,       0,                      "COMBYCCA",       CombYCCA,         "Combine YCCA",      ""              )
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
DefNode( CompositorNode, CMP_NODE_DBLUR,          def_cmp_dblur,          "DBLUR",          DBlur,            "DBlur",             ""              )
DefNode( CompositorNode, CMP_NODE_BILATERALBLUR,  def_cmp_bilateral_blur, "BILATERALBLUR",  Bilateralblur,    "Bilateral Blur",    ""              )
DefNode( CompositorNode, CMP_NODE_PREMULKEY,      def_cmp_premul_key,     "PREMULKEY",      PremulKey,        "Premul Key",        ""              )
DefNode( CompositorNode, CMP_NODE_GLARE,          def_cmp_glare,          "GLARE",          Glare,            "Glare",             ""              )
DefNode( CompositorNode, CMP_NODE_TONEMAP,        def_cmp_tonemap,        "TONEMAP",        Tonemap,          "Tonemap",           ""              )
DefNode( CompositorNode, CMP_NODE_LENSDIST,       def_cmp_lensdist,       "LENSDIST",       Lensdist,         "Lensdist",          ""              )
DefNode( CompositorNode, CMP_NODE_VIEW_LEVELS,    0,                      "LEVELS",         Levels,           "Levels",            ""              )
DefNode( CompositorNode, CMP_NODE_COLOR_MATTE,    def_cmp_color_matte,    "COLOR_MATTE",    ColorMatte,       "Color Matte",       ""              )
DefNode( CompositorNode, CMP_NODE_DIST_MATTE,     def_cmp_distance_matte, "DISTANCE_MATTE", DistanceMatte,    "Distance Matte",    ""              )
                                                                                                                                                   
DefNode( TextureNode,    TEX_NODE_OUTPUT,         def_tex_output,         "OUTPUT",         Output,           "Output",            ""              )
DefNode( TextureNode,    TEX_NODE_CHECKER,        0,                      "CHECKER",        Checker,          "Checker",           ""              )
DefNode( TextureNode,    TEX_NODE_TEXTURE,        def_texture,            "TEXTURE",        Texture,          "Texture",           ""              )
DefNode( TextureNode,    TEX_NODE_BRICKS,         def_tex_bricks,         "BRICKS",         Bricks,           "Bricks",            ""              )
DefNode( TextureNode,    TEX_NODE_MATH,           def_math,               "MATH",           Math,             "Math",              ""              )
DefNode( TextureNode,    TEX_NODE_MIX_RGB,        def_mix_rgb,            "MIX_RGB",        MixRGB,           "Mix RGB",           ""              )
DefNode( TextureNode,    TEX_NODE_RGBTOBW,        0,                      "RGBTOBW",        RGBToBW,          "RGB To BW",         ""              )
DefNode( TextureNode,    TEX_NODE_VALTORGB,       def_val_to_rgb,         "VALTORGB",       ValToRGB,         "Val To RGB",        ""              )
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
DefNode( TextureNode,    TEX_NODE_VALTONOR,       0,                      "VALTONOR",       ValToNor,         "Val to Nor",        ""              )
DefNode( TextureNode,    TEX_NODE_SCALE,          0,                      "SCALE",          Scale,            "Scale",             ""              )

