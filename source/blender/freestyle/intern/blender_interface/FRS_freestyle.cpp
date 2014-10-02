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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/blender_interface/FRS_freestyle.cpp
 *  \ingroup freestyle
 */

#include <iostream>
#include <map>
#include <set>

#include "../application/AppCanvas.h"
#include "../application/AppConfig.h"
#include "../application/AppView.h"
#include "../application/Controller.h"

#include "BlenderStrokeRenderer.h"

using namespace std;
using namespace Freestyle;

extern "C" {

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_text_types.h"

#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_text.h"
#include "BKE_context.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_callbacks.h"

#include "BPY_extern.h"

#include "renderpipeline.h"
#include "pixelblending.h"

#include "FRS_freestyle.h"

#define DEFAULT_SPHERE_RADIUS 1.0f
#define DEFAULT_DKR_EPSILON   0.0f

// Freestyle configuration
static bool freestyle_is_initialized = false;
static Config::Path *pathconfig = NULL;
static Controller *controller = NULL;
static AppView *view = NULL;

// line set buffer for copy & paste
static FreestyleLineSet lineset_buffer;
static bool lineset_copied = false;

// camera information
float freestyle_viewpoint[3];
float freestyle_mv[4][4];
float freestyle_proj[4][4];
int freestyle_viewport[4];

// current scene
Scene *freestyle_scene;

static void load_post_callback(struct Main *main, struct ID *id, void *arg)
{
	lineset_copied = false;
}

static bCallbackFuncStore load_post_callback_funcstore = {
	NULL, NULL, /* next, prev */
	load_post_callback, /* func */
	NULL, /* arg */
	0 /* alloc */
};

//=======================================================
//   Initialization 
//=======================================================

void FRS_initialize()
{
	if (freestyle_is_initialized)
		return;

	pathconfig = new Config::Path;
	controller = new Controller();
	view = new AppView;
	controller->setView(view);
	controller->Clear();
	freestyle_scene = NULL;
	lineset_copied = false;

	BLI_callback_add(&load_post_callback_funcstore, BLI_CB_EVT_LOAD_POST);

	freestyle_is_initialized = 1;
}

void FRS_set_context(bContext *C)
{
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "FRS_set_context: context 0x" << C << " scene 0x" << CTX_data_scene(C) << endl;
	}
	controller->setContext(C);
}

void FRS_exit()
{
	delete pathconfig;
	delete controller;
	delete view;
}

//=======================================================
//   Rendering 
//=======================================================

static void init_view(Render *re)
{
	int width = re->winx;
	int height = re->winy;
	int xmin = re->disprect.xmin;
	int ymin = re->disprect.ymin;
	int xmax = re->disprect.xmax;
	int ymax = re->disprect.ymax;

	float thickness = 1.0f;
	switch (re->r.line_thickness_mode) {
	case R_LINE_THICKNESS_ABSOLUTE:
		thickness = re->r.unit_line_thickness * (re->r.size / 100.f);
		break;
	case R_LINE_THICKNESS_RELATIVE:
		thickness = height / 480.f;
		break;
	}

	freestyle_viewport[0] = freestyle_viewport[1] = 0;
	freestyle_viewport[2] = width;
	freestyle_viewport[3] = height;

	view->setWidth(width);
	view->setHeight(height);
	view->setBorder(xmin, ymin, xmax, ymax);
	view->setThickness(thickness);

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\n===  Dimensions of the 2D image coordinate system  ===" << endl;
		cout << "Width  : " << width << endl;
		cout << "Height : " << height << endl;
		if (re->r.mode & R_BORDER)
			cout << "Border : (" << xmin << ", " << ymin << ") - (" << xmax << ", " << ymax << ")" << endl;
		cout << "Unit line thickness : " << thickness << " pixel(s)" << endl;
	}
}

