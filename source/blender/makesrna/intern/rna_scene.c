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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_scene.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_brush_types.h"
#include "DNA_group_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_userdef_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_freestyle.h"
#include "BKE_editmesh.h"
#include "BKE_paint.h"
#include "BKE_scene.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

/* Include for Bake Options */
#include "RE_engine.h"
#include "RE_pipeline.h"

#ifdef WITH_QUICKTIME
#  include "quicktime_export.h"
#  ifdef WITH_AUDASPACE
#    include "AUD_Space.h"
#  endif
#endif

#ifdef WITH_FFMPEG
#  include "BKE_writeffmpeg.h"
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include "ffmpeg_compat.h"
#endif

#include "ED_render.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLI_threads.h"

#ifdef WITH_OPENEXR
EnumPropertyItem exr_codec_items[] = {
	{R_IMF_EXR_CODEC_NONE, "NONE", 0, "None", ""},
	{R_IMF_EXR_CODEC_PXR24, "PXR24", 0, "Pxr24 (lossy)", ""},
	{R_IMF_EXR_CODEC_ZIP, "ZIP", 0, "ZIP (lossless)", ""},
	{R_IMF_EXR_CODEC_PIZ, "PIZ", 0, "PIZ (lossless)", ""},
	{R_IMF_EXR_CODEC_RLE, "RLE", 0, "RLE (lossless)", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

EnumPropertyItem uv_sculpt_relaxation_items[] = {
	{UV_SCULPT_TOOL_RELAX_LAPLACIAN, "LAPLACIAN", 0, "Laplacian", "Use Laplacian method for relaxation"},
	{UV_SCULPT_TOOL_RELAX_HC, "HC", 0, "HC", "Use HC method for relaxation"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem uv_sculpt_tool_items[] = {
	{UV_SCULPT_TOOL_PINCH, "PINCH", 0, "Pinch", "Pinch UVs"},
	{UV_SCULPT_TOOL_RELAX, "RELAX", 0, "Relax", "Relax UVs"},
	{UV_SCULPT_TOOL_GRAB, "GRAB", 0, "Grab", "Grab UVs"},
	{0, NULL, 0, NULL, NULL}
};


EnumPropertyItem snap_target_items[] = {
	{SCE_SNAP_TARGET_CLOSEST, "CLOSEST", 0, "Closest", "Snap closest point onto target"},
	{SCE_SNAP_TARGET_CENTER, "CENTER", 0, "Center", "Snap center onto target"},
	{SCE_SNAP_TARGET_MEDIAN, "MEDIAN", 0, "Median", "Snap median onto target"},
	{SCE_SNAP_TARGET_ACTIVE, "ACTIVE", 0, "Active", "Snap active onto target"},
	{0, NULL, 0, NULL, NULL}
};
	
EnumPropertyItem proportional_falloff_items[] = {
	{PROP_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "Smooth falloff"},
	{PROP_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", "Spherical falloff"},
	{PROP_ROOT, "ROOT", ICON_ROOTCURVE, "Root", "Root falloff"},
	{PROP_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", "Sharp falloff"},
	{PROP_LIN, "LINEAR", ICON_LINCURVE, "Linear", "Linear falloff"},
	{PROP_CONST, "CONSTANT", ICON_NOCURVE, "Constant", "Constant falloff"},
	{PROP_RANDOM, "RANDOM", ICON_RNDCURVE, "Random", "Random falloff"},
	{0, NULL, 0, NULL, NULL}
};

/* subset of the enum - only curves, missing random and const */
EnumPropertyItem proportional_falloff_curve_only_items[] = {
	{PROP_SMOOTH, "SMOOTH", ICON_SMOOTHCURVE, "Smooth", "Smooth falloff"},
	{PROP_SPHERE, "SPHERE", ICON_SPHERECURVE, "Sphere", "Spherical falloff"},
	{PROP_ROOT, "ROOT", ICON_ROOTCURVE, "Root", "Root falloff"},
	{PROP_SHARP, "SHARP", ICON_SHARPCURVE, "Sharp", "Sharp falloff"},
	{PROP_LIN, "LINEAR", ICON_LINCURVE, "Linear", "Linear falloff"},
	{0, NULL, 0, NULL, NULL}
};


EnumPropertyItem proportional_editing_items[] = {
	{PROP_EDIT_OFF, "DISABLED", ICON_PROP_OFF, "Disable", "Proportional Editing disabled"},
	{PROP_EDIT_ON, "ENABLED", ICON_PROP_ON, "Enable", "Proportional Editing enabled"},
	{PROP_EDIT_PROJECTED, "PROJECTED", ICON_PROP_ON, "Projected (2D)",
	                      "Proportional Editing using screen space locations"},
	{PROP_EDIT_CONNECTED, "CONNECTED", ICON_PROP_CON, "Connected",
	                      "Proportional Editing using connected geometry only"},
	{0, NULL, 0, NULL, NULL}
};

/* keep for operators, not used here */
EnumPropertyItem mesh_select_mode_items[] = {
	{SCE_SELECT_VERTEX, "VERTEX", ICON_VERTEXSEL, "Vertex", "Vertex selection mode"},
	{SCE_SELECT_EDGE, "EDGE", ICON_EDGESEL, "Edge", "Edge selection mode"},
	{SCE_SELECT_FACE, "FACE", ICON_FACESEL, "Face", "Face selection mode"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem snap_element_items[] = {
	{SCE_SNAP_MODE_INCREMENT, "INCREMENT", ICON_SNAP_INCREMENT, "Increment", "Snap to increments of grid"},
	{SCE_SNAP_MODE_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"},
	{SCE_SNAP_MODE_EDGE, "EDGE", ICON_SNAP_EDGE, "Edge", "Snap to edges"},
	{SCE_SNAP_MODE_FACE, "FACE", ICON_SNAP_FACE, "Face", "Snap to faces"},
	{SCE_SNAP_MODE_VOLUME, "VOLUME", ICON_SNAP_VOLUME, "Volume", "Snap to volume"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem snap_node_element_items[] = {
	{SCE_SNAP_MODE_GRID, "GRID", ICON_SNAP_INCREMENT, "Grid", "Snap to grid"},
	{SCE_SNAP_MODE_NODE_X, "NODE_X", ICON_SNAP_EDGE, "Node X", "Snap to left/right node border"},
	{SCE_SNAP_MODE_NODE_Y, "NODE_Y", ICON_SNAP_EDGE, "Node Y", "Snap to top/bottom node border"},
	{SCE_SNAP_MODE_NODE_XY, "NODE_XY", ICON_SNAP_EDGE, "Node X / Y", "Snap to any node border"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem snap_uv_element_items[] = {
	{SCE_SNAP_MODE_INCREMENT, "INCREMENT", ICON_SNAP_INCREMENT, "Increment", "Snap to increments of grid"},
	{SCE_SNAP_MODE_VERTEX, "VERTEX", ICON_SNAP_VERTEX, "Vertex", "Snap to vertices"},
	{0, NULL, 0, NULL, NULL}
};

/* workaround for duplicate enums,
 * have each enum line as a define then conditionally set it or not
 */

#define R_IMF_ENUM_BMP      {R_IMF_IMTYPE_BMP, "BMP", ICON_FILE_IMAGE, "BMP", "Output image in bitmap format"},
#define R_IMF_ENUM_IRIS     {R_IMF_IMTYPE_IRIS, "IRIS", ICON_FILE_IMAGE, "Iris", \
                                                "Output image in (old!) SGI IRIS format"},
#define R_IMF_ENUM_PNG      {R_IMF_IMTYPE_PNG, "PNG", ICON_FILE_IMAGE, "PNG", "Output image in PNG format"},
#define R_IMF_ENUM_JPEG     {R_IMF_IMTYPE_JPEG90, "JPEG", ICON_FILE_IMAGE, "JPEG", "Output image in JPEG format"},
#define R_IMF_ENUM_TAGA     {R_IMF_IMTYPE_TARGA, "TARGA", ICON_FILE_IMAGE, "Targa", "Output image in Targa format"},
#define R_IMF_ENUM_TAGA_RAW {R_IMF_IMTYPE_RAWTGA, "TARGA_RAW", ICON_FILE_IMAGE, "Targa Raw", \
                                                  "Output image in uncompressed Targa format"},

#if 0 /* UNUSED (so far) */
#ifdef WITH_DDS
#  define R_IMF_ENUM_DDS {R_IMF_IMTYPE_DDS, "DDS", ICON_FILE_IMAGE, "DDS", "Output image in DDS format"},
#else
#  define R_IMF_ENUM_DDS
#endif
#endif

#ifdef WITH_OPENJPEG
#  define R_IMF_ENUM_JPEG2K {R_IMF_IMTYPE_JP2, "JPEG2000", ICON_FILE_IMAGE, "JPEG 2000", \
                                               "Output image in JPEG 2000 format"},
#else
#  define R_IMF_ENUM_JPEG2K
#endif

#ifdef WITH_CINEON
#  define R_IMF_ENUM_CINEON {R_IMF_IMTYPE_CINEON, "CINEON", ICON_FILE_IMAGE, "Cineon", \
                                                  "Output image in Cineon format"},
#  define R_IMF_ENUM_DPX    {R_IMF_IMTYPE_DPX, "DPX", ICON_FILE_IMAGE, "DPX", "Output image in DPX format"},
#else
#  define R_IMF_ENUM_CINEON
#  define R_IMF_ENUM_DPX
#endif

#ifdef WITH_OPENEXR
#  define R_IMF_ENUM_EXR_MULTI  {R_IMF_IMTYPE_MULTILAYER, "OPEN_EXR_MULTILAYER", ICON_FILE_IMAGE, \
                                                          "OpenEXR MultiLayer", \
                                                          "Output image in multilayer OpenEXR format"},
#  define R_IMF_ENUM_EXR        {R_IMF_IMTYPE_OPENEXR, "OPEN_EXR", ICON_FILE_IMAGE, "OpenEXR", \
                                                       "Output image in OpenEXR format"},
#else
#  define R_IMF_ENUM_EXR_MULTI
#  define R_IMF_ENUM_EXR
#endif

#ifdef WITH_HDR
#  define R_IMF_ENUM_HDR  {R_IMF_IMTYPE_RADHDR, "HDR", ICON_FILE_IMAGE, "Radiance HDR", \
                                                "Output image in Radiance HDR format"},
#else
#  define R_IMF_ENUM_HDR
#endif

#ifdef WITH_TIFF
#  define R_IMF_ENUM_TIFF {R_IMF_IMTYPE_TIFF, "TIFF", ICON_FILE_IMAGE, "TIFF", "Output image in TIFF format"},
#else
#  define R_IMF_ENUM_TIFF
#endif

#define IMAGE_TYPE_ITEMS_IMAGE_ONLY                                           \
	R_IMF_ENUM_BMP                                                            \
	/* DDS save not supported yet R_IMF_ENUM_DDS */                           \
	R_IMF_ENUM_IRIS                                                           \
	R_IMF_ENUM_PNG                                                            \
	R_IMF_ENUM_JPEG                                                           \
	R_IMF_ENUM_JPEG2K                                                         \
	R_IMF_ENUM_TAGA                                                           \
	R_IMF_ENUM_TAGA_RAW                                                       \
	{0, "", 0, " ", NULL},                                                    \
	R_IMF_ENUM_CINEON                                                         \
	R_IMF_ENUM_DPX                                                            \
	R_IMF_ENUM_EXR_MULTI                                                      \
	R_IMF_ENUM_EXR                                                            \
	R_IMF_ENUM_HDR                                                            \
	R_IMF_ENUM_TIFF                                                           \


EnumPropertyItem image_only_type_items[] = {

	IMAGE_TYPE_ITEMS_IMAGE_ONLY

	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem image_type_items[] = {
	{0, "", 0, N_("Image"), NULL},

	IMAGE_TYPE_ITEMS_IMAGE_ONLY

	{0, "", 0, N_("Movie"), NULL},
	{R_IMF_IMTYPE_AVIJPEG, "AVI_JPEG", ICON_FILE_MOVIE, "AVI JPEG", "Output video in AVI JPEG format"},
	{R_IMF_IMTYPE_AVIRAW, "AVI_RAW", ICON_FILE_MOVIE, "AVI Raw", "Output video in AVI Raw format"},
#ifdef WITH_FRAMESERVER
	{R_IMF_IMTYPE_FRAMESERVER, "FRAMESERVER", ICON_FILE_SCRIPT, "Frame Server", "Output image to a frameserver"},
#endif
#ifdef WITH_FFMPEG
	{R_IMF_IMTYPE_H264, "H264", ICON_FILE_MOVIE, "H.264", "Output video in H.264 format"},
	{R_IMF_IMTYPE_FFMPEG, "FFMPEG", ICON_FILE_MOVIE, "MPEG", "Output video in MPEG format"},
	{R_IMF_IMTYPE_THEORA, "THEORA", ICON_FILE_MOVIE, "Ogg Theora", "Output video in Ogg format"},
#endif
#ifdef WITH_QUICKTIME
	{R_IMF_IMTYPE_QUICKTIME, "QUICKTIME", ICON_FILE_MOVIE, "QuickTime", "Output video in Quicktime format"},
#endif
#ifdef WITH_FFMPEG
	{R_IMF_IMTYPE_XVID, "XVID", ICON_FILE_MOVIE, "Xvid", "Output video in Xvid format"},
#endif
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem image_color_mode_items[] = {
	{R_IMF_PLANES_BW, "BW", 0, "BW", "Images get saved in 8 bits grayscale (only PNG, JPEG, TGA, TIF)"},
	{R_IMF_PLANES_RGB, "RGB", 0, "RGB", "Images are saved with RGB (color) data"},
	{R_IMF_PLANES_RGBA, "RGBA", 0, "RGBA", "Images are saved with RGB and Alpha data (if supported)"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME
#define IMAGE_COLOR_MODE_BW   image_color_mode_items[0]
#define IMAGE_COLOR_MODE_RGB  image_color_mode_items[1]
#define IMAGE_COLOR_MODE_RGBA image_color_mode_items[2]
#endif

EnumPropertyItem image_color_depth_items[] = {
	/* 1 (monochrome) not used */
	{R_IMF_CHAN_DEPTH_8,   "8", 0, "8",  "8 bit color channels"},
	{R_IMF_CHAN_DEPTH_10, "10", 0, "10", "10 bit color channels"},
	{R_IMF_CHAN_DEPTH_12, "12", 0, "12", "12 bit color channels"},
	{R_IMF_CHAN_DEPTH_16, "16", 0, "16", "16 bit color channels"},
	/* 24 not used */
	{R_IMF_CHAN_DEPTH_32, "32", 0, "32", "32 bit color channels"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem normal_space_items[] = {
	{R_BAKE_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
	{R_BAKE_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem normal_swizzle_items[] = {
	{R_BAKE_POSX, "POS_X", 0, "+X", ""},
	{R_BAKE_POSY, "POS_Y", 0, "+Y", ""},
	{R_BAKE_POSZ, "POS_Z", 0, "+Z", ""},
	{R_BAKE_NEGX, "NEG_X", 0, "-X", ""},
	{R_BAKE_NEGY, "NEG_Y", 0, "-Y", ""},
	{R_BAKE_NEGZ, "NEG_Z", 0, "-Z", ""},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem bake_save_mode_items[] = {
	{R_BAKE_SAVE_INTERNAL, "INTERNAL", 0, "Internal", "Save the baking map in an internal image datablock"},
	{R_BAKE_SAVE_EXTERNAL, "EXTERNAL", 0, "External", "Save the baking map in an external file"},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME

#include "DNA_anim_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_text_types.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_sound.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_animsys.h"
#include "BKE_freestyle.h"

#include "WM_api.h"

#include "ED_info.h"
#include "ED_node.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_keyframing.h"
#include "ED_image.h"

#include "RE_engine.h"

#ifdef WITH_FREESTYLE
#include "FRS_freestyle.h"
#endif

static void rna_SpaceImageEditor_uv_sculpt_update(Main *bmain, Scene *scene, PointerRNA *UNUSED(ptr))
{
	ED_space_image_uv_sculpt_update(bmain->wm.first, scene->toolsettings);
}

static int rna_Scene_object_bases_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
	Scene *scene = (Scene *)ptr->data;
	Base *base;

	for (base = scene->base.first; base; base = base->next) {
		if (strncmp(base->object->id.name + 2, key, sizeof(base->object->id.name) - 2) == 0) {
			*r_ptr = rna_pointer_inherit_refine(ptr, &RNA_ObjectBase, base);
			return true;
		}
	}

	return false;
}

static PointerRNA rna_Scene_objects_get(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;

	/* we are actually iterating a Base list, so override get */
	return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, ((Base *)internal->link)->object);
}

static Base *rna_Scene_object_link(Scene *scene, bContext *C, ReportList *reports, Object *ob)
{
	Scene *scene_act = CTX_data_scene(C);
	Base *base;

	if (BKE_scene_base_find(scene, ob)) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is already in scene '%s'", ob->id.name + 2, scene->id.name + 2);
		return NULL;
	}

	base = BKE_scene_base_add(scene, ob);
	id_us_plus(&ob->id);

	/* this is similar to what object_add_type and BKE_object_add do */
	base->lay = scene->lay;

	/* when linking to an inactive scene don't touch the layer */
	if (scene == scene_act)
		ob->lay = base->lay;

	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

	/* slows down importers too much, run scene.update() */
	/* DAG_srelations_tag_update(G.main); */

	WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, scene);

	return base;
}

static void rna_Scene_object_unlink(Scene *scene, ReportList *reports, Object *ob)
{
	Base *base = BKE_scene_base_find(scene, ob);
	if (!base) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' is not in this scene '%s'", ob->id.name + 2, scene->id.name + 2);
		return;
	}
	if (base == scene->basact && ob->mode != OB_MODE_OBJECT) {
		BKE_reportf(reports, RPT_ERROR, "Object '%s' must be in object mode to unlink", ob->id.name + 2);
		return;
	}
	if (scene->basact == base) {
		scene->basact = NULL;
	}

	BKE_scene_base_unlink(scene, base);
	MEM_freeN(base);

	ob->id.us--;

	/* needed otherwise the depgraph will contain freed objects which can crash, see [#20958] */
	DAG_relations_tag_update(G.main);

	WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, scene);
}

static void rna_Scene_skgen_etch_template_set(PointerRNA *ptr, PointerRNA value)
{
	ToolSettings *ts = (ToolSettings *)ptr->data;
	if (value.data && ((Object *)value.data)->type == OB_ARMATURE)
		ts->skgen_template = value.data;
	else
		ts->skgen_template = NULL;
}

static PointerRNA rna_Scene_active_object_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_Object, scene->basact ? scene->basact->object : NULL);
}

static void rna_Scene_active_object_set(PointerRNA *ptr, PointerRNA value)
{
	Scene *scene = (Scene *)ptr->data;
	if (value.data)
		scene->basact = BKE_scene_base_find(scene, (Object *)value.data);
	else
		scene->basact = NULL;
}

static void rna_Scene_set_set(PointerRNA *ptr, PointerRNA value)
{
	Scene *scene = (Scene *)ptr->data;
	Scene *set = (Scene *)value.data;
	Scene *nested_set;

	for (nested_set = set; nested_set; nested_set = nested_set->set) {
		if (nested_set == scene)
			return;
		/* prevent eternal loops, set can point to next, and next to set, without problems usually */
		if (nested_set->set == set)
			return;
	}

	scene->set = set;
}

static void rna_Scene_layer_set(PointerRNA *ptr, const int *values)
{
	Scene *scene = (Scene *)ptr->data;

	scene->lay = ED_view3d_scene_layer_set(scene->lay, values, &scene->layact);
}

static int rna_Scene_active_layer_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;

	return (int)(log(scene->layact) / M_LN2);
}

static void rna_Scene_view3d_update(Main *bmain, Scene *UNUSED(scene_unused), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;

	BKE_screen_view3d_main_sync(&bmain->screen, scene);
}

static void rna_Scene_layer_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Scene_view3d_update(bmain, scene, ptr);
	/* XXX We would need do_time=true here, else we can have update issues like [#36289]...
	 *     However, this has too much drawbacks (like slower layer switch, undesired updates...).
	 *     That's TODO for future DAG updates.
	 */
	DAG_on_visible_update(bmain, false);
}

static void rna_Scene_fps_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	sound_update_fps(scene);
	BKE_sequencer_update_sound_bounds_all(scene);
}

static void rna_Scene_listener_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	sound_update_scene_listener(scene);
}

static void rna_Scene_volume_set(PointerRNA *ptr, float value)
{
	Scene *scene = (Scene *)(ptr->data);

	scene->audio.volume = value;
	if (scene->sound_scene)
		sound_set_scene_volume(scene, value);
}

static void rna_Scene_framelen_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	scene->r.framelen = (float)scene->r.framapto / (float)scene->r.images;
}


static void rna_Scene_frame_current_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	
	/* if negative frames aren't allowed, then we can't use them */
	FRAMENUMBER_MIN_CLAMP(value);
	data->r.cfra = value;
}

static float rna_Scene_frame_current_final_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;

	return BKE_scene_frame_get_from_ctime(scene, (float)scene->r.cfra);
}

static void rna_Scene_start_frame_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	/* MINFRAME not MINAFRAME, since some output formats can't taken negative frames */
	CLAMP(value, MINFRAME, MAXFRAME);
	data->r.sfra = value;

	if (data->r.sfra >= data->r.efra) {
		data->r.efra = MIN2(data->r.sfra, MAXFRAME);
	}
}

static void rna_Scene_end_frame_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	CLAMP(value, MINFRAME, MAXFRAME);
	data->r.efra = value;

	if (data->r.sfra >= data->r.efra) {
		data->r.sfra = MAX2(data->r.efra, MINFRAME);
	}
}

static void rna_Scene_use_preview_range_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	
	if (value) {
		/* copy range from scene if not set before */
		if ((data->r.psfra == data->r.pefra) && (data->r.psfra == 0)) {
			data->r.psfra = data->r.sfra;
			data->r.pefra = data->r.efra;
		}
		
		data->r.flag |= SCER_PRV_RANGE;
	}
	else
		data->r.flag &= ~SCER_PRV_RANGE;
}


static void rna_Scene_preview_range_start_frame_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	
	/* check if enabled already */
	if ((data->r.flag & SCER_PRV_RANGE) == 0) {
		/* set end of preview range to end frame, then clamp as per normal */
		/* TODO: or just refuse to set instead? */
		data->r.pefra = data->r.efra;
	}
	
	/* now set normally */
	CLAMP(value, MINAFRAME, data->r.pefra);
	data->r.psfra = value;
}

static void rna_Scene_preview_range_end_frame_set(PointerRNA *ptr, int value)
{
	Scene *data = (Scene *)ptr->data;
	
	/* check if enabled already */
	if ((data->r.flag & SCER_PRV_RANGE) == 0) {
		/* set start of preview range to start frame, then clamp as per normal */
		/* TODO: or just refuse to set instead? */
		data->r.psfra = data->r.sfra;
	}
	
	/* now set normally */
	CLAMP(value, data->r.psfra, MAXFRAME);
	data->r.pefra = value;
}

static void rna_Scene_frame_update(Main *bmain, Scene *UNUSED(current_scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	sound_seek_scene(bmain, scene);
}

static PointerRNA rna_Scene_active_keying_set_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_KeyingSet, ANIM_scene_get_active_keyingset(scene));
}

static void rna_Scene_active_keying_set_set(PointerRNA *ptr, PointerRNA value)
{
	Scene *scene = (Scene *)ptr->data;
	KeyingSet *ks = (KeyingSet *)value.data;
	
	scene->active_keyingset = ANIM_scene_get_keyingset_index(scene, ks);
}

/* get KeyingSet index stuff for list of Keying Sets editing UI
 *	- active_keyingset-1 since 0 is reserved for 'none'
 *	- don't clamp, otherwise can never set builtins types as active...
 */
static int rna_Scene_active_keying_set_index_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	return scene->active_keyingset - 1;
}

/* get KeyingSet index stuff for list of Keying Sets editing UI
 *	- value+1 since 0 is reserved for 'none'
 */
static void rna_Scene_active_keying_set_index_set(PointerRNA *ptr, int value)
{
	Scene *scene = (Scene *)ptr->data;
	scene->active_keyingset = value + 1;
}

/* XXX: evil... builtin_keyingsets is defined in keyingsets.c! */
/* TODO: make API function to retrieve this... */
extern ListBase builtin_keyingsets;

