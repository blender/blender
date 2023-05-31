/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Main;
struct Scene;
struct Simulation;

void *BKE_simulation_add(struct Main *bmain, const char *name);

void BKE_simulation_data_update(struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Simulation *simulation);

void BKE_simulation_reset_scene(Scene *scene);

#ifdef __cplusplus
}
#endif
