/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_threads.h"
#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "RE_pipeline.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_types.h"
#include "WM_api.h"

#include "buttons_intern.h"

#define R_DISPLAYIMAGE  0
#define R_DISPLAYWIN    1
#define R_DISPLAYSCREEN 2

#if 0
static void render_panel_output(const bContext *C, ARegion *ar)
{
	uiBlock *block;
	Scene *scene= CTX_data_scene(C);
	//ID *id;
	int a,b;
	//char *strp;

	block= uiBeginBlock(C, ar, "render_panel_output", UI_EMBOSS);
	if(uiNewPanel(C, ar, block, "Output", "Render", 0, 0, 318, 204)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefIconBut(block, BUT, 0, ICON_FILESEL,	10, 190, 20, 20, 0, 0, 0, 0, 0, "Select the directory/name for saving animations");
	uiDefBut(block, TEX,0,"",							31, 190, 279, 20,scene->r.pic, 0.0,79.0, 0, 0, "Directory/name to save animations, # characters defines the position and length of frame numbers");
	uiDefIconBut(block, BUT,0, ICON_FILESEL, 10, 168, 20, 20, 0, 0, 0, 0, 0, "Select the directory/name for a Backbuf image");
	uiDefBut(block, TEX,0,"",							31, 168, 259, 20,scene->r.backbuf, 0.0,79.0, 0, 0, "Image to use as background for rendering");
	uiDefIconButBitS(block, ICONTOG, R_BACKBUF, 0, ICON_CHECKBOX_HLT-1,	290, 168, 20, 20, &scene->r.bufflag, 0.0, 0.0, 0, 0, "Enable/Disable use of Backbuf image");
	uiBlockEndAlign(block);
	
	uiDefButBitI(block, TOG, R_EXTENSION, 0, "Extensions", 10, 142, 100, 20, &scene->r.scemode, 0.0, 0.0, 0, 0, "Adds filetype extensions to the filename when rendering animations");
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_TOUCH, 0, "Touch",	170, 142, 50, 20, &scene->r.mode, 0.0, 0.0, 0, 0, "Create an empty file before rendering each frame, remove if cancelled (and empty)");
	uiDefButBitI(block, TOG, R_NO_OVERWRITE, 0, "No Overwrite", 220, 142, 90, 20, &scene->r.mode, 0.0, 0.0, 0, 0, "Skip rendering frames when the file exists (image output only)");
	uiBlockEndAlign(block);
	
	/* SET BUTTON */
	uiBlockBeginAlign(block);
	/*XXX id= (ID *)scene->set;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), id, &(G.buts->menunr));
	if(strp[0])
		uiDefButS(block, MENU, 0, strp,			10, 114, 20, 20, &(G.buts->menunr), 0, 0, 0, 0, "Scene to link as a Set");
	MEM_freeN(strp);*/

	if(scene->set) {
		uiBlockSetButLock(block, 1, NULL);
		//XXX uiDefIDPoinBut(block, test_scenepoin_but, ID_SCE, 0, "",	31, 114, 100, 20, &(scene->set), "Name of the Set");
		uiBlockClearButLock(block);
		uiDefIconBut(block, BUT, 0, ICON_X, 		132, 114, 20, 20, 0, 0, 0, 0, 0, "Remove Set link");
	} else {
		uiDefBut(block, LABEL, 0, "No Set Scene", 31, 114, 200, 20, 0, 0, 0, 0, 0, "");
	}
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefIconButBitI(block, TOGN, R_FIXED_THREADS, 0, ICON_AUTO,	10, 63, 20, 20, &scene->r.mode, 0.0, 0.0, 0, 0, "Automatically set the threads to the number of processors on the system");
	if ((scene->r.mode & R_FIXED_THREADS)==0) {
		char thread_str[16];
		sprintf(thread_str, " Threads: %d", BLI_system_thread_count());
		uiDefBut(block, LABEL, 0, thread_str, 30, 63,80,20, 0, 0, 0, 0, 0, "");
	} else {
		uiDefButS(block, NUM, 0, "Threads:", 30, 63, 80, 20, &scene->r.threads, 1, BLENDER_MAX_THREADS, 0, 0, "Amount of threads for render (takes advantage of multi-core and multi-processor computers)");
	}
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	for(b=2; b>=0; b--)
		for(a=0; a<3; a++)
			uiDefButBitS(block, TOG, 1<<(3*b+a), 800,"",	(short)(10+18*a),(short)(10+14*b),16,12, &G.winpos, 0, 0, 0, 0, "Render window placement on screen");
	uiBlockEndAlign(block);

#ifdef WITH_OPENEXR
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_EXR_TILE_FILE, 0, "Save Buffers", 72, 31, 120, 19, &scene->r.scemode, 0.0, 0.0, 0, 0, "Save tiles for all RenderLayers and used SceneNodes to files in the temp directory (saves memory, allows Full Sampling)");
	if(scene->r.scemode & R_EXR_TILE_FILE)
		uiDefButBitI(block, TOG, R_FULL_SAMPLE, 0, "FullSample",	 192, 31, 118, 19, &scene->r.scemode, 0.0, 0.0, 0, 0, "Saves for every OSA sample the entire RenderLayer results (Higher quality sampling but slower)");
	uiBlockEndAlign(block);
