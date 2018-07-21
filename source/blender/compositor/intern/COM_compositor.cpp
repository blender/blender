/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 */


extern "C" {
#include "BKE_node.h"
#include "BLI_threads.h"
}

#include "BLT_translation.h"

#include "BKE_scene.h"

#include "COM_compositor.h"
#include "COM_ExecutionSystem.h"
#include "COM_WorkScheduler.h"
#include "clew.h"
#include "COM_MovieDistortionOperation.h"

static ThreadMutex s_compositorMutex;
static bool is_compositorMutex_init = false;

void COM_execute(RenderData *rd, Scene *scene, bNodeTree *editingtree, int rendering,
                 const ColorManagedViewSettings *viewSettings,
                 const ColorManagedDisplaySettings *displaySettings,
                 const char *viewName)
{
	/* initialize mutex, TODO this mutex init is actually not thread safe and
	 * should be done somewhere as part of blender startup, all the other
	 * initializations can be done lazily */
	if (is_compositorMutex_init == false) {
		BLI_mutex_init(&s_compositorMutex);
		is_compositorMutex_init = true;
	}

	BLI_mutex_lock(&s_compositorMutex);

	if (editingtree->test_break(editingtree->tbh)) {
		// during editing multiple calls to this method can be triggered.
		// make sure one the last one will be doing the work.
		BLI_mutex_unlock(&s_compositorMutex);
		return;
	}

	/* Make sure node tree has previews.
	 * Don't create previews in advance, this is done when adding preview operations.
	 * Reserved preview size is determined by render output for now.
	 *
	 * We fit the aspect into COM_PREVIEW_SIZE x COM_PREVIEW_SIZE image to avoid
	 * insane preview resolution, which might even overflow preview dimensions.
	 */
	const float aspect = rd->xsch > 0 ? (float)rd->ysch / (float)rd->xsch : 1.0f;
	int preview_width, preview_height;
	if (aspect < 1.0f) {
		preview_width = COM_PREVIEW_SIZE;
		preview_height = (int)(COM_PREVIEW_SIZE * aspect);
	}
	else {
		preview_width = (int)(COM_PREVIEW_SIZE / aspect);
		preview_height = COM_PREVIEW_SIZE;
	}
	BKE_node_preview_init_tree(editingtree, preview_width, preview_height, false);

	/* initialize workscheduler, will check if already done. TODO deinitialize somewhere */
	bool use_opencl = (editingtree->flag & NTREE_COM_OPENCL) != 0;
	WorkScheduler::initialize(use_opencl, BKE_render_num_threads(rd));

	/* set progress bar to 0% and status to init compositing */
	editingtree->progress(editingtree->prh, 0.0);
	editingtree->stats_draw(editingtree->sdh, IFACE_("Compositing"));

	bool twopass = (editingtree->flag & NTREE_TWO_PASS) && !rendering;
	/* initialize execution system */
	if (twopass) {
		ExecutionSystem *system = new ExecutionSystem(rd, scene, editingtree, rendering, twopass, viewSettings, displaySettings, viewName);
		system->execute();
		delete system;

		if (editingtree->test_break(editingtree->tbh)) {
			// during editing multiple calls to this method can be triggered.
			// make sure one the last one will be doing the work.
			BLI_mutex_unlock(&s_compositorMutex);
			return;
		}
	}

	ExecutionSystem *system = new ExecutionSystem(rd, scene, editingtree, rendering, false,
	                                              viewSettings, displaySettings, viewName);
	system->execute();
	delete system;

	BLI_mutex_unlock(&s_compositorMutex);
}

void COM_deinitialize()
{
	if (is_compositorMutex_init) {
		BLI_mutex_lock(&s_compositorMutex);
		WorkScheduler::deinitialize();
		is_compositorMutex_init = false;
		BLI_mutex_unlock(&s_compositorMutex);
		BLI_mutex_end(&s_compositorMutex);
	}
}
