/* 
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
 */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* This little block needed for linking to Blender... */

#include "MEM_guardedalloc.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "GEN_messaging.h"

#include "DNA_ID.h"

#include "BLI_blenlib.h"

#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_mainqueue.h"
#include "BIF_graphics.h"
#include "BIF_editsound.h"
#include "BIF_usiblender.h"
#include "BIF_drawscene.h"      /* set_scene() */
#include "BIF_screen.h"         /* waitcursor and more */
#include "BIF_usiblender.h"
#include "BIF_toolbox.h"

#include "BLO_writefile.h"
#include "BLO_readfile.h"

#include "BPY_extern.h"  // for init of blender python extension

#include "BSE_headerbuttons.h" // for BIF_read_homefile

#include "BDR_drawmesh.h"

#include "RE_renderconverter.h"

#include "playanim_ext.h"
#include "mydevice.h"
#include "render.h"
#include "nla.h"

/* for passing information between creator and gameengine */
#include "SYS_System.h"

#include <signal.h>
#ifdef __FreeBSD__
  #ifndef __OpenBSD__
    #include <floatingpoint.h>
    #include <sys/rtprio.h>
  #endif
#endif

#ifdef WITH_QUICKTIME
#	ifdef _WIN32
#		include <QTML.h>
#	endif /* _WIN32 */
#	if defined (_WIN32) || defined (__APPLE__)
#		include <Movies.h>
#	elif defined (__linux__)
#		include <quicktime/lqt.h>
#	endif /* __linux__ */
#endif /* WITH_QUICKTIME */

// from buildinfo.c
extern char * build_date;
extern char * build_time;
extern char * build_platform;
extern char * build_type;

/*	Local Function prototypes */
static void print_help();

/* for the callbacks: */

extern int pluginapi_force_ref(void);  /* from blenpluginapi:pluginapi.c */

char bprogname[FILE_MAXDIR+FILE_MAXFILE];

/* Initialise callbacks for the modules that need them */
void setCallbacks(void); 

static void fpe_handler(int sig)
{
	// printf("SIGFPE trapped\n");
}

static void print_help(void)
{
				printf ("Blender V %d.%02d\n", G.version/100, G.version%100);
				printf ("Usage: blender [options ...] [file]\n");
				
				printf ("\nRender options:\n");
				printf ("  -b <file>\tRender <file> in background\n");
				printf ("    -S <name>\tSet scene <name>\n");				
				printf ("    -f <frame>\tRender frame <frame> and save it\n");				
				printf ("    -s <frame>\tSet start to frame <frame> (use with -a)\n");
				printf ("    -e <frame>\tSet end to frame (use with -a)<frame>\n");
				printf ("    -a\t\tRender animation\n");
				
				printf ("\nAnimation options:\n");
				printf ("  -a <file(s)>\tPlayback <file(s)>\n");
				printf ("    -p <sx> <sy>\tOpen with lower left corner at <sx>, <sy>\n");
				printf ("    -m\t\tRead from disk (Don't buffer)\n");
				
				printf ("\nWindow options:\n");
				printf ("  -w\t\tForce opening with borders\n");
#ifdef WIN32
				printf ("  -W\t\tForce opening without borders\n");
#endif				
				printf ("  -p <sx> <sy> <w> <h>\tOpen with lower left corner at <sx>, <sy>\n");
				printf ("                      \tand width and height <w>, <h>\n");
				printf ("\nGame Engine specific options:\n");
				printf ("  -g fixedtime\t\tRun on 50 hertz without dropping frames\n");
				printf ("  -g vertexarrays\tUse Vertex Arrays for rendering (usually faster)\n");
				printf ("  -g noaudio\t\tNo audio in Game Engine\n");
				printf ("  -g nomipmap\t\tNo Texture Mipmapping\n");
				printf ("  -g linearmipmap\tLinear Texture Mipmapping instead of Nearest (default)\n");
				
				
				
				
				printf ("\nMisc options:\n");
				printf ("  -d\t\tTurn debugging on\n");
				printf ("  -noaudio\tDisable audio on systems that support audio\n");
				printf ("  -h\t\tPrint this help text\n");
				printf ("  -y\t\tDisable OnLoad scene scripts, use -Y to find out why its -y\n");
#ifdef WIN32
				printf ("  -R\t\tRegister .blend extension\n");
#endif
}


