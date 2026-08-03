/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

struct BlendDataReader;
struct BlendWriter;
struct Object;
struct ReportList;
struct Scene;
struct bAnimVizSettings;
struct bMotionPath;
struct bPoseChannel;

namespace bke {

/* ---------------------------------------------------- */
/* Animation Visualization */

namespace animviz {

/**
 * Initialize the default settings for animation visualization.
 */
void settings_init(struct bAnimVizSettings *avs);

}  // namespace animviz

/* ---------------------------------------------------- */
/* Motion Path */

namespace motionpath {

/**
 * Make a copy of motion-path data, so that viewing with copy on write works.
 */
struct bMotionPath *copy(const struct bMotionPath *mpath_src);

/**
 * Free the given motion path's cache.
 */
void free_cache(struct bMotionPath *mpath);
/**
 * Free the given motion path instance and its data.
 * \note this frees the motion path given!
 */
void free(struct bMotionPath *mpath);

/**
 * Esnures that a motion path exists for the given data and that the settings correctly adhere to
 * the animviz settings.
 *
 * \param scene: Current scene (for frame ranges, etc.)
 * \param ob: Object to add paths for (must be provided)
 * \param pchan: Pose-channel to add paths for
 * (optional; if not provided, object-paths are assumed).
 */
struct bMotionPath *ensure(struct ReportList *reports,
                           struct Scene *scene,
                           struct Object *ob,
                           struct bPoseChannel *pchan);

void blend_write(struct BlendWriter *writer, struct bMotionPath *mpath);
void blend_read_data(struct BlendDataReader *reader, struct bMotionPath *mpath);

}  // namespace motionpath

}  // namespace bke
}  // namespace blender