static void rna_Scene_all_keyingsets_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	
	/* start going over the scene KeyingSets first, while we still have pointer to it
	 * but only if we have any Keying Sets to use...
	 */
	if (scene->keyingsets.first)
		rna_iterator_listbase_begin(iter, &scene->keyingsets, NULL);
	else
		rna_iterator_listbase_begin(iter, &builtin_keyingsets, NULL);
}

static void rna_Scene_all_keyingsets_next(CollectionPropertyIterator *iter)
{
	ListBaseIterator *internal = &iter->internal.listbase;
	KeyingSet *ks = (KeyingSet *)internal->link;
	
	/* if we've run out of links in Scene list, jump over to the builtins list unless we're there already */
	if ((ks->next == NULL) && (ks != builtin_keyingsets.last))
		internal->link = (Link *)builtin_keyingsets.first;
	else
		internal->link = (Link *)ks->next;
		
	iter->valid = (internal->link != NULL);
}

static char *rna_RenderSettings_path(PointerRNA *UNUSED(ptr))
{
	return BLI_sprintfN("render");
}

static int rna_RenderSettings_threads_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	return BKE_render_num_threads(rd);
}

static int rna_RenderSettings_threads_mode_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	int override = BLI_system_num_threads_override_get();

	if (override > 0)
		return R_FIXED_THREADS;
	else
		return (rd->mode & R_FIXED_THREADS);
}

static int rna_RenderSettings_is_movie_format_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	return BKE_imtype_is_movie(rd->im_format.imtype);
}

static int rna_RenderSettings_save_buffers_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	Scene *scene = (Scene *)ptr->id.data;
	
	if (rd->mode & R_BORDER)
		return 0;
	else if (!BKE_scene_use_new_shading_nodes(scene))
		return (rd->scemode & (R_EXR_TILE_FILE | R_FULL_SAMPLE)) != 0;
	else 
		return (rd->scemode & R_EXR_TILE_FILE);
}

static int rna_RenderSettings_full_sample_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;

	return (rd->scemode & R_FULL_SAMPLE) && !(rd->mode & R_BORDER);
}

static void rna_ImageFormatSettings_file_format_set(PointerRNA *ptr, int value)
{
	ImageFormatData *imf = (ImageFormatData *)ptr->data;
	ID *id = ptr->id.data;
	const char is_render = (id && GS(id->name) == ID_SCE);
	/* see note below on why this is */
	const char chan_flag = BKE_imtype_valid_channels(imf->imtype, true) | (is_render ? IMA_CHAN_FLAG_BW : 0);

	imf->imtype = value;

	/* ensure depth and color settings match */
	if ( ((imf->planes == R_IMF_PLANES_BW) &&   !(chan_flag & IMA_CHAN_FLAG_BW)) ||
	     ((imf->planes == R_IMF_PLANES_RGBA) && !(chan_flag & IMA_CHAN_FLAG_ALPHA)))
	{
		imf->planes = R_IMF_PLANES_RGB;
	}

	/* ensure usable depth */
	{
		const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
		if ((imf->depth & depth_ok) == 0) {
			/* set first available depth */
			char depth_ls[] = {R_IMF_CHAN_DEPTH_32,
			                   R_IMF_CHAN_DEPTH_24,
			                   R_IMF_CHAN_DEPTH_16,
			                   R_IMF_CHAN_DEPTH_12,
			                   R_IMF_CHAN_DEPTH_10,
			                   R_IMF_CHAN_DEPTH_8,
			                   R_IMF_CHAN_DEPTH_1,
			                   0};
			int i;

			for (i = 0; depth_ls[i]; i++) {
				if (depth_ok & depth_ls[i]) {
					imf->depth = depth_ls[i];
					break;
				}
			}
		}
	}

	if (id && GS(id->name) == ID_SCE) {
		Scene *scene = ptr->id.data;
		RenderData *rd = &scene->r;
#ifdef WITH_FFMPEG
		BKE_ffmpeg_image_type_verify(rd, imf);
#endif
#ifdef WITH_QUICKTIME
		quicktime_verify_image_type(rd, imf);
#endif
		(void)rd;
	}
}

static EnumPropertyItem *rna_ImageFormatSettings_file_format_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                                   PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	ID *id = ptr->id.data;
	if (id && GS(id->name) == ID_SCE) {
		return image_type_items;
	}
	else {
		return image_only_type_items;
	}
}

static EnumPropertyItem *rna_ImageFormatSettings_color_mode_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                                  PropertyRNA *UNUSED(prop), bool *r_free)
{
	ImageFormatData *imf = (ImageFormatData *)ptr->data;
	ID *id = ptr->id.data;
	const char is_render = (id && GS(id->name) == ID_SCE);

	/* note, we need to act differently for render
	 * where 'BW' will force grayscale even if the output format writes
	 * as RGBA, this is age old blender convention and not sure how useful
	 * it really is but keep it for now - campbell */
	char chan_flag = BKE_imtype_valid_channels(imf->imtype, true) | (is_render ? IMA_CHAN_FLAG_BW : 0);

#ifdef WITH_FFMPEG
	/* a WAY more crappy case than B&W flag: depending on codec, file format MIGHT support
	 * alpha channel. for example MPEG format with h264 codec can't do alpha channel, but
	 * the same MPEG format with QTRLE codec can easily handle alpha channel.
	 * not sure how to deal with such cases in a nicer way (sergey) */
	if (is_render) {
		Scene *scene = ptr->id.data;
		RenderData *rd = &scene->r;

		if (BKE_ffmpeg_alpha_channel_is_supported(rd))
			chan_flag |= IMA_CHAN_FLAG_ALPHA;
	}
#endif

	if (chan_flag == (IMA_CHAN_FLAG_BW | IMA_CHAN_FLAG_RGB | IMA_CHAN_FLAG_ALPHA)) {
		return image_color_mode_items;
	}
	else {
		int totitem = 0;
		EnumPropertyItem *item = NULL;

		if (chan_flag & IMA_CHAN_FLAG_BW)    RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_BW);
		if (chan_flag & IMA_CHAN_FLAG_RGB)   RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGB);
		if (chan_flag & IMA_CHAN_FLAG_ALPHA) RNA_enum_item_add(&item, &totitem, &IMAGE_COLOR_MODE_RGBA);

		RNA_enum_item_end(&item, &totitem);
		*r_free = true;

		return item;
	}
}

static EnumPropertyItem *rna_ImageFormatSettings_color_depth_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                                   PropertyRNA *UNUSED(prop), bool *r_free)
{
	ImageFormatData *imf = (ImageFormatData *)ptr->data;

	if (imf == NULL) {
		return image_color_depth_items;
	}
	else {
		const int depth_ok = BKE_imtype_valid_depths(imf->imtype);
		const int is_float = ELEM(imf->imtype, R_IMF_IMTYPE_RADHDR, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER);

		EnumPropertyItem *item_8bit =  &image_color_depth_items[0];
		EnumPropertyItem *item_10bit = &image_color_depth_items[1];
		EnumPropertyItem *item_12bit = &image_color_depth_items[2];
		EnumPropertyItem *item_16bit = &image_color_depth_items[3];
		EnumPropertyItem *item_32bit = &image_color_depth_items[4];

		int totitem = 0;
		EnumPropertyItem *item = NULL;
		EnumPropertyItem tmp = {0, "", 0, "", ""};

		if (depth_ok & R_IMF_CHAN_DEPTH_8) {
			RNA_enum_item_add(&item, &totitem, item_8bit);
		}

		if (depth_ok & R_IMF_CHAN_DEPTH_10) {
			RNA_enum_item_add(&item, &totitem, item_10bit);
		}

		if (depth_ok & R_IMF_CHAN_DEPTH_12) {
			RNA_enum_item_add(&item, &totitem, item_12bit);
		}

		if (depth_ok & R_IMF_CHAN_DEPTH_16) {
			if (is_float) {
				tmp = *item_16bit;
				tmp.name = "Float (Half)";
				RNA_enum_item_add(&item, &totitem, &tmp);
			}
			else {
				RNA_enum_item_add(&item, &totitem, item_16bit);
			}
		}

		if (depth_ok & R_IMF_CHAN_DEPTH_32) {
			if (is_float) {
				tmp = *item_32bit;
				tmp.name = "Float (Full)";
				RNA_enum_item_add(&item, &totitem, &tmp);
			}
			else {
				RNA_enum_item_add(&item, &totitem, item_32bit);
			}
		}

		RNA_enum_item_end(&item, &totitem);
		*r_free = true;

		return item;
	}
}

static int rna_SceneRender_file_ext_length(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	char ext[8];
	ext[0] = '\0';
	BKE_add_image_extension(ext, &rd->im_format);
	return strlen(ext);
}

static void rna_SceneRender_file_ext_get(PointerRNA *ptr, char *str)
{
	RenderData *rd = (RenderData *)ptr->data;
	str[0] = '\0';
	BKE_add_image_extension(str, &rd->im_format);
}

#ifdef WITH_QUICKTIME
static int rna_RenderSettings_qtcodecsettings_codecType_get(PointerRNA *ptr)
{
	QuicktimeCodecSettings *settings = (QuicktimeCodecSettings *)ptr->data;
	
	return quicktime_rnatmpvalue_from_videocodectype(settings->codecType);
}

static void rna_RenderSettings_qtcodecsettings_codecType_set(PointerRNA *ptr, int value)
{
	QuicktimeCodecSettings *settings = (QuicktimeCodecSettings *)ptr->data;

	settings->codecType = quicktime_videocodecType_from_rnatmpvalue(value);
}

static EnumPropertyItem *rna_RenderSettings_qtcodecsettings_codecType_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
                                                                            PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	QuicktimeCodecTypeDesc *codecTypeDesc;
	int i = 1, totitem = 0;

	for (i = 0; i < quicktime_get_num_videocodecs(); i++) {
		codecTypeDesc = quicktime_get_videocodecType_desc(i);
		if (!codecTypeDesc) break;

		tmp.value = codecTypeDesc->rnatmpvalue;
		tmp.identifier = codecTypeDesc->codecName;
		tmp.name = codecTypeDesc->codecName;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}

static int rna_RenderSettings_qtcodecsettings_audiocodecType_get(PointerRNA *ptr)
{
	QuicktimeCodecSettings *settings = (QuicktimeCodecSettings *)ptr->data;
	
	return quicktime_rnatmpvalue_from_audiocodectype(settings->audiocodecType);
}

static void rna_RenderSettings_qtcodecsettings_audiocodecType_set(PointerRNA *ptr, int value)
{
	QuicktimeCodecSettings *settings = (QuicktimeCodecSettings *)ptr->data;
	
	settings->audiocodecType = quicktime_audiocodecType_from_rnatmpvalue(value);
}

static EnumPropertyItem *rna_RenderSettings_qtcodecsettings_audiocodecType_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
                                                                                 PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	QuicktimeCodecTypeDesc *codecTypeDesc;
	int i = 1, totitem = 0;
	
	for (i = 0; i < quicktime_get_num_audiocodecs(); i++) {
		codecTypeDesc = quicktime_get_audiocodecType_desc(i);
		if (!codecTypeDesc) break;
		
		tmp.value = codecTypeDesc->rnatmpvalue;
		tmp.identifier = codecTypeDesc->codecName;
		tmp.name = codecTypeDesc->codecName;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;
	
	return item;
}
#endif

#ifdef WITH_FFMPEG
static void rna_FFmpegSettings_lossless_output_set(PointerRNA *ptr, int value)
{
	Scene *scene = (Scene *) ptr->id.data;
	RenderData *rd = &scene->r;

	if (value)
		rd->ffcodecdata.flags |= FFMPEG_LOSSLESS_OUTPUT;
	else
		rd->ffcodecdata.flags &= ~FFMPEG_LOSSLESS_OUTPUT;

	BKE_ffmpeg_codec_settings_verify(rd);
}

static void rna_FFmpegSettings_codec_settings_update(Main *UNUSED(bmain), Scene *UNUSED(scene_unused), PointerRNA *ptr)
{
	Scene *scene = (Scene *) ptr->id.data;
	RenderData *rd = &scene->r;

	BKE_ffmpeg_codec_settings_verify(rd);
}
#endif

static int rna_RenderSettings_active_layer_index_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	return rd->actlay;
}

static void rna_RenderSettings_active_layer_index_set(PointerRNA *ptr, int value)
{
	RenderData *rd = (RenderData *)ptr->data;
	int num_layers = BLI_listbase_count(&rd->layers);
	rd->actlay = min_ff(value, num_layers - 1);
}

static void rna_RenderSettings_active_layer_index_range(PointerRNA *ptr, int *min, int *max,
                                                        int *UNUSED(softmin), int *UNUSED(softmax))
{
	RenderData *rd = (RenderData *)ptr->data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&rd->layers) - 1);
}

static PointerRNA rna_RenderSettings_active_layer_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	SceneRenderLayer *srl = BLI_findlink(&rd->layers, rd->actlay);
	
	return rna_pointer_inherit_refine(ptr, &RNA_SceneRenderLayer, srl);
}

static void rna_RenderSettings_active_layer_set(PointerRNA *ptr, PointerRNA value)
{
	RenderData *rd = (RenderData *)ptr->data;
	SceneRenderLayer *srl = (SceneRenderLayer *)value.data;
	const int index = BLI_findindex(&rd->layers, srl);
	if (index != -1) rd->actlay = index;
}

static SceneRenderLayer *rna_RenderLayer_new(ID *id, RenderData *UNUSED(rd), const char *name)
{
	Scene *scene = (Scene *)id;
	SceneRenderLayer *srl = BKE_scene_add_render_layer(scene, name);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

	return srl;
}

static void rna_RenderLayer_remove(ID *id, RenderData *UNUSED(rd), Main *bmain, ReportList *reports,
                                   PointerRNA *srl_ptr)
{
	SceneRenderLayer *srl = srl_ptr->data;
	Scene *scene = (Scene *)id;

	if (!BKE_scene_remove_render_layer(bmain, scene, srl)) {
		BKE_reportf(reports, RPT_ERROR, "Render layer '%s' could not be removed from scene '%s'",
		            srl->name, scene->id.name + 2);
		return;
	}

	RNA_POINTER_INVALIDATE(srl_ptr);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static void rna_RenderSettings_engine_set(PointerRNA *ptr, int value)
{
	RenderData *rd = (RenderData *)ptr->data;
	RenderEngineType *type = BLI_findlink(&R_engines, value);

	if (type)
		BLI_strncpy_utf8(rd->engine, type->idname, sizeof(rd->engine));
}

static EnumPropertyItem *rna_RenderSettings_engine_itemf(bContext *UNUSED(C), PointerRNA *UNUSED(ptr),
                                                         PropertyRNA *UNUSED(prop), bool *r_free)
{
	RenderEngineType *type;
	EnumPropertyItem *item = NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	int a = 0, totitem = 0;

	for (type = R_engines.first; type; type = type->next, a++) {
		tmp.value = a;
		tmp.identifier = type->idname;
		tmp.name = type->name;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}
	
	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int rna_RenderSettings_engine_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	RenderEngineType *type;
	int a = 0;

	for (type = R_engines.first; type; type = type->next, a++)
		if (strcmp(type->idname, rd->engine) == 0)
			return a;
	
	return 0;
}

static void rna_RenderSettings_engine_update(Main *bmain, Scene *UNUSED(unused), PointerRNA *UNUSED(ptr))
{
	ED_render_engine_changed(bmain);
}

static void rna_Scene_glsl_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;

	DAG_id_tag_update(&scene->id, 0);
}

static void rna_Scene_freestyle_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;

	DAG_id_tag_update(&scene->id, 0);
}

static void rna_Scene_use_view_map_cache_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
#ifdef WITH_FREESTYLE
	FRS_free_view_map_cache();
#endif
}

static void rna_SceneRenderLayer_name_set(PointerRNA *ptr, const char *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	SceneRenderLayer *rl = (SceneRenderLayer *)ptr->data;
	char oldname[sizeof(rl->name)];

	BLI_strncpy(oldname, rl->name, sizeof(rl->name));

	BLI_strncpy_utf8(rl->name, value, sizeof(rl->name));
	BLI_uniquename(&scene->r.layers, rl, DATA_("RenderLayer"), '.', offsetof(SceneRenderLayer, name), sizeof(rl->name));

	if (scene->nodetree) {
		bNode *node;
		int index = BLI_findindex(&scene->r.layers, rl);

		for (node = scene->nodetree->nodes.first; node; node = node->next) {
			if (node->type == CMP_NODE_R_LAYERS && node->id == NULL) {
				if (node->custom1 == index)
					BLI_strncpy(node->name, rl->name, NODE_MAXSTR);
			}
		}
	}

	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename(NULL, "render.layers", oldname, rl->name);
}

static char *rna_SceneRenderLayer_path(PointerRNA *ptr)
{
	SceneRenderLayer *srl = (SceneRenderLayer *)ptr->data;
	char name_esc[sizeof(srl->name) * 2];

	BLI_strescape(name_esc, srl->name, sizeof(name_esc));
	return BLI_sprintfN("render.layers[\"%s\"]", name_esc);
}

static int rna_RenderSettings_multiple_engines_get(PointerRNA *UNUSED(ptr))
{
	return (BLI_listbase_count(&R_engines) > 1);
}

static int rna_RenderSettings_use_shading_nodes_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	return BKE_scene_use_new_shading_nodes(scene);
}

static int rna_RenderSettings_use_game_engine_get(PointerRNA *ptr)
{
	RenderData *rd = (RenderData *)ptr->data;
	RenderEngineType *type;

	for (type = R_engines.first; type; type = type->next)
		if (strcmp(type->idname, rd->engine) == 0)
			return (type->flag & RE_GAME);
	
	return 0;
}

static void rna_SceneRenderLayer_layer_set(PointerRNA *ptr, const int *values)
{
	SceneRenderLayer *rl = (SceneRenderLayer *)ptr->data;
	rl->lay = ED_view3d_scene_layer_set(rl->lay, values, NULL);
}

static void rna_SceneRenderLayer_pass_update(Main *bmain, Scene *activescene, PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;

	if (scene->nodetree)
		ntreeCompositForceHidden(scene->nodetree);
	
	rna_Scene_glsl_update(bmain, activescene, ptr);
}

static void rna_Scene_use_nodes_update(bContext *C, PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;

	if (scene->use_nodes && scene->nodetree == NULL)
		ED_node_composit_default(C, scene);
}

static void rna_Physics_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	Base *base;

	for (base = scene->base.first; base; base = base->next)
		BKE_ptcache_object_reset(scene, base->object, PTCACHE_RESET_DEPSGRAPH);
}

static void rna_Scene_editmesh_select_mode_set(PointerRNA *ptr, const int *value)
{
	Scene *scene = (Scene *)ptr->id.data;
	ToolSettings *ts = (ToolSettings *)ptr->data;
	int flag = (value[0] ? SCE_SELECT_VERTEX : 0) | (value[1] ? SCE_SELECT_EDGE : 0) | (value[2] ? SCE_SELECT_FACE : 0);

	if (flag) {
		ts->selectmode = flag;

		if (scene->basact) {
			Mesh *me = BKE_mesh_from_object(scene->basact->object);
			if (me && me->edit_btmesh && me->edit_btmesh->selectmode != flag) {
				me->edit_btmesh->selectmode = flag;
				EDBM_selectmode_set(me->edit_btmesh);
			}
		}
	}
}

static void rna_Scene_editmesh_select_mode_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	Mesh *me = NULL;

	if (scene->basact) {
		me = BKE_mesh_from_object(scene->basact->object);
		if (me && me->edit_btmesh == NULL)
			me = NULL;
	}

	WM_main_add_notifier(NC_GEOM | ND_SELECT, me);
	WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);
}

static void object_simplify_update(Object *ob)
{
	ModifierData *md;
	ParticleSystem *psys;

	if ((ob->id.flag & LIB_DOIT) == 0) {
		return;
	}

	ob->id.flag &= ~LIB_DOIT;

	for (md = ob->modifiers.first; md; md = md->next) {
		if (ELEM(md->type, eModifierType_Subsurf, eModifierType_Multires, eModifierType_ParticleSystem)) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
	}

	for (psys = ob->particlesystem.first; psys; psys = psys->next)
		psys->recalc |= PSYS_RECALC_CHILD;
	
	if (ob->dup_group) {
		GroupObject *gob;

		for (gob = ob->dup_group->gobject.first; gob; gob = gob->next)
			object_simplify_update(gob->ob);
	}
}

static void rna_Scene_use_simplify_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *sce = ptr->id.data;
	Scene *sce_iter;
	Base *base;

	BKE_main_id_tag_listbase(&bmain->object, true);
	for (SETLOOPER(sce, sce_iter, base))
		object_simplify_update(base->object);
	
	WM_main_add_notifier(NC_GEOM | ND_DATA, NULL);
}

static void rna_Scene_simplify_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *sce = ptr->id.data;

	if (sce->r.mode & R_SIMPLIFY)
		rna_Scene_use_simplify_update(bmain, sce, ptr);
}

static void rna_Scene_use_persistent_data_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *sce = ptr->id.data;

	if (!(sce->r.mode & R_PERSISTENT_DATA))
		RE_FreePersistentData();
}

static int rna_Scene_use_audio_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	return scene->audio.flag & AUDIO_MUTE;
}

static void rna_Scene_use_audio_set(PointerRNA *ptr, int value)
{
	Scene *scene = (Scene *)ptr->data;

	if (value)
		scene->audio.flag |= AUDIO_MUTE;
	else
		scene->audio.flag &= ~AUDIO_MUTE;

	sound_mute_scene(scene, value);
}

static int rna_Scene_sync_mode_get(PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->data;
	if (scene->audio.flag & AUDIO_SYNC)
		return AUDIO_SYNC;
	return scene->flag & SCE_FRAME_DROP;
}

static void rna_Scene_sync_mode_set(PointerRNA *ptr, int value)
{
	Scene *scene = (Scene *)ptr->data;

	if (value == AUDIO_SYNC) {
		scene->audio.flag |= AUDIO_SYNC;
	}
	else if (value == SCE_FRAME_DROP) {
		scene->audio.flag &= ~AUDIO_SYNC;
		scene->flag |= SCE_FRAME_DROP;
	}
	else {
		scene->audio.flag &= ~AUDIO_SYNC;
		scene->flag &= ~SCE_FRAME_DROP;
	}
}

static int rna_GameSettings_auto_start_get(PointerRNA *UNUSED(ptr))
{
	if (G.fileflags & G_FILE_AUTOPLAY)
		return 1;

	return 0;
}

static void rna_GameSettings_auto_start_set(PointerRNA *UNUSED(ptr), int value)
{
	if (value)
		G.fileflags |= G_FILE_AUTOPLAY;
	else
		G.fileflags &= ~G_FILE_AUTOPLAY;
}

static void rna_GameSettings_exit_key_set(PointerRNA *ptr, int value)
{
	GameData *gm = (GameData *)ptr->data;

	if (ISKEYBOARD(value))
		gm->exitkey = value;
}

static TimeMarker *rna_TimeLine_add(Scene *scene, const char name[], int frame)
{
	TimeMarker *marker = MEM_callocN(sizeof(TimeMarker), "TimeMarker");
	marker->flag = SELECT;
	marker->frame = frame;
	BLI_strncpy_utf8(marker->name, name, sizeof(marker->name));
	BLI_addtail(&scene->markers, marker);

	WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
	WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);

	return marker;
}

static void rna_TimeLine_remove(Scene *scene, ReportList *reports, PointerRNA *marker_ptr)
{
	TimeMarker *marker = marker_ptr->data;
	if (BLI_remlink_safe(&scene->markers, marker) == false) {
		BKE_reportf(reports, RPT_ERROR, "Timeline marker '%s' not found in scene '%s'",
		            marker->name, scene->id.name + 2);
		return;
	}

	MEM_freeN(marker);
	RNA_POINTER_INVALIDATE(marker_ptr);

	WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
	WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);
}

static void rna_TimeLine_clear(Scene *scene)
{
	BLI_freelistN(&scene->markers);

	WM_main_add_notifier(NC_SCENE | ND_MARKERS, NULL);
	WM_main_add_notifier(NC_ANIMATION | ND_MARKERS, NULL);
}

