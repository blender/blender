/**
 * $Id: playanim.c 17755 2008-12-09 04:57:42Z bdiego $
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

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#include <sys/times.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include <math.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#ifdef WITH_QUICKTIME
#ifdef _WIN32
#include <QTML.h>
#include <Movies.h>
#elif defined(__APPLE__)
#include <QuickTime/Movies.h>
#endif /* __APPLE__ */
#endif /* WITH_QUICKTIME */

#include "DNA_scene_types.h"
#include "BLI_utildefines.h"
#include "wm_event_types.h"
#include "GHOST_C-api.h"

#define WINCLOSE -1
#define REDRAW -3
#define RESHAPE -2
#define WINQUIT -4

static int qtest(void)
{
	return 0;
}

/* ***************** gl_util.c ****************** */

static GHOST_SystemHandle g_system = NULL;
static void *g_window = NULL;

static int qualN = 0;

#define LSHIFT  (1 << 0)
#define RSHIFT  (1 << 1)
#define SHIFT   (LSHIFT | RSHIFT)
#define LALT    (1 << 2)
#define RALT    (1 << 3)
#define ALT (LALT | RALT)
#define LCTRL   (1 << 4)
#define RCTRL   (1 << 5)
#define LMOUSE  (1 << 16)
#define MMOUSE  (1 << 17)
#define RMOUSE  (1 << 18)
#define MOUSE   (LMOUSE | MMOUSE | RMOUSE)

unsigned short screen_qread(short *val, char *ascii);

void playanim_window_get_size(int *width_r, int *height_r)
{
	GHOST_RectangleHandle bounds = GHOST_GetClientBounds(g_window);
	*width_r = GHOST_GetWidthRectangle(bounds);
	*height_r = GHOST_GetHeightRectangle(bounds);
	GHOST_DisposeRectangle(bounds);
}

/* implementation */
static int qreadN(short *val)
{
#if 0 // XXX25
	char ascii;
	int event = screen_qread(val, &ascii);
#else
	int event = 123456789;
#endif

	switch (event) {
		case LEFTMOUSE:
			if (*val) qualN |= LMOUSE;
			else qualN &= ~LMOUSE;
			break;
		case MIDDLEMOUSE:
			if (*val) qualN |= MMOUSE;
			else qualN &= ~MMOUSE;
			break;
		case RIGHTMOUSE:
			if (*val) qualN |= RMOUSE;
			else qualN &= ~RMOUSE;
			break;
		case LEFTSHIFTKEY:
			if (*val) qualN |= LSHIFT;
			else qualN &= ~LSHIFT;
			break;
		case RIGHTSHIFTKEY:
			if (*val) qualN |= RSHIFT;
			else qualN &= ~RSHIFT;
			break;
		case LEFTCTRLKEY:
			if (*val) qualN |= LCTRL;
			else qualN &= ~LCTRL;
			break;
		case RIGHTCTRLKEY:
			if (*val) qualN |= RCTRL;
			else qualN &= ~RCTRL;
			break;
		case LEFTALTKEY:
			if (*val) qualN |= LALT;
			else qualN &= ~LALT;
			break;
		case RIGHTALTKEY:
			if (*val) qualN |= RALT;
			else qualN &= ~RALT;
			break;
	}

	return(event);
}

/* ***************** gl_util.c ****************** */




typedef struct pict {
	struct pict *next, *prev;
	char *mem;
	int size;
	char *name;
	struct ImBuf *ibuf;
	struct anim *anim;
	int frame;
	int IB_flags;
}Pict;

static struct ListBase _picsbase = {0, 0};
static struct ListBase *picsbase = &_picsbase;
static int fromdisk = FALSE;
static int fstep = 1;
static float zoomx = 1.0, zoomy = 1.0;
static double ptottime = 0.0, swaptime = 0.04;

static int pupdate_time(void)
{
	static double ltime;
	double time;

	time = PIL_check_seconds_timer();

	ptottime += (time - ltime);
	ltime = time;
	return (ptottime < 0);
}

