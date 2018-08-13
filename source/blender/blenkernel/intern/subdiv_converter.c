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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "subdiv_converter.h"

#include "BLI_utildefines.h"

#include "opensubdiv_converter_capi.h"

void BKE_subdiv_converter_free(struct OpenSubdiv_Converter *converter)
{
	if (converter->freeUserData) {
		converter->freeUserData(converter);
	}
}

/*OpenSubdiv_FVarLinearInterpolation*/ int
BKE_subdiv_converter_fvar_linear_from_settings(const SubdivSettings *settings)
{
	switch (settings->fvar_linear_interpolation) {
		case SUBDIV_FVAR_LINEAR_INTERPOLATION_NONE:
			return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
		case SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY:
			return OSD_FVAR_LINEAR_INTERPOLATION_CORNERS_ONLY;
		case SUBDIV_FVAR_LINEAR_INTERPOLATION_BOUNDARIES:
			return OSD_FVAR_LINEAR_INTERPOLATION_BOUNDARIES;
		case SUBDIV_FVAR_LINEAR_INTERPOLATION_ALL:
			return OSD_FVAR_LINEAR_INTERPOLATION_ALL;
	}
	BLI_assert(!"Unknown fvar linear interpolation");
	return OSD_FVAR_LINEAR_INTERPOLATION_NONE;
}