static void init_camera(Render *re)
{
	// It is assumed that imported meshes are in the camera coordinate system.
	// Therefore, the view point (i.e., camera position) is at the origin, and
	// the the model-view matrix is simply the identity matrix.

	freestyle_viewpoint[0] = 0.0;
	freestyle_viewpoint[1] = 0.0;
	freestyle_viewpoint[2] = 0.0;

	unit_m4(freestyle_mv);

	copy_m4_m4(freestyle_proj, re->winmat);

#if 0
	print_m4("mv", freestyle_mv);
	print_m4("proj", freestyle_proj);
#endif
}

static char *escape_quotes(char *name)
{
	char *s = (char *)MEM_mallocN(strlen(name) * 2 + 1, "escape_quotes");
	char *p = s;
	while (*name) {
		if (*name == '\'')
			*(p++) = '\\';
		*(p++) = *(name++);
	}
	*p = '\0';
	return s;
}

static Text *create_lineset_handler(Main *bmain, char *layer_name, char *lineset_name)
{
	char *s1 = escape_quotes(layer_name);
	char *s2 = escape_quotes(lineset_name);
	Text *text = BKE_text_add(bmain, lineset_name);
	BKE_text_write(text, "import parameter_editor; parameter_editor.process('");
	BKE_text_write(text, s1);
	BKE_text_write(text, "', '");
	BKE_text_write(text, s2);
	BKE_text_write(text, "')\n");
	MEM_freeN(s1);
	MEM_freeN(s2);
	return text;
}

struct edge_type_condition
{
	int edge_type, value;
};

// examines the conditions and returns true if the target edge type needs to be computed
static bool test_edge_type_conditions(struct edge_type_condition *conditions,
                                      int num_edge_types, bool logical_and, int target, bool distinct)
{
	int target_condition = 0;
	int num_non_target_positive_conditions = 0;
	int num_non_target_negative_conditions = 0;

	for (int i = 0; i < num_edge_types; i++) {
		if (conditions[i].edge_type == target)
			target_condition = conditions[i].value;
		else if (conditions[i].value > 0)
			++num_non_target_positive_conditions;
		else if (conditions[i].value < 0)
			++num_non_target_negative_conditions;
	}
	if (distinct) {
		// In this case, the 'target' edge type is assumed to appear on distinct edge
		// of its own and never together with other edge types.
		if (logical_and) {
			if (num_non_target_positive_conditions > 0)
				return false;
			if (target_condition > 0)
				return true;
			if (target_condition < 0)
				return false;
			if (num_non_target_negative_conditions > 0)
				return true;
		}
		else {
			if (target_condition > 0)
				return true;
			if (num_non_target_negative_conditions > 0)
				return true;
			if (target_condition < 0)
				return false;
			if (num_non_target_positive_conditions > 0)
				return false;
		}
	}
	else {
		// In this case, the 'target' edge type may appear together with other edge types.
		if (target_condition > 0)
			return true;
		if (target_condition < 0)
			return true;
		if (logical_and) {
			if (num_non_target_positive_conditions > 0)
				return false;
			if (num_non_target_negative_conditions > 0)
				return true;
		}
		else {
			if (num_non_target_negative_conditions > 0)
				return true;
			if (num_non_target_positive_conditions > 0)
				return false;
		}
	}
	return true;
}