static void toscreen(Pict *picture, struct ImBuf *ibuf)
{

	if (ibuf == 0) {
		printf("no ibuf !\n");
		return;
	}
	if (ibuf->rect == NULL && ibuf->rect_float) {
		IMB_rect_from_float(ibuf);
		imb_freerectfloatImBuf(ibuf);
	}
	if (ibuf->rect == NULL)
		return;

	glRasterPos2f(0.0f, 0.0f);

	glDrawPixels(ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	pupdate_time();

	if (picture && (qualN & (SHIFT | LMOUSE))) {
		char str[512];
		cpack(-1);
		glRasterPos2f(0.02f,  0.03f);
		sprintf(str, "%s | %.2f frames/s\n", picture->name, fstep / swaptime);
#if 0 // XXX25
		BMF_DrawString(G.fonts, str);
#endif
	}

	GHOST_SwapWindowBuffers(g_window);
}

static void build_pict_list(char *first, int totframes, int fstep)
{
	int size, pic, file;
	char *mem, name[512];
//	short val;
	struct pict *picture = 0;
	struct ImBuf *ibuf = 0;
	int count = 0;
	char str[512];
	struct anim *anim;

	if (IMB_isanim(first)) {
		anim = IMB_open_anim(first, IB_rect, 0);
		if (anim) {
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			if (ibuf) {
				toscreen(NULL, ibuf);
				IMB_freeImBuf(ibuf);
			}

			for (pic = 0; pic < IMB_anim_get_duration(anim, IMB_TC_NONE); pic++) {
				picture = (Pict *)MEM_callocN(sizeof(Pict), "Pict");
				picture->anim = anim;
				picture->frame = pic;
				picture->IB_flags = IB_rect;
				sprintf(str, "%s : %d", first, pic + 1);
				picture->name = strdup(str);
				BLI_addtail(picsbase, picture);
			}
		}
		else printf("couldn't open anim %s\n", first);
	}
	else {

		strcpy(name, first);

		pupdate_time();
		ptottime = 1.0;

/*
     O_DIRECT
            If set, all reads and writes on the resulting file descriptor will
            be performed directly to or from the user program buffer, provided
            appropriate size and alignment restrictions are met.  Refer to the
            F_SETFL and F_DIOINFO commands in the fcntl(2) manual entry for
            information about how to determine the alignment constraints.
            O_DIRECT is a Silicon Graphics extension and is only supported on
            local EFS and XFS file systems.
 */

		while (IMB_ispic(name) && totframes) {
			file = open(name, O_BINARY | O_RDONLY, 0);
			if (file < 0) return;
			picture = (struct pict *)MEM_callocN(sizeof(struct pict), "picture");
			if (picture == 0) {
				printf("Not enough memory for pict struct \n");
				close(file);
				return;
			}
			size = BLI_file_descriptor_size(file);

			if (size < 1) {
				close(file);
				MEM_freeN(picture);
				return;
			}

			picture->size = size;
			picture->IB_flags = IB_rect;

			if (fromdisk == FALSE) {
				mem = (char *)MEM_mallocN(size, "build pic list");
				if (mem == 0) {
					printf("Couldn't get memory\n");
					close(file);
					MEM_freeN(picture);
					return;
				}

				if (read(file, mem, size) != size) {
					printf("Error while reading %s\n", name);
					close(file);
					MEM_freeN(picture);
					MEM_freeN(mem);
					return;
				}
			}
			else mem = 0;

			picture->mem = mem;
			picture->name = strdup(name);
			close(file);
			BLI_addtail(picsbase, picture);
			count++;

			pupdate_time();

			if (ptottime > 1.0) {
				if (picture->mem) ibuf = IMB_ibImageFromMemory((unsigned char *)picture->mem, picture->size, picture->IB_flags, picture->name);
				else ibuf = IMB_loadiffname(picture->name, picture->IB_flags);
				if (ibuf) {
					toscreen(picture, ibuf);
					IMB_freeImBuf(ibuf);
				}
				pupdate_time();
				ptottime = 0.0;
			}

			BLI_newname(name, +fstep);

#if 0 // XXX25
			while (qtest()) {
				switch (qreadN(&val)) {
					case ESCKEY:
						if (val) return;
						break;
				}
			}
#endif
			totframes--;
		}
	}
	return;
}

static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr C_void_ptr)
{
	(void)evt;
	(void)C_void_ptr;

	return 1;
}

