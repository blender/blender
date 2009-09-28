/*
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <string.h>


/* for setuid / getuid */
#ifdef __sgi
#include <sys/types.h>
#include <unistd.h>
#endif

/* This little block needed for linking to Blender... */

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "GEN_messaging.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"

#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_node.h"
#include "BKE_report.h"

#include "IMB_imbuf.h"	// for quicktime_init

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "RE_pipeline.h"

//XXX #include "playanim_ext.h"
#include "ED_datafiles.h"
#include "UI_interface.h"

#include "WM_api.h"

#include "RNA_define.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

/* for passing information between creator and gameengine */
#include "SYS_System.h"

#include <signal.h>

#ifdef __FreeBSD__
# include <sys/types.h>
# include <floatingpoint.h>
# include <sys/rtprio.h>
#endif

#ifdef WITH_BINRELOC
#include "binreloc.h"
#endif

// from buildinfo.c
#ifdef BUILD_DATE
extern char * build_date;
extern char * build_time;
extern char * build_rev;
extern char * build_platform;
extern char * build_type;
#endif

/*	Local Function prototypes */
static void print_help(void);
static void print_version(void);

/* for the callbacks: */

extern int pluginapi_force_ref(void);  /* from blenpluginapi:pluginapi.c */

char bprogname[FILE_MAXDIR+FILE_MAXFILE]; /* from blenpluginapi:pluginapi.c */
char btempdir[FILE_MAXDIR+FILE_MAXFILE];

/* unix path support.
 * defined by the compiler. eg "/usr/share/blender/2.5" "/opt/blender/2.5" */
#ifndef BLENDERPATH
#define BLENDERPATH ""
#endif
 
char blender_path[FILE_MAXDIR+FILE_MAXFILE] = BLENDERPATH;

/* Initialise callbacks for the modules that need them */
static void setCallbacks(void); 

#if defined(__sgi) || defined(__alpha__)
static void fpe_handler(int sig)
{
	// printf("SIGFPE trapped\n");
}
#endif

/* handling ctrl-c event in console */
static void blender_esc(int sig)
{
	static int count = 0;
	
	G.afbreek = 1;	/* forces render loop to read queue, not sure if its needed */
	
	if (sig == 2) {
		if (count) {
			printf("\nBlender killed\n");
			exit(2);
		}
		printf("\nSent an internal break event. Press ^C again to kill Blender\n");
		count++;
	}
}

static void print_version(void)
{
#ifdef BUILD_DATE
	printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
	printf ("\tbuild date: %s\n", build_date);
	printf ("\tbuild time: %s\n", build_time);
	printf ("\tbuild revision: %s\n", build_rev);
	printf ("\tbuild platform: %s\n", build_platform);
	printf ("\tbuild type: %s\n", build_type);
#else
	printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
#endif
}

