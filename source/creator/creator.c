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
 */

#if defined(__linux__) && defined(__GNUC__)
#define _GNU_SOURCE
#include <fenv.h>
#endif

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

#include "BLI_args.h"
#include "BLI_threads.h"

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
#include "BKE_sound.h"

#include "IMB_imbuf.h"	// for quicktime_init

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "RE_pipeline.h"

//XXX #include "playanim_ext.h"
#include "ED_datafiles.h"

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
extern char build_date[];
extern char build_time[];
extern char build_rev[];
extern char build_platform[];
extern char build_type[];
#endif

/*	Local Function prototypes */
static int print_help(int argc, char **argv, void *data);
static int print_version(int argc, char **argv, void *data);

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

/* on linux set breakpoints here when running in debug mode, useful to catch floating point errors */
#if defined(__sgi) || defined(__linux__)
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

/* buildinfo can have quotes */
#ifdef BUILD_DATE
static void strip_quotes(char *str)
{
    if(str[0] == '"') {
        int len= strlen(str) - 1;
        memmove(str, str+1, len);
        if(str[len-1] == '"') {
            str[len-1]= '\0';
        }
    }
}
#endif

static int print_version(int argc, char **argv, void *data)
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

	exit(0);

	return 0;
}

static int print_help(int argc, char **argv, void *data)
{
	printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
	printf ("Usage: blender [args ...] [file] [args ...]\n");
	printf ("\nRender options:\n");
	printf ("  -b <file>\tLoad <file> in background (often used for background rendering)\n");
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
	printf ("    \t\t * prints every operator call and their arguments\n");
	printf ("    \t\t * disables mouse grab (to interact with a debugger in some cases)\n");
	printf ("    \t\t * keeps python sys.stdin rather then setting it to None\n");
	printf ("  -nojoystick\tDisable joystick support\n");
	printf ("  -noglsl\tDisable GLSL shading\n");
	printf ("  -noaudio\tForce sound system to None\n");
	printf ("  -setaudio\tForce sound system to a specific device\n");
	printf ("    \tNULL SDL OPENAL JACK\n");
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

	exit(0);

	return 0;
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

static int end_arguments(int argc, char **argv, void *data)
{
	return -1;
}

static int disable_python(int argc, char **argv, void *data)
{
	G.f &= ~G_SCRIPT_AUTOEXEC;
	return 0;
}


static int forked_tongue(int argc, char **argv, void *data)
{
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
	return 0;
}

static int background_mode(int argc, char **argv, void *data)
{
	G.background = 1;
	return 0;
}

static int debug_mode(int argc, char **argv, void *data)
{
	G.f |= G_DEBUG;		/* std output printf's */
	printf ("Blender %d.%02d (sub %d) Build\n", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
	MEM_set_memory_debug();

#ifdef NAN_BUILDINFO
	printf("Build: %s %s %s %s\n", build_date, build_time, build_platform, build_type);
#endif // NAN_BUILDINFO

	BLI_argsPrint(data);
	return 0;
}

static int set_fpe(int argc, char **argv, void *data)
{
#if defined(__sgi) || defined(__linux__)
	/* zealous but makes float issues a heck of a lot easier to find!
	 * set breakpoints on fpe_handler */
	signal(SIGFPE, fpe_handler);

#if defined(__linux__) && defined(__GNUC__)
	feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW );
#endif
#endif
	return 0;
}

static int playback_mode(int argc, char **argv, void *data)
{
	/* not if -b was given first */
	if (G.background == 0) {
		/* exception here, see below, it probably needs happens after qt init? */
		libtiff_init();

// XXX				playanim(argc, argv); /* not the same argc and argv as before */
		exit(0);
	}

	return -2;
}

static int prefsize(int argc, char **argv, void *data)
{
	int stax, stay, sizx, sizy;

	if (argc < 5) {
		printf ("-p requires four arguments\n");
		exit(1);
	}

	stax= atoi(argv[1]);
	stay= atoi(argv[2]);
	sizx= atoi(argv[3]);
	sizy= atoi(argv[4]);

	WM_setprefsize(stax, stay, sizx, sizy);

	return 4;
}

static int with_borders(int argc, char **argv, void *data)
{
	/* with borders XXX OLD CRUFT!*/

	return 0;
}

static int without_borders(int argc, char **argv, void *data)
{
	/* borderless, win + linux XXX OLD CRUFT */
	/* XXX, fixme mein, borderless on OSX */

	return 0;
}

static int register_extension(int argc, char **argv, void *data)
{
#ifdef WIN32
	char *path = BLI_argsArgv(data)[0];
	RegisterBlendExtension(path);
#endif

	return 0;
}

static int no_joystick(int argc, char **argv, void *data)
{
	SYS_SystemHandle *syshandle = data;

	/**
	 	don't initialize joysticks if user doesn't want to use joysticks
		failed joystick initialization delays over 5 seconds, before game engine start
	*/
	SYS_WriteCommandLineInt(*syshandle, "nojoystick",1);
	if (G.f & G_DEBUG) printf("disabling nojoystick\n");

	return 0;
}

static int no_glsl(int argc, char **argv, void *data)
{
	GPU_extensions_disable();
	return 0;
}

static int no_audio(int argc, char **argv, void *data)
{
	sound_force_device(0);
	return 0;
}

static int set_audio(int argc, char **argv, void *data)
{
	if (argc < 1) {
		printf("-setaudio require one argument\n");
		exit(1);
	}

	sound_force_device(sound_define_from_str(argv[1]));
	return 1;
}

static int set_output(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (argc >= 1){
		if (CTX_data_scene(C)) {
			Scene *scene= CTX_data_scene(C);
			BLI_strncpy(scene->r.pic, argv[1], FILE_MAXDIR);
		} else {
			printf("\nError: no blend loaded. cannot use '-o'.\n");
		}
		return 1;
	} else {
		printf("\nError: you must specify a path after '-o '.\n");
		return 0;
	}
}

static int set_engine(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (argc >= 1)
	{
		if (!strcmp(argv[1],"help"))
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
				printf("\nError: no blend loaded. order the arguments so '-E ' is after a blend is loaded.\n");
			}
			else
			{
				Scene *scene= CTX_data_scene(C);
				RenderData *rd = &scene->r;
				RenderEngineType *type = NULL;

				for( type = R_engines.first; type; type = type->next )
				{
					if (!strcmp(argv[1],type->idname))
					{
						BLI_strncpy(rd->engine, type->idname, sizeof(rd->engine));
					}
				}
			}
		}

		return 1;
	}
	else
	{
		printf("\nEngine not specified.\n");
		return 0;
	}
}