#endif
	
	uiDefButS(block, MENU, 0, "Render Display %t|Render Window %x1|Image Editor %x0|Full Screen %x2",	
					72, 10, 120, 19, &G.displaymode, 0.0, (float)R_DISPLAYWIN, 0, 0, "Sets render output display");
	
	/* Dither control */
	uiDefButF(block, NUM,0, "Dither:",         10,89,100,19, &scene->r.dither_intensity, 0.0, 2.0, 0, 0, "The amount of dithering noise present in the output image (0.0 = no dithering)");
	
	/* Toon shading buttons */
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_EDGE, 0,"Edge",   115, 89, 60, 20, &scene->r.mode, 0, 0, 0, 0, "Enable Toon Edge-enhance");
	//XXX uiDefBlockBut(block, edge_render_menu, NULL, "Edge Settings", 175, 89, 135, 20, "Display Edge settings");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_NO_TEX, 0, "Disable Tex", 115, 63, 75, 20, &scene->r.scemode, 0.0, 0.0, 0, 0, "Disables Textures for render");
	uiDefButBitI(block, TOG, R_FREE_IMAGE, 0, "Free Tex Images", 210, 63, 100, 20, &scene->r.scemode, 0.0, 0.0, 0, 0, "Frees all Images used by Textures after each render");
	uiBlockEndAlign(block);

	uiEndBlock(C, block);
}
#endif

#if 0
static void do_bake_func(bContext *C, void *unused_v, void *unused_p)
{
	//XXX objects_bake_render_ui(0);
}

static void render_panel_bake(const bContext *C, ARegion *ar)
{
	uiBlock *block;
	Scene *scene= CTX_data_scene(C);
	uiBut *but;
	
	block= uiBeginBlock(C, ar, "render_panel_bake", UI_EMBOSS);
	uiNewPanelTabbed("Anim", "Render");
	if(uiNewPanel(C, ar, block, "Bake", "Render", 320, 0, 318, 204)==0) return;
	
	but= uiDefBut(block, BUT, 0, "BAKE",	10, 150, 190,40, 0, 0, 0, 0, 0, "Start the bake render for selected Objects");
	uiButSetFunc(but, do_bake_func, NULL, NULL);

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, R_BAKE_TO_ACTIVE, 0, "Selected to Active", 10,120,190,20,&scene->r.bake_flag, 0.0, 0, 0, 0, "Bake shading on the surface of selected objects to the active object");
	uiDefButF(block, NUM, 0, "Dist:", 10,100,95,20,&scene->r.bake_maxdist, 0.0, 1000.0, 1, 0, "Maximum distance from active object to other object (in blender units)");
	uiDefButF(block, NUM, 0, "Bias:", 105,100,95,20,&scene->r.bake_biasdist, 0.0, 1000.0, 1, 0, "Bias towards faces further away from the object (in blender units)");
	uiBlockEndAlign(block);

	if(scene->r.bake_mode == RE_BAKE_NORMALS)
		uiDefButS(block, MENU, 0, "Normal Space %t|Camera %x0|World %x1|Object %x2|Tangent %x3", 
			10,70,190,20, &scene->r.bake_normal_space, 0, 0, 0, 0, "Choose normal space for baking");
	else if(scene->r.bake_mode == RE_BAKE_AO || scene->r.bake_mode == RE_BAKE_DISPLACEMENT) {
		uiDefButBitS(block, TOG, R_BAKE_NORMALIZE, 0, "Normalized", 10,70,190,20, &scene->r.bake_flag, 0.0, 0, 0, 0,
				scene->r.bake_mode == RE_BAKE_AO ?
				 "Bake ambient occlusion normalized, without taking into acount material settings":
				 "Normalized displacement value to fit the 'Dist' range"
		);
	}
	
	uiDefButS(block, MENU, 0, "Quad Split Order%t|Quad Split Auto%x0|Quad Split A (0,1,2) (0,2,3)%x1|Quad Split B (1,2,3) (1,3,0)%x2", 
		10,10,190,20, &scene->r.bake_quad_split, 0, 0, 0, 0, "Method to divide quads (use A or B for external applications that use a fixed order)");
	
