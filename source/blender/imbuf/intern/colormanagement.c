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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Xavier Thomas,
 *                 Lukas Toenne,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "IMB_colormanagement.h"

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_path_util.h"

#include "BKE_utildefines.h"

#include <ocio_capi.h>

static void colormgmt_load_config(ConstConfigRcPtr* config)
{
	OCIO_setCurrentConfig(config);
}

void colormgmt_free_config(void)
{
}

void IMB_colormanagement_init(void)
{
	const char *ocio_env;
	const char *configdir;
	char configfile[FILE_MAXDIR+FILE_MAXFILE];
	ConstConfigRcPtr* config;

	ocio_env = getenv("OCIO");

	if (ocio_env) {
		config = OCIO_configCreateFromEnv();
	}
	else {
		configdir = BLI_get_folder(BLENDER_DATAFILES, "colormanagement");

		if (configdir) 	{
			BLI_join_dirfile(configfile, sizeof(configfile), configdir, BCM_CONFIG_FILE);
		}

		config = OCIO_configCreateFromFile(configfile);
	}

	if (config) {
		colormgmt_load_config(config);
	}

	OCIO_configRelease(config);
}

void IMB_colormanagement_exit(void)
{
	colormgmt_free_config();
}