static void prepare(Main *bmain, Render *re, SceneRenderLayer *srl)
{
	// load mesh
	re->i.infostr = "Freestyle: Mesh loading";
	re->stats_draw(re->sdh, &re->i);
	re->i.infostr = NULL;
	if (controller->LoadMesh(re, srl)) // returns if scene cannot be loaded or if empty
		return;
	if (re->test_break(re->tbh))
		return;

	// add style modules
	FreestyleConfig *config = &srl->freestyleConfig;

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\n===  Rendering options  ===" << endl;
	}
	int layer_count = 0;

	switch (config->mode) {
	case FREESTYLE_CONTROL_SCRIPT_MODE:
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Modules :" << endl;
		}
		for (FreestyleModuleConfig *module_conf = (FreestyleModuleConfig *)config->modules.first;
		     module_conf;
		     module_conf = module_conf->next)
		{
			if (module_conf->script && module_conf->is_displayed) {
				const char *id_name = module_conf->script->id.name + 2;
				if (G.debug & G_DEBUG_FREESTYLE) {
					cout << "  " << layer_count + 1 << ": " << id_name;
					if (module_conf->script->name)
						cout << " (" << module_conf->script->name << ")";
					cout << endl;
				}
				controller->InsertStyleModule(layer_count, id_name, module_conf->script);
				controller->toggleLayer(layer_count, true);
				layer_count++;
			}
		}
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << endl;
		}
		controller->setComputeRidgesAndValleysFlag((config->flags & FREESTYLE_RIDGES_AND_VALLEYS_FLAG) ? true : false);
		controller->setComputeSuggestiveContoursFlag((config->flags & FREESTYLE_SUGGESTIVE_CONTOURS_FLAG) ? true : false);
		controller->setComputeMaterialBoundariesFlag((config->flags & FREESTYLE_MATERIAL_BOUNDARIES_FLAG) ? true : false);
		break;
	case FREESTYLE_CONTROL_EDITOR_MODE:
		int use_ridges_and_valleys = 0;
		int use_suggestive_contours = 0;
		int use_material_boundaries = 0;
		struct edge_type_condition conditions[] = {
			{FREESTYLE_FE_SILHOUETTE, 0},
			{FREESTYLE_FE_BORDER, 0},
			{FREESTYLE_FE_CREASE, 0},
			{FREESTYLE_FE_RIDGE_VALLEY, 0},
			{FREESTYLE_FE_SUGGESTIVE_CONTOUR, 0},
			{FREESTYLE_FE_MATERIAL_BOUNDARY, 0},
			{FREESTYLE_FE_CONTOUR, 0},
			{FREESTYLE_FE_EXTERNAL_CONTOUR, 0},
			{FREESTYLE_FE_EDGE_MARK, 0}
		};
		int num_edge_types = sizeof(conditions) / sizeof(struct edge_type_condition);
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Linesets:" << endl;
		}
		for (FreestyleLineSet *lineset = (FreestyleLineSet *)config->linesets.first;
		     lineset;
		     lineset = lineset->next)
		{
			if (lineset->flags & FREESTYLE_LINESET_ENABLED) {
				if (G.debug & G_DEBUG_FREESTYLE) {
					cout << "  " << layer_count+1 << ": " << lineset->name << " - " <<
					        (lineset->linestyle ? (lineset->linestyle->id.name + 2) : "<NULL>") << endl;
				}
				Text *text = create_lineset_handler(bmain, srl->name, lineset->name);
				controller->InsertStyleModule(layer_count, lineset->name, text);
				controller->toggleLayer(layer_count, true);
				if (!(lineset->selection & FREESTYLE_SEL_EDGE_TYPES) || !lineset->edge_types) {
					++use_ridges_and_valleys;
					++use_suggestive_contours;
					++use_material_boundaries;
				}
				else {
					// conditions for feature edge selection by edge types
					for (int i = 0; i < num_edge_types; i++) {
						if (!(lineset->edge_types & conditions[i].edge_type))
							conditions[i].value = 0; // no condition specified
						else if (!(lineset->exclude_edge_types & conditions[i].edge_type))
							conditions[i].value = 1; // condition: X
						else
							conditions[i].value = -1; // condition: NOT X
					}
					// logical operator for the selection conditions
					bool logical_and = ((lineset->flags & FREESTYLE_LINESET_FE_AND) != 0);
					// negation operator
					if (lineset->flags & FREESTYLE_LINESET_FE_NOT) {
						// convert an Exclusive condition into an Inclusive equivalent using De Morgan's laws:
						//   NOT (X OR Y) --> (NOT X) AND (NOT Y)
						//   NOT (X AND Y) --> (NOT X) OR (NOT Y)
						for (int i = 0; i < num_edge_types; i++)
							conditions[i].value *= -1;
						logical_and = !logical_and;
					}
					if (test_edge_type_conditions(conditions, num_edge_types, logical_and,
					                              FREESTYLE_FE_RIDGE_VALLEY, true))
					{
						++use_ridges_and_valleys;
					}
					if (test_edge_type_conditions(conditions, num_edge_types, logical_and,
					                              FREESTYLE_FE_SUGGESTIVE_CONTOUR, true))
					{
						++use_suggestive_contours;
					}
					if (test_edge_type_conditions(conditions, num_edge_types, logical_and,
					                              FREESTYLE_FE_MATERIAL_BOUNDARY, true))
					{
						++use_material_boundaries;
					}
				}
				layer_count++;
			}
		}
		controller->setComputeRidgesAndValleysFlag(use_ridges_and_valleys > 0);
		controller->setComputeSuggestiveContoursFlag(use_suggestive_contours > 0);
		controller->setComputeMaterialBoundariesFlag(use_material_boundaries > 0);
		break;
	}

	// set parameters
	if (config->flags & FREESTYLE_ADVANCED_OPTIONS_FLAG) {
		controller->setSphereRadius(config->sphere_radius);
		controller->setSuggestiveContourKrDerivativeEpsilon(config->dkr_epsilon);
	}
	else {
		controller->setSphereRadius(DEFAULT_SPHERE_RADIUS);
		controller->setSuggestiveContourKrDerivativeEpsilon(DEFAULT_DKR_EPSILON);
	}
	controller->setFaceSmoothness((config->flags & FREESTYLE_FACE_SMOOTHNESS_FLAG) ? true : false);
	controller->setCreaseAngle(RAD2DEGF(config->crease_angle));
	controller->setVisibilityAlgo((config->flags & FREESTYLE_CULLING) ?
	                              FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE :
	                              FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE);

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Crease angle : " << controller->getCreaseAngle() << endl;
		cout << "Sphere radius : " << controller->getSphereRadius() << endl;
		cout << "Face smoothness : " << (controller->getFaceSmoothness() ? "enabled" : "disabled") << endl;
		cout << "Redges and valleys : " << (controller->getComputeRidgesAndValleysFlag() ? "enabled" : "disabled") <<
		        endl;
		cout << "Suggestive contours : " <<
		        (controller->getComputeSuggestiveContoursFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contour Kr derivative epsilon : " <<
		        controller->getSuggestiveContourKrDerivativeEpsilon() << endl;
		cout << "Material boundaries : " <<
		        (controller->getComputeMaterialBoundariesFlag() ? "enabled" : "disabled") << endl;
		cout << endl;
	}

	// set diffuse and z depth passes
	RenderLayer *rl = RE_GetRenderLayer(re->result, srl->name);
	bool diffuse = false, z = false;
	for (RenderPass *rpass = (RenderPass *)rl->passes.first; rpass; rpass = rpass->next) {
		switch (rpass->passtype) {
		case SCE_PASS_DIFFUSE:
			controller->setPassDiffuse(rpass->rect, rpass->rectx, rpass->recty);
			diffuse = true;
			break;
		case SCE_PASS_Z:
			controller->setPassZ(rpass->rect, rpass->rectx, rpass->recty);
			z = true;
			break;
		}
	}
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Passes :" << endl;
		cout << "  Diffuse = " << (diffuse ? "enabled" : "disabled") << endl;
		cout << "  Z = " << (z ? "enabled" : "disabled") << endl;
	}

	if (controller->hitViewMapCache())
		return;

	// compute view map
	re->i.infostr = "Freestyle: View map creation";
	re->stats_draw(re->sdh, &re->i);
	re->i.infostr = NULL;
	controller->ComputeViewMap();
}

