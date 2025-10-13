# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations


def _configure_argument_parser():
    import argparse
    # No help because it conflicts with general Python scripts argument parsing
    parser = argparse.ArgumentParser(description="Cycles Addon argument parser",
                                     add_help=False)
    parser.add_argument("--cycles-print-stats",
                        help="Print rendering statistics to stderr",
                        action='store_true')
    parser.add_argument("--cycles-device",
                        help="Set the device to use for Cycles, overriding user preferences and the scene setting."
                             "Valid options are 'CPU', 'CUDA', 'OPTIX', 'HIP', 'ONEAPI', or 'METAL'."
                             "Additionally, you can append '+CPU' to any GPU type for hybrid rendering.",
                        default=None)
    return parser


def _parse_command_line():
    import sys

    argv = sys.argv
    if "--" not in argv:
        return

    parser = _configure_argument_parser()
    args, _ = parser.parse_known_args(argv[argv.index("--") + 1:])

    if args.cycles_print_stats:
        import _cycles
        _cycles.enable_print_stats()

    if args.cycles_device:
        import _cycles
        if not _cycles.set_device_override(args.cycles_device):
            sys.exit(1)


def init():
    import bpy
    import _cycles
    import os.path

    path = os.path.dirname(__file__)
    user_path = os.path.dirname(os.path.abspath(bpy.utils.user_resource('CONFIG', path='')))

    _cycles.init(path, user_path, bpy.app.background)
    _parse_command_line()


def exit():
    import _cycles
    _cycles.exit()


def create(engine, data, region=None, v3d=None, rv3d=None, preview_osl=False):
    import _cycles
    import bpy

    data = data.as_pointer()
    prefs = bpy.context.preferences.as_pointer()
    screen = 0
    if region:
        screen = region.id_data.as_pointer()
        region = region.as_pointer()
    if v3d:
        screen = screen or v3d.id_data.as_pointer()
        v3d = v3d.as_pointer()
    if rv3d:
        screen = screen or rv3d.id_data.as_pointer()
        rv3d = rv3d.as_pointer()

    engine.session = _cycles.create(engine.as_pointer(), prefs, data, screen, region, v3d, rv3d, preview_osl)


def free(engine):
    if hasattr(engine, "session"):
        if engine.session:
            import _cycles
            _cycles.free(engine.session)
        del engine.session


def render(engine, depsgraph):
    import _cycles
    if hasattr(engine, "session"):
        _cycles.render(engine.session, depsgraph.as_pointer())


def render_frame_finish(engine):
    if not engine.session:
        return

    import _cycles
    _cycles.render_frame_finish(engine.session)


def draw(engine, depsgraph, space_image):
    if not engine.session:
        return

    depsgraph_ptr = depsgraph.as_pointer()
    space_image_ptr = space_image.as_pointer()
    screen_ptr = space_image.id_data.as_pointer()

    import _cycles
    _cycles.draw(engine.session, depsgraph_ptr, screen_ptr, space_image_ptr)


def bake(engine, depsgraph, obj, pass_type, pass_filter, width, height):
    import _cycles
    session = getattr(engine, "session", None)
    if session is not None:
        _cycles.bake(engine.session, depsgraph.as_pointer(), obj.as_pointer(), pass_type, pass_filter, width, height)


def reset(engine, data, depsgraph):
    import _cycles
    import bpy

    prefs = bpy.context.preferences
    if prefs.experimental.use_cycles_debug and prefs.view.show_developer_ui:
        _cycles.debug_flags_update(depsgraph.scene.as_pointer())
    else:
        _cycles.debug_flags_reset()

    data = data.as_pointer()
    depsgraph = depsgraph.as_pointer()
    _cycles.reset(engine.session, data, depsgraph)


def sync(engine, depsgraph, data):
    import _cycles
    _cycles.sync(engine.session, depsgraph.as_pointer())


def view_draw(engine, depsgraph, region, v3d, rv3d):
    import _cycles
    depsgraph = depsgraph.as_pointer()
    v3d = v3d.as_pointer()
    rv3d = rv3d.as_pointer()

    # draw render image
    _cycles.view_draw(engine.session, depsgraph, v3d, rv3d)


def available_devices():
    import _cycles
    return _cycles.available_devices()


def with_osl():
    import _cycles
    return _cycles.with_osl


def osl_version():
    import _cycles
    return _cycles.osl_version


def with_path_guiding():
    import _cycles
    return _cycles.with_path_guiding


def system_info():
    import _cycles
    return _cycles.system_info()