#if 0	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, R_BAKE_OSA, 0, "OSA",		10,120,190,20, &scene->r.bake_flag, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButS(block, ROW,0,"5",			10,100,50,20,&scene->r.bake_osa,2.0,5.0, 0, 0, "Sets oversample level to 5");
	uiDefButS(block, ROW,0,"8",			60,100,45,20,&scene->r.bake_osa,2.0,8.0, 0, 0, "Sets oversample level to 8");
	uiDefButS(block, ROW,0,"11",			105,100,45,20,&scene->r.bake_osa,2.0,11.0, 0, 0, "Sets oversample level to 11");
	uiDefButS(block, ROW,0,"16",			150,100,50,20,&scene->r.bake_osa,2.0,16.0, 0, 0, "Sets oversample level to 16");
#endif	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,0,"Full Render",		210,170,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_ALL, 0, 0, "");
	uiDefButS(block, ROW,0,"Ambient Occlusion",210,150,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_AO, 0, 0, "");
	uiDefButS(block, ROW,0,"Shadow",			210,130,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_SHADOW, 0, 0, "");
	uiDefButS(block, ROW,0,"Normals",			210,110,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_NORMALS, 0, 0, "");
	uiDefButS(block, ROW,0,"Textures",			210,90,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_TEXTURE, 0, 0, "");
	uiDefButS(block, ROW,0,"Displacement",		210,70,120,20,&scene->r.bake_mode, 1.0, RE_BAKE_DISPLACEMENT, 0, 0, "");
	uiBlockEndAlign(block);
	
	uiDefButBitS(block, TOG, R_BAKE_CLEAR, 0, "Clear",		210,40,120,20,&scene->r.bake_flag, 0.0, 0, 0, 0, "Clear Images before baking");
	
	uiDefButS(block, NUM, 0,"Margin:",				210,10,120,20,&scene->r.bake_filter, 0.0, 32.0, 0, 0, "Amount of pixels to extend the baked result with, as post process filter");

	uiEndBlock(C, block);
}
#endif

static void render_panel_shading(const bContext *C, Panel *pnl)
{
	uiLayout *layout= pnl->layout;
	Scene *scene= CTX_data_scene(C);
	PointerRNA sceneptr, renderptr;

	RNA_id_pointer_create(&scene->id, &sceneptr);
	renderptr = RNA_pointer_get(&sceneptr, "render_data");
		
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, "Shadow", 0, &renderptr, "render_shadows");
	uiItemR(layout, "SSS", 0, &renderptr, "render_sss");
	uiItemR(layout, "EnvMap", 0, &renderptr, "render_envmaps");
	uiItemR(layout, "Radio", 0, &renderptr, "render_radiosity");
	uiItemR(layout, "Ray Tracing", 0, &renderptr, "render_raytracing");
	uiItemR(layout, NULL, 0, &renderptr, "octree_resolution");

	uiTemplateColumn(layout);
	uiItemR(layout, NULL, 0, &renderptr, "alpha_mode");
	
}
static void render_panel_image(const bContext *C, Panel *pnl)
{
	uiLayout *layout= pnl->layout;
	Scene *scene= CTX_data_scene(C);
	PointerRNA sceneptr, renderptr;

	RNA_id_pointer_create(&scene->id, &sceneptr);
	renderptr = RNA_pointer_get(&sceneptr, "render_data");
	
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, "SizeX", 0, &renderptr, "resolution_x");
	uiItemR(layout, "SizeY", 0, &renderptr, "resolution_y");
	uiItemR(layout, "AspX", 0, &renderptr, "pixel_aspect_x");
	uiItemR(layout, "AspY", 0, &renderptr, "pixel_aspect_y");

	uiTemplateColumn(layout);
	uiItemR(layout, NULL, 0, &renderptr, "crop_to_border");

}
static void render_panel_antialiasing(const bContext *C, Panel *pnl)
{
	uiLayout *layout= pnl->layout;
	Scene *scene= CTX_data_scene(C);
	PointerRNA sceneptr, renderptr;

	RNA_id_pointer_create(&scene->id, &sceneptr);
	renderptr = RNA_pointer_get(&sceneptr, "render_data");
	
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, "Enable", 0, &renderptr, "antialiasing");
	uiItemR(layout, "Samples", 0, &renderptr, "antialiasing_samples");
	uiItemR(layout, NULL, 0, &renderptr, "pixel_filter");
	uiItemR(layout, NULL, 0, &renderptr, "filter_size");
	
}

