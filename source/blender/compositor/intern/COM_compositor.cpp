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


#include "BKE_node.h"
extern "C" {
	#include "BLI_threads.h"
}
#include "BKE_main.h"
#include "BKE_global.h"

#include "COM_compositor.h"
#include "COM_ExecutionSystem.h"
#include "COM_WorkScheduler.h"
#include "OCL_opencl.h"
#include "COM_MovieDistortionOperation.h"

static ThreadMutex s_compositorMutex;
static char is_compositorMutex_init = FALSE;
void COM_execute(RenderData *rd, bNodeTree *editingtree, int rendering)
{
	if (is_compositorMutex_init == FALSE) { /// TODO: move to blender startup phase
		memset(&s_compositorMutex, 0, sizeof(s_compositorMutex));
		BLI_mutex_init(&s_compositorMutex);
		OCL_init();
		WorkScheduler::initialize(); ///TODO: call workscheduler.deinitialize somewhere
		is_compositorMutex_init = TRUE;
	}
	BLI_mutex_lock(&s_compositorMutex);
	if (editingtree->test_break(editingtree->tbh)) {
		// during editing multiple calls to this method can be triggered.
		// make sure one the last one will be doing the work.
		BLI_mutex_unlock(&s_compositorMutex);
		return;

	}


	/* set progress bar to 0% and status to init compositing */
	editingtree->progress(editingtree->prh, 0.0);

	bool twopass = (editingtree->flag&NTREE_TWO_PASS) > 0 && !rendering;
	/* initialize execution system */
	if (twopass) {
		ExecutionSystem *system = new ExecutionSystem(rd, editingtree, rendering, twopass);
		system->execute();
		delete system;
		
		if (editingtree->test_break(editingtree->tbh)) {
			// during editing multiple calls to this method can be triggered.
			// make sure one the last one will be doing the work.
			BLI_mutex_unlock(&s_compositorMutex);
			return;
		}
	}

	
	ExecutionSystem *system = new ExecutionSystem(rd, editingtree, rendering, false);
	system->execute();
	delete system;

	BLI_mutex_unlock(&s_compositorMutex);
}

void COM_deinitialize() 
{
	if (is_compositorMutex_init)
	{
		BLI_mutex_lock(&s_compositorMutex);
		deintializeDistortionCache();
		WorkScheduler::deinitialize();
		is_compositorMutex_init = FALSE;
		BLI_mutex_unlock(&s_compositorMutex);
		BLI_mutex_end(&s_compositorMutex);
	}
}