static void print_help(void)
{
	printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
	printf ("Usage: blender [args ...] [file] [args ...]\n");
	printf ("\nRender options:\n");
	printf ("  -b <file>\tRender <file> in background (doesn't load the user defaults .B.blend file)\n");
	printf ("    -a render frames from start to end (inclusive), only works when used after -b\n");
	printf ("    -S <name>\tSet scene <name>\n");
	printf ("    -f <frame>\tRender frame <frame> and save it\n");				
	printf ("    -s <frame>\tSet start to frame <frame> (use before the -a argument)\n");
	printf ("    -e <frame>\tSet end to frame <frame> (use before the -a argument)\n");
	printf ("    -o <path>\tSet the render path and file name.\n");
	printf ("      Use // at the start of the path to\n");
	printf ("        render relative to the blend file.\n");
	printf ("      The # characters are replaced by the frame number, and used to define zero padding.\n");
	printf ("        ani_##_test.png becomes ani_01_test.png\n");
	printf ("        test-######.png becomes test-000001.png\n");
	printf ("        When the filename has no #, The suffix #### is added to the filename\n");
	printf ("      The frame number will be added at the end of the filename.\n");
	printf ("      eg: blender -b foobar.blend -o //render_ -F PNG -x 1 -a\n");
	printf ("    -E <engine>\tSpecify the render engine.\n");
	printf ("      use -E help to list available engines.\n");
	printf ("\nFormat options:\n");
	printf ("    -F <format>\tSet the render format, Valid options are...\n");
	printf ("    \tTGA IRIS HAMX JPEG MOVIE IRIZ RAWTGA\n");
	printf ("    \tAVIRAW AVIJPEG PNG BMP FRAMESERVER\n");
	printf ("    (formats that can be compiled into blender, not available on all systems)\n");
	printf ("    \tHDR TIFF EXR MULTILAYER MPEG AVICODEC QUICKTIME CINEON DPX DDS\n");
	printf ("    -x <bool>\tSet option to add the file extension to the end of the file.\n");
	printf ("    -t <threads>\tUse amount of <threads> for rendering (background mode only).\n");
	printf ("      [1-8], 0 for systems processor count.\n");
	printf ("\nAnimation playback options:\n");
	printf ("  -a <options> <file(s)>\tPlayback <file(s)>, only operates this way when -b is not used.\n");
	printf ("    -p <sx> <sy>\tOpen with lower left corner at <sx>, <sy>\n");
	printf ("    -m\t\tRead from disk (Don't buffer)\n");
	printf ("    -f <fps> <fps-base>\t\tSpecify FPS to start with\n");
	printf ("    -j <frame>\tSet frame step to <frame>\n");
				
	printf ("\nWindow options:\n");
	printf ("  -w\t\tForce opening with borders (default)\n");
	printf ("  -W\t\tForce opening without borders\n");
	printf ("  -p <sx> <sy> <w> <h>\tOpen with lower left corner at <sx>, <sy>\n");
	printf ("                      \tand width and height <w>, <h>\n");
	printf ("\nGame Engine specific options:\n");
	printf ("  -g fixedtime\t\tRun on 50 hertz without dropping frames\n");
	printf ("  -g vertexarrays\tUse Vertex Arrays for rendering (usually faster)\n");
	printf ("  -g nomipmap\t\tNo Texture Mipmapping\n");
	printf ("  -g linearmipmap\tLinear Texture Mipmapping instead of Nearest (default)\n");

	printf ("\nMisc options:\n");
	printf ("  -d\t\tTurn debugging on\n");
	printf ("  -nojoystick\tDisable joystick support\n");
	printf ("  -noglsl\tDisable GLSL shading\n");
	printf ("  -h\t\tPrint this help text\n");
	printf ("  -y\t\tDisable automatic python script execution (pydrivers, pyconstraints, pynodes)\n");
	printf ("  -P <filename>\tRun the given Python script (filename or Blender Text)\n");
#ifdef WIN32
	printf ("  -R\t\tRegister .blend extension\n");
#endif
	printf ("  -v\t\tPrint Blender version and exit\n");
	printf ("  --\t\tEnds option processing.  Following arguments are \n");
	printf ("    \t\t   passed unchanged.  Access via Python's sys.argv\n");
	printf ("\nEnvironment Variables:\n");
	printf ("  $HOME\t\t\tStore files such as .blender/ .B.blend .Bfs .Blog here.\n");
	printf ("  $BLENDERPATH\tSystem directory to use for data files and scripts.\n");
	printf ("              \tFor this build of blender the default BLENDERPATH is...\n");
	printf ("              \t\"%s\"\n", blender_path);
	printf ("              \tseting the $BLENDERPATH will override this\n");
#ifdef WIN32
	printf ("  $TEMP\t\tStore temporary files here.\n");
#else
	printf ("  $TMP or $TMPDIR\tStore temporary files here.\n");
	printf ("  $BF_TIFF_LIB\t\tUse an alternative libtiff.so for loading tiff image files.\n");
#endif
#ifndef DISABLE_SDL
	printf ("  $SDL_AUDIODRIVER\tLibSDL audio driver - alsa, esd, alsa, dma.\n");
#endif
	printf ("  $IMAGEEDITOR\t\tImage editor executable, launch with the IKey from the file selector.\n");
	printf ("  $WINEDITOR\t\tText editor executable, launch with the EKey from the file selector.\n");
	printf ("  $PYTHONHOME\t\tPath to the python directory, eg. /usr/lib/python.\n");
	printf ("\nNote: Arguments must be separated by white space. eg:\n");
	printf ("    \"blender -ba test.blend\"\n");
	printf ("  ...will ignore the 'a'\n");
	printf ("    \"blender -b test.blend -f8\"\n");
	printf ("  ...will ignore 8 because there is no space between the -f and the frame value\n");
	printf ("Note: Arguments are executed in the order they are given. eg:\n");
	printf ("    \"blender -b test.blend -f 1 -o /tmp\"\n");
	printf ("  ...may not render to /tmp because '-f 1' renders before the output path is set\n");
	printf ("    \"blender -b -o /tmp test.blend -f 1\"\n");
	printf ("  ...may not render to /tmp because loading the blend file overwrites the output path that was set\n");
	printf ("    \"blender -b test.blend -o /tmp -f 1\" works as expected.\n\n");
}