double PIL_check_seconds_timer(void);
extern void winlay_get_screensize(int *width_r, int *height_r);
int main(int argc, char **argv)	
{
	int a, i, stax, stay, sizx, sizy;
	SYS_SystemHandle syshandle;
	Scene *sce;

#if defined(WIN32) || defined (__linux__)
	int audio = 1;
#else
	int audio = 0;
#endif

	setCallbacks();

#ifdef __APPLE__
		/* patch to ignore argument finder gives us (pid?) */
	if (argc==2 && strncmp(argv[1], "-psn_", 5)==0) {
		extern int GHOST_HACK_getFirstFile(char buf[]);
		static char firstfilebuf[512];
		int scr_x,scr_y;
		
		argc= 1;
		
		setprefsize(100, 100, 800, 600);
		
		winlay_get_screensize(&scr_x, &scr_y);
		winlay_process_events(0);
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
	
		/* Hack - force inclusion of the plugin api functions,
		 * see blenpluginapi:pluginapi.c
		 */
	pluginapi_force_ref();
	
	initglobals();	/* blender.c */

	syshandle = SYS_GetSystem();
	GEN_init_messaging_system();

	/* eerste testen op background */
	G.f |= G_SCENESCRIPT; /* scenescript always set! */
	for(a=1; a<argc; a++) {

		/* Handle unix and windows style help requests */
		if ((!strcmp(argv[a], "--help")) || (!strcmp(argv[a], "/?"))){
			print_help();
			exit(0);
		}

		/* Handle -* switches */
		else if(argv[a][0] == '-') {
			switch(argv[a][1]) {
			case 'a':
				playanim(argc-1, argv+1);
				exit(0);
				break;
			case 'b':
			case 'B':
				G.background = 1;
				a= argc;
				break;

         case 'm':
             /* unified render pipeline */
/*               G.magic = 1; has become obsolete */
			 printf("-m: enable unified renderer has become obsolete. Set \n");
			 printf("\tthis option per individual file now.\n");
             break;

			case 'y':
				G.f &= ~G_SCENESCRIPT;
				break;

			case 'Y':
				printf ("-y was used to disable scene scripts because,\n");
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
								
			default:
				break;
			}
		}
	}

#ifdef __sgi
	setuid(getuid()); /* einde superuser */
#endif

	RE_init_render_data();	/* moet vooraan staan ivm R.winpos uit defaultfile */
	
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
	
					setprefsize(stax, stay, sizx, sizy);
					break;
				case 'd':
					G.f |= G_DEBUG;		/* std output printf's */ 
					printf ("Blender V %d.%02d\n", G.version/100, G.version%100);
#ifdef NAN_BUILDINFO
					printf("Build: %s %s %s %s\n", build_date, build_time, build_platform, build_type);

#endif // NAN_BUILDINFO
					for (i = 0; i < argc; i++) {
						printf("argv[%d] = %s\n", i, argv[i]);
					}
					break;
            
				case 'w':
					/* XXX, fixme zr, with borders */
					/* there probably is a better way to do
					 * this, right now do as if blender was
					 * called with "-p 0 0 xres yres" -- sgefant
					 */ 
					winlay_get_screensize(&sizx, &sizy);
					setprefsize(0, 0, sizx, sizy);
					break;
				case 'W':
						/* XXX, fixme zr, borderless on win32 */
					break;
				case 'n':
				case 'N':
					if (strcasecmp(argv[a], "-noaudio") == 0|| strcasecmp(argv[a], "-nosound") == 0) {
						/**
							notify the gameengine that no audio is wanted, even if the user didn't give
							the flag -g noaudio 
					*/

						SYS_WriteCommandLineInt(syshandle,"noaudio",1);
						audio = 0;
						if (G.f & G_DEBUG) printf("setting audio to: %d\n", audio);
					}
					else if (strcasecmp(argv[a], "-nofrozen") == 0) {
						/* disable initialization of frozen python modules */
						if (G.f & G_DEBUG) printf("disable frozen modules\n");
						G.f |= G_NOFROZEN;
					}
					break;
				}
			}
		}

		BPY_start_python();
		
		/* NOTE: initialize_sound *must be* after start_python,
		 * at least on FreeBSD */

		sound_init_audio();

		BIF_init();
	}
	else {
		BPY_start_python();
		SYS_WriteCommandLineInt(syshandle,"noaudio",1);
        audio = 0;
        sound_init_audio();
        if (G.f & G_DEBUG) printf("setting audio to: %d\n", audio);
	}

	RE_init_filt_mask();
	
