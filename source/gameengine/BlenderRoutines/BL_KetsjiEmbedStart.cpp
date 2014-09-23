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

/** \file gameengine/BlenderRoutines/BL_KetsjiEmbedStart.cpp
 *  \ingroup blroutines
 */


#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
   /* don't show stl-warnings */
#  pragma warning (disable:4786)
#endif

#include "GL/glew.h"

#include "KX_BlenderCanvas.h"
#include "KX_BlenderKeyboardDevice.h"
#include "KX_BlenderMouseDevice.h"
#include "KX_BlenderSystem.h"
#include "BL_Material.h"

#include "KX_KetsjiEngine.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "KX_PythonMain.h"

#include "RAS_GLExtensionManager.h"
#include "RAS_OpenGLRasterizer.h"
#include "RAS_ListRasterizer.h"

#include "NG_LoopBackNetworkDeviceInterface.h"

#include "BL_System.h"

#include "GPU_extensions.h"
#include "Value.h"


extern "C" {
	#include "DNA_object_types.h"
	#include "DNA_view3d_types.h"
	#include "DNA_screen_types.h"
	#include "DNA_userdef_types.h"
	#include "DNA_scene_types.h"
	#include "DNA_windowmanager_types.h"

	#include "BKE_global.h"
	#include "BKE_report.h"
	#include "BKE_ipo.h"
	#include "BKE_main.h"
	#include "BKE_context.h"

	/* avoid c++ conflict with 'new' */
	#define new _new
	#include "BKE_screen.h"
	#undef new

	#include "MEM_guardedalloc.h"

	#include "BLI_blenlib.h"
	#include "BLO_readfile.h"

	#include "../../blender/windowmanager/WM_types.h"
	#include "../../blender/windowmanager/wm_window.h"

/* avoid more includes (not used by BGE) */
typedef void * wmUIHandlerFunc;
typedef void * wmUIHandlerRemoveFunc;

	#include "../../blender/windowmanager/wm_event_system.h"
}

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#  include "AUD_I3DDevice.h"
#  include "AUD_IDevice.h"
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

static int BL_KetsjiNextFrame(KX_KetsjiEngine *ketsjiengine, bContext *C, wmWindow *win, Scene *scene, ARegion *ar,
                              KX_BlenderKeyboardDevice* keyboarddevice, KX_BlenderMouseDevice* mousedevice, int draw_letterbox)
{
	int exitrequested;

	// first check if we want to exit
	exitrequested = ketsjiengine->GetExitCode();

	// kick the engine
	bool render = ketsjiengine->NextFrame();

	if (render) {
		if (draw_letterbox) {
			// Clear screen to border color
			// We do this here since we set the canvas to be within the frames. This means the engine
			// itself is unaware of the extra space, so we clear the whole region for it.
			glClearColor(scene->gm.framing.col[0], scene->gm.framing.col[1], scene->gm.framing.col[2], 1.0f);
			glViewport(ar->winrct.xmin, ar->winrct.ymin,
			           BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));
			glClear(GL_COLOR_BUFFER_BIT);
		}

		// render the frame
		ketsjiengine->Render();
	}

	wm_window_process_events_nosleep();

	// test for the ESC key
	//XXX while (qtest())
	while (wmEvent *event= (wmEvent *)win->queue.first) {
		short val = 0;
		//unsigned short event = 0; //XXX extern_qread(&val);
		unsigned int unicode = event->utf8_buf[0] ? BLI_str_utf8_as_unicode(event->utf8_buf) : event->ascii;

		if (keyboarddevice->ConvertBlenderEvent(event->type, event->val, unicode))
			exitrequested = KX_EXIT_REQUEST_BLENDER_ESC;

		/* Coordinate conversion... where
		 * should this really be?
		 */
		if (event->type == MOUSEMOVE) {
			/* Note, not nice! XXX 2.5 event hack */
			val = event->x - ar->winrct.xmin;
			mousedevice->ConvertBlenderEvent(MOUSEX, val, 0);

			val = ar->winy - (event->y - ar->winrct.ymin) - 1;
			mousedevice->ConvertBlenderEvent(MOUSEY, val, 0);
		}
		else {
			mousedevice->ConvertBlenderEvent(event->type, event->val, 0);
		}

		BLI_remlink(&win->queue, event);
		wm_event_free(event);
	}

	if (win != CTX_wm_window(C)) {
		exitrequested= KX_EXIT_REQUEST_OUTSIDE; /* window closed while bge runs */
	}
	return exitrequested;
}


