/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <BPY_extern.h>

#include <stdio.h>

void BPY_start_python(void)
{
	return;
}

void BPY_end_python(void)
{
	return;
}

int BPY_Err_getLinenumber(void)
{
	return (0);
}

const char *BPY_Err_getFilename(void)
{
	return (NULL);
}

struct _object *BPY_txt_do_python(struct SpaceText* st)
{
	return (NULL);
}

void BPY_free_compiled_text(struct Text* text)
{
	return;
}

void BPY_clear_bad_scriptlinks(struct Text *byebye)
{
	return;
}

void BPY_do_all_scripts(short event)
{
	return;
}

void BPY_do_pyscript(struct ID *id, short event)
{
	return;
}

void BPY_free_scriptlink(struct ScriptLink *slink)
{
	return;
}

void BPY_copy_scriptlink(struct ScriptLink *scriptlink)
{
	return;
}

int BPY_call_importloader(char *name)
{
	return (0);
}

int BPY_spacetext_is_pywin(struct SpaceText *st)
{
	return (0);
}

void BPY_spacetext_do_pywin_draw(struct SpaceText *st)
{
	return;
}

void BPY_spacetext_do_pywin_event(struct SpaceText *st, unsigned short event, short val)
{
	return;
}

