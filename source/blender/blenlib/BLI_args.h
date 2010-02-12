/**
 * A general argument parsing module
 *
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

#ifndef BLI_ARGS_H
#define BLI_ARGS_H

struct bArgs;
typedef struct bArgs bArgs;

/* returns the number of extra arguments consumed by the function. 0 is normal value, -1 stops parsing arguments, other negative indicates skip */
typedef int	(*BA_ArgCallback)(int argc, char **argv, void *data);

struct bArgs *BLI_argsInit(int argc, char **argv);
void BLI_argsFree(struct bArgs *ba);

/* pass starts at 1, -1 means valid all the time */
void BLI_argsAdd(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data);
void BLI_argsAddCase(struct bArgs *ba, char *arg, int pass, BA_ArgCallback cb, void *data); /* not case specific */

void BLI_argsParse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *data);

void BLI_argsPrint(struct bArgs *ba);
char **BLI_argsArgv(struct bArgs *ba);

#endif