static KeyingSet *rna_Scene_keying_set_new(Scene *sce, ReportList *reports, const char idname[], const char name[])
{
	KeyingSet *ks = NULL;

	/* call the API func, and set the active keyingset index */
	ks = BKE_keyingset_add(&sce->keyingsets, idname, name, KEYINGSET_ABSOLUTE, 0);
	
	if (ks) {
		sce->active_keyingset = BLI_listbase_count(&sce->keyingsets);
		return ks;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Keying set could not be added");
		return NULL;
	}
}

static void rna_UnifiedPaintSettings_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	Brush *br = BKE_paint_brush(BKE_paint_get_active(scene));
	WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
}

static void rna_UnifiedPaintSettings_size_set(PointerRNA *ptr, int value)
{
	UnifiedPaintSettings *ups = ptr->data;

	/* scale unprojected radius so it stays consistent with brush size */
	BKE_brush_scale_unprojected_radius(&ups->unprojected_radius,
	                                   value, ups->size);
	ups->size = value;
}

static void rna_UnifiedPaintSettings_unprojected_radius_set(PointerRNA *ptr, float value)
{
	UnifiedPaintSettings *ups = ptr->data;

	/* scale brush size so it stays consistent with unprojected_radius */
	BKE_brush_scale_size(&ups->size, value, ups->unprojected_radius);
	ups->unprojected_radius = value;
}

static void rna_UnifiedPaintSettings_radius_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	/* changing the unified size should invalidate the overlay but also update the brush */
	BKE_paint_invalidate_overlay_all();
	rna_UnifiedPaintSettings_update(bmain, scene, ptr);
}

static char *rna_UnifiedPaintSettings_path(PointerRNA *UNUSED(ptr))
{
	return BLI_strdup("tool_settings.unified_paint_settings");
}

/* generic function to recalc geometry */
static void rna_EditMesh_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	Mesh *me = NULL;

	if (scene->basact) {
		me = BKE_mesh_from_object(scene->basact->object);
		if (me && me->edit_btmesh == NULL)
			me = NULL;
	}

	if (me) {
		DAG_id_tag_update(&me->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_GEOM | ND_DATA, me);
	}
}

static char *rna_MeshStatVis_path(PointerRNA *UNUSED(ptr))
{
	return BLI_strdup("tool_settings.statvis");
}

/* note: without this, when Multi-Paint is activated/deactivated, the colors
 * will not change right away when multiple bones are selected, this function
 * is not for general use and only for the few cases where changing scene
 * settings and NOT for general purpose updates, possibly this should be
 * given its own notifier. */
static void rna_Scene_update_active_object_data(Main *UNUSED(bmain), Scene *scene, PointerRNA *UNUSED(ptr))
{
	Object *ob = OBACT;
	if (ob) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob->id);
	}
}

static void rna_SceneCamera_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Scene *scene = (Scene *)ptr->id.data;
	Object *camera = scene->camera;

	if (camera)
		DAG_id_tag_update(&camera->id, 0);
}

static void rna_SceneSequencer_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	BKE_sequencer_cache_cleanup();
	BKE_sequencer_preprocessed_cache_cleanup();
}

static char *rna_ToolSettings_path(PointerRNA *UNUSED(ptr))
{
	return BLI_strdup("tool_settings");
}

static PointerRNA rna_FreestyleLineSet_linestyle_get(PointerRNA *ptr)
{
	FreestyleLineSet *lineset = (FreestyleLineSet *)ptr->data;

	return rna_pointer_inherit_refine(ptr, &RNA_FreestyleLineStyle, lineset->linestyle);
}

static void rna_FreestyleLineSet_linestyle_set(PointerRNA *ptr, PointerRNA value)
{
	FreestyleLineSet *lineset = (FreestyleLineSet *)ptr->data;

	if (lineset->linestyle)
		lineset->linestyle->id.us--;
	lineset->linestyle = (FreestyleLineStyle *)value.data;
	lineset->linestyle->id.us++;
}

static FreestyleLineSet *rna_FreestyleSettings_lineset_add(ID *id, FreestyleSettings *config, const char *name)
{
	Scene *scene = (Scene *)id;
	FreestyleLineSet *lineset = BKE_freestyle_lineset_add((FreestyleConfig *)config, name);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

	return lineset;
}

static void rna_FreestyleSettings_lineset_remove(ID *id, FreestyleSettings *config, ReportList *reports,
                                                 PointerRNA *lineset_ptr)
{
	FreestyleLineSet *lineset = lineset_ptr->data;
	Scene *scene = (Scene *)id;

	if (!BKE_freestyle_lineset_delete((FreestyleConfig *)config, lineset)) {
		BKE_reportf(reports, RPT_ERROR, "Line set '%s' could not be removed", lineset->name);
		return;
	}

	RNA_POINTER_INVALIDATE(lineset_ptr);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static PointerRNA rna_FreestyleSettings_active_lineset_get(PointerRNA *ptr)
{
	FreestyleConfig *config = (FreestyleConfig *)ptr->data;
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);
	return rna_pointer_inherit_refine(ptr, &RNA_FreestyleLineSet, lineset);
}

static void rna_FreestyleSettings_active_lineset_index_range(PointerRNA *ptr, int *min, int *max,
                                                             int *UNUSED(softmin), int *UNUSED(softmax))
{
	FreestyleConfig *config = (FreestyleConfig *)ptr->data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&config->linesets) - 1);
}

static int rna_FreestyleSettings_active_lineset_index_get(PointerRNA *ptr)
{
	FreestyleConfig *config = (FreestyleConfig *)ptr->data;
	return BKE_freestyle_lineset_get_active_index(config);
}

static void rna_FreestyleSettings_active_lineset_index_set(PointerRNA *ptr, int value)
{
	FreestyleConfig *config = (FreestyleConfig *)ptr->data;
	BKE_freestyle_lineset_set_active_index(config, value);
}

static FreestyleModuleConfig *rna_FreestyleSettings_module_add(ID *id, FreestyleSettings *config)
{
	Scene *scene = (Scene *)id;
	FreestyleModuleConfig *module = BKE_freestyle_module_add((FreestyleConfig *)config);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);

	return module;
}

