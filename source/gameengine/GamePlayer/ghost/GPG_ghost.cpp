/**
* $Id$
*
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
* Start up of the Blender Player on GHOST.
*/

#include <iostream>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __linux__
#ifdef __alpha__
#include <signal.h>
#endif /* __alpha__ */
#endif /* __linux__ */

#ifdef __APPLE__
// Can't use Carbon right now because of double defined type ID (In Carbon.h and DNA_ID.h, sigh)
//#include <Carbon/Carbon.h>
//#include <CFBundle.h>
#endif // __APPLE__
#include "GEN_messaging.h"
#include "KX_KetsjiEngine.h"

/**********************************
* Begin Blender include block
**********************************/
#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus
	
#include "BLI_blenlib.h"
#include "DNA_scene_types.h"
#include "BLO_readfile.h"
#include "BLO_readblenfile.h"
	
	int GHOST_HACK_getFirstFile(char buf[]);
	
#ifdef __cplusplus
}
#endif // __cplusplus
/**********************************
* End Blender include block
**********************************/

#include "SYS_System.h"
#include "GPG_Application.h"
#include "GPC_PolygonMaterial.h"

#include "GHOST_ISystem.h"
#include "RAS_IRasterizer.h"

#include "BKE_main.h"
#include "BKE_utildefines.h"

#ifdef WIN32
#ifdef NDEBUG
#include <windows.h>
#include <wincon.h>
#endif // NDEBUG
#endif // WIN32

const int kMinWindowWidth = 100;
const int kMinWindowHeight = 100;

char bprogname[FILE_MAXDIR+FILE_MAXFILE];

void usage(char* program)
{
	char * consoleoption;
#ifdef _WIN32
	consoleoption = "-c ";
#else
	consoleoption = "";
#endif
	
	printf("usage:   %s [-w [-p l t w h]] %s[-g gamengineoptions] "
		"[-s stereomode] filename.blend\n", program, consoleoption);
	printf("  -w: display in a window\n");
	printf("  -p: specify window position\n");
	printf("       l = window left coordinate\n");
	printf("       t = window top coordinate\n");
	printf("       w = window width\n");
	printf("       h = window height\n");
/*	printf("  -f: start game in full screen mode\n");
	printf("       fw = full screen mode pixel width\n");
	printf("       fh = full screen mode pixel height\n");
	printf("       fb = full screen mode bits per pixel\n");
	printf("       ff = full screen mode frequency\n"); */
	printf("  -s: start player in stereo\n");
	printf("       stereomode = hwpageflip or syncdoubling depending on the type of stereo you want\n");
#ifdef _WIN32
	printf("  -c: keep console window open\n");
#endif
	printf("\n");
	printf("example: %s -p 10 10 320 200 -g noaudio c:\\loadtest.blend\n", program);
}

char *get_filename(int argc, char **argv) {
#ifdef __APPLE__
/* On Mac we park the game file (called game.blend) in the application bundle.
* The executable is located in the bundle as well.
* Therefore, we can locate the game relative to the executable.
	*/
	int srclen = ::strlen(argv[0]);
	int len = 0;
	char *filename = NULL;
	
	if (argc > 1) {
		if (BLI_exists(argv[argc-1])) {
			len = ::strlen(argv[argc-1]);
			filename = new char [len + 1];
			::strcpy(filename, argv[argc-1]);
			return(filename);
		}
		if (::strncmp(argv[argc-1], "-psn_", 5)==0) {
			static char firstfilebuf[512];
			if (GHOST_HACK_getFirstFile(firstfilebuf)) {
				len = ::strlen(firstfilebuf);
				filename = new char [len + 1];
				::strcpy(filename, firstfilebuf);
				return(filename);
			}
		}                        
	}
	
	srclen -= ::strlen("MacOS/blenderplayer");
	if (srclen > 0) {
		len = srclen + ::strlen("Resources/game.blend"); 
		filename = new char [len + 1];
		::strcpy(filename, argv[0]);
		::strcpy(filename + srclen, "Resources/game.blend");
		//::printf("looking for file: %s\n", filename);
		
		if (BLI_exists(filename)) {
			return (filename);
		}
	}
	
	return(NULL);
#else
	return (argc>1)?argv[argc-1]:NULL;
#endif // !_APPLE
}