static int set_image_type(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (argc >= 1){
		char *imtype = argv[1];
		if (CTX_data_scene(C)==NULL) {
			printf("\nError: no blend loaded. order the arguments so '-F ' is after the blend is loaded.\n");
		} else {
			Scene *scene= CTX_data_scene(C);
			if      (!strcmp(imtype,"TGA")) scene->r.imtype = R_TARGA;
			else if (!strcmp(imtype,"IRIS")) scene->r.imtype = R_IRIS;
			else if (!strcmp(imtype,"HAMX")) scene->r.imtype = R_HAMX;
#ifdef WITH_DDS
			else if (!strcmp(imtype,"DDS")) scene->r.imtype = R_DDS;
#endif
			else if (!strcmp(imtype,"JPEG")) scene->r.imtype = R_JPEG90;
			else if (!strcmp(imtype,"MOVIE")) scene->r.imtype = R_MOVIE;
			else if (!strcmp(imtype,"IRIZ")) scene->r.imtype = R_IRIZ;
			else if (!strcmp(imtype,"RAWTGA")) scene->r.imtype = R_RAWTGA;
			else if (!strcmp(imtype,"AVIRAW")) scene->r.imtype = R_AVIRAW;
			else if (!strcmp(imtype,"AVIJPEG")) scene->r.imtype = R_AVIJPEG;
			else if (!strcmp(imtype,"PNG")) scene->r.imtype = R_PNG;
			else if (!strcmp(imtype,"AVICODEC")) scene->r.imtype = R_AVICODEC;
			else if (!strcmp(imtype,"QUICKTIME")) scene->r.imtype = R_QUICKTIME;
			else if (!strcmp(imtype,"BMP")) scene->r.imtype = R_BMP;
			else if (!strcmp(imtype,"HDR")) scene->r.imtype = R_RADHDR;
			else if (!strcmp(imtype,"TIFF")) scene->r.imtype = R_TIFF;
#ifdef WITH_OPENEXR
			else if (!strcmp(imtype,"EXR")) scene->r.imtype = R_OPENEXR;
			else if (!strcmp(imtype,"MULTILAYER")) scene->r.imtype = R_MULTILAYER;
#endif
			else if (!strcmp(imtype,"MPEG")) scene->r.imtype = R_FFMPEG;
			else if (!strcmp(imtype,"FRAMESERVER")) scene->r.imtype = R_FRAMESERVER;
			else if (!strcmp(imtype,"CINEON")) scene->r.imtype = R_CINEON;
			else if (!strcmp(imtype,"DPX")) scene->r.imtype = R_DPX;
#if WITH_OPENJPEG
			else if (!strcmp(imtype,"JP2")) scene->r.imtype = R_JP2;
#endif
			else printf("\nError: Format from '-F' not known or not compiled in this release.\n");
		}
		return 1;
	} else {
		printf("\nError: you must specify a format after '-F '.\n");
		return 0;
	}
}

