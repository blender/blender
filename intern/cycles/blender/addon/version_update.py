# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import bpy

from bpy.app.handlers import persistent


def custom_bake_remap(scene):
    """
    Remap bake types into the new types and set the flags accordingly
    """
    bake_lookup = (
        'COMBINED',
        'AO',
        'SHADOW',
        'NORMAL',
        'UV',
        'EMIT',
        'ENVIRONMENT',
        'DIFFUSE_DIRECT',
        'DIFFUSE_INDIRECT',
        'DIFFUSE_COLOR',
        'GLOSSY_DIRECT',
        'GLOSSY_INDIRECT',
        'GLOSSY_COLOR',
        'TRANSMISSION_DIRECT',
        'TRANSMISSION_INDIRECT',
        'TRANSMISSION_COLOR')

    diffuse_direct_idx = bake_lookup.index('DIFFUSE_DIRECT')

    cscene = scene.cycles

    # Old bake type
    bake_type_idx = cscene.get("bake_type")

    if bake_type_idx is None:
        cscene.bake_type = 'COMBINED'
        return

    # File doesn't need versioning
    if bake_type_idx < diffuse_direct_idx:
        return

    # File needs versioning
    bake_type = bake_lookup[bake_type_idx]
    cscene.bake_type, end = bake_type.split('_')

    if end == 'DIRECT':
        scene.render.bake.use_pass_indirect = False
        scene.render.bake.use_pass_color = False

    elif end == 'INDIRECT':
        scene.render.bake.use_pass_direct = False
        scene.render.bake.use_pass_color = False

    elif end == 'COLOR':
        scene.render.bake.use_pass_direct = False
        scene.render.bake.use_pass_indirect = False


