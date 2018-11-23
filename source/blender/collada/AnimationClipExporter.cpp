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

#include "GeometryExporter.h"
#include "AnimationClipExporter.h"
#include "MaterialExporter.h"

void AnimationClipExporter::exportAnimationClips(Scene *sce)
{
	openLibrary();
	std::map<std::string, COLLADASW::ColladaAnimationClip *> clips;

	std::vector<std::vector<std::string>>::iterator anim_meta_entry;
	for (anim_meta_entry = anim_meta.begin(); anim_meta_entry != anim_meta.end(); ++anim_meta_entry) {
		std::vector<std::string> entry = *anim_meta_entry;
		std::string action_id = entry[0];
		std::string action_name = entry[1];

		std::map<std::string, COLLADASW::ColladaAnimationClip *>::iterator it = clips.find(action_name);
		if (it == clips.end())
		{
			COLLADASW::ColladaAnimationClip *clip = new COLLADASW::ColladaAnimationClip(action_name);
			clips[action_name] = clip;
		}
		COLLADASW::ColladaAnimationClip *clip = clips[action_name];
		clip->setInstancedAnimation(action_id);
	}

	std::map<std::string, COLLADASW::ColladaAnimationClip *>::iterator clips_it;
	for (clips_it = clips.begin(); clips_it != clips.end(); clips_it++) {
		COLLADASW::ColladaAnimationClip *clip = (COLLADASW::ColladaAnimationClip *)clips_it->second;
		addAnimationClip(*clip);
	}

	closeLibrary();
}
