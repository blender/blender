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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_playanim.c
 *  \ingroup wm
 *
 * \note This file uses ghost directly and none of the WM definitions.
 *       this could be made into its own module, alongside creator/
 */

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#  include <unistd.h>
#  include <sys/times.h>
#  include <sys/wait.h>
#else
#  include <io.h>
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
#  ifdef _WIN32
#    include <QTML.h>
#    include <Movies.h>
#  elif defined(__APPLE__)
#    include <QuickTime/Movies.h>
#  endif /* __APPLE__ */
#endif /* WITH_QUICKTIME */

#include "DNA_scene_types.h"
#include "BLI_utildefines.h"
#include "ED_datafiles.h" /* for fonts */
#include "wm_event_types.h"
#include "GHOST_C-api.h"
#include "BLF_api.h"

#define WINCLOSE -1
#define REDRAW -3
#define RESHAPE -2
#define WINQUIT -4

/* use */
typedef struct PlayState {
	short direction;
	short next;
	short once;
	short turbo;
	short pingpong;
	short noskip;
	short sstep;
	short pause;
	short wait2;
	short stopped;
	short go;

	struct PlayAnimPict *picture;

	/* set once at the start */
	int ibufx, ibufy;
	int fontid;

	/* saves passing args */
	struct ImBuf *curframe_ibuf;
} PlayState;

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
static void playanim_event_qual_update(void)
{
	int val;

	/* Shift */
	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyLeftShift, &val);
	if (val) qualN |=  LSHIFT;
	else     qualN &= ~LSHIFT;

	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyRightShift, &val);
	if (val) qualN |=  RSHIFT;
	else     qualN &= ~RSHIFT;

	/* Control */
	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyLeftControl, &val);
	if (val) qualN |=  LCTRL;
	else     qualN &= ~LCTRL;

	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyRightControl, &val);
	if (val) qualN |=  RCTRL;
	else     qualN &= ~RCTRL;

	/* Alt */
	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyLeftAlt, &val);
	if (val) qualN |=  LCTRL;
	else     qualN &= ~LCTRL;

	GHOST_GetModifierKeyState(g_system, GHOST_kModifierKeyRightAlt, &val);
	if (val) qualN |=  RCTRL;
	else     qualN &= ~RCTRL;

	/* LMB */
	GHOST_GetButtonState(g_system, GHOST_kButtonMaskLeft, &val);
	if (val) qualN |=  LMOUSE;
	else     qualN &= ~LMOUSE;

	/* MMB */
	GHOST_GetButtonState(g_system, GHOST_kButtonMaskMiddle, &val);
	if (val) qualN |=  MMOUSE;
	else     qualN &= ~MMOUSE;

	/* RMB */
	GHOST_GetButtonState(g_system, GHOST_kButtonMaskRight, &val);
	if (val) qualN |=  RMOUSE;
	else     qualN &= ~RMOUSE;
}

typedef struct PlayAnimPict {
	struct PlayAnimPict *next, *prev;
	char *mem;
	int size;
	char *name;
	struct ImBuf *ibuf;
	struct anim *anim;
	int frame;
	int IB_flags;
} PlayAnimPict;

static struct ListBase _picsbase = {NULL, NULL};
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

