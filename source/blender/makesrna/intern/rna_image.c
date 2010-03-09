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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_image.h"

#include "WM_types.h"

static EnumPropertyItem image_source_items[]= {
	{IMA_SRC_FILE, "FILE", 0, "File", "Single image file"},
	{IMA_SRC_SEQUENCE, "SEQUENCE", 0, "Sequence", "Multiple image files, as a sequence"},
	{IMA_SRC_MOVIE, "MOVIE", 0, "Movie", "Movie file"},
	{IMA_SRC_GENERATED, "GENERATED", 0, "Generated", "Generated image"},
	{IMA_SRC_VIEWER, "VIEWER", 0, "Viewer", "Compositing node viewer"},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "IMB_imbuf_types.h"

static void rna_Image_animated_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Image *ima= (Image*)ptr->data;
	int  nr;

	if(ima->flag & IMA_TWINANIM) {
		nr= ima->xrep*ima->yrep;
		if(ima->twsta>=nr) ima->twsta= 1;
		if(ima->twend>=nr) ima->twend= nr-1;
		if(ima->twsta>ima->twend) ima->twsta= 1;
	}
}

static int rna_Image_dirty_get(PointerRNA *ptr)
{
	Image *ima= (Image*)ptr->data;
	ImBuf *ibuf;

	for(ibuf=ima->ibufs.first; ibuf; ibuf=ibuf->next)
		if(ibuf->userflags & IB_BITMAPDIRTY)
			return 1;
	
	return 0;
}

static void rna_Image_source_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Image *ima= ptr->id.data;
	BKE_image_signal(ima, NULL, IMA_SIGNAL_SRC_CHANGE);
}

static void rna_Image_fields_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Image *ima= ptr->id.data;
	ImBuf *ibuf;
	void *lock;

	ibuf= BKE_image_acquire_ibuf(ima, NULL, &lock);

	if(ibuf) {
		short nr= 0;

		if(!(ima->flag & IMA_FIELDS) && (ibuf->flags & IB_fields)) nr= 1;
		if((ima->flag & IMA_FIELDS) && !(ibuf->flags & IB_fields)) nr= 1;

		if(nr)
			BKE_image_signal(ima, NULL, IMA_SIGNAL_FREE);
	}

	BKE_image_release_ibuf(ima, lock);
}

static void rna_Image_reload_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Image *ima= ptr->id.data;
	BKE_image_signal(ima, NULL, IMA_SIGNAL_RELOAD);
}

static void rna_Image_generated_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Image *ima= ptr->id.data;
	BKE_image_signal(ima, NULL, IMA_SIGNAL_FREE);
}

static void rna_ImageUser_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	ImageUser *iuser= ptr->data;

	BKE_image_user_calc_frame(iuser, scene->r.cfra, 0);
}

static EnumPropertyItem *rna_Image_source_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	Image *ima= (Image*)ptr->data;
	EnumPropertyItem *item= NULL;
	int totitem= 0;
	
	if(ima->source == IMA_SRC_VIEWER) {
		RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_VIEWER);
	}
	else {
		RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_FILE);
		RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_SEQUENCE);
		RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_MOVIE);
		RNA_enum_items_add_value(&item, &totitem, image_source_items, IMA_SRC_GENERATED);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

static int rna_Image_has_data_get(PointerRNA *ptr)
{
	Image *im= (Image*)ptr->data;

	if (im->ibufs.first)
		return 1;

	return 0;
}

static void rna_Image_size_get(PointerRNA *ptr,int *values)
{
	Image *im= (Image*)ptr->data;
	ImBuf *ibuf;
	void *lock;

	ibuf = BKE_image_acquire_ibuf(im, NULL , &lock);
	if (ibuf) {
		values[0]= ibuf->x;
		values[1]= ibuf->y;
	}
    else {
		values[0]= 0;
		values[1]= 0;
	}

	BKE_image_release_ibuf(im, lock);
}


static int rna_Image_depth_get(PointerRNA *ptr)
{
	Image *im= (Image*)ptr->data;
	ImBuf *ibuf;
	void *lock;
	int depth;
	
	ibuf= BKE_image_acquire_ibuf(im, NULL, &lock);

	if(!ibuf)
		depth= 0;
	else if(ibuf->rect_float)
		depth= 128;
	else
		depth= ibuf->depth;

	BKE_image_release_ibuf(im, lock);

	return depth;
}

