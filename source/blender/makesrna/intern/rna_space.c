/*
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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_space.c
 *  \ingroup RNA
 */


#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLF_api.h"

#include "rna_internal.h"

#include "BKE_key.h"

#include "DNA_action_types.h"
#include "DNA_key_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "WM_api.h"
#include "WM_types.h"

EnumPropertyItem space_type_items[] = {
	{SPACE_EMPTY, "EMPTY", 0, N_("Empty"), ""},
	{SPACE_VIEW3D, "VIEW_3D", 0, N_("3D View"), ""},
	{SPACE_IPO, "GRAPH_EDITOR", 0, N_("Graph Editor"), ""},
	{SPACE_OUTLINER, "OUTLINER", 0, N_("Outliner"), ""},
	{SPACE_BUTS, "PROPERTIES", 0, N_("Properties"), ""},
	{SPACE_FILE, "FILE_BROWSER", 0, N_("File Browser"), ""},
	{SPACE_IMAGE, "IMAGE_EDITOR", 0, N_("Image Editor"), ""},
	{SPACE_INFO, "INFO", 0, N_("Info"), ""},
	{SPACE_SEQ, "SEQUENCE_EDITOR", 0, N_("Sequence Editor"), ""},
	{SPACE_TEXT, "TEXT_EDITOR", 0, N_("Text Editor"), ""},
	//{SPACE_IMASEL, "IMAGE_BROWSER", 0, "Image Browser", ""},
	{SPACE_SOUND, "AUDIO_WINDOW", 0, N_("Audio Window"), ""},
	{SPACE_ACTION, "DOPESHEET_EDITOR", 0, N_("DopeSheet Editor"), ""},
	{SPACE_NLA, "NLA_EDITOR", 0, N_("NLA Editor"), ""},
	{SPACE_SCRIPT, "SCRIPTS_WINDOW", 0, N_("Scripts Window"), ""},
	{SPACE_TIME, "TIMELINE", 0, N_("Timeline"), ""},
	{SPACE_NODE, "NODE_EDITOR", 0, N_("Node Editor"), ""},
	{SPACE_LOGIC, "LOGIC_EDITOR", 0, N_("Logic Editor"), ""},
	{SPACE_CONSOLE, "CONSOLE", 0, N_("Python Console"), ""},
	{SPACE_USERPREF, "USER_PREFERENCES", 0, N_("User Preferences"), ""},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem draw_channels_items[] = {
	{0, "COLOR", ICON_IMAGE_RGB, N_("Color"), N_("Draw image with RGB colors")},
	{SI_USE_ALPHA, "COLOR_ALPHA", ICON_IMAGE_RGB_ALPHA, N_("Color and Alpha"), N_("Draw image with RGB colors and alpha transparency")},
	{SI_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, N_("Alpha"), N_("Draw alpha transparency channel")},
	{SI_SHOW_ZBUF, "Z_BUFFER", ICON_IMAGE_ZDEPTH, N_("Z-Buffer"), N_("Draw Z-buffer associated with image (mapped from camera clip start to end)")},
	{0, NULL, 0, NULL, NULL}};

static EnumPropertyItem transform_orientation_items[] = {
	{V3D_MANIP_GLOBAL, "GLOBAL", 0, N_("Global"), N_("Align the transformation axes to world space")},
	{V3D_MANIP_LOCAL, "LOCAL", 0, N_("Local"), N_("Align the transformation axes to the selected objects' local space")},
	{V3D_MANIP_GIMBAL, "GIMBAL", 0, N_("Gimbal"), N_("Align each axis to the Euler rotation axis as used for input")},
	{V3D_MANIP_NORMAL, "NORMAL", 0, N_("Normal"), N_("Align the transformation axes to average normal of selected elements (bone Y axis for pose mode)")},
	{V3D_MANIP_VIEW, "VIEW", 0, N_("View"), N_("Align the transformation axes to the window")},
	{V3D_MANIP_CUSTOM, "CUSTOM", 0, N_("Custom"), N_("Use a custom transform orientation")},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem autosnap_items[] = {
	{SACTSNAP_OFF, "NONE", 0, N_("No Auto-Snap"), ""},
	{SACTSNAP_STEP, "STEP", 0, N_("Time Step"), N_("Snap to 1.0 frame/second intervals")},
	{SACTSNAP_FRAME, "FRAME", 0, N_("Nearest Frame"), N_("Snap to actual frames/seconds (nla-action time)")},
	{SACTSNAP_MARKER, "MARKER", 0, N_("Nearest Marker"), N_("Snap to nearest marker")},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem viewport_shade_items[] = {
	{OB_BOUNDBOX, "BOUNDBOX", ICON_BBOX, N_("Bounding Box"), N_("Display the object's local bounding boxes only")},
	{OB_WIRE, "WIREFRAME", ICON_WIRE, N_("Wireframe"), N_("Display the object as wire edges")},
	{OB_SOLID, "SOLID", ICON_SOLID, N_("Solid"), N_("Display the object solid, lit with default OpenGL lights")},
	//{OB_SHADED, "SHADED", ICON_SMOOTH, "Shaded", "Display the object solid, with preview shading interpolated at vertices"},
	{OB_TEXTURE, "TEXTURED", ICON_POTATO, N_("Textured"), N_("Display the object solid, with face-assigned textures")},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_math.h"

#include "BKE_screen.h"
#include "BKE_animsys.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_paint.h"

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_sequencer.h"

#include "IMB_imbuf_types.h"

static StructRNA* rna_Space_refine(struct PointerRNA *ptr)
{
	SpaceLink *space= (SpaceLink*)ptr->data;

	switch(space->spacetype) {
		case SPACE_VIEW3D:
			return &RNA_SpaceView3D;
		case SPACE_IPO:
			return &RNA_SpaceGraphEditor;
		case SPACE_OUTLINER:
			return &RNA_SpaceOutliner;
		case SPACE_BUTS:
			return &RNA_SpaceProperties;
		case SPACE_FILE:
			return &RNA_SpaceFileBrowser;
		case SPACE_IMAGE:
			return &RNA_SpaceImageEditor;
		case SPACE_INFO:
			return &RNA_SpaceInfo;
		case SPACE_SEQ:
			return &RNA_SpaceSequenceEditor;
		case SPACE_TEXT:
			return &RNA_SpaceTextEditor;
		//case SPACE_IMASEL:
		//	return &RNA_SpaceImageBrowser;
		/*case SPACE_SOUND:
			return &RNA_SpaceAudioWindow;*/
		case SPACE_ACTION:
			return &RNA_SpaceDopeSheetEditor;
		case SPACE_NLA:
			return &RNA_SpaceNLA;
		/*case SPACE_SCRIPT:
			return &RNA_SpaceScriptsWindow;*/
		case SPACE_TIME:
			return &RNA_SpaceTimeline;
		case SPACE_NODE:
			return &RNA_SpaceNodeEditor;
		case SPACE_LOGIC:
			return &RNA_SpaceLogicEditor;
		case SPACE_CONSOLE:
			return &RNA_SpaceConsole;
		case SPACE_USERPREF:
			return &RNA_SpaceUserPreferences;
		default:
			return &RNA_Space;
	}
}

static ScrArea *rna_area_from_space(PointerRNA *ptr)
{
	bScreen *sc = (bScreen*)ptr->id.data;
	SpaceLink *link= (SpaceLink*)ptr->data;
	ScrArea *sa;

	for(sa=sc->areabase.first; sa; sa=sa->next)
		if(BLI_findindex(&sa->spacedata, link) != -1)
			return sa;

	return NULL;
}

static void rna_area_region_from_regiondata(PointerRNA *ptr, ScrArea **sa_r, ARegion **ar_r)
{
	bScreen *sc = (bScreen*)ptr->id.data;
	ScrArea *sa;
	ARegion *ar;
	void *regiondata= ptr->data;

	*sa_r= NULL;
	*ar_r= NULL;

	for(sa=sc->areabase.first; sa; sa=sa->next) {
		for(ar=sa->regionbase.first; ar; ar=ar->next) {
			if(ar->regiondata == regiondata) {
				*sa_r= sa;
				*ar_r= ar;
				return;
			}
		}
	}
}

static PointerRNA rna_CurrentOrientation_get(PointerRNA *ptr)
{
	Scene *scene = ((bScreen*)ptr->id.data)->scene;
	View3D *v3d= (View3D*)ptr->data;
	
	if (v3d->twmode < V3D_MANIP_CUSTOM)
		return rna_pointer_inherit_refine(ptr, &RNA_TransformOrientation, NULL);
	else
		return rna_pointer_inherit_refine(ptr, &RNA_TransformOrientation, BLI_findlink(&scene->transform_spaces, v3d->twmode - V3D_MANIP_CUSTOM));
}

EnumPropertyItem *rna_TransformOrientation_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *free)
{
	Scene *scene = NULL;
	ListBase *transform_spaces;
	TransformOrientation *ts= NULL;
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item= NULL;
	int i = V3D_MANIP_CUSTOM, totitem= 0;

	RNA_enum_items_add_value(&item, &totitem, transform_orientation_items, V3D_MANIP_GLOBAL);
	RNA_enum_items_add_value(&item, &totitem, transform_orientation_items, V3D_MANIP_NORMAL);
	RNA_enum_items_add_value(&item, &totitem, transform_orientation_items, V3D_MANIP_GIMBAL);
	RNA_enum_items_add_value(&item, &totitem, transform_orientation_items, V3D_MANIP_LOCAL);
	RNA_enum_items_add_value(&item, &totitem, transform_orientation_items, V3D_MANIP_VIEW);

	if (ptr->type == &RNA_SpaceView3D)
		scene = ((bScreen*)ptr->id.data)->scene;
	else
		scene = CTX_data_scene(C); /* can't use scene from ptr->id.data because that enum is also used by operators */

	if(scene) {
		transform_spaces = &scene->transform_spaces;
		ts = transform_spaces->first;
	}

	if(ts)
	{
		RNA_enum_item_add_separator(&item, &totitem);

		for(; ts; ts = ts->next) {
			tmp.identifier = ts->name;
			tmp.name= ts->name;
			tmp.value = i++;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

/* Space 3D View */
static void rna_SpaceView3D_lock_camera_and_layers_set(PointerRNA *ptr, int value)
{
	View3D *v3d= (View3D*)(ptr->data);
	bScreen *sc= (bScreen*)ptr->id.data;

	v3d->scenelock = value;

	if(value) {
		int bit;
		v3d->lay= sc->scene->lay;
		/* seek for layact */
		bit= 0;
		while(bit<32) {
			if(v3d->lay & (1<<bit)) {
				v3d->layact= 1<<bit;
				break;
			}
			bit++;
		}
		v3d->camera= sc->scene->camera;
	}
}

static void rna_View3D_CursorLocation_get(PointerRNA *ptr, float *values)
{
	View3D *v3d= (View3D*)(ptr->data);
	bScreen *sc= (bScreen*)ptr->id.data;
	Scene *scene= (Scene *)sc->scene;
	float *loc = give_cursor(scene, v3d);
	
	copy_v3_v3(values, loc);
}

static void rna_View3D_CursorLocation_set(PointerRNA *ptr, const float *values)
{
	View3D *v3d= (View3D*)(ptr->data);
	bScreen *sc= (bScreen*)ptr->id.data;
	Scene *scene= (Scene *)sc->scene;
	float *cursor = give_cursor(scene, v3d);
	
	copy_v3_v3(cursor, values);
}

static void rna_SpaceView3D_layer_set(PointerRNA *ptr, const int *values)
{
	View3D *v3d= (View3D*)(ptr->data);
	
	v3d->lay= ED_view3d_scene_layer_set(v3d->lay, values, &v3d->layact);
}

static void rna_SpaceView3D_layer_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	DAG_on_visible_update(bmain, FALSE);
}

static void rna_SpaceView3D_pivot_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	if (U.uiflag & USER_LOCKAROUND) {
		View3D *v3d_act= (View3D*)(ptr->data);

		/* TODO, space looper */
		bScreen *screen;
		for(screen= bmain->screen.first; screen; screen= screen->id.next) {
			ScrArea *sa;
			for(sa= screen->areabase.first; sa; sa= sa->next) {
				SpaceLink *sl;
				for(sl= sa->spacedata.first; sl ;sl= sl->next) {
					if(sl->spacetype==SPACE_VIEW3D) {
						View3D *v3d= (View3D *)sl;
						if (v3d != v3d_act) {
							v3d->around= v3d_act->around;
							v3d->flag= (v3d->flag & ~V3D_ALIGN) | (v3d_act->flag & V3D_ALIGN);
							ED_area_tag_redraw_regiontype(sa, RGN_TYPE_HEADER);
						}
					}
				}
			}
		}
	}
}

static PointerRNA rna_SpaceView3D_region_3d_get(PointerRNA *ptr)
{
	View3D *v3d= (View3D*)(ptr->data);
	ScrArea *sa= rna_area_from_space(ptr);
	void *regiondata= NULL;
	if(sa) {
		ListBase *regionbase= (sa->spacedata.first == v3d)? &sa->regionbase: &v3d->regionbase;
		ARegion *ar= regionbase->last; /* always last in list, weak .. */
		regiondata= ar->regiondata;
	}

	return rna_pointer_inherit_refine(ptr, &RNA_RegionView3D, regiondata);
}

static PointerRNA rna_SpaceView3D_region_quadview_get(PointerRNA *ptr)
{
	View3D *v3d= (View3D*)(ptr->data);
	ScrArea *sa= rna_area_from_space(ptr);
	void *regiondata= NULL;
	if(sa) {
		ListBase *regionbase= (sa->spacedata.first == v3d)? &sa->regionbase: &v3d->regionbase;
		ARegion *ar= regionbase->last; /* always before last in list, weak .. */

		ar= (ar->alignment == RGN_ALIGN_QSPLIT)? ar->prev: NULL;
		if(ar) {
			regiondata= ar->regiondata;
		}
	}

	return rna_pointer_inherit_refine(ptr, &RNA_RegionView3D, regiondata);
}

static void rna_RegionView3D_quadview_update(Main *UNUSED(main), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ScrArea *sa;
	ARegion *ar;

	rna_area_region_from_regiondata(ptr, &sa, &ar);
	if(sa && ar && ar->alignment==RGN_ALIGN_QSPLIT)
		ED_view3d_quadview_update(sa, ar, FALSE);
}

/* same as above but call clip==TRUE */
static void rna_RegionView3D_quadview_clip_update(Main *UNUSED(main), Scene *UNUSED(scene), PointerRNA *ptr)
{
	ScrArea *sa;
	ARegion *ar;

	rna_area_region_from_regiondata(ptr, &sa, &ar);
	if(sa && ar && ar->alignment==RGN_ALIGN_QSPLIT)
		ED_view3d_quadview_update(sa, ar, TRUE);
}

static void rna_RegionView3D_view_location_get(PointerRNA *ptr, float *values)
{
	RegionView3D *rv3d= (RegionView3D *)(ptr->data);
	negate_v3_v3(values, rv3d->ofs);
}

static void rna_RegionView3D_view_location_set(PointerRNA *ptr, const float *values)
{
	RegionView3D *rv3d= (RegionView3D *)(ptr->data);
	negate_v3_v3(rv3d->ofs, values);
}

static void rna_RegionView3D_view_rotation_get(PointerRNA *ptr, float *values)
{
	RegionView3D *rv3d= (RegionView3D *)(ptr->data);
	invert_qt_qt(values, rv3d->viewquat);
}

static void rna_RegionView3D_view_rotation_set(PointerRNA *ptr, const float *values)
{
	RegionView3D *rv3d= (RegionView3D *)(ptr->data);
	invert_qt_qt(rv3d->viewquat, values);
}

static void rna_RegionView3D_view_matrix_set(PointerRNA *ptr, const float *values)
{
	RegionView3D *rv3d= (RegionView3D *)(ptr->data);
	negate_v3_v3(rv3d->ofs, values);
	ED_view3d_from_m4((float (*)[4])values, rv3d->ofs, rv3d->viewquat, &rv3d->dist);
}

/* Space Image Editor */

static PointerRNA rna_SpaceImageEditor_uvedit_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_SpaceUVEditor, ptr->data);
}

static void rna_SpaceImageEditor_paint_update(Main *bmain, Scene *scene, PointerRNA *UNUSED(ptr))
{
	paint_init(&scene->toolsettings->imapaint.paint, PAINT_CURSOR_TEXTURE_PAINT);

	ED_space_image_paint_update(bmain->wm.first, scene->toolsettings);
}

static int rna_SpaceImageEditor_show_render_get(PointerRNA *ptr)
{
	SpaceImage *sima= (SpaceImage*)(ptr->data);
	return ED_space_image_show_render(sima);
}

static int rna_SpaceImageEditor_show_paint_get(PointerRNA *ptr)
{
	SpaceImage *sima= (SpaceImage*)(ptr->data);
	return ED_space_image_show_paint(sima);
}

static int rna_SpaceImageEditor_show_uvedit_get(PointerRNA *ptr)
{
	SpaceImage *sima= (SpaceImage*)(ptr->data);
	bScreen *sc= (bScreen*)ptr->id.data;
	return ED_space_image_show_uvedit(sima, sc->scene->obedit);
}

static void rna_SpaceImageEditor_image_set(PointerRNA *ptr, PointerRNA value)
{
	SpaceImage *sima= (SpaceImage*)(ptr->data);
	bScreen *sc= (bScreen*)ptr->id.data;

	ED_space_image_set(NULL, sima, sc->scene, sc->scene->obedit, (Image*)value.data);
}

static EnumPropertyItem *rna_SpaceImageEditor_draw_channels_itemf(bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), int *free)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	EnumPropertyItem *item= NULL;
	ImBuf *ibuf;
	void *lock;
	int zbuf, alpha, totitem= 0;

	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	
	alpha= ibuf && (ibuf->channels == 4);
	zbuf= ibuf && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels==1));

	ED_space_image_release_buffer(sima, lock);

	if(alpha && zbuf)
		return draw_channels_items;

	RNA_enum_items_add_value(&item, &totitem, draw_channels_items, 0);

	if(alpha) {
		RNA_enum_items_add_value(&item, &totitem, draw_channels_items, SI_USE_ALPHA);
		RNA_enum_items_add_value(&item, &totitem, draw_channels_items, SI_SHOW_ALPHA);
	}
	else if(zbuf) {
		RNA_enum_items_add_value(&item, &totitem, draw_channels_items, SI_SHOW_ZBUF);
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

static void rna_SpaceImageEditor_zoom_get(PointerRNA *ptr, float *values)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	ScrArea *sa;
	ARegion *ar;

	values[0] = values[1] = 1;

	/* find aregion */
	sa= rna_area_from_space(ptr); /* can be NULL */
	ar= BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	if(ar) {
		ED_space_image_zoom(sima, ar, &values[0], &values[1]);
	}
}

