#include "AppView.h"
#include "Controller.h"
#include "AppConfig.h"
#include "AppCanvas.h"

#include <iostream>
#include <map>
#include <set>
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BLI_blenlib.h"
#include "BIF_renderwin.h"
#include "BPY_extern.h"

#include "render_types.h"
#include "renderpipeline.h"
#include "pixelblending.h"

#include "../../FRS_freestyle.h"

	// Freestyle configuration
	short freestyle_is_initialized = 0;
	static Config::Path *pathconfig = NULL;
	static Controller *controller = NULL;
	static AppView *view = NULL;

	// camera information
	float freestyle_viewpoint[3];
	float freestyle_mv[4][4];
	float freestyle_proj[4][4];
	int freestyle_viewport[4];
	
	// Panel configuration
	short freestyle_current_layer_number = 0;
	char* freestyle_current_module_path = NULL;
	SceneRenderLayer* freestyle_current_layer = NULL;

	ListBase* freestyle_modules;
	int* freestyle_flags;
	float* freestyle_sphere_radius;
	float* freestyle_dkr_epsilon;
	
	class FreestylePanelConfigurationData {
	public:
		set<SceneRenderLayer*> layers;
		map<SceneRenderLayer*, ListBase*> modules;
		map<SceneRenderLayer*, int*> flags;
		map<SceneRenderLayer*, float*> sphere_radius;
		map<SceneRenderLayer*, float*> dkr_epsilon;
		
		FreestylePanelConfigurationData() {}
		~FreestylePanelConfigurationData() {
			set<SceneRenderLayer*>::iterator it;
			
			for( it=layers.begin(); it!=layers.end(); it++)
				FRS_delete_layer( *it, 1 );
		}
	};
	static FreestylePanelConfigurationData* panelConfig;
	
	string default_module_path;

	//=======================================================
	//   Initialization 
	//=======================================================

	void FRS_initialize(){
		
		if( !freestyle_is_initialized ) {

			pathconfig = new Config::Path;
			controller = new Controller;
			view = new AppView;
			controller->setView(view);
			
			panelConfig = new FreestylePanelConfigurationData;
			
			default_module_path = pathconfig->getProjectDir() + Config::DIR_SEP + "style_modules" + Config::DIR_SEP + "contour.py";
			FRS_select_layer( (SceneRenderLayer*) BLI_findlink(&G.scene->r.layers, G.scene->r.actlay) );
			
			freestyle_is_initialized = 1;
		}
		
	}
	
	void FRS_exit() {
		delete pathconfig;
		delete controller;
		delete view;
		delete panelConfig;
	}

	//=======================================================
	//   Rendering 
	//=======================================================

	void init_view(Render* re){
		int width = re->scene->r.xsch;
		int height = re->scene->r.ysch;
		
		freestyle_viewport[0] = freestyle_viewport[1] = 0;
		freestyle_viewport[2] = width;
		freestyle_viewport[3] = height;
		
		view->setWidth( width );
		view->setHeight( height );
	}

	void init_camera(Render* re){
		Object* maincam_obj = re->scene->camera;
		Camera *cam = (Camera*) maincam_obj->data;

		view->setHorizontalFov( cam->angle );
		
		freestyle_viewpoint[0] = maincam_obj->obmat[3][0];
		freestyle_viewpoint[1] = maincam_obj->obmat[3][1];
		freestyle_viewpoint[2] = maincam_obj->obmat[3][2];
		
		freestyle_mv[0][0] = maincam_obj->obmat[0][0];
		freestyle_mv[0][1] = maincam_obj->obmat[1][0];
		freestyle_mv[0][2] = maincam_obj->obmat[2][0];
		freestyle_mv[0][3] = 0.0;

		freestyle_mv[1][0] = maincam_obj->obmat[0][1];
		freestyle_mv[1][1] = maincam_obj->obmat[1][1];
		freestyle_mv[1][2] = maincam_obj->obmat[2][1];
		freestyle_mv[1][3] = 0.0;

		freestyle_mv[2][0] = re->viewmat[2][0];
		freestyle_mv[2][1] = re->viewmat[2][1];
		freestyle_mv[2][2] = re->viewmat[2][2];
		freestyle_mv[2][3] = 0.0;

		freestyle_mv[3][0] = re->viewmat[3][0];
		freestyle_mv[3][1] = re->viewmat[3][1];
		freestyle_mv[3][2] = re->viewmat[3][2];
		freestyle_mv[3][3] = 1.0;

		for( int i = 0; i < 4; i++ )
		   for( int j = 0; j < 4; j++ )
			freestyle_proj[i][j] = re->winmat[i][j];
	}

	
	void prepare(Render* re, SceneRenderLayer* srl ) {
		
		// clear canvas
		controller->Clear();

		// load mesh
		if( controller->LoadMesh(re, srl) ) // returns if scene cannot be loaded or if empty
			return;
		
		// add style modules
		cout << "\n===  Rendering options  ===" << endl;
		cout << "Modules :"<< endl;
		int layer_count = 0;
		for( StyleModuleConf* module_conf = (StyleModuleConf *)panelConfig->modules[srl]->first; module_conf; module_conf = module_conf->next ) {
			if( module_conf->is_displayed ) {
				cout << "  " << layer_count+1 << ": " << module_conf->module_path << endl;
				controller->InsertStyleModule( layer_count, module_conf->module_path );
				controller->toggleLayer(layer_count, true);
				layer_count++;
			}
		}	
		cout << endl;
		
		// set parameters
		controller->setSphereRadius(*panelConfig->sphere_radius[srl]);
		controller->setComputeRidgesAndValleysFlag((*panelConfig->flags[srl] & FREESTYLE_RIDGES_AND_VALLEYS_FLAG) ? true : false);
		controller->setComputeSuggestiveContoursFlag((*panelConfig->flags[srl] & FREESTYLE_SUGGESTIVE_CONTOURS_FLAG) ? true : false);
		controller->setSuggestiveContourKrDerivativeEpsilon(*panelConfig->dkr_epsilon[srl]);

		cout << "Sphere radius : " << controller->getSphereRadius() << endl;
		cout << "Redges and valleys : " << (controller->getComputeRidgesAndValleysFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contours : " << (controller->getComputeSuggestiveContoursFlag() ? "enabled" : "disabled") << endl;
		cout << "Suggestive contour dkr epsilon : " << controller->getSuggestiveContourKrDerivativeEpsilon() << endl;

		// compute view map
		controller->ComputeViewMap();
	}
	
	void composite_result(Render* re, SceneRenderLayer* srl, Render* freestyle_render)
	{

		RenderLayer *rl;
	    float *src, *dest, *pixSrc, *pixDest;
		int x, y, rectx, recty;
		
		if( freestyle_render == NULL || freestyle_render->result == NULL )
			return;

		rl = render_get_active_layer( freestyle_render, freestyle_render->result );
	    if( !rl || rl->rectf == NULL) { cout << "Cannot find Freestyle result image" << endl; return; }
		src  = rl->rectf;
		
		rl = RE_GetRenderLayer(re->result, srl->name);
	    if( !rl || rl->rectf == NULL) { cout << "No layer to composite to" << endl; return; }
		dest  = rl->rectf;
		
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
	
	int displayed_layer_count( SceneRenderLayer* srl ) {
		int count = 0;
		
		if( panelConfig->layers.find(srl) == panelConfig->layers.end() )
			return 0;

		for( StyleModuleConf* module_conf = (StyleModuleConf *)panelConfig->modules[srl]->first; module_conf; module_conf = module_conf->next ) {
			if( module_conf->is_displayed )
				count++;
		}
		return count;
	}
	
	void FRS_add_Freestyle(Render* re) {
		
		SceneRenderLayer *srl;
		Render* freestyle_render = NULL;
		
		// init
		cout << "\n#===============================================================" << endl;
		cout << "#  Freestyle" << endl;
		cout << "#===============================================================" << endl;
		
		FRS_initialize();
		init_view(re);
		init_camera(re);
		
		for(srl= (SceneRenderLayer *)G.scene->r.layers.first; srl; srl= srl->next) {
			if( !(srl->layflag & SCE_LAY_DISABLE) &&
			 	srl->layflag & SCE_LAY_FRS &&
				displayed_layer_count(srl) > 0       )
			{
				cout << "\n----------------------------------------------------------" << endl;
				cout << "|  "<< srl->name << endl;
				cout << "----------------------------------------------------------" << endl;
				
				// prepare Freestyle:
				//   - clear canvas
				//   - load mesh
				//   - add style modules
				//   - set parameters
				//   - compute view map
				prepare(re, srl);

				// render and composite Freestyle result
				if( controller->_ViewMap ) {
					
					// render strokes					
					controller->DrawStrokes();
					freestyle_render = controller->RenderStrokes(re);
					controller->CloseFile();
					
					// composite result
					composite_result(re, srl, freestyle_render);
					
					// free resources
					RE_FreeRender(freestyle_render);
				}
			}
		}
		
	}

	//=======================================================
	//   Freestyle Panel Configuration
	//=======================================================

	void FRS_select_layer( SceneRenderLayer* srl )
	{
		if( panelConfig->layers.find(srl) == panelConfig->layers.end() )
		{
			panelConfig->layers.insert(srl);
			
			panelConfig->modules[srl] = new ListBase;
			panelConfig->modules[srl]->first = panelConfig->modules[srl]->last = NULL;
			
			panelConfig->flags[srl] = new int(0);
			panelConfig->sphere_radius[srl] = new float(1.0);
			panelConfig->dkr_epsilon[srl] = new float(0.001);
		}
		
		freestyle_modules = panelConfig->modules[srl];
		freestyle_flags = panelConfig->flags[srl];
		freestyle_sphere_radius = panelConfig->sphere_radius[srl];
		freestyle_dkr_epsilon = panelConfig->dkr_epsilon[srl];
		
		freestyle_current_layer = srl;
		freestyle_current_layer_number = BLI_findindex(&G.scene->r.layers, freestyle_current_layer);
	}

	void FRS_delete_layer( SceneRenderLayer* srl, short isDestructor )
	{
		BLI_freelistN( panelConfig->modules[srl] );
		delete panelConfig->modules[srl];
		
		delete panelConfig->flags[srl];
		delete panelConfig->sphere_radius[srl];
		delete panelConfig->dkr_epsilon[srl];
		
		panelConfig->modules.erase(srl);
		panelConfig->flags.erase(srl);
		panelConfig->sphere_radius.erase(srl);
		panelConfig->dkr_epsilon.erase(srl);
		
		if( !isDestructor )
			panelConfig->layers.erase(srl);
	}
	
	void FRS_add_module()
	{
		StyleModuleConf* module_conf = (StyleModuleConf*) MEM_callocN( sizeof(StyleModuleConf), "style module configuration");
		BLI_addtail(freestyle_modules, (void*) module_conf);
		
		strcpy( module_conf->module_path, default_module_path.c_str() );
		module_conf->is_displayed = 1;	
	}
	
	void FRS_delete_module(void *module_index_ptr, void *unused)
	{
		StyleModuleConf* module_conf = (StyleModuleConf*) BLI_findlink(freestyle_modules, (intptr_t)module_index_ptr);

		BLI_freelinkN( freestyle_modules, module_conf);
	}
	
	void FRS_move_up_module(void *module_index_ptr, void *unused)
	{
		StyleModuleConf* module_conf = (StyleModuleConf*) BLI_findlink(freestyle_modules, (intptr_t)module_index_ptr);
		
		BLI_remlink(freestyle_modules, module_conf);
		BLI_insertlink(freestyle_modules, module_conf->prev->prev, module_conf);
	}
	
	void FRS_move_down_module(void *module_index_ptr, void *unused)
	{			
		StyleModuleConf* module_conf = (StyleModuleConf*) BLI_findlink(freestyle_modules, (intptr_t)module_index_ptr);
		
		BLI_remlink(freestyle_modules, module_conf);
		BLI_insertlink(freestyle_modules, module_conf->next, module_conf);
	}
	
	void FRS_set_module_path(void *module_index_ptr, void *unused)
	{
		StyleModuleConf* module_conf = (StyleModuleConf*) BLI_findlink(freestyle_modules, (intptr_t)module_index_ptr);
		freestyle_current_module_path = module_conf->module_path;
	}
	
	
	
#ifdef __cplusplus
}
#endif
