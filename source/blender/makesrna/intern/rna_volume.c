/*
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
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BKE_volume.h"

#include "BLI_math_base.h"

#ifdef RNA_RUNTIME

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "WM_api.h"
#  include "WM_types.h"

/* Updates */

static void rna_Volume_update_display(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->owner_id;
  WM_main_add_notifier(NC_GEOM | ND_DATA, volume);
}

static void rna_Volume_update_filepath(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->owner_id;
  BKE_volume_unload(volume);
  DEG_id_tag_update(&volume->id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_GEOM | ND_DATA, volume);
}

static void rna_Volume_update_is_sequence(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Volume_update_filepath(bmain, scene, ptr);
  DEG_relations_tag_update(bmain);
}

/* Grid */

static void rna_VolumeGrid_name_get(PointerRNA *ptr, char *value)
{
  VolumeGrid *grid = ptr->data;
  strcpy(value, BKE_volume_grid_name(grid));
}

static int rna_VolumeGrid_name_length(PointerRNA *ptr)
{
  VolumeGrid *grid = ptr->data;
  return strlen(BKE_volume_grid_name(grid));
}

static int rna_VolumeGrid_data_type_get(PointerRNA *ptr)
{
  const VolumeGrid *grid = ptr->data;
  return BKE_volume_grid_type(grid);
}

static int rna_VolumeGrid_channels_get(PointerRNA *ptr)
{
  const VolumeGrid *grid = ptr->data;
  return BKE_volume_grid_channels(grid);
}

static void rna_VolumeGrid_matrix_object_get(PointerRNA *ptr, float *value)
{
  VolumeGrid *grid = ptr->data;
  BKE_volume_grid_transform_matrix(grid, (float(*)[4])value);
}

static bool rna_VolumeGrid_is_loaded_get(PointerRNA *ptr)
{
  VolumeGrid *grid = ptr->data;
  return BKE_volume_grid_is_loaded(grid);
}

static bool rna_VolumeGrid_load(ID *id, VolumeGrid *grid)
{
  return BKE_volume_grid_load((Volume *)id, grid);
}

static void rna_VolumeGrid_unload(ID *id, VolumeGrid *grid)
{
  BKE_volume_grid_unload((Volume *)id, grid);
}

/* Grids Iterator */

static void rna_Volume_grids_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Volume *volume = ptr->data;
  int num_grids = BKE_volume_num_grids(volume);
  iter->internal.count.ptr = volume;
  iter->internal.count.item = 0;
  iter->valid = (iter->internal.count.item < num_grids);
}

static void rna_Volume_grids_next(CollectionPropertyIterator *iter)
{
  Volume *volume = iter->internal.count.ptr;
  int num_grids = BKE_volume_num_grids(volume);
  iter->internal.count.item++;
  iter->valid = (iter->internal.count.item < num_grids);
}

static void rna_Volume_grids_end(CollectionPropertyIterator *UNUSED(iter))
{
}

static PointerRNA rna_Volume_grids_get(CollectionPropertyIterator *iter)
{
  Volume *volume = iter->internal.count.ptr;
  const VolumeGrid *grid = BKE_volume_grid_get(volume, iter->internal.count.item);
  return rna_pointer_inherit_refine(&iter->parent, &RNA_VolumeGrid, (void *)grid);
}

static int rna_Volume_grids_length(PointerRNA *ptr)
{
  Volume *volume = ptr->data;
  return BKE_volume_num_grids(volume);
}

/* Active Grid */

static void rna_VolumeGrids_active_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Volume *volume = (Volume *)ptr->data;
  int num_grids = BKE_volume_num_grids(volume);

  *min = 0;
  *max = max_ii(0, num_grids - 1);
}

static int rna_VolumeGrids_active_index_get(PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->data;
  int num_grids = BKE_volume_num_grids(volume);
  return clamp_i(volume->active_grid, 0, max_ii(num_grids - 1, 0));
}

static void rna_VolumeGrids_active_index_set(PointerRNA *ptr, int value)
{
  Volume *volume = (Volume *)ptr->data;
  volume->active_grid = value;
}

/* Loading */

static bool rna_VolumeGrids_is_loaded_get(PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->data;
  return BKE_volume_is_loaded(volume);
}

/* Error Message */

static void rna_VolumeGrids_error_message_get(PointerRNA *ptr, char *value)
{
  Volume *volume = (Volume *)ptr->data;
  strcpy(value, BKE_volume_grids_error_msg(volume));
}