static int set_threads(int argc, char **argv, void *data)
{
	if (argc >= 1) {
		if(G.background) {
			RE_set_max_threads(atoi(argv[1]));
		} else {
			printf("Warning: threads can only be set in background mode\n");
		}
		return 1;
	} else {
		printf("\nError: you must specify a number of threads between 0 and 8 '-t '.\n");
		return 0;
	}
}

static int set_extension(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (argc >= 1) {
		if (CTX_data_scene(C)) {
			Scene *scene= CTX_data_scene(C);
			if (argv[1][0] == '0') {
				scene->r.scemode &= ~R_EXTENSION;
			} else if (argv[1][0] == '1') {
				scene->r.scemode |= R_EXTENSION;
			} else {
				printf("\nError: Use '-x 1' or '-x 0' To set the extension option.\n");
			}
		} else {
			printf("\nError: no blend loaded. order the arguments so '-o ' is after '-x '.\n");
		}
		return 1;
	} else {
		printf("\nError: you must specify a path after '- '.\n");
		return 0;
	}
}

static int set_ge_parameters(int argc, char **argv, void *data)
{
	SYS_SystemHandle syshandle = *(SYS_SystemHandle*)data;
	int a = 0;
/**
gameengine parameters are automaticly put into system
-g [paramname = value]
-g [boolparamname]
example:
-g novertexarrays
-g maxvertexarraysize = 512
*/

	if(argc >= 1)
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
				return 0;
			}
			/* name arg eaten */

		} else {
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
	}

	return a;
}

static int render_frame(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (CTX_data_scene(C)) {
		Scene *scene= CTX_data_scene(C);

		if (argc > 1) {
			int frame = atoi(argv[1]);
			Render *re = RE_NewRender(scene->id.name);
			ReportList reports;

			BKE_reports_init(&reports, RPT_PRINT);

			frame = MIN2(MAXFRAME, MAX2(MINAFRAME, frame));

			RE_BlenderAnim(re, scene, scene->lay, frame, frame, scene->r.frame_step, &reports);
			return 1;
		} else {
			printf("\nError: frame number must follow '-f'.\n");
			return 0;
		}
	} else {
		printf("\nError: no blend loaded. cannot use '-f'.\n");
		return 0;
	}
}

static int render_animation(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (CTX_data_scene(C)) {
		Scene *scene= CTX_data_scene(C);
		Render *re= RE_NewRender(scene->id.name);
		ReportList reports;
		BKE_reports_init(&reports, RPT_PRINT);
		RE_BlenderAnim(re, scene, scene->lay, scene->r.sfra, scene->r.efra, scene->r.frame_step, &reports);
	} else {
		printf("\nError: no blend loaded. cannot use '-a'.\n");
	}
	return 0;
}

static int set_scene(int argc, char **argv, void *data)
{
	if(argc > 1) {
		set_scene_name(argv[1]);
		return 1;
	} else {
		printf("\nError: Scene name must follow '-S'.\n");
		return 0;
	}
}

static int set_start_frame(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (CTX_data_scene(C)) {
		Scene *scene= CTX_data_scene(C);
		if (argc > 1) {
			int frame = atoi(argv[1]);
			(scene->r.sfra) = CLAMPIS(frame, MINFRAME, MAXFRAME);
			return 1;
		} else {
			printf("\nError: frame number must follow '-s'.\n");
			return 0;
		}
	} else {
		printf("\nError: no blend loaded. cannot use '-s'.\n");
		return 0;
	}
}

static int set_end_frame(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (CTX_data_scene(C)) {
		Scene *scene= CTX_data_scene(C);
		if (argc > 1) {
			int frame = atoi(argv[1]);
			(scene->r.efra) = CLAMPIS(frame, MINFRAME, MAXFRAME);
			return 1;
		} else {
			printf("\nError: frame number must follow '-e'.\n");
			return 0;
		}
	} else {
		printf("\nError: no blend loaded. cannot use '-e'.\n");
		return 0;
	}
}