static void render_panel_render(const bContext *C, Panel *pnl)
{
	uiLayout *layout= pnl->layout;
	Scene *scene= CTX_data_scene(C);
	PointerRNA sceneptr, renderptr;

	RNA_id_pointer_create(&scene->id, &sceneptr);
	renderptr = RNA_pointer_get(&sceneptr, "render_data");

	uiTemplateColumnFlow(layout, 2);
	uiItemO(layout, "RENDER", ICON_SCENE, "SCREEN_OT_render");
	uiItemBooleanO(layout, "ANIM", 0, "SCREEN_OT_render", "anim", 1);

	uiTemplateColumnFlow(layout, 3);
	uiItemR(layout, "Start", 0, &sceneptr, "start_frame");
	uiItemR(layout, "End", 0, &sceneptr, "end_frame");
	uiItemR(layout, "Frame", 0, &sceneptr, "current_frame");

	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, NULL, 0, &renderptr, "do_composite");
	uiItemR(layout, NULL, 0, &renderptr, "do_sequence");
	uiTemplateColumn(layout);
	uiItemR(layout, "Camera:", 0, &sceneptr, "camera");
	
	uiTemplateColumn(layout);
	uiItemL(layout, "General:", 0);
	uiTemplateColumn(layout);
	uiItemR(layout, "Size ", 0, &renderptr, "resolution_percentage");
	uiItemR(layout, NULL, 0, &renderptr, "dither_intensity");
	
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, NULL, 0, &renderptr, "parts_x");
	uiItemR(layout, NULL, 0, &renderptr, "parts_y");
	
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, NULL, 0, &renderptr, "threads");
	uiItemR(layout, "", 0, &renderptr, "threads_mode");
	
	uiTemplateColumnFlow(layout, 3);
	uiItemR(layout, "Fields", 0, &renderptr, "fields");
	uiItemR(layout, "Order", 0, &renderptr, "field_order");
	uiItemR(layout, "Still", 0, &renderptr, "fields_still");
	
	uiTemplateColumn(layout);
	uiItemL(layout, "Extra:", 0);
	uiTemplateColumnFlow(layout, 2);
	uiItemR(layout, "Border Render", 0, &renderptr, "border");
	uiItemR(layout, NULL, 0, &renderptr, "panorama");


