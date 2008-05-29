
#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"
#include "test_config.h"

#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include "render_types.h"
//#include "renderdatabase.h"
/* display_draw() needs render layer info */
#include "renderpipeline.h"

#ifdef __cplusplus
}
#endif

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

	void FRS_execute(Render* re) {
		cout << "Freestyle start" << endl;
	
		Config::Path pathconfig;
		Controller *c = new Controller;
		AppGLWidget *view = new AppGLWidget;
		
		c->SetView(view);
		unsigned int width = re->winx;
		unsigned int height = re->winy;
		view->setWidth(width);
		view->setHeight(height);
		
		c->Load3DSFile( TEST_3DS_FILE );
		
		c->InsertStyleModule( 0, TEST_STYLE_MODULE_FILE );
		c->toggleLayer(0, true);
		c->ComputeViewMap();
		
		c->DrawStrokes(); // build strokes
		view->draw(); // render final result
		
		RenderResult rres;
		RE_GetResultImage(re, &rres);
		view->readPixels(0,0,width,height,AppGLWidget::RGBA, rres.rectf );		
		re->result->renlay = render_get_active_layer(re, re->result);
		re->display_draw(re->result, NULL);
		
		cout << "Freestyle end" << endl;

	}
	
#ifdef __cplusplus
}
#endif