#ifdef WITH_PYTHON
static struct BL_KetsjiNextFrameState {
	class KX_KetsjiEngine* ketsjiengine;
	struct bContext *C;
	struct wmWindow* win;
	struct Scene* scene;
	struct ARegion *ar;
	KX_BlenderKeyboardDevice* keyboarddevice;
	KX_BlenderMouseDevice* mousedevice;
	int draw_letterbox;
} ketsjinextframestate;

static int BL_KetsjiPyNextFrame(void *state0)
{
	BL_KetsjiNextFrameState *state = (BL_KetsjiNextFrameState *) state0;
	return BL_KetsjiNextFrame(
		state->ketsjiengine, 
		state->C, 
		state->win, 
		state->scene, 
		state->ar,
		state->keyboarddevice, 
		state->mousedevice, 
		state->draw_letterbox);
}
#endif


extern "C" void StartKetsjiShell(struct bContext *C, struct ARegion *ar, rcti *cam_frame, int always_use_expand_framing)
{
	/* context values */
	struct wmWindowManager *wm= CTX_wm_manager(C);
	struct wmWindow *win= CTX_wm_window(C);
	struct Scene *startscene= CTX_data_scene(C);
	struct Main* maggie1= CTX_data_main(C);


	RAS_Rect area_rect;
	area_rect.SetLeft(cam_frame->xmin);
	area_rect.SetBottom(cam_frame->ymin);
	area_rect.SetRight(cam_frame->xmax);
	area_rect.SetTop(cam_frame->ymax);

	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	Main* blenderdata = maggie1;

	char* startscenename = startscene->id.name+2;
	char pathname[FILE_MAXDIR+FILE_MAXFILE], oldsce[FILE_MAXDIR+FILE_MAXFILE];
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;

	BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
	BLI_strncpy(oldsce, G.main->name, sizeof(oldsce));
#ifdef WITH_PYTHON
	resetGamePythonPath(); // need this so running a second time wont use an old blendfiles path
	setGamePythonPath(G.main->name);

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();
	
	PyObject *pyGlobalDict = PyDict_New(); /* python utility storage, spans blend file loading */
#endif
	
	bgl::InitExtensions(true);

	// VBO code for derived mesh is not compatible with BGE (couldn't find why), so disable
	int disableVBO = (U.gameflags & USER_DISABLE_VBO);
	U.gameflags |= USER_DISABLE_VBO;

	// Globals to be carried on over blender files
	GlobalSettings gs;
	gs.matmode= startscene->gm.matmode;
	gs.glslflag= startscene->gm.flag;

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
		bool displaylists = (SYS_GetCommandLineInt(syshandle, "displaylists", 0) != 0) && GPU_display_list_support();
#ifdef WITH_PYTHON
		bool nodepwarnings = (SYS_GetCommandLineInt(syshandle, "ignore_deprecation_warnings", 0) != 0);
#endif
		// bool novertexarrays = (SYS_GetCommandLineInt(syshandle, "novertexarrays", 0) != 0);
		bool mouse_state = startscene->gm.flag & GAME_SHOW_MOUSE;
		bool restrictAnimFPS = startscene->gm.flag & GAME_RESTRICT_ANIM_UPDATES;

		short drawtype = v3d->drawtype;
		
		/* we do not support material mode in game engine, force change to texture mode */
		if (drawtype == OB_MATERIAL) drawtype = OB_TEXTURE;
		if (animation_record) usefixed= false; /* override since you don't want to run full-speed for sim recording */

		// create the canvas and rasterizer
		RAS_ICanvas* canvas = new KX_BlenderCanvas(wm, win, area_rect, ar);
		
		// default mouse state set on render panel
		if (mouse_state)
			canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		else
			canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);

		// Setup vsync
		int previous_vsync = canvas->GetSwapInterval();
		if (startscene->gm.vsync == VSYNC_ADAPTIVE)
			canvas->SetSwapInterval(-1);
		else
			canvas->SetSwapInterval((startscene->gm.vsync == VSYNC_ON) ? 1 : 0);

		RAS_IRasterizer* rasterizer = NULL;
		//Don't use displaylists with VBOs
		//If auto starts using VBOs, make sure to check for that here
		if (displaylists && startscene->gm.raster_storage != RAS_STORE_VBO)
			rasterizer = new RAS_ListRasterizer(canvas, true, startscene->gm.raster_storage);
		else
			rasterizer = new RAS_OpenGLRasterizer(canvas, startscene->gm.raster_storage);

		RAS_IRasterizer::MipmapOption mipmapval = rasterizer->GetMipmapping();

		
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
		ketsjiengine->SetRasterizer(rasterizer);
		ketsjiengine->SetUseFixedTime(usefixed);
		ketsjiengine->SetTimingDisplay(frameRate, profile, properties);
		ketsjiengine->SetRestrictAnimationFPS(restrictAnimFPS);
		KX_KetsjiEngine::SetExitKey(ConvertKeyCode(startscene->gm.exitkey));

		//set the global settings (carried over if restart/load new files)
		ketsjiengine->SetGlobalSettings(&gs);

