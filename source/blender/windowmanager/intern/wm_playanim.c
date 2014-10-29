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
#include <math.h>

#ifndef WIN32
#  include <unistd.h>
#  include <sys/times.h>
#  include <sys/wait.h>
#else
#  include <io.h>
#endif
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_depsgraph.h"
#include "BKE_image.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "DNA_scene_types.h"
#include "ED_datafiles.h" /* for fonts */
#include "GHOST_C-api.h"
#include "BLF_api.h"

#include "wm_event_types.h"

#include "WM_api.h"  /* only for WM_main_playanim */

struct PlayState;
static void playanim_window_zoom(struct PlayState *ps, const float zoom_offset);

typedef struct PlayState {

	/* window and viewport size */
	int win_x, win_y;
	
	/* current zoom level */
	float zoom;

	/* playback state */
	short direction;
	short next_frame;

	bool  once;
	bool  turbo;
	bool  pingpong;
	bool  noskip;
	bool  sstep;
	bool  wait2;
	bool  stopped;
	bool  go;
	/* waiting for images to load */
	bool  loading;
	/* x/y image flip */
	bool draw_flip[2];
	
	int fstep;

	/* current picture */
	struct PlayAnimPict *picture;

	/* set once at the start */
	int ibufx, ibufy;
	int fontid;

	/* saves passing args */
	struct ImBuf *curframe_ibuf;
	
	/* restarts player for file drop */
	char dropped_file[FILE_MAX];
} PlayState;

/* for debugging */
#if 0
void print_ps(PlayState *ps)
{
	printf("ps:\n");
	printf("    direction=%d,\n", (int)ps->direction);
	printf("    next=%d,\n", ps->next);
	printf("    once=%d,\n", ps->once);
	printf("    turbo=%d,\n", ps->turbo);
	printf("    pingpong=%d,\n", ps->pingpong);
	printf("    noskip=%d,\n", ps->noskip);
	printf("    sstep=%d,\n", ps->sstep);
	printf("    pause=%d,\n", ps->pause);
	printf("    wait2=%d,\n", ps->wait2);
	printf("    stopped=%d,\n", ps->stopped);
	printf("    go=%d,\n\n", ps->go);
	fflush(stdout);
}
#endif

/* global for window and events */
typedef enum eWS_Qual {
	WS_QUAL_LSHIFT  = (1 << 0),
	WS_QUAL_RSHIFT  = (1 << 1),
	WS_QUAL_SHIFT   = (WS_QUAL_LSHIFT | WS_QUAL_RSHIFT),
	WS_QUAL_LALT    = (1 << 2),
	WS_QUAL_RALT    = (1 << 3),
	WS_QUAL_ALT     = (WS_QUAL_LALT | WS_QUAL_RALT),
	WS_QUAL_LCTRL   = (1 << 4),
	WS_QUAL_RCTRL   = (1 << 5),
	WS_QUAL_CTRL    = (WS_QUAL_LCTRL | WS_QUAL_RCTRL),
	WS_QUAL_LMOUSE  = (1 << 16),
	WS_QUAL_MMOUSE  = (1 << 17),
	WS_QUAL_RMOUSE  = (1 << 18),
	WS_QUAL_MOUSE   = (WS_QUAL_LMOUSE | WS_QUAL_MMOUSE | WS_QUAL_RMOUSE)
} eWS_Qual;

static struct WindowStateGlobal {
	GHOST_SystemHandle ghost_system;
	void *ghost_window;

	/* events */
	eWS_Qual qual;
} g_WS = {NULL};

static void playanim_window_get_size(int *width_r, int *height_r)
{
	GHOST_RectangleHandle bounds = GHOST_GetClientBounds(g_WS.ghost_window);
	*width_r = GHOST_GetWidthRectangle(bounds);
	*height_r = GHOST_GetHeightRectangle(bounds);
	GHOST_DisposeRectangle(bounds);
}

static void playanim_gl_matrix(void)
{
	/* unified matrix, note it affects offset for drawing */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
}