void FRS_composite_result(Render *re, SceneRenderLayer *srl, Render *freestyle_render)
{
	RenderLayer *rl;
	float *src, *dest, *pixSrc, *pixDest;
	int x, y, rectx, recty;

	if (freestyle_render == NULL || freestyle_render->result == NULL)
		return;

	rl = render_get_active_layer( freestyle_render, freestyle_render->result );
	if (!rl || rl->rectf == NULL) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Cannot find Freestyle result image" << endl;
		}
		return;
	}
	src  = rl->rectf;
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "src: " << rl->rectx << " x " << rl->recty << endl;
	}
#endif

	rl = RE_GetRenderLayer(re->result, srl->name);
	if (!rl || rl->rectf == NULL) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "No layer to composite to" << endl;
		}
		return;
	}
	dest = rl->rectf;
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "dest: " << rl->rectx << " x " << rl->recty << endl;
	}
#endif

	rectx = re->rectx;
	recty = re->recty;
	for (y = 0; y < recty; y++) {
		for (x = 0; x < rectx; x++) {
			pixSrc = src + 4 * (rectx * y + x);
			if (pixSrc[3] > 0.0) {
				pixDest = dest + 4 * (rectx * y + x);
				addAlphaOverFloat(pixDest, pixSrc);
			}
		}
	}
}

