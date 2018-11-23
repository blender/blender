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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "COLLADASWLibraryAnimationClips.h"


class AnimationClipExporter:COLLADASW::LibraryAnimationClips {
private:
	Depsgraph *depsgraph;
	Scene *scene;
	COLLADASW::StreamWriter *sw;
	const ExportSettings *export_settings;
	std::vector<std::vector<std::string>> anim_meta;

public:

	AnimationClipExporter(Depsgraph *depsgraph , COLLADASW::StreamWriter *sw, const ExportSettings *export_settings, std::vector<std::vector<std::string>> anim_meta) :
		depsgraph(depsgraph),
		COLLADASW::LibraryAnimationClips(sw),
		export_settings(export_settings),
		anim_meta(anim_meta)
	{
		this->sw = sw;
	}

	void exportAnimationClips(Scene *sce);
};