#ifdef WITH_QUICKTIME
#ifdef _WIN32
        if (InitializeQTML(0) != noErr)
            G.have_quicktime = FALSE;
        else
            G.have_quicktime = TRUE;
#endif /* _WIN32 */

        /* Initialize QuickTime */
#if defined(_WIN32) || defined (__APPLE__)
        if (EnterMovies() != noErr)
            G.have_quicktime = FALSE;
        else
#endif /* _WIN32 || __APPLE__ */
#ifdef __linux__
			/* inititalize quicktime codec registry */
			lqt_registry_init();
#endif
			G.have_quicktime = TRUE;
#endif /* WITH_QUICKTIME */

		/* OK we zijn er klaar voor */

	for(a=1; a<argc; a++) {
		if (G.afbreek==1) break;

		if(argv[a][0] == '-') {
			switch(argv[a][1]) {
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
								set_mipmap(0); //doMipMap = 0;
							}
							/* linearMipMap */
							if (!strcmp(argv[a],"linearmipmap"))
							{
								set_linear_mipmap(1); //linearMipMap = 1;
							}
						

						} /* if (*(argv[a+1]) == '=') */
					} /*	if(++a < argc)  */
					break;
				}
			case 'f':
				a++;
				if (G.scene && a < argc) {
					G.real_sfra = (G.scene->r.sfra);
					G.real_efra = (G.scene->r.efra);
					(G.scene->r.sfra) = atoi(argv[a]);
					(G.scene->r.efra) = (G.scene->r.sfra);
					RE_animrender(NULL);
				}
				break;
			case 'a':
				if (G.scene) {
					G.real_sfra = (G.scene->r.sfra);
					G.real_efra = (G.scene->r.efra);
					RE_animrender(NULL);
				}
				break;
			case 'S':
				if(++a < argc) {
					set_scene_name(argv[a]);
				}
				break;
			case 's':
				a++;
				if(G.scene) {
					if (a < argc) (G.scene->r.sfra) = atoi(argv[a]);
				}
				break;
			case 'e':
				a++;
				if(G.scene) {
					if (a < argc) (G.scene->r.efra) = atoi(argv[a]);
				}
				break;
			case 'R':
				/* Registering filetypes only makes sense on windows...      */
#ifdef WIN32
				RegisterBlendExtension(argv[0]);
#endif
				break;
			}
		}
		else {
			BKE_read_file(argv[a], NULL);
			sound_initialize_sounds();
		}
	}
	
	if(G.background) 
	{
		exit_usiblender();
	}

	setscreen(G.curscreen);
	
	if(G.main->scene.first==0) {
		sce= add_scene("1");
		set_scene(sce);
	}
	
	screenmain();

	return 0;
} /* end of int main(argc,argv)	*/

static void error_cb(char *err)
{
	error("%s", err);
}

void setCallbacks(void)
{
	/* Error output from the alloc routines: */
	MEM_set_error_stream(stderr);

	
	/* BLI_blenlib: */
	
	BLI_setErrorCallBack(error_cb); /* */
	BLI_setInterruptCallBack(blender_test_break);

	/* render module: execution flow, timers, cursors and display. */
	RE_set_getrenderdata_callback(RE_rotateBlenderScene);
	RE_set_freerenderdata_callback(RE_freeRotateBlenderScene);
}