static int set_skip_frame(int argc, char **argv, void *data)
{
	bContext *C = data;
	if (CTX_data_scene(C)) {
		Scene *scene= CTX_data_scene(C);
		if (argc > 1) {
			int frame = atoi(argv[1]);
			(scene->r.frame_step) = CLAMPIS(frame, 1, MAXFRAME);
			return 1;
		} else {
			printf("\nError: number of frames to step must follow '-j'.\n");
			return 0;
		}
	} else {
		printf("\nError: no blend loaded. cannot use '-j'.\n");
		return 0;
	}
}

static int run_python(int argc, char **argv, void *data)
{
#ifndef DISABLE_PYTHON
	bContext *C = data;

	/* Make the path absolute because its needed for relative linked blends to be found */
	char filename[FILE_MAXDIR + FILE_MAXFILE];
	BLI_strncpy(filename, argv[1], sizeof(filename));
	BLI_path_cwd(filename);

	/* workaround for scripts not getting a bpy.context.scene, causes internal errors elsewhere */
	if (argc > 1) {
		/* XXX, temp setting the WM is ugly, splash also does this :S */
		wmWindowManager *wm= CTX_wm_manager(C);
		wmWindow *prevwin= CTX_wm_window(C);
		Scene *prevscene= CTX_data_scene(C);

		if(wm->windows.first) {
			CTX_wm_window_set(C, wm->windows.first);

			BPY_run_python_script(C, filename, NULL, NULL); // use reports?

			CTX_wm_window_set(C, prevwin);
		}
		else {
			fprintf(stderr, "Python script \"%s\" running with missing context data.\n", argv[1]);
			BPY_run_python_script(C, filename, NULL, NULL); // use reports?
		}

		CTX_data_scene_set(C, prevscene);

		return 1;
	} else {
		printf("\nError: you must specify a Python script after '-P '.\n");
		return 0;
	}
#else
	printf("This blender was built without python support\n");
	return 0;
#endif /* DISABLE_PYTHON */
}

static int load_file(int argc, char **argv, void *data)
{
	bContext *C = data;

	/* Make the path absolute because its needed for relative linked blends to be found */
	char filename[FILE_MAXDIR + FILE_MAXFILE];
	BLI_strncpy(filename, argv[0], sizeof(filename));
	BLI_path_cwd(filename);

	if (G.background) {
		int retval = BKE_read_file(C, filename, NULL, NULL);

		/*we successfully loaded a blend file, get sure that
		pointcache works */
		if (retval!=0) {
			wmWindowManager *wm= CTX_wm_manager(C);
			CTX_wm_manager_set(C, NULL); /* remove wm to force check */
			WM_check(C);
			G.relbase_valid = 1;
			if (CTX_wm_manager(C) == NULL) CTX_wm_manager_set(C, wm); /* reset wm */
		}

		/* happens for the UI on file reading too (huh? (ton))*/
	// XXX			BKE_reset_undo();
	//				BKE_write_undo("original");	/* save current state */
	} else {
		/* we are not running in background mode here, but start blender in UI mode with
		   a file - this should do everything a 'load file' does */
		WM_read_file(C, filename, NULL);
	}

	G.file_loaded = 1;

	return 0;
}

