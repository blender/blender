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
 * Start up of the Blender Player on GHOST.
 */

/** \file gameengine/GamePlayer/ghost/GPG_ghost.cpp
 *  \ingroup player
 */


#include <iostream>
#include <math.h>

#ifdef __linux__
#ifdef __alpha__
#include <signal.h>
#endif /* __alpha__ */
#endif /* __linux__ */

#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"
#include "KX_PythonMain.h"

/**********************************
* Begin Blender include block
**********************************/
#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus
#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_threads.h"
#include "BLI_mempool.h"
#include "BLI_blenlib.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLO_readfile.h"
#include "BLO_runtime.h"

#include "BKE_blender.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_text.h"
#include "BKE_sound.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"
	
// For BLF
#include "BLF_api.h"
#include "BLF_translation.h"
extern int datatoc_bfont_ttf_size;
extern char datatoc_bfont_ttf[];
extern int datatoc_bmonofont_ttf_size;
extern char datatoc_bmonofont_ttf[];

#ifdef __cplusplus
}
#endif // __cplusplus

#include "GPU_draw.h"

/**********************************
* End Blender include block
**********************************/

#include "BL_System.h"
#include "GPG_Application.h"

#include "GHOST_ISystem.h"
#include "RAS_IRasterizer.h"

#include "BKE_main.h"

#include "RNA_define.h"

#ifdef WIN32
#include <windows.h>
#if !defined(DEBUG)
#include <wincon.h>
#endif // !defined(DEBUG)
#endif // WIN32

#ifdef WITH_SDL_DYNLOAD
#  include "sdlew.h"
#endif

const int kMinWindowWidth = 100;
const int kMinWindowHeight = 100;

static void mem_error_cb(const char *errorStr)
{
	fprintf(stderr, "%s", errorStr);
	fflush(stderr);
}

// library.c will only free window managers with a callback function.
// We don't actually use a wmWindowManager, but loading a blendfile
// loads wmWindows, so we need to free those.
static void wm_free(bContext *C, wmWindowManager *wm)
{
	BLI_freelistN(&wm->windows);
}

#ifdef WIN32
typedef enum {
	SCREEN_SAVER_MODE_NONE = 0,
	SCREEN_SAVER_MODE_PREVIEW,
	SCREEN_SAVER_MODE_SAVER,
	SCREEN_SAVER_MODE_CONFIGURATION,
	SCREEN_SAVER_MODE_PASSWORD,
} ScreenSaverMode;

static ScreenSaverMode scr_saver_mode = SCREEN_SAVER_MODE_NONE;
static HWND scr_saver_hwnd = NULL;

static BOOL scr_saver_init(int argc, char **argv) 
{
	scr_saver_mode = SCREEN_SAVER_MODE_NONE;
	scr_saver_hwnd = NULL;
	BOOL ret = false;

	int len = ::strlen(argv[0]);
	if (len > 4 && !::stricmp(".scr", argv[0] + len - 4))
	{
		scr_saver_mode = SCREEN_SAVER_MODE_CONFIGURATION;
		ret = true;
		if (argc >= 2)
		{
			if (argc >= 3)
			{
				scr_saver_hwnd = (HWND) ::atoi(argv[2]);
			}
			if (!::stricmp("/c", argv[1]))
			{
				scr_saver_mode = SCREEN_SAVER_MODE_CONFIGURATION;
				if (scr_saver_hwnd == NULL)
					scr_saver_hwnd = ::GetForegroundWindow();
			}
			else if (!::stricmp("/s", argv[1]))
			{
				scr_saver_mode = SCREEN_SAVER_MODE_SAVER;
			}
			else if (!::stricmp("/a", argv[1]))
			{
				scr_saver_mode = SCREEN_SAVER_MODE_PASSWORD;
			}
			else if (!::stricmp("/p", argv[1])
				 || !::stricmp("/l", argv[1]))
			{
				scr_saver_mode = SCREEN_SAVER_MODE_PREVIEW;
			}
		}
	}
	return ret;
}

#endif /* WIN32 */

