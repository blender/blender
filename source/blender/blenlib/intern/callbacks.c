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
 * Contributor(s): Blender Foundation (2011)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/callbacks.c
 *  \ingroup bli
 */

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_callbacks.h"

#include "MEM_guardedalloc.h"

static ListBase callback_slots[BLI_CB_EVT_TOT] = {{NULL}};

void BLI_callback_exec(struct Main *main, struct ID *self, eCbEvent evt)
{
	ListBase *lb = &callback_slots[evt];
	bCallbackFuncStore *funcstore;

	for (funcstore = lb->first; funcstore; funcstore = funcstore->next) {
		funcstore->func(main, self, funcstore->arg);
	}
}

void BLI_callback_add(bCallbackFuncStore *funcstore, eCbEvent evt)
{
	ListBase *lb = &callback_slots[evt];
	BLI_addtail(lb, funcstore);
}

void BLI_callback_global_init(void)
{
	/* do nothing */
}

/* call on application exit */
void BLI_callback_global_finalize(void)
{
	eCbEvent evt;
	for (evt = 0; evt < BLI_CB_EVT_TOT; evt++) {
		ListBase *lb = &callback_slots[evt];
		bCallbackFuncStore *funcstore;
		bCallbackFuncStore *funcstore_next;
		for (funcstore = lb->first; funcstore; funcstore = funcstore_next) {
			funcstore_next = funcstore->next;
			BLI_remlink(lb, funcstore);
			if (funcstore->alloc) {
				MEM_freeN(funcstore);
			}
		}
	}
}
