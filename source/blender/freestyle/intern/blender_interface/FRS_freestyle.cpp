#include "../application/Controller.h"
#include "../application/AppView.h"
#include "../application/AppConfig.h"
#include "../application/AppCanvas.h"

#include <iostream>
#include <map>
#include <set>
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_text_types.h"
#include "DNA_group_types.h"
#include "DNA_freestyle_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_text.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BPY_extern.h"

#include "renderpipeline.h"
#include "pixelblending.h"

#include "../../FRS_freestyle.h"
#include "../../FRS_freestyle_config.h"

#define DEFAULT_SPHERE_RADIUS  1.0f
#define DEFAULT_DKR_EPSILON    0.0f

	// Freestyle configuration
	static short freestyle_is_initialized = 0;
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

	string default_module_path;

	//=======================================================
	//   Initialization 
	//=======================================================

	void FRS_initialize() {
		
		if( freestyle_is_initialized )
			return;

		pathconfig = new Config::Path;
		controller = new Controller();
		view = new AppView;
		controller->setView(view);
		controller->Clear();
		freestyle_scene = NULL;
		lineset_copied = false;
			
		default_module_path = pathconfig->getProjectDir() + Config::DIR_SEP + "style_modules" + Config::DIR_SEP + "contour.py";
			
		freestyle_is_initialized = 1;
	}
	
	void FRS_set_context(bContext* C) {
		cout << "FRS_set_context: context 0x" << C << " scene 0x" << CTX_data_scene(C) << endl;
		controller->setContext(C);
	}

	void FRS_read_file(bContext* C) {
		lineset_copied = false;
	}

	void FRS_exit() {
		delete pathconfig;
		delete controller;
		delete view;
	}

	//=======================================================
	//   Rendering 
	//=======================================================

	static void init_view(Render* re){
		int width = re->winx;
		int height = re->winy;
		int xmin = re->disprect.xmin;
		int ymin = re->disprect.ymin;
		int xmax = re->disprect.xmax;
		int ymax = re->disprect.ymax;
		
		float thickness = 1.f;
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
		
		view->setWidth( width );
		view->setHeight( height );
		view->setBorder( xmin, ymin, xmax, ymax );
		view->setThickness( thickness );

		cout << "\n===  Dimensions of the 2D image coordinate system  ===" << endl;
		cout << "Width  : " << width << endl;
		cout << "Height : " << height << endl;
		if (re->r.mode & R_BORDER)
			cout << "Border : (" << xmin << ", " << ymin << ") - (" << xmax << ", " << ymax << ")" << endl;
		cout << "Unit line thickness : " << thickness << " pixel(s)" << endl;
	}

	static void init_camera(Render* re){
		// It is assumed that imported meshes are in the camera coordinate system.
		// Therefore, the view point (i.e., camera position) is at the origin, and
		// the the model-view matrix is simply the identity matrix.

		freestyle_viewpoint[0] = 0.0;
		freestyle_viewpoint[1] = 0.0;
		freestyle_viewpoint[2] = 0.0;
		
		for( int i = 0; i < 4; i++ )
		   for( int j = 0; j < 4; j++ )
			freestyle_mv[i][j] = (i == j) ? 1.0 : 0.0;
		
		for( int i = 0; i < 4; i++ )
		   for( int j = 0; j < 4; j++ )
			freestyle_proj[i][j] = re->winmat[i][j];

		//print_m4("mv", freestyle_mv);
		//print_m4("proj", freestyle_proj);
	}

	static char *escape_quotes(char *name)
	{
		char *s= (char *)MEM_mallocN(strlen(name) * 2 + 1, "escape_quotes");
		char *p= s;
		while (*name) {
			if (*name == '\'')
				*p++= '\\';
			*p++ = *name++;
		}
		*p = '\0';
		return s;
	}

	static Text *create_lineset_handler(char *layer_name, char *lineset_name)
	{
		char *s1= escape_quotes(layer_name);
		char *s2= escape_quotes(lineset_name);
		Text *text= BKE_text_add(lineset_name);
		BKE_text_write(text, "import parameter_editor; parameter_editor.process('");
		BKE_text_write(text, s1);
		BKE_text_write(text, "', '");
		BKE_text_write(text, s2);
		BKE_text_write(text, "')\n");
		MEM_freeN(s1);
		MEM_freeN(s2);
		return text;
	}

	struct edge_type_condition {
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
			} else {
				if (target_condition > 0)
					return true;
				if (num_non_target_negative_conditions > 0)
					return true;
				if (target_condition < 0)
					return false;
				if (num_non_target_positive_conditions > 0)
					return false;
			}
		} else {
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
			} else {
				if (num_non_target_negative_conditions > 0)
					return true;
				if (num_non_target_positive_conditions > 0)
					return false;
			}
		}
		return true;
	}

	static void prepare(Render* re, SceneRenderLayer* srl ) {
				
		// load mesh
        re->i.infostr= "Freestyle: Mesh loading";
		re->stats_draw(re->sdh, &re->i);
        re->i.infostr= NULL;
		if( controller->LoadMesh(re, srl) ) // returns if scene cannot be loaded or if empty
			return;
        if( re->test_break(re->tbh) )
            return;
		
		// add style modules
		FreestyleConfig* config = &srl->freestyleConfig;
		
		cout << "\n===  Rendering options  ===" << endl;
		int layer_count = 0;
		
		switch (config->mode) {
		case FREESTYLE_CONTROL_SCRIPT_MODE:
			cout << "Modules :"<< endl;
			for (FreestyleModuleConfig* module_conf = (FreestyleModuleConfig *)config->modules.first; module_conf; module_conf = module_conf->next) {
				if( module_conf->is_displayed ) {
					cout << "  " << layer_count+1 << ": " << module_conf->module_path << endl;
					controller->InsertStyleModule( layer_count, module_conf->module_path );
					controller->toggleLayer(layer_count, true);
					layer_count++;
				}
			}
			cout << endl;
			controller->setComputeRidgesAndValleysFlag( (config->flags & FREESTYLE_RIDGES_AND_VALLEYS_FLAG) ? true : false);
			controller->setComputeSuggestiveContoursFlag( (config->flags & FREESTYLE_SUGGESTIVE_CONTOURS_FLAG) ? true : false);
			controller->setComputeMaterialBoundariesFlag( (config->flags & FREESTYLE_MATERIAL_BOUNDARIES_FLAG) ? true : false);
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
				{FREESTYLE_FE_EDGE_MARK, 0}};
			int num_edge_types = sizeof(conditions) / sizeof(struct edge_type_condition);
			cout << "Linesets:"<< endl;
			for (FreestyleLineSet *lineset = (FreestyleLineSet *)config->linesets.first; lineset; lineset = lineset->next) {
				if (lineset->flags & FREESTYLE_LINESET_ENABLED) {
					cout << "  " << layer_count+1 << ": " << lineset->name << " - " << lineset->linestyle->id.name+2 << endl;
					Text *text = create_lineset_handler(srl->name, lineset->name);
					controller->InsertStyleModule( layer_count, lineset->name, text );
					controller->toggleLayer(layer_count, true);
					if (!(lineset->selection & FREESTYLE_SEL_EDGE_TYPES) || !lineset->edge_types) {
						++use_ridges_and_valleys;
						++use_suggestive_contours;
						++use_material_boundaries;
					} else {
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
						if (test_edge_type_conditions(conditions, num_edge_types, logical_and, FREESTYLE_FE_RIDGE_VALLEY, true))
							++use_ridges_and_valleys;
						if (test_edge_type_conditions(conditions, num_edge_types, logical_and, FREESTYLE_FE_SUGGESTIVE_CONTOUR, true))
							++use_suggestive_contours;
						if (test_edge_type_conditions(conditions, num_edge_types, logical_and, FREESTYLE_FE_MATERIAL_BOUNDARY, true))
							++use_material_boundaries;
					}
					layer_count++;
				}
			}
			controller->setComputeRidgesAndValleysFlag( use_ridges_and_valleys > 0 );
			controller->setComputeSuggestiveContoursFlag( use_suggestive_contours > 0 );
			controller->setComputeMaterialBoundariesFlag( use_material_boundaries > 0 );
			break;
		}
		
		// set parameters
		if (config->flags & FREESTYLE_ADVANCED_OPTIONS_FLAG) {
			controller->setSphereRadius( config->sphere_radius );
			controller->setSuggestiveContourKrDerivativeEpsilon( config->dkr_epsilon );
		} else {
			controller->setSphereRadius( DEFAULT_SPHERE_RADIUS );
			controller->setSuggestiveContourKrDerivativeEpsilon( DEFAULT_DKR_EPSILON );
		}
		controller->setFaceSmoothness( (config->flags & FREESTYLE_FACE_SMOOTHNESS_FLAG) ? true : false);
		controller->setCreaseAngle( config->crease_angle );
		controller->setVisibilityAlgo( (config->flags & FREESTYLE_CULLING) ?
			FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE : FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE );

		cout << "Crease angle : " << controller->getCreaseAngle() << endl;
		cout << "Sphere radius : " << controller->getSphereRadius() << endl;
		cout << "Face smoothness : " << (controller->getFaceSmoothness() ? "enabled" : "disabled") << endl;
		cout << "Redges and valleys : " << (controller->getComputeRidgesAndValleysFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contours : " << (controller->getComputeSuggestiveContoursFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contour Kr derivative epsilon : " << controller->getSuggestiveContourKrDerivativeEpsilon() << endl;
		cout << "Material boundaries : " << (controller->getComputeMaterialBoundariesFlag() ? "enabled" : "disabled") << endl;
		cout << endl;

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
        cout << "Passes :" << endl;
        cout << "  Diffuse = " << (diffuse ? "enabled" : "disabled") << endl;
        cout << "  Z = " << (z ? "enabled" : "disabled") << endl;

		// compute view map
        re->i.infostr= "Freestyle: View map creation";
		re->stats_draw(re->sdh, &re->i);
        re->i.infostr= NULL;
		controller->ComputeViewMap();
	}
	
	void FRS_composite_result(Render* re, SceneRenderLayer* srl, Render* freestyle_render)
	{
		RenderLayer *rl;
	    float *src, *dest, *pixSrc, *pixDest;
		int x, y, rectx, recty;
		
		if( freestyle_render == NULL || freestyle_render->result == NULL )
			return;

		rl = render_get_active_layer( freestyle_render, freestyle_render->result );
	    if( !rl || rl->rectf == NULL) { cout << "Cannot find Freestyle result image" << endl; return; }
		src  = rl->rectf;
		//cout << "src: " << rl->rectx << " x " << rl->recty << endl;
		
		rl = RE_GetRenderLayer(re->result, srl->name);
	    if( !rl || rl->rectf == NULL) { cout << "No layer to composite to" << endl; return; }
		dest  = rl->rectf;
		//cout << "dest: " << rl->rectx << " x " << rl->recty << endl;

		rectx = re->rectx;
		recty = re->recty;
		for( y = 0; y < recty; y++) {
	        for( x = 0; x < rectx; x++) {
	            pixSrc = src + 4 * (rectx * y + x);
	            if( pixSrc[3] > 0.0) {
					pixDest = dest + 4 * (rectx * y + x);
					addAlphaOverFloat(pixDest, pixSrc);
				}
			}
	    }
	}
	
	static int displayed_layer_count( SceneRenderLayer* srl ) {
		int count = 0;

		switch (srl->freestyleConfig.mode) {
		case FREESTYLE_CONTROL_SCRIPT_MODE:
			for (FreestyleModuleConfig* module = (FreestyleModuleConfig *)srl->freestyleConfig.modules.first; module; module = module->next) {
				if( module->is_displayed )
					count++;
			}
			break;
		case FREESTYLE_CONTROL_EDITOR_MODE:
			for (FreestyleLineSet *lineset = (FreestyleLineSet *)srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
				if (lineset->flags & FREESTYLE_LINESET_ENABLED)
					count++;
			}
			break;
		}
		return count;
	}

	int FRS_is_freestyle_enabled(SceneRenderLayer* srl) {
		return (!(srl->layflag & SCE_LAY_DISABLE) &&
			 	srl->layflag & SCE_LAY_FRS &&
				displayed_layer_count(srl) > 0);
	}
	
	void FRS_init_stroke_rendering(Render* re) {

		cout << "\n#===============================================================" << endl;
		cout << "#  Freestyle" << endl;
		cout << "#===============================================================" << endl;
		
		init_view(re);
		init_camera(re);

		controller->ResetRenderCount();
	}
	
	Render* FRS_do_stroke_rendering(Render* re, SceneRenderLayer *srl) {
		
		Render* freestyle_render = NULL;

		RenderMonitor monitor(re);
		controller->setRenderMonitor(&monitor);
		
		cout << "\n----------------------------------------------------------" << endl;
		cout << "|  " << (re->scene->id.name+2) << "|" << srl->name << endl;
		cout << "----------------------------------------------------------" << endl;
		
		// prepare Freestyle:
		//   - load mesh
		//   - add style modules
		//   - set parameters
		//   - compute view map
		prepare(re, srl);

        if( re->test_break(re->tbh) ) {
			controller->CloseFile();
			cout << "Break" << endl;
            return NULL;
        }

		// render and composite Freestyle result
		if( controller->_ViewMap ) {
			
			// render strokes					
            re->i.infostr= "Freestyle: Stroke rendering";
            re->stats_draw(re->sdh, &re->i);
        	re->i.infostr= NULL;
			freestyle_scene = re->scene;
			controller->DrawStrokes();
			freestyle_render = controller->RenderStrokes(re);
			controller->CloseFile();
			freestyle_scene = NULL;
			
			// composite result
			FRS_composite_result(re, srl, freestyle_render);
			RE_FreeRenderResult(freestyle_render->result);
			freestyle_render->result = NULL;
		}

		return freestyle_render;
	}

	void FRS_finish_stroke_rendering(Render* re) {
		// clear canvas
		controller->Clear();
	}

	//=======================================================
	//   Freestyle Panel Configuration
	//=======================================================

	void FRS_add_freestyle_config( SceneRenderLayer* srl )
	{		
		FreestyleConfig* config = &srl->freestyleConfig;
		
		config->mode = FREESTYLE_CONTROL_SCRIPT_MODE;

		config->modules.first = config->modules.last = NULL;
		config->flags = 0;
		config->sphere_radius = DEFAULT_SPHERE_RADIUS;
		config->dkr_epsilon = DEFAULT_DKR_EPSILON;
		config->crease_angle = 134.43f;

		config->linesets.first = config->linesets.last = NULL;
	}
	
	void FRS_free_freestyle_config( SceneRenderLayer* srl )
	{		
		FreestyleLineSet *lineset;

		for(lineset=(FreestyleLineSet *)srl->freestyleConfig.linesets.first; lineset; lineset=lineset->next) {
			if (lineset->group) {
				lineset->group->id.us--;
				lineset->group = NULL;
			}
			lineset->linestyle->id.us--;
			lineset->linestyle = NULL;
		}
		BLI_freelistN( &srl->freestyleConfig.linesets );
		BLI_freelistN( &srl->freestyleConfig.modules );
	}

	void FRS_add_module(FreestyleConfig *config)
	{
		FreestyleModuleConfig* module_conf = (FreestyleModuleConfig*) MEM_callocN( sizeof(FreestyleModuleConfig), "style module configuration");
		BLI_addtail(&config->modules, (void*) module_conf);
		
		strcpy( module_conf->module_path, default_module_path.c_str() );
		module_conf->is_displayed = 1;	
	}
	
	void FRS_delete_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
	{
		BLI_freelinkN(&config->modules, module_conf);
	}
	
	void FRS_move_module_up(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
	{
		BLI_remlink(&config->modules, module_conf);
		BLI_insertlinkbefore(&config->modules, module_conf->prev, module_conf);
	}
	
	void FRS_move_module_down(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
	{			
		BLI_remlink(&config->modules, module_conf);
		BLI_insertlinkafter(&config->modules, module_conf->next, module_conf);
	}

	static void unique_lineset_name(FreestyleConfig *config, FreestyleLineSet *lineset)
	{
		BLI_uniquename(&config->linesets, lineset, "FreestyleLineSet", '.', offsetof(FreestyleLineSet, name), sizeof(lineset->name));
	}

	FreestyleLineSet *FRS_add_lineset(FreestyleConfig *config)
	{
		int lineset_index = BLI_countlist(&config->linesets);

		FreestyleLineSet *lineset = (FreestyleLineSet *) MEM_callocN( sizeof(FreestyleLineSet), "Freestyle line set");
		BLI_addtail(&config->linesets, (void *) lineset);
		FRS_set_active_lineset_index(config, lineset_index);

		lineset->linestyle = FRS_new_linestyle("LineStyle", NULL);
		lineset->flags |= FREESTYLE_LINESET_ENABLED;
		lineset->selection = FREESTYLE_SEL_IMAGE_BORDER;
		lineset->qi = FREESTYLE_QI_VISIBLE;
		lineset->qi_start = 0;
		lineset->qi_end = 100;
		lineset->edge_types = FREESTYLE_FE_SILHOUETTE | FREESTYLE_FE_BORDER | FREESTYLE_FE_CREASE;
		lineset->exclude_edge_types = 0;
		lineset->group = NULL;
		if (lineset_index > 0)
			sprintf(lineset->name, "LineSet %i", lineset_index+1);
		else
			strcpy(lineset->name, "LineSet");
		unique_lineset_name(config, lineset);

		return lineset;
	}

	void FRS_copy_active_lineset(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset = FRS_get_active_lineset(config);

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

		FreestyleLineSet *lineset = FRS_get_active_lineset(config);

		if (lineset) {
			lineset->linestyle->id.us--;
			lineset->linestyle = lineset_buffer.linestyle;
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
			unique_lineset_name(config, lineset);
			lineset->flags |= FREESTYLE_LINESET_CURRENT;
		}
	}

	void FRS_delete_active_lineset(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset = FRS_get_active_lineset(config);

		if (lineset) {
			if (lineset->group) {
				lineset->group->id.us--;
				lineset->group = NULL;
			}
			lineset->linestyle->id.us--;
			lineset->linestyle = NULL;
			BLI_remlink(&config->linesets, lineset);
			MEM_freeN(lineset);
			FRS_set_active_lineset_index(config, 0);
		}
	}

	void FRS_move_active_lineset_up(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset = FRS_get_active_lineset(config);

		if (lineset) {
			BLI_remlink(&config->linesets, lineset);
			BLI_insertlinkbefore(&config->linesets, lineset->prev, lineset);
		}
	}

	void FRS_move_active_lineset_down(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset = FRS_get_active_lineset(config);

		if (lineset) {
			BLI_remlink(&config->linesets, lineset);
			BLI_insertlinkafter(&config->linesets, lineset->next, lineset);
		}
	}

	FreestyleLineSet *FRS_get_active_lineset(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset;

		for(lineset=(FreestyleLineSet *)config->linesets.first; lineset; lineset=lineset->next)
			if(lineset->flags & FREESTYLE_LINESET_CURRENT)
				return lineset;
		return NULL;
	}

	short FRS_get_active_lineset_index(FreestyleConfig *config)
	{
		FreestyleLineSet *lineset;
		short i;

		for(lineset=(FreestyleLineSet *)config->linesets.first, i=0; lineset; lineset=lineset->next, i++)
			if(lineset->flags & FREESTYLE_LINESET_CURRENT)
				return i;
		return 0;
	}

	void FRS_set_active_lineset_index(FreestyleConfig *config, short index)
	{
		FreestyleLineSet *lineset;
		short i;

		for(lineset=(FreestyleLineSet *)config->linesets.first, i=0; lineset; lineset=lineset->next, i++) {
			if(i == index)
				lineset->flags |= FREESTYLE_LINESET_CURRENT;
			else
				lineset->flags &= ~FREESTYLE_LINESET_CURRENT;
		}
	}

	void FRS_unlink_target_object(FreestyleConfig *config, Object *ob)
	{
		FreestyleLineSet *lineset;

		for(lineset=(FreestyleLineSet *)config->linesets.first; lineset; lineset=lineset->next) {
			FRS_unlink_linestyle_target_object(lineset->linestyle, ob);
		}
	}

#ifdef __cplusplus
}
#endif