static void toscreen(PlayAnimPict *picture, struct ImBuf *ibuf, int fontid)
{

	if (ibuf == NULL) {
		printf("no ibuf !\n");
		return;
	}
	if (ibuf->rect == NULL && ibuf->rect_float) {
		IMB_rect_from_float(ibuf);
		imb_freerectfloatImBuf(ibuf);
	}
	if (ibuf->rect == NULL)
		return;

	GHOST_ActivateWindowDrawingContext(g_window);

	glRasterPos2f(0.0f, 0.0f);

	glDrawPixels(ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	pupdate_time();

	if (picture && (qualN & (SHIFT | LMOUSE)) && (fontid != -1)) {
		char str[32 + FILE_MAX];
		cpack(-1);
//		glRasterPos2f(0.02f, 0.03f);
		BLI_snprintf(str, sizeof(str), "%s | %.2f frames/s", picture->name, fstep / swaptime);
//		BMF_DrawString(font, str);

		BLF_enable(fontid, BLF_ASPECT);
		BLF_aspect(fontid, 1.0f / ibuf->x, 1.0f / ibuf->y, 1.0f);
		BLF_position(fontid, 0.02f, 0.03f, 0.0f);
		BLF_draw(fontid, str, 256); // XXX
		printf("Drawing text '%s'\n", str);
	}

	GHOST_SwapWindowBuffers(g_window);
}

static void build_pict_list(char *first, int totframes, int fstep, int fontid)
{
	char *mem, filepath[FILE_MAX];
//	short val;
	PlayAnimPict *picture = NULL;
	struct ImBuf *ibuf = NULL;
	char str[32 + FILE_MAX];
	struct anim *anim;

	if (IMB_isanim(first)) {
		anim = IMB_open_anim(first, IB_rect, 0);
		if (anim) {
			int pic;
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			if (ibuf) {
				toscreen(NULL, ibuf, fontid);
				IMB_freeImBuf(ibuf);
			}

			for (pic = 0; pic < IMB_anim_get_duration(anim, IMB_TC_NONE); pic++) {
				picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "Pict");
				picture->anim = anim;
				picture->frame = pic;
				picture->IB_flags = IB_rect;
				BLI_snprintf(str, sizeof(str), "%s : %d", first, pic + 1);
				picture->name = strdup(str);
				BLI_addtail(picsbase, picture);
			}
		}
		else {
			printf("couldn't open anim %s\n", first);
		}
	}
	else {
		int count = 0;

		BLI_strncpy(filepath, first, sizeof(filepath));

		pupdate_time();
		ptottime = 1.0;

		/* O_DIRECT
		 *
		 * If set, all reads and writes on the resulting file descriptor will
		 * be performed directly to or from the user program buffer, provided
		 * appropriate size and alignment restrictions are met.  Refer to the
		 * F_SETFL and F_DIOINFO commands in the fcntl(2) manual entry for
		 * information about how to determine the alignment constraints.
		 * O_DIRECT is a Silicon Graphics extension and is only supported on
		 * local EFS and XFS file systems.
		 */

		while (IMB_ispic(filepath) && totframes) {
			size_t size;
			int file;

			file = open(filepath, O_BINARY | O_RDONLY, 0);
			if (file < 0) return;
			picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "picture");
			if (picture == NULL) {
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
				if (mem == NULL) {
					printf("Couldn't get memory\n");
					close(file);
					MEM_freeN(picture);
					return;
				}

				if (read(file, mem, size) != size) {
					printf("Error while reading %s\n", filepath);
					close(file);
					MEM_freeN(picture);
					MEM_freeN(mem);
					return;
				}
			}
			else {
				mem = NULL;
			}

			picture->mem = mem;
			picture->name = strdup(filepath);
			close(file);
			BLI_addtail(picsbase, picture);
			count++;

			pupdate_time();

			if (ptottime > 1.0) {
				if (picture->mem) {
					ibuf = IMB_ibImageFromMemory((unsigned char *)picture->mem, picture->size,
					                             picture->IB_flags, picture->name);
				}
				else {
					ibuf = IMB_loadiffname(picture->name, picture->IB_flags);
				}
				if (ibuf) {
					toscreen(picture, ibuf, fontid);
					IMB_freeImBuf(ibuf);
				}
				pupdate_time();
				ptottime = 0.0;
			}

			BLI_newname(filepath, +fstep);

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

static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr ps_void)
{
	PlayState *ps = (PlayState *)ps_void;

	GHOST_TEventType type = GHOST_GetEventType(evt);
	int val;

	playanim_event_qual_update();

	/* convert ghost event into value keyboard or mouse */
	val = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventButtonDown);

	if (ps->wait2 && ps->stopped) {
		ps->stopped = FALSE;
	}

	switch (type) {
		case GHOST_kEventKeyDown:
		case GHOST_kEventKeyUp:
		{
			GHOST_TEventKeyData *key_data;

			key_data = (GHOST_TEventKeyData *)GHOST_GetEventData(evt);
			switch (key_data->key) {
				case GHOST_kKeyA:
					if (val) ps->noskip = !ps->noskip;
					break;
				case GHOST_kKeyP:
					if (val) ps->pingpong = !ps->pingpong;
					break;
				case GHOST_kKeyNumpad1:
					if (val) swaptime = fstep / 60.0;
					break;
				case GHOST_kKeyNumpad2:
					if (val) swaptime = fstep / 50.0;
					break;
				case GHOST_kKeyNumpad3:
					if (val) swaptime = fstep / 30.0;
					break;
				case GHOST_kKeyNumpad4:
					if (qualN & SHIFT)
						swaptime = fstep / 24.0;
					else
						swaptime = fstep / 25.0;
					break;
				case GHOST_kKeyNumpad5:
					if (val) swaptime = fstep / 20.0;
					break;
				case GHOST_kKeyNumpad6:
					if (val) swaptime = fstep / 15.0;
					break;
				case GHOST_kKeyNumpad7:
					if (val) swaptime = fstep / 12.0;
					break;
				case GHOST_kKeyNumpad8:
					if (val) swaptime = fstep / 10.0;
					break;
				case GHOST_kKeyNumpad9:
					if (val) swaptime = fstep / 6.0;
					break;
				case GHOST_kKeyLeftArrow:
					if (val) {
						ps->sstep = TRUE;
						ps->wait2 = FALSE;
						if (qualN & SHIFT) {
							ps->picture = picsbase->first;
							ps->next = 0;
						}
						else {
							ps->next = -1;
						}
					}
					break;
				case GHOST_kKeyDownArrow:
					if (val) {
						ps->wait2 = FALSE;
						if (qualN & SHIFT) {
							ps->next = ps->direction = -1;
						}
						else {
							ps->next = -10;
							ps->sstep = TRUE;
						}
					}
					break;
				case GHOST_kKeyRightArrow:
					if (val) {
						ps->sstep = TRUE;
						ps->wait2 = FALSE;
						if (qualN & SHIFT) {
							ps->picture = picsbase->last;
							ps->next = 0;
						}
						else {
							ps->next = 1;
						}
					}
					break;
				case GHOST_kKeyUpArrow:
					if (val) {
						ps->wait2 = FALSE;
						if (qualN & SHIFT) {
							ps->next = ps->direction = 1;
						}
						else {
							ps->next = 10;
							ps->sstep = TRUE;
						}
					}
					break;

				case GHOST_kKeySlash:
				case GHOST_kKeyNumpadSlash:
					if (val) {
						if (qualN & SHIFT) {
							if (ps->curframe_ibuf)
								printf(" Name: %s | Speed: %.2f frames/s\n", ps->curframe_ibuf->name, fstep / swaptime);
						}
						else {
							swaptime = fstep / 5.0;
						}
					}
					break;
#if 0
				case LEFTMOUSE:
				case MOUSEX:
					if (qualN & LMOUSE) {
						int sizex, sizey;
						int i;
						playanim_window_get_size(&sizex, &sizey);
						ps->picture = picsbase->first;
						i = 0;
						while (ps->picture) {
							i++;
							ps->picture = ps->picture->next;
						}
						i = (i * val) / sizex;
						ps->picture = picsbase->first;
						for (; i > 0; i--) {
							if (ps->picture->next == NULL) break;
							ps->picture = ps->picture->next;
						}
						ps->sstep = TRUE;
						ps->wait2 = FALSE;
						ps->next = 0;
					}
					break;
#endif
				case GHOST_kKeyEqual:
					if (val) {
						if (qualN & SHIFT) {
							ps->pause++;
							printf("pause:%d\n", ps->pause);
						}
						else {
							swaptime /= 1.1;
						}
					}
					break;
				case GHOST_kKeyMinus:
					if (val) {
						if (qualN & SHIFT) {
							ps->pause--;
							printf("pause:%d\n", ps->pause);
						}
						else {
							swaptime *= 1.1;
						}
					}
					break;
				case GHOST_kKeyNumpad0:
					if (val) {
						if (ps->once) {
							ps->once = ps->wait2 = FALSE;
						}
						else {
							ps->picture = NULL;
							ps->once = TRUE;
							ps->wait2 = FALSE;
						}
					}
					break;
				case GHOST_kKeyEnter:
				case GHOST_kKeyNumpadEnter:
					if (val) {
						ps->wait2 = ps->sstep = FALSE;
					}
					break;
				case GHOST_kKeyNumpadPeriod:
					if (val) {
						if (ps->sstep) ps->wait2 = FALSE;
						else {
							ps->sstep = TRUE;
							ps->wait2 = !ps->wait2;
						}
					}
					break;
				case GHOST_kKeyNumpadPlus:
					if (val == 0) break;
					zoomx += 2.0;
					zoomy += 2.0;
					/* no break??? - is this intentional? - campbell XXX25 */
				case GHOST_kKeyNumpadMinus:
				{
					int sizex, sizey;
					/* int ofsx, ofsy; */ /* UNUSED */

					if (val == 0) break;
					if (zoomx > 1.0) zoomx -= 1.0;
					if (zoomy > 1.0) zoomy -= 1.0;
					// playanim_window_get_position(&ofsx, &ofsy);
					playanim_window_get_size(&sizex, &sizey);
					/* ofsx += sizex / 2; */ /* UNUSED */
					/* ofsy += sizey / 2; */ /* UNUSED */
					sizex = zoomx * ps->ibufx;
					sizey = zoomy * ps->ibufy;
					/* ofsx -= sizex / 2; */ /* UNUSED */
					/* ofsy -= sizey / 2; */ /* UNUSED */
					// window_set_position(g_window,sizex,sizey);
					GHOST_SetClientSize(g_window, sizex, sizey);
					break;
				}
				case GHOST_kKeyEsc:
					ps->go = FALSE;
					break;
				default:
					break;
			}
			break;
		}
		case GHOST_kEventWindowSize:
		case GHOST_kEventWindowMove:
		{
			int sizex, sizey;

			playanim_window_get_size(&sizex, &sizey);
			GHOST_ActivateWindowDrawingContext(g_window);

			glViewport(0, 0, sizex, sizey);
			glScissor(0, 0, sizex, sizey);

			zoomx = (float) sizex / ps->ibufx;
			zoomy = (float) sizey / ps->ibufy;
			zoomx = floor(zoomx + 0.5);
			zoomy = floor(zoomy + 0.5);
			if (zoomx < 1.0) zoomx = 1.0;
			if (zoomy < 1.0) zoomy = 1.0;

			sizex = zoomx * ps->ibufx;
			sizey = zoomy * ps->ibufy;

			glPixelZoom(zoomx, zoomy);
			glEnable(GL_DITHER);
			ptottime = 0.0;
			toscreen(ps->picture, ps->curframe_ibuf, ps->fontid);
			//XXX25				while (qtest()) qreadN(&val);

			break;
		}
		case GHOST_kEventQuit:
		case GHOST_kEventWindowClose:
		{
			ps->go = FALSE;
			break;
		}
		default:
			/* quiet warnings */
			break;
	}

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
#if defined(__APPLE__) && !defined(GHOST_COCOA)
	{
		extern int macPrefState; /* creator.c */
		initial_state += macPrefState;
	}
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
	struct ImBuf *ibuf = NULL;
	char filepath[FILE_MAX];
	GHOST_TUns32 maxwinx, maxwiny;
	/* short c233 = FALSE, yuvx = FALSE; */ /* UNUSED */
	int i;
	/* This was done to disambiguate the name for use under c++. */
	struct anim *anim = NULL;
	int start_x = 0, start_y = 0;
	int sfra = -1;
	int efra = -1;
	int totblock;

	PlayState ps = {0};

	/* ps.doubleb   = TRUE;*/ /* UNUSED */
	ps.go        = TRUE;
	ps.direction = TRUE;
	ps.next      = TRUE;
	ps.once      = FALSE;
	ps.turbo     = FALSE;
	ps.pingpong  = FALSE;
	ps.noskip    = FALSE;
	ps.sstep     = FALSE;
	ps.pause     = FALSE;
	ps.wait2     = FALSE;
	ps.stopped   = FALSE;
	ps.picture   = NULL;
	/* resetmap = FALSE */

	ps.fontid = -1;

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
						if (fps == 0.0) {
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
		else {
			break;
		}
	}

#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__) && !defined(GHOST_COCOA)
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
#endif /* _WIN32 || __APPLE__  && !defined(GHOST_COCOA)*/
	G.have_quicktime = TRUE;
#endif /* WITH_QUICKTIME */

	if (argc > 1) {
		BLI_strncpy(filepath, argv[1], sizeof(filepath));
	}
	else {
		BLI_current_working_dir(filepath, sizeof(filepath));
		BLI_add_slash(filepath);
	}

	if (IMB_isanim(filepath)) {
		anim = IMB_open_anim(filepath, IB_rect, 0);
		if (anim) {
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			IMB_close_anim(anim);
			anim = NULL;
		}
	}
	else if (!IMB_ispic(filepath)) {
		exit(1);
	}

	if (ibuf == NULL) {
		ibuf = IMB_loadiffname(filepath, IB_rect);
	}

	if (ibuf == NULL) {
		printf("couldn't open %s\n", filepath);
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

		GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(ghost_event_proc, &ps);

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

	//GHOST_ActivateWindowDrawingContext(g_window);

	/* initialize the font */
	BLF_init(11, 72);
	ps.fontid = BLF_load_mem("monospace", (unsigned char *)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);
	BLF_size(ps.fontid, 11, 72);

	ps.ibufx = ibuf->x;
	ps.ibufy = ibuf->y;

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

	build_pict_list(filepath, (efra - sfra) + 1, fstep, ps.fontid);

	for (i = 2; i < argc; i++) {
		BLI_strncpy(filepath, argv[i], sizeof(filepath));
		build_pict_list(filepath, (efra - sfra) + 1, fstep, ps.fontid);
	}

	IMB_freeImBuf(ibuf);
	ibuf = NULL;

	pupdate_time();
	ptottime = 0;

	while (ps.go) {
		if (ps.pingpong)
			ps.direction = -ps.direction;

		if (ps.direction == 1) {
			ps.picture = picsbase->first;
		}
		else {
			ps.picture = picsbase->last;
		}

		if (ps.picture == NULL) {
			printf("couldn't find pictures\n");
			ps.go = FALSE;
		}
		if (ps.pingpong) {
			if (ps.direction == 1) {
				ps.picture = ps.picture->next;
			}
			else {
				ps.picture = ps.picture->prev;
			}
		}
		if (ptottime > 0.0) ptottime = 0.0;

		while (ps.picture) {
			if (ibuf != NULL && ibuf->ftype == 0) IMB_freeImBuf(ibuf);

			if (ps.picture->ibuf) {
				ibuf = ps.picture->ibuf;
			}
			else if (ps.picture->anim) {
				ibuf = IMB_anim_absolute(ps.picture->anim, ps.picture->frame, IMB_TC_NONE, IMB_PROXY_NONE);
			}
			else if (ps.picture->mem) {
				ibuf = IMB_ibImageFromMemory((unsigned char *) ps.picture->mem, ps.picture->size,
				                             ps.picture->IB_flags, ps.picture->name);
			}
			else {
				ibuf = IMB_loadiffname(ps.picture->name, ps.picture->IB_flags);
			}

			if (ibuf) {
				BLI_strncpy(ibuf->name, ps.picture->name, sizeof(ibuf->name));

#ifdef _WIN32
				GHOST_SetTitle(g_window, picture->name);
#endif

				while (pupdate_time()) PIL_sleep_ms(1);
				ptottime -= swaptime;
				toscreen(ps.picture, ibuf, ps.fontid);
			} /* else deleten */
			else {
				printf("error: can't play this image type\n");
				exit(0);
			}

			if (ps.once) {
				if (ps.picture->next == NULL) {
					ps.wait2 = TRUE;
				}
				else if (ps.picture->prev == NULL) {
					ps.wait2 = TRUE;
				}
			}

			ps.next = ps.direction;


			{
				int hasevent = GHOST_ProcessEvents(g_system, 0);
				if (hasevent) {
					GHOST_DispatchEvents(g_system);
				}
			}

			ps.wait2 = ps.sstep;

			if (ps.wait2 == 0 && ps.stopped == 0) {
				ps.stopped = TRUE;
			}

			pupdate_time();

			if (ps.picture && ps.next) {
				/* always at least set one step */
				while (ps.picture) {
					if (ps.next < 0) {
						ps.picture = ps.picture->prev;
					}
					else {
						ps.picture = ps.picture->next;
					}

					if (ps.once && ps.picture != NULL) {
						if (ps.picture->next == NULL) {
							ps.wait2 = TRUE;
						}
						else if (ps.picture->prev == NULL) {
							ps.wait2 = TRUE;
						}
					}

					if (ps.wait2 || ptottime < swaptime || ps.turbo || ps.noskip) break;
					ptottime -= swaptime;
				}
				if (ps.picture == NULL && ps.sstep) {
					if (ps.next < 0) {
						ps.picture = picsbase->last;
					}
					else if (ps.next > 0) {
						ps.picture = picsbase->first;
					}
				}
			}
			if (ps.go == FALSE) {
				break;
			}
		}
	}
	ps.picture = picsbase->first;
	anim = NULL;
	while (ps.picture) {
		if (ps.picture && ps.picture->anim && (anim != ps.picture->anim)) {
			// to prevent divx crashes
			anim = ps.picture->anim;
			IMB_close_anim(anim);
		}

		if (ps.picture->ibuf) {
			IMB_freeImBuf(ps.picture->ibuf);
		}
		if (ps.picture->mem) {
			MEM_freeN(ps.picture->mem);
		}

		ps.picture = ps.picture->next;
	}
#ifdef WITH_QUICKTIME
#if defined(_WIN32) || defined(__APPLE__) && !defined(GHOST_COCOA)
	if (G.have_quicktime) {
		ExitMovies();
#ifdef _WIN32
		TerminateQTML();
#endif /* _WIN32 */
	}
#endif /* _WIN32 || __APPLE__ && !defined(GHOST_COCOA) */
#endif /* WITH_QUICKTIME */

	/* cleanup */
	if (ibuf) IMB_freeImBuf(ibuf);
	BLI_freelistN(picsbase);
#if 0 // XXX25
	free_blender();
#else
	/* we still miss freeing a lot!,
	 * but many areas could skip initialization too for anim play */
	IMB_exit();
	BLF_exit();
#endif
	GHOST_DisposeWindow(g_system, g_window);

	totblock = MEM_get_memory_blocks_in_use();
	if (totblock != 0) {
		printf("Error Totblock: %d\n", totblock);
		MEM_printmemlist();
	}
}