static int displayed_layer_count(SceneRenderLayer *srl)
{
	int count = 0;

	switch (srl->freestyleConfig.mode) {
	case FREESTYLE_CONTROL_SCRIPT_MODE:
		for (FreestyleModuleConfig *module = (FreestyleModuleConfig *)srl->freestyleConfig.modules.first;
		     module;
		     module = module->next)
		{
			if (module->script && module->is_displayed)
				count++;
		}
		break;
	case FREESTYLE_CONTROL_EDITOR_MODE:
		for (FreestyleLineSet *lineset = (FreestyleLineSet *)srl->freestyleConfig.linesets.first;
		     lineset;
		     lineset = lineset->next)
		{
			if (lineset->flags & FREESTYLE_LINESET_ENABLED)
				count++;
		}
		break;
	}
	return count;
}

int FRS_is_freestyle_enabled(SceneRenderLayer *srl)
{
	return (!(srl->layflag & SCE_LAY_DISABLE) && srl->layflag & SCE_LAY_FRS && displayed_layer_count(srl) > 0);
}

void FRS_init_stroke_rendering(Render *re)
{
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << endl;
		cout << "#===============================================================" << endl;
		cout << "#  Freestyle" << endl;
		cout << "#===============================================================" << endl;
	}

	init_view(re);
	init_camera(re);

	controller->ResetRenderCount();
}

Render *FRS_do_stroke_rendering(Render *re, SceneRenderLayer *srl, int render)
{
	Main *freestyle_bmain = re->freestyle_bmain;
	Render *freestyle_render = NULL;
	Text *text, *next_text;

	if (!render)
		return controller->RenderStrokes(re, false);

	RenderMonitor monitor(re);
	controller->setRenderMonitor(&monitor);
	controller->setViewMapCache((srl->freestyleConfig.flags & FREESTYLE_VIEW_MAP_CACHE) ? true : false);

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << endl;
		cout << "----------------------------------------------------------" << endl;
		cout << "|  " << (re->scene->id.name + 2) << "|" << srl->name << endl;
		cout << "----------------------------------------------------------" << endl;
	}

	// prepare Freestyle:
	//   - load mesh
	//   - add style modules
	//   - set parameters
	//   - compute view map
	prepare(freestyle_bmain, re, srl);

	if (re->test_break(re->tbh)) {
		controller->CloseFile();
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Break" << endl;
		}
	}
	else {
		// render and composite Freestyle result
		if (controller->_ViewMap) {
			// render strokes
			re->i.infostr = "Freestyle: Stroke rendering";
			re->stats_draw(re->sdh, &re->i);
			re->i.infostr = NULL;
			freestyle_scene = re->scene;
			controller->DrawStrokes();
			freestyle_render = controller->RenderStrokes(re, true);
			controller->CloseFile();
			freestyle_scene = NULL;

			// composite result
			FRS_composite_result(re, srl, freestyle_render);
			RE_FreeRenderResult(freestyle_render->result);
			freestyle_render->result = NULL;
		}
	}

	// Free temp main (currently only text blocks are stored there)
	for (text = (Text *)freestyle_bmain->text.first; text; text = next_text) {
		next_text = (Text *) text->id.next;

		BKE_text_unlink(freestyle_bmain, text);
		BKE_libblock_free(freestyle_bmain, text);
	}

	return freestyle_render;
}