void playanim_window_open(const char *title, int posx, int posy, int sizex, int sizey, int start_maximized)
{
	GHOST_TWindowState inital_state;
	GHOST_TUns32 scr_w, scr_h;

	GHOST_GetMainDisplayDimensions(g_system, &scr_w, &scr_h);

	posy = (scr_h - posy - sizey);

	if (start_maximized == G_WINDOWSTATE_FULLSCREEN)
		inital_state = start_maximized ? GHOST_kWindowStateFullScreen : GHOST_kWindowStateNormal;
	else
		inital_state = start_maximized ? GHOST_kWindowStateMaximized : GHOST_kWindowStateNormal;
#ifdef __APPLE__
	inital_state += macPrefState;
#endif

	g_window = GHOST_CreateWindow(g_system,
	                              title,
	                              posx, posy, sizex, sizey,
	                              inital_state,
	                              GHOST_kDrawingContextTypeOpenGL,
	                              FALSE /* no stereo */, FALSE);

	//if (ghostwin) {
	//if (win) {
	// GHOST_SetWindowUserData(ghostwin, win);
	//} else {
	//	GHOST_DisposeWindow(g_system, ghostwin);
	//}
	//}
}


void playanim(int argc, const char **argv)
{
	struct ImBuf *ibuf = 0;
	struct pict *picture = 0;
	char name[512];
	short val = 0, go = TRUE, ibufx = 0, ibufy = 0;
	int event, stopped = FALSE;
	GHOST_TUns32 maxwinx, maxwiny;
	short /*  c233 = FALSE, */ /*  yuvx = FALSE, */ once = FALSE, sstep = FALSE, wait2 = FALSE, /*  resetmap = FALSE, */ pause = 0;
	short pingpong = FALSE, direction = 1, next = 1, turbo = FALSE, /*  doubleb = TRUE, */ noskip = FALSE;
	int sizex, sizey, ofsx, ofsy, i;
	/* This was done to disambiguate the name for use under c++. */
	struct anim *anim = 0;
	int start_x = 0, start_y = 0;
	int sfra = -1;
	int efra = -1;
	int totblock;

	while (argc > 1) {
		if (argv[1][0] == '-') {
			switch (argv[1][1]) {
				case 'm':
					fromdisk = TRUE;
					break;
				case 'p':
					if (argc > 3) {
						start_x = atoi(argv[2]);
						start_y = atoi(argv[3]);
						argc -= 2;
						argv += 2;
					}
					else {
						printf("too few arguments for -p (need 2): skipping\n");
					}
					break;
				case 'f':
					if (argc > 3) {
						double fps = atof(argv[2]);
						double fps_base = atof(argv[3]);
						if (fps == 0) {
							fps = 1;
							printf("invalid fps,"
							       "forcing 1\n");
						}
						swaptime = fps_base / fps;
						argc -= 2;
						argv += 2;
					}
					else {
						printf("too few arguments for -f (need 2): skipping\n");
					}
					break;
				case 's':
					sfra = MIN2(MAXFRAME, MAX2(1, atoi(argv[2]) ));
					argc--;
					argv++;
					break;
				case 'e':
					efra = MIN2(MAXFRAME, MAX2(1, atoi(argv[2]) ));
					argc--;
					argv++;
					break;
				case 'j':
					fstep = MIN2(MAXFRAME, MAX2(1, atoi(argv[2])));
					swaptime *= fstep;
					argc--;
					argv++;
					break;
				default:
					printf("unknown option '%c': skipping\n", argv[1][1]);
					break;
			}
			argc--;
			argv++;
		}
		else break;
	}

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)
	/* Initialize QuickTime */