double PIL_check_seconds_timer(void);

/* XXX This was here to fix a crash when running python scripts
 * with -P that used the screen.
 *
 * static void main_init_screen( void )
{
	setscreen(G.curscreen);
	
	if(G.main->scene.first==0) {
		set_scene( add_scene("1") );
	}
}*/

int main(int argc, char **argv)
{
	SYS_SystemHandle syshandle;
	bContext *C= CTX_create();
	int a, i, stax, stay, sizx, sizy /*XXX, scr_init = 0*/;

#ifdef WITH_BINRELOC
	br_init( NULL );
#endif

	setCallbacks();
#ifdef __APPLE__
		/* patch to ignore argument finder gives us (pid?) */
	if (argc==2 && strncmp(argv[1], "-psn_", 5)==0) {
		extern int GHOST_HACK_getFirstFile(char buf[]);
		static char firstfilebuf[512];

		argc= 1;

		if (GHOST_HACK_getFirstFile(firstfilebuf)) {
			argc= 2;
			argv[1]= firstfilebuf;
		}
	}

#endif

#ifdef __FreeBSD__
	fpsetmask(0);
#endif
#ifdef __linux__
    #ifdef __alpha__
	signal (SIGFPE, fpe_handler);
    #endif
#endif
#if defined(__sgi)
	signal (SIGFPE, fpe_handler);
#endif

	// copy path to executable in bprogname. playanim and creting runtimes
	// need this.

	BLI_where_am_i(bprogname, argv[0]);
	
	{	/* override the hard coded blender path */
		char *blender_path_env = getenv("BLENDERPATH");
		if(blender_path_env)
			BLI_strncpy(blender_path, blender_path_env, sizeof(blender_path));
	}
	
	RNA_init();
	RE_engines_init();

		/* Hack - force inclusion of the plugin api functions,
		 * see blenpluginapi:pluginapi.c
		 */
	pluginapi_force_ref();

	init_nodesystem();
	
	initglobals();	/* blender.c */

	syshandle = SYS_GetSystem();
	GEN_init_messaging_system();

	/* first test for background */

	G.f |= G_DOSCRIPTLINKS; /* script links enabled by default */

	for(a=1; a<argc; a++) {

		/* Handle unix and windows style help requests */
		if ((!strcmp(argv[a], "--help")) || (!strcmp(argv[a], "/?"))){
			print_help();
			exit(0);
		}

		/* end argument processing after -- */
		if (!strcmp( argv[a], "--")){
			a = argc;
			break;
		}

		/* Handle long version request */
		if (!strcmp(argv[a], "--version")){
			print_version();
			exit(0);
		}

		/* Handle -* switches */
		else if(argv[a][0] == '-') {
			switch(argv[a][1]) {
			case 'a': /* -b was not given, play an animation */
				
				/* exception here, see below, it probably needs happens after qt init? */
				libtiff_init();

// XXX				playanim(argc-1, argv+1);
				exit(0);
				break;
			case 'b':
			case 'B':
				G.background = 1;
				a= argc;
				break;

			case 'y':
				G.f &= ~G_DOSCRIPTLINKS;
				break;

			case 'Y':
				printf ("-y was used to disable script links because,\n");
				printf ("\t-p being taken, Ton was of the opinion that Y\n");
				printf ("\tlooked like a split (disabled) snake, and also\n");
				printf ("\twas similar to a python's tongue (unproven).\n\n");

				printf ("\tZr agreed because it gave him a reason to add a\n");
				printf ("\tcompletely useless text into Blender.\n\n");
				
				printf ("\tADDENDUM! Ton, in defense, found this picture of\n");
				printf ("\tan Australian python, exhibiting her (his/its) forked\n");
				printf ("\tY tongue. It could be part of an H Zr retorted!\n\n");
				printf ("\thttp://www.users.bigpond.com/snake.man/\n");
				
				exit(252);
				
			case 'h':			
				print_help();
				exit(0);
			case 'v':
				print_version();
				exit(0);
			default:
				break;
			}
		}
	}

#ifdef __sgi
	setuid(getuid()); /* end superuser */
#endif

	/* for all platforms, even windos has it! */
	if(G.background) signal(SIGINT, blender_esc);	/* ctrl c out bg render */
	
	/* background render uses this font too */
	BKE_font_register_builtin(datatoc_Bfont, datatoc_Bfont_size);
	
	init_def_material();

	if(G.background==0) {
		for(a=1; a<argc; a++) {
			if(argv[a][0] == '-') {
				switch(argv[a][1]) {
				case 'p':	/* prefsize */
					if (argc-a < 5) {
						printf ("-p requires four arguments\n");
						exit(1);
					}
					a++;
					stax= atoi(argv[a]);
					a++;
					stay= atoi(argv[a]);
					a++;
					sizx= atoi(argv[a]);
					a++;
					sizy= atoi(argv[a]);

					WM_setprefsize(stax, stay, sizx, sizy);
					break;
				case 'd':
					G.f |= G_DEBUG;		/* std output printf's */ 
					printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
					MEM_set_memory_debug();
#ifdef NAN_BUILDINFO
					printf("Build: %s %s %s %s\n", build_date, build_time, build_platform, build_type);

#endif // NAN_BUILDINFO
					for (i = 0; i < argc; i++) {
						printf("argv[%d] = %s\n", i, argv[i]);
					}
					break;
				case 'E':
					G.f |= G_SWAP_EXCHANGE;
					break;
				case 'w':
					/* with borders XXX OLD CRUFT!*/
					break;
				case 'W':
					/* borderless, win + linux XXX OLD CRUFT */
					/* XXX, fixme mein, borderless on OSX */
					break;
				case 'R':
					/* Registering filetypes only makes sense on windows...      */
#ifdef WIN32
					RegisterBlendExtension(argv[0]);
#endif
					break;
				case 'n':
				case 'N':
					if (BLI_strcasecmp(argv[a], "-nojoystick") == 0) {
						/**
						 	don't initialize joysticks if user doesn't want to use joysticks
							failed joystick initialization delays over 5 seconds, before game engine start
						*/
						SYS_WriteCommandLineInt(syshandle,"nojoystick",1);
						if (G.f & G_DEBUG) printf("disabling nojoystick\n");
					}
					if (BLI_strcasecmp(argv[a], "-noglsl") == 0)
						GPU_extensions_disable();
					break;
				}
			}
		}

#ifndef DISABLE_PYTHON		
		BPY_start_python(argc, argv);
#endif		

		WM_init(C);
		
		// XXX BRECHT SOLVE
		BLI_where_is_temp( btempdir, 1 ); /* call after loading the .B.blend so we can read U.tempdir */

#ifndef DISABLE_SDL
	BLI_setenv("SDL_VIDEODRIVER", "dummy");
#ifdef __linux__
	/* On linux the default SDL driver dma often would not play
	 * use alsa if none is set */
	setenv("SDL_AUDIODRIVER", "alsa", 0);
#endif
#endif
	}
	else {
#ifndef DISABLE_PYTHON
		BPY_start_python(argc, argv);
#endif		
		BLI_where_is_temp( btempdir, 0 ); /* call after loading the .B.blend so we can read U.tempdir */
	}
#ifndef DISABLE_PYTHON
	/**
	 * NOTE: the U.pythondir string is NULL until WM_init() is executed,
	 * so we provide the BPY_ function below to append the user defined
	 * pythondir to Python's sys.path at this point.  Simply putting
	 * WM_init() before BPY_start_python() crashes Blender at startup.
	 * Update: now this function also inits the bpymenus, which also depend
	 * on U.pythondir.
	 */
	BPY_post_start_python();

	if(!G.background)
		BPY_run_ui_scripts(C, 0); /* dont need to reload the first time */
#endif
	
	CTX_py_init_set(C, 1);
	if(!G.background)
		WM_keymap_init(C); /* after BPY_run_ui_scripts() */

#ifdef WITH_QUICKTIME

	quicktime_init();

#endif /* WITH_QUICKTIME */

	/* dynamically load libtiff, if available */
	libtiff_init();
	if (!G.have_libtiff && (G.f & G_DEBUG)) {
		printf("Unable to load: libtiff.\n");
		printf("Try setting the BF_TIFF_LIB environment variable if you want this support.\n");
		printf("Example: setenv BF_TIFF_LIB /usr/lib/libtiff.so\n");
	}

	/* OK we are ready for it */

	for(a=1; a<argc; a++) {
		if (G.afbreek==1) break;

		if(argv[a][0] == '-') {
			switch(argv[a][1]) {
			case '-':  /* -- ends argument processing */
				a = argc;
				break;
				
			case 'p':	/* prefsize */
				a+= 4;
				break;

			case 'g':
				{
				/**
				gameengine parameters are automaticly put into system
				-g [paramname = value]
				-g [boolparamname]
				example:
				-g novertexarrays
				-g maxvertexarraysize = 512
				*/

					if(++a < argc)
					{
						char* paramname = argv[a];
						/* check for single value versus assignment */
						if (a+1 < argc && (*(argv[a+1]) == '='))
						{
							a++;
							if (a+1 < argc)
							{
								a++;
								/* assignment */
								SYS_WriteCommandLineString(syshandle,paramname,argv[a]);
							}  else
							{
								printf("error: argument assignment (%s) without value.\n",paramname);
							}
							/* name arg eaten */

						} else
						{
							SYS_WriteCommandLineInt(syshandle,argv[a],1);

							/* doMipMap */
							if (!strcmp(argv[a],"nomipmap"))
							{
								GPU_set_mipmap(0); //doMipMap = 0;
							}
							/* linearMipMap */
							if (!strcmp(argv[a],"linearmipmap"))
							{
								GPU_set_linear_mipmap(1); //linearMipMap = 1;
							}


						} /* if (*(argv[a+1]) == '=') */
					} /*	if(++a < argc)  */
					break;
				}
			case 'f':
				a++;
				if (CTX_data_scene(C)) {
					Scene *scene= CTX_data_scene(C);
					
					if (a < argc) {
						int frame = atoi(argv[a]);
						Render *re = RE_NewRender(scene->id.name);

						frame = MIN2(MAXFRAME, MAX2(1, frame));
						
						RE_BlenderAnim(re, scene, frame, frame, scene->frame_step);
					}
				} else {
					printf("\nError: no blend loaded. cannot use '-f'.\n");
				}
				break;
			case 'a':
				if (CTX_data_scene(C)) {
					Scene *scene= CTX_data_scene(C);
					Render *re= RE_NewRender(scene->id.name);
					RE_BlenderAnim(re, scene, scene->r.sfra, scene->r.efra, scene->frame_step);
				} else {
					printf("\nError: no blend loaded. cannot use '-a'.\n");
				}
				break;
			case 'S':
				if(++a < argc) {
					set_scene_name(argv[a]);
				}
				break;
			case 's':
				a++;
				if (CTX_data_scene(C)) {
					Scene *scene= CTX_data_scene(C);
					if (a < argc) {
						int frame = atoi(argv[a]);
						(scene->r.sfra) = MIN2(MAXFRAME, MAX2(1, frame));
					}
				} else {
					printf("\nError: no blend loaded. cannot use '-s'.\n");
				}
				break;
			case 'e':
				a++;
				if (CTX_data_scene(C)) {
					Scene *scene= CTX_data_scene(C);
					if (a < argc) {
						int frame = atoi(argv[a]);
						(scene->r.efra) = MIN2(MAXFRAME, MAX2(1, frame));
					}
				} else {
					printf("\nError: no blend loaded. cannot use '-e'.\n");
				}
				break;
			case 'j':
				a++;
				if (CTX_data_scene(C)) {
					Scene *scene= CTX_data_scene(C);
					if (a < argc) {
						int frame = atoi(argv[a]);
						(scene->frame_step) = MIN2(MAXFRAME, MAX2(1, frame));
					}
				} else {
					printf("\nError: no blend loaded. cannot use '-j'.\n");
				}
				break;
			case 'P':

#ifndef DISABLE_PYTHON
				//XXX 
				// FOR TESTING ONLY
				a++;
				BPY_run_python_script(C, argv[a], NULL, NULL); // use reports?
#if 0
				a++;
				if (a < argc) {
					/* If we're not running in background mode, then give python a valid screen */
					if ((G.background==0) && (scr_init==0)) {
						main_init_screen();
						scr_init = 1;
					}
					BPY_run_python_script(C, argv[a], NULL, NULL); // use reports?
				}
				else printf("\nError: you must specify a Python script after '-P '.\n");
#endif
#else
				printf("This blender was built without python support\n");
#endif /* DISABLE_PYTHON */
				break;
			case 'o':
				a++;
				if (a < argc){
					if (CTX_data_scene(C)) {
						Scene *scene= CTX_data_scene(C);
						BLI_strncpy(scene->r.pic, argv[a], FILE_MAXDIR);
					} else {
						printf("\nError: no blend loaded. cannot use '-o'.\n");
					}
				} else {
					printf("\nError: you must specify a path after '-o '.\n");
				}
				break;
			case 'E':
				a++;
				if (a < argc)
				{
					if (!strcmp(argv[a],"help"))
					{
						RenderEngineType *type = NULL;

						for( type = R_engines.first; type; type = type->next )
						{
							printf("\t%s\n", type->idname);
						}
						exit(0);
					}
					else 
					{
						if (CTX_data_scene(C)==NULL)
						{
							printf("\nError: no blend loaded. order the arguments so '-E ' is after the blend is loaded.\n");
						}
						else 
						{
							Scene *scene= CTX_data_scene(C);
							RenderData *rd = &scene->r;
							RenderEngineType *type = NULL;
							
							for( type = R_engines.first; type; type = type->next )
							{
								if (!strcmp(argv[a],type->idname))
								{
									BLI_strncpy(rd->engine, type->idname, sizeof(rd->engine));
								}
							}
						}
					}
				}
				else
				{
					printf("\nEngine not specified.\n");
				}
				break;
			case 'F':
				a++;
				if (a < argc){
					if (CTX_data_scene(C)==NULL) {
						printf("\nError: no blend loaded. order the arguments so '-F ' is after the blend is loaded.\n");
					} else {
						Scene *scene= CTX_data_scene(C);
						if      (!strcmp(argv[a],"TGA")) scene->r.imtype = R_TARGA;
						else if (!strcmp(argv[a],"IRIS")) scene->r.imtype = R_IRIS;
						else if (!strcmp(argv[a],"HAMX")) scene->r.imtype = R_HAMX;
#ifdef WITH_DDS
						else if (!strcmp(argv[a],"DDS")) scene->r.imtype = R_DDS;
#endif
						else if (!strcmp(argv[a],"JPEG")) scene->r.imtype = R_JPEG90;
						else if (!strcmp(argv[a],"MOVIE")) scene->r.imtype = R_MOVIE;
						else if (!strcmp(argv[a],"IRIZ")) scene->r.imtype = R_IRIZ;
						else if (!strcmp(argv[a],"RAWTGA")) scene->r.imtype = R_RAWTGA;
						else if (!strcmp(argv[a],"AVIRAW")) scene->r.imtype = R_AVIRAW;
						else if (!strcmp(argv[a],"AVIJPEG")) scene->r.imtype = R_AVIJPEG;
						else if (!strcmp(argv[a],"PNG")) scene->r.imtype = R_PNG;
						else if (!strcmp(argv[a],"AVICODEC")) scene->r.imtype = R_AVICODEC;
						else if (!strcmp(argv[a],"QUICKTIME")) scene->r.imtype = R_QUICKTIME;
						else if (!strcmp(argv[a],"BMP")) scene->r.imtype = R_BMP;
						else if (!strcmp(argv[a],"HDR")) scene->r.imtype = R_RADHDR;
						else if (!strcmp(argv[a],"TIFF")) scene->r.imtype = R_TIFF;
#ifdef WITH_OPENEXR
						else if (!strcmp(argv[a],"EXR")) scene->r.imtype = R_OPENEXR;
						else if (!strcmp(argv[a],"MULTILAYER")) scene->r.imtype = R_MULTILAYER;
#endif
						else if (!strcmp(argv[a],"MPEG")) scene->r.imtype = R_FFMPEG;
						else if (!strcmp(argv[a],"FRAMESERVER")) scene->r.imtype = R_FRAMESERVER;
						else if (!strcmp(argv[a],"CINEON")) scene->r.imtype = R_CINEON;
						else if (!strcmp(argv[a],"DPX")) scene->r.imtype = R_DPX;
#if WITH_OPENJPEG
						else if (!strcmp(argv[a],"JP2")) scene->r.imtype = R_JP2;
#endif
						else printf("\nError: Format from '-F' not known or not compiled in this release.\n");
					}
				} else {
					printf("\nError: no blend loaded. cannot use '-x'.\n");
				}
				break;
				
			case 't':
				a++;
				if (a < argc) {
					if(G.background) {
						RE_set_max_threads(atoi(argv[a]));
					} else {
						printf("Warning: threads can only be set in background mode\n");
					}
				} else {
					printf("\nError: you must specify a number of threads between 0 and 8 '-t '.\n");
				}
				break;
			case 'x': /* extension */
				a++;
				if (a < argc) {
					if (CTX_data_scene(C)) {
						Scene *scene= CTX_data_scene(C);
						if (argv[a][0] == '0') {
							scene->r.scemode &= ~R_EXTENSION;
						} else if (argv[a][0] == '1') {
							scene->r.scemode |= R_EXTENSION;
						} else {
							printf("\nError: Use '-x 1' or '-x 0' To set the extension option.\n");
						}
					} else {
						printf("\nError: no blend loaded. order the arguments so '-o ' is after '-x '.\n");
					}
				} else {
					printf("\nError: you must specify a path after '- '.\n");
				}
				break;
			}
		}
		else {
			
			/* Make the path absolute because its needed for relative linked blends to be found */
			char filename[FILE_MAXDIR + FILE_MAXFILE];
			
			BLI_strncpy(filename, argv[a], sizeof(filename));
			BLI_convertstringcwd(filename);
			
			if (G.background) {
				int retval = BKE_read_file(C, argv[a], NULL, NULL);
				
				/*we successfully loaded a blend file, get sure that
				pointcache works */
				if (retval!=0) G.relbase_valid = 1;

				/* happens for the UI on file reading too (huh? (ton))*/
// XXX			BKE_reset_undo();
//				BKE_write_undo("original");	/* save current state */
			} else {
				/* we are not running in background mode here, but start blender in UI mode with 
				   a file - this should do everything a 'load file' does */
				WM_read_file(C, filename, NULL);
			}
		}
	}
	
	if(G.background) {
		/* actually incorrect, but works for now (ton) */
		WM_exit(C);
	}


	WM_main(C);
	
	/*XXX if (scr_init==0) {
		main_init_screen();
	}
	
	screenmain();*/ /* main display loop */

	return 0;
} /* end of int main(argc,argv)	*/

static void error_cb(char *err)
{
	
	printf("%s\n", err);	/* XXX do this in WM too */
}

static void mem_error_cb(char *errorStr)
{
	fputs(errorStr, stderr);
	fflush(stderr);
}

static void setCallbacks(void)
{
	/* Error output from the alloc routines: */
	MEM_set_error_callback(mem_error_cb);


	/* BLI_blenlib: */

	BLI_setErrorCallBack(error_cb); /* */
// XXX	BLI_setInterruptCallBack(blender_test_break);

}
