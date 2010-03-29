#include "../application/AppView.h"
#include "../application/Controller.h"
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
#include "DNA_freestyle_types.h"

#include "BKE_main.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BPY_extern.h"

#include "renderpipeline.h"
#include "pixelblending.h"

#include "../../FRS_freestyle.h"
#include "../../FRS_freestyle_config.h"

	// Freestyle configuration
	static short freestyle_is_initialized = 0;
	static Config::Path *pathconfig = NULL;
	static Controller *controller = NULL;
	static AppView *view = NULL;

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
		freestyle_scene = NULL;
			
		default_module_path = pathconfig->getProjectDir() + Config::DIR_SEP + "style_modules" + Config::DIR_SEP + "contour.py";
			
		freestyle_is_initialized = 1;
	}
	
	void FRS_set_context(bContext* C) {
		cout << "FRS_set_context: context 0x" << C << " scene 0x" << CTX_data_scene(C) << endl;
		controller->setContext(C);
	}

	void FRS_exit() {
		delete pathconfig;
		delete controller;
		delete view;
	}

	//=======================================================
	//   Rendering 
	//=======================================================

	void init_view(Render* re){
		float ycor = ((float)re->r.yasp) / ((float)re->r.xasp);
		int width = re->r.xsch;
		int height = (int)(((float)re->r.ysch) * ycor);
		
		freestyle_viewport[0] = freestyle_viewport[1] = 0;
		freestyle_viewport[2] = width;
		freestyle_viewport[3] = height;
		
		view->setWidth( width );
		view->setHeight( height );
	}

	void init_camera(Render* re){
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

	
	void prepare(Render* re, SceneRenderLayer* srl ) {
				
		// clear canvas
		controller->Clear();

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
		cout << "Modules :"<< endl;
		int layer_count = 0;
		

		for( FreestyleModuleConfig* module_conf = (FreestyleModuleConfig *)config->modules.first; module_conf; module_conf = module_conf->next ) {
			if( module_conf->is_displayed ) {
				cout << "  " << layer_count+1 << ": " << module_conf->module_path << endl;
				controller->InsertStyleModule( layer_count, module_conf->module_path );
				controller->toggleLayer(layer_count, true);
				layer_count++;
			}
		}	
		cout << endl;
		
		// set parameters
		controller->setSphereRadius( config->sphere_radius );
		controller->setComputeRidgesAndValleysFlag( (config->flags & FREESTYLE_RIDGES_AND_VALLEYS_FLAG) ? true : false);
		controller->setComputeSuggestiveContoursFlag( (config->flags & FREESTYLE_SUGGESTIVE_CONTOURS_FLAG) ? true : false);
		controller->setSuggestiveContourKrDerivativeEpsilon( config->dkr_epsilon ) ;

		cout << "Sphere radius : " << controller->getSphereRadius() << endl;
		cout << "Redges and valleys : " << (controller->getComputeRidgesAndValleysFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contours : " << (controller->getComputeSuggestiveContoursFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contour dkr epsilon : " << controller->getSuggestiveContourKrDerivativeEpsilon() << endl;
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
		int x, y, src_rectx, src_recty, dest_rectx, dest_recty, x_offset, y_offset;
		
		if( freestyle_render == NULL || freestyle_render->result == NULL )
			return;

		rl = render_get_active_layer( freestyle_render, freestyle_render->result );
	    if( !rl || rl->rectf == NULL) { cout << "Cannot find Freestyle result image" << endl; return; }
		src  = rl->rectf;
		src_rectx = rl->rectx;
		src_recty = rl->recty;
		//cout << "src: " << src_rectx << " x " << src_recty << endl;
		
		rl = RE_GetRenderLayer(re->result, srl->name);
	    if( !rl || rl->rectf == NULL) { cout << "No layer to composite to" << endl; return; }
		dest  = rl->rectf;
		dest_rectx = rl->rectx;
		dest_recty = rl->recty;
		//cout << "dest: " << dest_rectx << " x " << dest_recty << endl;

		if (re->r.mode & R_BORDER && !(re->r.mode & R_CROP)) {
			x_offset = re->disprect.xmin;
			y_offset = re->disprect.ymin;
		} else {
			x_offset = 0;
			y_offset = 0;
		}
	    for( y = 0; y < dest_recty; y++) {
	        for( x = 0; x < dest_rectx; x++) {
	            pixSrc = src + 4 * (src_rectx * (y + y_offset) + (x + x_offset));
	            if( pixSrc[3] > 0.0) {
					pixDest = dest + 4 * (dest_rectx * y + x);
					addAlphaOverFloat(pixDest, pixSrc);
				}
			}
	    }
	}
	
	int displayed_layer_count( SceneRenderLayer* srl ) {
		int count = 0;

		for( FreestyleModuleConfig* module_conf = (FreestyleModuleConfig *)srl->freestyleConfig.modules.first; module_conf; module_conf = module_conf->next ) {
			if( module_conf->is_displayed )
				count++;
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
		
		cout << "\n----------------------------------------------------------" << endl;
		cout << "|  " << (re->scene->id.name+2) << "|" << srl->name << endl;
		cout << "----------------------------------------------------------" << endl;
		
		// prepare Freestyle:
		//   - clear canvas
		//   - load mesh
		//   - add style modules
		//   - set parameters
		//   - compute view map
		prepare(re, srl);

        if( re->test_break(re->tbh) ) {
			controller->CloseFile();
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

	//=======================================================
	//   Freestyle Panel Configuration
	//=======================================================

	void FRS_add_freestyle_config( SceneRenderLayer* srl )
	{		
		FreestyleConfig* config = &srl->freestyleConfig;
		
		config->modules.first = config->modules.last = NULL;
		config->flags = 0;
		config->sphere_radius = 1.0;
		config->dkr_epsilon = 0.001;
	}
	
	void FRS_free_freestyle_config( SceneRenderLayer* srl )
	{		
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
	
	void FRS_move_up_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
	{
		BLI_remlink(&config->modules, module_conf);
		BLI_insertlink(&config->modules, module_conf->prev->prev, module_conf);
	}
	
	void FRS_move_down_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
	{			
		BLI_remlink(&config->modules, module_conf);
		BLI_insertlink(&config->modules, module_conf->next, module_conf);
	}

#ifdef __cplusplus
}
#endif
