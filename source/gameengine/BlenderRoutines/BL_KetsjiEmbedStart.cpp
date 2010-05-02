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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * Blender's Ketsji startpoint
 */

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
// don't show stl-warnings
#pragma warning (disable:4786)
#endif

#include "GL/glew.h"

#include "KX_BlenderGL.h"
#include "KX_BlenderCanvas.h"
#include "KX_BlenderKeyboardDevice.h"
#include "KX_BlenderMouseDevice.h"
#include "KX_BlenderRenderTools.h"
#include "KX_BlenderSystem.h"
#include "BL_Material.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"

#include "RAS_GLExtensionManager.h"
#include "RAS_OpenGLRasterizer.h"
#include "RAS_VAOpenGLRasterizer.h"
#include "RAS_ListRasterizer.h"

#include "NG_LoopBackNetworkDeviceInterface.h"

#include "SYS_System.h"

#include "GPU_extensions.h"
#include "Value.h"



#ifdef __cplusplus
extern "C" {
#endif
	/***/
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "BKE_utildefines.h"
//XXX #include "BIF_screen.h"
//XXX #include "BIF_scrarea.h"

#include "BKE_main.h"
#include "BLI_blenlib.h"
#include "BLO_readfile.h"
#include "DNA_scene_types.h"
	/***/

#include "AUD_C-API.h"

//XXX #include "BSE_headerbuttons.h"
#include "BKE_context.h"
#include "../../blender/windowmanager/WM_types.h"
#include "../../blender/windowmanager/wm_window.h"
#include "../../blender/windowmanager/wm_event_system.h"
#ifdef __cplusplus
}
#endif


static BlendFileData *load_game_data(char *filename)
{
	ReportList reports;
	BlendFileData *bfd;
	
	BKE_reports_init(&reports, RPT_STORE);
	bfd= BLO_read_from_file(filename, &reports);

	if (!bfd) {
		printf("Loading %s failed: ", filename);
		BKE_reports_print(&reports, RPT_ERROR);
	}

	BKE_reports_clear(&reports);

	return bfd;
}