#if 0
	block= uiBeginBlock(C, ar, "render_panel_render", UI_EMBOSS);
	if(uiNewPanel(C, ar, block, "Render", "Render", 320, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefButO(block, BUT, "SCREEN_OT_render", WM_OP_INVOKE_DEFAULT, "RENDER",  369, 164, 191,37, "Render the current frame (F12)");
	
#ifndef DISABLE_YAFRAY
	/* yafray: on request, render engine menu is back again, and moved to Render panel */
	uiDefButS(block, MENU, 0, "Rendering Engine %t|Blender Internal %x0|YafRay %x1", 
												369, 142, 191, 20, &scene->r.renderer, 0, 0, 0, 0, "Choose rendering engine");	
#else
	uiDefButS(block, MENU, 0, "Rendering Engine %t|Blender Internal %x0", 
												369, 142, 191, 20, &scene->r.renderer, 0, 0, 0, 0, "Choose rendering engine");	
#endif /* disable yafray */

	uiBlockBeginAlign(block);
	if((scene->r.scemode & R_FULL_SAMPLE) && (scene->r.scemode & R_EXR_TILE_FILE))
		uiDefButBitI(block, TOG, R_OSA, 0, "FSA",	369,109,122,20,&scene->r.mode, 0, 0, 0, 0, "Saves all samples, then composites, and then merges (for best Anti-aliasing)");
	else
		uiDefButBitI(block, TOG, R_OSA, 0, "OSA",	369,109,122,20,&scene->r.mode, 0, 0, 0, 0, "Enables Oversampling (Anti-aliasing)");
	uiDefButS(block, ROW,0,"5",			369,88,29,20,&scene->r.osa,2.0,5.0, 0, 0, "Render 5 samples per pixel for smooth edges (Fast)");
	uiDefButS(block, ROW,0,"8",			400,88,29,20,&scene->r.osa,2.0,8.0, 0, 0, "Render 8 samples per pixel for smooth edges (Recommended)");
	uiDefButS(block, ROW,0,"11",			431,88,29,20,&scene->r.osa,2.0,11.0, 0, 0, "Render 11 samples per pixel for smooth edges (High Quality)");
	uiDefButS(block, ROW,0,"16",			462,88,29,20,&scene->r.osa,2.0,16.0, 0, 0, "Render 16 samples per pixel for smooth edges (Highest Quality)");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_MBLUR, 0, "MBLUR",	496,109,64,20,&scene->r.mode, 0, 0, 0, 0, "Enables Motion Blur calculation");
	uiDefButF(block, NUM,0,"Bf:",			496,88,64,20,&scene->r.blurfac, 0.01, 5.0, 10, 2, "Sets motion blur factor");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM,0,"Xparts:",		369,46,95,29,&scene->r.xparts,1.0, 512.0, 0, 0, "Sets the number of horizontal parts to render image in (For panorama sets number of camera slices)");
	uiDefButS(block, NUM,0,"Yparts:",		465,46,95,29,&scene->r.yparts,1.0, 64.0, 0, 0, "Sets the number of vertical parts to render image in");
	uiBlockEndAlign(block);

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,800,"Sky",		369,13,35,20,&scene->r.alphamode,3.0,0.0, 0, 0, "Fill background with sky");
	uiDefButS(block, ROW,800,"Premul",	405,13,50,20,&scene->r.alphamode,3.0,1.0, 0, 0, "Multiply alpha in advance");
	uiDefButS(block, ROW,800,"Key",		456,13,35,20,&scene->r.alphamode,3.0,2.0, 0, 0, "Alpha and color values remain unchanged");
	uiBlockEndAlign(block);

	uiDefButS(block, MENU, 0,"Octree resolution %t|64 %x64|128 %x128|256 %x256|512 %x512",	496,13,64,20,&scene->r.ocres,0.0,0.0, 0, 0, "Octree resolution for ray tracing and baking, Use higher values for complex scenes");

	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_SHADOW, 0,"Shadow",	565,172,52,29, &scene->r.mode, 0, 0, 0, 0, "Enable shadow calculation");
	uiDefButBitI(block, TOG, R_SSS, 0,"SSS",	617,172,32,29, &scene->r.mode, 0, 0, 0, 0, "Enable subsurface scattering map rendering");
	uiDefButBitI(block, TOG, R_PANORAMA, 0,"Pano",	649,172,38,29, &scene->r.mode, 0, 0, 0, 0, "Enable panorama rendering (output width is multiplied by Xparts)");
	uiDefButBitI(block, TOG, R_ENVMAP, 0,"EnvMap",	565,142,52,29, &scene->r.mode, 0, 0, 0, 0, "Enable environment map rendering");
	uiDefButBitI(block, TOG, R_RAYTRACE, 0,"Ray",617,142,32,29, &scene->r.mode, 0, 0, 0, 0, "Enable ray tracing");
	uiDefButBitI(block, TOG, R_RADIO, 0,"Radio",	649,142,38,29, &scene->r.mode, 0, 0, 0, 0, "Enable radiosity rendering");
	uiBlockEndAlign(block);
	
	uiDefButS(block, NUMSLI, 0, "Size %: ",
			  565,109,122,20,
			  &(scene->r.size), 1.0, 100.0, 0, 0,
			  "Render at percentage of frame size");
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_FIELDS, 0,"Fields",  565,55,60,20,&scene->r.mode, 0, 0, 0, 0, "Enables field rendering");
	uiDefButBitI(block, TOG, R_ODDFIELD, 0,"Odd",	627,55,39,20,&scene->r.mode, 0, 0, 0, 0, "Enables Odd field first rendering (Default: Even field)");
	uiDefButBitI(block, TOG, R_FIELDSTILL, 0,"X",		668,55,19,20,&scene->r.mode, 0, 0, 0, 0, "Disables time difference in field calculations");
	
	sprintf(str, "Filter%%t|Box %%x%d|Tent %%x%d|Quad %%x%d|Cubic %%x%d|Gauss %%x%d|CatRom %%x%d|Mitch %%x%d", R_FILTER_BOX, R_FILTER_TENT, R_FILTER_QUAD, R_FILTER_CUBIC, R_FILTER_GAUSS, R_FILTER_CATROM, R_FILTER_MITCH);
	uiDefButS(block, MENU, 0,str,		565,34,60,20, &scene->r.filtertype, 0, 0, 0, 0, "Set sampling filter for antialiasing");
	uiDefButF(block, NUM,0,"",			627,34,60,20,&scene->r.gauss,0.5, 1.5, 10, 2, "Sets the filter size");
	
	uiDefButBitI(block, TOG, R_BORDER, 0, "Border",	565,13,122,20, &scene->r.mode, 0, 0, 0, 0, "Render a small cut-out of the image (Shift+B to set in the camera view)");
	uiBlockEndAlign(block);

	uiEndBlock(C, block);