#ifdef WITH_PYTHON
		CValue::SetDeprecationWarnings(nodepwarnings);
#endif

		//lock frame and camera enabled - storing global values
		int tmp_lay= startscene->lay;
		Object *tmp_camera = startscene->camera;

		if (v3d->scenelock==0) {
			startscene->lay= v3d->lay;
			startscene->camera= v3d->camera;
		}

		// some blender stuff
		float camzoom;
		int draw_letterbox = 0;
		
		if (rv3d->persp==RV3D_CAMOB) {
			if (startscene->gm.framing.type == SCE_GAMEFRAMING_BARS) { /* Letterbox */
				camzoom = 1.0f;
				draw_letterbox = 1;
			}
			else {
				camzoom = 1.0f / BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
			}
		}
		else {
			camzoom = 2.0;
		}

		rasterizer->SetDrawingMode(drawtype);
		ketsjiengine->SetCameraZoom(camzoom);
		
		// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
		if (exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME || exitrequested == KX_EXIT_REQUEST_RESTART_GAME)
		{
			exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
			if (bfd) BLO_blendfiledata_free(bfd);
			
			char basedpath[FILE_MAX];
			// base the actuator filename with respect
			// to the original file working directory

			if (exitstring != "")
				BLI_strncpy(basedpath, exitstring.ReadPtr(), sizeof(basedpath));

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
				char temppath[FILE_MAX] = "//";
				BLI_strncpy(temppath + 2, basedpath, FILE_MAX - 2);
				
				BLI_path_abs(temppath, pathname);
				bfd = load_game_data(temppath);
			}
			
			// if we got a loaded blendfile, proceed
			if (bfd)
			{
				blenderdata = bfd->main;
				startscenename = bfd->curscene->id.name + 2;

				if (blenderdata) {
					BLI_strncpy(G.main->name, blenderdata->name, sizeof(G.main->name));
					BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
#ifdef WITH_PYTHON
					setGamePythonPath(G.main->name);
#endif
				}
			}
			// else forget it, we can't find it
			else
			{
				exitrequested = KX_EXIT_REQUEST_QUIT_GAME;
			}
		}

		Scene *scene= bfd ? bfd->curscene : (Scene *)BLI_findstring(&blenderdata->scene, startscenename, offsetof(ID, name) + 2);

		if (scene)
		{
			int startFrame = scene->r.cfra;
			ketsjiengine->SetAnimRecordMode(animation_record, startFrame);
			
			// Quad buffered needs a special window.
			if (scene->gm.stereoflag == STEREO_ENABLED) {
				if (scene->gm.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
					rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) scene->gm.stereomode);

				rasterizer->SetEyeSeparation(scene->gm.eyeseparation);
			}

			rasterizer->SetBackColor(scene->gm.framing.col[0], scene->gm.framing.col[1], scene->gm.framing.col[2], 0.0f);
		}
		
		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			if (rv3d->persp != RV3D_CAMOB)
			{
				ketsjiengine->EnableCameraOverride(startscenename);
				ketsjiengine->SetCameraOverrideUseOrtho((rv3d->persp == RV3D_ORTHO));
				ketsjiengine->SetCameraOverrideProjectionMatrix(MT_CmMatrix4x4(rv3d->winmat));
				ketsjiengine->SetCameraOverrideViewMatrix(MT_CmMatrix4x4(rv3d->viewmat));
				if (rv3d->persp == RV3D_ORTHO)
				{
					ketsjiengine->SetCameraOverrideClipping(v3d->near, v3d->far);
				}
				else
				{
					ketsjiengine->SetCameraOverrideClipping(v3d->near, v3d->far);
				}
				ketsjiengine->SetCameraOverrideLens(v3d->lens);
			}
			
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(blenderdata, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			sceneconverter->addInitFromFrame=false;
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);

			bool usemat = false, useglslmat = false;

			if (GLEW_ARB_multitexture && GLEW_VERSION_1_1)
				usemat = true;

			if (GPU_glsl_support())
				useglslmat = true;
			else if (gs.matmode == GAME_MAT_GLSL)
				usemat = false;

			if (usemat)
				sceneconverter->SetMaterials(true);
			if (useglslmat && (gs.matmode == GAME_MAT_GLSL))
				sceneconverter->SetGLSLMaterials(true);
			if (scene->gm.flag & GAME_NO_MATERIAL_CACHING)
				sceneconverter->SetCacheMaterials(false);
					
			KX_Scene* startscene = new KX_Scene(keyboarddevice,
				mousedevice,
				networkdevice,
				startscenename,
				scene,
				canvas);

