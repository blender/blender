# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import sys


def main():
    bpy.context.scene.frame_set(1)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    depsgraph.update()

    obj = bpy.data.objects["testParticleLegacyTextureUV"].evaluated_get(depsgraph)
    particles = obj.particle_systems[0].particles
    existing_particles = sum(1 for particle in particles if particle.is_exist)

    if len(particles) != 1:
        print(f"Expected exactly one deterministic parent hair particle, got {len(particles)}")
        sys.exit(1)
    if existing_particles != 1:
        print(
            "Expected the empty legacy texture UV name to use the active UV map, "
            f"got {existing_particles} existing particles"
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