static void rna_SpaceImageEditor_cursor_location_get(PointerRNA *ptr, float *values)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	
	if (sima->flag & SI_COORDFLOATS) {
		copy_v2_v2(values, sima->cursor);
	} else {
		int w, h;
		ED_space_image_size(sima, &w, &h);
		
		values[0] = sima->cursor[0] * w;
		values[1] = sima->cursor[1] * h;
	}
}

static void rna_SpaceImageEditor_cursor_location_set(PointerRNA *ptr, const float *values)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	
	if (sima->flag & SI_COORDFLOATS) {
		copy_v2_v2(sima->cursor, values);
	} else {
		int w, h;
		ED_space_image_size(sima, &w, &h);
		
		sima->cursor[0] = values[0] / w;
		sima->cursor[1] = values[1] / h;
	}
}

static void rna_SpaceImageEditor_curves_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	ImBuf *ibuf;
	void *lock;

	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	if(ibuf->rect_float)
		curvemapping_do_ibuf(sima->cumap, ibuf);
	ED_space_image_release_buffer(sima, lock);

	WM_main_add_notifier(NC_IMAGE, sima->image);
}

static void rna_SpaceImageEditor_scopes_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	SpaceImage *sima= (SpaceImage*)ptr->data;
	ImBuf *ibuf;
	void *lock;
	
	ibuf= ED_space_image_acquire_buffer(sima, &lock);
	if(ibuf) {
		scopes_update(&sima->scopes, ibuf, scene->r.color_mgt_flag & R_COLOR_MANAGEMENT);
		WM_main_add_notifier(NC_IMAGE, sima->image);
	}
	ED_space_image_release_buffer(sima, lock);
}

/* Space Text Editor */

static void rna_SpaceTextEditor_word_wrap_set(PointerRNA *ptr, int value)
{
	SpaceText *st= (SpaceText*)(ptr->data);

	st->wordwrap= value;
	st->left= 0;
}

static void rna_SpaceTextEditor_text_set(PointerRNA *ptr, PointerRNA value)
{
	SpaceText *st= (SpaceText*)(ptr->data);

	st->text= value.data;
	st->top= 0;
}

static void rna_SpaceTextEditor_updateEdited(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	SpaceText *st= (SpaceText*)ptr->data;

	if(st->text)
		WM_main_add_notifier(NC_TEXT|NA_EDITED, st->text);
}


/* Space Properties */

/* note: this function exists only to avoid id refcounting */
static void rna_SpaceProperties_pin_id_set(PointerRNA *ptr, PointerRNA value)
{
	SpaceButs *sbuts= (SpaceButs*)(ptr->data);
	sbuts->pinid= value.data;
}

static StructRNA *rna_SpaceProperties_pin_id_typef(PointerRNA *ptr)
{
	SpaceButs *sbuts= (SpaceButs*)(ptr->data);

	if(sbuts->pinid)
		return ID_code_to_RNA_type(GS(sbuts->pinid->name));

	return &RNA_ID;
}

static void rna_SpaceProperties_pin_id_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	SpaceButs *sbuts= (SpaceButs*)(ptr->data);
	ID *id = sbuts->pinid;
	
	if (id == NULL) {
		sbuts->flag &= ~SB_PIN_CONTEXT;
		return;
	}
	
	switch (GS(id->name)) {
		case ID_MA:
			WM_main_add_notifier(NC_MATERIAL|ND_SHADING, NULL);
			break;
		case ID_TE:
			WM_main_add_notifier(NC_TEXTURE, NULL);
			break;
		case ID_WO:
			WM_main_add_notifier(NC_WORLD, NULL);
			break;
		case ID_LA:
			WM_main_add_notifier(NC_LAMP, NULL);
			break;
	}
}


static void rna_SpaceProperties_context_set(PointerRNA *ptr, int value)
{
	SpaceButs *sbuts= (SpaceButs*)(ptr->data);
	
	sbuts->mainb= value;
	sbuts->mainbuser = value;
}

static void rna_SpaceProperties_align_set(PointerRNA *ptr, int value)
{
	SpaceButs *sbuts= (SpaceButs*)(ptr->data);

	sbuts->align= value;
	sbuts->re_align= 1;
}

/* Space Console */
static void rna_ConsoleLine_body_get(PointerRNA *ptr, char *value)
{
	ConsoleLine *ci= (ConsoleLine*)ptr->data;
	strcpy(value, ci->line);
}

static int rna_ConsoleLine_body_length(PointerRNA *ptr)
{
	ConsoleLine *ci= (ConsoleLine*)ptr->data;
	return ci->len;
}

static void rna_ConsoleLine_body_set(PointerRNA *ptr, const char *value)
{
	ConsoleLine *ci= (ConsoleLine*)ptr->data;
	int len= strlen(value);
	
	if((len >= ci->len_alloc) || (len * 2 < ci->len_alloc) ) { /* allocate a new string */
		MEM_freeN(ci->line);
		ci->line= MEM_mallocN((len + 1) * sizeof(char), "rna_consoleline");
		ci->len_alloc= len + 1;
	}
	memcpy(ci->line, value, len + 1);
	ci->len= len;

	if(ci->cursor > len) /* clamp the cursor */
		ci->cursor= len;
}

static void rna_ConsoleLine_cursor_index_range(PointerRNA *ptr, int *min, int *max)
{
	ConsoleLine *ci= (ConsoleLine*)ptr->data;

	*min= 0;
	*max= ci->len; /* intentionally _not_ -1 */
}