#ifndef noErr
#define noErr 0
#endif

#ifdef _WIN32
	if (InitializeQTML(0) != noErr)
		G.have_quicktime = FALSE;
	else
		G.have_quicktime = TRUE;
#endif /* _WIN32 */
	if (EnterMovies() != noErr)
		G.have_quicktime = FALSE;
	else
#endif /* _WIN32 || __APPLE__ */
	G.have_quicktime = TRUE;
#endif /* WITH_QUICKTIME */

	if (argc > 1) strcpy(name, argv[1]);
	else {
		BLI_current_working_dir(name, sizeof(name));
		if (name[strlen(name) - 1] != '/') strcat(name, "/");
	}

	if (IMB_isanim(name)) {
		anim = IMB_open_anim(name, IB_rect, 0);
		if (anim) {
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			IMB_close_anim(anim);
			anim = NULL;
		}
	}
	else if (!IMB_ispic(name)) {
		exit(1);
	}

	if (ibuf == 0) ibuf = IMB_loadiffname(name, IB_rect);
	if (ibuf == 0) {
		printf("couldn't open %s\n", name);
		exit(1);
	}

#if 0 //XXX25
	#if !defined(WIN32) && !defined(__APPLE__)
	if (fork()) exit(0);
	#endif
#endif //XXX25

	/* XXX, fixme zr */
	{
//		extern void add_to_mainqueue(wmWindow *win, void *user_data, short evt, short val, char ascii);

		void *some_handle = NULL; // XXX25, fixme
		GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(ghost_event_proc, some_handle);

		g_system = GHOST_CreateSystem();
		GHOST_AddEventConsumer(g_system, consumer);



		playanim_window_open("Blender:Anim", start_x, start_y, ibuf->x, ibuf->y, 0);
//XXX25		window_set_handler(g_window, add_to_mainqueue, NULL);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
		glMatrixMode(GL_MODELVIEW);
	}

	GHOST_GetMainDisplayDimensions(g_system, &maxwinx, &maxwiny);

#if 0 //XXX25
	G.fonts = BMF_GetFont(BMF_kHelvetica10);