static void usage(const char* program, bool isBlenderPlayer)
{
	const char * consoleoption;
	const char * example_filename = "";
	const char * example_pathname = "";

#ifdef _WIN32
	consoleoption = "[-c] ";
#else
	consoleoption = "";
#endif

	if (isBlenderPlayer) {
		example_filename = "filename.blend";
#ifdef _WIN32
		example_pathname = "c:\\";
#else
		example_pathname = "/home/user/";
#endif
	}
	printf("\n");
	printf("usage:   %s [--options] %s\n\n", program, example_filename);
	printf("Available options are: [-w [w h l t]] [-f [fw fh fb ff]] %s[-g gamengineoptions] ", consoleoption);
	printf("[-s stereomode] [-m aasamples]\n");
	printf("Optional parameters must be passed in order.\n");
	printf("Default values are set in the blend file.\n\n");
	printf("  -h: Prints this command summary\n\n");
	printf("  -w: display in a window\n");
	printf("       --Optional parameters--\n"); 
	printf("       w = window width\n");
	printf("       h = window height\n");
	printf("       l = window left coordinate\n");
	printf("       t = window top coordinate\n");
	printf("       Note: To define 'w' or 'h', both must be used.");
	printf("Also, to define 'l' or 't', all four parameters must be used.\n");
	printf("       Example: -w   or  -w 500 300  or  -w 500 300 0 0\n\n");
	printf("  -f: start game in fullscreen mode\n");
	printf("       --Optional parameters--\n");
	printf("       fw = fullscreen mode pixel width    (use 0 to detect automatically)\n");
	printf("       fh = fullscreen mode pixel height   (use 0 to detect automatically)\n");
	printf("       fb = fullscreen mode bits per pixel (default unless set in the blend file: 32)\n");
	printf("       ff = fullscreen mode frequency      (default unless set in the blend file: 60)\n");
	printf("       Note: To define 'fw'' or 'fh'', both must be used.\n");
	printf("       Example: -f  or  -f 1024 768  or  -f 0 0 16  or  -f 1024 728 16 30\n\n");
	printf("  -s: start player in stereoscopy mode (requires 3D capable hardware)\n");
	printf("       stereomode: nostereo         (default unless stereo is set in the blend file)\n");
	printf("                   anaglyph         (Red-Blue glasses)\n");
	printf("                   sidebyside       (Left Right)\n");
	printf("                   syncdoubling     (Above Below)\n");
	printf("                   3dtvtopbottom    (Squashed Top-Bottom for passive glasses)\n");
	printf("                   interlace        (Interlace horizontally)\n");
	printf("                   vinterlace       (Vertical interlace for autostereo display)\n");
	printf("                   hwpageflip       (Quad buffered shutter glasses)\n");
	printf("       Example: -s sidebyside  or  -s vinterlace\n\n");
	printf("  -D: start player in dome mode\n");
	printf("       --Optional parameters--\n");
	printf("       angle    = field of view in degrees\n");
	printf("       tilt     = tilt angle in degrees\n");
	printf("       warpdata = a file to use for warping the image (absolute path)\n");
	printf("       mode: fisheye                (Fisheye)\n");
	printf("             truncatedfront         (Front-Truncated)\n");
	printf("             truncatedrear          (Rear-Truncated)\n");
	printf("             cubemap                (Cube Map)\n");
	printf("             sphericalpanoramic     (Spherical Panoramic)\n");
	printf("       Example: -D  or  -D mode cubemap\n\n");
	printf("  -m: maximum anti-aliasing (eg. 2,4,8,16)\n\n");
	printf("  -i: parent window's ID\n\n");
#ifdef _WIN32
	printf("  -c: keep console window open\n\n");
#endif
	printf("  -d: turn debugging on\n\n");
	printf("  -g: game engine options:\n\n");
	printf("       Name                       Default      Description\n");
	printf("       ------------------------------------------------------------------------\n");
	printf("       fixedtime                      0         \"Enable all frames\"\n");
	printf("       nomipmap                       0         Disable mipmaps\n");
	printf("       show_framerate                 0         Show the frame rate\n");
	printf("       show_properties                0         Show debug properties\n");
	printf("       show_profile                   0         Show profiling information\n");
	printf("       blender_material               0         Enable material settings\n");
	printf("       ignore_deprecation_warnings    1         Ignore deprecation warnings\n");
	printf("\n");
	printf("  - : all arguments after this are ignored, allowing python to access them from sys.argv\n");
	printf("\n");
	printf("example: %s -w 320 200 10 10 -g noaudio %s%s\n", program, example_pathname, example_filename);
	printf("example: %s -g show_framerate = 0 %s%s\n", program, example_pathname, example_filename);
	printf("example: %s -i 232421 -m 16 %s%s\n\n", program, example_pathname, example_filename);
}

