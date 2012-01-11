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
 * Contributor(s): Campbell Barton
 *
 * Mode switching
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (c) 1993-2011 Tim Riker
 * Copyright (C) 2012 Alex Fraser
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DisplayManagerSDL.cpp
 *  \ingroup GHOST
 */

#include "GHOST_SystemSDL.h"
#include "GHOST_DisplayManagerSDL.h"

GHOST_DisplayManagerSDL::GHOST_DisplayManagerSDL(GHOST_SystemSDL *system)
    :
      GHOST_DisplayManager(),
      m_system(system)
{
	/* do nothing */
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getNumDisplays(GHOST_TUns8& numDisplays) const
{
	numDisplays=  SDL_GetNumVideoDisplays();
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerSDL::getNumDisplaySettings(GHOST_TUns8 display,
                                                              GHOST_TInt32& numSettings) const
{
	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
	int i;
	SDL_Rect **vidmodes;

	vidmodes = SDL_ListModes(NULL, SDL_HWSURFACE | SDL_OPENGL |
			SDL_FULLSCREEN | SDL_HWPALETTE);
	if (!vidmodes) {
		fprintf(stderr, "Could not get available video modes: %s.\n",
				SDL_GetError());
		return GHOST_kFailure;
	}
	for (i = 0; vidmodes[i]; i++);
	numSettings = GHOST_TInt32(i);

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getDisplaySetting(GHOST_TUns8 display,
                                           GHOST_TInt32 index,
                                           GHOST_DisplaySetting& setting) const
{
	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

	int i;
	SDL_Rect **vidmodes;
	/* NULL is passed in here to get the modes for the current bit depth.
	 * Other bit depths may be possible; in that case, an SDL_PixelFormat struct
	 * should be passed in. To get a complete profile, all possible bit depths
	 * would need to be iterated over. - z0r */
	vidmodes = SDL_ListModes(NULL, SDL_HWSURFACE | SDL_OPENGL |
			SDL_FULLSCREEN | SDL_HWPALETTE);
	if (!vidmodes) {
		fprintf(stderr, "Could not get available video modes: %s.\n",
				SDL_GetError());
		return GHOST_kFailure;
	}
	for (i = 0; vidmodes[i]; i++);
	GHOST_ASSERT(index < i, "Requested setting outside of valid range.\n");

	setting.xPixels = vidmodes[index]->w;
	setting.yPixels = vidmodes[index]->h;

	SDL_Surface *surf;
	surf = SDL_GetVideoSurface();
	if (surf == NULL) {
		fprintf(stderr, "Getting display setting: %s\n", SDL_GetError());
		/* Just guess the bit depth */
		setting.bpp = 32;
	} else {
		setting.bpp = surf->format->BitsPerPixel;
	}
	/* Just guess the frequency :( */
	setting.frequency = 60;

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getCurrentDisplaySetting(GHOST_TUns8 display,
                                                  GHOST_DisplaySetting& setting) const
{
	SDL_Surface *surf;
	const SDL_VideoInfo *info;

	/* Note: not using SDL_GetDesktopDisplayMode because that does not return
	 * the current mode. Try to use GetVideoSurface first, as it seems more
	 * accurate. If that fails, try other methods. - z0r */
	surf = SDL_GetVideoSurface();

	if (surf != NULL) {
		setting.xPixels = surf->w;
		setting.yPixels = surf->h;
		setting.bpp = surf->format->BitsPerPixel;
		/* Just guess the frequency :( */
		setting.frequency = 60;
	} else {
		/* This may happen if the surface hasn't been created yet, e.g. on
		 * application startup. */
		info = SDL_GetVideoInfo();
		setting.xPixels = info->current_w;
		setting.yPixels = info->current_h;
		setting.bpp = info->vfmt->BitsPerPixel;
		/* Just guess the frequency :( */
		setting.frequency = 60;
	}

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_DisplayManagerSDL:: setCurrentDisplaySetting(GHOST_TUns8 display,
                                                   const GHOST_DisplaySetting& setting)
{

	/*
	 * Mode switching code ported from Quake 2 version 3.21 and bzflag version
	 * 2.4.0:
	 * ftp://ftp.idsoftware.com/idstuff/source/q2source-3.21.zip
	 * See linux/gl_glx.c:GLimp_SetMode
	 * http://wiki.bzflag.org/BZFlag_Source
	 * See src/platform/SDLDisplay.cxx:SDLDisplay and createWindow
	 */
	SDL_Surface *surf;
	int best_fit, best_dist, dist, x, y;

	SDL_Rect **vidmodes = SDL_ListModes(NULL, SDL_HWSURFACE | SDL_OPENGL |
			SDL_FULLSCREEN | SDL_HWPALETTE);
	if (!vidmodes) {
		fprintf(stderr, "Could not get available video modes: %s.\n",
				SDL_GetError());
	}

	best_dist = 9999999;
	best_fit = -1;

	if (vidmodes == (SDL_Rect **) -1) {
		/* Any mode is OK. */
		x = setting.xPixels;
		y = setting.yPixels;
	} else {
		for (int i = 0; vidmodes[i]; i++) {
			if (setting.xPixels > vidmodes[i]->w ||
				setting.yPixels > vidmodes[i]->h)
				continue;

			x = setting.xPixels - vidmodes[i]->w;
			y = setting.yPixels - vidmodes[i]->h;
			dist = (x * x) + (y * y);
			if (dist < best_dist) {
				best_dist = dist;
				best_fit = i;
			}
		}

		if (best_fit == -1)
			return GHOST_kFailure;

		x = vidmodes[best_fit]->w;
		y = vidmodes[best_fit]->h;
	}

#  ifdef _DEBUG
	printf("Switching to video mode %dx%d\n", x, y);
#  endif

	// limit us to the main display
	static char singleDisplayEnv[] = "SDL_SINGLEDISPLAY=1";
	putenv(singleDisplayEnv);

	// change to the mode
	surf = SDL_SetVideoMode(x, y, setting.bpp, SDL_OPENGL | SDL_FULLSCREEN);
	if (surf == NULL) {
		fprintf(stderr, "Could not set video mode: %s.\n", SDL_GetError());
		return GHOST_kFailure;
	}

	return GHOST_kSuccess;
}