#ifdef WITH_PYTHON
			// some python things
			PyObject *gameLogic, *gameLogic_keys;
			setupGamePython(ketsjiengine, startscene, blenderdata, pyGlobalDict, &gameLogic, &gameLogic_keys, 0, NULL);
#endif // WITH_PYTHON

			//initialize Dome Settings
			if (scene->gm.stereoflag == STEREO_DOME)
				ketsjiengine->InitDome(scene->gm.dome.res, scene->gm.dome.mode, scene->gm.dome.angle, scene->gm.dome.resbuf, scene->gm.dome.tilt, scene->gm.dome.warptext);

			// initialize 3D Audio Settings
			AUD_I3DDevice* dev = AUD_get3DDevice();
			if (dev)
			{
				dev->setSpeedOfSound(scene->audio.speed_of_sound);
				dev->setDopplerFactor(scene->audio.doppler_factor);
				dev->setDistanceModel(AUD_DistanceModel(scene->audio.distance_model));
			}

			// from see blender.c:
			// FIXME: this version patching should really be part of the file-reading code,
			// but we still get too many unrelated data-corruption crashes otherwise...
			if (blenderdata->versionfile < 250)
				do_versions_ipos_to_animato(blenderdata);

			if (sceneconverter)
			{
				// convert and add scene
				sceneconverter->ConvertScene(
					startscene,
				    rasterizer,
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
				
#ifdef WITH_PYTHON
				char *python_main = NULL;
				pynextframestate.state = NULL;
				pynextframestate.func = NULL;
				python_main = KX_GetPythonMain(scene);

				// the mainloop
				printf("\nBlender Game Engine Started\n");
				if (python_main) {
					char *python_code = KX_GetPythonCode(blenderdata, python_main);
					if (python_code) {
						ketsjinextframestate.ketsjiengine = ketsjiengine;
						ketsjinextframestate.C = C;
						ketsjinextframestate.win = win;
						ketsjinextframestate.scene = scene;
						ketsjinextframestate.ar = ar;
						ketsjinextframestate.keyboarddevice = keyboarddevice;
						ketsjinextframestate.mousedevice = mousedevice;
						ketsjinextframestate.draw_letterbox = draw_letterbox;
			
						pynextframestate.state = &ketsjinextframestate;
						pynextframestate.func = &BL_KetsjiPyNextFrame;
						printf("Yielding control to Python script '%s'...\n", python_main);
						PyRun_SimpleString(python_code);
						printf("Exit Python script '%s'\n", python_main);
						MEM_freeN(python_code);
					}
				}
				else
#endif  /* WITH_PYTHON */
				{
					while (!exitrequested)
					{
						exitrequested = BL_KetsjiNextFrame(ketsjiengine, C, win, scene, ar, keyboarddevice, mousedevice, draw_letterbox);
					}
				}
				printf("Blender Game Engine Finished\n");
				exitstring = ketsjiengine->GetExitString();
#ifdef WITH_PYTHON
				if (python_main) MEM_freeN(python_main);
#endif  /* WITH_PYTHON */

				gs = *(ketsjiengine->GetGlobalSettings());

				// when exiting the mainloop
#ifdef WITH_PYTHON
				// Clears the dictionary by hand:
				// This prevents, extra references to global variables
				// inside the GameLogic dictionary when the python interpreter is finalized.
				// which allows the scene to safely delete them :)
				// see: (space.c)->start_game
				
				//PyDict_Clear(PyModule_GetDict(gameLogic));
				
				// Keep original items, means python plugins will autocomplete members
				PyObject *gameLogic_keys_new = PyDict_Keys(PyModule_GetDict(gameLogic));
				const Py_ssize_t numitems= PyList_GET_SIZE(gameLogic_keys_new);
				Py_ssize_t listIndex;
				for (listIndex=0; listIndex < numitems; listIndex++) {
					PyObject *item = PyList_GET_ITEM(gameLogic_keys_new, listIndex);
					if (!PySequence_Contains(gameLogic_keys, item)) {
						PyDict_DelItem(	PyModule_GetDict(gameLogic), item);
					}
				}
				Py_DECREF(gameLogic_keys_new);
				gameLogic_keys_new = NULL;
#endif
				ketsjiengine->StopEngine();
#ifdef WITH_PYTHON
				exitGamePythonScripting();
#endif
				networkdevice->Disconnect();
			}
			if (sceneconverter)
			{
				delete sceneconverter;
				sceneconverter = NULL;
			}

#ifdef WITH_PYTHON
			Py_DECREF(gameLogic_keys);
			gameLogic_keys = NULL;
#endif
		}
		//lock frame and camera enabled - restoring global values
		if (v3d->scenelock==0) {
			startscene->lay= tmp_lay;
			startscene->camera= tmp_camera;
		}

		if (exitrequested != KX_EXIT_REQUEST_OUTSIDE)
		{
			// set the cursor back to normal
			canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);

			// set mipmap setting back to its original value
			rasterizer->SetMipmapping(mipmapval);
		}
		
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
		if (canvas)
		{
			canvas->SetSwapInterval(previous_vsync); // Set the swap interval back
			delete canvas;
			canvas = NULL;
		}

		// stop all remaining playing sounds
		AUD_getDevice()->stopAll();
	
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);
	
	if (!disableVBO)
		U.gameflags &= ~USER_DISABLE_VBO;

	if (bfd) BLO_blendfiledata_free(bfd);

	BLI_strncpy(G.main->name, oldsce, sizeof(G.main->name));

#ifdef WITH_PYTHON
	Py_DECREF(pyGlobalDict);

	// Release Python's GIL
	PyGILState_Release(gilstate);
#endif

}