static void get_filename(int argc, char **argv, char *filename)
{
#ifdef __APPLE__
	/* On Mac we park the game file (called game.blend) in the application bundle.
	 * The executable is located in the bundle as well.
	 * Therefore, we can locate the game relative to the executable.
	 */
	int srclen = ::strlen(argv[0]);
	int len = 0;
	char *gamefile = NULL;
	
	filename[0] = '\0';

	if (argc > 1) {
		if (BLI_exists(argv[argc-1])) {
			BLI_strncpy(filename, argv[argc-1], FILE_MAX);
		}
	}
	
	srclen -= ::strlen("MacOS/blenderplayer");
	if (srclen > 0) {
		len = srclen + ::strlen("Resources/game.blend"); 
		gamefile = new char [len + 1];
		::strcpy(gamefile, argv[0]);
		::strcpy(gamefile + srclen, "Resources/game.blend");
		//::printf("looking for file: %s\n", filename);
		
		if (BLI_exists(gamefile))
			BLI_strncpy(filename, gamefile, FILE_MAX);

		delete [] gamefile;
	}
	
#else
	filename[0] = '\0';

	if (argc > 1)
		BLI_strncpy(filename, argv[argc-1], FILE_MAX);
#endif // !_APPLE
}

static BlendFileData *load_game_data(const char *progname, char *filename = NULL, char *relativename = NULL)
{
	ReportList reports;
	BlendFileData *bfd = NULL;

	BKE_reports_init(&reports, RPT_STORE);
	
	/* try to load ourself, will only work if we are a runtime */
	if (BLO_is_a_runtime(progname)) {
		bfd= BLO_read_runtime(progname, &reports);
		if (bfd) {
			bfd->type= BLENFILETYPE_RUNTIME;
			BLI_strncpy(bfd->main->name, progname, sizeof(bfd->main->name));
		}
	} else {
		bfd= BLO_read_from_file(progname, &reports);
	}
	
	if (!bfd && filename) {
		bfd = load_game_data(filename);
		if (!bfd) {
			printf("Loading %s failed: ", filename);
			BKE_reports_print(&reports, RPT_ERROR);
		}
	}

	BKE_reports_clear(&reports);
	
	return bfd;
}

static bool GPG_NextFrame(GHOST_ISystem* system, GPG_Application *app, int &exitcode, STR_String &exitstring, GlobalSettings *gs)
{
	bool run = true;
	system->processEvents(false);
	system->dispatchEvents();
	app->EngineNextFrame();
	if ((exitcode = app->getExitRequested())) {
		run = false;
		exitstring = app->getExitString();
		*gs = *app->getGlobalSettings();
	}
	return run;
}

struct GPG_NextFrameState {
	GHOST_ISystem* system;
	GPG_Application *app;
	GlobalSettings *gs;
} gpg_nextframestate;

static int GPG_PyNextFrame(void *state0)
{
	GPG_NextFrameState *state = (GPG_NextFrameState *) state0;
	int exitcode;
	STR_String exitstring;
	bool run = GPG_NextFrame(state->system, state->app, exitcode, exitstring, state->gs);
	if (run) return 0;  
	else {
		if (exitcode) 
			fprintf(stderr, "Exit code %d: %s\n", exitcode, exitstring.ReadPtr());
		return 1;
	}
}