static void rna_FreestyleSettings_module_remove(ID *id, FreestyleSettings *config, ReportList *reports,
                                                PointerRNA *module_ptr)
{
	Scene *scene = (Scene *)id;
	FreestyleModuleConfig *module = module_ptr->data;

	if (!BKE_freestyle_module_delete((FreestyleConfig *)config, module)) {
		if (module->script)
			BKE_reportf(reports, RPT_ERROR, "Style module '%s' could not be removed", module->script->id.name + 2);
		else
			BKE_reportf(reports, RPT_ERROR, "Style module could not be removed");
		return;
	}

	RNA_POINTER_INVALIDATE(module_ptr);

	DAG_id_tag_update(&scene->id, 0);
	WM_main_add_notifier(NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

#else

static void rna_def_transform_orientation(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TransformOrientation", NULL);
	
	prop = RNA_def_property(srna, "matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "mat");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_3x3);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_ui_text(prop, "Name", "Name of the custom transform orientation");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

static void rna_def_tool_settings(BlenderRNA  *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem uv_select_mode_items[] = {
		{UV_SELECT_VERTEX, "VERTEX", ICON_UV_VERTEXSEL, "Vertex", "Vertex selection mode"},
		{UV_SELECT_EDGE, "EDGE", ICON_UV_EDGESEL, "Edge", "Edge selection mode"},
		{UV_SELECT_FACE, "FACE", ICON_UV_FACESEL, "Face", "Face selection mode"},
		{UV_SELECT_ISLAND, "ISLAND", ICON_UV_ISLANDSEL, "Island", "Island selection mode"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* the construction of this enum is quite special - everything is stored as bitflags,
	 * with 1st position only for for on/off (and exposed as boolean), while others are mutually
	 * exclusive options but which will only have any effect when autokey is enabled
	 */
	static EnumPropertyItem auto_key_items[] = {
		{AUTOKEY_MODE_NORMAL & ~AUTOKEY_ON, "ADD_REPLACE_KEYS", 0, "Add & Replace", ""},
		{AUTOKEY_MODE_EDITKEYS & ~AUTOKEY_ON, "REPLACE_KEYS", 0, "Replace", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem retarget_roll_items[] = {
		{SK_RETARGET_ROLL_NONE, "NONE", 0, "None", "Don't adjust roll"},
		{SK_RETARGET_ROLL_VIEW, "VIEW", 0, "View", "Roll bones to face the view"},
		{SK_RETARGET_ROLL_JOINT, "JOINT", 0, "Joint", "Roll bone to original joint plane offset"},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem sketch_convert_items[] = {
		{SK_CONVERT_CUT_FIXED, "FIXED", 0, "Fixed", "Subdivide stroke in fixed number of bones"},
		{SK_CONVERT_CUT_LENGTH, "LENGTH", 0, "Length", "Subdivide stroke in bones of specific length"},
		{SK_CONVERT_CUT_ADAPTATIVE, "ADAPTIVE", 0, "Adaptive",
		 "Subdivide stroke adaptively, with more subdivision in curvier parts"},
		{SK_CONVERT_RETARGET, "RETARGET", 0, "Retarget", "Retarget template bone chain to stroke"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem edge_tag_items[] = {
		{EDGE_MODE_SELECT, "SELECT", 0, "Select", ""},
		{EDGE_MODE_TAG_SEAM, "SEAM", 0, "Tag Seam", ""},
		{EDGE_MODE_TAG_SHARP, "SHARP", 0, "Tag Sharp", ""},
		{EDGE_MODE_TAG_CREASE, "CREASE", 0, "Tag Crease", ""},
		{EDGE_MODE_TAG_BEVEL, "BEVEL", 0, "Tag Bevel", ""},
		{EDGE_MODE_TAG_FREESTYLE, "FREESTYLE", 0, "Tag Freestyle Edge Mark", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem draw_groupuser_items[] = {
		{OB_DRAW_GROUPUSER_NONE, "NONE", 0, "None", ""},
		{OB_DRAW_GROUPUSER_ACTIVE, "ACTIVE", 0, "Active", "Show vertices with no weights in the active group"},
		{OB_DRAW_GROUPUSER_ALL, "ALL", 0, "All", "Show vertices with no weights in any group"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem vertex_group_select_items[] = {
		{WT_VGROUP_ALL, "ALL", 0, "All", "All Vertex Groups"},
		{WT_VGROUP_BONE_DEFORM, "BONE_DEFORM", 0, "Deform", "Vertex Groups assigned to Deform Bones"},
		{WT_VGROUP_BONE_DEFORM_OFF, "OTHER_DEFORM", 0, "Other", "Vertex Groups assigned to non Deform Bones"},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem gpencil_source_3d_items[] = {
		{GP_TOOL_SOURCE_SCENE, "SCENE", 0, "Scene", 
		 "Grease Pencil data attached to the current scene is used, unless the active object already has Grease Pencil data (i.e. for old files)"},
		{GP_TOOL_SOURCE_OBJECT, "OBJECT", 0, "Object",
		 "Grease Pencil datablocks attached to the active object are used (required using pre 2.73 add-ons, e.g. BSurfaces)"},
		{0, NULL, 0, NULL, NULL}
	};


	srna = RNA_def_struct(brna, "ToolSettings", NULL);
	RNA_def_struct_path_func(srna, "rna_ToolSettings_path");
	RNA_def_struct_ui_text(srna, "Tool Settings", "");
	
	prop = RNA_def_property(srna, "sculpt", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Sculpt");
	RNA_def_property_ui_text(prop, "Sculpt", "");
	
	prop = RNA_def_property(srna, "use_auto_normalize", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "auto_normalize", 1);
	RNA_def_property_ui_text(prop, "WPaint Auto-Normalize",
	                         "Ensure all bone-deforming vertex groups add up "
	                         "to 1.0 while weight painting");
	RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

	prop = RNA_def_property(srna, "use_multipaint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "multipaint", 1);
	RNA_def_property_ui_text(prop, "WPaint Multi-Paint",
	                         "Paint across all selected bones while "
	                         "weight painting");
	RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

	prop = RNA_def_property(srna, "vertex_group_user", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "weightuser");
	RNA_def_property_enum_items(prop, draw_groupuser_items);
	RNA_def_property_ui_text(prop, "Mask Non-Group Vertices", "Display unweighted vertices (multi-paint overrides)");
	RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

	prop = RNA_def_property(srna, "vertex_group_subset", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vgroupsubset");
	RNA_def_property_enum_items(prop, vertex_group_select_items);
	RNA_def_property_ui_text(prop, "Subset", "Filter Vertex groups for Display");
	RNA_def_property_update(prop, 0, "rna_Scene_update_active_object_data");

	prop = RNA_def_property(srna, "vertex_paint", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vpaint");	RNA_def_property_ui_text(prop, "Vertex Paint", "");

	prop = RNA_def_property(srna, "weight_paint", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "wpaint");
	RNA_def_property_ui_text(prop, "Weight Paint", "");

	prop = RNA_def_property(srna, "image_paint", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "imapaint");
	RNA_def_property_ui_text(prop, "Image Paint", "");

	prop = RNA_def_property(srna, "uv_sculpt", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "uvsculpt");
	RNA_def_property_ui_text(prop, "UV Sculpt", "");

	prop = RNA_def_property(srna, "particle_edit", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "particle");
	RNA_def_property_ui_text(prop, "Particle Edit", "");

	prop = RNA_def_property(srna, "use_uv_sculpt", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_uv_sculpt", 1);
	RNA_def_property_ui_text(prop, "UV Sculpt", "Enable brush for UV sculpting");
	RNA_def_property_ui_icon(prop, ICON_TPAINT_HLT, 0);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_uv_sculpt_update");

	prop = RNA_def_property(srna, "uv_sculpt_lock_borders", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "uv_sculpt_settings", UV_SCULPT_LOCK_BORDERS);
	RNA_def_property_ui_text(prop, "Lock Borders", "Disable editing of boundary edges");

	prop = RNA_def_property(srna, "uv_sculpt_all_islands", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "uv_sculpt_settings", UV_SCULPT_ALL_ISLANDS);
	RNA_def_property_ui_text(prop, "Sculpt All Islands", "Brush operates on all islands");

	prop = RNA_def_property(srna, "uv_sculpt_tool", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "uv_sculpt_tool");
	RNA_def_property_enum_items(prop, uv_sculpt_tool_items);
	RNA_def_property_ui_text(prop, "UV Sculpt Tools", "Select Tools for the UV sculpt brushes");

	prop = RNA_def_property(srna, "uv_relax_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "uv_relax_method");
	RNA_def_property_enum_items(prop, uv_sculpt_relaxation_items);
	RNA_def_property_ui_text(prop, "Relaxation Method", "Algorithm used for UV relaxation");

	/* Transform */
	prop = RNA_def_property(srna, "proportional_edit", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "proportional");
	RNA_def_property_enum_items(prop, proportional_editing_items);
	RNA_def_property_ui_text(prop, "Proportional Editing",
	                         "Proportional Editing mode, allows transforms with distance fall-off");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_proportional_edit_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "proportional_objects", 0);
	RNA_def_property_ui_text(prop, "Proportional Editing Objects", "Proportional editing object mode");
	RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_proportional_edit_mask", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "proportional_mask", 0);
	RNA_def_property_ui_text(prop, "Proportional Editing Objects", "Proportional editing mask mode");
	RNA_def_property_ui_icon(prop, ICON_PROP_OFF, 1);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "proportional_edit_falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "prop_mode");
	RNA_def_property_enum_items(prop, proportional_falloff_items);
	RNA_def_property_ui_text(prop, "Proportional Editing Falloff", "Falloff type for proportional editing mode");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "proportional_size", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "proportional_size");
	RNA_def_property_ui_text(prop, "Proportional Size", "Display size for proportional editing circle");
	RNA_def_property_range(prop, 0.00001, 5000.0);
	
	prop = RNA_def_property(srna, "normal_size", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "normalsize");
	RNA_def_property_ui_text(prop, "Normal Size", "Display size for normals in the 3D view");
	RNA_def_property_range(prop, 0.00001, 1000.0);
	RNA_def_property_ui_range(prop, 0.01, 10.0, 10.0, 2);
	RNA_def_property_update(prop, NC_GEOM | ND_DATA, NULL);

	prop = RNA_def_property(srna, "double_threshold", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "doublimit");
	RNA_def_property_ui_text(prop, "Double Threshold", "Limit for removing duplicates and 'Auto Merge'");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 0.1, 0.01, 6);

	prop = RNA_def_property(srna, "use_mesh_automerge", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "automerge", 0);
	RNA_def_property_ui_text(prop, "AutoMerge Editing", "Automatically merge vertices moved to the same location");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_snap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP);
	RNA_def_property_ui_text(prop, "Snap", "Snap during transform");
	RNA_def_property_ui_icon(prop, ICON_SNAP_OFF, 1);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_snap_align_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_ROTATE);
	RNA_def_property_ui_text(prop, "Snap Align Rotation", "Align rotation with the snapping target");
	RNA_def_property_ui_icon(prop, ICON_SNAP_NORMAL, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "snap_element", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "snap_mode");
	RNA_def_property_enum_items(prop, snap_element_items);
	RNA_def_property_ui_text(prop, "Snap Element", "Type of element to snap to");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */
	
	/* node editor uses own set of snap modes */
	prop = RNA_def_property(srna, "snap_node_element", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "snap_node_mode");
	RNA_def_property_enum_items(prop, snap_node_element_items);
	RNA_def_property_ui_text(prop, "Snap Node Element", "Type of element to snap to");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */
	
	/* image editor uses own set of snap modes */
	prop = RNA_def_property(srna, "snap_uv_element", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "snap_uv_mode");
	RNA_def_property_enum_items(prop, snap_uv_element_items);
	RNA_def_property_ui_text(prop, "Snap UV Element", "Type of element to snap to");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "snap_target", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "snap_target");
	RNA_def_property_enum_items(prop, snap_target_items);
	RNA_def_property_ui_text(prop, "Snap Target", "Which part to snap onto the target");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_snap_peel_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_PEEL_OBJECT);
	RNA_def_property_ui_text(prop, "Snap Peel Object", "Consider objects as whole when finding volume center");
	RNA_def_property_ui_icon(prop, ICON_SNAP_PEEL_OBJECT, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */
	
	prop = RNA_def_property(srna, "use_snap_project", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "snap_flag", SCE_SNAP_PROJECT);
	RNA_def_property_ui_text(prop, "Project Individual Elements",
	                         "Project individual elements on the surface of other objects");
	RNA_def_property_ui_icon(prop, ICON_RETOPO, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	prop = RNA_def_property(srna, "use_snap_self", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "snap_flag", SCE_SNAP_NO_SELF);
	RNA_def_property_ui_text(prop, "Project to Self", "Snap onto itself (editmode)");
	RNA_def_property_ui_icon(prop, ICON_ORTHO, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* header redraw */

	/* Grease Pencil */
	prop = RNA_def_property(srna, "use_grease_pencil_sessions", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gpencil_flags", GP_TOOL_FLAG_PAINTSESSIONS_ON);
	RNA_def_property_ui_text(prop, "Use Sketching Sessions",
	                         "Allow drawing multiple strokes at a time with Grease Pencil");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL); /* xxx: need toolbar to be redrawn... */
	
	prop = RNA_def_property(srna, "grease_pencil_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "gpencil_src");
	RNA_def_property_enum_items(prop, gpencil_source_3d_items);
	RNA_def_property_ui_text(prop, "Grease Pencil Source",
	                         "Datablock where active Grease Pencil data is found from");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);
	
	/* Auto Keying */
	prop = RNA_def_property(srna, "use_keyframe_insert_auto", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "autokey_mode", AUTOKEY_ON);
	RNA_def_property_ui_text(prop, "Auto Keying", "Automatic keyframe insertion for Objects and Bones");
	RNA_def_property_ui_icon(prop, ICON_REC, 0);
	
	prop = RNA_def_property(srna, "auto_keying_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "autokey_mode");
	RNA_def_property_enum_items(prop, auto_key_items);
	RNA_def_property_ui_text(prop, "Auto-Keying Mode", "Mode of automatic keyframe insertion for Objects and Bones");
	
	prop = RNA_def_property(srna, "use_record_with_nla", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", ANIMRECORD_FLAG_WITHNLA);
	RNA_def_property_ui_text(prop, "Layered",
	                         "Add a new NLA Track + Strip for every loop/pass made over the animation "
	                         "to allow non-destructive tweaking");
	
	prop = RNA_def_property(srna, "use_keyframe_insert_keyingset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_ONLYKEYINGSET);
	RNA_def_property_ui_text(prop, "Auto Keyframe Insert Keying Set",
	                         "Automatic keyframe insertion using active Keying Set only");
	RNA_def_property_ui_icon(prop, ICON_KEYINGSET, 0);
	
	/* UV */
	prop = RNA_def_property(srna, "uv_select_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "uv_selectmode");
	RNA_def_property_enum_items(prop, uv_select_mode_items);
	RNA_def_property_ui_text(prop, "UV Selection Mode", "UV selection and display mode");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

	prop = RNA_def_property(srna, "use_uv_select_sync", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "uv_flag", UV_SYNC_SELECTION);
	RNA_def_property_ui_text(prop, "UV Sync Selection", "Keep UV and edit mode mesh selection in sync");
	RNA_def_property_ui_icon(prop, ICON_EDIT, 0);
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

	prop = RNA_def_property(srna, "show_uv_local_view", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "uv_flag", UV_SHOW_SAME_IMAGE);
	RNA_def_property_ui_text(prop, "UV Local View", "Draw only faces with the currently displayed image assigned");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

	/* Mesh */
	prop = RNA_def_property(srna, "mesh_select_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selectmode", 1);
	RNA_def_property_array(prop, 3);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Scene_editmesh_select_mode_set");
	RNA_def_property_ui_text(prop, "Mesh Selection Mode", "Which mesh elements selection works on");
	RNA_def_property_update(prop, 0, "rna_Scene_editmesh_select_mode_update");

	prop = RNA_def_property(srna, "vertex_group_weight", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "vgroup_weight");
	RNA_def_property_ui_text(prop, "Vertex Group Weight", "Weight to assign in vertex groups");

	/* use with MESH_OT_shortest_path_pick */
	prop = RNA_def_property(srna, "edge_path_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "edge_mode");
	RNA_def_property_enum_items(prop, edge_tag_items);
	RNA_def_property_ui_text(prop, "Edge Tag Mode", "The edge flag to tag when selecting the shortest path");

	prop = RNA_def_property(srna, "edge_path_live_unwrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_mode_live_unwrap", 1);
	RNA_def_property_ui_text(prop, "Live Unwrap", "Changing edges seam re-calculates UV unwrap");

	/* etch-a-ton */
	prop = RNA_def_property(srna, "use_bone_sketching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bone_sketching", BONE_SKETCHING);
	RNA_def_property_ui_text(prop, "Use Bone Sketching", "Use sketching to create and edit bones");
/*	RNA_def_property_ui_icon(prop, ICON_EDIT, 0); */
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	prop = RNA_def_property(srna, "use_etch_quick", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bone_sketching", BONE_SKETCHING_QUICK);
	RNA_def_property_ui_text(prop, "Quick Sketching", "Automatically convert and delete on stroke end");

	prop = RNA_def_property(srna, "use_etch_overdraw", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bone_sketching", BONE_SKETCHING_ADJUST);
	RNA_def_property_ui_text(prop, "Overdraw Sketching", "Adjust strokes by drawing near them");
	
	prop = RNA_def_property(srna, "use_etch_autoname", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skgen_retarget_options", SK_RETARGET_AUTONAME);
	RNA_def_property_ui_text(prop, "Autoname Bones", "Automatically generate values to replace &N and &S suffix placeholders in template names");

	prop = RNA_def_property(srna, "etch_number", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "skgen_num_string");
	RNA_def_property_ui_text(prop, "Number", "Text to replace &N with (e.g. 'Finger.&N' -> 'Finger.1' or 'Finger.One')");

	prop = RNA_def_property(srna, "etch_side", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "skgen_num_string");
	RNA_def_property_ui_text(prop, "Side", "Text to replace &S with (e.g. 'Arm.&S' -> 'Arm.R' or 'Arm.Right')");

	prop = RNA_def_property(srna, "etch_template", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "skgen_template");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Scene_skgen_etch_template_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Template", "Template armature that will be retargeted to the stroke");

	prop = RNA_def_property(srna, "etch_subdivision_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "skgen_subdivision_number");
	RNA_def_property_range(prop, 1, 255);
	RNA_def_property_ui_text(prop, "Subdivisions", "Number of bones in the subdivided stroke");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	prop = RNA_def_property(srna, "etch_adaptive_limit", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "skgen_correlation_limit");
	RNA_def_property_range(prop, 0.00001, 1.0);
	RNA_def_property_ui_range(prop, 0.01, 1.0, 0.01, 2);
	RNA_def_property_ui_text(prop, "Limit", "Correlation threshold for number of bones in the subdivided stroke");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	prop = RNA_def_property(srna, "etch_length_limit", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "skgen_length_limit");
	RNA_def_property_range(prop, 0.00001, 100000.0);
	RNA_def_property_ui_range(prop, 0.001, 100.0, 0.1, 3);
	RNA_def_property_ui_text(prop, "Length", "Maximum length of the subdivided bones");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	prop = RNA_def_property(srna, "etch_roll_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "skgen_retarget_roll");
	RNA_def_property_enum_items(prop, retarget_roll_items);
	RNA_def_property_ui_text(prop, "Retarget roll mode", "Method used to adjust the roll of bones when retargeting");
	
	prop = RNA_def_property(srna, "etch_convert_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "bone_sketching_convert");
	RNA_def_property_enum_items(prop, sketch_convert_items);
	RNA_def_property_ui_text(prop, "Stroke conversion method", "Method used to convert stroke to bones");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	/* Unified Paint Settings */
	prop = RNA_def_property(srna, "unified_paint_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "UnifiedPaintSettings");
	RNA_def_property_ui_text(prop, "Unified Paint Settings", NULL);

	/* Mesh Statistics */
	prop = RNA_def_property(srna, "statvis", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "MeshStatVis");
	RNA_def_property_ui_text(prop, "Mesh Statistics Visualization", NULL);
}

static void rna_def_unified_paint_settings(BlenderRNA  *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "UnifiedPaintSettings", NULL);
	RNA_def_struct_path_func(srna, "rna_UnifiedPaintSettings_path");
	RNA_def_struct_ui_text(srna, "Unified Paint Settings", "Overrides for some of the active brush's settings");

	/* high-level flags to enable or disable unified paint settings */
	prop = RNA_def_property(srna, "use_unified_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_SIZE);
	RNA_def_property_ui_text(prop, "Use Unified Radius",
	                         "Instead of per-brush radius, the radius is shared across brushes");

	prop = RNA_def_property(srna, "use_unified_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_ALPHA);
	RNA_def_property_ui_text(prop, "Use Unified Strength",
	                         "Instead of per-brush strength, the strength is shared across brushes");

	prop = RNA_def_property(srna, "use_unified_weight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_WEIGHT);
	RNA_def_property_ui_text(prop, "Use Unified Weight",
	                         "Instead of per-brush weight, the weight is shared across brushes");

	prop = RNA_def_property(srna, "use_unified_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_COLOR);
	RNA_def_property_ui_text(prop, "Use Unified Color",
	                         "Instead of per-brush color, the color is shared across brushes");

	/* unified paint settings that override the equivalent settings
	 * from the active brush */
	prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_funcs(prop, NULL, "rna_UnifiedPaintSettings_size_set", NULL);
	RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS * 10);
	RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS, 1, -1);
	RNA_def_property_ui_text(prop, "Radius", "Radius of the brush");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_radius_update");

	prop = RNA_def_property(srna, "unprojected_radius", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_funcs(prop, NULL, "rna_UnifiedPaintSettings_unprojected_radius_set", NULL);
	RNA_def_property_range(prop, 0.001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001, 1, 0, -1);
	RNA_def_property_ui_text(prop, "Unprojected Radius", "Radius of brush in Blender units");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_radius_update");

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Strength", "How powerful the effect of the brush is when applied");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
	RNA_def_property_ui_text(prop, "Weight", "Weight to assign in vertex groups");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

	prop = RNA_def_property(srna, "secondary_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "secondary_rgb");
	RNA_def_property_ui_text(prop, "Secondary Color", "");
	RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

	prop = RNA_def_property(srna, "use_pressure_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_BRUSH_SIZE_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Size Pressure", "Enable tablet pressure sensitivity for size");

	prop = RNA_def_property(srna, "use_pressure_strength", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_BRUSH_ALPHA_PRESSURE);
	RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
	RNA_def_property_ui_text(prop, "Strength Pressure", "Enable tablet pressure sensitivity for strength");

	prop = RNA_def_property(srna, "use_locked_size", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", UNIFIED_PAINT_BRUSH_LOCK_SIZE);
	RNA_def_property_ui_text(prop, "Use Blender Units",
	                         "When locked brush stays same size relative to object; "
	                         "when unlocked brush size is given in pixels");
}

static void rna_def_statvis(BlenderRNA  *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem stat_type[] = {
		{SCE_STATVIS_OVERHANG,  "OVERHANG",  0, "Overhang",  ""},
		{SCE_STATVIS_THICKNESS, "THICKNESS", 0, "Thickness", ""},
		{SCE_STATVIS_INTERSECT, "INTERSECT", 0, "Intersect", ""},
		{SCE_STATVIS_DISTORT,   "DISTORT",   0, "Distortion", ""},
		{SCE_STATVIS_SHARP, "SHARP", 0, "Sharp", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MeshStatVis", NULL);
	RNA_def_struct_path_func(srna, "rna_MeshStatVis_path");
	RNA_def_struct_ui_text(srna, "Mesh Visualize Statistics", "");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, stat_type);
	RNA_def_property_ui_text(prop, "Type", "Type of data to visualize/check");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");


	/* overhang */
	prop = RNA_def_property(srna, "overhang_min", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "overhang_min");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Overhang Min", "Minimum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "overhang_max", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "overhang_max");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Overhang Max", "Maximum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "overhang_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "overhang_axis");
	RNA_def_property_enum_items(prop, object_axis_items);
	RNA_def_property_ui_text(prop, "Axis", "");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");


	/* thickness */
	prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "thickness_min");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1000.0);
	RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
	RNA_def_property_ui_text(prop, "Thickness Min", "Minimum for measuring thickness");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "thickness_max");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, 1000.0);
	RNA_def_property_ui_range(prop, 0.0f, 100.0, 0.001, 3);
	RNA_def_property_ui_text(prop, "Thickness Max", "Maximum for measuring thickness");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "thickness_samples", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "thickness_samples");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples to test per face");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	/* distort */
	prop = RNA_def_property(srna, "distort_min", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "distort_min");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Distort Min", "Minimum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "distort_max", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "distort_max");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, 0.0f, DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Distort Max", "Maximum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	/* sharp */
	prop = RNA_def_property(srna, "sharp_min", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "sharp_min");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Distort Min", "Minimum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");

	prop = RNA_def_property(srna, "sharp_max", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "sharp_max");
	RNA_def_property_float_default(prop, 0.5f);
	RNA_def_property_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f));
	RNA_def_property_ui_range(prop, -DEG2RADF(180.0f), DEG2RADF(180.0f), 0.001, 3);
	RNA_def_property_ui_text(prop, "Distort Max", "Maximum angle to display");
	RNA_def_property_update(prop, 0, "rna_EditMesh_update");
}

static void rna_def_unit_settings(BlenderRNA  *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem unit_systems[] = {
		{USER_UNIT_NONE, "NONE", 0, "None", ""},
		{USER_UNIT_METRIC, "METRIC", 0, "Metric", ""},
		{USER_UNIT_IMPERIAL, "IMPERIAL", 0, "Imperial", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem rotation_units[] = {
		{0, "DEGREES", 0, "Degrees", "Use degrees for measuring angles and rotations"},
		{USER_UNIT_ROT_RADIANS, "RADIANS", 0, "Radians", ""},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "UnitSettings", NULL);
	RNA_def_struct_ui_text(srna, "Unit Settings", "");

	/* Units */
	prop = RNA_def_property(srna, "system", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, unit_systems);
	RNA_def_property_ui_text(prop, "Unit System", "The unit system to use for button display");
	RNA_def_property_update(prop, NC_WINDOW, NULL);
	
	prop = RNA_def_property(srna, "system_rotation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, rotation_units);
	RNA_def_property_ui_text(prop, "Rotation Units", "Unit to use for displaying/editing rotation values");
	RNA_def_property_update(prop, NC_WINDOW, NULL);

	prop = RNA_def_property(srna, "scale_length", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_ui_text(prop, "Unit Scale", "Scale to use when converting between blender units and dimensions");
	RNA_def_property_range(prop, 0.00001, 100000.0);
	RNA_def_property_ui_range(prop, 0.001, 100.0, 0.1, 3);
	RNA_def_property_update(prop, NC_WINDOW, NULL);

	prop = RNA_def_property(srna, "use_separate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_UNIT_OPT_SPLIT);
	RNA_def_property_ui_text(prop, "Separate Units", "Display units in pairs (e.g. 1m 0cm)");
	RNA_def_property_update(prop, NC_WINDOW, NULL);
}

void rna_def_render_layer_common(StructRNA *srna, int scene)
{
	PropertyRNA *prop;

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	if (scene) RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SceneRenderLayer_name_set");
	else RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Render layer name");
	RNA_def_struct_name_property(srna, prop);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "material_override", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mat_override");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material Override",
	                         "Material to override all other materials in this render layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "light_override", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "light_override");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Light Override", "Group to override all other lights in this render layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* layers */
	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Visible Layers", "Scene layers included in this render layer");
	if (scene) RNA_def_property_boolean_funcs(prop, NULL, "rna_SceneRenderLayer_layer_set");
	else RNA_def_property_boolean_funcs(prop, NULL, "rna_RenderLayer_layer_set");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	/* this seems to be too much trouble with depsgraph updates/etc. currently (20140423) */
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

	prop = RNA_def_property(srna, "layers_zmask", PROP_BOOLEAN, PROP_LAYER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay_zmask", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Zmask Layers", "Zmask scene layers for solid faces");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "layers_exclude", PROP_BOOLEAN, PROP_LAYER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay_exclude", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_ui_text(prop, "Exclude Layers", "Exclude scene layers from having any influence");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	if (scene) {
		prop = RNA_def_property(srna, "samples", PROP_INT, PROP_UNSIGNED);
		RNA_def_property_ui_text(prop, "Samples", "Override number of render samples for this render layer, "
		                                          "0 will use the scene setting");
		RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

		prop = RNA_def_property(srna, "pass_alpha_threshold", PROP_FLOAT, PROP_FACTOR);
		RNA_def_property_ui_text(prop, "Alpha Threshold",
		                         "Z, Index, normal, UV and vector passes are only affected by surfaces with "
		                         "alpha transparency equal to or higher than this threshold");
		RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	}

	/* layer options */
	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "layflag", SCE_LAY_DISABLE);
	RNA_def_property_ui_text(prop, "Enabled", "Disable or enable the render layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_zmask", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ZMASK);
	RNA_def_property_ui_text(prop, "Zmask", "Only render what's in front of the solid z values");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "invert_zmask", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_NEG_ZMASK);
	RNA_def_property_ui_text(prop, "Zmask Negate",
	                         "For Zmask, only render what is behind solid z values instead of in front");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_all_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ALL_Z);
	RNA_def_property_ui_text(prop, "All Z", "Fill in Z values for solid faces in invisible layers, for masking");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_solid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_SOLID);
	RNA_def_property_ui_text(prop, "Solid", "Render Solid faces in this Layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_halo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_HALO);
	RNA_def_property_ui_text(prop, "Halo", "Render Halos in this Layer (on top of Solid)");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_ztransp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_ZTRA);
	RNA_def_property_ui_text(prop, "ZTransp", "Render Z-Transparent faces in this Layer (on top of Solid and Halos)");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_SKY);
	RNA_def_property_ui_text(prop, "Sky", "Render Sky in this Layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_edge_enhance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_EDGE);
	RNA_def_property_ui_text(prop, "Edge", "Render Edge-enhance in this Layer (only works for Solid faces)");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_strand", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_STRAND);
	RNA_def_property_ui_text(prop, "Strand", "Render Strands in this Layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "layflag", SCE_LAY_FRS);
	RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* passes */
	prop = RNA_def_property(srna, "use_pass_combined", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_COMBINED);
	RNA_def_property_ui_text(prop, "Combined", "Deliver full combined RGBA buffer");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_Z);
	RNA_def_property_ui_text(prop, "Z", "Deliver Z values pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "use_pass_vector", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_VECTOR);
	RNA_def_property_ui_text(prop, "Vector", "Deliver speed vector pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_normal", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_NORMAL);
	RNA_def_property_ui_text(prop, "Normal", "Deliver normal pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_uv", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_UV);
	RNA_def_property_ui_text(prop, "UV", "Deliver texture UV pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_MIST);
	RNA_def_property_ui_text(prop, "Mist", "Deliver mist factor pass (0.0-1.0)");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_object_index", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_INDEXOB);
	RNA_def_property_ui_text(prop, "Object Index", "Deliver object index pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_material_index", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_INDEXMA);
	RNA_def_property_ui_text(prop, "Material Index", "Deliver material index pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_RGBA);
	RNA_def_property_ui_text(prop, "Color", "Deliver shade-less color pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE);
	RNA_def_property_ui_text(prop, "Diffuse", "Deliver diffuse pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SPEC);
	RNA_def_property_ui_text(prop, "Specular", "Deliver specular pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_shadow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SHADOW);
	RNA_def_property_ui_text(prop, "Shadow", "Deliver shadow pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_AO);
	RNA_def_property_ui_text(prop, "AO", "Deliver AO pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "use_pass_reflection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_REFLECT);
	RNA_def_property_ui_text(prop, "Reflection", "Deliver raytraced reflection pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_refraction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_REFRACT);
	RNA_def_property_ui_text(prop, "Refraction", "Deliver raytraced refraction pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_emit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_EMIT);
	RNA_def_property_ui_text(prop, "Emit", "Deliver emission pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_environment", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_ENVIRONMENT);
	RNA_def_property_ui_text(prop, "Environment", "Deliver environment lighting pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_INDIRECT);
	RNA_def_property_ui_text(prop, "Indirect", "Deliver indirect lighting pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_SPEC);
	RNA_def_property_ui_text(prop, "Specular Exclude", "Exclude specular pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_shadow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_SHADOW);
	RNA_def_property_ui_text(prop, "Shadow Exclude", "Exclude shadow pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_AO);
	RNA_def_property_ui_text(prop, "AO Exclude", "Exclude AO pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "exclude_reflection", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_REFLECT);
	RNA_def_property_ui_text(prop, "Reflection Exclude", "Exclude raytraced reflection pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_refraction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_REFRACT);
	RNA_def_property_ui_text(prop, "Refraction Exclude", "Exclude raytraced refraction pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_emit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_EMIT);
	RNA_def_property_ui_text(prop, "Emit Exclude", "Exclude emission pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_environment", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_ENVIRONMENT);
	RNA_def_property_ui_text(prop, "Environment Exclude", "Exclude environment pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "exclude_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pass_xor", SCE_PASS_INDIRECT);
	RNA_def_property_ui_text(prop, "Indirect Exclude", "Exclude indirect pass from combined");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_diffuse_direct", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_DIRECT);
	RNA_def_property_ui_text(prop, "Diffuse Direct", "Deliver diffuse direct pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_diffuse_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_INDIRECT);
	RNA_def_property_ui_text(prop, "Diffuse Indirect", "Deliver diffuse indirect pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_diffuse_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_DIFFUSE_COLOR);
	RNA_def_property_ui_text(prop, "Diffuse Color", "Deliver diffuse color pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_glossy_direct", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_DIRECT);
	RNA_def_property_ui_text(prop, "Glossy Direct", "Deliver glossy direct pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_glossy_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_INDIRECT);
	RNA_def_property_ui_text(prop, "Glossy Indirect", "Deliver glossy indirect pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_glossy_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_GLOSSY_COLOR);
	RNA_def_property_ui_text(prop, "Glossy Color", "Deliver glossy color pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_transmission_direct", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_DIRECT);
	RNA_def_property_ui_text(prop, "Transmission Direct", "Deliver transmission direct pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_transmission_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_INDIRECT);
	RNA_def_property_ui_text(prop, "Transmission Indirect", "Deliver transmission indirect pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_transmission_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_TRANSM_COLOR);
	RNA_def_property_ui_text(prop, "Transmission Color", "Deliver transmission color pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "use_pass_subsurface_direct", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_DIRECT);
	RNA_def_property_ui_text(prop, "Subsurface Direct", "Deliver subsurface direct pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_subsurface_indirect", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_INDIRECT);
	RNA_def_property_ui_text(prop, "Subsurface Indirect", "Deliver subsurface indirect pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "use_pass_subsurface_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "passflag", SCE_PASS_SUBSURFACE_COLOR);
	RNA_def_property_ui_text(prop, "Subsurface Color", "Deliver subsurface color pass");
	if (scene) RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneRenderLayer_pass_update");
	else RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_freestyle_modules(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "FreestyleModules");
	srna = RNA_def_struct(brna, "FreestyleModules", NULL);
	RNA_def_struct_sdna(srna, "FreestyleSettings");
	RNA_def_struct_ui_text(srna, "Style Modules", "A list of style modules (to be applied from top to bottom)");

	func = RNA_def_function(srna, "new", "rna_FreestyleSettings_module_add");
	RNA_def_function_ui_description(func, "Add a style module to scene render layer Freestyle settings");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "module", "FreestyleModuleSettings", "", "Newly created style module");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_FreestyleSettings_module_remove");
	RNA_def_function_ui_description(func, "Remove a style module from scene render layer Freestyle settings");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "module", "FreestyleModuleSettings", "", "Style module to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
}

static void rna_def_freestyle_linesets(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "Linesets");
	srna = RNA_def_struct(brna, "Linesets", NULL);
	RNA_def_struct_sdna(srna, "FreestyleSettings");
	RNA_def_struct_ui_text(srna, "Line Sets", "Line sets for associating lines and style parameters");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "FreestyleLineSet");
	RNA_def_property_pointer_funcs(prop, "rna_FreestyleSettings_active_lineset_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Line Set", "Active line set being displayed");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_FreestyleSettings_active_lineset_index_get",
	                           "rna_FreestyleSettings_active_lineset_index_set",
	                           "rna_FreestyleSettings_active_lineset_index_range");
	RNA_def_property_ui_text(prop, "Active Line Set Index", "Index of active line set slot");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	func = RNA_def_function(srna, "new", "rna_FreestyleSettings_lineset_add");
	RNA_def_function_ui_description(func, "Add a line set to scene render layer Freestyle settings");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_string(func, "name", "LineSet", 0, "", "New name for the line set (not unique)");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Newly created line set");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_FreestyleSettings_lineset_remove");
	RNA_def_function_ui_description(func, "Remove a line set from scene render layer Freestyle settings");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "lineset", "FreestyleLineSet", "", "Line set to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
}

static void rna_def_freestyle_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem edge_type_negation_items[] = {
		{0, "INCLUSIVE", 0, "Inclusive", "Select feature edges satisfying the given edge type conditions"},
		{FREESTYLE_LINESET_FE_NOT, "EXCLUSIVE", 0, "Exclusive",
		                           "Select feature edges not satisfying the given edge type conditions"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem edge_type_combination_items[] = {
		{0, "OR", 0, "Logical OR", "Select feature edges satisfying at least one of edge type conditions"},
		{FREESTYLE_LINESET_FE_AND, "AND", 0, "Logical AND",
		                           "Select feature edges satisfying all edge type conditions"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem group_negation_items[] = {
		{0, "INCLUSIVE", 0, "Inclusive", "Select feature edges belonging to some object in the group"},
		{FREESTYLE_LINESET_GR_NOT, "EXCLUSIVE", 0, "Exclusive",
		                           "Select feature edges not belonging to any object in the group"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem face_mark_negation_items[] = {
		{0, "INCLUSIVE", 0, "Inclusive", "Select feature edges satisfying the given face mark conditions"},
		{FREESTYLE_LINESET_FM_NOT, "EXCLUSIVE", 0, "Exclusive",
		                           "Select feature edges not satisfying the given face mark conditions"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem face_mark_condition_items[] = {
		{0, "ONE", 0, "One Face", "Select a feature edge if either of its adjacent faces is marked"},
		{FREESTYLE_LINESET_FM_BOTH, "BOTH", 0, "Both Faces",
		                            "Select a feature edge if both of its adjacent faces are marked"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem freestyle_ui_mode_items[] = {
		{FREESTYLE_CONTROL_SCRIPT_MODE, "SCRIPT", 0, "Python Scripting Mode",
		                                "Advanced mode for using style modules written in Python"},
		{FREESTYLE_CONTROL_EDITOR_MODE, "EDITOR", 0, "Parameter Editor Mode",
		                                "Basic mode for interactive style parameter editing"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem visibility_items[] = {
		{FREESTYLE_QI_VISIBLE, "VISIBLE", 0, "Visible", "Select visible feature edges"},
		{FREESTYLE_QI_HIDDEN, "HIDDEN", 0, "Hidden", "Select hidden feature edges"},
		{FREESTYLE_QI_RANGE, "RANGE", 0, "QI Range",
		                     "Select feature edges within a range of quantitative invisibility (QI) values"},
		{0, NULL, 0, NULL, NULL}
	};

	/* FreestyleLineSet */

	srna = RNA_def_struct(brna, "FreestyleLineSet", NULL);
	RNA_def_struct_ui_text(srna, "Freestyle Line Set", "Line set for associating lines and style parameters");

	/* access to line style settings is redirected through functions
	 * to allow proper id-buttons functionality
	 */
	prop = RNA_def_property(srna, "linestyle", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "FreestyleLineStyle");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_pointer_funcs(prop, "rna_FreestyleLineSet_linestyle_get",
	                               "rna_FreestyleLineSet_linestyle_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Line Style", "Line style settings");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Line Set Name", "Line set name");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_LINESET_ENABLED);
	RNA_def_property_ui_text(prop, "Render", "Enable or disable this line set during stroke rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_by_visibility", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_VISIBILITY);
	RNA_def_property_ui_text(prop, "Selection by Visibility", "Select feature edges based on visibility");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_by_edge_types", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_EDGE_TYPES);
	RNA_def_property_ui_text(prop, "Selection by Edge Types", "Select feature edges based on edge types");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_by_group", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_GROUP);
	RNA_def_property_ui_text(prop, "Selection by Group", "Select feature edges based on a group of objects");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_by_image_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_IMAGE_BORDER);
	RNA_def_property_ui_text(prop, "Selection by Image Border",
	                         "Select feature edges by image border (less memory consumption)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_by_face_marks", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "selection", FREESTYLE_SEL_FACE_MARK);
	RNA_def_property_ui_text(prop, "Selection by Face Marks", "Select feature edges by face marks");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "edge_type_negation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, edge_type_negation_items);
	RNA_def_property_ui_text(prop, "Edge Type Negation",
	                         "Specify either inclusion or exclusion of feature edges selected by edge types");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "edge_type_combination", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, edge_type_combination_items);
	RNA_def_property_ui_text(prop, "Edge Type Combination",
	                         "Specify a logical combination of selection conditions on feature edge types");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "group");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Group", "A group of objects based on which feature edges are selected");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "group_negation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, group_negation_items);
	RNA_def_property_ui_text(prop, "Group Negation",
	                         "Specify either inclusion or exclusion of feature edges belonging to a group of objects");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "face_mark_negation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, face_mark_negation_items);
	RNA_def_property_ui_text(prop, "Face Mark Negation",
	                         "Specify either inclusion or exclusion of feature edges selected by face marks");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "face_mark_condition", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, face_mark_condition_items);
	RNA_def_property_ui_text(prop, "Face Mark Condition", "Specify a feature edge selection condition based on face marks");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_silhouette", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_SILHOUETTE);
	RNA_def_property_ui_text(prop, "Silhouette", "Select silhouettes (edges at the boundary of visible and hidden faces)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_BORDER);
	RNA_def_property_ui_text(prop, "Border", "Select border edges (open mesh edges)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_crease", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_CREASE);
	RNA_def_property_ui_text(prop, "Crease", "Select crease edges (those between two faces making an angle smaller than the Crease Angle)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_ridge_valley", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_RIDGE_VALLEY);
	RNA_def_property_ui_text(prop, "Ridge & Valley", "Select ridges and valleys (boundary lines between convex and concave areas of surface)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
	RNA_def_property_ui_text(prop, "Suggestive Contour", "Select suggestive contours (almost silhouette/contour edges)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_material_boundary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
	RNA_def_property_ui_text(prop, "Material Boundary", "Select edges at material boundaries");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_CONTOUR);
	RNA_def_property_ui_text(prop, "Contour", "Select contours (outer silhouettes of each object)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_external_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
	RNA_def_property_ui_text(prop, "External Contour", "Select external contours (outer silhouettes of occluding and occluded objects)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "select_edge_mark", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "edge_types", FREESTYLE_FE_EDGE_MARK);
	RNA_def_property_ui_text(prop, "Edge Mark", "Select edge marks (edges annotated by Freestyle edge marks)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_silhouette", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_SILHOUETTE);
	RNA_def_property_ui_text(prop, "Silhouette", "Exclude silhouette edges");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_BORDER);
	RNA_def_property_ui_text(prop, "Border", "Exclude border edges");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_crease", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_CREASE);
	RNA_def_property_ui_text(prop, "Crease", "Exclude crease edges");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_ridge_valley", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_RIDGE_VALLEY);
	RNA_def_property_ui_text(prop, "Ridge & Valley", "Exclude ridges and valleys");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_suggestive_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_SUGGESTIVE_CONTOUR);
	RNA_def_property_ui_text(prop, "Suggestive Contour", "Exclude suggestive contours");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_material_boundary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_MATERIAL_BOUNDARY);
	RNA_def_property_ui_text(prop, "Material Boundary", "Exclude edges at material boundaries");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_CONTOUR);
	RNA_def_property_ui_text(prop, "Contour", "Exclude contours");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_external_contour", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_EXTERNAL_CONTOUR);
	RNA_def_property_ui_text(prop, "External Contour", "Exclude external contours");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "exclude_edge_mark", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "exclude_edge_types", FREESTYLE_FE_EDGE_MARK);
	RNA_def_property_ui_text(prop, "Edge Mark", "Exclude edge marks");
	RNA_def_property_ui_icon(prop, ICON_X, 0);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "visibility", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "qi");
	RNA_def_property_enum_items(prop, visibility_items);
	RNA_def_property_ui_text(prop, "Visibility", "Determine how to use visibility for feature edge selection");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "qi_start", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "qi_start");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "Start", "First QI value of the QI range");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "qi_end", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "qi_end");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_text(prop, "End", "Last QI value of the QI range");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	/* FreestyleModuleSettings */

	srna = RNA_def_struct(brna, "FreestyleModuleSettings", NULL);
	RNA_def_struct_sdna(srna, "FreestyleModuleConfig");
	RNA_def_struct_ui_text(srna, "Freestyle Module", "Style module configuration for specifying a style module");

	prop = RNA_def_property(srna, "script", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Style Module", "Python script to define a style module");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "is_displayed", 1);
	RNA_def_property_ui_text(prop, "Use", "Enable or disable this style module during stroke rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	/* FreestyleSettings */

	srna = RNA_def_struct(brna, "FreestyleSettings", NULL);
	RNA_def_struct_sdna(srna, "FreestyleConfig");
	RNA_def_struct_nested(brna, srna, "SceneRenderLayer");
	RNA_def_struct_ui_text(srna, "Freestyle Settings", "Freestyle settings for a SceneRenderLayer datablock");

	prop = RNA_def_property(srna, "modules", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "modules", NULL);
	RNA_def_property_struct_type(prop, "FreestyleModuleSettings");
	RNA_def_property_ui_text(prop, "Style Modules", "A list of style modules (to be applied from top to bottom)");
	rna_def_freestyle_modules(brna, prop);

	prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, freestyle_ui_mode_items);
	RNA_def_property_ui_text(prop, "Control Mode", "Select the Freestyle control mode");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_CULLING);
	RNA_def_property_ui_text(prop, "Culling", "If enabled, out-of-view edges are ignored");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_suggestive_contours", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_SUGGESTIVE_CONTOURS_FLAG);
	RNA_def_property_ui_text(prop, "Suggestive Contours", "Enable suggestive contours");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_ridges_and_valleys", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_RIDGES_AND_VALLEYS_FLAG);
	RNA_def_property_ui_text(prop, "Ridges and Valleys", "Enable ridges and valleys");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_material_boundaries", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_MATERIAL_BOUNDARIES_FLAG);
	RNA_def_property_ui_text(prop, "Material Boundaries", "Enable material boundaries");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_smoothness", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_FACE_SMOOTHNESS_FLAG);
	RNA_def_property_ui_text(prop, "Face Smoothness", "Take face smoothness into account in view map calculation");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_advanced_options", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_ADVANCED_OPTIONS_FLAG);
	RNA_def_property_ui_text(prop, "Advanced Options",
	                         "Enable advanced edge detection options (sphere radius and Kr derivative epsilon)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "use_view_map_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FREESTYLE_VIEW_MAP_CACHE);
	RNA_def_property_ui_text(prop, "View Map Cache", "Keep the computed view map and avoid re-calculating it if mesh geometry is unchanged");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_view_map_cache_update");

	prop = RNA_def_property(srna, "sphere_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sphere_radius");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Sphere Radius", "Sphere radius for computing curvatures");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "kr_derivative_epsilon", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dkr_epsilon");
	RNA_def_property_range(prop, -1000.0, 1000.0);
	RNA_def_property_ui_text(prop, "Kr Derivative Epsilon", "Kr derivative epsilon for computing suggestive contours");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "crease_angle", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "crease_angle");
	RNA_def_property_range(prop, 0.0, DEG2RAD(180.0));
	RNA_def_property_ui_text(prop, "Crease Angle", "Angular threshold for detecting crease edges");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "linesets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "linesets", NULL);
	RNA_def_property_struct_type(prop, "FreestyleLineSet");
	RNA_def_property_ui_text(prop, "Line Sets", "");
	rna_def_freestyle_linesets(brna, prop);
}

static void rna_def_scene_game_recast_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SceneGameRecastData", NULL);
	RNA_def_struct_sdna(srna, "RecastData");
	RNA_def_struct_nested(brna, srna, "Scene");
	RNA_def_struct_ui_text(srna, "Recast Data", "Recast data for a Game datablock");

	prop = RNA_def_property(srna, "cell_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cellsize");
	RNA_def_property_ui_range(prop, 0.1, 1, 1, 2);
	RNA_def_property_ui_text(prop, "Cell Size", "Rasterized cell size");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "cell_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cellheight");
	RNA_def_property_ui_range(prop, 0.1, 1, 1, 2);
	RNA_def_property_ui_text(prop, "Cell Height", "Rasterized cell height");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "agent_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "agentheight");
	RNA_def_property_ui_range(prop, 0.1, 5, 1, 2);
	RNA_def_property_ui_text(prop, "Agent Height", "Minimum height where the agent can still walk");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "agent_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "agentradius");
	RNA_def_property_ui_range(prop, 0.1, 5, 1, 2);
	RNA_def_property_ui_text(prop, "Agent Radius", "Radius of the agent");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "climb_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "agentmaxclimb");
	RNA_def_property_ui_range(prop, 0.1, 5, 1, 2);
	RNA_def_property_ui_text(prop, "Max Climb", "Maximum height between grid cells the agent can climb");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "slope_max", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "agentmaxslope");
	RNA_def_property_range(prop, 0, M_PI / 2);
	RNA_def_property_ui_text(prop, "Max Slope", "Maximum walkable slope angle");
	RNA_def_property_update(prop, NC_SCENE, NULL);


	prop = RNA_def_property(srna, "region_min_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "regionminsize");
	RNA_def_property_ui_range(prop, 0, 150, 1, 2);
	RNA_def_property_ui_text(prop, "Min Region Size", "Minimum regions size (smaller regions will be deleted)");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "region_merge_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "regionmergesize");
	RNA_def_property_ui_range(prop, 0, 150, 1, 2);
	RNA_def_property_ui_text(prop, "Merged Region Size", "Minimum regions size (smaller regions will be merged)");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "edge_max_len", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "edgemaxlen");
	RNA_def_property_ui_range(prop, 0, 50, 1, 2);
	RNA_def_property_ui_text(prop, "Max Edge Length", "Maximum contour edge length");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "edge_max_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "edgemaxerror");
	RNA_def_property_ui_range(prop, 0.1, 3.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max Edge Error", "Maximum distance error from contour to cells");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "verts_per_poly", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vertsperpoly");
	RNA_def_property_ui_range(prop, 3, 12, 1, -1);
	RNA_def_property_ui_text(prop, "Verts Per Poly", "Max number of vertices per polygon");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "sample_dist", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "detailsampledist");
	RNA_def_property_ui_range(prop, 0.0, 16.0, 1, 2);
	RNA_def_property_ui_text(prop, "Sample Distance", "Detail mesh sample spacing");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "sample_max_error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "detailsamplemaxerror");
	RNA_def_property_ui_range(prop, 0.0, 16.0, 1, 2);
	RNA_def_property_ui_text(prop, "Max Sample Error", "Detail mesh simplification max sample error");
	RNA_def_property_update(prop, NC_SCENE, NULL);
}


static void rna_def_bake_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BakeSettings", NULL);
	RNA_def_struct_sdna(srna, "BakeData");
	RNA_def_struct_nested(brna, srna, "RenderSettings");
	RNA_def_struct_ui_text(srna, "Bake Data", "Bake data for a Scene datablock");

	prop = RNA_def_property(srna, "cage_object", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "cage");
	RNA_def_property_ui_text(prop, "Cage Object", "Object to use as cage "
	                         "instead of calculating the cage from the active object with cage extrusion");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_ui_text(prop, "File Path", "Image filepath to use when saving externally");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "width", PROP_INT, PROP_PIXEL);
	RNA_def_property_range(prop, 4, 10000);
	RNA_def_property_ui_text(prop, "Width", "Horizontal dimension of the baking map");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "height", PROP_INT, PROP_PIXEL);
	RNA_def_property_range(prop, 4, 10000);
	RNA_def_property_ui_text(prop, "Height", "Vertical dimension of the baking map");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "margin", PROP_INT, PROP_PIXEL);
	RNA_def_property_range(prop, 0, SHRT_MAX);
	RNA_def_property_ui_range(prop, 0, 64, 1, 1);
	RNA_def_property_ui_text(prop, "Margin", "Extends the baked result as a post process filter");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "cage_extrusion", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
	RNA_def_property_ui_text(prop, "Cage Extrusion",
	                         "Distance to use for the inward ray cast when using selected to active");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "normal_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_space");
	RNA_def_property_enum_items(prop, normal_space_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Choose normal space for baking");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "normal_r", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[0]");
	RNA_def_property_enum_items(prop, normal_swizzle_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in red channel");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "normal_g", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[1]");
	RNA_def_property_enum_items(prop, normal_swizzle_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in green channel");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "normal_b", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "normal_swizzle[2]");
	RNA_def_property_enum_items(prop, normal_swizzle_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Axis to bake in blue channel");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "im_format");
	RNA_def_property_struct_type(prop, "ImageFormatSettings");
	RNA_def_property_ui_text(prop, "Image Format", "");

	prop = RNA_def_property(srna, "save_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "save_mode");
	RNA_def_property_enum_items(prop, bake_save_mode_items);
	RNA_def_property_ui_text(prop, "Save Mode", "Choose how to save the baking map");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* flags */
	prop = RNA_def_property(srna, "use_selected_to_active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_TO_ACTIVE);
	RNA_def_property_ui_text(prop, "Selected to Active",
	                         "Bake shading on the surface of selected objects to the active object");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_clear", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_CLEAR);
	RNA_def_property_ui_text(prop, "Clear",
	                         "Clear Images before baking (internal only)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_split_materials", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_SPLIT_MAT);
	RNA_def_property_ui_text(prop, "Split Materials",
	                         "Split external images per material (external only)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_automatic_name", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_AUTO_NAME);
	RNA_def_property_ui_text(prop, "Automatic Name",
	                         "Automatically name the output file with the pass type (external only)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_cage", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_BAKE_CAGE);
	RNA_def_property_ui_text(prop, "Cage",
	                         "Cast rays to active object from a cage");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
}

static void rna_def_scene_game_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem aasamples_items[] = {
		{0, "SAMPLES_0", 0, "Off", ""},
		{2, "SAMPLES_2", 0, "2x", ""},
		{4, "SAMPLES_4", 0, "4x", ""},
		{8, "SAMPLES_8", 0, "8x", ""},
		{16, "SAMPLES_16", 0, "16x", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem framing_types_items[] = {
		{SCE_GAMEFRAMING_BARS, "LETTERBOX", 0, "Letterbox",
		                       "Show the entire viewport in the display window, using bar horizontally or vertically"},
		{SCE_GAMEFRAMING_EXTEND, "EXTEND", 0, "Extend",
		                         "Show the entire viewport in the display window, viewing more horizontally "
		                         "or vertically"},
		{SCE_GAMEFRAMING_SCALE, "SCALE", 0, "Scale", "Stretch or squeeze the viewport to fill the display window"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem dome_modes_items[] = {
		{DOME_FISHEYE, "FISHEYE", 0, "Fisheye", ""},
		{DOME_TRUNCATED_FRONT, "TRUNCATED_FRONT", 0, "Front-Truncated", ""},
		{DOME_TRUNCATED_REAR, "TRUNCATED_REAR", 0, "Rear-Truncated", ""},
		{DOME_ENVMAP, "ENVMAP", 0, "Cube Map", ""},
		{DOME_PANORAM_SPH, "PANORAM_SPH", 0, "Spherical Panoramic", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem stereo_modes_items[] = {
		{STEREO_QUADBUFFERED, "QUADBUFFERED", 0, "Quad-Buffer", ""},
		{STEREO_ABOVEBELOW, "ABOVEBELOW", 0, "Above-Below", ""},
		{STEREO_INTERLACED, "INTERLACED", 0, "Interlaced", ""},
		{STEREO_ANAGLYPH, "ANAGLYPH", 0, "Anaglyph", ""},
		{STEREO_SIDEBYSIDE, "SIDEBYSIDE", 0, "Side-by-side", ""},
		{STEREO_VINTERLACE, "VINTERLACE", 0, "Vinterlace", ""},
		{STEREO_3DTVTOPBOTTOM, "3DTVTOPBOTTOM", 0, "3DTV Top-Bottom", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem stereo_items[] = {
		{STEREO_NOSTEREO, "NONE", 0, "None", "Disable Stereo and Dome environments"},
		{STEREO_ENABLED, "STEREO", 0, "Stereo", "Enable Stereo environment"},
		{STEREO_DOME, "DOME", 0, "Dome", "Enable Dome environment"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem physics_engine_items[] = {
		{WOPHY_NONE, "NONE", 0, "None", "Don't use a physics engine"},
		{WOPHY_BULLET, "BULLET", 0, "Bullet", "Use the Bullet physics engine"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem material_items[] = {
		{GAME_MAT_MULTITEX, "MULTITEXTURE", 0, "Multitexture", "Multitexture materials"},
		{GAME_MAT_GLSL, "GLSL", 0, "GLSL", "OpenGL shading language shaders"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem obstacle_simulation_items[] = {
		{OBSTSIMULATION_NONE, "NONE", 0, "None", ""},
		{OBSTSIMULATION_TOI_rays, "RVO_RAYS", 0, "RVO (rays)", ""},
		{OBSTSIMULATION_TOI_cells, "RVO_CELLS", 0, "RVO (cells)", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem vsync_items[] = {
		{VSYNC_OFF, "OFF", 0, "Off", "Disable vsync"},
		{VSYNC_ON, "ON", 0, "On", "Enable vsync"},
		{VSYNC_ADAPTIVE, "ADAPTIVE", 0, "Adaptive", "Enable adaptive vsync (if supported)"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem storage_items[] = {
		{RAS_STORE_AUTO, "AUTO", 0, "Auto Select", "Choose the best supported mode"},
		{RAS_STORE_IMMEDIATE, "IMMEDIATE", 0, "Immediate Mode", "Slowest performance, requires OpenGL (any version)"},
		{RAS_STORE_VA, "VERTEX_ARRAY", 0, "Vertex Arrays", "Better performance, requires at least OpenGL 1.1"},
#if 0  /* XXX VBOS are currently disabled since they cannot beat vertex array with display lists in performance. */
		{RAS_STORE_VBO, "VERTEX_BUFFER_OBJECT", 0, "Vertex Buffer Objects",
		                "Best performance, requires at least OpenGL 1.4"}, 
#endif
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "SceneGameData", NULL);
	RNA_def_struct_sdna(srna, "GameData");
	RNA_def_struct_nested(brna, srna, "Scene");
	RNA_def_struct_ui_text(srna, "Game Data", "Game data for a Scene datablock");
	
	prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "xplay");
	RNA_def_property_range(prop, 4, 10000);
	RNA_def_property_ui_text(prop, "Resolution X", "Number of horizontal pixels in the screen");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "yplay");
	RNA_def_property_range(prop, 4, 10000);
	RNA_def_property_ui_text(prop, "Resolution Y", "Number of vertical pixels in the screen");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "vsync", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "vsync");
	RNA_def_property_enum_items(prop, vsync_items);
	RNA_def_property_ui_text(prop, "Vsync", "Change vsync settings");
	
	prop = RNA_def_property(srna, "samples", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aasamples");
	RNA_def_property_enum_items(prop, aasamples_items);
	RNA_def_property_ui_text(prop, "AA Samples", "The number of AA Samples to use for MSAA");
	
	prop = RNA_def_property(srna, "depth", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "depth");
	RNA_def_property_range(prop, 8, 32);
	RNA_def_property_ui_text(prop, "Bits", "Display bit depth of full screen display");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "exit_key", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "exitkey");
	RNA_def_property_enum_items(prop, event_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_GameSettings_exit_key_set", NULL);
	RNA_def_property_ui_text(prop, "Exit Key", "The key that exits the Game Engine");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "raster_storage", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "raster_storage");
	RNA_def_property_enum_items(prop, storage_items);
	RNA_def_property_ui_text(prop, "Storage", "Set the storage mode used by the rasterizer");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* Do we need it here ? (since we already have it in World */
	prop = RNA_def_property(srna, "frequency", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "freqplay");
	RNA_def_property_range(prop, 4, 2000);
	RNA_def_property_ui_text(prop, "Freq", "Display clock frequency of fullscreen display");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "show_fullscreen", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "playerflag", GAME_PLAYER_FULLSCREEN);
	RNA_def_property_ui_text(prop, "Fullscreen", "Start player in a new fullscreen display");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "use_desktop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "playerflag", GAME_PLAYER_DESKTOP_RESOLUTION);
	RNA_def_property_ui_text(prop, "Desktop", "Use the current desktop resolution in fullscreen mode");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	/* Framing */
	prop = RNA_def_property(srna, "frame_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "framing.type");
	RNA_def_property_enum_items(prop, framing_types_items);
	RNA_def_property_ui_text(prop, "Framing Types", "Select the type of Framing you want");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "frame_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "framing.col");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Framing Color", "Set color of the bars");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* Stereo */
	prop = RNA_def_property(srna, "stereo", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stereoflag");
	RNA_def_property_enum_items(prop, stereo_items);
	RNA_def_property_ui_text(prop, "Stereo Options", "");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "stereo_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "stereomode");
	RNA_def_property_enum_items(prop, stereo_modes_items);
	RNA_def_property_ui_text(prop, "Stereo Mode", "Stereographic techniques");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "stereo_eye_separation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "eyeseparation");
	RNA_def_property_range(prop, 0.01, 5.0);
	RNA_def_property_ui_text(prop, "Eye Separation",
	                         "Set the distance between the eyes - the camera focal distance/30 should be fine");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* Dome */
	prop = RNA_def_property(srna, "dome_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dome.mode");
	RNA_def_property_enum_items(prop, dome_modes_items);
	RNA_def_property_ui_text(prop, "Dome Mode", "Dome physical configurations");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "dome_tessellation", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dome.res");
	RNA_def_property_ui_range(prop, 1, 8, 1, 1);
	RNA_def_property_ui_text(prop, "Tessellation", "Tessellation level - check the generated mesh in wireframe mode");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "dome_buffer_resolution", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dome.resbuf");
	RNA_def_property_ui_range(prop, 0.1, 1.0, 0.1, 2);
	RNA_def_property_ui_text(prop, "Buffer Resolution", "Buffer Resolution - decrease it to increase speed");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "dome_angle", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dome.angle");
	RNA_def_property_ui_range(prop, 90, 250, 1, 1);
	RNA_def_property_ui_text(prop, "Angle", "Field of View of the Dome - it only works in mode Fisheye and Truncated");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "dome_tilt", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dome.tilt");
	RNA_def_property_ui_range(prop, -180, 180, 1, 1);
	RNA_def_property_ui_text(prop, "Tilt", "Camera rotation in horizontal axis");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	prop = RNA_def_property(srna, "dome_text", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "dome.warptext");
	RNA_def_property_struct_type(prop, "Text");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Warp Data", "Custom Warp Mesh data file");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* physics */
	prop = RNA_def_property(srna, "physics_engine", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "physicsEngine");
	RNA_def_property_enum_items(prop, physics_engine_items);
	RNA_def_property_ui_text(prop, "Physics Engine", "Physics engine used for physics simulation in the game engine");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "physics_gravity", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_float_sdna(prop, NULL, "gravity");
	RNA_def_property_ui_range(prop, 0.0, 25.0, 1, 2);
	RNA_def_property_range(prop, 0.0, 10000.0);
	RNA_def_property_ui_text(prop, "Physics Gravity",
	                         "Gravitational constant used for physics simulation in the game engine");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "occlusion_culling_resolution", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "occlusionRes");
	RNA_def_property_range(prop, 128.0, 1024.0);
	RNA_def_property_ui_text(prop, "Occlusion Resolution",
	                         "Size of the occlusion buffer, use higher value for better precision (slower)");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "fps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ticrate");
	RNA_def_property_ui_range(prop, 1, 60, 1, 1);
	RNA_def_property_range(prop, 1, 10000);
	RNA_def_property_ui_text(prop, "Frames Per Second",
	                         "Nominal number of game frames per second "
	                         "(physics fixed timestep = 1/fps, independently of actual frame rate)");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "logic_step_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxlogicstep");
	RNA_def_property_ui_range(prop, 1, 5, 1, 1);
	RNA_def_property_range(prop, 1, 5);
	RNA_def_property_ui_text(prop, "Max Logic Steps",
	                         "Maximum number of logic frame per game frame if graphics slows down the game, "
	                         "higher value allows better synchronization with physics");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "physics_step_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxphystep");
	RNA_def_property_ui_range(prop, 1, 5, 1, 1);
	RNA_def_property_range(prop, 1, 5);
	RNA_def_property_ui_text(prop, "Max Physics Steps",
	                         "Maximum number of physics step per game frame if graphics slows down the game, "
	                         "higher value allows physics to keep up with realtime");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "physics_step_sub", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "physubstep");
	RNA_def_property_range(prop, 1, 50);
	RNA_def_property_ui_range(prop, 1, 5, 1, 1);
	RNA_def_property_ui_text(prop, "Physics Sub Steps",
	                         "Number of simulation substep per physic timestep, "
	                         "higher value give better physics precision");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "deactivation_linear_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lineardeactthreshold");
	RNA_def_property_ui_range(prop, 0.001, 10000.0, 2, 3);
	RNA_def_property_range(prop, 0.001, 10000.0);
	RNA_def_property_ui_text(prop, "Deactivation Linear Threshold",
	                         "Linear velocity that an object must be below before the deactivation timer can start");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "deactivation_angular_threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angulardeactthreshold");
	RNA_def_property_ui_range(prop, 0.001, 10000.0, 2, 3);
	RNA_def_property_range(prop, 0.001, 10000.0);
	RNA_def_property_ui_text(prop, "Deactivation Angular Threshold",
	                         "Angular velocity that an object must be below before the deactivation timer can start");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "deactivation_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "deactivationtime");
	RNA_def_property_ui_range(prop, 0.0, 60.0, 1, 1);
	RNA_def_property_range(prop, 0.0, 60.0);
	RNA_def_property_ui_text(prop, "Deactivation Time",
	                         "Amount of time (in seconds) after which objects with a velocity less than the given "
	                         "threshold will deactivate (0.0 means no deactivation)");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	/* mode */
	/* not used  *//* deprecated !!!!!!!!!!!!! */
	prop = RNA_def_property(srna, "use_occlusion_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_DBVT_CULLING);
	RNA_def_property_ui_text(prop, "DBVT Culling",
	                         "Use optimized Bullet DBVT tree for view frustum and occlusion culling (more efficient, "
	                         "but it can waste unnecessary CPU if the scene doesn't have occluder objects)");
	
	/* not used  *//* deprecated !!!!!!!!!!!!! */
	prop = RNA_def_property(srna, "use_activity_culling", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_ACTIVITY_CULLING);
	RNA_def_property_ui_text(prop, "Activity Culling", "Activity culling is enabled");

	/* not used  *//* deprecated !!!!!!!!!!!!! */
	prop = RNA_def_property(srna, "activity_culling_box_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "activityBoxRadius");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Box Radius",
	                         "Radius of the activity bubble, in Manhattan length "
	                         "(objects outside the box are activity-culled)");

	/* booleans */
	prop = RNA_def_property(srna, "show_debug_properties", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_SHOW_DEBUG_PROPS);
	RNA_def_property_ui_text(prop, "Show Debug Properties",
	                         "Show properties marked for debugging while the game runs");

	prop = RNA_def_property(srna, "show_framerate_profile", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_SHOW_FRAMERATE);
	RNA_def_property_ui_text(prop, "Show Framerate and Profile",
	                         "Show framerate and profiling information while the game runs");

	prop = RNA_def_property(srna, "show_physics_visualization", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_SHOW_PHYSICS);
	RNA_def_property_ui_text(prop, "Show Physics Visualization",
	                         "Show a visualization of physics bounds and interactions");

	prop = RNA_def_property(srna, "show_mouse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_SHOW_MOUSE);
	RNA_def_property_ui_text(prop, "Show Mouse", "Start player with a visible mouse cursor");

	prop = RNA_def_property(srna, "use_frame_rate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_ENABLE_ALL_FRAMES);
	RNA_def_property_ui_text(prop, "Use Frame Rate",
	                         "Respect the frame rate from the Physics panel in the world properties "
	                         "rather than rendering as many frames as possible");

	prop = RNA_def_property(srna, "use_display_lists", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_DISPLAY_LISTS);
	RNA_def_property_ui_text(prop, "Display Lists",
	                         "Use display lists to speed up rendering by keeping geometry on the GPU");

	prop = RNA_def_property(srna, "use_deprecation_warnings", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_IGNORE_DEPRECATION_WARNINGS);
	RNA_def_property_ui_text(prop, "Deprecation Warnings",
	                         "Print warnings when using deprecated features in the python API");

	prop = RNA_def_property(srna, "use_animation_record", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_ENABLE_ANIMATION_RECORD);
	RNA_def_property_ui_text(prop, "Record Animation", "Record animation to F-Curves");

	prop = RNA_def_property(srna, "use_auto_start", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_GameSettings_auto_start_get", "rna_GameSettings_auto_start_set");
	RNA_def_property_ui_text(prop, "Auto Start", "Automatically start game at load time");

	prop = RNA_def_property(srna, "use_restrict_animation_updates", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_RESTRICT_ANIM_UPDATES);
	RNA_def_property_ui_text(prop, "Restrict Animation Updates",
	                         "Restrict the number of animation updates to the animation FPS (this is "
	                         "better for performance, but can cause issues with smooth playback)");
	
	/* materials */
	prop = RNA_def_property(srna, "material_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "matmode");
	RNA_def_property_enum_items(prop, material_items);
	RNA_def_property_ui_text(prop, "Material Mode", "Material mode to use for rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "use_glsl_lights", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_LIGHTS);
	RNA_def_property_ui_text(prop, "GLSL Lights", "Use lights for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_shaders", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_SHADERS);
	RNA_def_property_ui_text(prop, "GLSL Shaders", "Use shaders for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_SHADOWS);
	RNA_def_property_ui_text(prop, "GLSL Shadows", "Use shadows for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_ramps", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_RAMPS);
	RNA_def_property_ui_text(prop, "GLSL Ramps", "Use ramps for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_NODES);
	RNA_def_property_ui_text(prop, "GLSL Nodes", "Use nodes for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_color_management", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_COLOR_MANAGEMENT);
	RNA_def_property_ui_text(prop, "GLSL Color Management", "Use color management for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_glsl_extra_textures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_GLSL_NO_EXTRA_TEX);
	RNA_def_property_ui_text(prop, "GLSL Extra Textures",
	                         "Use extra textures like normal or specular maps for GLSL rendering");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "use_material_caching", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GAME_NO_MATERIAL_CACHING);
	RNA_def_property_ui_text(prop, "Use Material Caching",
	                         "Cache materials in the converter (this is faster, but can cause problems with older "
	                         "Singletexture and Multitexture games)");

	/* obstacle simulation */
	prop = RNA_def_property(srna, "obstacle_simulation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "obstacleSimulation");
	RNA_def_property_enum_items(prop, obstacle_simulation_items);
	RNA_def_property_ui_text(prop, "Obstacle simulation", "Simulation used for obstacle avoidance in the game engine");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "level_height", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_float_sdna(prop, NULL, "levelHeight");
	RNA_def_property_range(prop, 0.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Level height",
	                         "Max difference in heights of obstacles to enable their interaction");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "show_obstacle_simulation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GAME_SHOW_OBSTACLE_SIMULATION);
	RNA_def_property_ui_text(prop, "Visualization", "Enable debug visualization for obstacle simulation");

	/* Recast Settings */
	prop = RNA_def_property(srna, "recast_data", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "recastData");
	RNA_def_property_struct_type(prop, "SceneGameRecastData");
	RNA_def_property_ui_text(prop, "Recast Data", "");

	/* Nestled Data  */
	rna_def_scene_game_recast_data(brna);
}

static void rna_def_scene_render_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SceneRenderLayer", NULL);
	RNA_def_struct_ui_text(srna, "Scene Render Layer", "Render layer");
	RNA_def_struct_ui_icon(srna, ICON_RENDERLAYERS);
	RNA_def_struct_path_func(srna, "rna_SceneRenderLayer_path");

	rna_def_render_layer_common(srna, 1);

	/* Freestyle */
	rna_def_freestyle_settings(brna);

	prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "freestyleConfig");
	RNA_def_property_struct_type(prop, "FreestyleSettings");
	RNA_def_property_ui_text(prop, "Freestyle Settings", "");
}

/* Render Layers */
static void rna_def_render_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "RenderLayers");
	srna = RNA_def_struct(brna, "RenderLayers", NULL);
	RNA_def_struct_sdna(srna, "RenderData");
	RNA_def_struct_ui_text(srna, "Render Layers", "Collection of render layers");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "actlay");
	RNA_def_property_int_funcs(prop, "rna_RenderSettings_active_layer_index_get",
	                           "rna_RenderSettings_active_layer_index_set",
	                           "rna_RenderSettings_active_layer_index_range");
	RNA_def_property_ui_text(prop, "Active Layer Index", "Active index in render layer array");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "SceneRenderLayer");
	RNA_def_property_pointer_funcs(prop, "rna_RenderSettings_active_layer_get",
	                               "rna_RenderSettings_active_layer_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_ui_text(prop, "Active Render Layer", "Active Render Layer");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");

	func = RNA_def_function(srna, "new", "rna_RenderLayer_new");
	RNA_def_function_ui_description(func, "Add a render layer to scene");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_string(func, "name", "RenderLayer", 0, "", "New name for the render layer (not unique)");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_pointer(func, "result", "SceneRenderLayer", "", "Newly created render layer");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_RenderLayer_remove");
	RNA_def_function_ui_description(func, "Remove a render layer");
	RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
	parm = RNA_def_pointer(func, "layer", "SceneRenderLayer", "", "Render layer to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);
}

/* use for render output and image save operator,
 * note: there are some cases where the members act differently when this is
 * used from a scene, video formats can only be selected for render output
 * for example, this is checked by seeing if the ptr->id.data is a Scene id */

static void rna_def_scene_image_format_data(BlenderRNA *brna)
{

#ifdef WITH_OPENJPEG
	static EnumPropertyItem jp2_codec_items[] = {
		{R_IMF_JP2_CODEC_JP2, "JP2", 0, "JP2", ""},
		{R_IMF_JP2_CODEC_J2K, "J2K", 0, "J2K", ""},
		{0, NULL, 0, NULL, NULL}
	};
#endif

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ImageFormatSettings", NULL);
	RNA_def_struct_sdna(srna, "ImageFormatData");
	RNA_def_struct_nested(brna, srna, "Scene");
	/* RNA_def_struct_path_func(srna, "rna_RenderSettings_path");  *//* no need for the path, its not animated! */
	RNA_def_struct_ui_text(srna, "Image Format", "Settings for image formats");

	prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "imtype");
	RNA_def_property_enum_items(prop, image_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_ImageFormatSettings_file_format_set",
	                            "rna_ImageFormatSettings_file_format_itemf");
	RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "planes");
	RNA_def_property_enum_items(prop, image_color_mode_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_color_mode_itemf");
	RNA_def_property_ui_text(prop, "Color Mode",
	                         "Choose BW for saving grayscale images, RGB for saving red, green and blue channels, "
	                         "and RGBA for saving red, green, blue and alpha channels");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "color_depth", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "depth");
	RNA_def_property_enum_items(prop, image_color_depth_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_ImageFormatSettings_color_depth_itemf");
	RNA_def_property_ui_text(prop, "Color Depth", "Bit depth per channel");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* was 'file_quality' */
	prop = RNA_def_property(srna, "quality", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "quality");
	RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
	RNA_def_property_ui_text(prop, "Quality", "Quality for image formats that support lossy compression");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* was shared with file_quality */
	prop = RNA_def_property(srna, "compression", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "compress");
	RNA_def_property_range(prop, 0, 100); /* 0 is needed for compression. */
	RNA_def_property_ui_text(prop, "Compression", "Amount of time to determine best compression: "
	                                              "0 = no compression with fast file output, "
	                                              "100 = maximum lossless compression with slow file output");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* flag */
	prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_IMF_FLAG_ZBUF);
	RNA_def_property_ui_text(prop, "Z Buffer", "Save the z-depth per pixel (32 bit unsigned int z-buffer)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", R_IMF_FLAG_PREVIEW_JPG);
	RNA_def_property_ui_text(prop, "Preview", "When rendering animations, save JPG preview images in same directory");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* format specific */

#ifdef WITH_OPENEXR
	/* OpenEXR */

	prop = RNA_def_property(srna, "exr_codec", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "exr_codec");
	RNA_def_property_enum_items(prop, exr_codec_items);
	RNA_def_property_ui_text(prop, "Codec", "Codec settings for OpenEXR");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

#endif

#ifdef WITH_OPENJPEG
	/* Jpeg 2000 */
	prop = RNA_def_property(srna, "use_jpeg2k_ycc", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_YCC);
	RNA_def_property_ui_text(prop, "YCC", "Save luminance-chrominance-chrominance channels instead of RGB colors");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_jpeg2k_cinema_preset", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_CINE_PRESET);
	RNA_def_property_ui_text(prop, "Cinema", "Use Openjpeg Cinema Preset");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_jpeg2k_cinema_48", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "jp2_flag", R_IMF_JP2_FLAG_CINE_48);
	RNA_def_property_ui_text(prop, "Cinema (48)", "Use Openjpeg Cinema Preset (48fps)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "jpeg2k_codec", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "jp2_codec");
	RNA_def_property_enum_items(prop, jp2_codec_items);
	RNA_def_property_ui_text(prop, "Codec", "Codec settings for Jpek2000");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#endif

	/* Cineon and DPX */

	prop = RNA_def_property(srna, "use_cineon_log", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cineon_flag", R_IMF_CINEON_FLAG_LOG);
	RNA_def_property_ui_text(prop, "Log", "Convert to logarithmic color space");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "cineon_black", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "cineon_black");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "B", "Log conversion reference blackpoint");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "cineon_white", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "cineon_white");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "W", "Log conversion reference whitepoint");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "cineon_gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cineon_gamma");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "G", "Log conversion gamma");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* color management */
	prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "view_settings");
	RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
	RNA_def_property_ui_text(prop, "View Settings", "Color management settings applied on image before saving");

	prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "display_settings");
	RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
	RNA_def_property_ui_text(prop, "Display Settings", "Settings of device saved image would be displayed on");
}

static void rna_def_scene_ffmpeg_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

#ifdef WITH_FFMPEG
	static EnumPropertyItem ffmpeg_format_items[] = {
		{FFMPEG_MPEG1, "MPEG1", 0, "MPEG-1", ""},
		{FFMPEG_MPEG2, "MPEG2", 0, "MPEG-2", ""},
		{FFMPEG_MPEG4, "MPEG4", 0, "MPEG-4", ""},
		{FFMPEG_AVI, "AVI", 0, "AVI", ""},
		{FFMPEG_MOV, "QUICKTIME", 0, "Quicktime", ""},
		{FFMPEG_DV, "DV", 0, "DV", ""},
		{FFMPEG_H264, "H264", 0, "H.264", ""},
		{FFMPEG_XVID, "XVID", 0, "Xvid", ""},
		{FFMPEG_OGG, "OGG", 0, "Ogg", ""},
		{FFMPEG_MKV, "MKV", 0, "Matroska", ""},
		{FFMPEG_FLV, "FLASH", 0, "Flash", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem ffmpeg_codec_items[] = {
		{AV_CODEC_ID_NONE, "NONE", 0, "None", ""},
		{AV_CODEC_ID_MPEG1VIDEO, "MPEG1", 0, "MPEG-1", ""},
		{AV_CODEC_ID_MPEG2VIDEO, "MPEG2", 0, "MPEG-2", ""},
		{AV_CODEC_ID_MPEG4, "MPEG4", 0, "MPEG-4(divx)", ""},
		{AV_CODEC_ID_HUFFYUV, "HUFFYUV", 0, "HuffYUV", ""},
		{AV_CODEC_ID_DVVIDEO, "DV", 0, "DV", ""},
		{AV_CODEC_ID_H264, "H264", 0, "H.264", ""},
		{AV_CODEC_ID_THEORA, "THEORA", 0, "Theora", ""},
		{AV_CODEC_ID_FLV1, "FLASH", 0, "Flash Video", ""},
		{AV_CODEC_ID_FFV1, "FFV1", 0, "FFmpeg video codec #1", ""},
		{AV_CODEC_ID_QTRLE, "QTRLE", 0, "QT rle / QT Animation", ""},
		{AV_CODEC_ID_DNXHD, "DNXHD", 0, "DNxHD", ""},
		{AV_CODEC_ID_PNG, "PNG", 0, "PNG", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem ffmpeg_audio_codec_items[] = {
		{AV_CODEC_ID_NONE, "NONE", 0, "None", ""},
		{AV_CODEC_ID_MP2, "MP2", 0, "MP2", ""},
		{AV_CODEC_ID_MP3, "MP3", 0, "MP3", ""},
		{AV_CODEC_ID_AC3, "AC3", 0, "AC3", ""},
		{AV_CODEC_ID_AAC, "AAC", 0, "AAC", ""},
		{AV_CODEC_ID_VORBIS, "VORBIS", 0, "Vorbis", ""},
		{AV_CODEC_ID_FLAC, "FLAC", 0, "FLAC", ""},
		{AV_CODEC_ID_PCM_S16LE, "PCM", 0, "PCM", ""},
		{0, NULL, 0, NULL, NULL}
	};
#endif

	static EnumPropertyItem audio_channel_items[] = {
		{1, "MONO", 0, "Mono", "Set audio channels to mono"},
		{2, "STEREO", 0, "Stereo", "Set audio channels to stereo"},
		{4, "SURROUND4", 0, "4 Channels", "Set audio channels to 4 channels"},
		{6, "SURROUND51", 0, "5.1 Surround", "Set audio channels to 5.1 surround sound"},
		{8, "SURROUND71", 0, "7.1 Surround", "Set audio channels to 7.1 surround sound"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "FFmpegSettings", NULL);
	RNA_def_struct_sdna(srna, "FFMpegCodecData");
	RNA_def_struct_ui_text(srna, "FFmpeg Settings", "FFmpeg related settings for the scene");

#ifdef WITH_FFMPEG
	prop = RNA_def_property(srna, "format", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "type");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, ffmpeg_format_items);
	RNA_def_property_ui_text(prop, "Format", "Output file format");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_FFmpegSettings_codec_settings_update");

	prop = RNA_def_property(srna, "codec", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "codec");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, ffmpeg_codec_items);
	RNA_def_property_ui_text(prop, "Codec", "FFmpeg codec to use");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_FFmpegSettings_codec_settings_update");

	prop = RNA_def_property(srna, "video_bitrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "video_bitrate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Bitrate", "Video bitrate (kb/s)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "minrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rc_min_rate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Min Rate", "Rate control: min rate (kb/s)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "maxrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rc_max_rate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Max Rate", "Rate control: max rate (kb/s)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "muxrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mux_rate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 100000000);
	RNA_def_property_ui_text(prop, "Mux Rate", "Mux rate (bits/s(!))");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "gopsize", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gop_size");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "GOP Size", "Distance between key frames");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "buffersize", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "rc_buffer_size");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 2000);
	RNA_def_property_ui_text(prop, "Buffersize", "Rate control: buffer size (kb)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "packetsize", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mux_packet_size");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0, 16384);
	RNA_def_property_ui_text(prop, "Mux Packet Size", "Mux packet size (byte)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_autosplit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FFMPEG_AUTOSPLIT_OUTPUT);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Autosplit Output", "Autosplit output at 2GB boundary");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_lossless_output", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", FFMPEG_LOSSLESS_OUTPUT);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_FFmpegSettings_lossless_output_set");
	RNA_def_property_ui_text(prop, "Lossless Output", "Use lossless output for video streams");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* FFMPEG Audio*/
	prop = RNA_def_property(srna, "audio_codec", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audio_codec");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, ffmpeg_audio_codec_items);
	RNA_def_property_ui_text(prop, "Audio Codec", "FFmpeg audio codec to use");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_bitrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "audio_bitrate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 32, 384);
	RNA_def_property_ui_text(prop, "Bitrate", "Audio bitrate (kb/s)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "audio_volume");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Volume", "Audio volume");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_SOUND);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#endif

	/* the following two "ffmpeg" settings are general audio settings */
	prop = RNA_def_property(srna, "audio_mixrate", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "audio_mixrate");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 8000, 192000);
	RNA_def_property_ui_text(prop, "Samplerate", "Audio samplerate(samples/s)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_channels", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "audio_channels");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, audio_channel_items);
	RNA_def_property_ui_text(prop, "Audio Channels", "Audio channel count");
}

#ifdef WITH_QUICKTIME
static void rna_def_scene_quicktime_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem quicktime_codec_type_items[] = {
		{0, "codec", 0, "codec", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem quicktime_audio_samplerate_items[] = {
		{22050, "22050", 0, "22kHz", ""},
		{44100, "44100", 0, "44.1kHz", ""},
		{48000, "48000", 0, "48kHz", ""},
		{88200, "88200", 0, "88.2kHz", ""},
		{96000, "96000", 0, "96kHz", ""},
		{192000, "192000", 0, "192kHz", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem quicktime_audio_bitdepth_items[] = {
		{AUD_FORMAT_U8, "8BIT", 0, "8bit", ""},
		{AUD_FORMAT_S16, "16BIT", 0, "16bit", ""},
		{AUD_FORMAT_S24, "24BIT", 0, "24bit", ""},
		{AUD_FORMAT_S32, "32BIT", 0, "32bit", ""},
		{AUD_FORMAT_FLOAT32, "FLOAT32", 0, "float32", ""},
		{AUD_FORMAT_FLOAT64, "FLOAT64", 0, "float64", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem quicktime_audio_bitrate_items[] = {
		{64000, "64000", 0, "64kbps", ""},
		{112000, "112000", 0, "112kpbs", ""},
		{128000, "128000", 0, "128kbps", ""},
		{192000, "192000", 0, "192kbps", ""},
		{256000, "256000", 0, "256kbps", ""},
		{320000, "320000", 0, "320kbps", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* QuickTime */
	srna = RNA_def_struct(brna, "QuickTimeSettings", NULL);
	RNA_def_struct_sdna(srna, "QuicktimeCodecSettings");
	RNA_def_struct_ui_text(srna, "QuickTime Settings", "QuickTime related settings for the scene");

	prop = RNA_def_property(srna, "codec_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "codecType");
	RNA_def_property_enum_items(prop, quicktime_codec_type_items);
	RNA_def_property_enum_funcs(prop, "rna_RenderSettings_qtcodecsettings_codecType_get",
	                            "rna_RenderSettings_qtcodecsettings_codecType_set",
	                            "rna_RenderSettings_qtcodecsettings_codecType_itemf");
	RNA_def_property_ui_text(prop, "Codec", "QuickTime codec type");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "codec_spatial_quality", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "codecSpatialQuality");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Spatial quality", "Intra-frame spatial quality level");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audiocodec_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audiocodecType");
	RNA_def_property_enum_items(prop, quicktime_codec_type_items);
	RNA_def_property_enum_funcs(prop, "rna_RenderSettings_qtcodecsettings_audiocodecType_get",
	                            "rna_RenderSettings_qtcodecsettings_audiocodecType_set",
	                            "rna_RenderSettings_qtcodecsettings_audiocodecType_itemf");
	RNA_def_property_ui_text(prop, "Audio Codec", "QuickTime audio codec type");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_samplerate", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audioSampleRate");
	RNA_def_property_enum_items(prop, quicktime_audio_samplerate_items);
	RNA_def_property_ui_text(prop, "Smp Rate", "Sample Rate");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_bitdepth", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audioBitDepth");
	RNA_def_property_enum_items(prop, quicktime_audio_bitdepth_items);
	RNA_def_property_ui_text(prop, "Bit Depth", "Bit Depth");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_resampling_hq", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "audioCodecFlags", QTAUDIO_FLAG_RESAMPLE_NOHQ);
	RNA_def_property_ui_text(prop, "HQ", "Use High Quality resampling algorithm");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_codec_isvbr", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "audioCodecFlags", QTAUDIO_FLAG_CODEC_ISCBR);
	RNA_def_property_ui_text(prop, "VBR", "Use Variable Bit Rate compression (improves quality at same bitrate)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "audio_bitrate", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audioBitRate");
	RNA_def_property_enum_items(prop, quicktime_audio_bitrate_items);
	RNA_def_property_ui_text(prop, "Bitrate", "Compressed audio bitrate");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
}
#endif

static void rna_def_scene_render_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem pixel_filter_items[] = {
		{R_FILTER_BOX, "BOX", 0, "Box", "Use a box filter for anti-aliasing"},
		{R_FILTER_TENT, "TENT", 0, "Tent", "Use a tent filter for anti-aliasing"},
		{R_FILTER_QUAD, "QUADRATIC", 0, "Quadratic", "Use a quadratic filter for anti-aliasing"},
		{R_FILTER_CUBIC, "CUBIC", 0, "Cubic", "Use a cubic filter for anti-aliasing"},
		{R_FILTER_CATROM, "CATMULLROM", 0, "Catmull-Rom", "Use a Catmull-Rom filter for anti-aliasing"},
		{R_FILTER_GAUSS, "GAUSSIAN", 0, "Gaussian", "Use a Gaussian filter for anti-aliasing"},
		{R_FILTER_MITCH, "MITCHELL", 0, "Mitchell-Netravali", "Use a Mitchell-Netravali filter for anti-aliasing"},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem alpha_mode_items[] = {
		{R_ADDSKY, "SKY", 0, "Sky", "Transparent pixels are filled with sky color"},
		{R_ALPHAPREMUL, "TRANSPARENT", 0, "Transparent", "World background is transparent with premultiplied alpha"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem display_mode_items[] = {
		{R_OUTPUT_SCREEN, "SCREEN", 0, "Full Screen", "Images are rendered in full Screen"},
		{R_OUTPUT_AREA, "AREA", 0, "Image Editor", "Images are rendered in Image Editor"},
		{R_OUTPUT_WINDOW, "WINDOW", 0, "New Window", "Images are rendered in new Window"},
		{R_OUTPUT_NONE, "NONE", 0, "Keep UI", "Images are rendered without forcing UI changes"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* Bake */
	static EnumPropertyItem bake_mode_items[] = {
		{RE_BAKE_ALL, "FULL", 0, "Full Render", "Bake everything"},
		{RE_BAKE_AO, "AO", 0, "Ambient Occlusion", "Bake ambient occlusion"},
		{RE_BAKE_SHADOW, "SHADOW", 0, "Shadow", "Bake shadows"},
		{RE_BAKE_NORMALS, "NORMALS", 0, "Normals", "Bake normals"},
		{RE_BAKE_TEXTURE, "TEXTURE", 0, "Textures", "Bake textures"},
		{RE_BAKE_DISPLACEMENT, "DISPLACEMENT", 0, "Displacement", "Bake displacement"},
		{RE_BAKE_DERIVATIVE, "DERIVATIVE", 0, "Derivative", "Bake derivative map"},
		{RE_BAKE_VERTEX_COLORS, "VERTEX_COLORS", 0, "Vertex Colors", "Bake vertex colors"},
		{RE_BAKE_EMIT, "EMIT", 0, "Emission", "Bake Emit values (glow)"},
		{RE_BAKE_ALPHA, "ALPHA", 0, "Alpha", "Bake Alpha values (transparency)"},
		{RE_BAKE_MIRROR_INTENSITY, "MIRROR_INTENSITY", 0, "Mirror Intensity", "Bake Mirror values"},
		{RE_BAKE_MIRROR_COLOR, "MIRROR_COLOR", 0, "Mirror Colors", "Bake Mirror colors"},
		{RE_BAKE_SPEC_INTENSITY, "SPEC_INTENSITY", 0, "Specular Intensity", "Bake Specular values"},
		{RE_BAKE_SPEC_COLOR, "SPEC_COLOR", 0, "Specular Colors", "Bake Specular colors"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bake_normal_space_items[] = {
		{R_BAKE_SPACE_CAMERA, "CAMERA", 0, "Camera", "Bake the normals in camera space"},
		{R_BAKE_SPACE_WORLD, "WORLD", 0, "World", "Bake the normals in world space"},
		{R_BAKE_SPACE_OBJECT, "OBJECT", 0, "Object", "Bake the normals in object space"},
		{R_BAKE_SPACE_TANGENT, "TANGENT", 0, "Tangent", "Bake the normals in tangent space"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bake_qyad_split_items[] = {
		{0, "AUTO", 0, "Automatic", "Split quads to give the least distortion while baking"},
		{1, "FIXED", 0, "Fixed", "Split quads predictably (0,1,2) (0,2,3)"},
		{2, "FIXED_ALT", 0, "Fixed Alternate", "Split quads predictably (1,2,3) (1,3,0)"},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem octree_resolution_items[] = {
		{64, "64", 0, "64", ""},
		{128, "128", 0, "128", ""},
		{256, "256", 0, "256", ""},
		{512, "512", 0, "512", ""},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem raytrace_structure_items[] = {
		{R_RAYSTRUCTURE_AUTO, "AUTO", 0, "Auto", "Automatically select acceleration structure"},
		{R_RAYSTRUCTURE_OCTREE, "OCTREE", 0, "Octree", "Use old Octree structure"},
		{R_RAYSTRUCTURE_VBVH, "VBVH", 0, "vBVH", "Use vBVH"},
		{R_RAYSTRUCTURE_SIMD_SVBVH, "SIMD_SVBVH", 0, "SIMD SVBVH", "Use SIMD SVBVH"},
		{R_RAYSTRUCTURE_SIMD_QBVH, "SIMD_QBVH", 0, "SIMD QBVH", "Use SIMD QBVH"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem fixed_oversample_items[] = {
		{5, "5", 0, "5", ""},
		{8, "8", 0, "8", ""},
		{11, "11", 0, "11", ""},
		{16, "16", 0, "16", ""},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem field_order_items[] = {
		{0, "EVEN_FIRST", 0, "Upper First", "Upper field first"},
		{R_ODDFIELD, "ODD_FIRST", 0, "Lower First", "Lower field first"},
		{0, NULL, 0, NULL, NULL}
	};
		
	static EnumPropertyItem threads_mode_items[] = {
		{0, "AUTO", 0, "Auto-detect", "Automatically determine the number of threads, based on CPUs"},
		{R_FIXED_THREADS, "FIXED", 0, "Fixed", "Manually determine the number of threads"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem engine_items[] = {
		{0, "BLENDER_RENDER", 0, "Blender Render", "Use the Blender internal rendering engine for rendering"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem freestyle_thickness_items[] = {
		{R_LINE_THICKNESS_ABSOLUTE, "ABSOLUTE", 0, "Absolute", "Specify unit line thickness in pixels"},
		{R_LINE_THICKNESS_RELATIVE, "RELATIVE", 0, "Relative",
		                            "Unit line thickness is scaled by the proportion of the present vertical image "
		                            "resolution to 480 pixels"},
		{0, NULL, 0, NULL, NULL}};

	rna_def_scene_ffmpeg_settings(brna);
#ifdef WITH_QUICKTIME
	rna_def_scene_quicktime_settings(brna);
#endif

	srna = RNA_def_struct(brna, "RenderSettings", NULL);
	RNA_def_struct_sdna(srna, "RenderData");
	RNA_def_struct_nested(brna, srna, "Scene");
	RNA_def_struct_path_func(srna, "rna_RenderSettings_path");
	RNA_def_struct_ui_text(srna, "Render Data", "Rendering settings for a Scene datablock");

	/* Render Data */
	prop = RNA_def_property(srna, "image_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "im_format");
	RNA_def_property_struct_type(prop, "ImageFormatSettings");
	RNA_def_property_ui_text(prop, "Image Format", "");

	prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "xsch");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 4, 65536);
	RNA_def_property_ui_text(prop, "Resolution X", "Number of horizontal pixels in the rendered image");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");
	
	prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "ysch");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 4, 65536);
	RNA_def_property_ui_text(prop, "Resolution Y", "Number of vertical pixels in the rendered image");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");
	
	prop = RNA_def_property(srna, "resolution_percentage", PROP_INT, PROP_PERCENTAGE);
	RNA_def_property_int_sdna(prop, NULL, "size");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, SHRT_MAX);
	RNA_def_property_ui_range(prop, 1, 100, 10, 1);
	RNA_def_property_ui_text(prop, "Resolution %", "Percentage scale for render resolution");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "tile_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "tilex");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 8, 65536);
	RNA_def_property_ui_text(prop, "Tile X", "Horizontal tile size to use while rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "tile_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "tiley");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 8, 65536);
	RNA_def_property_ui_text(prop, "Tile Y", "Vertical tile size to use while rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "preview_start_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 8, 16384);
	RNA_def_property_int_default(prop, 64);
	RNA_def_property_ui_text(prop, "Start Resolution", "Resolution to start rendering preview at, "
	                                                   "progressively increasing it to the full viewport size");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "pixel_aspect_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xasp");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Pixel Aspect X",
	                         "Horizontal aspect ratio - for anamorphic or non-square pixel output");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");
	
	prop = RNA_def_property(srna, "pixel_aspect_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yasp");
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1.0f, 200.0f);
	RNA_def_property_ui_text(prop, "Pixel Aspect Y",
	                         "Vertical aspect ratio - for anamorphic or non-square pixel output");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_SceneCamera_update");

#ifdef WITH_QUICKTIME
	prop = RNA_def_property(srna, "quicktime", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "QuickTimeSettings");
	RNA_def_property_pointer_sdna(prop, NULL, "qtcodecsettings");
	RNA_def_property_flag(prop, PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "QuickTime Settings", "QuickTime related settings for the scene");
#endif

	prop = RNA_def_property(srna, "ffmpeg", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "FFmpegSettings");
	RNA_def_property_pointer_sdna(prop, NULL, "ffcodecdata");
	RNA_def_property_flag(prop, PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "FFmpeg Settings", "FFmpeg related settings for the scene");

	prop = RNA_def_property(srna, "fps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "frs_sec");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 120);
	RNA_def_property_ui_text(prop, "FPS", "Framerate, expressed in frames per second");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");
	
	prop = RNA_def_property(srna, "fps_base", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "frs_sec_base");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.1f, 120.0f);
	RNA_def_property_ui_text(prop, "FPS Base", "Framerate base");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_fps_update");
	
	/* frame mapping */
	prop = RNA_def_property(srna, "frame_map_old", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "framapto");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 900);
	RNA_def_property_ui_text(prop, "Frame Map Old", "Old mapping value in frames");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");
	
	prop = RNA_def_property(srna, "frame_map_new", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "images");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 1, 900);
	RNA_def_property_ui_text(prop, "Frame Map New", "How many frames the Map Old will last");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_framelen_update");

	
	prop = RNA_def_property(srna, "dither_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dither_intensity");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Dither Intensity",
	                         "Amount of dithering noise added to the rendered image to break up banding");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "pixel_filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filtertype");
	RNA_def_property_enum_items(prop, pixel_filter_items);
	RNA_def_property_ui_text(prop, "Pixel Filter", "Reconstruction filter used for combining anti-aliasing samples");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "filter_size", PROP_FLOAT, PROP_PIXEL);
	RNA_def_property_float_sdna(prop, NULL, "gauss");
	RNA_def_property_range(prop, 0.5f, 1.5f);
	RNA_def_property_ui_text(prop, "Filter Size", "Width over which the reconstruction filter combines samples");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "alpha_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "alphamode");
	RNA_def_property_enum_items(prop, alpha_mode_items);
	RNA_def_property_ui_text(prop, "Alpha Mode", "Representation of alpha information in the RGBA pixels");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "octree_resolution", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ocres");
	RNA_def_property_enum_items(prop, octree_resolution_items);
	RNA_def_property_ui_text(prop, "Octree Resolution",
	                         "Resolution of raytrace accelerator, use higher resolutions for larger scenes");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "raytrace_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "raytrace_structure");
	RNA_def_property_enum_items(prop, raytrace_structure_items);
	RNA_def_property_ui_text(prop, "Raytrace Acceleration Structure", "Type of raytrace accelerator structure");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_instances", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "raytrace_options", R_RAYTRACE_USE_INSTANCES);
	RNA_def_property_ui_text(prop, "Use Instances",
	                         "Instance support leads to effective memory reduction when using duplicates");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_local_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "raytrace_options", R_RAYTRACE_USE_LOCAL_COORDS);
	RNA_def_property_ui_text(prop, "Use Local Coords",
	                         "Vertex coordinates are stored locally on each primitive "
	                         "(increases memory usage, but may have impact on speed)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_antialiasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_OSA);
	RNA_def_property_ui_text(prop, "Anti-Aliasing",
	                         "Render and combine multiple samples per pixel to prevent jagged edges");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "antialiasing_samples", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "osa");
	RNA_def_property_enum_items(prop, fixed_oversample_items);
	RNA_def_property_ui_text(prop, "Anti-Aliasing Samples", "Amount of anti-aliasing samples per pixel");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_fields", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_FIELDS);
	RNA_def_property_ui_text(prop, "Fields", "Render image to two fields per frame, for interlaced TV output");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "field_order", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, field_order_items);
	RNA_def_property_ui_text(prop, "Field Order",
	                         "Order of video fields (select which lines get rendered first, "
	                         "to create smooth motion for TV output)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_fields_still", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_FIELDSTILL);
	RNA_def_property_ui_text(prop, "Fields Still", "Disable the time difference between fields");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	/* rendering features */
	prop = RNA_def_property(srna, "use_shadows", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_SHADOW);
	RNA_def_property_ui_text(prop, "Shadows", "Calculate shadows while rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_envmaps", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_ENVMAP);
	RNA_def_property_ui_text(prop, "Environment Maps", "Calculate environment maps while rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_sss", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_SSS);
	RNA_def_property_ui_text(prop, "Subsurface Scattering", "Calculate sub-surface scattering in materials rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_raytrace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_RAYTRACE);
	RNA_def_property_ui_text(prop, "Raytracing",
	                         "Pre-calculate the raytrace accelerator and render raytracing effects");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_textures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "scemode", R_NO_TEX);
	RNA_def_property_ui_text(prop, "Textures", "Use textures to affect material properties");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_edge_enhance", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_EDGE);
	RNA_def_property_ui_text(prop, "Edge", "Create a toon outline around the edges of geometry");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "edge_threshold", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "edgeint");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Edge Threshold", "Threshold for drawing outlines on geometry edges");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "edge_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "edgeR");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Edge Color", "Edge color");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_EDGE_FRS);
	RNA_def_property_ui_text(prop, "Edge", "Draw stylized strokes using Freestyle");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	/* threads */
	prop = RNA_def_property(srna, "threads", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "threads");
	RNA_def_property_range(prop, 1, BLENDER_MAX_THREADS);
	RNA_def_property_int_funcs(prop, "rna_RenderSettings_threads_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Threads",
	                         "Number of CPU threads to use simultaneously while rendering "
	                         "(for multi-core/CPU systems)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "threads_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, threads_mode_items);
	RNA_def_property_enum_funcs(prop, "rna_RenderSettings_threads_mode_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Threads Mode", "Determine the amount of render threads used");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	/* motion blur */
	prop = RNA_def_property(srna, "use_motion_blur", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_MBLUR);
	RNA_def_property_ui_text(prop, "Motion Blur", "Use multi-sampled 3D scene motion blur");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "motion_blur_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "mblur_samples");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Motion Samples", "Number of scene samples to take with motion blur");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	prop = RNA_def_property(srna, "motion_blur_shutter", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "blurfac");
	RNA_def_property_ui_range(prop, 0.01f, 2.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Shutter", "Time taken in frames between shutter open and close");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_glsl_update");
	
	/* border */
	prop = RNA_def_property(srna, "use_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_BORDER);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Border",
	                         "Render a user-defined border region, within the frame size "
	                         "(note that this disables save_buffers and full_sample)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	
	
	prop = RNA_def_property(srna, "border_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "border.xmin");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Border Minimum X", "Minimum X value to for the render border");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "border_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "border.ymin");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Border Minimum Y", "Minimum Y value for the render border");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "border_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "border.xmax");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Border Maximum X", "Maximum X value for the render border");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "border_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "border.ymax");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Border Maximum Y", "Maximum Y value for the render border");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_crop_to_border", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_CROP);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Crop to Border", "Crop the rendered frame to the defined border size");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_placeholder", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_TOUCH);
	RNA_def_property_ui_text(prop, "Placeholders",
	                         "Create empty placeholder files while rendering frames (similar to Unix 'touch')");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "mode", R_NO_OVERWRITE);
	RNA_def_property_ui_text(prop, "Overwrite", "Overwrite existing files while rendering");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_compositing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_DOCOMP);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Compositing",
	                         "Process the render result through the compositing pipeline, "
	                         "if compositing nodes are enabled");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_sequencer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_DOSEQ);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Sequencer",
	                         "Process the render (and composited) result through the video sequence "
	                         "editor pipeline, if sequencer strips exist");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_file_extension", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXTENSION);
	RNA_def_property_ui_text(prop, "File Extensions",
	                         "Add the file format extensions to the rendered file name (eg: filename + .jpg)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

#if 0 /* moved */
	prop = RNA_def_property(srna, "file_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "imtype");
	RNA_def_property_enum_items(prop, image_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_RenderSettings_file_format_set", NULL);
	RNA_def_property_ui_text(prop, "File Format", "File format to save the rendered images as");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
#endif

	prop = RNA_def_property(srna, "file_extension", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_SceneRender_file_ext_get", "rna_SceneRender_file_ext_length", NULL);
	RNA_def_property_ui_text(prop, "Extension", "The file extension used for saving renders");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "is_movie_format", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_is_movie_format_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Movie Format", "When true the format is a movie");

	prop = RNA_def_property(srna, "use_free_image_textures", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_FREE_IMAGE);
	RNA_def_property_ui_text(prop, "Free Image Textures",
	                         "Free all image textures from memory after render, to save memory before compositing");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_free_unused_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_COMP_FREE);
	RNA_def_property_ui_text(prop, "Free Unused Nodes",
	                         "Free Nodes that are not used while compositing, to save memory");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_save_buffers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXR_TILE_FILE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_save_buffers_get", NULL);
	RNA_def_property_ui_text(prop, "Save Buffers",
	                         "Save tiles for all RenderLayers and SceneNodes to files in the temp directory "
	                         "(saves memory, required for Full Sample)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_full_sample", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_FULL_SAMPLE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_full_sample_get", NULL);
	RNA_def_property_ui_text(prop, "Full Sample",
	                         "Save for every anti-aliasing sample the entire RenderLayer results "
	                         "(this solves anti-aliasing issues with compositing)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "displaymode");
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_ui_text(prop, "Display", "Select where rendered images will be displayed");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_lock_interface", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_lock_interface", 1);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
	RNA_def_property_ui_text(prop, "Lock Interface", "Lock interface during rendering in favor of giving more memory to the renderer");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "pic");
	RNA_def_property_ui_text(prop, "Output Path",
	                         "Directory/name to save animations, # characters defines the position "
	                         "and length of frame numbers");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* Render result EXR cache. */
	prop = RNA_def_property(srna, "use_render_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_EXR_CACHE_FILE);
	RNA_def_property_ui_text(prop, "Cache Result",
	                         "Save render cache to EXR files (useful for heavy compositing, "
	                         "Note: affects indirectly rendered scenes)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* Bake */
	
	prop = RNA_def_property(srna, "bake_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "bake_mode");
	RNA_def_property_enum_items(prop, bake_mode_items);
	RNA_def_property_ui_text(prop, "Bake Mode", "Choose shading information to bake into the image");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_normal_space", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "bake_normal_space");
	RNA_def_property_enum_items(prop, bake_normal_space_items);
	RNA_def_property_ui_text(prop, "Normal Space", "Choose normal space for baking");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_quad_split", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, bake_qyad_split_items);
	RNA_def_property_ui_text(prop, "Quad Split", "Choose the method used to split a quad into 2 triangles for baking");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_aa_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "bake_osa");
	RNA_def_property_enum_items(prop, fixed_oversample_items);
	RNA_def_property_ui_text(prop, "Anti-Aliasing Level", "");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_selected_to_active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_TO_ACTIVE);
	RNA_def_property_ui_text(prop, "Selected to Active",
	                         "Bake shading on the surface of selected objects to the active object");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_normalize", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_NORMALIZE);
	RNA_def_property_ui_text(prop, "Normalized",
	                         "With displacement normalize to the distance, with ambient occlusion "
	                         "normalize without using material settings");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_clear", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_CLEAR);
	RNA_def_property_ui_text(prop, "Clear", "Clear Images before baking");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_antialiasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_OSA);
	RNA_def_property_ui_text(prop, "Anti-Aliasing", "Enables Anti-aliasing");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_margin", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "bake_filter");
	RNA_def_property_range(prop, 0, 64);
	RNA_def_property_ui_text(prop, "Margin",
	                         "Extends the baked result as a post process filter");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bake_maxdist");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Distance",
	                         "Maximum distance from active object to other object (in blender units)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bake_biasdist");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Bias", "Bias towards faces further away from the object (in blender units)");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_multires", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_MULTIRES);
	RNA_def_property_ui_text(prop, "Bake from Multires", "Bake directly from multires object");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_lores_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_LORES_MESH);
	RNA_def_property_ui_text(prop, "Low Resolution Mesh",
	                         "Calculate heights against unsubdivided low resolution mesh");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "bake_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bake_samples");
	RNA_def_property_range(prop, 64, 1024);
	RNA_def_property_int_default(prop, 256);
	RNA_def_property_ui_text(prop, "Samples", "Number of samples used for ambient occlusion baking from multires");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_to_vertex_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_VCOL);
	RNA_def_property_ui_text(prop, "Bake to Vertex Color",
	                         "Bake to vertex colors instead of to a UV-mapped image");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_bake_user_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bake_flag", R_BAKE_USERSCALE);
	RNA_def_property_ui_text(prop, "User scale", "Use a user scale for the derivative map");

	prop = RNA_def_property(srna, "bake_user_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "bake_user_scale");
	RNA_def_property_range(prop, 0.0, 1000.0);
	RNA_def_property_ui_text(prop, "Scale",
	                         "Instead of automatically normalizing to 0..1, "
	                         "apply a user scale to the derivative map");

	/* stamp */
	
	prop = RNA_def_property(srna, "use_stamp_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_TIME);
	RNA_def_property_ui_text(prop, "Stamp Time",
	                         "Include the rendered frame timecode as HH:MM:SS.FF in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_date", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_DATE);
	RNA_def_property_ui_text(prop, "Stamp Date", "Include the current date in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_FRAME);
	RNA_def_property_ui_text(prop, "Stamp Frame", "Include the frame number in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_camera", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_CAMERA);
	RNA_def_property_ui_text(prop, "Stamp Camera", "Include the name of the active camera in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_stamp_lens", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_CAMERALENS);
	RNA_def_property_ui_text(prop, "Stamp Lens", "Include the active camera's lens in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_scene", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_SCENE);
	RNA_def_property_ui_text(prop, "Stamp Scene", "Include the name of the active scene in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_note", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_NOTE);
	RNA_def_property_ui_text(prop, "Stamp Note", "Include a custom note in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_marker", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_MARKER);
	RNA_def_property_ui_text(prop, "Stamp Marker", "Include the name of the last marker in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_filename", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_FILENAME);
	RNA_def_property_ui_text(prop, "Stamp Filename", "Include the .blend filename in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "use_stamp_sequencer_strip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_SEQSTRIP);
	RNA_def_property_ui_text(prop, "Stamp Sequence Strip",
	                         "Include the name of the foreground sequence strip in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_stamp_render_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_RENDERTIME);
	RNA_def_property_ui_text(prop, "Stamp Render Time", "Include the render time in image metadata");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "stamp_note_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "stamp_udata");
	RNA_def_property_ui_text(prop, "Stamp Note Text", "Custom text to appear in the stamp note");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "use_stamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "stamp", R_STAMP_DRAW);
	RNA_def_property_ui_text(prop, "Render Stamp", "Render the stamp info text in the rendered image");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "stamp_font_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "stamp_font_id");
	RNA_def_property_range(prop, 8, 64);
	RNA_def_property_ui_text(prop, "Font Size", "Size of the font used when rendering stamp text");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	prop = RNA_def_property(srna, "stamp_foreground", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "fg_stamp");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Text Color", "Color to use for stamp text");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	prop = RNA_def_property(srna, "stamp_background", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "bg_stamp");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Background", "Color to use behind stamp text");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* sequencer draw options */

	prop = RNA_def_property(srna, "use_sequencer_gl_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "seq_flag", R_SEQ_GL_PREV);
	RNA_def_property_ui_text(prop, "Sequencer OpenGL", "");
	RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

#if 0  /* see R_SEQ_GL_REND comment */
	prop = RNA_def_property(srna, "use_sequencer_gl_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "seq_flag", R_SEQ_GL_REND);
	RNA_def_property_ui_text(prop, "Sequencer OpenGL", "");
#endif

	prop = RNA_def_property(srna, "sequencer_gl_preview", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "seq_prev_type");
	RNA_def_property_enum_items(prop, viewport_shade_items);
	RNA_def_property_ui_text(prop, "Sequencer Preview Shading", "Method to draw in the sequencer view");
	RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

	prop = RNA_def_property(srna, "sequencer_gl_render", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "seq_rend_type");
	RNA_def_property_enum_items(prop, viewport_shade_items);
	RNA_def_property_ui_text(prop, "Sequencer Preview Shading", "Method to draw in the sequencer view");

	prop = RNA_def_property(srna, "use_sequencer_gl_textured_solid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "seq_flag", R_SEQ_SOLID_TEX);
	RNA_def_property_ui_text(prop, "Textured Solid", "Draw face-assigned textures in solid draw method");
	RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SceneSequencer_update");

	/* layers */
	prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layers", NULL);
	RNA_def_property_struct_type(prop, "SceneRenderLayer");
	RNA_def_property_ui_text(prop, "Render Layers", "");
	rna_def_render_layers(brna, prop);


	prop = RNA_def_property(srna, "use_single_layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scemode", R_SINGLE_LAYER);
	RNA_def_property_ui_text(prop, "Single Layer", "Only render the active layer");
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	/* engine */
	prop = RNA_def_property(srna, "engine", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, engine_items);
	RNA_def_property_enum_funcs(prop, "rna_RenderSettings_engine_get", "rna_RenderSettings_engine_set",
	                            "rna_RenderSettings_engine_itemf");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Engine", "Engine to use for rendering");
	RNA_def_property_update(prop, NC_WINDOW, "rna_RenderSettings_engine_update");

	prop = RNA_def_property(srna, "has_multiple_engines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_multiple_engines_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Multiple Engines", "More than one rendering engine is available");

	prop = RNA_def_property(srna, "use_shading_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_use_shading_nodes_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Use Shading Nodes", "Active render engine uses new shading nodes system");

	prop = RNA_def_property(srna, "use_game_engine", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_RenderSettings_use_game_engine_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Use Game Engine", "Current rendering engine is a game engine");

	/* simplify */
	prop = RNA_def_property(srna, "use_simplify", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_SIMPLIFY);
	RNA_def_property_ui_text(prop, "Use Simplify", "Enable simplification of scene for quicker preview renders");
	RNA_def_property_update(prop, 0, "rna_Scene_use_simplify_update");

	prop = RNA_def_property(srna, "simplify_subdivision", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "simplify_subsurf");
	RNA_def_property_ui_range(prop, 0, 6, 1, -1);
	RNA_def_property_ui_text(prop, "Simplify Subdivision", "Global maximum subdivision level");
	RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

	prop = RNA_def_property(srna, "simplify_child_particles", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "simplify_particles");
	RNA_def_property_ui_text(prop, "Simplify Child Particles", "Global child particles percentage");
	RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

	prop = RNA_def_property(srna, "simplify_shadow_samples", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "simplify_shadowsamples");
	RNA_def_property_ui_range(prop, 1, 16, 1, -1);
	RNA_def_property_ui_text(prop, "Simplify Shadow Samples", "Global maximum shadow samples");
	RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

	prop = RNA_def_property(srna, "simplify_ao_sss", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "simplify_aosss");
	RNA_def_property_ui_text(prop, "Simplify AO and SSS", "Global approximate AO and SSS quality factor");
	RNA_def_property_update(prop, 0, "rna_Scene_simplify_update");

	prop = RNA_def_property(srna, "use_simplify_triangulate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "simplify_flag", R_SIMPLE_NO_TRIANGULATE);
	RNA_def_property_ui_text(prop, "Skip Quad to Triangles", "Disable non-planar quads being triangulated");

	/* persistent data */
	prop = RNA_def_property(srna, "use_persistent_data", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", R_PERSISTENT_DATA);
	RNA_def_property_ui_text(prop, "Persistent Data", "Keep render data around for faster re-renders");
	RNA_def_property_update(prop, 0, "rna_Scene_use_persistent_data_update");

	/* Freestyle line thickness options */
	prop = RNA_def_property(srna, "line_thickness_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "line_thickness_mode");
	RNA_def_property_enum_items(prop, freestyle_thickness_items);
	RNA_def_property_ui_text(prop, "Line Thickness Mode", "Line thickness mode for Freestyle line drawing");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	prop = RNA_def_property(srna, "line_thickness", PROP_FLOAT, PROP_PIXEL);
	RNA_def_property_float_sdna(prop, NULL, "unit_line_thickness");
	RNA_def_property_range(prop, 0.f, 10000.f);
	RNA_def_property_ui_text(prop, "Line Thickness", "Line thickness in pixels");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_freestyle_update");

	/* Bake Settings */
	prop = RNA_def_property(srna, "bake", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "bake");
	RNA_def_property_struct_type(prop, "BakeSettings");
	RNA_def_property_ui_text(prop, "Bake Data", "");

	/* Nestled Data  */
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_bake_data(brna);
	RNA_define_animate_sdna(true);

	/* *** Animated *** */

	/* Scene API */
	RNA_api_scene_render(srna);
}

/* scene.objects */
static void rna_def_scene_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;
	
	RNA_def_property_srna(cprop, "SceneObjects");
	srna = RNA_def_struct(brna, "SceneObjects", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Scene Objects", "Collection of scene objects");

	func = RNA_def_function(srna, "link", "rna_Scene_object_link");
	RNA_def_function_ui_description(func, "Link object to scene, run scene.update() after");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to add to scene");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);
	parm = RNA_def_pointer(func, "base", "ObjectBase", "", "The newly created base");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "unlink", "rna_Scene_object_unlink");
	RNA_def_function_ui_description(func, "Unlink object from scene");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "object", "Object", "", "Object to remove from scene");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_funcs(prop, "rna_Scene_active_object_get", "rna_Scene_active_object_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Object", "Active object for this scene");
	/* Could call: ED_base_object_activate(C, scene->basact);
	 * but would be a bad level call and it seems the notifier is enough */
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);
}


/* scene.bases.* */
static void rna_def_scene_bases(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

/*	FunctionRNA *func; */
/*	PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "SceneBases");
	srna = RNA_def_struct(brna, "SceneBases", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Scene Bases", "Collection of scene bases");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ObjectBase");
	RNA_def_property_pointer_sdna(prop, NULL, "basact");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Base", "Active object base in the scene");
	RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);
}

/* scene.timeline_markers */
static void rna_def_timeline_markers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "TimelineMarkers");
	srna = RNA_def_struct(brna, "TimelineMarkers", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Timeline Markers", "Collection of timeline markers");

	func = RNA_def_function(srna, "new", "rna_TimeLine_add");
	RNA_def_function_ui_description(func, "Add a keyframe to the curve");
	parm = RNA_def_string(func, "name", "Marker", 0, "", "New name for the marker (not unique)");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm = RNA_def_int(func, "frame", 1, -MAXFRAME, MAXFRAME, "", "The frame for the new marker", -MAXFRAME, MAXFRAME);
	parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Newly created timeline marker");
	RNA_def_function_return(func, parm);


	func = RNA_def_function(srna, "remove", "rna_TimeLine_remove");
	RNA_def_function_ui_description(func, "Remove a timeline marker");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "marker", "TimelineMarker", "", "Timeline marker to remove");
	RNA_def_property_flag(parm, PROP_REQUIRED | PROP_NEVER_NULL | PROP_RNAPTR);
	RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

	func = RNA_def_function(srna, "clear", "rna_TimeLine_clear");
	RNA_def_function_ui_description(func, "Remove all timeline markers");
}

/* scene.keying_sets */
static void rna_def_scene_keying_sets(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "KeyingSets");
	srna = RNA_def_struct(brna, "KeyingSets", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Keying Sets", "Scene keying sets");

	/* Add Keying Set */
	func = RNA_def_function(srna, "new", "rna_Scene_keying_set_new");
	RNA_def_function_ui_description(func, "Add a new Keying Set to Scene");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	/* name */
	RNA_def_string(func, "idname", "KeyingSet", 64, "IDName", "Internal identifier of Keying Set");
	RNA_def_string(func, "name", "KeyingSet", 64, "Name", "User visible name of Keying Set");

	/* returns the new KeyingSet */
	parm = RNA_def_pointer(func, "keyingset", "KeyingSet", "", "Newly created Keying Set");
	RNA_def_function_return(func, parm);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSet");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_Scene_active_keying_set_get",
	                               "rna_Scene_active_keying_set_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
	
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_keyingset");
	RNA_def_property_int_funcs(prop, "rna_Scene_active_keying_set_index_get",
	                           "rna_Scene_active_keying_set_index_set", NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set Index",
	                         "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
}

static void rna_def_scene_keying_sets_all(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	RNA_def_property_srna(cprop, "KeyingSetsAll");
	srna = RNA_def_struct(brna, "KeyingSetsAll", NULL);
	RNA_def_struct_sdna(srna, "Scene");
	RNA_def_struct_ui_text(srna, "Keying Sets All", "All available keying sets");
	
	/* NOTE: no add/remove available here, without screwing up this amalgamated list... */
	
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyingSet");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_Scene_active_keying_set_get",
	                               "rna_Scene_active_keying_set_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
	
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "active_keyingset");
	RNA_def_property_int_funcs(prop, "rna_Scene_active_keying_set_index_get",
	                           "rna_Scene_active_keying_set_index_set", NULL);
	RNA_def_property_ui_text(prop, "Active Keying Set Index",
	                         "Current Keying Set index (negative for 'builtin' and positive for 'absolute')");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
}

/* Runtime property, used to remember uv indices, used only in UV stitch for now.
 */
static void rna_def_selected_uv_element(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SelectedUvElement", "PropertyGroup");
	RNA_def_struct_ui_text(srna, "Selected UV Element", "");

	/* store the index to the UV element selected */
	prop = RNA_def_property(srna, "element_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Element Index", "");

	prop = RNA_def_property(srna, "face_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_flag(prop, PROP_IDPROPERTY);
	RNA_def_property_ui_text(prop, "Face Index", "");
}

static void rna_def_display_safe_areas(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static float default_title[2] = {0.035f, 0.035f};
	static float default_action[2] = {0.1f, 0.05f};

	static float default_title_center[2] = {0.175f, 0.05f};
	static float default_action_center[2] = {0.15f, 0.05f};

	srna = RNA_def_struct(brna, "DisplaySafeAreas", NULL);
	RNA_def_struct_ui_text(srna, "Safe Areas", "Safe Areas used in 3D view and the VSE");
	RNA_def_struct_sdna(srna, "DisplaySafeAreas");

	/* SAFE AREAS */
	prop = RNA_def_property(srna, "title", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "title");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_array_default(prop, default_title);
	RNA_def_property_ui_text(prop, "Title Safe margins", "Safe area for text and graphics");
	RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "action", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "action");
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_array_default(prop, default_action);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Action Safe Margins", "Safe area for general elements");
	RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);


	prop = RNA_def_property(srna, "title_center", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "title_center");
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_array_default(prop, default_title_center);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Center Title Safe Margins", "Safe area for text and graphics in a different aspect ratio");
	RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);

	prop = RNA_def_property(srna, "action_center", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "action_center");
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_array_default(prop, default_action_center);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Center Action Safe Margins", "Safe area for general elements in a different aspect ratio");
	RNA_def_property_update(prop, NC_SCENE | ND_DRAW_RENDER_VIEWPORT, NULL);
}


void RNA_def_scene(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;
	
	static EnumPropertyItem audio_distance_model_items[] = {
		{0, "NONE", 0, "None", "No distance attenuation"},
		{1, "INVERSE", 0, "Inverse", "Inverse distance model"},
		{2, "INVERSE_CLAMPED", 0, "Inverse Clamped", "Inverse distance model with clamping"},
		{3, "LINEAR", 0, "Linear", "Linear distance model"},
		{4, "LINEAR_CLAMPED", 0, "Linear Clamped", "Linear distance model with clamping"},
		{5, "EXPONENT", 0, "Exponent", "Exponent distance model"},
		{6, "EXPONENT_CLAMPED", 0, "Exponent Clamped", "Exponent distance model with clamping"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem sync_mode_items[] = {
		{0, "NONE", 0, "No Sync", "Do not sync, play every frame"},
		{SCE_FRAME_DROP, "FRAME_DROP", 0, "Frame Dropping", "Drop frames if playback is too slow"},
		{AUDIO_SYNC, "AUDIO_SYNC", 0, "AV-sync", "Sync to audio playback, dropping frames"},
		{0, NULL, 0, NULL, NULL}
	};

	/* Struct definition */
	srna = RNA_def_struct(brna, "Scene", "ID");
	RNA_def_struct_ui_text(srna, "Scene",
	                       "Scene data block, consisting in objects and defining time and render related settings");
	RNA_def_struct_ui_icon(srna, ICON_SCENE_DATA);
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
	
	/* Global Settings */
	prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Camera_object_poll");
	RNA_def_property_ui_text(prop, "Camera", "Active camera, used for rendering the scene");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_view3d_update");

	prop = RNA_def_property(srna, "background_set", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "set");
	RNA_def_property_struct_type(prop, "Scene");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_Scene_set_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Background Scene", "Background set scene");
	RNA_def_property_update(prop, NC_SCENE | NA_EDITED, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "world", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "World", "World used for rendering the scene");
	RNA_def_property_update(prop, NC_SCENE | ND_WORLD, "rna_Scene_glsl_update");

	prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "cursor");
	RNA_def_property_ui_text(prop, "Cursor Location", "3D cursor location");
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
	RNA_def_property_update(prop, NC_WINDOW, NULL);
	
	/* Bases/Objects */
	prop = RNA_def_property(srna, "object_bases", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "base", NULL);
	RNA_def_property_struct_type(prop, "ObjectBase");
	RNA_def_property_ui_text(prop, "Bases", "");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, NULL, NULL, NULL,
	                                  "rna_Scene_object_bases_lookup_string", NULL);
	rna_def_scene_bases(brna, prop);

	prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "base", NULL);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_ui_text(prop, "Objects", "");
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, "rna_Scene_objects_get", NULL, NULL, NULL, NULL);
	rna_def_scene_objects(brna, prop);

	/* Layers */
	prop = RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	/* this seems to be too much trouble with depsgraph updates/etc. currently (20110420) */
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Scene_layer_set");
	RNA_def_property_ui_text(prop, "Layers", "Visible layers - Shift-Click/Drag to select multiple layers");
	RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_Scene_layer_update");

	/* active layer */
	prop = RNA_def_property(srna, "active_layer", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_Scene_active_layer_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Layer", "Active scene layer index");

	/* Frame Range Stuff */
	prop = RNA_def_property(srna, "frame_current", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.cfra");
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_int_funcs(prop, NULL, "rna_Scene_frame_current_set", NULL);
	RNA_def_property_ui_text(prop, "Current Frame",
	                         "Current Frame, to update animation data from python frame_set() instead");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, "rna_Scene_frame_update");
	
	prop = RNA_def_property(srna, "frame_subframe", PROP_FLOAT, PROP_TIME);
	RNA_def_property_float_sdna(prop, NULL, "r.subframe");
	RNA_def_property_ui_text(prop, "Current Sub-Frame", "");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	
	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.sfra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Scene_start_frame_set", NULL);
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Start Frame", "First frame of the playback/rendering range");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);
	
	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.efra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Scene_end_frame_set", NULL);
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "End Frame", "Final frame of the playback/rendering range");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);
	
	prop = RNA_def_property(srna, "frame_step", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.frame_step");
	RNA_def_property_range(prop, 0, MAXFRAME);
	RNA_def_property_ui_range(prop, 1, 100, 1, -1);
	RNA_def_property_ui_text(prop, "Frame Step",
	                         "Number of frames to skip forward while rendering/playing back each frame");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	
	prop = RNA_def_property(srna, "frame_current_final", PROP_FLOAT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
	RNA_def_property_range(prop, MINAFRAME, MAXFRAME);
	RNA_def_property_float_funcs(prop, "rna_Scene_frame_current_final_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Current Frame Final",
	                         "Current frame with subframe and time remapping applied");

	prop = RNA_def_property(srna, "lock_frame_selection_to_range", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "r.flag", SCER_LOCK_FRAME_SELECTION);
	RNA_def_property_ui_text(prop, "Lock Frame Selection",
	                         "Don't allow frame to be selected with mouse outside of frame range");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	RNA_def_property_ui_icon(prop, ICON_LOCKED, 0);

	/* Preview Range (frame-range for UI playback) */
	prop = RNA_def_property(srna, "use_preview_range", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "r.flag", SCER_PRV_RANGE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Scene_use_preview_range_set");
	RNA_def_property_ui_text(prop, "Use Preview Range",
	                         "Use an alternative start/end frame range for animation playback and "
	                         "OpenGL renders instead of the Render properties start/end frame range");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	RNA_def_property_ui_icon(prop, ICON_PREVIEW_RANGE, 0);
	
	prop = RNA_def_property(srna, "frame_preview_start", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.psfra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Scene_preview_range_start_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Preview Range Start Frame", "Alternative start frame for UI playback");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	
	prop = RNA_def_property(srna, "frame_preview_end", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "r.pefra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Scene_preview_range_end_frame_set", NULL);
	RNA_def_property_ui_text(prop, "Preview Range End Frame", "Alternative end frame for UI playback");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	
	/* Timeline / Time Navigation settings */
	prop = RNA_def_property(srna, "show_keys_from_selected_only", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SCE_KEYS_NO_SELONLY);
	RNA_def_property_ui_text(prop, "Only Keyframes from Selected Channels",
	                         "Consider keyframes for active Object and/or its selected bones only "
	                         "(in timeline and when jumping between keyframes)");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME, NULL);
	
	/* Stamp */
	prop = RNA_def_property(srna, "use_stamp_note", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "r.stamp_udata");
	RNA_def_property_ui_text(prop, "Stamp Note", "User defined note for the render stamping");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	
	/* Animation Data (for Scene) */
	rna_def_animdata_common(srna);
	
	/* Readonly Properties */
	prop = RNA_def_property(srna, "is_nla_tweakmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_NLA_EDIT_ON);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* DO NOT MAKE THIS EDITABLE, OR NLA EDITOR BREAKS */
	RNA_def_property_ui_text(prop, "NLA TweakMode",
	                         "Whether there is any action referenced by NLA being edited (strictly read-only)");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);
	
	/* Frame dropping flag for playback and sync enum */
	prop = RNA_def_property(srna, "use_frame_drop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SCE_FRAME_DROP);
	RNA_def_property_ui_text(prop, "Frame Dropping", "Play back dropping frames if frame display is too slow");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "sync_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_Scene_sync_mode_get", "rna_Scene_sync_mode_set", NULL);
	RNA_def_property_enum_items(prop, sync_mode_items);
	RNA_def_property_ui_text(prop, "Sync Mode", "How to sync playback");
	RNA_def_property_update(prop, NC_SCENE, NULL);


	/* Nodes (Compositing) */
	prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
	RNA_def_property_ui_text(prop, "Node Tree", "Compositing node tree");

	prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_ui_text(prop, "Use Nodes", "Enable the compositing node tree");
	RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_Scene_use_nodes_update");
	
	/* Sequencer */
	prop = RNA_def_property(srna, "sequence_editor", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ed");
	RNA_def_property_struct_type(prop, "SequenceEditor");
	RNA_def_property_ui_text(prop, "Sequence Editor", "");
	
	func = RNA_def_function(srna, "sequence_editor_create", "BKE_sequencer_editing_ensure");
	RNA_def_function_ui_description(func, "Ensure sequence editor is valid in this scene");
	parm = RNA_def_pointer(func, "sequence_editor", "SequenceEditor", "", "New sequence editor data or NULL");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "sequence_editor_clear", "BKE_sequencer_editing_free");
	RNA_def_function_ui_description(func, "Clear sequence editor in this scene");

	/* Keying Sets */
	prop = RNA_def_property(srna, "keying_sets", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "keyingsets", NULL);
	RNA_def_property_struct_type(prop, "KeyingSet");
	RNA_def_property_ui_text(prop, "Absolute Keying Sets", "Absolute Keying Sets for this Scene");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
	rna_def_scene_keying_sets(brna, prop);
	
	prop = RNA_def_property(srna, "keying_sets_all", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_Scene_all_keyingsets_begin", "rna_Scene_all_keyingsets_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
	RNA_def_property_struct_type(prop, "KeyingSet");
	RNA_def_property_ui_text(prop, "All Keying Sets",
	                         "All Keying Sets available for use (Builtins and Absolute Keying Sets for this Scene)");
	RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET, NULL);
	rna_def_scene_keying_sets_all(brna, prop);
	
	/* Rigid Body Simulation */
	prop = RNA_def_property(srna, "rigidbody_world", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "rigidbody_world");
	RNA_def_property_struct_type(prop, "RigidBodyWorld");
	RNA_def_property_ui_text(prop, "Rigid Body World", "");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	
	/* Tool Settings */
	prop = RNA_def_property(srna, "tool_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "toolsettings");
	RNA_def_property_struct_type(prop, "ToolSettings");
	RNA_def_property_ui_text(prop, "Tool Settings", "");

	/* Unit Settings */
	prop = RNA_def_property(srna, "unit_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "unit");
	RNA_def_property_struct_type(prop, "UnitSettings");
	RNA_def_property_ui_text(prop, "Unit Settings", "Unit editing settings");

	/* Physics Settings */
	prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_ACCELERATION);
	RNA_def_property_float_sdna(prop, NULL, "physics_settings.gravity");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_range(prop, -200.0f, 200.0f, 1, 2);
	RNA_def_property_ui_text(prop, "Gravity", "Constant acceleration in a given direction");
	RNA_def_property_update(prop, 0, "rna_Physics_update");

	prop = RNA_def_property(srna, "use_gravity", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "physics_settings.flag", PHYS_GLOBAL_GRAVITY);
	RNA_def_property_ui_text(prop, "Global Gravity", "Use global gravity for all dynamics");
	RNA_def_property_update(prop, 0, "rna_Physics_update");
	
	/* Render Data */
	prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "r");
	RNA_def_property_struct_type(prop, "RenderSettings");
	RNA_def_property_ui_text(prop, "Render Data", "");
	
	/* Safe Areas */
	prop = RNA_def_property(srna, "safe_areas", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "safe_areas");
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "DisplaySafeAreas");
	RNA_def_property_ui_text(prop, "Safe Areas", "");

	/* Markers */
	prop = RNA_def_property(srna, "timeline_markers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "markers", NULL);
	RNA_def_property_struct_type(prop, "TimelineMarker");
	RNA_def_property_ui_text(prop, "Timeline Markers", "Markers used in all timelines for the current scene");
	rna_def_timeline_markers(brna, prop);

	/* Audio Settings */
	prop = RNA_def_property(srna, "use_audio", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Scene_use_audio_get", "rna_Scene_use_audio_set");
	RNA_def_property_ui_text(prop, "Audio Muted", "Play back of audio from Sequence Editor will be muted");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "use_audio_sync", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "audio.flag", AUDIO_SYNC);
	RNA_def_property_ui_text(prop, "Audio Sync",
	                         "Play back and sync with audio clock, dropping frames if frame display is too slow");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "use_audio_scrub", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "audio.flag", AUDIO_SCRUB);
	RNA_def_property_ui_text(prop, "Audio Scrubbing", "Play audio from Sequence Editor while scrubbing");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "audio_doppler_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "audio.speed_of_sound");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.01f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Speed of Sound", "Speed of sound for Doppler effect calculation");
	RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

	prop = RNA_def_property(srna, "audio_doppler_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "audio.doppler_factor");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Doppler Factor", "Pitch factor for Doppler effect calculation");
	RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

	prop = RNA_def_property(srna, "audio_distance_model", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "audio.distance_model");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_items(prop, audio_distance_model_items);
	RNA_def_property_ui_text(prop, "Distance Model", "Distance model for distance attenuation calculation");
	RNA_def_property_update(prop, NC_SCENE, "rna_Scene_listener_update");

	prop = RNA_def_property(srna, "audio_volume", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "audio.volume");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Volume", "Audio volume");
	RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_SOUND);
	RNA_def_property_update(prop, NC_SCENE, NULL);
	RNA_def_property_float_funcs(prop, NULL, "rna_Scene_volume_set", NULL);

	/* Game Settings */
	prop = RNA_def_property(srna, "game_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "gm");
	RNA_def_property_struct_type(prop, "SceneGameData");
	RNA_def_property_ui_text(prop, "Game Data", "");

	/* Statistics */
	func = RNA_def_function(srna, "statistics", "ED_info_stats_string");
	prop = RNA_def_string(func, "statistics", NULL, 0, "Statistics", "");
	RNA_def_function_return(func, prop);
	
	/* Grease Pencil */
	prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_struct_type(prop, "GreasePencil");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
	RNA_def_property_ui_text(prop, "Grease Pencil Data", "Grease Pencil datablock");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
	
	/* Transform Orientations */
	prop = RNA_def_property(srna, "orientations", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "transform_spaces", NULL);
	RNA_def_property_struct_type(prop, "TransformOrientation");
	RNA_def_property_ui_text(prop, "Transform Orientations", "");

	/* active MovieClip */
	prop = RNA_def_property(srna, "active_clip", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clip");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "MovieClip");
	RNA_def_property_ui_text(prop, "Active Movie Clip", "Active movie clip used for constraints and viewport drawing");
	RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

	/* color management */
	prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "view_settings");
	RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
	RNA_def_property_ui_text(prop, "View Settings", "Color management settings applied on image before saving");

	prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "display_settings");
	RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
	RNA_def_property_ui_text(prop, "Display Settings", "Settings of device saved image would be displayed on");

	prop = RNA_def_property(srna, "sequencer_colorspace_settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "sequencer_colorspace_settings");
	RNA_def_property_struct_type(prop, "ColorManagedSequencerColorspaceSettings");
	RNA_def_property_ui_text(prop, "Sequencer Color Space Settings", "Settings of color space sequencer is working in");

	/* Nestled Data  */
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_tool_settings(brna);
	rna_def_unified_paint_settings(brna);
	rna_def_statvis(brna);
	rna_def_unit_settings(brna);
	rna_def_scene_image_format_data(brna);
	rna_def_scene_game_data(brna);
	rna_def_transform_orientation(brna);
	rna_def_selected_uv_element(brna);
	rna_def_display_safe_areas(brna);
	RNA_define_animate_sdna(true);
	/* *** Animated *** */
	rna_def_scene_render_data(brna);
	rna_def_scene_render_layer(brna);
	
	/* Scene API */
	RNA_api_scene(srna);
}

#endif