/* implementation */
static void playanim_event_qual_update(void)
{
	int val;

	/* Shift */
	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftShift, &val);
	if (val) g_WS.qual |=  WS_QUAL_LSHIFT;
	else     g_WS.qual &= ~WS_QUAL_LSHIFT;

	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightShift, &val);
	if (val) g_WS.qual |=  WS_QUAL_RSHIFT;
	else     g_WS.qual &= ~WS_QUAL_RSHIFT;

	/* Control */
	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftControl, &val);
	if (val) g_WS.qual |=  WS_QUAL_LCTRL;
	else     g_WS.qual &= ~WS_QUAL_LCTRL;

	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightControl, &val);
	if (val) g_WS.qual |=  WS_QUAL_RCTRL;
	else     g_WS.qual &= ~WS_QUAL_RCTRL;

	/* Alt */
	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyLeftAlt, &val);
	if (val) g_WS.qual |=  WS_QUAL_LALT;
	else     g_WS.qual &= ~WS_QUAL_LALT;

	GHOST_GetModifierKeyState(g_WS.ghost_system, GHOST_kModifierKeyRightAlt, &val);
	if (val) g_WS.qual |=  WS_QUAL_RALT;
	else     g_WS.qual &= ~WS_QUAL_RALT;
}

typedef struct PlayAnimPict {
	struct PlayAnimPict *next, *prev;
	char *mem;
	int size;
	const char *name;
	struct ImBuf *ibuf;
	struct anim *anim;
	int frame;
	int IB_flags;
} PlayAnimPict;

static struct ListBase picsbase = {NULL, NULL};
static bool fromdisk = false;
static double ptottime = 0.0, swaptime = 0.04;

static PlayAnimPict *playanim_step(PlayAnimPict *playanim, int step)
{
	if (step > 0) {
		while (step-- && playanim) {
			playanim = playanim->next;
		}
	}
	else if (step < 0) {
		while (step++ && playanim) {
			playanim = playanim->prev;
		}
	}
	return playanim;
}

static int pupdate_time(void)
{
	static double ltime;
	double time;

	time = PIL_check_seconds_timer();

	ptottime += (time - ltime);
	ltime = time;
	return (ptottime < 0);
}