int main(int argc, char** argv)
{
	int i;
	int argc_py_clamped= argc; /* use this so python args can be added after ' - ' */
	bool error = false;
	SYS_SystemHandle syshandle = SYS_GetSystem();
	bool fullScreen = false;
	bool fullScreenParFound = false;
	bool windowParFound = false;
#ifdef WIN32
	bool closeConsole = true;
#endif
	RAS_IRasterizer::StereoMode stereomode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
	bool stereoWindow = false;
	bool stereoParFound = false;
	int stereoFlag = STEREO_NOSTEREO;
	int domeFov = -1;
	int domeTilt = -200;
	int domeMode = 0;
	char* domeWarp = NULL;
	Text *domeText  = NULL;
	int windowLeft = 100;
	int windowTop = 100;
	int windowWidth = 640;
	int windowHeight = 480;
	GHOST_TUns32 fullScreenWidth = 0;
	GHOST_TUns32 fullScreenHeight= 0;
	int fullScreenBpp = 32;
	int fullScreenFrequency = 60;
	GHOST_TEmbedderWindowID parentWindow = 0;
	bool isBlenderPlayer = false; //true when lauching from blender or command line. false for bundled player
	int validArguments=0;
	bool samplesParFound = false;
	GHOST_TUns16 aasamples = 0;
	
#ifdef __linux__
#ifdef __alpha__
	signal (SIGFPE, SIG_IGN);
#endif /* __alpha__ */
#endif /* __linux__ */

#ifdef WITH_SDL_DYNLOAD
	sdlewInit();
#endif

	BLI_init_program_path(argv[0]);
	BLI_temp_dir_init(NULL);
	
	// We don't use threads directly in the BGE, but we need to call this so things like
	// freeing up GPU_Textures works correctly.
	BLI_threadapi_init();

	RNA_init();

	init_nodesystem();
	
	initglobals();

	U.gameflags |= USER_DISABLE_VBO;
	// We load our own G.main, so free the one that initglobals() gives us
	BKE_main_free(G.main);
	G.main = NULL;

	MEM_CacheLimiter_set_disabled(true);
	IMB_init();
	BKE_images_init();
	BKE_modifier_init();
	DAG_init();

#ifdef WITH_FFMPEG
	IMB_ffmpeg_init();
#endif

	// Setup builtin font for BLF (mostly copied from creator.c, wm_init_exit.c and interface_style.c)
	BLF_init(11, U.dpi);
	BLF_lang_init();
	BLF_lang_set("");

	BLF_load_mem("default", (unsigned char*)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
	if (blf_mono_font == -1)
		blf_mono_font = BLF_load_mem_unique("monospace", (unsigned char*)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);

	// Parse command line options
#if defined(DEBUG)
	printf("argv[0] = '%s'\n", argv[0]);
#endif

#ifdef WIN32
	if (scr_saver_init(argc, argv))
	{
		switch (scr_saver_mode)
		{
		case SCREEN_SAVER_MODE_CONFIGURATION:
			MessageBox(scr_saver_hwnd, "This screen saver has no options that you can set", "Screen Saver", MB_OK);
			break;
		case SCREEN_SAVER_MODE_PASSWORD:
			/* This is W95 only, which we currently do not support.
			 * Fall-back to normal screen saver behavior in that case... */
		case SCREEN_SAVER_MODE_SAVER:
			fullScreen = true;
			fullScreenParFound = true;
			break;

		case SCREEN_SAVER_MODE_PREVIEW:
			/* This will actually be handled somewhere below... */
			break;
		}
	}
#endif
	// XXX add the ability to change this values to the command line parsing.
	U.mixbufsize = 2048;
	U.audiodevice = 2;
	U.audiorate = 44100;
	U.audioformat = 0x24;
	U.audiochannels = 2;

	// XXX this one too
	U.anisotropic_filter = 2;
	// enable fast mipmap generation
	U.use_gpu_mipmap = 1;

	sound_init_once();

	set_free_windowmanager_cb(wm_free);

	/* if running blenderplayer the last argument can't be parsed since it has to be the filename. else it is bundled */
	isBlenderPlayer = !BLO_is_a_runtime(argv[0]);
	if (isBlenderPlayer)
		validArguments = argc - 1;
	else
		validArguments = argc;


	/* Parsing command line arguments (can be set from WM_OT_blenderplayer_start) */
#if defined(DEBUG)
		printf("Parsing command line arguments...\n");
		printf("Num of arguments is: %i\n", validArguments-1); //-1 because i starts at 1
#endif

	for (i = 1; (i < validArguments) && !error 
#ifdef WIN32
		&& scr_saver_mode == SCREEN_SAVER_MODE_NONE
#endif
		;)

	{
#if defined(DEBUG)
		printf("argv[%d] = '%s'\n", i, argv[i]);
#endif
		if (argv[i][0] == '-')
		{
			/* ignore all args after " - ", allow python to have own args */
			if (argv[i][1]=='\0') {
				argc_py_clamped= i;
				break;
			}
			
			switch (argv[i][1])
			{
			case 'g': //game engine options (show_framerate, fixedtime, etc)
			{
				i++;
				if (i <= validArguments)
				{
					char* paramname = argv[i];
					// Check for single value versus assignment
					if (i+1 <= validArguments && (*(argv[i+1]) == '='))
					{
						i++;
						if (i + 1 <= validArguments)
						{
							i++;
							// Assignment
							SYS_WriteCommandLineInt(syshandle, paramname, atoi(argv[i]));
							SYS_WriteCommandLineFloat(syshandle, paramname, atof(argv[i]));
							SYS_WriteCommandLineString(syshandle, paramname, argv[i]);
#if defined(DEBUG)
							printf("%s = '%s'\n", paramname, argv[i]);
#endif
							i++;
						}
						else
						{
							error = true;
							printf("error: argument assignment %s without value.\n", paramname);
						}
					}
					else
					{
//						SYS_WriteCommandLineInt(syshandle, argv[i++], 1);
					}
				}
				break;
			}
			case 'd': //debug on
			{
				i++;
				G.debug |= G_DEBUG;
				MEM_set_memory_debug();
#ifdef DEBUG
				BLI_mempool_set_memory_debug();
#endif
				break;
			}
			case 'f': //fullscreen mode
			{
				i++;
				fullScreen = true;
				fullScreenParFound = true;
				if ((i + 2) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
				{
					fullScreenWidth = atoi(argv[i++]);
					fullScreenHeight = atoi(argv[i++]);
					if ((i + 1) <= validArguments && argv[i][0] != '-')
					{
						fullScreenBpp = atoi(argv[i++]);
						if ((i + 1) <= validArguments && argv[i][0] != '-')
							fullScreenFrequency = atoi(argv[i++]);
					}
				}
				else if ((i + 1) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
				{
					error = true;
					printf("error: to define fullscreen width or height, both options must be used.\n");
				}
				break;
			}
			case 'w': //display in a window
			{
				i++;
				fullScreen = false;
				windowParFound = true;

				// Parse window position and size options
				if ((i + 2) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
				{
					windowWidth = atoi(argv[i++]);
					windowHeight = atoi(argv[i++]);

					if ((i + 2) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
					{
						windowLeft = atoi(argv[i++]);
						windowTop = atoi(argv[i++]);
					}
					else if ((i + 1) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
					{
						error = true;
						printf("error: to define the window left or right coordinates, both options must be used.\n");
					}
				}
				else if ((i + 1) <= validArguments && argv[i][0] != '-' && argv[i+1][0] != '-')
				{
					error = true;
					printf("error: to define the window's width or height, both options must be used.\n");
				}
				break;
			}
			case 'h': //display help
			{
				usage(argv[0], isBlenderPlayer);
				return 0;
				break;
			}
			case 'i': //parent window ID
			{
				i++;
				if ( (i + 1) <= validArguments )
					parentWindow = atoi(argv[i++]);
				else {
					error = true;
					printf("error: too few options for parent window argument.\n");
				}
#if defined(DEBUG)
				printf("XWindows ID = %d\n", parentWindow);
#endif // defined(DEBUG)
				break;
			}
			case 'm': //maximum anti-aliasing (eg. 2,4,8,16)
			{
				i++;
				samplesParFound = true;
				if ((i+1) <= validArguments )
					aasamples = atoi(argv[i++]);
				else
				{
					error = true;
					printf("error: No argument supplied for -m");
				}
				break;
			}
			case 'c': //keep console (windows only)
			{
				i++;
#ifdef WIN32
				closeConsole = false;
#endif
				break;
			}
			case 's': //stereo mode
			{
				i++;
				if ((i + 1) <= validArguments)
				{
					stereoParFound = true;
					stereoFlag = STEREO_ENABLED;

					if (!strcmp(argv[i], "nostereo"))  // may not be redundant if the file has different setting
					{
						stereomode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
						stereoFlag = STEREO_NOSTEREO;
					}

					// only the hardware pageflip method needs a stereo window
					else if (!strcmp(argv[i], "hwpageflip")) {
						stereomode = RAS_IRasterizer::RAS_STEREO_QUADBUFFERED;
						stereoWindow = true;
					}
					else if (!strcmp(argv[i], "syncdoubling"))
						stereomode = RAS_IRasterizer::RAS_STEREO_ABOVEBELOW;

					else if (!strcmp(argv[i], "3dtvtopbottom"))
						stereomode = RAS_IRasterizer::RAS_STEREO_3DTVTOPBOTTOM;

					else if (!strcmp(argv[i], "anaglyph"))
						stereomode = RAS_IRasterizer::RAS_STEREO_ANAGLYPH;

					else if (!strcmp(argv[i], "sidebyside"))
						stereomode = RAS_IRasterizer::RAS_STEREO_SIDEBYSIDE;

					else if (!strcmp(argv[i], "interlace"))
						stereomode = RAS_IRasterizer::RAS_STEREO_INTERLACED;

					else if (!strcmp(argv[i], "vinterlace"))
						stereomode = RAS_IRasterizer::RAS_STEREO_VINTERLACE;

#if 0
//					// future stuff
//					else if (!strcmp(argv[i], "stencil")
//						stereomode = RAS_STEREO_STENCIL;
#endif
					else
					{
						error = true;
						printf("error: stereomode '%s' unrecognized.\n", argv[i]);
					}

					i++;
				}
				else
				{
					error = true;
					printf("error: too few options for stereo argument.\n");
				}
				break;
			}
			case 'D': //dome mode
			{
				stereoFlag = STEREO_DOME;
				stereomode = RAS_IRasterizer::RAS_STEREO_DOME;
				i++;
				if ((i + 1) <= validArguments)
				{
					if (!strcmp(argv[i], "angle")) {
						i++;
						domeFov = atoi(argv[i++]);
					}
					if (!strcmp(argv[i], "tilt")) {
						i++;
						domeTilt = atoi(argv[i++]);
					}
					if (!strcmp(argv[i], "warpdata")) {
						i++;
						domeWarp = argv[i++];
					}
					if (!strcmp(argv[i], "mode")) {
						i++;
						if (!strcmp(argv[i], "fisheye"))
							domeMode = DOME_FISHEYE;
							
						else if (!strcmp(argv[i], "truncatedfront"))
							domeMode = DOME_TRUNCATED_FRONT;
							
						else if (!strcmp(argv[i], "truncatedrear"))
							domeMode = DOME_TRUNCATED_REAR;
							
						else if (!strcmp(argv[i], "cubemap"))
							domeMode = DOME_ENVMAP;
							
						else if (!strcmp(argv[i], "sphericalpanoramic"))
							domeMode = DOME_PANORAM_SPH;

						else
							printf("error: %s is not a valid dome mode.\n", argv[i]);
					}
					i++;
				}
				break;
			}
			default:  //not recognized
			{
				printf("Unknown argument: %s\n", argv[i++]);
				break;
			}
			}
		}
		else
		{
			i++;
		}
	}

	if ((windowWidth < kMinWindowWidth) || (windowHeight < kMinWindowHeight))
	{
		error = true;
		printf("error: window size too small.\n");
	}
	
	if (error )
	{
		usage(argv[0], isBlenderPlayer);
		return 0;
	}

#ifdef WIN32
	if (scr_saver_mode != SCREEN_SAVER_MODE_CONFIGURATION)
#endif
	{
		// Create the system
		if (GHOST_ISystem::createSystem() == GHOST_kSuccess) {
			GHOST_ISystem* system = GHOST_ISystem::getSystem();
			assertd(system);
			
			if (!fullScreenWidth || !fullScreenHeight)
				system->getMainDisplayDimensions(fullScreenWidth, fullScreenHeight);
			// process first batch of events. If the user
			// drops a file on top off the blenderplayer icon, we
			// receive an event with the filename
			
			system->processEvents(0);
			
			// this bracket is needed for app (see below) to get out
			// of scope before GHOST_ISystem::disposeSystem() is called.
			{
				int exitcode = KX_EXIT_REQUEST_NO_REQUEST;
				STR_String exitstring = "";
				GPG_Application app(system);
				bool firstTimeRunning = true;
				char filename[FILE_MAX];
				char pathname[FILE_MAX];
				char *titlename;

				get_filename(argc_py_clamped, argv, filename);
				if (filename[0])
					BLI_path_cwd(filename);
				

				// fill the GlobalSettings with the first scene files
				// those may change during the game and persist after using Game Actuator
				GlobalSettings gs;

				do {
					// Read the Blender file
					BlendFileData *bfd;
					
					// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
					if (exitcode == KX_EXIT_REQUEST_START_OTHER_GAME)
					{
						char basedpath[FILE_MAX];
						
						// base the actuator filename relative to the last file
						BLI_strncpy(basedpath, exitstring.Ptr(), sizeof(basedpath));
						BLI_path_abs(basedpath, pathname);
						
						bfd = load_game_data(basedpath);

						if (!bfd) {
							// just add "//" in front of it
							char temppath[FILE_MAX] = "//";
							BLI_strncpy(temppath + 2, basedpath, FILE_MAX - 2);

							BLI_path_abs(temppath, pathname);
							bfd = load_game_data(temppath);
						}
					}
					else {
						bfd = load_game_data(BLI_program_path(), filename[0]? filename: NULL);
					}

#if defined(DEBUG)
					printf("Game data loaded from %s\n", filename);
#endif
					
					if (!bfd) {
						usage(argv[0], isBlenderPlayer);
						error = true;
						exitcode = KX_EXIT_REQUEST_QUIT_GAME;
					}
					else {
						/* Setting options according to the blend file if not overriden in the command line */
#ifdef WIN32
#if !defined(DEBUG)
						if (closeConsole) {
							system->toggleConsole(0); // Close a console window
						}
#endif // !defined(DEBUG)
#endif // WIN32
						Main *maggie = bfd->main;
						Scene *scene = bfd->curscene;
						G.main = maggie;

						if (firstTimeRunning) {
							G.fileflags  = bfd->fileflags;

							gs.matmode= scene->gm.matmode;
							gs.glslflag= scene->gm.flag;
						}

						//Seg Fault; icon.c gIcons == 0
						BKE_icons_init(1);
						
						titlename = maggie->name;
						
						// Check whether the game should be displayed full-screen
						if ((!fullScreenParFound) && (!windowParFound)) {
							// Only use file settings when command line did not override
							if ((scene->gm.playerflag & GAME_PLAYER_FULLSCREEN)) {
								//printf("fullscreen option found in Blender file\n");
								fullScreen = true;
								fullScreenWidth= scene->gm.xplay;
								fullScreenHeight= scene->gm.yplay;
								fullScreenFrequency= scene->gm.freqplay;
								fullScreenBpp = scene->gm.depth;
							}
							else
							{
								fullScreen = false;
								windowWidth = scene->gm.xplay;
								windowHeight = scene->gm.yplay;
							}
						}
						
						
						// Check whether the game should be displayed in stereo (dome included)
						if (!stereoParFound) {
							// Only use file settings when command line did not override
							if (scene->gm.stereoflag == STEREO_ENABLED) {
								stereomode = (RAS_IRasterizer::StereoMode) scene->gm.stereomode;
								if (stereomode == RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
									stereoWindow = true;
							}
						}
						else {
							scene->gm.stereoflag = STEREO_ENABLED;
						}

						if (!samplesParFound)
							aasamples = scene->gm.aasamples;

						// Dome specific settings
						if (stereoFlag == STEREO_DOME) {
							stereomode = RAS_IRasterizer::RAS_STEREO_DOME;
							scene->gm.stereoflag = STEREO_DOME;
							if (domeFov > 89)
								scene->gm.dome.angle = domeFov;
							if (domeTilt > -180)
								scene->gm.dome.tilt = domeTilt;
							if (domeMode > 0)
								scene->gm.dome.mode = domeMode;
							if (domeWarp) {
								//XXX to do: convert relative to absolute path
								domeText= BKE_text_load(G.main, domeWarp, "");
								if (!domeText)
									printf("error: invalid warpdata text file - %s\n", domeWarp);
								else
									scene->gm.dome.warptext = domeText;
							}
						}
						
						//					GPG_Application app (system, maggie, startscenename);
						app.SetGameEngineData(maggie, scene, &gs, argc, argv); /* this argc cant be argc_py_clamped, since python uses it */
						BLI_strncpy(pathname, maggie->name, sizeof(pathname));
						if (G.main != maggie) {
							BLI_strncpy(G.main->name, maggie->name, sizeof(G.main->name));
						}
#ifdef WITH_PYTHON
						setGamePythonPath(G.main->name);
#endif
						if (firstTimeRunning) {
							firstTimeRunning = false;

							if (fullScreen) {
#ifdef WIN32
								if (scr_saver_mode == SCREEN_SAVER_MODE_SAVER)
								{
									app.startScreenSaverFullScreen(fullScreenWidth, fullScreenHeight, fullScreenBpp, fullScreenFrequency,
									                               stereoWindow, stereomode, aasamples);
								}
								else
#endif
								{
									app.startFullScreen(fullScreenWidth, fullScreenHeight, fullScreenBpp, fullScreenFrequency,
									                    stereoWindow, stereomode, aasamples, (scene->gm.playerflag & GAME_PLAYER_DESKTOP_RESOLUTION));
								}
							}
							else
							{
#ifdef __APPLE__
								// on Mac's we'll show the executable name instead of the 'game.blend' name
								char tempname[1024], *appstring;
								::strcpy(tempname, titlename);
								
								appstring = strstr(tempname, ".app/");
								if (appstring) {
									appstring[2] = 0;
									titlename = &tempname[0];
								}
#endif
								// Strip the path so that we have the name of the game file
								STR_String path = titlename;
#ifndef WIN32
								vector<STR_String> parts = path.Explode('/');
#else  // WIN32
								vector<STR_String> parts = path.Explode('\\');
#endif // WIN32                        
								STR_String title;
								if (parts.size()) {
									title = parts[parts.size()-1];
									parts = title.Explode('.');
									if (parts.size() > 1)
									{
										title = parts[0];
									}
								}
								else {
									title = "blenderplayer";
								}
#ifdef WIN32
								if (scr_saver_mode == SCREEN_SAVER_MODE_PREVIEW)
								{
									app.startScreenSaverPreview(scr_saver_hwnd, stereoWindow, stereomode, aasamples);
								}
								else
#endif
								{
									if (parentWindow != 0)
										app.startEmbeddedWindow(title, parentWindow, stereoWindow, stereomode, aasamples);
									else
										app.startWindow(title, windowLeft, windowTop, windowWidth, windowHeight,
										                stereoWindow, stereomode, aasamples);

									if (SYS_GetCommandLineInt(syshandle, "nomipmap", 0)) {
										GPU_set_mipmap(0);
									}

									GPU_set_anisotropic(U.anisotropic_filter);
									GPU_set_gpu_mipmapping(U.use_gpu_mipmap);
								}
							}
						}
						else {
							app.StartGameEngine(stereomode);
							exitcode = KX_EXIT_REQUEST_NO_REQUEST;
						}
						
						// Add the application as event consumer
						system->addEventConsumer(&app);
						
						// Enter main loop
						bool run = true;
						char *python_main = NULL;
						pynextframestate.state = NULL;
						pynextframestate.func = NULL;
#ifdef WITH_PYTHON
						python_main = KX_GetPythonMain(scene);
#endif // WITH_PYTHON
						if (python_main) {
							char *python_code = KX_GetPythonCode(maggie, python_main);
							if (python_code) {
#ifdef WITH_PYTHON
								gpg_nextframestate.system = system;
								gpg_nextframestate.app = &app;
								gpg_nextframestate.gs = &gs;
								pynextframestate.state = &gpg_nextframestate;
								pynextframestate.func = &GPG_PyNextFrame;

								printf("Yielding control to Python script '%s'...\n", python_main);
								PyRun_SimpleString(python_code);
								printf("Exit Python script '%s'\n", python_main);
#endif // WITH_PYTHON
								MEM_freeN(python_code);
							}
							else {
								fprintf(stderr, "ERROR: cannot yield control to Python: no Python text data block named '%s'\n", python_main);
							}
						}
						else {
							while (run) {
								run = GPG_NextFrame(system, &app, exitcode, exitstring, &gs);
							}
						}
						app.StopGameEngine();

						/* 'app' is freed automatic when out of scope.
						 * removal is needed else the system will free an already freed value */
						system->removeEventConsumer(&app);

						BLO_blendfiledata_free(bfd);
						/* G.main == bfd->main, it gets referenced in free_nodesystem so we can't have a dangling pointer */
						G.main = NULL;
						if (python_main) MEM_freeN(python_main);
					}
				} while (exitcode == KX_EXIT_REQUEST_RESTART_GAME || exitcode == KX_EXIT_REQUEST_START_OTHER_GAME);
			}

			// Seg Fault; icon.c gIcons == 0
			BKE_icons_free();

			// Dispose the system
			GHOST_ISystem::disposeSystem();
		}
		else {
			error = true;
			printf("error: couldn't create a system.\n");
		}
	}

	/* refer to WM_exit_ext() and free_blender(),
	 * these are not called in the player but we need to match some of there behavior here,
	 * if the order of function calls or blenders state isn't matching that of blender proper,
	 * we may get troubles later on */

	free_nodesystem();

	// Cleanup
	RNA_exit();
	BLF_exit();

#ifdef WITH_INTERNATIONAL
	BLF_free_unifont();
	BLF_free_unifont_mono();
	BLF_lang_free();
#endif

	IMB_exit();
	BKE_images_exit();
	DAG_exit();
	IMB_moviecache_destruct();

	SYS_DeleteSystem(syshandle);

	int totblock= MEM_get_memory_blocks_in_use();
	if (totblock!=0) {
		printf("Error Totblock: %d\n",totblock);
		MEM_set_error_callback(mem_error_cb);
		MEM_printmemlist();
	}

	BLI_temp_dir_session_purge();

	return error ? -1 : 0;
}
