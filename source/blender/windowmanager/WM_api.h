/**
 * $Id:
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef WM_API_H
#define WM_API_H

/* dna-savable wmStructs here */
#include "DNA_windowmanager_types.h"

struct bContext;
struct wmEvent;
struct wmEventHandler;

			/* general API */
void		WM_setprefsize		(int stax, int stay, int sizx, int sizy);

void		WM_init				(struct bContext *C);
void		WM_exit				(struct bContext *C);
void		WM_main				(struct bContext *C);

			/* files */
int			WM_read_homefile	(struct bContext *C, int from_memory);
int			WM_write_homefile	(struct bContext *C, struct wmOperator *op);
void		WM_read_file		(struct bContext *C, char *name);
void		WM_write_file		(struct bContext *C, char *target);
void		WM_read_autosavefile(struct bContext *C);
void		WM_write_autosave	(struct bContext *C);

			/* mouse cursors */
void		WM_init_cursor_data	(void);
void		WM_set_cursor		(struct bContext *C, int curs);

			/* keymap and handlers */
void		WM_keymap_set_item	(ListBase *lb, char *idname, short type, 
								 short val, int modifier, short keymodifier);
void		WM_keymap_verify_item(ListBase *lb, char *idname, short type, 
								 short val, int modifier, short keymodifier);
struct wmEventHandler *WM_event_add_keymap_handler(ListBase *keymap, ListBase *handlers);
struct wmEventHandler *WM_event_add_modal_keymap_handler(ListBase *keymap, ListBase *handlers, wmOperator *op);
			
			/* operator api, default callbacks */
			/* confirm menu + exec */
int			WM_operator_confirm		(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
			/* context checks */
int			WM_operator_winactive	(struct bContext *C);

			/* operator api */
wmOperatorType *WM_operatortype_find(const char *idname);
void		WM_operator_register(wmWindowManager *wm, wmOperator *ot);


#endif /* WM_API_H */