extern "C" void StartKetsjiShell(struct bContext *C, struct ARegion *ar, rcti *cam_frame, int always_use_expand_framing)
{
	/* context values */
	struct wmWindow *win= CTX_wm_window(C);
	struct Scene *scene= CTX_data_scene(C);
	struct Main* maggie1= CTX_data_main(C);


	RAS_Rect area_rect;
	area_rect.SetLeft(cam_frame->xmin);
	area_rect.SetBottom(cam_frame->ymin);
	area_rect.SetRight(cam_frame->xmax);
	area_rect.SetTop(cam_frame->ymax);

	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	Main* blenderdata = maggie1;

	char* startscenename = scene->id.name+2;
	char pathname[FILE_MAXDIR+FILE_MAXFILE], oldsce[FILE_MAXDIR+FILE_MAXFILE];
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;

	BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
	BLI_strncpy(oldsce, G.sce, sizeof(oldsce));
#ifndef DISABLE_PYTHON
	resetGamePythonPath(); // need this so running a second time wont use an old blendfiles path
	setGamePythonPath(G.sce);

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();
	
	PyObject *pyGlobalDict = PyDict_New(); /* python utility storage, spans blend file loading */
#endif
	
	bgl::InitExtensions(true);

	do
	{
		View3D *v3d= CTX_wm_view3d(C);
		RegionView3D *rv3d= CTX_wm_region_view3d(C);

		// get some preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool usefixed = (SYS_GetCommandLineInt(syshandle, "fixedtime", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool animation_record = (SYS_GetCommandLineInt(syshandle, "animation_record", 0) != 0);
		bool displaylists = (SYS_GetCommandLineInt(syshandle, "displaylists", 0) != 0);
		bool nodepwarnings = (SYS_GetCommandLineInt(syshandle, "ignore_deprecation_warnings", 0) != 0);
		bool novertexarrays = (SYS_GetCommandLineInt(syshandle, "novertexarrays", 0) != 0);
		if(animation_record) usefixed= true; /* override since you's always want fixed time for sim recording */

		// create the canvas, rasterizer and rendertools
		RAS_ICanvas* canvas = new KX_BlenderCanvas(win, area_rect, ar);
		canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
		RAS_IRenderTools* rendertools = new KX_BlenderRenderTools();
		RAS_IRasterizer* rasterizer = NULL;
		
		if(displaylists) {
			if (GLEW_VERSION_1_1 && !novertexarrays)
				rasterizer = new RAS_ListRasterizer(canvas, true, true);
			else
				rasterizer = new RAS_ListRasterizer(canvas);
		}
		else if (GLEW_VERSION_1_1 && !novertexarrays)
			rasterizer = new RAS_VAOpenGLRasterizer(canvas, false);
		else
			rasterizer = new RAS_OpenGLRasterizer(canvas);
		
		// create the inputdevices
		KX_BlenderKeyboardDevice* keyboarddevice = new KX_BlenderKeyboardDevice();
		KX_BlenderMouseDevice* mousedevice = new KX_BlenderMouseDevice();
		
		// create a networkdevice
		NG_NetworkDeviceInterface* networkdevice = new
			NG_LoopBackNetworkDeviceInterface();

		//
		// create a ketsji/blendersystem (only needed for timing and stuff)
		KX_BlenderSystem* kxsystem = new KX_BlenderSystem();
		
		// create the ketsjiengine
		KX_KetsjiEngine* ketsjiengine = new KX_KetsjiEngine(kxsystem);
		
		// set the devices
		ketsjiengine->SetKeyboardDevice(keyboarddevice);
		ketsjiengine->SetMouseDevice(mousedevice);
		ketsjiengine->SetNetworkDevice(networkdevice);
		ketsjiengine->SetCanvas(canvas);
		ketsjiengine->SetRenderTools(rendertools);
		ketsjiengine->SetRasterizer(rasterizer);
		ketsjiengine->SetNetworkDevice(networkdevice);
		ketsjiengine->SetUseFixedTime(usefixed);
		ketsjiengine->SetTimingDisplay(frameRate, profile, properties);

#ifndef DISABLE_PYTHON
		CValue::SetDeprecationWarnings(nodepwarnings);
#endif

		//lock frame and camera enabled - storing global values
		int tmp_lay= scene->lay;
		Object *tmp_camera = scene->camera;

		if (v3d->scenelock==0){
			scene->lay= v3d->lay;
			scene->camera= v3d->camera;
		}

		// some blender stuff
		MT_CmMatrix4x4 projmat;
		MT_CmMatrix4x4 viewmat;
		float camzoom;
		int i;

		for (i = 0; i < 16; i++)
		{
			float *viewmat_linear= (float*) rv3d->viewmat;
			viewmat.setElem(i, viewmat_linear[i]);
		}
		for (i = 0; i < 16; i++)
		{
			float *projmat_linear= (float*) rv3d->winmat;
			projmat.setElem(i, projmat_linear[i]);
		}
		
		if(rv3d->persp==RV3D_CAMOB) {
			if(scene->gm.framing.type == SCE_GAMEFRAMING_BARS) { /* Letterbox */
				camzoom = 1.0f;
			}
			else {
				camzoom = (1.41421 + (rv3d->camzoom / 50.0));
				camzoom *= camzoom;
				camzoom = 4.0 / camzoom;
			}
		}
		else {
			camzoom = 2.0;
		}

		

		ketsjiengine->SetDrawType(v3d->drawtype);
		ketsjiengine->SetCameraZoom(camzoom);
		
		// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
		if (exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME || exitrequested == KX_EXIT_REQUEST_RESTART_GAME)
		{
			exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
			if (bfd) BLO_blendfiledata_free(bfd);
			
			char basedpath[240];
			// base the actuator filename with respect
			// to the original file working directory

			if (exitstring != "")
				strcpy(basedpath, exitstring.Ptr());

			// load relative to the last loaded file, this used to be relative
			// to the first file but that makes no sense, relative paths in
			// blend files should be relative to that file, not some other file
			// that happened to be loaded first
			BLI_path_abs(basedpath, pathname);
			bfd = load_game_data(basedpath);
			
			// if it wasn't loaded, try it forced relative
			if (!bfd)
			{
				// just add "//" in front of it
				char temppath[242];
				strcpy(temppath, "//");
				strcat(temppath, basedpath);
				
				BLI_path_abs(temppath, pathname);
				bfd = load_game_data(temppath);
			}
			
			// if we got a loaded blendfile, proceed
			if (bfd)
			{
				blenderdata = bfd->main;
				startscenename = bfd->curscene->id.name + 2;

				if(blenderdata) {
					BLI_strncpy(G.sce, blenderdata->name, sizeof(G.sce));
					BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
#ifndef DISABLE_PYTHON
					setGamePythonPath(G.sce);
#endif
				}
			}
			// else forget it, we can't find it
			else
			{
				exitrequested = KX_EXIT_REQUEST_QUIT_GAME;
			}
		}

		Scene *blscene= bfd ? bfd->curscene : (Scene *)BLI_findstring(&blenderdata->scene, startscenename, offsetof(ID, name) + 2);

		if (blscene)
		{
			int startFrame = blscene->r.cfra;
			ketsjiengine->SetAnimRecordMode(animation_record, startFrame);
			
			// Quad buffered needs a special window.
			if(blscene->gm.stereoflag == STEREO_ENABLED){
				if (blscene->gm.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
					rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) blscene->gm.stereomode);

				rasterizer->SetEyeSeparation(blscene->gm.eyeseparation);
			}

			rasterizer->SetBackColor(blscene->gm.framing.col[0], blscene->gm.framing.col[1], blscene->gm.framing.col[2], 0.0f);
		}
		
		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			if (rv3d->persp != RV3D_CAMOB)
			{
				ketsjiengine->EnableCameraOverride(startscenename);
				ketsjiengine->SetCameraOverrideUseOrtho((rv3d->persp == RV3D_ORTHO));
				ketsjiengine->SetCameraOverrideProjectionMatrix(projmat);
				ketsjiengine->SetCameraOverrideViewMatrix(viewmat);
				ketsjiengine->SetCameraOverrideClipping(v3d->near, v3d->far);
				ketsjiengine->SetCameraOverrideLens(v3d->lens);
			}
			
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(blenderdata, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			sceneconverter->addInitFromFrame=false;
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);

			bool usemat = false, useglslmat = false;

			if(GLEW_ARB_multitexture && GLEW_VERSION_1_1)
				usemat = true;

			if(GPU_glsl_support())
				useglslmat = true;
			else if(blscene->gm.matmode == GAME_MAT_GLSL)
				usemat = false;

            if(usemat && (blscene->gm.matmode != GAME_MAT_TEXFACE))
				sceneconverter->SetMaterials(true);
			if(useglslmat && (blscene->gm.matmode == GAME_MAT_GLSL))
				sceneconverter->SetGLSLMaterials(true);
					
			KX_Scene* startscene = new KX_Scene(keyboarddevice,
				mousedevice,
				networkdevice,
				startscenename,
				blscene,
				canvas);

#ifndef DISABLE_PYTHON
			// some python things
			PyObject *gameLogic, *gameLogic_keys;
			setupGamePython(ketsjiengine, startscene, blenderdata, pyGlobalDict, &gameLogic, &gameLogic_keys, 0, NULL);
#endif // DISABLE_PYTHON

			//initialize Dome Settings
			if(blscene->gm.stereoflag == STEREO_DOME)
				ketsjiengine->InitDome(blscene->gm.dome.res, blscene->gm.dome.mode, blscene->gm.dome.angle, blscene->gm.dome.resbuf, blscene->gm.dome.tilt, blscene->gm.dome.warptext);

			// initialize 3D Audio Settings
			AUD_set3DSetting(AUD_3DS_SPEED_OF_SOUND, blscene->audio.speed_of_sound);
			AUD_set3DSetting(AUD_3DS_DOPPLER_FACTOR, blscene->audio.doppler_factor);
			AUD_set3DSetting(AUD_3DS_DISTANCE_MODEL, blscene->audio.distance_model);

			if (sceneconverter)
			{
				// convert and add scene
				sceneconverter->ConvertScene(
					startscene,
					rendertools,
					canvas);
				ketsjiengine->AddScene(startscene);
				
				// init the rasterizer
				rasterizer->Init();
				
				// start the engine
				ketsjiengine->StartEngine(true);
				

				// Set the animation playback rate for ipo's and actions
				// the framerate below should patch with FPS macro defined in blendef.h
				// Could be in StartEngine set the framerate, we need the scene to do this
				ketsjiengine->SetAnimFrameRate(FPS);
				
				// the mainloop
				printf("\nBlender Game Engine Started\n\n");
				while (!exitrequested)
				{
					// first check if we want to exit
					exitrequested = ketsjiengine->GetExitCode();
					
					// kick the engine
					bool render = ketsjiengine->NextFrame(); // XXX 2.5 Bug, This is never true! FIXME-  Campbell
					
					if (render)
					{
						// render the frame
						ketsjiengine->Render();
					}
					
					wm_window_process_events_nosleep(C);
					
					// test for the ESC key
					//XXX while (qtest())
					while(wmEvent *event= (wmEvent *)win->queue.first)
					{
						short val = 0;
						//unsigned short event = 0; //XXX extern_qread(&val);
						
						if (keyboarddevice->ConvertBlenderEvent(event->type,event->val))
							exitrequested = KX_EXIT_REQUEST_BLENDER_ESC;
						
							/* Coordinate conversion... where
							* should this really be?
						*/
						if (event->type==MOUSEMOVE) {
							/* Note, not nice! XXX 2.5 event hack */
							val = event->x - ar->winrct.xmin;
							mousedevice->ConvertBlenderEvent(MOUSEX, val);
							
							val = ar->winy - (event->y - ar->winrct.ymin) - 1;
							mousedevice->ConvertBlenderEvent(MOUSEY, val);
						}
						else {
							mousedevice->ConvertBlenderEvent(event->type,event->val);
						}
						
						BLI_remlink(&win->queue, event);
						wm_event_free(event);
					}
					
				}
				printf("\nBlender Game Engine Finished\n\n");
				exitstring = ketsjiengine->GetExitString();


				// when exiting the mainloop
#ifndef DISABLE_PYTHON
				// Clears the dictionary by hand:
				// This prevents, extra references to global variables
				// inside the GameLogic dictionary when the python interpreter is finalized.
				// which allows the scene to safely delete them :)
				// see: (space.c)->start_game
				
				//PyDict_Clear(PyModule_GetDict(gameLogic));
				
				// Keep original items, means python plugins will autocomplete members
				int listIndex;
				PyObject *gameLogic_keys_new = PyDict_Keys(PyModule_GetDict(gameLogic));
				for (listIndex=0; listIndex < PyList_Size(gameLogic_keys_new); listIndex++)  {
					PyObject* item = PyList_GET_ITEM(gameLogic_keys_new, listIndex);
					if (!PySequence_Contains(gameLogic_keys, item)) {
						PyDict_DelItem(	PyModule_GetDict(gameLogic), item);
					}
				}
				Py_DECREF(gameLogic_keys_new);
				gameLogic_keys_new = NULL;
#endif
				ketsjiengine->StopEngine();
#ifndef DISABLE_PYTHON
				exitGamePythonScripting();
#endif
				networkdevice->Disconnect();
			}
			if (sceneconverter)
			{
				delete sceneconverter;
				sceneconverter = NULL;
			}

#ifndef DISABLE_PYTHON
			Py_DECREF(gameLogic_keys);
			gameLogic_keys = NULL;
#endif
		}
		//lock frame and camera enabled - restoring global values
		if (v3d->scenelock==0){
			scene->lay= tmp_lay;
			scene->camera= tmp_camera;
		}

		// set the cursor back to normal
		canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		
		// clean up some stuff
		if (ketsjiengine)
		{
			delete ketsjiengine;
			ketsjiengine = NULL;
		}
		if (kxsystem)
		{
			delete kxsystem;
			kxsystem = NULL;
		}
		if (networkdevice)
		{
			delete networkdevice;
			networkdevice = NULL;
		}
		if (keyboarddevice)
		{
			delete keyboarddevice;
			keyboarddevice = NULL;
		}
		if (mousedevice)
		{
			delete mousedevice;
			mousedevice = NULL;
		}
		if (rasterizer)
		{
			delete rasterizer;
			rasterizer = NULL;
		}
		if (rendertools)
		{
			delete rendertools;
			rendertools = NULL;
		}
		if (canvas)
		{
			delete canvas;
			canvas = NULL;
                }
	
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);
	
	if (bfd) BLO_blendfiledata_free(bfd);

	BLI_strncpy(G.sce, oldsce, sizeof(G.sce));

#ifndef DISABLE_PYTHON
	Py_DECREF(pyGlobalDict);

	// Release Python's GIL
	PyGILState_Release(gilstate);
#endif

}
