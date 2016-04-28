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
 */

/** \file blender/blenkernel/intern/blender.c
 *  \ingroup bke
 *
 * Application level startup/shutdown functionality.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender.h"  /* own include */
#include "BKE_blender_version.h"  /* own include */
#include "BKE_blendfile.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"

#include "RE_pipeline.h"
#include "RE_render_ext.h"

#include "BLF_api.h"


Global G;
UserDef U;

char versionstr[48] = "";

/* ********** free ********** */

/* only to be called on exit blender */
void BKE_blender_free(void)
{
	/* samples are in a global list..., also sets G.main->sound->sample NULL */
	BKE_main_free(G.main);
	G.main = NULL;

	BKE_spacetypes_free();      /* after free main, it uses space callbacks */
	
	IMB_exit();
	BKE_images_exit();
	DAG_exit();

	BKE_brush_system_exit();
	RE_texture_rng_exit();	

	BLI_callback_global_finalize();

	BKE_sequencer_cache_destruct();
	IMB_moviecache_destruct();
	
	free_nodesystem();
}

void BKE_blender_globals_init(void)
{
	memset(&G, 0, sizeof(Global));
	
	U.savetime = 1;

	G.main = BKE_main_new();

	strcpy(G.ima, "//");

	if (BLENDER_SUBVERSION)
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d.%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION);
	else
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);

#ifndef WITH_PYTHON_SECURITY /* default */
	G.f |= G_SCRIPT_AUTOEXEC;
#else
	G.f &= ~G_SCRIPT_AUTOEXEC;
#endif
}

void BKE_blender_globals_clear(void)
{
	BKE_main_free(G.main);          /* free all lib data */

	G.main = NULL;
}

/***/

static void keymap_item_free(wmKeyMapItem *kmi)
{
	if (kmi->properties) {
		IDP_FreeProperty(kmi->properties);
		MEM_freeN(kmi->properties);
	}
	if (kmi->ptr)
		MEM_freeN(kmi->ptr);
}

/**
 * When loading a new userdef from file,
 * or when exiting Blender.
 */
void BKE_blender_userdef_free(void)
{
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	bAddon *addon, *addon_next;
	uiFont *font;

	for (km = U.user_keymaps.first; km; km = km->next) {
		for (kmdi = km->diff_items.first; kmdi; kmdi = kmdi->next) {
			if (kmdi->add_item) {
				keymap_item_free(kmdi->add_item);
				MEM_freeN(kmdi->add_item);
			}
			if (kmdi->remove_item) {
				keymap_item_free(kmdi->remove_item);
				MEM_freeN(kmdi->remove_item);
			}
		}

		for (kmi = km->items.first; kmi; kmi = kmi->next)
			keymap_item_free(kmi);

		BLI_freelistN(&km->diff_items);
		BLI_freelistN(&km->items);
	}
	
	for (addon = U.addons.first; addon; addon = addon_next) {
		addon_next = addon->next;
		if (addon->prop) {
			IDP_FreeProperty(addon->prop);
			MEM_freeN(addon->prop);
		}
		MEM_freeN(addon);
	}

	for (font = U.uifonts.first; font; font = font->next) {
		BLF_unload_id(font->blf_id);
	}

	BLF_default_set(-1);

	BLI_freelistN(&U.autoexec_paths);

	BLI_freelistN(&U.uistyles);
	BLI_freelistN(&U.uifonts);
	BLI_freelistN(&U.themes);
	BLI_freelistN(&U.user_keymaps);
}

/**
 * Handle changes in settings that need refreshing.
 */
void BKE_blender_userdef_refresh(void)
{
	/* prevent accidents */
	if (U.pixelsize == 0) U.pixelsize = 1;
	
	BLF_default_dpi(U.pixelsize * U.dpi);
	U.widget_unit = (U.pixelsize * U.dpi * 20 + 36) / 72;

}

/* *****************  testing for break ************* */

static void (*blender_test_break_cb)(void) = NULL;

void BKE_blender_callback_test_break_set(void (*func)(void))
{
	blender_test_break_cb = func;
}


int BKE_blender_test_break(void)
{
	if (!G.background) {
		if (blender_test_break_cb)
			blender_test_break_cb();
	}
	
	return (G.is_break == true);
}

