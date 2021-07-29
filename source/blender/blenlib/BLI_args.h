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

#ifndef __BLI_ARGS_H__
#define __BLI_ARGS_H__

/** \file BLI_args.h
 *  \ingroup bli
 *  \brief A general argument parsing module.
 */

struct bArgs;
typedef struct bArgs bArgs;

/**
 * Returns the number of extra arguments consumed by the function.
 * -  0 is normal value,
 * - -1 stops parsing arguments, other negative indicates skip
 */
typedef int (*BA_ArgCallback)(int argc, const char **argv, void *data);

struct bArgs *BLI_argsInit(int argc, const char **argv);
void BLI_argsFree(struct bArgs *ba);

/**
 * Pass starts at 1, -1 means valid all the time
 * short_arg or long_arg can be null to specify no short or long versions
 */
void BLI_argsAdd(struct bArgs *ba, int pass,
                 const char *short_arg, const char *long_arg,
                 const char *doc, BA_ArgCallback cb, void *data);

/**
 * Short_case and long_case specify if those arguments are case specific
 */
void BLI_argsAddCase(struct bArgs *ba, int pass,
                     const char *short_arg, int short_case,
                     const char *long_arg, int long_case,
                     const char *doc, BA_ArgCallback cb, void *data);

void BLI_argsParse(struct bArgs *ba, int pass, BA_ArgCallback default_cb, void *data);

void BLI_argsPrintArgDoc(struct bArgs *ba, const char *arg);
void BLI_argsPrintOtherDoc(struct bArgs *ba);

void BLI_argsPrint(struct bArgs *ba);
const char **BLI_argsArgv(struct bArgs *ba);

#endif