static BlendFileData *load_game_data(char *progname, char *filename = NULL) {
	BlendReadError error;
	BlendFileData *bfd = NULL;
	
	/* try to load ourself, will only work if we are a runtime */
	if (blo_is_a_runtime(progname)) {
		bfd= blo_read_runtime(progname, &error);
		if (bfd) {
			bfd->type= BLENFILETYPE_RUNTIME;
			strcpy(bfd->main->name, progname);
		}
	} else {
		bfd= BLO_read_from_file(progname, &error);
	}
 	
	/*
	if (bfd && bfd->type == BLENFILETYPE_BLEND) {
		BLO_blendfiledata_free(bfd);
		bfd = NULL;
		error = BRE_NOT_A_PUBFILE;
	}
	*/
	
	if (!bfd && filename) {
		bfd = load_game_data(filename);
		if (!bfd) {
			printf("Loading %s failed: %s\n", filename, BLO_bre_as_string(error));
		}
	}
	
	return bfd;
}

int main(int argc, char** argv)
{
	int i;
	bool error = false;
	SYS_SystemHandle syshandle = SYS_GetSystem();
	bool fullScreen = true;
	bool fullScreenParFound = true;
	bool windowParFound = false;
	bool closeConsole = true;
	int stereomode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
	bool stereoWindow = false;
	bool stereoParFound = false;
	int windowLeft = 100;
	int windowTop = 100;
	int windowWidth = 640;
	int windowHeight = 480;
	GHOST_TUns32 fullScreenWidth = 0;
	GHOST_TUns32 fullScreenHeight= 0;
	int fullScreenBpp = 16;
	int fullScreenFrequency = 60;
	
#ifdef __linux__
#ifdef __alpha__
	signal (SIGFPE, SIG_IGN);
#endif /* __alpha__ */
#endif /* __linux__ */
	BLI_where_am_i(bprogname, argv[0]);
	
#ifdef __APPLE__
    // Can't use Carbon right now because of double defined type ID (In Carbon.h and DNA_ID.h, sigh)
    /*
    IBNibRef 		nibRef;
    WindowRef 		window;
    OSStatus		err;
	
	  // Create a Nib reference passing the name of the nib file (without the .nib extension)
	  // CreateNibReference only searches into the application bundle.
	  err = ::CreateNibReference(CFSTR("main"), &nibRef);
	  if (err) return -1;
	  
		// Once the nib reference is created, set the menu bar. "MainMenu" is the name of the menu bar
		// object. This name is set in InterfaceBuilder when the nib is created.
		err = ::SetMenuBarFromNib(nibRef, CFSTR("MenuBar"));
		if (err) return -1;
		
		  // We don't need the nib reference anymore.
		  ::DisposeNibReference(nibRef);
    */
#endif // __APPLE__
	
	GEN_init_messaging_system();
	
	// Parse command line options
#ifndef NDEBUG
	printf("argv[0] = '%s'\n", argv[0]);
#endif
	for (i = 1; (i < argc) && !error; i++)
	{
#ifndef NDEBUG
		printf("argv[%d] = '%s'\n", i, argv[i]);
#endif
		
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'g':
				// Parse game options
				{
					i++;
					if (i < argc)
					{
						char* paramname = argv[i];
						// Check for single value versus assignment
						if (i+1 < argc && (*(argv[i+1]) == '='))
						{
							i++;
							if (i + 1 < argc)
							{
								i++;
								// Assignment
								SYS_WriteCommandLineString(syshandle, paramname, argv[i]);
							}
							else
							{
								error = true;
								printf("error: argument assignment %s without value.\n", paramname);
							}
						}
						else
						{
							SYS_WriteCommandLineInt(syshandle, argv[i], 1);
						}
					}
				}
				break;
				
			case 'p':
				// Parse window position and size options
				if (argv[i][2] == 0) {
					i++;
					if ((i + 4) < argc)
					{
						windowLeft = atoi(argv[i++]);
						windowTop = atoi(argv[i++]);
						windowWidth = atoi(argv[i++]);
						windowHeight = atoi(argv[i]);
						windowParFound = true;
					}
					else
					{
						error = true;
						printf("error: too few options for window argument.\n");
					}
				}
				break;
				
			case 'w':
				// Parse window position and size options
				{
					fullScreen = false;
						fullScreenParFound = true;
					i++;
				}
				break;
			case 'c':
				i++;
				closeConsole = false;
				break;
			case 's':  // stereo
				i++;
				if ((i + 1) < argc)
				{
					if(!strcmp(argv[i], "nostereo"))  // ok, redundant but clear
						stereomode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
					
					// only the hardware pageflip method needs a stereo window
					if(!strcmp(argv[i], "hwpageflip")) {
						stereomode = RAS_IRasterizer::RAS_STEREO_QUADBUFFERED;
						stereoWindow = true;
					}
					if(!strcmp(argv[i], "syncdoubling"))
						stereomode = RAS_IRasterizer::RAS_STEREO_ABOVEBELOW;
#if 0
					// future stuff
					if(strcmp(argv[i], "stencil")
						stereomode = RAS_STEREO_STENCIL;
#endif
					
					i++;
					stereoParFound = true;
				}
				else
				{
					error = true;
					printf("error: too few options for stereo argument.\n");
				}
				break;
			}
		}
		else
		{
		}
	}
	
	if ((windowWidth < kMinWindowWidth) || (windowHeight < kMinWindowHeight))
	{
		error = true;
		printf("error: window size too small.\n");
	}
	
	if (error)
	{
		usage(argv[0]);
	}
	else
	{
#ifdef __APPLE__
		//SYS_WriteCommandLineInt(syshandle, "show_framerate", 1);
		SYS_WriteCommandLineInt(syshandle, "nomipmap", 1);
		//fullScreen = false;		// Can't use full screen
#endif
		if (SYS_GetCommandLineInt(syshandle, "nomipmap", 0))
		{
			GPC_PolygonMaterial::SetMipMappingEnabled(0);
		}
		
		// Create the system
		if (GHOST_ISystem::createSystem() == GHOST_kSuccess)
		{
			GHOST_ISystem* system = GHOST_ISystem::getSystem();
			assertd(system);
			
			system->getMainDisplayDimensions(fullScreenWidth, fullScreenHeight);
			// process first batch of events. If the user
			// drops a file on top off the blenderplayer icon, we 
			// recieve an event with the filename
			
			system->processEvents(0);
			
			// this bracket is needed for app (see below) to get out
			// of scope before GHOST_ISystem::disposeSystem() is called.
			{
				int exitcode = KX_EXIT_REQUEST_NO_REQUEST;
				STR_String exitstring = "";
				GPG_Application app(system, NULL, exitstring);
				bool firstTimeRunning = true;
				
				do
				{
					// Read the Blender file
					char *filename = get_filename(argc, argv);
					char *titlename;
					char pathname[160];
					BlendFileData *bfd;
					
					// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
					if (exitcode == KX_EXIT_REQUEST_START_OTHER_GAME)
					{
						char basedpath[160];
						
						// base the actuator filename with respect
						// to the original file working directory
						strcpy(basedpath, exitstring.Ptr());
						BLI_convertstringcode(basedpath, pathname, 0);
						
						bfd = load_game_data(basedpath);
					}
					else
					{
						bfd = load_game_data(argv[0], filename);
					}
					
					//::printf("game data loaded from %s\n", filename);
					
					if (!bfd) {
						usage(argv[0]);
						error = true;
						exitcode = KX_EXIT_REQUEST_QUIT_GAME;
					} 
					else 
					{
#ifdef WIN32
#ifdef NDEBUG
						if (closeConsole)
						{
							::FreeConsole();    // Close a console window
						}
#endif // NDEBUG
#endif // WIN32
						Main *maggie = bfd->main;
						Scene *scene = bfd->curscene;
						strcpy (pathname, maggie->name);
						char *startscenename = scene->id.name + 2;
						
						titlename = maggie->name;
						
						// Check whether the game should be displayed full-screen
						if ((!fullScreenParFound) && (!windowParFound))
						{
							// Only use file settings when command line did not override
							if (scene->r.fullscreen) {
								//printf("fullscreen option found in Blender file\n");
								fullScreen = true;
								fullScreenWidth= scene->r.xplay;
								fullScreenHeight= scene->r.yplay;
								fullScreenFrequency= scene->r.freqplay;
								fullScreenBpp = scene->r.depth;
							}
							else
							{
								fullScreen = false;
								windowWidth = scene->r.xplay;
								windowHeight = scene->r.yplay;
							}
						}
						
						
						// Check whether the game should be displayed in stereo
						if (!stereoParFound)
						{
							if(scene->r.stereomode == RAS_IRasterizer::RAS_STEREO_NOSTEREO)  // ok, redundant but clear
								stereomode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
							
							// only the hardware pageflip method needs a stereo window
							if(scene->r.stereomode == RAS_IRasterizer::RAS_STEREO_QUADBUFFERED) {
								stereomode = RAS_IRasterizer::RAS_STEREO_QUADBUFFERED;
								stereoWindow = true;
							}
							if(scene->r.stereomode == RAS_IRasterizer::RAS_STEREO_ABOVEBELOW)
								stereomode = RAS_IRasterizer::RAS_STEREO_ABOVEBELOW;
#if 0
							// future stuff
							if(scene->r.stereomode == RAS_IRasterizer::RAS_STEREO_STENCIL)
								stereomode = RAS_STEREO_STENCIL;
#endif
						}
						
						//					GPG_Application app (system, maggie, startscenename);
						app.SetGameEngineData(maggie, startscenename);
						
						if (firstTimeRunning)
						{
							firstTimeRunning = false;
							
							if (fullScreen)
							{
								app.startFullScreen(fullScreenWidth, fullScreenHeight, fullScreenBpp, fullScreenFrequency,
									stereoWindow, stereomode);
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
								if (parts.size())
								{
									title = parts[parts.size()-1];
									parts = title.Explode('.');
									if (parts.size() > 1)
									{
										title = parts[0];
									}
								}
								else
								{
									title = "blenderplayer";
								}
								app.startWindow(title, windowLeft, windowTop, windowWidth, windowHeight,
									stereoWindow, stereomode);
							}
						}
						else
						{
							app.StartGameEngine(stereomode);
							exitcode = KX_EXIT_REQUEST_NO_REQUEST;
						}
						
						// Add the application as event consumer
						system->addEventConsumer(&app);
						
						// Enter main loop
						bool run = true;
						while (run)
						{
							system->processEvents(false);
							system->dispatchEvents();
							if ((exitcode = app.getExitRequested()))
							{
								run = false;
								exitstring = app.getExitString();
							}
						}
						app.StopGameEngine();
						BLO_blendfiledata_free(bfd);
						
#ifdef __APPLE__
						if (filename) {
							delete [] filename;
						}
#endif // __APPLE__
					}
				} while (exitcode == KX_EXIT_REQUEST_RESTART_GAME || exitcode == KX_EXIT_REQUEST_START_OTHER_GAME);
			}
			// Dispose the system
			GHOST_ISystem::disposeSystem();
		} else {
			error = true;
			printf("error: couldn't create a system.\n");
		}
	}
	
	return error ? -1 : 0;
}