static void playanim_toscreen(PlayState *ps, PlayAnimPict *picture, struct ImBuf *ibuf, int fontid, int fstep)
{
	float offs_x, offs_y;
	float span_x, span_y;

	if (ibuf == NULL) {
		printf("%s: no ibuf for picture '%s'\n", __func__, picture ? picture->name : "<NIL>");
		return;
	}
	if (ibuf->rect == NULL && ibuf->rect_float) {
		IMB_rect_from_float(ibuf);
		imb_freerectfloatImBuf(ibuf);
	}
	if (ibuf->rect == NULL)
		return;

	GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

	/* size within window */
	span_x = (ps->zoom * ibuf->x) / (float)ps->win_x;
	span_y = (ps->zoom * ibuf->y) / (float)ps->win_y;

	/* offset within window */
	offs_x = 0.5f * (1.0f - span_x);
	offs_y = 0.5f * (1.0f - span_y);

	CLAMP(offs_x, 0.0f, 1.0f);
	CLAMP(offs_y, 0.0f, 1.0f);
	glRasterPos2f(offs_x, offs_y);

	glClearColor(0.1, 0.1, 0.1, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* checkerboard for case alpha */
	if (ibuf->planes == 32) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		fdrawcheckerboard(offs_x, offs_y, offs_x + span_x, offs_y + span_y);
	}

	glRasterPos2f(offs_x + (ps->draw_flip[0] ? span_x : 0.0f),
	              offs_y + (ps->draw_flip[1] ? span_y : 0.0f));

	glPixelZoom(ps->zoom * (ps->draw_flip[0] ? -1.0f : 1.0f),
	            ps->zoom * (ps->draw_flip[1] ? -1.0f : 1.0f));

	glDrawPixels(ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	glDisable(GL_BLEND);
	
	pupdate_time();

	if (picture && (g_WS.qual & (WS_QUAL_SHIFT | WS_QUAL_LMOUSE)) && (fontid != -1)) {
		int sizex, sizey;
		float fsizex_inv, fsizey_inv;
		char str[32 + FILE_MAX];
		cpack(-1);
		BLI_snprintf(str, sizeof(str), "%s | %.2f frames/s", picture->name, fstep / swaptime);

		playanim_window_get_size(&sizex, &sizey);
		fsizex_inv = 1.0f / sizex;
		fsizey_inv = 1.0f / sizey;

		BLF_enable(fontid, BLF_ASPECT);
		BLF_aspect(fontid, fsizex_inv, fsizey_inv, 1.0f);
		BLF_position(fontid, 10.0f * fsizex_inv, 10.0f * fsizey_inv, 0.0f);
		BLF_draw(fontid, str, sizeof(str));
	}

	GHOST_SwapWindowBuffers(g_WS.ghost_window);
}

static void build_pict_list_ex(PlayState *ps, const char *first, int totframes, int fstep, int fontid)
{
	char *mem, filepath[FILE_MAX];
//	short val;
	PlayAnimPict *picture = NULL;
	struct ImBuf *ibuf = NULL;
	char str[32 + FILE_MAX];
	struct anim *anim;

	if (IMB_isanim(first)) {
		/* OCIO_TODO: support different input color space */
		anim = IMB_open_anim(first, IB_rect, 0, NULL);
		if (anim) {
			int pic;
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			if (ibuf) {
				playanim_toscreen(ps, NULL, ibuf, fontid, fstep);
				IMB_freeImBuf(ibuf);
			}

			for (pic = 0; pic < IMB_anim_get_duration(anim, IMB_TC_NONE); pic++) {
				picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "Pict");
				picture->anim = anim;
				picture->frame = pic;
				picture->IB_flags = IB_rect;
				BLI_snprintf(str, sizeof(str), "%s : %4.d", first, pic + 1);
				picture->name = strdup(str);
				BLI_addtail(&picsbase, picture);
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
			bool hasevent;
			size_t size;
			int file;

			file = BLI_open(filepath, O_BINARY | O_RDONLY, 0);
			if (file < 0) {
				/* print errno? */
				return;
			}

			picture = (PlayAnimPict *)MEM_callocN(sizeof(PlayAnimPict), "picture");
			if (picture == NULL) {
				printf("Not enough memory for pict struct '%s'\n", filepath);
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

			if (fromdisk == false) {
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
			BLI_addtail(&picsbase, picture);
			count++;

			pupdate_time();

			if (ptottime > 1.0) {
				/* OCIO_TODO: support different input color space */
				if (picture->mem) {
					ibuf = IMB_ibImageFromMemory((unsigned char *)picture->mem, picture->size,
					                             picture->IB_flags, NULL, picture->name);
				}
				else {
					ibuf = IMB_loadiffname(picture->name, picture->IB_flags, NULL);
				}
				if (ibuf) {
					playanim_toscreen(ps, picture, ibuf, fontid, fstep);
					IMB_freeImBuf(ibuf);
				}
				pupdate_time();
				ptottime = 0.0;
			}

			BLI_newname(filepath, +fstep);

			while ((hasevent = GHOST_ProcessEvents(g_WS.ghost_system, 0))) {
				if (hasevent) {
					GHOST_DispatchEvents(g_WS.ghost_system);
				}
				if (ps->loading == false) {
					return;
				}
			}

			totframes--;
		}
	}
	return;
}

static void build_pict_list(PlayState *ps, const char *first, int totframes, int fstep, int fontid)
{
	ps->loading = true;
	build_pict_list_ex(ps, first, totframes, fstep, fontid);
	ps->loading = false;
}

static int ghost_event_proc(GHOST_EventHandle evt, GHOST_TUserDataPtr ps_void)
{
	PlayState *ps = (PlayState *)ps_void;
	GHOST_TEventType type = GHOST_GetEventType(evt);
	int val;

	// print_ps(ps);

	playanim_event_qual_update();

	/* convert ghost event into value keyboard or mouse */
	val = ELEM(type, GHOST_kEventKeyDown, GHOST_kEventButtonDown);


	/* first check if we're busy loading files */
	if (ps->loading) {
		switch (type) {
			case GHOST_kEventKeyDown:
			case GHOST_kEventKeyUp:
			{
				GHOST_TEventKeyData *key_data;

				key_data = (GHOST_TEventKeyData *)GHOST_GetEventData(evt);
				switch (key_data->key) {
					case GHOST_kKeyEsc:
						ps->loading = false;
						break;
					default:
						break;
				}
				break;
			}
			default:
				break;
		}
		return 1;
	}


	if (ps->wait2 && ps->stopped) {
		ps->stopped = false;
	}

	if (ps->wait2) {
		pupdate_time();
		ptottime = 0;
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
				case GHOST_kKeyF:
				{
					if (val) {
						int axis = (g_WS.qual & WS_QUAL_SHIFT) ? 1 : 0;
						ps->draw_flip[axis] = !ps->draw_flip[axis];
					}
					break;
				}
				case GHOST_kKey1:
				case GHOST_kKeyNumpad1:
					if (val) swaptime = ps->fstep / 60.0;
					break;
				case GHOST_kKey2:
				case GHOST_kKeyNumpad2:
					if (val) swaptime = ps->fstep / 50.0;
					break;
				case GHOST_kKey3:
				case GHOST_kKeyNumpad3:
					if (val) swaptime = ps->fstep / 30.0;
					break;
				case GHOST_kKey4:
				case GHOST_kKeyNumpad4:
					if (g_WS.qual & WS_QUAL_SHIFT)
						swaptime = ps->fstep / 24.0;
					else
						swaptime = ps->fstep / 25.0;
					break;
				case GHOST_kKey5:
				case GHOST_kKeyNumpad5:
					if (val) swaptime = ps->fstep / 20.0;
					break;
				case GHOST_kKey6:
				case GHOST_kKeyNumpad6:
					if (val) swaptime = ps->fstep / 15.0;
					break;
				case GHOST_kKey7:
				case GHOST_kKeyNumpad7:
					if (val) swaptime = ps->fstep / 12.0;
					break;
				case GHOST_kKey8:
				case GHOST_kKeyNumpad8:
					if (val) swaptime = ps->fstep / 10.0;
					break;
				case GHOST_kKey9:
				case GHOST_kKeyNumpad9:
					if (val) swaptime = ps->fstep / 6.0;
					break;
				case GHOST_kKeyLeftArrow:
					if (val) {
						ps->sstep = true;
						ps->wait2 = false;
						if (g_WS.qual & WS_QUAL_SHIFT) {
							ps->picture = picsbase.first;
							ps->next_frame = 0;
						}
						else {
							ps->next_frame = -1;
						}
					}
					break;
				case GHOST_kKeyDownArrow:
					if (val) {
						ps->wait2 = false;
						if (g_WS.qual & WS_QUAL_SHIFT) {
							ps->next_frame = ps->direction = -1;
						}
						else {
							ps->next_frame = -10;
							ps->sstep = true;
						}
					}
					break;
				case GHOST_kKeyRightArrow:
					if (val) {
						ps->sstep = true;
						ps->wait2 = false;
						if (g_WS.qual & WS_QUAL_SHIFT) {
							ps->picture = picsbase.last;
							ps->next_frame = 0;
						}
						else {
							ps->next_frame = 1;
						}
					}
					break;
				case GHOST_kKeyUpArrow:
					if (val) {
						ps->wait2 = false;
						if (g_WS.qual & WS_QUAL_SHIFT) {
							ps->next_frame = ps->direction = 1;
						}
						else {
							ps->next_frame = 10;
							ps->sstep = true;
						}
					}
					break;

				case GHOST_kKeySlash:
				case GHOST_kKeyNumpadSlash:
					if (val) {
						if (g_WS.qual & WS_QUAL_SHIFT) {
							if (ps->curframe_ibuf)
								printf(" Name: %s | Speed: %.2f frames/s\n",
								       ps->curframe_ibuf->name, ps->fstep / swaptime);
						}
						else {
							swaptime = ps->fstep / 5.0;
						}
					}
					break;
				case GHOST_kKey0:
				case GHOST_kKeyNumpad0:
					if (val) {
						if (ps->once) {
							ps->once = ps->wait2 = false;
						}
						else {
							ps->picture = NULL;
							ps->once = true;
							ps->wait2 = false;
						}
					}
					break;
				case GHOST_kKeyEnter:
				case GHOST_kKeyNumpadEnter:
					if (val) {
						ps->wait2 = ps->sstep = false;
					}
					break;
				case GHOST_kKeyPeriod:
				case GHOST_kKeyNumpadPeriod:
					if (val) {
						if (ps->sstep) {
							ps->wait2 = false;
						}
						else {
							ps->sstep = true;
							ps->wait2 = !ps->wait2;
						}
					}
					break;
				case GHOST_kKeyEqual:
				case GHOST_kKeyNumpadPlus:
				{
					if (val == 0) break;
					if (g_WS.qual & WS_QUAL_CTRL) {
						playanim_window_zoom(ps, 1.0f);
					}
					else {
						swaptime /= 1.1;
					}
					break;
				}
				case GHOST_kKeyMinus:
				case GHOST_kKeyNumpadMinus:
				{
					if (val == 0) break;
					if (g_WS.qual & WS_QUAL_CTRL) {
						playanim_window_zoom(ps, -1.0f);
					}
					else {
						swaptime *= 1.1;
					}
					break;
				}
				case GHOST_kKeyEsc:
					ps->go = false;
					break;
				default:
					break;
			}
			break;
		}
		case GHOST_kEventButtonDown:
		case GHOST_kEventButtonUp:
		{
			GHOST_TEventButtonData *bd = GHOST_GetEventData(evt);
			int cx, cy, sizex, sizey, inside_window;
			
			GHOST_GetCursorPosition(g_WS.ghost_system, &cx, &cy);
			GHOST_ScreenToClient(g_WS.ghost_window, cx, cy, &cx, &cy);
			playanim_window_get_size(&sizex, &sizey);

			inside_window = (cx >= 0 && cx < sizex && cy >= 0 && cy <= sizey);
			
			if (bd->button == GHOST_kButtonMaskLeft) {
				if (type == GHOST_kEventButtonDown) {
					if (inside_window)
						g_WS.qual |= WS_QUAL_LMOUSE;
				}
				else
					g_WS.qual &= ~WS_QUAL_LMOUSE;
			}
			else if (bd->button == GHOST_kButtonMaskMiddle) {
				if (type == GHOST_kEventButtonDown) {
					if (inside_window)
						g_WS.qual |= WS_QUAL_MMOUSE;
				}
				else
					g_WS.qual &= ~WS_QUAL_MMOUSE;
			}
			else if (bd->button == GHOST_kButtonMaskRight) {
				if (type == GHOST_kEventButtonDown) {
					if (inside_window)
						g_WS.qual |= WS_QUAL_RMOUSE;
				}
				else
					g_WS.qual &= ~WS_QUAL_RMOUSE;
			}
			break;
		}
		case GHOST_kEventCursorMove:
		{
			if (g_WS.qual & WS_QUAL_LMOUSE) {
				int sizex, sizey;
				int i;

				GHOST_TEventCursorData *cd = GHOST_GetEventData(evt);
				int cx, cy;

				GHOST_ScreenToClient(g_WS.ghost_window, cd->x, cd->y, &cx, &cy);

				playanim_window_get_size(&sizex, &sizey);
				ps->picture = picsbase.first;
				/* TODO - store in ps direct? */
				i = 0;
				while (ps->picture) {
					i++;
					ps->picture = ps->picture->next;
				}
				i = (i * cx) / sizex;
				ps->picture = picsbase.first;
				for (; i > 0; i--) {
					if (ps->picture->next == NULL) break;
					ps->picture = ps->picture->next;
				}
				ps->sstep = true;
				ps->wait2 = false;
				ps->next_frame = 0;
			}
			break;
		}
		case GHOST_kEventWindowActivate:
		case GHOST_kEventWindowDeactivate:
		{
			g_WS.qual &= ~WS_QUAL_MOUSE;
			break;
		}
		case GHOST_kEventWindowSize:
		case GHOST_kEventWindowMove:
		{
			float zoomx, zoomy;
			
			playanim_window_get_size(&ps->win_x, &ps->win_y);
			GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

			zoomx = (float) ps->win_x / ps->ibufx;
			zoomy = (float) ps->win_y / ps->ibufy;
			
			/* zoom always show entire image */
			ps->zoom = MIN2(zoomx, zoomy);
			
			/* zoom steps of 2 for speed */
			ps->zoom = floor(ps->zoom + 0.5f);
			if (ps->zoom < 1.0f) ps->zoom = 1.0f;
			
			glViewport(0, 0, ps->win_x, ps->win_y);
			glScissor(0, 0, ps->win_x, ps->win_y);
			
			playanim_gl_matrix();

			ptottime = 0.0;
			playanim_toscreen(ps, ps->picture, ps->curframe_ibuf, ps->fontid, ps->fstep);

			break;
		}
		case GHOST_kEventQuit:
		case GHOST_kEventWindowClose:
		{
			ps->go = false;
			break;
		}
		case GHOST_kEventDraggingDropDone:
		{
			GHOST_TEventDragnDropData *ddd = GHOST_GetEventData(evt);
			
			if (ddd->dataType == GHOST_kDragnDropTypeFilenames) {
				GHOST_TStringArray *stra = ddd->data;
				int a;
				
				for (a = 0; a < stra->count; a++) {
					BLI_strncpy(ps->dropped_file, (char *)stra->strings[a], sizeof(ps->dropped_file));
					ps->go = false;
					printf("drop file %s\n", stra->strings[a]);
					break; /* only one drop element supported now */
				}
			}
			break;
		}
		default:
			/* quiet warnings */
			break;
	}

	return 1;
}

static void playanim_window_open(const char *title, int posx, int posy, int sizex, int sizey)
{
	GHOST_TUns32 scr_w, scr_h;

	GHOST_GetMainDisplayDimensions(g_WS.ghost_system, &scr_w, &scr_h);

	posy = (scr_h - posy - sizey);

	g_WS.ghost_window = GHOST_CreateWindow(g_WS.ghost_system,
	                                       title,
	                                       posx, posy, sizex, sizey,
	                                       /* could optionally start fullscreen */
	                                       GHOST_kWindowStateNormal,
	                                       GHOST_kDrawingContextTypeOpenGL,
	                                       false /* no stereo */, false);
}

static void playanim_window_zoom(PlayState *ps, const float zoom_offset)
{
	int sizex, sizey;
	/* int ofsx, ofsy; */ /* UNUSED */

	if (ps->zoom + zoom_offset > 0.0f) ps->zoom += zoom_offset;

	// playanim_window_get_position(&ofsx, &ofsy);
	playanim_window_get_size(&sizex, &sizey);
	/* ofsx += sizex / 2; */ /* UNUSED */
	/* ofsy += sizey / 2; */ /* UNUSED */
	sizex = ps->zoom * ps->ibufx;
	sizey = ps->zoom * ps->ibufy;
	/* ofsx -= sizex / 2; */ /* UNUSED */
	/* ofsy -= sizey / 2; */ /* UNUSED */
	// window_set_position(g_WS.ghost_window, sizex, sizey);
	GHOST_SetClientSize(g_WS.ghost_window, sizex, sizey);
}

/* return path for restart */
static char *wm_main_playanim_intern(int argc, const char **argv)
{
	struct ImBuf *ibuf = NULL;
	static char filepath[FILE_MAX];	/* abused to return dropped file path */
	GHOST_TUns32 maxwinx, maxwiny;
	int i;
	/* This was done to disambiguate the name for use under c++. */
	struct anim *anim = NULL;
	int start_x = 0, start_y = 0;
	int sfra = -1;
	int efra = -1;
	int totblock;
	
	PlayState ps = {0};

	/* ps.doubleb   = true;*/ /* UNUSED */
	ps.go        = true;
	ps.direction = true;
	ps.next_frame = 1;
	ps.once      = false;
	ps.turbo     = false;
	ps.pingpong  = false;
	ps.noskip    = false;
	ps.sstep     = false;
	ps.wait2     = false;
	ps.stopped   = false;
	ps.loading   = false;
	ps.picture   = NULL;
	ps.dropped_file[0] = 0;
	ps.zoom      = 1.0f;
	/* resetmap = false */
	ps.draw_flip[0] = false;
	ps.draw_flip[1] = false;

	ps.fstep     = 1;

	ps.fontid = -1;

	while (argc > 1) {
		if (argv[1][0] == '-') {
			switch (argv[1][1]) {
				case 'm':
					fromdisk = true;
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
					sfra = atoi(argv[2]);
					CLAMP(sfra, 1, MAXFRAME);
					argc--;
					argv++;
					break;
				case 'e':
					efra = atoi(argv[2]);
					CLAMP(efra, 1, MAXFRAME);
					argc--;
					argv++;
					break;
				case 'j':
					ps.fstep = atoi(argv[2]);
					CLAMP(ps.fstep, 1, MAXFRAME);
					swaptime *= ps.fstep;
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

	if (argc > 1) {
		BLI_strncpy(filepath, argv[1], sizeof(filepath));
	}
	else {
		BLI_current_working_dir(filepath, sizeof(filepath));
		BLI_add_slash(filepath);
	}

	if (IMB_isanim(filepath)) {
		/* OCIO_TODO: support different input color spaces */
		anim = IMB_open_anim(filepath, IB_rect, 0, NULL);
		if (anim) {
			ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
			IMB_close_anim(anim);
			anim = NULL;
		}
	}
	else if (!IMB_ispic(filepath)) {
		printf("%s: '%s' not an image file\n", __func__, filepath);
		exit(1);
	}

	if (ibuf == NULL) {
		/* OCIO_TODO: support different input color space */
		ibuf = IMB_loadiffname(filepath, IB_rect, NULL);
	}

	if (ibuf == NULL) {
		printf("%s: '%s' couldn't open\n", __func__, filepath);
		exit(1);
	}

#if 0 //XXX25
#if !defined(WIN32) && !defined(__APPLE__)
	if (fork()) exit(0);
#endif
#endif //XXX25

	{

		GHOST_EventConsumerHandle consumer = GHOST_CreateEventConsumer(ghost_event_proc, &ps);

		g_WS.ghost_system = GHOST_CreateSystem();
		GHOST_AddEventConsumer(g_WS.ghost_system, consumer);

		playanim_window_open("Blender:Anim", start_x, start_y, ibuf->x, ibuf->y);

		playanim_gl_matrix();
	}

	GHOST_GetMainDisplayDimensions(g_WS.ghost_system, &maxwinx, &maxwiny);

	//GHOST_ActivateWindowDrawingContext(g_WS.ghost_window);

	/* initialize the font */
	BLF_init(11, 72);
	ps.fontid = BLF_load_mem("monospace", (unsigned char *)datatoc_bmonofont_ttf, datatoc_bmonofont_ttf_size);
	BLF_size(ps.fontid, 11, 72);

	ps.ibufx = ibuf->x;
	ps.ibufy = ibuf->y;
	
	ps.win_x = ps.ibufx;
	ps.win_y = ps.ibufy;

	if (maxwinx % ibuf->x) maxwinx = ibuf->x * (1 + (maxwinx / ibuf->x));
	if (maxwiny % ibuf->y) maxwiny = ibuf->y * (1 + (maxwiny / ibuf->y));

	
	glClearColor(0.1, 0.1, 0.1, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	GHOST_SwapWindowBuffers(g_WS.ghost_window);

	if (sfra == -1 || efra == -1) {
		/* one of the frames was invalid, just use all images */
		sfra = 1;
		efra = MAXFRAME;
	}

	build_pict_list(&ps, filepath, (efra - sfra) + 1, ps.fstep, ps.fontid);

	for (i = 2; i < argc; i++) {
		BLI_strncpy(filepath, argv[i], sizeof(filepath));
		build_pict_list(&ps, filepath, (efra - sfra) + 1, ps.fstep, ps.fontid);
	}

	IMB_freeImBuf(ibuf);
	ibuf = NULL;

	pupdate_time();
	ptottime = 0;

	/* newly added in 2.6x, without this images never get freed */
#define USE_IMB_CACHE

	while (ps.go) {
		if (ps.pingpong)
			ps.direction = -ps.direction;

		if (ps.direction == 1) {
			ps.picture = picsbase.first;
		}
		else {
			ps.picture = picsbase.last;
		}

		if (ps.picture == NULL) {
			printf("couldn't find pictures\n");
			ps.go = false;
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
			int hasevent;
#ifndef USE_IMB_CACHE
			if (ibuf != NULL && ibuf->ftype == 0) IMB_freeImBuf(ibuf);
#endif
			if (ps.picture->ibuf) {
				ibuf = ps.picture->ibuf;
			}
			else if (ps.picture->anim) {
				ibuf = IMB_anim_absolute(ps.picture->anim, ps.picture->frame, IMB_TC_NONE, IMB_PROXY_NONE);
			}
			else if (ps.picture->mem) {
				/* use correct colorspace here */
				ibuf = IMB_ibImageFromMemory((unsigned char *) ps.picture->mem, ps.picture->size,
				                             ps.picture->IB_flags, NULL, ps.picture->name);
			}
			else {
				/* use correct colorspace here */
				ibuf = IMB_loadiffname(ps.picture->name, ps.picture->IB_flags, NULL);
			}

			if (ibuf) {

#ifdef USE_IMB_CACHE
				ps.picture->ibuf = ibuf;
#endif

				BLI_strncpy(ibuf->name, ps.picture->name, sizeof(ibuf->name));

				/* why only windows? (from 2.4x) - campbell */
#ifdef _WIN32
				GHOST_SetTitle(g_WS.ghost_window, ps.picture->name);
#endif

				while (pupdate_time()) PIL_sleep_ms(1);
				ptottime -= swaptime;
				playanim_toscreen(&ps, ps.picture, ibuf, ps.fontid, ps.fstep);
			} /* else delete */
			else {
				printf("error: can't play this image type\n");
				exit(0);
			}

			if (ps.once) {
				if (ps.picture->next == NULL) {
					ps.wait2 = true;
				}
				else if (ps.picture->prev == NULL) {
					ps.wait2 = true;
				}
			}

			ps.next_frame = ps.direction;


			while ((hasevent = GHOST_ProcessEvents(g_WS.ghost_system, 0)) || ps.wait2) {
				if (hasevent) {
					GHOST_DispatchEvents(g_WS.ghost_system);
				}
				/* Note, this still draws for mousemoves on pause */
				if (ps.wait2) {
					if (hasevent) {
						if (ibuf) {
							while (pupdate_time()) PIL_sleep_ms(1);
							ptottime -= swaptime;
							playanim_toscreen(&ps, ps.picture, ibuf, ps.fontid, ps.fstep);
						}
					}
				}
				if (ps.go == false) {
					break;
				}
			}

			ps.wait2 = ps.sstep;

			if (ps.wait2 == false && ps.stopped == false) {
				ps.stopped = true;
			}

			pupdate_time();

			if (ps.picture && ps.next_frame) {
				/* always at least set one step */
				while (ps.picture) {
					ps.picture = playanim_step(ps.picture, ps.next_frame);

					if (ps.once && ps.picture != NULL) {
						if (ps.picture->next == NULL) {
							ps.wait2 = true;
						}
						else if (ps.picture->prev == NULL) {
							ps.wait2 = true;
						}
					}

					if (ps.wait2 || ptottime < swaptime || ps.turbo || ps.noskip) break;
					ptottime -= swaptime;
				}
				if (ps.picture == NULL && ps.sstep) {
					ps.picture = playanim_step(ps.picture, ps.next_frame);
				}
			}
			if (ps.go == false) {
				break;
			}
		}
	}
	ps.picture = picsbase.first;
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

	/* cleanup */
#ifndef USE_IMB_CACHE
	if (ibuf) IMB_freeImBuf(ibuf);
#endif

	BLI_freelistN(&picsbase);
#if 0 // XXX25
	free_blender();
#else
	/* we still miss freeing a lot!,
	 * but many areas could skip initialization too for anim play */
	
	BLF_exit();
#endif
	GHOST_DisposeWindow(g_WS.ghost_system, g_WS.ghost_window);

	/* early exit, IMB and BKE should be exited only in end */
	if (ps.dropped_file[0]) {
		BLI_strncpy(filepath, ps.dropped_file, sizeof(filepath));
		return filepath;
	}
	
	IMB_exit();
	BKE_images_exit();
	DAG_exit();

	totblock = MEM_get_memory_blocks_in_use();
	if (totblock != 0) {
		/* prints many bAKey, bArgument's which are tricky to fix */
#if 0
		printf("Error Totblock: %d\n", totblock);
		MEM_printmemlist();
#endif
	}
	
	return NULL;
}


void WM_main_playanim(int argc, const char **argv)
{
	bool looping = true;

	while (looping) {
		const char *filepath = wm_main_playanim_intern(argc, argv);

		if (filepath) {	/* use simple args */
			argv[1] = "-a";
			argv[2] = filepath;
			argc = 3;
		}
		else {
			looping = false;
		}
	}
}