void FRS_finish_stroke_rendering(Render *re)
{
	// clear canvas
	controller->Clear();
}

void FRS_free_view_map_cache(void)
{
	// free cache
	controller->DeleteViewMap(true);
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("View map cache freed\n");
	}
#endif
}

//=======================================================
//   Freestyle Panel Configuration
//=======================================================

void FRS_copy_active_lineset(FreestyleConfig *config)
{
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);

	if (lineset) {
		lineset_buffer.linestyle = lineset->linestyle;
		lineset_buffer.flags = lineset->flags;
		lineset_buffer.selection = lineset->selection;
		lineset_buffer.qi = lineset->qi;
		lineset_buffer.qi_start = lineset->qi_start;
		lineset_buffer.qi_end = lineset->qi_end;
		lineset_buffer.edge_types = lineset->edge_types;
		lineset_buffer.exclude_edge_types = lineset->exclude_edge_types;
		lineset_buffer.group = lineset->group;
		strcpy(lineset_buffer.name, lineset->name);
		lineset_copied = true;
	}
}

void FRS_paste_active_lineset(FreestyleConfig *config)
{
	if (!lineset_copied)
		return;

	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);

	if (lineset) {
		if (lineset->linestyle)
			lineset->linestyle->id.us--;
		lineset->linestyle = lineset_buffer.linestyle;
		if (lineset->linestyle)
			lineset->linestyle->id.us++;
		lineset->flags = lineset_buffer.flags;
		lineset->selection = lineset_buffer.selection;
		lineset->qi = lineset_buffer.qi;
		lineset->qi_start = lineset_buffer.qi_start;
		lineset->qi_end = lineset_buffer.qi_end;
		lineset->edge_types = lineset_buffer.edge_types;
		lineset->exclude_edge_types = lineset_buffer.exclude_edge_types;
		if (lineset->group) {
			lineset->group->id.us--;
			lineset->group = NULL;
		}
		if (lineset_buffer.group) {
			lineset->group = lineset_buffer.group;
			lineset->group->id.us++;
		}
		strcpy(lineset->name, lineset_buffer.name);
		BKE_freestyle_lineset_unique_name(config, lineset);
		lineset->flags |= FREESTYLE_LINESET_CURRENT;
	}
}

void FRS_delete_active_lineset(FreestyleConfig *config)
{
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);

	if (lineset) {
		BKE_freestyle_lineset_delete(config, lineset);
	}
}

void FRS_move_active_lineset_up(FreestyleConfig *config)
{
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);

	if (lineset) {
		BLI_remlink(&config->linesets, lineset);
		BLI_insertlinkbefore(&config->linesets, lineset->prev, lineset);
	}
}

void FRS_move_active_lineset_down(FreestyleConfig *config)
{
	FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);

	if (lineset) {
		BLI_remlink(&config->linesets, lineset);
		BLI_insertlinkafter(&config->linesets, lineset->next, lineset);
	}
}

// Testing

Material *FRS_create_stroke_material(Main *bmain, struct FreestyleLineStyle *linestyle)
{
	bNodeTree *nt = (linestyle->use_nodes) ? linestyle->nodetree : NULL;
	Material *ma = BlenderStrokeRenderer::GetStrokeShader(bmain, nt, true);
	ma->id.us = 0;
	return ma;
}

} // extern "C"
