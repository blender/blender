/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_types.hh"

#include "rna_internal.hh"
#include "rna_nodetree_common.hh"

#ifndef RNA_RUNTIME

namespace blender {

static void def_texture(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Texture", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "node_output", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_ui_text(
      prop, "Node Output", "For node-based textures, which output node to use");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_combsep_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, rna_enum_node_combsep_color_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");
}

static void def_tex_output(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TexNodeOutput", "storage");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "Output Name", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_image(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_Image_no_renderresult_or_viewer_poll");

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "ImageUser");
  RNA_def_property_ui_text(
      prop, "Image User", "Parameters defining the image duration, offset and related settings");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_tex_bricks(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom3");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(
      prop, "Offset Amount", "Determines the brick offset of the various rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "offset_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Offset Frequency", "Offset every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "custom4");
  RNA_def_property_range(prop, 0.0f, 99.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Squash Amount",
      "Factor to adjust the brick's width for particular rows determined by the Offset Frequency");

  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "squash_frequency", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom2");
  RNA_def_property_range(prop, 2, 99);
  RNA_def_property_ui_text(prop, "Squash Frequency", "Squash every N rows");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

void RNA_def_nodes_texture(BlenderRNA *brna)
{
  const auto define = [&](const char *base_name,
                          const char *struct_name,
                          void (*func)(BlenderRNA *, StructRNA *) = nullptr) {
    StructRNA *srna = define_specific_node(brna, struct_name, base_name);
    if (func) {
      func(brna, srna);
    }
  };

  /* clang-format off */

  define("TextureNode", "TextureNodeAt");
  define("TextureNode", "TextureNodeBricks", def_tex_bricks);
  define("TextureNode", "TextureNodeChecker");
  define("TextureNode", "TextureNodeCombineColor", def_tex_combsep_color);
  define("TextureNode", "TextureNodeCompose");
  define("TextureNode", "TextureNodeCoordinates");
  define("TextureNode", "TextureNodeCurveRGB", def_rgb_curve);
  define("TextureNode", "TextureNodeCurveTime", def_time);
  define("TextureNode", "TextureNodeDecompose");
  define("TextureNode", "TextureNodeDistance");
  define("TextureNode", "TextureNodeHueSaturation");
  define("TextureNode", "TextureNodeImage", def_tex_image);
  define("TextureNode", "TextureNodeInvert");
  define("TextureNode", "TextureNodeMath", def_math);
  define("TextureNode", "TextureNodeMixRGB", def_mix_rgb);
  define("TextureNode", "TextureNodeOutput", def_tex_output);
  define("TextureNode", "TextureNodeRGBToBW");
  define("TextureNode", "TextureNodeRotate");
  define("TextureNode", "TextureNodeScale");
  define("TextureNode", "TextureNodeSeparateColor", def_tex_combsep_color);
  define("TextureNode", "TextureNodeTexBlend");
  define("TextureNode", "TextureNodeTexClouds");
  define("TextureNode", "TextureNodeTexDistNoise");
  define("TextureNode", "TextureNodeTexMagic");
  define("TextureNode", "TextureNodeTexMarble");
  define("TextureNode", "TextureNodeTexMusgrave");
  define("TextureNode", "TextureNodeTexNoise");
  define("TextureNode", "TextureNodeTexStucci");
  define("TextureNode", "TextureNodeTexture", def_texture);
  define("TextureNode", "TextureNodeTexVoronoi");
  define("TextureNode", "TextureNodeTexWood");
  define("TextureNode", "TextureNodeTranslate");
  define("TextureNode", "TextureNodeValToNor");
  define("TextureNode", "TextureNodeValToRGB", def_colorramp);
  define("TextureNode", "TextureNodeViewer");
  define("TextureNode", "TextureNodeGroup", def_group);

  /* clang-format on */
}

}  // namespace blender

#endif