/* Space Dopesheet */

static void rna_SpaceDopeSheetEditor_action_set(PointerRNA *ptr, PointerRNA value)
{
	SpaceAction *saction= (SpaceAction*)(ptr->data);
	bAction *act = (bAction*)value.data;
	
	if ((act == NULL) || (act->idroot == 0)) {
		/* just set if we're clearing the action or if the action is "amorphous" still */
		saction->action= act;
	}
	else {
		/* action to set must strictly meet the mode criteria... */
		if (saction->mode == SACTCONT_ACTION) {
			/* currently, this is "object-level" only, until we have some way of specifying this */
			if (act->idroot == ID_OB)
				saction->action = act;
			else
				printf("ERROR: cannot assign Action '%s' to Action Editor, as action is not object-level animation\n", act->id.name+2);
		}
		else if (saction->mode == SACTCONT_SHAPEKEY) {
			/* as the name says, "shapekey-level" only... */
			if (act->idroot == ID_KE)
				saction->action = act;
			else
				printf("ERROR: cannot assign Action '%s' to Shape Key Editor, as action doesn't animate Shape Keys\n", act->id.name+2);
		}
		else {
			printf("ACK: who's trying to set an action while not in a mode displaying a single Action only?\n");
		}
	}
}

static void rna_SpaceDopeSheetEditor_action_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	SpaceAction *saction= (SpaceAction*)(ptr->data);
	Object *obact= (scene->basact)? scene->basact->object: NULL;

	/* we must set this action to be the one used by active object (if not pinned) */
	if (obact/* && saction->pin == 0*/) {
		AnimData *adt = NULL;
		
		if (saction->mode == SACTCONT_ACTION) {
			// TODO: context selector could help decide this with more control?
			adt= BKE_id_add_animdata(&obact->id); /* this only adds if non-existant */
		}
		else if (saction->mode == SACTCONT_SHAPEKEY) {
			Key *key = ob_get_key(obact);
			if (key)
				adt= BKE_id_add_animdata(&key->id); /* this only adds if non-existant */
		}
		
		/* set action */
		if (adt) {
			/* fix id-count of action we're replacing */
			id_us_min(&adt->action->id);
			
			/* show new id-count of action we're replacing */
			adt->action= saction->action;
			id_us_plus(&adt->action->id);
		}
		
		/* force depsgraph flush too */
		DAG_id_tag_update(&obact->id, OB_RECALC_OB|OB_RECALC_DATA);
	}
}

static void rna_SpaceDopeSheetEditor_mode_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
	SpaceAction *saction= (SpaceAction*)(ptr->data);
	Object *obact= (scene->basact)? scene->basact->object: NULL;
	
	/* special exceptions for ShapeKey Editor mode */
	if (saction->mode == SACTCONT_SHAPEKEY) {
		Key *key = ob_get_key(obact);
		
		/* 1)	update the action stored for the editor */
		if (key)
			saction->action = (key->adt)? key->adt->action : NULL;
		else
			saction->action = NULL;
		
		/* 2)	enable 'show sliders' by default, since one of the main
		 *		points of the ShapeKey Editor is to provide a one-stop shop
		 *		for controlling the shapekeys, whose main control is the value
		 */
		saction->flag |= SACTION_SLIDERS;
	}
	/* make sure action stored is valid */
	else if (saction->mode == SACTCONT_ACTION) {
		/* 1)	update the action stored for the editor */
		// TODO: context selector could help decide this with more control?
		if (obact)
			saction->action = (obact->adt)? obact->adt->action : NULL;
		else
			saction->action = NULL;
	}
}

/* Space Graph Editor */

static void rna_SpaceGraphEditor_display_mode_update(bContext *C, PointerRNA *UNUSED(ptr))
{
	//SpaceIpo *sipo= (SpaceIpo*)(ptr->data);
	ScrArea *sa= CTX_wm_area(C);
	
	/* after changing view mode, must force recalculation of F-Curve colors 
	 * which can only be achieved using refresh as opposed to redraw
	 */
	ED_area_tag_refresh(sa);
}

static int rna_SpaceGraphEditor_has_ghost_curves_get(PointerRNA *ptr)
{
	SpaceIpo *sipo= (SpaceIpo*)(ptr->data);
	return (sipo->ghostCurves.first != NULL);
}

static void rna_Sequencer_display_mode_update(bContext *C, PointerRNA *ptr)
{
	int view = RNA_enum_get(ptr, "view_type");

	ED_sequencer_update_view(C, view);
}

static float rna_BackgroundImage_opacity_get(PointerRNA *ptr)
{
	BGpic *bgpic= (BGpic *)ptr->data;
	return 1.0f-bgpic->blend;
}

static void rna_BackgroundImage_opacity_set(PointerRNA *ptr, float value)
{
	BGpic *bgpic= (BGpic *)ptr->data;
	bgpic->blend = 1.0f - value;
}