void setupArguments(bContext *C, bArgs *ba, SYS_SystemHandle *syshandle)
{
	//BLI_argsAdd(ba, arg, pass, BA_ArgCallback cb, C);

	/* end argument processing after -- */
	BLI_argsAdd(ba, "--", -1, end_arguments, NULL);

	/* first pass: background mode, disable python and commands that exit after usage */
	BLI_argsAdd(ba, "--help", 1, print_help, NULL);
	BLI_argsAdd(ba, "-h", 1, print_help, NULL);
	BLI_argsAdd(ba, "/?", 1, print_help, NULL);

	BLI_argsAdd(ba, "--version", 1, print_version, NULL);
	BLI_argsAdd(ba, "-v", 1, print_version, NULL);

	BLI_argsAdd(ba, "-Y", 1, forked_tongue, NULL);
	BLI_argsAdd(ba, "-y", 1, disable_python, NULL);

	BLI_argsAdd(ba, "-fpe", 1, set_fpe, NULL);

	BLI_argsAdd(ba, "-B", 1, background_mode, NULL);
	BLI_argsAdd(ba, "-b", 1, background_mode, NULL);
	BLI_argsAdd(ba, "-a", 1, playback_mode, NULL);

	BLI_argsAdd(ba, "-d", 1, debug_mode, ba);

	/* second pass: custom window stuff */
	BLI_argsAdd(ba, "-p", 2, prefsize, NULL);
	BLI_argsAdd(ba, "-w", 2, with_borders, NULL);
	BLI_argsAdd(ba, "-W", 2, without_borders, NULL);
	BLI_argsAdd(ba, "-R", 2, register_extension, ba);

	/* third pass: disabling things and forcing settings */
	BLI_argsAddCase(ba, "-nojoystick", 3, no_joystick, syshandle);
	BLI_argsAddCase(ba, "-noglsl", 3, no_glsl, NULL);
	BLI_argsAddCase(ba, "-noaudio", 3, no_audio, NULL);
	BLI_argsAddCase(ba, "-setaudio", 3, set_audio, NULL);

	/* fourth pass: processing arguments */
	BLI_argsAdd(ba, "-g", 4, set_ge_parameters, syshandle);
	BLI_argsAdd(ba, "-f", 4, render_frame, C);
	BLI_argsAdd(ba, "-a", 4, render_animation, C);
	BLI_argsAdd(ba, "-S", 4, set_scene, NULL);
	BLI_argsAdd(ba, "-s", 4, set_start_frame, C);
	BLI_argsAdd(ba, "-e", 4, set_end_frame, C);
	BLI_argsAdd(ba, "-j", 4, set_skip_frame, C);
	BLI_argsAdd(ba, "-P", 4, run_python, C);
	BLI_argsAdd(ba, "-o", 4, set_output, C);
	BLI_argsAdd(ba, "-E", 4, set_engine, C);
	BLI_argsAdd(ba, "-F", 4, set_image_type, C);
	BLI_argsAdd(ba, "-t", 4, set_threads, NULL);
	BLI_argsAdd(ba, "-x", 4, set_extension, C);

}

int main(int argc, char **argv)
{
	SYS_SystemHandle syshandle;
	bContext *C= CTX_create();
	bArgs *ba;

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

	// copy path to executable in bprogname. playanim and creting runtimes
	// need this.

	BLI_where_am_i(bprogname, argv[0]);
	
	{	/* override the hard coded blender path */
		char *blender_path_env = getenv("BLENDERPATH");
		if(blender_path_env)
			BLI_strncpy(blender_path, blender_path_env, sizeof(blender_path));
	}

#ifdef BUILD_DATE	
    strip_quotes(build_date);
    strip_quotes(build_time);
    strip_quotes(build_rev);
    strip_quotes(build_platform);
    strip_quotes(build_type);
#endif

	BLI_threadapi_init();

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

	G.f |= G_SCRIPT_AUTOEXEC; /* script links enabled by default */

	ba = BLI_argsInit(argc, argv); /* skip binary path */
	setupArguments(C, ba, &syshandle);

	BLI_argsParse(ba, 1, NULL, NULL);

#ifdef __sgi
	setuid(getuid()); /* end superuser */
#endif


	/* for all platforms, even windos has it! */
	if(G.background) signal(SIGINT, blender_esc);	/* ctrl c out bg render */
	
	/* background render uses this font too */
	BKE_font_register_builtin(datatoc_Bfont, datatoc_Bfont_size);

	/* Initialiaze ffmpeg if built in, also needed for bg mode if videos are
	   rendered via ffmpeg */
	sound_init_once();
	
	init_def_material();

	if(G.background==0) {
		BLI_argsParse(ba, 2, NULL, NULL);
		BLI_argsParse(ba, 3, NULL, NULL);

		WM_init(C, argc, argv);
		
		/* this is properly initialized with user defs, but this is default */
		BLI_where_is_temp( btempdir, 1 ); /* call after loading the .B.blend so we can read U.tempdir */

#ifndef DISABLE_SDL
	BLI_setenv("SDL_VIDEODRIVER", "dummy");
/* I think this is not necessary anymore (04-24-2010 neXyon)
#ifdef __linux__
	// On linux the default SDL driver dma often would not play
	// use alsa if none is set
	setenv("SDL_AUDIODRIVER", "alsa", 0);
#endif
*/
#endif
	}
	else {
		BLI_argsParse(ba, 3, NULL, NULL);

		WM_init(C, argc, argv);

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

	// TODO - U.pythondir

#endif
	
	CTX_py_init_set(C, 1);
	WM_keymap_init(C);

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
	BLI_argsParse(ba, 4, load_file, C);

	BLI_argsFree(ba);

	if(G.background) {
		/* actually incorrect, but works for now (ton) */
		WM_exit(C);
	}

	else {
		if((G.fileflags & G_FILE_AUTOPLAY) && (G.f & G_SCRIPT_AUTOEXEC))
			WM_init_game(C);

		else if(!G.file_loaded)
			WM_init_splash(C);
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

static void mem_error_cb(const char *errorStr)
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