static int rna_VolumeGrids_error_message_length(PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->data;
  return strlen(BKE_volume_grids_error_msg(volume));
}

/* Frame Filepath */
static void rna_VolumeGrids_frame_filepath_get(PointerRNA *ptr, char *value)
{
  Volume *volume = (Volume *)ptr->data;
  strcpy(value, BKE_volume_grids_frame_filepath(volume));
}

static int rna_VolumeGrids_frame_filepath_length(PointerRNA *ptr)
{
  Volume *volume = (Volume *)ptr->data;
  return strlen(BKE_volume_grids_frame_filepath(volume));
}

#else

static void rna_def_volume_grid(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VolumeGrid", NULL);
  RNA_def_struct_ui_text(srna, "Volume Grid", "3D volume grid");
  RNA_def_struct_ui_icon(srna, ICON_VOLUME_DATA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_VolumeGrid_name_get", "rna_VolumeGrid_name_length", NULL);
  RNA_def_property_ui_text(prop, "Name", "Volume grid name");

  static const EnumPropertyItem data_type_items[] = {
      {VOLUME_GRID_BOOLEAN, "BOOLEAN", 0, "Boolean", "Boolean"},
      {VOLUME_GRID_FLOAT, "FLOAT", 0, "Float", "Single precision float"},
      {VOLUME_GRID_DOUBLE, "DOUBLE", 0, "Double", "Double precision"},
      {VOLUME_GRID_INT, "INT", 0, "Integer", "32-bit integer"},
      {VOLUME_GRID_INT64, "INT64", 0, "Integer 64-bit", "64-bit integer"},
      {VOLUME_GRID_MASK, "MASK", 0, "Mask", "No data, boolean mask of active voxels"},
      {VOLUME_GRID_STRING, "STRING", 0, "String", "Text string"},
      {VOLUME_GRID_VECTOR_FLOAT, "VECTOR_FLOAT", 0, "Float Vector", "3D float vector"},
      {VOLUME_GRID_VECTOR_DOUBLE, "VECTOR_DOUBLE", 0, "Double Vector", "3D double vector"},
      {VOLUME_GRID_VECTOR_INT, "VECTOR_INT", 0, "Integer Vector", "3D integer vector"},
      {VOLUME_GRID_POINTS,
       "POINTS",
       0,
       "Points (Unsupported)",
       "Points grid, currently unsupported by volume objects"},
      {VOLUME_GRID_UNKNOWN, "UNKNOWN", 0, "Unknown", "Unsupported data type"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_funcs(prop, "rna_VolumeGrid_data_type_get", NULL, NULL);
  RNA_def_property_enum_items(prop, data_type_items);
  RNA_def_property_ui_text(prop, "Data Type", "Data type of voxel values");

  prop = RNA_def_property(srna, "channels", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_VolumeGrid_channels_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Channels", "Number of dimensions of the grid data type");

  prop = RNA_def_property(srna, "matrix_object", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_float_funcs(prop, "rna_VolumeGrid_matrix_object_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Matrix Object", "Transformation matrix from voxel index to object space");

  prop = RNA_def_property(srna, "is_loaded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_VolumeGrid_is_loaded_get", NULL);
  RNA_def_property_ui_text(prop, "Is Loaded", "Grid tree is loaded in memory");

  /* API */
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "load", "rna_VolumeGrid_load");
  RNA_def_function_ui_description(func, "Load grid tree from file");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  parm = RNA_def_boolean(func, "success", 0, "", "True if grid tree was successfully loaded");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "unload", "rna_VolumeGrid_unload");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(
      func, "Unload grid tree and voxel data from memory, leaving only metadata");
}

static void rna_def_volume_grids(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "VolumeGrids");
  srna = RNA_def_struct(brna, "VolumeGrids", NULL);
  RNA_def_struct_sdna(srna, "Volume");
  RNA_def_struct_ui_text(srna, "Volume Grids", "3D volume grids");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_VolumeGrids_active_index_get",
                             "rna_VolumeGrids_active_index_set",
                             "rna_VolumeGrids_active_index_range");
  RNA_def_property_ui_text(prop, "Active Grid Index", "Index of active volume grid");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "error_message", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_VolumeGrids_error_message_get", "rna_VolumeGrids_error_message_length", NULL);
  RNA_def_property_ui_text(
      prop, "Error Message", "If loading grids failed, error message with details");

  prop = RNA_def_property(srna, "is_loaded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_VolumeGrids_is_loaded_get", NULL);
  RNA_def_property_ui_text(prop, "Is Loaded", "List of grids and metadata are loaded in memory");

  prop = RNA_def_property(srna, "frame", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "runtime.frame");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Frame",
                           "Frame number that volume grids will be loaded at, based on scene time "
                           "and volume parameters");

  prop = RNA_def_property(srna, "frame_filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_VolumeGrids_frame_filepath_get", "rna_VolumeGrids_frame_filepath_length", NULL);

  RNA_def_property_ui_text(prop,
                           "Frame File Path",
                           "Volume file used for loading the volume at the current frame. Empty "
                           "if the volume has not be loaded or the frame only exists in memory");

  /* API */
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "load", "BKE_volume_load");
  RNA_def_function_ui_description(func, "Load list of grids and metadata from file");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_boolean(func, "success", 0, "", "True if grid list was successfully loaded");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "unload", "BKE_volume_unload");
  RNA_def_function_ui_description(func, "Unload all grid and voxel data from memory");

  func = RNA_def_function(srna, "save", "BKE_volume_save");
  RNA_def_function_ui_description(func, "Save grids and metadata to file");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_string_file_path(func, "filepath", NULL, 0, "", "File path to save to");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_boolean(func, "success", 0, "", "True if grid list was successfully loaded");
  RNA_def_function_return(func, parm);
}