@persistent
def do_versions(self):
    if bpy.context.preferences.version <= (2, 78, 1):
        prop = bpy.context.preferences.addons[__package__].preferences
        system = bpy.context.preferences.system
        if not prop.is_property_set("compute_device_type"):
            # Device might not currently be available so this can fail
            try:
                if system.legacy_compute_device_type == 1:
                    prop.compute_device_type = 'NONE'  # Was OpenCL
                elif system.legacy_compute_device_type == 2:
                    prop.compute_device_type = 'CUDA'
                else:
                    prop.compute_device_type = 'NONE'
            except:
                pass

            # Init device list for UI
            prop.get_devices(prop.compute_device_type)

    if bpy.context.preferences.version <= (3, 0, 40):
        # Disable OpenCL device
        prop = bpy.context.preferences.addons[__package__].preferences
        if prop.is_property_set("compute_device_type") and prop['compute_device_type'] == 4:
            prop.compute_device_type = 'NONE'

    # We don't modify startup file because it assumes to
    # have all the default values only.
    if not bpy.data.is_saved:
        return

    # Map of versions used by libraries.
    library_versions = {}
    library_versions[bpy.data.version] = [None]
    for library in bpy.data.libraries:
        library_versions.setdefault(library.version, []).append(library)

    # Do versioning per library, since they might have different versions.
    max_need_versioning = (5, 0, 77)
    for version, libraries in library_versions.items():
        if version > max_need_versioning:
            continue

        # Scenes
        for scene in bpy.data.scenes:
            if scene.library not in libraries:
                continue

            # Auto tiling is always enabled now
            if version <= (5, 0, 77):
                cscene = scene.cycles
                if not cscene.use_auto_tile:
                    cscene.use_auto_tile = True
                    cscene.tile_size = 8192

            # Clamp Direct/Indirect separation in 270
            if version <= (2, 70, 0):
                cscene = scene.cycles
                sample_clamp = cscene.get("sample_clamp", False)
                if (sample_clamp and
                    not cscene.is_property_set("sample_clamp_direct") and
                        not cscene.is_property_set("sample_clamp_indirect")):
                    cscene.sample_clamp_direct = sample_clamp
                    cscene.sample_clamp_indirect = sample_clamp

            # Change of Volume Bounces in 271
            if version <= (2, 71, 0):
                cscene = scene.cycles
                if not cscene.is_property_set("volume_bounces"):
                    cscene.volume_bounces = 1

            # Caustics Reflective/Refractive separation in 272
            if version <= (2, 72, 0):
                cscene = scene.cycles
                if (
                        cscene.get("no_caustics", False) and
                        not cscene.is_property_set("caustics_reflective") and
                        not cscene.is_property_set("caustics_refractive")
                ):
                    cscene.caustics_reflective = False
                    cscene.caustics_refractive = False

            # Baking types changed
            if version <= (2, 76, 6):
                custom_bake_remap(scene)

            # Several default changes for 2.77
            if version <= (2, 76, 8):
                cscene = scene.cycles

                # Samples
                if not cscene.is_property_set("samples"):
                    cscene.samples = 10

                # Preview Samples
                if not cscene.is_property_set("preview_samples"):
                    cscene.preview_samples = 10

                # Filter
                if cscene.get("filter_type", -1) == -1:
                    cscene.pixel_filter_type = 'GAUSSIAN'

            if version <= (2, 76, 10):
                cscene = scene.cycles
                if not cscene.is_property_set("pixel_filter_type"):
                    filter_type_int = cscene.get("filter_type", -1)
                    if filter_type_int == 0:
                        cscene.pixel_filter_type = 'BOX'
                    elif filter_type_int == 1:
                        cscene.pixel_filter_type = 'GAUSSIAN'

            if version <= (2, 78, 2):
                cscene = scene.cycles
                if not cscene.is_property_set("light_sampling_threshold"):
                    cscene.light_sampling_threshold = 0.0

            if version <= (2, 79, 0):
                cscene = scene.cycles
                # Default changes
                if not cscene.is_property_set("blur_glossy"):
                    cscene.blur_glossy = 0.0
                if not cscene.is_property_set("sample_clamp_indirect"):
                    cscene.sample_clamp_indirect = 0.0

            if version <= (2, 92, 4):
                if scene.render.engine == 'CYCLES':
                    for view_layer in scene.view_layers:
                        cview_layer = view_layer.cycles
                        view_layer.use_pass_cryptomatte_object = cview_layer.get("use_pass_crypto_object", False)
                        view_layer.use_pass_cryptomatte_material = cview_layer.get("use_pass_crypto_material", False)
                        view_layer.use_pass_cryptomatte_asset = cview_layer.get("use_pass_crypto_asset", False)
                        view_layer.pass_cryptomatte_depth = cview_layer.get("pass_crypto_depth", 6)

            if version <= (2, 93, 7):
                if scene.render.engine == 'CYCLES':
                    for view_layer in scene.view_layers:
                        cview_layer = view_layer.cycles
                        for caov in cview_layer.get("aovs", []):
                            aov_name = caov.get("name", "AOV")
                            if aov_name in view_layer.aovs:
                                continue
                            baov = view_layer.aovs.add()
                            baov.name = caov.get("name", "AOV")
                            baov.type = "COLOR" if caov.get("type", 1) == 1 else "VALUE"

            if version <= (2, 93, 16):
                cscene = scene.cycles
                ao_bounces = cscene.get("ao_bounces", 0)
                ao_bounces_render = cscene.get("ao_bounces_render", 0)
                if scene.render.use_simplify and (ao_bounces or ao_bounces_render):
                    cscene.use_fast_gi = True
                    cscene.ao_bounces = ao_bounces
                    cscene.ao_bounces_render = ao_bounces_render
                else:
                    cscene.ao_bounces = 1
                    cscene.ao_bounces_render = 1

            if version <= (3, 0, 25):
                cscene = scene.cycles

                # Default changes.
                if not cscene.is_property_set("samples"):
                    cscene.samples = 128
                if not cscene.is_property_set("preview_samples"):
                    cscene.preview_samples = 32
                if not cscene.is_property_set("use_adaptive_sampling"):
                    cscene.use_adaptive_sampling = False
                    cscene.use_preview_adaptive_sampling = False
                if not cscene.is_property_set("use_denoising"):
                    cscene.use_denoising = False
                if not cscene.is_property_set("use_preview_denoising"):
                    cscene.use_preview_denoising = False
                if not cscene.is_property_set("sampling_pattern") or \
                   cscene.get('sampling_pattern') >= 2:
                    cscene.sampling_pattern = 'TABULATED_SOBOL'

                # Removal of square samples.
                cscene = scene.cycles
                use_square_samples = cscene.get("use_square_samples", False)

                if use_square_samples:
                    cscene.samples *= cscene.samples
                    cscene.preview_samples *= cscene.preview_samples
                    for layer in scene.view_layers:
                        layer.samples *= layer.samples
                    cscene["use_square_samples"] = False

            if version <= (3, 5, 3):
                cscene = scene.cycles
                # Disable light tree for existing scenes.
                if not cscene.is_property_set("use_light_tree"):
                    cscene.use_light_tree = False
                # Sampling pattern settings are hidden behind a debug menu. Switch to the
                # default faster and fully featured (Supports Scrambling Distance)
                # Tabulated Sobol.
                cscene.sampling_pattern = 'TABULATED_SOBOL'

            if version <= (4, 2, 52):
                cscene = scene.cycles
                # Previous versions defaulted to Tabulated Sobol unless debugging options
                # were enabled, so keep this behavior instead of suddenly defaulting to
                # blue noise if the file happens to contain a different option for the enum.
                cscene.sampling_pattern = 'TABULATED_SOBOL'

        # Lamps
        for light in bpy.data.lights:
            if light.library not in libraries:
                continue

            if version <= (2, 76, 5):
                clight = light.cycles

                # MIS
                if not clight.is_property_set("use_multiple_importance_sampling"):
                    clight.use_multiple_importance_sampling = False

        # Worlds
        for world in bpy.data.worlds:
            if world.library not in libraries:
                continue

            if version <= (2, 76, 9):
                cworld = world.cycles

                # World MIS Resolution
                if not cworld.is_property_set("sample_map_resolution"):
                    cworld.sample_map_resolution = 256

            if version <= (2, 79, 4) or \
               (version >= (2, 80, 0) and version <= (2, 80, 18)):
                cworld = world.cycles
                # World MIS
                if not cworld.is_property_set("sampling_method"):
                    if cworld.get("sample_as_light", True):
                        cworld.sampling_method = 'MANUAL'
                    else:
                        cworld.sampling_method = 'NONE'

        # Materials
        for mat in bpy.data.materials:
            if mat.library not in libraries:
                continue

            if version <= (2, 76, 5):
                cmat = mat.cycles
                # Volume Sampling
                if not cmat.is_property_set("volume_sampling"):
                    cmat.volume_sampling = 'DISTANCE'

            if version <= (2, 79, 2):
                cmat = mat.cycles
                if cmat.get("displacement_method", -1) == -1:
                    cmat['displacement_method'] = 0

            # Change default to bump again.
            if version <= (2, 79, 6) or \
               (version >= (2, 80, 0) and version <= (2, 80, 41)):
                cmat = mat.cycles
                if cmat.get("displacement_method", -1) == -1:
                    cmat['displacement_method'] = 1

            if version <= (3, 5, 3):
                cmat = mat.cycles
                if not cmat.get("sample_as_light", True):
                    cmat.emission_sampling = 'NONE'