#else

static void rna_def_imageuser(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ImageUser", NULL);
	RNA_def_struct_ui_text(srna, "Image User", "Parameters defining how an Image datablock is used by another datablock");

	prop= RNA_def_property(srna, "auto_refresh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMA_ANIM_ALWAYS);
	RNA_def_property_ui_text(prop, "Auto Refresh", "Always refresh image on frame changes");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	/* animation */
	prop= RNA_def_property(srna, "cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cycl", 0);
	RNA_def_property_ui_text(prop, "Cyclic", "Cycle the images in the movie");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	prop= RNA_def_property(srna, "frames", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 0, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Frames", "Sets the number of images of a movie to use");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	prop= RNA_def_property(srna, "offset", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, -MAXFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Offset", "Offsets the number of the frame to use in the animation");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	prop= RNA_def_property(srna, "start_frame", PROP_INT, PROP_TIME);
	RNA_def_property_int_sdna(prop, NULL, "sfra");
	RNA_def_property_range(prop, 1.0f, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Start Frame", "Sets the global starting frame of the movie");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	prop= RNA_def_property(srna, "fields_per_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "fie_ima");
	RNA_def_property_range(prop, -MAXFRAMEF, MAXFRAMEF);
	RNA_def_property_ui_text(prop, "Fields per Frame", "The number of fields per rendered frame (2 fields is 1 image)");
	RNA_def_property_update(prop, 0, "rna_ImageUser_update");

	prop= RNA_def_property(srna, "multilayer_layer", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "layer");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* image_multi_cb */
	RNA_def_property_ui_text(prop, "Layer", "Layer in multilayer image");

	prop= RNA_def_property(srna, "multilayer_pass", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "pass");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* image_multi_cb */
	RNA_def_property_ui_text(prop, "Pass", "Pass in multilayer image");
}

static void rna_def_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static const EnumPropertyItem prop_type_items[]= {
		{IMA_TYPE_IMAGE, "IMAGE", 0, "Image", ""},
		{IMA_TYPE_MULTILAYER, "MULTILAYER", 0, "Multilayer", ""},
		{IMA_TYPE_UV_TEST, "UV_TEST", 0, "UV Test", ""},
		{IMA_TYPE_R_RESULT, "RENDER_RESULT", 0, "Render Result", ""},
		{IMA_TYPE_COMPOSITE, "COMPOSITING", 0, "Compositing", ""},
		{0, NULL, 0, NULL, NULL}};
	static const EnumPropertyItem prop_generated_type_items[]= {
		{0, "BLANK", 0, "Blank", "Generate a blank image"},
		{1, "UVGRID", 0, "UV Grid", "Generated grid to test UV mappings"},
		{0, NULL, 0, NULL, NULL}};
	static const EnumPropertyItem prop_mapping_items[]= {
		{0, "UV", 0, "UV Coordinates", "Use UV coordinates for mapping the image"},
		{IMA_REFLECT, "REFLECTION", 0, "Reflection", "Use reflection mapping for mapping the image"},
		{0, NULL, 0, NULL, NULL}};
	static const EnumPropertyItem prop_field_order_items[]= {
		{0, "EVEN", 0, "Upper First", "Upper field first"},
		{IMA_STD_FIELD, "ODD", 0, "Lower First", "Lower field first"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Image", "ID");
	RNA_def_struct_ui_text(srna, "Image", "Image datablock referencing an external or packed image");
	RNA_def_struct_ui_icon(srna, ICON_IMAGE_DATA);

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILEPATH);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Filename", "Image/Movie file name");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_reload_update");

	prop= RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, image_source_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Image_source_itemf");
	RNA_def_property_ui_text(prop, "Source", "Where the image comes from");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_source_update");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Type", "How to generate the image");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "packed_file", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "packedfile");
	RNA_def_property_ui_text(prop, "Packed File", "");
	
	prop= RNA_def_property(srna, "field_order", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_field_order_items);
	RNA_def_property_ui_text(prop, "Field Order", "Order of video fields. Select which lines are displayed first");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);
	
	/* booleans */
	prop= RNA_def_property(srna, "fields", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMA_FIELDS);
	RNA_def_property_ui_text(prop, "Fields", "Use fields of the image");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_fields_update");

	prop= RNA_def_property(srna, "antialias", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMA_ANTIALI);
	RNA_def_property_ui_text(prop, "Anti-alias", "Toggles image anti-aliasing, only works with solid colors");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", IMA_DO_PREMUL);
	RNA_def_property_ui_text(prop, "Premultiply", "Convert RGB from key alpha to premultiplied alpha");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_reload_update");

	prop= RNA_def_property(srna, "dirty", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Image_dirty_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Dirty", "Image has changed and is not saved");

	/* generated image (image_generated_change_cb) */
	prop= RNA_def_property(srna, "generated_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "gen_type");
	RNA_def_property_enum_items(prop, prop_generated_type_items);
	RNA_def_property_ui_text(prop, "Generated Type", "Generated image type");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_generated_update");

	prop= RNA_def_property(srna, "generated_width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gen_x");
	RNA_def_property_range(prop, 1, 16384);
	RNA_def_property_ui_text(prop, "Generated Width", "Generated image width");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_generated_update");

	prop= RNA_def_property(srna, "generated_height", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gen_y");
	RNA_def_property_range(prop, 1, 16384);
	RNA_def_property_ui_text(prop, "Generated Height", "Generated image height");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_generated_update");

	/* realtime properties */
	prop= RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "Mapping type to use for this image in the game engine");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "display_aspect", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "aspx");
	RNA_def_property_array(prop, 2);
	RNA_def_property_range(prop, 0.1f, 5000.0f);
	RNA_def_property_ui_text(prop, "Display Aspect", "Display Aspect for this image, does not affect rendering");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "animated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "tpageflag", IMA_TWINANIM);
	RNA_def_property_ui_text(prop, "Animated", "Use as animated texture in the game engine");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_animated_update");

	prop= RNA_def_property(srna, "animation_start", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "twsta");
	RNA_def_property_range(prop, 0, 128);
	RNA_def_property_ui_text(prop, "Animation Start", "Start frame of an animated texture");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_animated_update");

	prop= RNA_def_property(srna, "animation_end", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "twend");
	RNA_def_property_range(prop, 0, 128);
	RNA_def_property_ui_text(prop, "Animation End", "End frame of an animated texture");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, "rna_Image_animated_update");

	prop= RNA_def_property(srna, "animation_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "animspeed");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Animation Speed", "Speed of the animation in frames per second");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "tiles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "tpageflag", IMA_TILES);
	RNA_def_property_ui_text(prop, "Tiles", "Use of tilemode for faces (default shift-LMB to pick the tile for selected faces)");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "tiles_x", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "xrep");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Tiles X", "Degree of repetition in the X direction");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "tiles_y", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "yrep");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Tiles Y", "Degree of repetition in the Y direction");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "clamp_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "tpageflag", IMA_CLAMP_U);
	RNA_def_property_ui_text(prop, "Clamp X", "Disable texture repeating horizontally");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	prop= RNA_def_property(srna, "clamp_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "tpageflag", IMA_CLAMP_V);
	RNA_def_property_ui_text(prop, "Clamp Y", "Disable texture repeating vertically");
	RNA_def_property_update(prop, NC_IMAGE|ND_DISPLAY, NULL);

	/*
	   Image.has_data and Image.depth are temporary,
	   Update import_obj.py when they are replaced (Arystan)
	*/
	prop= RNA_def_property(srna, "has_data", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Image_has_data_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Has data", "True if this image has data");

	prop= RNA_def_property(srna, "depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_funcs(prop, "rna_Image_depth_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Depth", "Image bit depth");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_int_vector(srna, "size" , 2 , NULL , 0, 0, "Size" , "Width and height in pixels, zero when image data cant be loaded" , 0 , 0);
	RNA_def_property_int_funcs(prop, "rna_Image_size_get" , NULL, NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	RNA_api_image(srna);
}

void RNA_def_image(BlenderRNA *brna)
{
	rna_def_image(brna);
	rna_def_imageuser(brna);
}

#endif