static EnumPropertyItem *rna_SpaceProperties_texture_context_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), int *free)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	EnumPropertyItem *item= NULL;
	EnumPropertyItem tmp= {0, "", 0, "", ""};
	int totitem= 0;

	if(ob) {
		if(ob->type == OB_LAMP) {
			tmp.value = SB_TEXC_MAT_OR_LAMP;
			tmp.description = "Show Lamp Textures";
			tmp.identifier = "LAMP";
			tmp.icon = ICON_LAMP_POINT;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
		else if(ob->totcol) {
			tmp.value = SB_TEXC_MAT_OR_LAMP;
			tmp.description = "Show Material Textures";
			tmp.identifier = "MATERIAL";
			tmp.icon = ICON_MATERIAL;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}

		if(ob->particlesystem.first) {
			tmp.value = SB_TEXC_PARTICLES;
			tmp.description = "Show Particle Textures";
			tmp.identifier = "PARTICLE";
			tmp.icon = ICON_PARTICLES;
			RNA_enum_item_add(&item, &totitem, &tmp);
		}
	}

	if(scene && scene->world) {
		tmp.value = SB_TEXC_WORLD;
		tmp.description = "Show World Textures";
		tmp.identifier = "WORLD";
		tmp.icon = ICON_WORLD;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	tmp.value = SB_TEXC_BRUSH;
	tmp.description = "Show Brush Textures";
	tmp.identifier = "BRUSH";
	tmp.icon = ICON_BRUSH_DATA;
	RNA_enum_item_add(&item, &totitem, &tmp);
	
	RNA_enum_item_end(&item, &totitem);
	*free = 1;

	return item;
}

#else

static void rna_def_space(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "Space", NULL);
	RNA_def_struct_sdna(srna, "SpaceLink");
	RNA_def_struct_ui_text(srna, N_("Space"), N_("Space data for a screen area"));
	RNA_def_struct_refine_func(srna, "rna_Space_refine");
	
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spacetype");
	RNA_def_property_enum_items(prop, space_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Type"), N_("Space data type"));
}

static void rna_def_space_image_uv(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem sticky_mode_items[] = {
		{SI_STICKY_DISABLE, "DISABLED", ICON_STICKY_UVS_DISABLE, N_("Disabled"), N_("Sticky vertex selection disabled")},
		{SI_STICKY_LOC, "SHARED_LOCATION", ICON_STICKY_UVS_LOC, N_("Shared Location"), N_("Select UVs that are at the same location and share a mesh vertex")},
		{SI_STICKY_VERTEX, "SHARED_VERTEX", ICON_STICKY_UVS_VERT, N_("Shared Vertex"), N_("Select UVs that share mesh vertex, irrespective if they are in the same location")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem dt_uv_items[] = {
		{SI_UVDT_OUTLINE, "OUTLINE", 0, N_("Outline"), N_("Draw white edges with black outline")},
		{SI_UVDT_DASH, "DASH", 0, N_("Dash"), N_("Draw dashed black-white edges")},
		{SI_UVDT_BLACK, "BLACK", 0, N_("Black"), N_("Draw black edges")},
		{SI_UVDT_WHITE, "WHITE", 0, N_("White"), N_("Draw white edges")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem dt_uvstretch_items[] = {
		{SI_UVDT_STRETCH_ANGLE, "ANGLE", 0, N_("Angle"), N_("Angular distortion between UV and 3D angles")},
		{SI_UVDT_STRETCH_AREA, "AREA", 0, N_("Area"), N_("Area distortion between UV and 3D faces")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem pivot_items[] = {
		{V3D_CENTER, "CENTER", ICON_ROTATE, N_("Bounding Box Center"), ""},
		{V3D_CENTROID, "MEDIAN", ICON_ROTATECENTER, N_("Median Point"), ""},
		{V3D_CURSOR, "CURSOR", ICON_CURSOR, N_("2D Cursor"), ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "SpaceUVEditor", NULL);
	RNA_def_struct_sdna(srna, "SpaceImage");
	RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
	RNA_def_struct_ui_text(srna, N_("Space UV Editor"), N_("UV editor data for the image editor space"));

	/* selection */
	prop= RNA_def_property(srna, "sticky_select_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sticky");
	RNA_def_property_enum_items(prop, sticky_mode_items);
	RNA_def_property_ui_text(prop, N_("Sticky Selection Mode"), N_("Automatically select also UVs sharing the same vertex as the ones being selected"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	/* drawing */
	prop= RNA_def_property(srna, "edge_draw_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt_uv");
	RNA_def_property_enum_items(prop, dt_uv_items);
	RNA_def_property_ui_text(prop, N_("Edge Draw Type"), N_("Draw type for drawing UV edges"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_smooth_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_SMOOTH_UV);
	RNA_def_property_ui_text(prop, N_("Draw Smooth Edges"), N_("Draw UV edges anti-aliased"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_STRETCH);
	RNA_def_property_ui_text(prop, N_("Draw Stretch"), N_("Draw faces colored according to the difference in shape between UVs and their 3D coordinates (blue for low distortion, red for high distortion)"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "draw_stretch_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "dt_uvstretch");
	RNA_def_property_enum_items(prop, dt_uvstretch_items);
	RNA_def_property_ui_text(prop, N_("Draw Stretch Type"), N_("Type of stretch to draw"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_modified_edges", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAWSHADOW);
	RNA_def_property_ui_text(prop, N_("Draw Modified Edges"), N_("Draw edges after modifiers are applied"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_other_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_OTHER);
	RNA_def_property_ui_text(prop, N_("Draw Other Objects"), N_("Draw other selected objects that share the same image"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_normalized_coords", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_COORDFLOATS);
	RNA_def_property_ui_text(prop, N_("Normalized Coordinates"), N_("Display UV coordinates from 0.0 to 1.0 rather than in pixels"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SI_NO_DRAWFACES);
	RNA_def_property_ui_text(prop, N_("Draw Faces"), N_("Draw faces over the image"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 2);
	RNA_def_property_float_funcs(prop, "rna_SpaceImageEditor_cursor_location_get", "rna_SpaceImageEditor_cursor_location_set", NULL);
	RNA_def_property_ui_text(prop, N_("2D Cursor Location"), N_("2D cursor location for this view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	/* todo: move edge and face drawing options here from G.f */

	prop= RNA_def_property(srna, "use_snap_to_pixels", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_PIXELSNAP);
	RNA_def_property_ui_text(prop, N_("Snap to Pixels"), N_("Snap UVs to pixel locations while editing"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "lock_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_CLIP_UV);
	RNA_def_property_ui_text(prop, N_("Constrain to Image Bounds"), N_("Constraint to stay within the image bounds while editing"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "use_live_unwrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_LIVE_UNWRAP);
	RNA_def_property_ui_text(prop, N_("Live Unwrap"), N_("Continuously unwrap the selected UV island while transforming pinned vertices"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "around");
	RNA_def_property_enum_items(prop, pivot_items);
	RNA_def_property_ui_text(prop, N_("Pivot"), N_("Rotation/Scaling Pivot"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);
}

static void rna_def_space_outliner(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem display_mode_items[] = {
		{SO_ALL_SCENES, "ALL_SCENES", 0, N_("All Scenes"), N_("Display datablocks in all scenes")},
		{SO_CUR_SCENE, "CURRENT_SCENE", 0, N_("Current Scene"), N_("Display datablocks in current scene")},
		{SO_VISIBLE, "VISIBLE_LAYERS", 0, N_("Visible Layers"), N_("Display datablocks in visible layers")},
		{SO_SELECTED, "SELECTED", 0, N_("Selected"), N_("Display datablocks of selected objects")},
		{SO_ACTIVE, "ACTIVE", 0, N_("Active"), N_("Display datablocks of active object")},
		{SO_SAME_TYPE, "SAME_TYPES", 0, N_("Same Types"), N_("Display datablocks of all objects of same type as selected object")},
		{SO_GROUPS, "GROUPS", 0, N_("Groups"), N_("Display groups and their datablocks")},
		{SO_LIBRARIES, "LIBRARIES", 0, N_("Libraries"), N_("Display libraries")},
		{SO_SEQUENCE, "SEQUENCE", 0, N_("Sequence"), N_("Display sequence datablocks")},
		{SO_DATABLOCKS, "DATABLOCKS", 0, N_("Datablocks"), N_("Display raw datablocks")},
		{SO_USERDEF, "USER_PREFERENCES", 0, N_("User Preferences"), N_("Display the user preference datablocks")},
		{SO_KEYMAP, "KEYMAPS", 0, N_("Key Maps"), N_("Display keymap datablocks")},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "SpaceOutliner", "Space");
	RNA_def_struct_sdna(srna, "SpaceOops");
	RNA_def_struct_ui_text(srna, "Space Outliner", "Outliner space data");
	
	prop= RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "outlinevis");
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_ui_text(prop, N_("Display Mode"), N_("Type of information to display"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_OUTLINER, NULL);
	
	prop= RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "search_string");
	RNA_def_property_ui_text(prop, N_("Display Filter"), N_("Live search filtering string"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_OUTLINER, NULL);
	
	prop= RNA_def_property(srna, "use_filter_case_sensitive", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "search_flags", SO_FIND_CASE_SENSITIVE);
	RNA_def_property_ui_text(prop, N_("Case Sensitive Matches Only"), N_("Only use case sensitive matches of search string"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_OUTLINER, NULL);
	
	prop= RNA_def_property(srna, "use_filter_complete", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "search_flags", SO_FIND_COMPLETE);
	RNA_def_property_ui_text(prop, N_("Complete Matches Only"), N_("Only use complete matches of search string"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_OUTLINER, NULL);
	
	prop= RNA_def_property(srna, "show_restrict_columns", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SO_HIDE_RESTRICTCOLS);
	RNA_def_property_ui_text(prop, N_("Show Restriction Columns"), N_("Show column"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_OUTLINER, NULL);
}

static void rna_def_background_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* note: combinations work but dont flip so arnt that useful */
	static EnumPropertyItem bgpic_axis_items[] = {
		{0, "", 0, N_("X Axis"), ""},
		{(1<<RV3D_VIEW_LEFT), "LEFT", 0, N_("Left"), N_("Show background image while looking to the left")},
		{(1<<RV3D_VIEW_RIGHT), "RIGHT", 0, N_("Right"), N_("Show background image while looking to the right")},
		/*{(1<<RV3D_VIEW_LEFT)|(1<<RV3D_VIEW_RIGHT), "LEFT_RIGHT", 0, "Left/Right", ""},*/
		{0, "", 0, N_("Y Axis"), ""},
		{(1<<RV3D_VIEW_BACK), "BACK", 0, N_("Back"), N_("Show background image in back view")},
		{(1<<RV3D_VIEW_FRONT), "FRONT", 0, N_("Front"), N_("Show background image in front view")},
		/*{(1<<RV3D_VIEW_BACK)|(1<<RV3D_VIEW_FRONT), "BACK_FRONT", 0, "Back/Front", ""},*/
		{0, "", 0, N_("Z Axis"), ""},
		{(1<<RV3D_VIEW_BOTTOM), "BOTTOM", 0, N_("Bottom"), N_("Show background image in bottom view")},
		{(1<<RV3D_VIEW_TOP), "TOP", 0, N_("Top"), N_("Show background image in top view")},
		/*{(1<<RV3D_VIEW_BOTTOM)|(1<<RV3D_VIEW_TOP), "BOTTOM_TOP", 0, "Top/Bottom", ""},*/
		{0, "", 0, N_("Other"), ""},
		{0, "ALL", 0, N_("All Views"), N_("Show background image in all views")},
		{(1<<RV3D_VIEW_CAMERA), "CAMERA", 0, N_("Camera"), N_("Show background image in camera view")},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "BackgroundImage", NULL);
	RNA_def_struct_sdna(srna, "BGpic");
	RNA_def_struct_ui_text(srna, N_("Background Image"), N_("Image and settings for display in the 3d View background"));

	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "ima");
	RNA_def_property_ui_text(prop, N_("Image"), N_("Image displayed and edited in this space"));
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, N_("Image User"), N_("Parameters defining which layer, pass and frame of the image is displayed"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_ui_text(prop, N_("X Offset"), N_("Offsets image horizontally from the world origin"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_ui_text(prop, N_("Y Offset"), N_("Offsets image vertically from the world origin"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "size");
	RNA_def_property_ui_text(prop, N_("Size"), N_("Scaling factor for the background image"));
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blend");
	RNA_def_property_float_funcs(prop, "rna_BackgroundImage_opacity_get", "rna_BackgroundImage_opacity_set", NULL);
	RNA_def_property_ui_text(prop, N_("Opacity"), N_("Image opacity to blend the image against the background color"));
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "view_axis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "view");
	RNA_def_property_enum_items(prop, bgpic_axis_items);
	RNA_def_property_ui_text(prop, N_("Image Axis"), N_("The axis to display the image on"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_BGPIC_EXPANDED);
	RNA_def_property_ui_text(prop, N_("Show Expanded"), N_("Show the expanded in the user interface"));
	RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);

}

static void rna_def_space_view3d(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	const int matrix_dimsize[]= {4, 4};
		
	static EnumPropertyItem pivot_items[] = {
		{V3D_CENTER, "BOUNDING_BOX_CENTER", ICON_ROTATE, N_("Bounding Box Center"), N_("Pivot around bounding box center of selected object(s)")},
		{V3D_CURSOR, "CURSOR", ICON_CURSOR, N_("3D Cursor"), N_("Pivot around the 3D cursor")},
		{V3D_LOCAL, "INDIVIDUAL_ORIGINS", ICON_ROTATECOLLECTION, N_("Individual Origins"), N_("Pivot around each object's own origin")},
		{V3D_CENTROID, "MEDIAN_POINT", ICON_ROTATECENTER, N_("Median Point"), N_("Pivot around the median point of selected objects")},
		{V3D_ACTIVE, "ACTIVE_ELEMENT", ICON_ROTACTIVE, N_("Active Element"), N_("Pivot around active object")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem rv3d_persp_items[] = {
		{RV3D_PERSP, "PERSP", 0, N_("Perspective"), ""},
		{RV3D_ORTHO, "ORTHO", 0, N_("Orthographic"), ""},
		{RV3D_CAMOB, "CAMERA", 0, N_("Camera"), ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "SpaceView3D", "Space");
	RNA_def_struct_sdna(srna, "View3D");
	RNA_def_struct_ui_text(srna, N_("3D View Space"), N_("3D View space data"));
	
	prop= RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "camera");
	RNA_def_property_ui_text(prop, N_("Camera"), N_("Active camera used in this view (when unlocked from the scene's active camera)"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "lock_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "ob_centre");
	RNA_def_property_ui_text(prop, N_("Lock to Object"), N_("3D View center is locked to this object's position"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "lock_bone", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "ob_centre_bone");
	RNA_def_property_ui_text(prop, N_("Lock to Bone"), N_("3D View center is locked to this bone's position"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "lock_cursor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ob_centre_cursor", 1);
	RNA_def_property_ui_text(prop, N_("Lock to Cursor"), N_("3D View center is locked to the cursor's position"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "viewport_shade", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "drawtype");
	RNA_def_property_enum_items(prop, viewport_shade_items);
	RNA_def_property_ui_text(prop, N_("Viewport Shading"), N_("Method to display/shade objects in the 3D View"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "local_view", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "localvd");
	RNA_def_property_ui_text(prop, N_("Local View"), N_("Display an isolated sub-set of objects, apart from the scene visibility"));
	
	prop= RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ_LENGTH);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_View3D_CursorLocation_get", "rna_View3D_CursorLocation_set", NULL);
	RNA_def_property_ui_text(prop, N_("3D Cursor Location"), N_("3D cursor location for this view (dependent on local view setting)"));
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "lens", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lens");
	RNA_def_property_ui_text(prop, N_("Lens"), N_("Lens angle (mm) in perspective view"));
	RNA_def_property_range(prop, 1.0f, 250.0f);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "near");
	RNA_def_property_range(prop, 0.001f, FLT_MAX);
	RNA_def_property_ui_text(prop, N_("Clip Start"), N_("3D View near clipping distance"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "far");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, N_("Clip End"), N_("3D View far clipping distance"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "grid_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "grid");
	RNA_def_property_ui_text(prop, N_("Grid Scale"), N_("The distance between 3D View grid lines"));
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "grid_lines", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gridlines");
	RNA_def_property_ui_text(prop, N_("Grid Lines"), N_("The number of grid lines to display in perspective view"));
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "grid_subdivisions", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gridsubdiv");
	RNA_def_property_ui_text(prop, N_("Grid Subdivisions"), N_("The number of subdivisions between grid lines"));
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_floor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_FLOOR);
	RNA_def_property_ui_text(prop, N_("Display Grid Floor"), N_("Show the ground plane grid in perspective view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_axis_x", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_X);
	RNA_def_property_ui_text(prop, N_("Display X Axis"), N_("Show the X axis line in perspective view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_axis_y", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_Y);
	RNA_def_property_ui_text(prop, N_("Display Y Axis"), N_("Show the Y axis line in perspective view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_axis_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_Z);
	RNA_def_property_ui_text(prop, N_("Display Z Axis"), N_("Show the Z axis line in perspective view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_outline_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SELECT_OUTLINE);
	RNA_def_property_ui_text(prop, N_("Outline Selected"), N_("Show an outline highlight around selected objects in non-wireframe views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_all_objects_origin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_DRAW_CENTERS);
	RNA_def_property_ui_text(prop, N_("All Object Origins"), N_("Show the object origin center dot for all (selected and unselected) objects"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "show_relationship_lines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", V3D_HIDE_HELPLINES);
	RNA_def_property_ui_text(prop, N_("Relationship Lines"), N_("Show dashed lines indicating parent or constraint relationships"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "show_textured_solid", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_SOLID_TEX);
	RNA_def_property_ui_text(prop, N_("Textured Solid"), N_("Display face-assigned textures in solid view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "lock_camera", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_LOCK_CAMERA);
	RNA_def_property_ui_text(prop, N_("Lock Camera to View"), N_("Enable view navigation within the camera view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "show_only_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_RENDER_OVERRIDE);
	RNA_def_property_ui_text(prop, N_("Only Render"), N_("Display only objects which will be rendered"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "use_occlude_geometry", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_ZBUF_SELECT);
	RNA_def_property_ui_text(prop, N_("Occlude Geometry"), N_("Limit selection to visible (clipped with depth buffer)"));
	RNA_def_property_ui_icon(prop, ICON_ORTHO, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "background_images", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bgpicbase", NULL);
	RNA_def_property_struct_type(prop, "BackgroundImage");
	RNA_def_property_ui_text(prop, N_("Background Images"), N_("List of background images"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "show_background_images", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_DISPBGPICS);
	RNA_def_property_ui_text(prop, N_("Display Background Images"), N_("Display reference images behind objects in the 3D View"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "around");
	RNA_def_property_enum_items(prop, pivot_items);
	RNA_def_property_ui_text(prop, N_("Pivot Point"), N_("Pivot center for rotation/scaling"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_SpaceView3D_pivot_update");
	
	prop= RNA_def_property(srna, "use_pivot_point_align", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_ALIGN);
	RNA_def_property_ui_text(prop, N_("Align"), N_("Manipulate object centers only"));
	RNA_def_property_ui_icon(prop, ICON_ALIGN, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_SpaceView3D_pivot_update");

	prop= RNA_def_property(srna, "show_manipulator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "twflag", V3D_USE_MANIPULATOR);
	RNA_def_property_ui_text(prop, N_("Manipulator"), N_("Use a 3D manipulator widget for controlling transforms"));
	RNA_def_property_ui_icon(prop, ICON_MANIPUL, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "use_manipulator_translate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "twtype", V3D_MANIP_TRANSLATE);
	RNA_def_property_ui_text(prop, N_("Manipulator Translate"), N_("Use the manipulator for movement transformations"));
	RNA_def_property_ui_icon(prop, ICON_MAN_TRANS, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "use_manipulator_rotate", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "twtype", V3D_MANIP_ROTATE);
	RNA_def_property_ui_text(prop, N_("Manipulator Rotate"), N_("Use the manipulator for rotation transformations"));
	RNA_def_property_ui_icon(prop, ICON_MAN_ROT, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "use_manipulator_scale", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "twtype", V3D_MANIP_SCALE);
	RNA_def_property_ui_text(prop, N_("Manipulator Scale"), N_("Use the manipulator for scale transformations"));
	RNA_def_property_ui_icon(prop, ICON_MAN_SCALE, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	prop= RNA_def_property(srna, "transform_orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "twmode");
	RNA_def_property_enum_items(prop, transform_orientation_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_TransformOrientation_itemf");
	RNA_def_property_ui_text(prop, N_("Transform Orientation"), N_("Transformation orientation"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "current_orientation", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "TransformOrientation");
	RNA_def_property_pointer_funcs(prop, "rna_CurrentOrientation_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, N_("Current Transform Orientation"), N_("Current Transformation orientation"));

	prop= RNA_def_property(srna, "lock_camera_and_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scenelock", 1);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceView3D_lock_camera_and_layers_set");
	RNA_def_property_ui_text(prop, N_("Lock Camera and Layers"), N_("Use the scene's active camera and layers in this view, rather than local layers"));
	RNA_def_property_ui_icon(prop, ICON_LOCKVIEW_OFF, 1);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "layers", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceView3D_layer_set");
	RNA_def_property_ui_text(prop, N_("Visible Layers"), N_("Layers visible in this 3D View"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_SpaceView3D_layer_update");
	
	prop= RNA_def_property(srna, "layers_used", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "lay_used", 1);
	RNA_def_property_array(prop, 20);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Used Layers"), N_("Layers that contain something"));

	prop= RNA_def_property(srna, "region_3d", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "RegionView3D");
	RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_region_3d_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, N_("3D Region"), N_("3D region in this space, in case of quad view the camera region"));

	prop= RNA_def_property(srna, "region_quadview", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "RegionView3D");
	RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_region_quadview_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, N_("Quad View Region"), N_("3D region that defines the quad view settings"));

	/* region */

	srna= RNA_def_struct(brna, "RegionView3D", NULL);
	RNA_def_struct_sdna(srna, "RegionView3D");
	RNA_def_struct_ui_text(srna, N_("3D View Region"), N_("3D View region data"));

	prop= RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_LOCKED);
	RNA_def_property_ui_text(prop, N_("Lock"), N_("Lock view rotation in side views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");
	
	prop= RNA_def_property(srna, "show_sync_view", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_BOXVIEW);
	RNA_def_property_ui_text(prop, N_("Box"), N_("Sync view position between side views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");
	
	prop= RNA_def_property(srna, "use_box_clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_BOXCLIP);
	RNA_def_property_ui_text(prop, N_("Clip"), N_("Clip objects based on what's visible in other side views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_clip_update");
	
	prop= RNA_def_property(srna, "perspective_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "persmat");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // XXX: for now, it's too risky for users to do this
	RNA_def_property_multi_array(prop, 2, matrix_dimsize);
	RNA_def_property_ui_text(prop, N_("Perspective Matrix"), N_("Current perspective matrix of the 3D region"));
	
	prop= RNA_def_property(srna, "view_matrix", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "viewmat");
	RNA_def_property_multi_array(prop, 2, matrix_dimsize);
	RNA_def_property_float_funcs(prop, NULL, "rna_RegionView3D_view_matrix_set", NULL);
	RNA_def_property_ui_text(prop, N_("View Matrix"), N_("Current view matrix of the 3D region"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "view_perspective", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "persp");
	RNA_def_property_enum_items(prop, rv3d_persp_items);
	RNA_def_property_ui_text(prop, N_("Perspective"), N_("View Perspective"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);

	prop= RNA_def_property(srna, "is_perspective", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "is_persp", 1);
	RNA_def_property_ui_text(prop, N_("Is Perspective"), "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	
	prop= RNA_def_property(srna, "view_location", PROP_FLOAT, PROP_TRANSLATION);
#if 0
	RNA_def_property_float_sdna(prop, NULL, "ofs"); // cant use because its negated
#else
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_RegionView3D_view_location_get", "rna_RegionView3D_view_location_set", NULL);
#endif
	RNA_def_property_ui_text(prop, N_("View Location"), N_("View pivot location"));
	RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, 4);
	RNA_def_property_update(prop, NC_WINDOW, NULL);
	
	prop= RNA_def_property(srna, "view_rotation", PROP_FLOAT, PROP_QUATERNION); // cant use because its inverted
#if 0
	RNA_def_property_float_sdna(prop, NULL, "viewquat");
#else
	RNA_def_property_array(prop, 4);
	RNA_def_property_float_funcs(prop, "rna_RegionView3D_view_rotation_get", "rna_RegionView3D_view_rotation_set", NULL);
#endif
	RNA_def_property_ui_text(prop, N_("View Rotation"), N_("Rotation in quaternions (keep normalized)"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
	
	/* not sure we need rna access to these but adding anyway */
	prop= RNA_def_property(srna, "view_distance", PROP_FLOAT, PROP_UNSIGNED);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_ui_text(prop, N_("Distance"), N_("Distance to the view location"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_VIEW3D, NULL);
}

static void rna_def_space_buttons(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem buttons_context_items[] = {
		{BCONTEXT_SCENE, "SCENE", ICON_SCENE, N_("Scene"), "Scene"},
		{BCONTEXT_RENDER, "RENDER", ICON_SCENE_DATA, N_("Render"), "Render"},
		{BCONTEXT_WORLD, "WORLD", ICON_WORLD, N_("World"), "World"},
		{BCONTEXT_OBJECT, "OBJECT", ICON_OBJECT_DATA, N_("Object"), "Object"},
		{BCONTEXT_CONSTRAINT, "CONSTRAINT", ICON_CONSTRAINT, N_("Constraints"), "Constraints"},
		{BCONTEXT_MODIFIER, "MODIFIER", ICON_MODIFIER, N_("Modifiers"), "Modifiers"},
		{BCONTEXT_DATA, "DATA", 0, N_("Data"), "Data"},
		{BCONTEXT_BONE, "BONE", ICON_BONE_DATA, N_("Bone"), "Bone"},
		{BCONTEXT_BONE_CONSTRAINT, "BONE_CONSTRAINT", ICON_CONSTRAINT, N_("Bone Constraints"), "Bone Constraints"},
		{BCONTEXT_MATERIAL, "MATERIAL", ICON_MATERIAL, N_("Material"), "Material"},
		{BCONTEXT_TEXTURE, "TEXTURE", ICON_TEXTURE, N_("Texture"), "Texture"},
		{BCONTEXT_PARTICLE, "PARTICLES", ICON_PARTICLES, N_("Particles"), "Particle"},
		{BCONTEXT_PHYSICS, "PHYSICS", ICON_PHYSICS, N_("Physics"), "Physics"},
		{0, NULL, 0, NULL, NULL}};
		
	static EnumPropertyItem align_items[] = {
		{BUT_HORIZONTAL, "HORIZONTAL", 0, N_("Horizontal"), ""},
		{BUT_VERTICAL, "VERTICAL", 0, N_("Vertical"), ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem buttons_texture_context_items[] = {
		{SB_TEXC_MAT_OR_LAMP, "MATERIAL", ICON_MATERIAL, N_("Material"), "Material"},
		{0, NULL, 0, NULL, NULL}}; //actually populated dynamically trough a function
		
	srna= RNA_def_struct(brna, "SpaceProperties", "Space");
	RNA_def_struct_sdna(srna, "SpaceButs");
	RNA_def_struct_ui_text(srna, N_("Properties Space"), N_("Properties space data"));
	
	prop= RNA_def_property(srna, "context", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mainb");
	RNA_def_property_enum_items(prop, buttons_context_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_SpaceProperties_context_set", NULL);
	RNA_def_property_ui_text(prop, N_("Context"), N_("Type of active data to display and edit"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_PROPERTIES, NULL);
	
	prop= RNA_def_property(srna, "align", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "align");
	RNA_def_property_enum_items(prop, align_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_SpaceProperties_align_set", NULL);
	RNA_def_property_ui_text(prop, N_("Align"), N_("Arrangement of the panels"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_PROPERTIES, NULL);

	prop= RNA_def_property(srna, "texture_context", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, buttons_texture_context_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceProperties_texture_context_itemf");
	RNA_def_property_ui_text(prop, N_("Texture Context"), N_("Type of texture data to display and edit"));
	RNA_def_property_update(prop, NC_TEXTURE, NULL);

	/* pinned data */
	prop= RNA_def_property(srna, "pin_id", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "pinid");
	RNA_def_property_struct_type(prop, "ID");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceProperties_pin_id_set", "rna_SpaceProperties_pin_id_typef", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_PROPERTIES, "rna_SpaceProperties_pin_id_update");

	prop= RNA_def_property(srna, "use_pin_id", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SB_PIN_CONTEXT);
	RNA_def_property_ui_text(prop, N_("Pin ID"), N_("Use the pinned context"));
}

static void rna_def_space_image(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SpaceImageEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceImage");
	RNA_def_struct_ui_text(srna, N_("Space Image Editor"), N_("Image and UV editor space data"));

	/* image */
	prop= RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceImageEditor_image_set", NULL, NULL);
	RNA_def_property_ui_text(prop, N_("Image"), N_("Image displayed and edited in this space"));
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, NC_GEOM|ND_DATA, NULL); // is handled in image editor too

	prop= RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "iuser");
	RNA_def_property_ui_text(prop, N_("Image User"), N_("Parameters defining which layer, pass and frame of the image is displayed"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "cumap");
	RNA_def_property_ui_text(prop, N_("Curve"), N_("Color curve mapping to use for displaying the image"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, "rna_SpaceImageEditor_curves_update");

	prop= RNA_def_property(srna, "scopes", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "scopes");
	RNA_def_property_struct_type(prop, "Scopes");
	RNA_def_property_ui_text(prop, N_("Scopes"), N_("Scopes to visualize image statistics."));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, "rna_SpaceImageEditor_scopes_update");

	prop= RNA_def_property(srna, "use_image_pin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "pin", 0);
	RNA_def_property_ui_text(prop, N_("Image Pin"), N_("Display current image regardless of object selection"));
	RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "sample_histogram", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "sample_line_hist");
	RNA_def_property_struct_type(prop, "Histogram");
	RNA_def_property_ui_text(prop, N_("Line sample"), N_("Sampled colors along line"));

	prop= RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_array(prop, 2);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_funcs(prop, "rna_SpaceImageEditor_zoom_get", NULL, NULL);
	RNA_def_property_ui_text(prop, N_("Zoom"), N_("Zoom factor"));
	
	/* image draw */
	prop= RNA_def_property(srna, "show_repeat", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_TILE);
	RNA_def_property_ui_text(prop, N_("Draw Repeated"), N_("Draw the image repeated outside of the main view"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	prop= RNA_def_property(srna, "draw_channels", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, draw_channels_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceImageEditor_draw_channels_itemf");
	RNA_def_property_ui_text(prop, N_("Draw Channels"), N_("Channels of the image to draw"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, NULL);

	/* uv */
	prop= RNA_def_property(srna, "uv_editor", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "SpaceUVEditor");
	RNA_def_property_pointer_funcs(prop, "rna_SpaceImageEditor_uvedit_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, N_("UV Editor"), N_("UV editor settings"));
	
	/* paint */
	prop= RNA_def_property(srna, "use_image_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAWTOOL);
	RNA_def_property_ui_text(prop, N_("Image Painting"), N_("Enable image painting mode"));
	RNA_def_property_ui_icon(prop, ICON_TPAINT_HLT, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_IMAGE, "rna_SpaceImageEditor_paint_update");

	/* grease pencil */
	prop= RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_struct_type(prop, "GreasePencil");
	RNA_def_property_ui_text(prop, N_("Grease Pencil"), N_("Grease pencil data for this space"));

	prop= RNA_def_property(srna, "use_grease_pencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DISPGP);
	RNA_def_property_ui_text(prop, N_("Use Grease Pencil"), N_("Display and edit the grease pencil freehand annotations overlay"));

	/* update */
	prop= RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "lock", 0);
	RNA_def_property_ui_text(prop, N_("Update Automatically"), N_("Update other affected window spaces automatically to reflect changes during interactive operations such as transform"));

	/* state */
	prop= RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_render_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Show Render"), N_("Show render related properties"));

	prop= RNA_def_property(srna, "show_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_paint_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Show Paint"), N_("Show paint related properties"));

	prop= RNA_def_property(srna, "show_uvedit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_uvedit_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Show UV Editor"), N_("Show UV editing related properties"));

	rna_def_space_image_uv(brna);
}

static void rna_def_space_sequencer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem view_type_items[] = {
		{SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, N_("Sequencer"), ""},
		{SEQ_VIEW_PREVIEW,  "PREVIEW", ICON_SEQ_PREVIEW, N_("Image Preview"), ""},
		{SEQ_VIEW_SEQUENCE_PREVIEW,  "SEQUENCER_PREVIEW", ICON_SEQ_SPLITVIEW, N_("Sequencer and Image Preview"), ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem display_mode_items[] = {
		{SEQ_DRAW_IMG_IMBUF, "IMAGE", ICON_SEQ_PREVIEW, N_("Image Preview"), ""},
		{SEQ_DRAW_IMG_WAVEFORM, "WAVEFORM", ICON_SEQ_LUMA_WAVEFORM, N_("Luma Waveform"), ""},
		{SEQ_DRAW_IMG_VECTORSCOPE, "VECTOR_SCOPE", ICON_SEQ_CHROMA_SCOPE, N_("Chroma Vectorscope"), ""},
		{SEQ_DRAW_IMG_HISTOGRAM, "HISTOGRAM", ICON_SEQ_HISTOGRAM, N_("Histogram"), ""},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem proxy_render_size_items[] = {
		{SEQ_PROXY_RENDER_SIZE_NONE, "NONE", 0, N_("No display"), ""},
		{SEQ_PROXY_RENDER_SIZE_SCENE, "SCENE", 0, N_("Scene render size"), ""},
		{SEQ_PROXY_RENDER_SIZE_25, "PROXY_25", 0, N_("Proxy size 25%"), ""},
		{SEQ_PROXY_RENDER_SIZE_50, "PROXY_50", 0, N_("Proxy size 50%"), ""},
		{SEQ_PROXY_RENDER_SIZE_75, "PROXY_75", 0, N_("Proxy size 75%"), ""},
		{SEQ_PROXY_RENDER_SIZE_FULL, "FULL", 0, N_("No proxy, full render"), ""},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "SpaceSequenceEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceSeq");
	RNA_def_struct_ui_text(srna, N_("Space Sequence Editor"), N_("Sequence editor space data"));
	
	/* view type, fairly important */
	prop= RNA_def_property(srna, "view_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "view");
	RNA_def_property_enum_items(prop, view_type_items);
	RNA_def_property_ui_text(prop, N_("View Type"), N_("The type of the Sequencer view (sequencer, preview or both)"));
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, 0, "rna_Sequencer_display_mode_update");

	/* display type, fairly important */
	prop= RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mainb");
	RNA_def_property_enum_items(prop, display_mode_items);
	RNA_def_property_ui_text(prop, N_("Display Mode"), N_("The view mode to use for displaying sequencer output"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
		
	/* flag's */
	prop= RNA_def_property(srna, "show_frame_indicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SEQ_NO_DRAW_CFRANUM);
	RNA_def_property_ui_text(prop, N_("Show Frame Number Indicator"), N_("Show frame number beside the current frame indicator line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "show_frames", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAWFRAMES);
	RNA_def_property_ui_text(prop, N_("Draw Frames"), N_("Draw frames rather than seconds"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MARKER_TRANS);
	RNA_def_property_ui_text(prop, N_("Transform Markers"), N_("Transform markers as well as strips"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "show_separate_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAW_COLOR_SEPERATED);
	RNA_def_property_ui_text(prop, N_("Separate Colors"), N_("Separate color channels in preview"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "show_safe_margin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAW_SAFE_MARGINS);
	RNA_def_property_ui_text(prop, N_("Safe Margin"), N_("Draw title safe margins in preview"));	
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "use_grease_pencil", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAW_GPENCIL);
	RNA_def_property_ui_text(prop, N_("Use Grease Pencil"), N_("Display and edit the grease pencil freehand annotations overlay"));	
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	/* grease pencil */
	prop= RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "gpd");
	RNA_def_property_struct_type(prop, "UnknownType");
	RNA_def_property_ui_text(prop, N_("Grease Pencil"), N_("Grease pencil data for this space"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "display_channel", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "chanshown");
	RNA_def_property_ui_text(prop, N_("Display Channel"), N_("The channel number shown in the image preview. 0 is the result of all strips combined"));
	RNA_def_property_range(prop, -5, 32); // MAXSEQ --- todo, move from BKE_sequencer.h, allow up to 5 layers up the metastack. Should be dynamic...
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "draw_overexposed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "zebra");
	RNA_def_property_ui_text(prop, N_("Show Overexposed"), N_("Show overexposed areas with zebra stripes"));
	RNA_def_property_range(prop, 0, 110);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "proxy_render_size", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "render_size");
	RNA_def_property_enum_items(prop, proxy_render_size_items);
	RNA_def_property_ui_text(prop, N_("Proxy render size"), N_("Draw preview using full resolution or different proxy resolutions"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	
	/* not sure we need rna access to these but adding anyway */
	prop= RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_ui_text(prop, N_("X Offset"), N_("Offsets image horizontally from the view center"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);

	prop= RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_ui_text(prop, N_("Y Offset"), N_("Offsets image horizontally from the view center"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
	
	prop= RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zoom");
	RNA_def_property_ui_text(prop, N_("Zoom"), N_("Display zoom level"));	
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_SEQUENCER, NULL);
}

static void rna_def_space_text(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SpaceTextEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceText");
	RNA_def_struct_ui_text(srna, N_("Space Text Editor"), N_("Text editor space data"));

	/* text */
	prop= RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Text"), N_("Text displayed and edited in this space"));
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceTextEditor_text_set", NULL, NULL);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	/* display */
	prop= RNA_def_property(srna, "show_word_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "wordwrap", 0);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceTextEditor_word_wrap_set");
	RNA_def_property_ui_text(prop, N_("Word Wrap"), N_("Wrap words if there is not enough horizontal space"));
	RNA_def_property_ui_icon(prop, ICON_WORDWRAP_OFF, 1);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "show_line_numbers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "showlinenrs", 0);
	RNA_def_property_ui_text(prop, N_("Line Numbers"), N_("Show line numbers next to the text"));
	RNA_def_property_ui_icon(prop, ICON_LINENUMBERS_OFF, 1);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "show_syntax_highlight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "showsyntax", 0);
	RNA_def_property_ui_text(prop, N_("Syntax Highlight"), N_("Syntax highlight for scripting"));
	RNA_def_property_ui_icon(prop, ICON_SYNTAX_OFF, 1);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);
	
	prop= RNA_def_property(srna, "show_line_highlight", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "line_hlight", 0);
	RNA_def_property_ui_text(prop, N_("Highlight Line"), N_("Highlight the current line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "tab_width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "tabnumber");
	RNA_def_property_range(prop, 2, 8);
	RNA_def_property_ui_text(prop, N_("Tab Width"), N_("Number of spaces to display tabs with"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, "rna_SpaceTextEditor_updateEdited");

	prop= RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "lheight");
	RNA_def_property_range(prop, 8, 32);
	RNA_def_property_ui_text(prop, N_("Font Size"), N_("Font size to use for displaying the text"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "show_margin", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_SHOW_MARGIN);
	RNA_def_property_ui_text(prop, N_("Show Margin"), N_("Show right margin"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "margin_column", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "margin_column");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, N_("Margin Column"), N_("Column number to show right margin at"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	/* functionality options */
	prop= RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "overwrite", 1);
	RNA_def_property_ui_text(prop, N_("Overwrite"), N_("Overwrite characters when typing rather than inserting them"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);
	
	prop= RNA_def_property(srna, "use_live_edit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "live_edit", 1);
	RNA_def_property_ui_text(prop, N_("Live Edit"), N_("Run python while editing"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);
	
	/* find */
	prop= RNA_def_property(srna, "use_find_all", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_ALL);
	RNA_def_property_ui_text(prop, N_("Find All"), N_("Search in all text datablocks, instead of only the active one"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "use_find_wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_WRAP);
	RNA_def_property_ui_text(prop, N_("Find Wrap"), N_("Search again from the start of the file when reaching the end"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "use_match_case", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_MATCH_CASE);
	RNA_def_property_ui_text(prop, N_("Match case"), N_("Search string is sensitive to uppercase and lowercase letters"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "find_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "findstr");
	RNA_def_property_ui_text(prop, N_("Find Text"), N_("Text to search for with the find tool"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);

	prop= RNA_def_property(srna, "replace_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "replacestr");
	RNA_def_property_ui_text(prop, N_("Replace Text"), N_("Text to replace selected text with using the replace tool"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TEXT, NULL);
}

static void rna_def_space_dopesheet(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem mode_items[] = {
		{SACTCONT_DOPESHEET, "DOPESHEET", 0, N_("DopeSheet"), N_("DopeSheet Editor")},
		{SACTCONT_ACTION, "ACTION", 0, N_("Action Editor"), N_("Action Editor")},
		{SACTCONT_SHAPEKEY, "SHAPEKEY", 0, N_("ShapeKey Editor"), N_("ShapeKey Editor")},
		{SACTCONT_GPENCIL, "GPENCIL", 0, N_("Grease Pencil"), N_("Grease Pencil")},
		{0, NULL, 0, NULL, NULL}};
		
	
	srna= RNA_def_struct(brna, "SpaceDopeSheetEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceAction");
	RNA_def_struct_ui_text(srna, N_("Space DopeSheet Editor"), N_("DopeSheet space data"));

	/* data */
	prop= RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceDopeSheetEditor_action_set", NULL, "rna_Action_actedit_assign_poll");
	RNA_def_property_ui_text(prop, N_("Action"), N_("Action displayed and edited in this space"));
	RNA_def_property_update(prop, NC_ANIMATION|ND_KEYFRAME|NA_EDITED, "rna_SpaceDopeSheetEditor_action_update");
	
	/* mode */
	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_ui_text(prop, N_("Mode"), N_("Editing context being displayed"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, "rna_SpaceDopeSheetEditor_mode_update");
	
	/* display */
	prop= RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_DRAWTIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // XXX for now, only set with operator
	RNA_def_property_ui_text(prop, N_("Show Seconds"), N_("Show timing in seconds not frames"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
	
	prop= RNA_def_property(srna, "show_frame_indicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SACTION_NODRAWCFRANUM);
	RNA_def_property_ui_text(prop, N_("Show Frame Number Indicator"), N_("Show frame number beside the current frame indicator line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
	
	prop= RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SLIDERS);
	RNA_def_property_ui_text(prop, N_("Show Sliders"), N_("Show sliders beside F-Curve channels"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
	
	prop= RNA_def_property(srna, "show_pose_markers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_POSEMARKERS_SHOW);
	RNA_def_property_ui_text(prop, N_("Show Pose Markers"), N_("Show markers belonging to the active action instead of Scene markers (Action and Shape Key Editors only)"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
	
	/* editing */
	prop= RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SACTION_NOTRANSKEYCULL);
	RNA_def_property_ui_text(prop, N_("AutoMerge Keyframes"), N_("Automatically merge nearby keyframes"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
	
	prop= RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SACTION_NOREALTIMEUPDATES);
	RNA_def_property_ui_text(prop, N_("Realtime Updates"), N_("When transforming keyframes, changes to the animation data are flushed to other views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);

	prop= RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_MARKERS_MOVE);
	RNA_def_property_ui_text(prop, N_("Sync Markers"), N_("Sync Markers with keyframe edits"));

	/* dopesheet */
	prop= RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "DopeSheet");
	RNA_def_property_pointer_sdna(prop, NULL, "ads");
	RNA_def_property_ui_text(prop, N_("DopeSheet"), N_("Settings for filtering animation data"));

	/* autosnap */
	prop= RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "autosnap");
	RNA_def_property_enum_items(prop, autosnap_items);
	RNA_def_property_ui_text(prop, N_("Auto Snap"), N_("Automatic time snapping settings for transformations"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_DOPESHEET, NULL);
}

static void rna_def_space_graph(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem mode_items[] = {
		{SIPO_MODE_ANIMATION, "FCURVES", 0, N_("F-Curve Editor"), N_("Edit f-curves")},
		{SIPO_MODE_DRIVERS, "DRIVERS", 0, N_("Drivers"), N_("Edit drivers")},
		{0, NULL, 0, NULL, NULL}};
		
		/* this is basically the same as the one for the 3D-View, but with some entries ommitted */
	static EnumPropertyItem gpivot_items[] = {
		{V3D_CENTER, "BOUNDING_BOX_CENTER", ICON_ROTATE, N_("Bounding Box Center"), ""},
		{V3D_CURSOR, "CURSOR", ICON_CURSOR, N_("2D Cursor"), ""},
		{V3D_LOCAL, "INDIVIDUAL_ORIGINS", ICON_ROTATECOLLECTION, N_("Individual Centers"), ""},
		//{V3D_CENTROID, "MEDIAN_POINT", 0, "Median Point", ""},
		//{V3D_ACTIVE, "ACTIVE_ELEMENT", 0, "Active Element", ""},
		{0, NULL, 0, NULL, NULL}};

	
	srna= RNA_def_struct(brna, "SpaceGraphEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceIpo");
	RNA_def_struct_ui_text(srna, N_("Space Graph Editor"), N_("Graph Editor space data"));
	
	/* mode */
	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, mode_items);
	RNA_def_property_ui_text(prop, N_("Mode"), N_("Editing context being displayed"));
	RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, "rna_SpaceGraphEditor_display_mode_update");
	
	/* display */
	prop= RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_DRAWTIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // XXX for now, only set with operator
	RNA_def_property_ui_text(prop, N_("Show Seconds"), N_("Show timing in seconds not frames"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "show_frame_indicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NODRAWCFRANUM);
	RNA_def_property_ui_text(prop, N_("Show Frame Number Indicator"), N_("Show frame number beside the current frame indicator line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SLIDERS);
	RNA_def_property_ui_text(prop, N_("Show Sliders"), N_("Show sliders beside F-Curve channels"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "show_handles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOHANDLES);
	RNA_def_property_ui_text(prop, N_("Show Handles"), N_("Show handles of Bezier control points"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "use_only_selected_curves_handles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SELCUVERTSONLY);
	RNA_def_property_ui_text(prop, N_("Only Selected Curve Keyframes"), N_("Only keyframes of selected F-Curves are visible and editable"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "use_only_selected_keyframe_handles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SELVHANDLESONLY);
	RNA_def_property_ui_text(prop, N_("Only Selected Keyframes Handles"), N_("Only show and edit handles of selected keyframes"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "use_fancy_drawing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_BEAUTYDRAW_OFF);
	RNA_def_property_ui_text(prop, N_("Use Fancy Drawing"), N_("Draw F-Curves using Anti-Aliasing and other fancy effects. Disable for better performance"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	/* editing */
	prop= RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOTRANSKEYCULL);
	RNA_def_property_ui_text(prop, N_("AutoMerge Keyframes"), N_("Automatically merge nearby keyframes"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOREALTIMEUPDATES);
	RNA_def_property_ui_text(prop, N_("Realtime Updates"), N_("When transforming keyframes, changes to the animation data are flushed to other views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	/* cursor */
	prop= RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NODRAWCURSOR);
	RNA_def_property_ui_text(prop, N_("Show Cursor"), N_("Show 2D cursor"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "cursor_position_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cursorVal");
	RNA_def_property_ui_text(prop, N_("Cursor Y-Value"), N_("Graph Editor 2D-Value cursor - Y-Value component"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	prop= RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "around");
	RNA_def_property_enum_items(prop, gpivot_items);
	RNA_def_property_ui_text(prop, N_("Pivot Point"), N_("Pivot center for rotation/scaling"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);

	/* dopesheet */
	prop= RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "DopeSheet");
	RNA_def_property_pointer_sdna(prop, NULL, "ads");
	RNA_def_property_ui_text(prop, N_("DopeSheet"), N_("Settings for filtering animation data"));

	/* autosnap */
	prop= RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "autosnap");
	RNA_def_property_enum_items(prop, autosnap_items);
	RNA_def_property_ui_text(prop, N_("Auto Snap"), N_("Automatic time snapping settings for transformations"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
	
	/* readonly state info */
	prop= RNA_def_property(srna, "has_ghost_curves", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", 0); /* XXX: hack to make this compile, since this property doesn't actually exist*/
	RNA_def_property_boolean_funcs(prop, "rna_SpaceGraphEditor_has_ghost_curves_get", NULL);
	RNA_def_property_ui_text(prop, N_("Has Ghost Curves"), N_("Graph Editor instance has some ghost curves stored"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_GRAPH, NULL);
}

static void rna_def_space_nla(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "SpaceNLA", "Space");
	RNA_def_struct_sdna(srna, "SpaceNla");
	RNA_def_struct_ui_text(srna, N_("Space Nla Editor"), N_("NLA editor space data"));
	
	/* display */
	prop= RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SNLA_DRAWTIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); // XXX for now, only set with operator
	RNA_def_property_ui_text(prop, N_("Show Seconds"), N_("Show timing in seconds not frames"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NLA, NULL);
	
	prop= RNA_def_property(srna, "show_frame_indicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NODRAWCFRANUM);
	RNA_def_property_ui_text(prop, N_("Show Frame Number Indicator"), N_("Show frame number beside the current frame indicator line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NLA, NULL);
	
	prop= RNA_def_property(srna, "show_strip_curves", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NOSTRIPCURVES);
	RNA_def_property_ui_text(prop, N_("Show Control Curves"), N_("Show influence curves on strips"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NLA, NULL);
	
	/* editing */
	prop= RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NOREALTIMEUPDATES);
	RNA_def_property_ui_text(prop, N_("Realtime Updates"), N_("When transforming strips, changes to the animation data are flushed to other views"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NLA, NULL);

	/* dopesheet */
	prop= RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "DopeSheet");
	RNA_def_property_pointer_sdna(prop, NULL, "ads");
	RNA_def_property_ui_text(prop, N_("DopeSheet"), N_("Settings for filtering animation data"));

	/* autosnap */
	prop= RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "autosnap");
	RNA_def_property_enum_items(prop, autosnap_items);
	RNA_def_property_ui_text(prop, N_("Auto Snap"), N_("Automatic time snapping settings for transformations"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NLA, NULL);
}

static void rna_def_space_time(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "SpaceTimeline", "Space");
	RNA_def_struct_sdna(srna, "SpaceTime");
	RNA_def_struct_ui_text(srna, N_("Space Timeline Editor"), N_("Timeline editor space data"));
	
	/* view settings */	
	prop= RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TIME_ONLYACTSEL);
	RNA_def_property_ui_text(prop, N_("Only Selected channels"), N_("Show keyframes for active Object and/or its selected channels only"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	prop= RNA_def_property(srna, "show_frame_indicator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TIME_CFRA_NUM);
	RNA_def_property_ui_text(prop, N_("Show Frame Number Indicator"), N_("Show frame number beside the current frame indicator line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	/* displaying cache status */
	prop= RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_DISPLAY);
	RNA_def_property_ui_text(prop, N_("Show Cache"), N_("Show the status of cached frames in the timeline"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	prop= RNA_def_property(srna, "cache_softbody", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_SOFTBODY);
	RNA_def_property_ui_text(prop, N_("Softbody"), N_("Show the active object's softbody point cache"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	prop= RNA_def_property(srna, "cache_particles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_PARTICLES);
	RNA_def_property_ui_text(prop, N_("Particles"), N_("Show the active object's particle point cache"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	prop= RNA_def_property(srna, "cache_cloth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_CLOTH);
	RNA_def_property_ui_text(prop, N_("Cloth"), N_("Show the active object's cloth point cache"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
	
	prop= RNA_def_property(srna, "cache_smoke", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_SMOKE);
	RNA_def_property_ui_text(prop, N_("Smoke"), N_("Show the active object's smoke cache"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_TIME, NULL);
}

static void rna_def_console_line(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "ConsoleLine", NULL);
	RNA_def_struct_ui_text(srna, N_("Console Input"), N_("Input line for the interactive console"));
	// XXX using non-inited "prop", uh? RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);
	
	prop= RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, "rna_ConsoleLine_body_get", "rna_ConsoleLine_body_length", "rna_ConsoleLine_body_set");
	RNA_def_property_ui_text(prop, N_("Line"), N_("Text in the line"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);
	
	prop= RNA_def_property(srna, "current_character", PROP_INT, PROP_NONE); /* copied from text editor */
	RNA_def_property_int_sdna(prop, NULL, "cursor");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_ConsoleLine_cursor_index_range");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);
}
	
static void rna_def_space_console(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "SpaceConsole", "Space");
	RNA_def_struct_sdna(srna, "SpaceConsole");
	RNA_def_struct_ui_text(srna, N_("Space Console"), N_("Interactive python console"));
	
	/* display */
	prop= RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE); /* copied from text editor */
	RNA_def_property_int_sdna(prop, NULL, "lheight");
	RNA_def_property_range(prop, 8, 32);
	RNA_def_property_ui_text(prop, N_("Font Size"), N_("Font size to use for displaying the text"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);


	prop= RNA_def_property(srna, "select_start", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
	RNA_def_property_int_sdna(prop, NULL, "sel_start");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);
	
	prop= RNA_def_property(srna, "select_end", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
	RNA_def_property_int_sdna(prop, NULL, "sel_end");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_CONSOLE, NULL);

	prop= RNA_def_property(srna, "prompt", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, N_("Prompt"), N_("Command line prompt"));
	
	prop= RNA_def_property(srna, "language", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, N_("Language"), N_("Command line prompt language"));

	prop= RNA_def_property(srna, "history", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "history", NULL);
	RNA_def_property_struct_type(prop, "ConsoleLine");
	RNA_def_property_ui_text(prop, N_("History"), N_("Command history"));
	
	prop= RNA_def_property(srna, "scrollback", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "scrollback", NULL);
	RNA_def_property_struct_type(prop, "ConsoleLine");
	RNA_def_property_ui_text(prop, N_("Output"), N_("Command output"));
}

static void rna_def_fileselect_params(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem file_display_items[] = {
		{FILE_SHORTDISPLAY, "FILE_SHORTDISPLAY", ICON_SHORTDISPLAY, N_("Short List"), N_("Display files as short list")},
		{FILE_LONGDISPLAY, "FILE_LONGDISPLAY", ICON_LONGDISPLAY, N_("Long List"), N_("Display files as a detailed list")},
		{FILE_IMGDISPLAY, "FILE_IMGDISPLAY", ICON_IMGDISPLAY, N_("Thumbnails"), N_("Display files as thumbnails")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem file_sort_items[] = {
		{FILE_SORT_ALPHA, "FILE_SORT_ALPHA", ICON_SORTALPHA, N_("Sort alphabetically"), N_("Sort the file list alphabetically")},
		{FILE_SORT_EXTENSION, "FILE_SORT_EXTENSION", ICON_SORTBYEXT, N_("Sort by extension"), N_("Sort the file list by extension")},
		{FILE_SORT_TIME, "FILE_SORT_TIME", ICON_SORTTIME, N_("Sort by time"), N_("Sort files by modification time")},
		{FILE_SORT_SIZE, "FILE_SORT_SIZE", ICON_SORTSIZE, N_("Sort by size"), N_("Sort files by size")},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "FileSelectParams", NULL);
	RNA_def_struct_ui_text(srna, N_("File Select Parameters"), N_("File Select Parameters"));

	prop= RNA_def_property(srna, "title", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "title");
	RNA_def_property_ui_text(prop, N_("Title"), N_("Title for the file browser"));
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop= RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "dir");
	RNA_def_property_ui_text(prop, N_("Directory"), N_("Directory displayed in the file browser"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
	RNA_def_property_string_sdna(prop, NULL, "file");
	RNA_def_property_ui_text(prop, N_("File Name"), N_("Active file in the file browser"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "display");
	RNA_def_property_enum_items(prop, file_display_items);
	RNA_def_property_ui_text(prop, N_("Display Mode"), N_("Display mode for the file list"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", FILE_FILTER);
	RNA_def_property_ui_text(prop, N_("Filter Files"), N_("Enable filtering of files"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", FILE_HIDE_DOT);
	RNA_def_property_ui_text(prop, N_("Show Hidden"), N_("Show hidden dot files"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS , NULL);

	prop= RNA_def_property(srna, "sort_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "sort");
	RNA_def_property_enum_items(prop, file_sort_items);
	RNA_def_property_ui_text(prop, N_("Sort"), "");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_image", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", IMAGEFILE);
	RNA_def_property_ui_text(prop, N_("Filter Images"), N_("Show image files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_IMAGE, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_blender", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", BLENDERFILE);
	RNA_def_property_ui_text(prop, N_("Filter Blender"), N_("Show .blend files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_BLEND, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_movie", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", MOVIEFILE);
	RNA_def_property_ui_text(prop, N_("Filter Movies"), N_("Show movie files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_MOVIE, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_script", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", PYSCRIPTFILE);
	RNA_def_property_ui_text(prop, N_("Filter Script"), N_("Show script files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_SCRIPT, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_font", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", FTFONTFILE);
	RNA_def_property_ui_text(prop, N_("Filter Fonts"), N_("Show font files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_FONT, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_sound", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", SOUNDFILE);
	RNA_def_property_ui_text(prop, N_("Filter Sound"), N_("Show sound files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_SOUND, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_text", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", TEXTFILE);
	RNA_def_property_ui_text(prop, N_("Filter Text"), N_("Show text files"));
	RNA_def_property_ui_icon(prop, ICON_FILE_BLANK, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);

	prop= RNA_def_property(srna, "use_filter_folder", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "filter", FOLDERFILE);
	RNA_def_property_ui_text(prop, N_("Filter Folder"), N_("Show folders"));
	RNA_def_property_ui_icon(prop, ICON_FILE_FOLDER, 0);
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_PARAMS, NULL);
	
	prop= RNA_def_property(srna, "filter_glob", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "filter_glob");
	RNA_def_property_ui_text(prop, N_("Extension Filter"), "");
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_FILE_LIST, NULL);

}

static void rna_def_space_filebrowser(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SpaceFileBrowser", "Space");
	RNA_def_struct_sdna(srna, "SpaceFile");
	RNA_def_struct_ui_text(srna, N_("Space File Browser"), N_("File browser space data"));

	prop= RNA_def_property(srna, "params", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "params");
	RNA_def_property_ui_text(prop, N_("Filebrowser Parameter"), N_("Parameters and Settings for the Filebrowser"));
	
	prop= RNA_def_property(srna, "operator", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "op");
	RNA_def_property_ui_text(prop, N_("Operator"), "");
}

static void rna_def_space_info(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SpaceInfo", "Space");
	RNA_def_struct_sdna(srna, "SpaceInfo");
	RNA_def_struct_ui_text(srna, N_("Space Info"), N_("Info space data"));
	
	/* reporting display */
	prop= RNA_def_property(srna, "show_report_debug", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_DEBUG);
	RNA_def_property_ui_text(prop, N_("Show Debug"), N_("Display debug reporting info"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);
	
	prop= RNA_def_property(srna, "show_report_info", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_INFO);
	RNA_def_property_ui_text(prop, N_("Show Info"), N_("Display general information"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);
	
	prop= RNA_def_property(srna, "show_report_operator", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_OP);
	RNA_def_property_ui_text(prop, N_("Show Operator"), N_("Display the operator log"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);
	
	prop= RNA_def_property(srna, "show_report_warning", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_WARN);
	RNA_def_property_ui_text(prop, N_("Show Warn"), N_("Display warnings"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);
	
	prop= RNA_def_property(srna, "show_report_error", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_ERR);
	RNA_def_property_ui_text(prop, N_("Show Error"), N_("Display error text"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_INFO_REPORT, NULL);	
}

static void rna_def_space_userpref(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna= RNA_def_struct(brna, "SpaceUserPreferences", "Space");
	RNA_def_struct_sdna(srna, "SpaceUserPref");
	RNA_def_struct_ui_text(srna, N_("Space User Preferences"), N_("User preferences space data"));
	
	prop= RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "filter");
	RNA_def_property_ui_text(prop, N_("Filter"), N_("Search term for filtering in the UI"));

}

static void rna_def_space_node(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem tree_type_items[] = {
		{NTREE_SHADER, "MATERIAL", ICON_MATERIAL, N_("Material"), N_("Material nodes")},
		{NTREE_TEXTURE, "TEXTURE", ICON_TEXTURE, N_("Texture"), N_("Texture nodes")},
		{NTREE_COMPOSIT, "COMPOSITING", ICON_RENDERLAYERS, N_("Compositing"), N_("Compositing nodes")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem texture_type_items[] = {
		{SNODE_TEX_OBJECT, "OBJECT", ICON_OBJECT_DATA, N_("Object"), N_("Edit texture nodes from Object")},
		{SNODE_TEX_WORLD, "WORLD", ICON_WORLD_DATA, N_("World"), N_("Edit texture nodes from World")},
		{SNODE_TEX_BRUSH, "BRUSH", ICON_BRUSH_DATA, N_("Brush"), N_("Edit texture nodes from Brush")},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem backdrop_channels_items[] = {
		{0, "COLOR", ICON_IMAGE_RGB, N_("Color"), N_("Draw image with RGB colors")},
		{SNODE_USE_ALPHA, "COLOR_ALPHA", ICON_IMAGE_RGB_ALPHA, N_("Color and Alpha"), N_("Draw image with RGB colors and alpha transparency")},
		{SNODE_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, N_("Alpha"), N_("Draw alpha transparency channel")},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "SpaceNodeEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceNode");
	RNA_def_struct_ui_text(srna, N_("Space Node Editor"), N_("Node editor space data"));

	prop= RNA_def_property(srna, "tree_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "treetype");
	RNA_def_property_enum_items(prop, tree_type_items);
	RNA_def_property_ui_text(prop, N_("Tree Type"), N_("Node tree type to display and edit"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE, NULL);

	prop= RNA_def_property(srna, "texture_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texfrom");
	RNA_def_property_enum_items(prop, texture_type_items);
	RNA_def_property_ui_text(prop, N_("Texture Type"), N_("Type of data to take texture from"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE, NULL);

	prop= RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "ID", N_("Datablock whose nodes are being edited"));

	prop= RNA_def_property(srna, "id_from", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "from");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("ID From"), N_("Datablock from which the edited datablock is linked"));

	prop= RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, N_("Node Tree"), N_("Node tree being displayed and edited"));

	prop= RNA_def_property(srna, "show_backdrop", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_BACKDRAW);
	RNA_def_property_ui_text(prop, N_("Backdrop"), N_("Use active Viewer Node output as backdrop for compositing nodes"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);

	prop= RNA_def_property(srna, "use_auto_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_AUTO_RENDER);
	RNA_def_property_ui_text(prop, N_("Auto Render"), N_("Re-render and composite changed layer on 3D edits"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);
	
	prop= RNA_def_property(srna, "backdrop_zoom", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zoom");
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_range(prop, 0.01f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
	RNA_def_property_ui_text(prop, N_("Backdrop Zoom"), N_("Backdrop zoom factor"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);
	
	prop= RNA_def_property(srna, "backdrop_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_ui_text(prop, N_("Backdrop X"), N_("Backdrop X offset"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);

	prop= RNA_def_property(srna, "backdrop_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_ui_text(prop, N_("Backdrop Y"), N_("Backdrop Y offset"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);

	prop= RNA_def_property(srna, "backdrop_channels", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, backdrop_channels_items);
	RNA_def_property_ui_text(prop, N_("Draw Channels"), N_("Channels of the image to draw"));
	RNA_def_property_update(prop, NC_SPACE|ND_SPACE_NODE_VIEW, NULL);
}

static void rna_def_space_logic(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "SpaceLogicEditor", "Space");
	RNA_def_struct_sdna(srna, "SpaceLogic");
	RNA_def_struct_ui_text(srna, N_("Space Logic Editor"), N_("Logic editor space data"));

	/* sensors */
	prop= RNA_def_property(srna, "show_sensors_selected_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_SENS_SEL);
	RNA_def_property_ui_text(prop, N_("Show Selected Object"), N_("Show sensors of all selected objects"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "show_sensors_active_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_SENS_ACT);
	RNA_def_property_ui_text(prop, N_("Show Active Object"), N_("Show sensors of active object"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "show_sensors_linked_controller", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_SENS_LINK);
	RNA_def_property_ui_text(prop, N_("Show Linked to Controller"), N_("Show linked objects to the controller"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "show_sensors_active_states", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_SENS_STATE);
	RNA_def_property_ui_text(prop, N_("Show Active States"), N_("Show only sensors connected to active states"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* controllers */
	prop= RNA_def_property(srna, "show_controllers_selected_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_CONT_SEL);
	RNA_def_property_ui_text(prop, N_("Show Selected Object"), N_("Show controllers of all selected objects"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "show_controllers_active_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_CONT_ACT);
	RNA_def_property_ui_text(prop, N_("Show Active Object"), N_("Show controllers of active object"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "show_controllers_linked_controller", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_CONT_LINK);
	RNA_def_property_ui_text(prop, N_("Show Linked to Controller"), N_("Show linked objects to sensor/actuator"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	/* actuators */
	prop= RNA_def_property(srna, "show_actuators_selected_objects", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_ACT_SEL);
	RNA_def_property_ui_text(prop, N_("Show Selected Object"), N_("Show actuators of all selected objects"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "show_actuators_active_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_ACT_ACT);
	RNA_def_property_ui_text(prop, N_("Show Active Object"), N_("Show actuators of active object"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);
	
	prop= RNA_def_property(srna, "show_actuators_linked_controller", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_ACT_LINK);
	RNA_def_property_ui_text(prop, N_("Show Linked to Actuator"), N_("Show linked objects to the actuator"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

	prop= RNA_def_property(srna, "show_actuators_active_states", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "scaflag", BUTS_ACT_STATE);
	RNA_def_property_ui_text(prop, N_("Show Active States"), N_("Show only actuators connected to active states"));
	RNA_def_property_update(prop, NC_LOGIC, NULL);

}

void RNA_def_space(BlenderRNA *brna)
{
	rna_def_space(brna);
	rna_def_space_image(brna);
	rna_def_space_sequencer(brna);
	rna_def_space_text(brna);
	rna_def_fileselect_params(brna);
	rna_def_space_filebrowser(brna);
	rna_def_space_outliner(brna);
	rna_def_background_image(brna);
	rna_def_space_view3d(brna);
	rna_def_space_buttons(brna);
	rna_def_space_dopesheet(brna);
	rna_def_space_graph(brna);
	rna_def_space_nla(brna);
	rna_def_space_time(brna);
	rna_def_space_console(brna);
	rna_def_console_line(brna);
	rna_def_space_info(brna);
	rna_def_space_userpref(brna);
	rna_def_space_node(brna);
	rna_def_space_logic(brna);
}

#endif