def list_render_passes(scene, srl):
    import _cycles

    crl = srl.cycles

    # Combined pass.
    yield ("Combined", "RGBA", 'COLOR')

    # Keep alignment for readability.
    # autopep8: off

    # Data passes.
    if srl.use_pass_z:                     yield ("Depth",           "Z",    'VALUE')
    if srl.use_pass_mist:                  yield ("Mist",            "Z",    'VALUE')
    if srl.use_pass_position:              yield ("Position",        "XYZ",  'VECTOR')
    if srl.use_pass_normal:                yield ("Normal",          "XYZ",  'VECTOR')
    if srl.use_pass_vector:                yield ("Vector",          "XYZW", 'VECTOR')
    if srl.use_pass_uv:                    yield ("UV",              "UVA",  'VECTOR')
    if srl.use_pass_object_index:          yield ("Object Index",    "X",    'VALUE')
    if srl.use_pass_material_index:        yield ("Material Index",  "X",    'VALUE')

    # Light passes.
    if srl.use_pass_diffuse_direct:        yield ("Diffuse Direct",        "RGB",  'COLOR')
    if srl.use_pass_diffuse_indirect:      yield ("Diffuse Indirect",      "RGB",  'COLOR')
    if srl.use_pass_diffuse_color:         yield ("Diffuse Color",         "RGB",  'COLOR')
    if srl.use_pass_glossy_direct:         yield ("Glossy Direct",         "RGB",  'COLOR')
    if srl.use_pass_glossy_indirect:       yield ("Glossy Indirect",       "RGB",  'COLOR')
    if srl.use_pass_glossy_color:          yield ("Glossy Color",          "RGB",  'COLOR')
    if srl.use_pass_transmission_direct:   yield ("Transmission Direct",   "RGB",  'COLOR')
    if srl.use_pass_transmission_indirect: yield ("Transmission Indirect", "RGB",  'COLOR')
    if srl.use_pass_transmission_color:    yield ("Transmission Color",    "RGB",  'COLOR')
    if crl.use_pass_volume_direct:         yield ("Volume Direct",         "RGB",  'COLOR')
    if crl.use_pass_volume_indirect:       yield ("Volume Indirect",       "RGB",  'COLOR')
    if crl.use_pass_volume_scatter:        yield ("Volume Scatter",        "RGB",  'COLOR')
    if crl.use_pass_volume_transmit:       yield ("Volume Transmit",       "RGB",  'COLOR')
    if crl.use_pass_volume_majorant:       yield ("Volume Majorant",       "Z",    'VALUE')
    if srl.use_pass_emit:                  yield ("Emission",              "RGB",  'COLOR')
    if srl.use_pass_environment:           yield ("Environment",           "RGB",  'COLOR')
    if srl.use_pass_ambient_occlusion:     yield ("Ambient Occlusion",     "RGB",  'COLOR')
    if crl.use_pass_shadow_catcher:        yield ("Shadow Catcher",        "RGB",  'COLOR')
    # autopep8: on

    # Debug passes.
    if crl.pass_debug_sample_count:
        yield ("Debug Sample Count", "X", 'VALUE')
    if crl.pass_render_time:
        # Only yield the pass if rendering on CPU
        if scene.cycles.device == 'CPU':
            yield ("Render Time", "X", "VALUE")

    # Cryptomatte passes.
    # NOTE: Name channels are lowercase RGBA so that compression rules check in OpenEXR DWA code
    # uses lossless compression. Reportedly this naming is the only one which works good from the
    # interoperability point of view. Using XYZW naming is not portable.
    crypto_depth = (min(16, srl.pass_cryptomatte_depth) + 1) // 2
    if srl.use_pass_cryptomatte_object:
        for i in range(0, crypto_depth):
            yield ("CryptoObject" + '{:02d}'.format(i), "rgba", 'COLOR')
    if srl.use_pass_cryptomatte_material:
        for i in range(0, crypto_depth):
            yield ("CryptoMaterial" + '{:02d}'.format(i), "rgba", 'COLOR')
    if srl.use_pass_cryptomatte_asset:
        for i in range(0, crypto_depth):
            yield ("CryptoAsset" + '{:02d}'.format(i), "rgba", 'COLOR')

    # Denoising passes.
    if scene.cycles.use_denoising and crl.use_denoising:
        yield ("Noisy Image", "RGBA", 'COLOR')
        if crl.use_pass_shadow_catcher:
            yield ("Noisy Shadow Catcher", "RGB", 'COLOR')
    if crl.denoising_store_passes:
        yield ("Denoising Normal", "XYZ", 'VECTOR')
        yield ("Denoising Albedo", "RGB", 'COLOR')
        yield ("Denoising Depth", "Z", 'VALUE')

    # Custom AOV passes.
    for aov in srl.aovs:
        if not aov.is_valid:
            continue
        if aov.type == 'VALUE':
            yield (aov.name, "X", 'VALUE')
        else:
            yield (aov.name, "RGBA", 'COLOR')

    # Light groups.
    for lightgroup in srl.lightgroups:
        yield ("Combined_%s" % lightgroup.name, "RGB", 'COLOR')

    # Path guiding debug passes.
    if _cycles.with_debug:
        yield ("Guiding Color", "RGB", 'COLOR')
        yield ("Guiding Probability", "X", 'VALUE')
        yield ("Guiding Average Roughness", "X", 'VALUE')


def register_passes(engine, scene, view_layer):
    for name, channelids, channeltype in list_render_passes(scene, view_layer):
        engine.register_pass(scene, view_layer, name, len(channelids), channelids, channeltype)