#endif
}


void render_panel_anim(const bContext *C, ARegion *ar)
{
	Scene *scene= CTX_data_scene(C);
	uiBlock *block;
	uiBut *but;
	
	block= uiBeginBlock(C, ar,  "render_panel_anim", UI_EMBOSS);
	if(uiNewPanel(C, ar, block, "Anim", "Render", 640, 0, 318, 204) == 0) return;

	but= uiDefButO(block, BUT, "SCREEN_OT_render", WM_OP_INVOKE_DEFAULT, "ANIM",  692,142,192,47, "Render the animation to disk from start to end frame, (Ctrl+F12)");
	RNA_boolean_set(uiButGetOperatorPtrRNA(but), "anim", 1);
	
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, R_DOSEQ, 0, "Do Sequence",692,114,192,20, &scene->r.scemode, 0, 0, 0, 0, "Enables sequence output rendering (Default: 3D rendering)");
	uiDefButBitI(block, TOG, R_DOCOMP, 0, "Do Composite",692,90,192,20, &scene->r.scemode, 0, 0, 0, 0, "Uses compositing nodes for output rendering");
	uiBlockEndAlign(block);

	uiDefBut(block, BUT, 0, "PLAY",692,50,94,33, 0, 0, 0, 0, 0, "Play rendered images/avi animation (Ctrl+F11), (Play Hotkeys: A-Noskip, P-PingPong)");
	uiDefButS(block, NUM, 0, "rt:",789,50,95,33, &G.rt, -1000.0, 1000.0, 0, 0, "General testing/debug button");

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM,0,"Sta:",692,20,94,24, &scene->r.sfra,1.0,MAXFRAMEF, 0, 0, "The start frame of the animation (inclusive)");
	uiDefButI(block, NUM,0,"End:",789,20,95,24, &scene->r.efra,SFRA,MAXFRAMEF, 0, 0, "The end  frame of the animation  (inclusive)");
	uiDefButI(block, NUM,0,"Step:",692,0,192,18, &scene->frame_step, 1.0, MAXFRAMEF, 0, 0, "Frame Step");
	uiBlockEndAlign(block);

	uiEndBlock(C, block);
}

void buttons_scene_register(ARegionType *art)
{
	PanelType *pt;

	/* panels: Render */
	pt= MEM_callocN(sizeof(PanelType), "spacetype buttons panel");
	pt->idname= "RENDER_PT_render";
	pt->name= "Render";
	pt->context= "render";
	pt->draw= render_panel_render;
	BLI_addtail(&art->paneltypes, pt);
	
	/* panels: Shading */
	pt= MEM_callocN(sizeof(PanelType), "spacetype buttons panel");
	pt->idname= "RENDER_PT_image";
	pt->name= "Image";
	pt->context= "render";
	pt->draw= render_panel_image;
	BLI_addtail(&art->paneltypes, pt);
		
	/* panels: AntiAliasing */
	pt= MEM_callocN(sizeof(PanelType), "spacetype buttons panel");
	pt->idname= "RENDER_PT_antialias";
	pt->name= "AntiAliasing";
	pt->context= "render";
	pt->draw= render_panel_antialiasing;
	BLI_addtail(&art->paneltypes, pt);

	/* panels: Shading */
	pt= MEM_callocN(sizeof(PanelType), "spacetype buttons panel");
	pt->idname= "RENDER_PT_shading";
	pt->name= "Shading";
	pt->context= "render";
	pt->draw= render_panel_shading;
	BLI_addtail(&art->paneltypes, pt);
}