static void rna_def_volume_display(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VolumeDisplay", NULL);
  RNA_def_struct_ui_text(srna, "Volume Display", "Volume object display settings for 3D viewport");
  RNA_def_struct_sdna(srna, "VolumeDisplay");

  prop = RNA_def_property(srna, "density", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.00001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.1, 100.0, 1, 3);
  RNA_def_property_ui_text(prop, "Density", "Thickness of volume display in the viewport");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  static const EnumPropertyItem wireframe_type_items[] = {
      {VOLUME_WIREFRAME_NONE, "NONE", 0, "None", "Don't display volume in wireframe mode"},
      {VOLUME_WIREFRAME_BOUNDS,
       "BOUNDS",
       0,
       "Bounds",
       "Display single bounding box for the entire grid"},
      {VOLUME_WIREFRAME_BOXES,
       "BOXES",
       0,
       "Boxes",
       "Display bounding boxes for nodes in the volume tree"},
      {VOLUME_WIREFRAME_POINTS,
       "POINTS",
       0,
       "Points",
       "Display points for nodes in the volume tree"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem wireframe_detail_items[] = {
      {VOLUME_WIREFRAME_COARSE,
       "COARSE",
       0,
       "Coarse",
       "Display one box or point for each intermediate tree node"},
      {VOLUME_WIREFRAME_FINE,
       "FINE",
       0,
       "Fine",
       "Display box for each leaf node containing 8x8 voxels"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem interpolation_method_items[] = {
      {VOLUME_DISPLAY_INTERP_LINEAR, "LINEAR", 0, "Linear", "Good smoothness and speed"},
      {VOLUME_DISPLAY_INTERP_CUBIC,
       "CUBIC",
       0,
       "Cubic",
       "Smoothed high quality interpolation, but slower"},
      {VOLUME_DISPLAY_INTERP_CLOSEST, "CLOSEST", 0, "Closest", "No interpolation"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem axis_slice_position_items[] = {
      {VOLUME_SLICE_AXIS_AUTO,
       "AUTO",
       0,
       "Auto",
       "Adjust slice direction according to the view direction"},
      {VOLUME_SLICE_AXIS_X, "X", 0, "X", "Slice along the X axis"},
      {VOLUME_SLICE_AXIS_Y, "Y", 0, "Y", "Slice along the Y axis"},
      {VOLUME_SLICE_AXIS_Z, "Z", 0, "Z", "Slice along the Z axis"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "wireframe_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, wireframe_type_items);
  RNA_def_property_ui_text(prop, "Wireframe", "Type of wireframe display");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "wireframe_detail", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, wireframe_detail_items);
  RNA_def_property_ui_text(prop, "Wireframe Detail", "Amount of detail for wireframe display");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "interpolation_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, interpolation_method_items);
  RNA_def_property_ui_text(
      prop, "Interpolation", "Interpolation method to use for volumes in solid mode");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "use_slice", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "axis_slice_method", VOLUME_AXIS_SLICE_SINGLE);
  RNA_def_property_ui_text(prop, "Slice", "Perform a single slice of the domain object");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "slice_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, axis_slice_position_items);
  RNA_def_property_ui_text(prop, "Axis", "");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "slice_depth", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 3);
  RNA_def_property_ui_text(prop, "Position", "Position of the slice");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");
}

static void rna_def_volume_render(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VolumeRender", NULL);
  RNA_def_struct_ui_text(srna, "Volume Render", "Volume object render settings");
  RNA_def_struct_sdna(srna, "VolumeRender");

  static const EnumPropertyItem space_items[] = {
      {VOLUME_SPACE_OBJECT,
       "OBJECT",
       0,
       "Object",
       "Keep volume opacity and detail the same regardless of object scale"},
      {VOLUME_SPACE_WORLD,
       "WORLD",
       0,
       "World",
       "Specify volume step size and density in world space"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "space", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, space_items);
  RNA_def_property_ui_text(
      prop, "Space", "Specify volume density and step size  in object or world space");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "step_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 100.0, 1, 3);
  RNA_def_property_ui_text(prop,
                           "Step Size",
                           "Distance between volume samples. Lower values render more detail at "
                           "the cost of performance. If set to zero, the step size is "
                           "automatically determined based on voxel size");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");

  prop = RNA_def_property(srna, "clipping", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "clipping");
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 3);
  RNA_def_property_ui_text(
      prop,
      "Clipping",
      "Value under which voxels are considered empty space to optimize rendering");
  RNA_def_property_update(prop, 0, "rna_Volume_update_display");
}

static void rna_def_volume(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Volume", "ID");
  RNA_def_struct_ui_text(srna, "Volume", "Volume data-block for 3D volume grids");
  RNA_def_struct_ui_icon(srna, ICON_VOLUME_DATA);

  /* File */
  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "File Path", "Volume file used by this Volume data-block");
  RNA_def_property_update(prop, 0, "rna_Volume_update_filepath");

  prop = RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "packedfile");
  RNA_def_property_ui_text(prop, "Packed File", "");

  /* Sequence */
  prop = RNA_def_property(srna, "is_sequence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Sequence", "Whether the cache is separated in a series of files");
  RNA_def_property_update(prop, 0, "rna_Volume_update_is_sequence");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Start Frame", "Global starting frame of the sequence, assuming first has a #1");
  RNA_def_property_update(prop, 0, "rna_Volume_update_filepath");

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_text(prop, "Frames", "Number of frames of the sequence to use");
  RNA_def_property_update(prop, 0, "rna_Volume_update_filepath");

  prop = RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Offset", "Offset the number of the frame to use in the animation");
  RNA_def_property_update(prop, 0, "rna_Volume_update_filepath");

  static const EnumPropertyItem sequence_mode_items[] = {
      {VOLUME_SEQUENCE_CLIP, "CLIP", 0, "Clip", "Hide frames outside the specified frame range"},
      {VOLUME_SEQUENCE_EXTEND,
       "EXTEND",
       0,
       "Extend",
       "Repeat the start frame before, and the end frame after the frame range"},
      {VOLUME_SEQUENCE_REPEAT, "REPEAT", 0, "Repeat", "Cycle the frames in the sequence"},
      {VOLUME_SEQUENCE_PING_PONG,
       "PING_PONG",
       0,
       "Ping-Pong",
       "Repeat the frames, reversing the playback direction every other cycle"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "sequence_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_enum_items(prop, sequence_mode_items);
  RNA_def_property_ui_text(prop, "Sequence Mode", "Sequence playback mode");
  RNA_def_property_update(prop, 0, "rna_Volume_update_filepath");

  /* Grids */
  prop = RNA_def_property(srna, "grids", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "VolumeGrid");
  RNA_def_property_ui_text(prop, "Grids", "3D volume grids");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Volume_grids_begin",
                                    "rna_Volume_grids_next",
                                    "rna_Volume_grids_end",
                                    "rna_Volume_grids_get",
                                    "rna_Volume_grids_length",
                                    NULL,
                                    NULL,
                                    NULL);
  rna_def_volume_grids(brna, prop);

  /* Materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

  /* Display */
  prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "display");
  RNA_def_property_struct_type(prop, "VolumeDisplay");
  RNA_def_property_ui_text(prop, "Display", "Volume display settings for 3D viewport");

  /* Render */
  prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "render");
  RNA_def_property_struct_type(prop, "VolumeRender");
  RNA_def_property_ui_text(prop, "Render", "Volume render settings for 3D viewport");

  /* Common */
  rna_def_animdata_common(srna);
}

void RNA_def_volume(BlenderRNA *brna)
{
  rna_def_volume_grid(brna);
  rna_def_volume_display(brna);
  rna_def_volume_render(brna);
  rna_def_volume(brna);
}

#endif