#endif

	ibufx = ibuf->x;
	ibufy = ibuf->y;

	if (maxwinx % ibuf->x) maxwinx = ibuf->x * (1 + (maxwinx / ibuf->x));
	if (maxwiny % ibuf->y) maxwiny = ibuf->y * (1 + (maxwiny / ibuf->y));

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	GHOST_SwapWindowBuffers(g_window);

	if (sfra == -1 || efra == -1) {
		/* one of the frames was invalid, just use all images */
		sfra = 1;
		efra = MAXFRAME;
	}

	build_pict_list(name, (efra - sfra) + 1, fstep);

	for (i = 2; i < argc; i++) {
		strcpy(name, argv[i]);
		build_pict_list(name, (efra - sfra) + 1, fstep);
	}

	IMB_freeImBuf(ibuf);
	ibuf = 0;

	pupdate_time();
	ptottime = 0;

	while (go) {
		if (pingpong) direction = -direction;

		if (direction == 1) picture = picsbase->first;
		else picture = picsbase->last;

		if (picture == 0) {
			printf("couldn't find pictures\n");
			go = FALSE;
		}
		if (pingpong) {
			if (direction == 1) picture = picture->next;
			else picture = picture->prev;
		}
		if (ptottime > 0.0) ptottime = 0.0;

		while (picture) {
			if (ibuf != 0 && ibuf->ftype == 0) IMB_freeImBuf(ibuf);

			if (picture->ibuf) ibuf = picture->ibuf;
			else if (picture->anim) ibuf = IMB_anim_absolute(picture->anim, picture->frame, IMB_TC_NONE, IMB_PROXY_NONE);
			else if (picture->mem) ibuf = IMB_ibImageFromMemory((unsigned char *) picture->mem, picture->size, picture->IB_flags, picture->name);
			else ibuf = IMB_loadiffname(picture->name, picture->IB_flags);

			if (ibuf) {
				strcpy(ibuf->name, picture->name);

#ifdef _WIN32
				window_set_title(g_window, picture->name);
#endif

				while (pupdate_time()) PIL_sleep_ms(1);
				ptottime -= swaptime;
				toscreen(picture, ibuf);
			} /* else deleten */
			else {
				printf("error: can't play this image type\n");
				exit(0);
			}

			if (once) {
				if (picture->next == 0) wait2 = TRUE;
				else if (picture->prev == 0) wait2 = TRUE;
			}

			next = direction;

			while ((qtest() != 0) || (wait2 != 0)) {
				if (wait2 && stopped) {
					stopped = FALSE;
				}

				event = qreadN(&val);
				/* printf("%d %d\n", event, val); */

				if (wait2) {
					pupdate_time();
					ptottime = 0;
				}
				switch (event) {
					case AKEY:
						if (val)
							noskip = !noskip;
						break;
					case PKEY:
						if (val)
							pingpong = !pingpong;
						break;
					case SLASHKEY:
						if (val) {
							if (qualN & SHIFT) {
								if (ibuf)
									printf(" Name: %s | Speed: %.2f frames/s\n", ibuf->name, fstep / swaptime);
							}
							else {
								swaptime = fstep / 5.0;
							}
						}
						break;
					case LEFTARROWKEY:
						if (val) {
							sstep = TRUE;
							wait2 = FALSE;
							if (qualN & SHIFT) {
								picture = picsbase->first;
								next = 0;
							}
							else {
								next = -1;
							}
						}
						break;
					case DOWNARROWKEY:
						if (val) {
							wait2 = FALSE;
							if (qualN & SHIFT) {
								next = direction = -1;
							}
							else {
								next = -10;
								sstep = TRUE;
							}
						}
						break;
					case RIGHTARROWKEY:
						if (val) {
							sstep = TRUE;
							wait2 = FALSE;
							if (qualN & SHIFT) {
								picture = picsbase->last;
								next = 0;
							}
							else {
								next = 1;
							}
						}
						break;
					case UPARROWKEY:
						if (val) {
							wait2 = FALSE;
							if (qualN & SHIFT) {
								next = direction = 1;
							}
							else {
								next = 10;
								sstep = TRUE;
							}
						}
						break;
					case LEFTMOUSE:
					case MOUSEX:
						if (qualN & LMOUSE) {
							playanim_window_get_size(&sizex, &sizey);
							picture = picsbase->first;
							i = 0;
							while (picture) {
								i++;
								picture = picture->next;
							}
							i = (i * val) / sizex;
							picture = picsbase->first;
							for (; i > 0; i--) {
								if (picture->next == 0) break;
								picture = picture->next;
							}
							sstep = TRUE;
							wait2 = FALSE;
							next = 0;
						}
						break;
						go = FALSE;
						break;
					case EQUALKEY:
						if (val) {
							if (qualN & SHIFT) {
								pause++;
								printf("pause:%d\n", pause);
							}
							else swaptime /= 1.1;
						}
						break;
					case MINUSKEY:
						if (val) {
							if (qualN & SHIFT) {
								pause--;
								printf("pause:%d\n", pause);
							}
							else swaptime *= 1.1;
						}
						break;
					case PAD0:
						if (val) {
							if (once) once = wait2 = FALSE;
							else {
								picture = 0;
								once = TRUE;
								wait2 = FALSE;
							}
						}
						break;
					case RETKEY:
					case PADENTER:
						if (val) {
							wait2 = sstep = FALSE;
						}
						break;
					case PADPERIOD:
						if (val) {
							if (sstep) wait2 = FALSE;
							else {
								sstep = TRUE;
								wait2 = !wait2;
							}
						}
						break;
					case PAD1:
						swaptime = fstep / 60.0;
						break;
					case PAD2:
						swaptime = fstep / 50.0;
						break;
					case PAD3:
						swaptime = fstep / 30.0;
						break;
					case PAD4:
						if (qualN & SHIFT)
							swaptime = fstep / 24.0;
						else
							swaptime = fstep / 25.0;
						break;
					case PAD5:
						swaptime = fstep / 20.0;
						break;
					case PAD6:
						swaptime = fstep / 15.0;
						break;
					case PAD7:
						swaptime = fstep / 12.0;
						break;
					case PAD8:
						swaptime = fstep / 10.0;
						break;
					case PAD9:
						swaptime = fstep / 6.0;
						break;
					case PADPLUSKEY:
						if (val == 0) break;
						zoomx += 2.0;
						zoomy += 2.0;
					case PADMINUS:
						if (val == 0) break;
						if (zoomx > 1.0) zoomx -= 1.0;
						if (zoomy > 1.0) zoomy -= 1.0;
						// playanim_window_get_position(&ofsx, &ofsy);
						playanim_window_get_size(&sizex, &sizey);
						ofsx += sizex / 2;
						ofsy += sizey / 2;
						sizex = zoomx * ibufx;
						sizey = zoomy * ibufy;
						ofsx -= sizex / 2;
						ofsy -= sizey / 2;
						// window_set_position(g_window,sizex,sizey);
						GHOST_SetClientSize(g_window, sizex, sizey);
						break;
					case RESHAPE:
					case REDRAW:
						playanim_window_get_size(&sizey, &sizey);
						GHOST_ActivateWindowDrawingContext(g_window);

						glViewport(0,  0, sizex, sizey);
						glScissor(0,  0, sizex, sizey);

						zoomx = (float) sizex / ibufx;
						zoomy = (float) sizey / ibufy;
						zoomx = floor(zoomx + 0.5);
						zoomy = floor(zoomy + 0.5);
						if (zoomx < 1.0) zoomx = 1.0;
						if (zoomy < 1.0) zoomy = 1.0;

						sizex = zoomx * ibufx;
						sizey = zoomy * ibufy;

						glPixelZoom(zoomx, zoomy);
						glEnable(GL_DITHER);
						ptottime = 0.0;
						toscreen(picture, ibuf);
						while (qtest()) qreadN(&val);

						break;
					case ESCKEY:
					case WINCLOSE:
					case WINQUIT:
						go = FALSE;
						break;
				}
				if (go == FALSE) break;
			}

			wait2 = sstep;

			if (wait2 == 0 && stopped == 0) {
				stopped = TRUE;
			}

			pupdate_time();

			if (picture && next) {
				/* always at least set one step */
				while (picture) {
					if (next < 0) picture = picture->prev;
					else picture = picture->next;

					if (once && picture != 0) {
						if (picture->next == 0) wait2 = TRUE;
						else if (picture->prev == 0) wait2 = TRUE;
					}

					if (wait2 || ptottime < swaptime || turbo || noskip) break;
					ptottime -= swaptime;
				}
				if (picture == 0 && sstep) {
					if (next < 0) picture = picsbase->last;
					else if (next > 0) picture = picsbase->first;
				}
			}
			if (go == FALSE) break;
		}
	}
	picture = picsbase->first;
	anim = NULL;
	while (picture) {
		if (picture && picture->anim && (anim != picture->anim)) {
			// to prevent divx crashes
			anim = picture->anim;
			IMB_close_anim(anim);
		}
		if (picture->ibuf) IMB_freeImBuf(picture->ibuf);
		if (picture->mem) MEM_freeN(picture->mem);

		picture = picture->next;
	}
#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__)
	if (G.have_quicktime) {
		ExitMovies();
#ifdef _WIN32
		TerminateQTML();
#endif /* _WIN32 */
	}
#endif /* _WIN32 || __APPLE__ */
#endif /* WITH_QUICKTIME */

	/* cleanup */
	if (ibuf) IMB_freeImBuf(ibuf);
	BLI_freelistN(picsbase);
	free_blender();
	GHOST_DisposeWindow(g_system, g_window);

	totblock = MEM_get_memory_blocks_in_use();
	if (totblock != 0) {
		printf("Error Totblock: %d\n", totblock);
		MEM_printmemlist();
	}
}
