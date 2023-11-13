/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "AnimationClipExporter.h"
#include "GeometryExporter.h"
#include "MaterialExporter.h"

void AnimationClipExporter::exportAnimationClips(Scene *sce)
{
  openLibrary();
  std::map<std::string, COLLADASW::ColladaAnimationClip *> clips;

  std::vector<std::vector<std::string>>::iterator anim_meta_entry;
  for (anim_meta_entry = anim_meta.begin(); anim_meta_entry != anim_meta.end(); ++anim_meta_entry)
  {
    std::vector<std::string> entry = *anim_meta_entry;
    std::string action_id = entry[0];
    std::string action_name = entry[1];

    std::map<std::string, COLLADASW::ColladaAnimationClip *>::iterator it = clips.find(
        action_name);
    if (it == clips.end()) {
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
