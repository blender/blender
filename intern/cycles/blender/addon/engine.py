#
# Copyright 2011-2013 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# <pep8 compliant>


def _is_using_buggy_driver():
    import bgl
    # We need to be conservative here because in multi-GPU systems display card
    # might be quite old, but others one might be just good.
    #
    # So We shouldn't disable possible good dedicated cards just because display
    # card seems weak. And instead we only blacklist configurations which are
    # proven to cause problems.
    if bgl.glGetString(bgl.GL_VENDOR) == "ATI Technologies Inc.":
        import re
        version = bgl.glGetString(bgl.GL_VERSION)
        if version.endswith("Compatibility Profile Context"):
            # Old HD 4xxx and 5xxx series drivers did not have driver version
            # in the version string, but those cards do not quite work and
            # causing crashes.
            return True
        regex = re.compile(".*Compatibility Profile Context ([0-9]+(\\.[0-9]+)+)$")
        if not regex.match(version):
            # Skip cards like FireGL
            return False
        version = regex.sub("\\1", version).split('.')
        return int(version[0]) == 8
    return False


def _workaround_buggy_drivers():
    if _is_using_buggy_driver():
        import _cycles
        if hasattr(_cycles, "opencl_disable"):
            print("Cycles: OpenGL driver known to be buggy, disabling OpenCL platform.")
            _cycles.opencl_disable()


def _configure_argument_parser():
    import argparse
    # No help because it conflicts with general Python scripts argument parsing
    parser = argparse.ArgumentParser(description="Cycles Addon argument parser",
                                     add_help=False)
    parser.add_argument("--cycles-resumable-num-chunks",
                        help="Number of chunks to split sample range into",
                        default=None)
    parser.add_argument("--cycles-resumable-current-chunk",
                        help="Current chunk of samples range to render",
                        default=None)
    parser.add_argument("--cycles-resumable-start-chunk",
                        help="Start chunk to render",
                        default=None)
    parser.add_argument("--cycles-resumable-end-chunk",
                        help="End chunk to render",
                        default=None)
    parser.add_argument("--cycles-print-stats",
                        help="Print rendering statistics to stderr",
                        action='store_true')
    return parser


def _parse_command_line():
    import sys

    argv = sys.argv
    if "--" not in argv:
        return

    parser = _configure_argument_parser()
    args, _ = parser.parse_known_args(argv[argv.index("--") + 1:])

    if args.cycles_resumable_num_chunks is not None:
        if args.cycles_resumable_current_chunk is not None:
            import _cycles
            _cycles.set_resumable_chunk(
                int(args.cycles_resumable_num_chunks),
                int(args.cycles_resumable_current_chunk),
            )
        elif args.cycles_resumable_start_chunk is not None and \
                args.cycles_resumable_end_chunk:
            import _cycles
            _cycles.set_resumable_chunk_range(
                int(args.cycles_resumable_num_chunks),
                int(args.cycles_resumable_start_chunk),
                int(args.cycles_resumable_end_chunk),
            )
    if args.cycles_print_stats:
        import _cycles
        _cycles.enable_print_stats()


def init():
    import bpy
    import _cycles
    import os.path

    # Workaround possibly buggy legacy drivers which crashes on the OpenCL
    # device enumeration.
    #
    # This checks are not really correct because they might still fail
    # in the case of multiple GPUs. However, currently buggy drivers
    # are really old and likely to be used in single GPU systems only
    # anyway.
    #
    # Can't do it in the background mode, so we hope OpenCL is no enabled
    # in the user preferences.
    if not bpy.app.background:
        _workaround_buggy_drivers()

    path = os.path.dirname(__file__)
    user_path = os.path.dirname(os.path.abspath(bpy.utils.user_resource('CONFIG', '')))

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

    engine.session = _cycles.create(
            engine.as_pointer(), prefs, data, screen, region, v3d, rv3d, preview_osl)


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


def draw(engine, depsgraph, region, v3d, rv3d):
    import _cycles
    depsgraph = depsgraph.as_pointer()
    v3d = v3d.as_pointer()
    rv3d = rv3d.as_pointer()

    # draw render image
    _cycles.draw(engine.session, depsgraph, v3d, rv3d)


def available_devices():
    import _cycles
    return _cycles.available_devices()


def with_osl():
    import _cycles
    return _cycles.with_osl


def with_network():
    import _cycles
    return _cycles.with_network


def system_info():
    import _cycles
    return _cycles.system_info()

def list_render_passes(srl):
    # Builtin Blender passes.
    yield ("Combined", "RGBA", 'COLOR')

    if srl.use_pass_z:                     yield ("Depth",         "Z",    'VALUE')
    if srl.use_pass_mist:                  yield ("Mist",          "Z",    'VALUE')
    if srl.use_pass_normal:                yield ("Normal",        "XYZ",  'VECTOR')
    if srl.use_pass_vector:                yield ("Vector",        "XYZW", 'VECTOR')
    if srl.use_pass_uv:                    yield ("UV",            "UVA",  'VECTOR')
    if srl.use_pass_object_index:          yield ("IndexOB",       "X",    'VALUE')
    if srl.use_pass_material_index:        yield ("IndexMA",       "X",    'VALUE')
    if srl.use_pass_shadow:                yield ("Shadow",        "RGB",  'COLOR')
    if srl.use_pass_ambient_occlusion:     yield ("AO",            "RGB",  'COLOR')
    if srl.use_pass_diffuse_direct:        yield ("DiffDir",       "RGB",  'COLOR')
    if srl.use_pass_diffuse_indirect:      yield ("DiffInd",       "RGB",  'COLOR')
    if srl.use_pass_diffuse_color:         yield ("DiffCol",       "RGB",  'COLOR')
    if srl.use_pass_glossy_direct:         yield ("GlossDir",      "RGB",  'COLOR')
    if srl.use_pass_glossy_indirect:       yield ("GlossInd",      "RGB",  'COLOR')
    if srl.use_pass_glossy_color:          yield ("GlossCol",      "RGB",  'COLOR')
    if srl.use_pass_transmission_direct:   yield ("TransDir",      "RGB",  'COLOR')
    if srl.use_pass_transmission_indirect: yield ("TransInd",      "RGB",  'COLOR')
    if srl.use_pass_transmission_color:    yield ("TransCol",      "RGB",  'COLOR')
    if srl.use_pass_emit:                  yield ("Emit",          "RGB",  'COLOR')
    if srl.use_pass_environment:           yield ("Env",           "RGB",  'COLOR')

    # Cycles specific passes.
    crl = srl.cycles
    if crl.pass_debug_render_time:             yield ("Debug Render Time",             "X",   'VALUE')
    if crl.pass_debug_bvh_traversed_nodes:     yield ("Debug BVH Traversed Nodes",     "X",   'VALUE')
    if crl.pass_debug_bvh_traversed_instances: yield ("Debug BVH Traversed Instances", "X",   'VALUE')
    if crl.pass_debug_bvh_intersections:       yield ("Debug BVH Intersections",       "X",   'VALUE')
    if crl.pass_debug_ray_bounces:             yield ("Debug Ray Bounces",             "X",   'VALUE')
    if crl.pass_debug_sample_count:            yield ("Debug Sample Count",            "X",   'VALUE')
    if crl.use_pass_volume_direct:             yield ("VolumeDir",                     "RGB", 'COLOR')
    if crl.use_pass_volume_indirect:           yield ("VolumeInd",                     "RGB", 'COLOR')

    # Cryptomatte passes.
    crypto_depth = (crl.pass_crypto_depth + 1) // 2
    if crl.use_pass_crypto_object:
        for i in range(0, crypto_depth):
            yield ("CryptoObject" + '{:02d}'.format(i), "RGBA", 'COLOR')
    if crl.use_pass_crypto_material:
        for i in range(0, crypto_depth):
            yield ("CryptoMaterial" + '{:02d}'.format(i), "RGBA", 'COLOR')
    if srl.cycles.use_pass_crypto_asset:
        for i in range(0, crypto_depth):
            yield ("CryptoAsset" + '{:02d}'.format(i), "RGBA", 'COLOR')

    # Denoising passes.
    if crl.use_denoising or crl.denoising_store_passes:
        yield ("Noisy Image", "RGBA", 'COLOR')
        if crl.denoising_store_passes:
            yield ("Denoising Normal",          "XYZ", 'VECTOR')
            yield ("Denoising Albedo",          "RGB", 'COLOR')
            yield ("Denoising Depth",           "Z",   'VALUE')
            yield ("Denoising Shadowing",       "X",   'VALUE')
            yield ("Denoising Variance",        "RGB", 'COLOR')
            yield ("Denoising Intensity",       "X",   'VALUE')
            clean_options = ("denoising_diffuse_direct", "denoising_diffuse_indirect",
                             "denoising_glossy_direct", "denoising_glossy_indirect",
                             "denoising_transmission_direct", "denoising_transmission_indirect")
            if any(getattr(crl, option) for option in clean_options):
                yield ("Denoising Clean", "RGB", 'COLOR')

    # Custom AOV passes.
    for aov in crl.aovs:
        if aov.type == 'VALUE':
            yield (aov.name, "X", 'VALUE')
        else:
            yield (aov.name, "RGBA", 'COLOR')

def register_passes(engine, scene, view_layer):
    # Detect duplicate render pass names, first one wins.
    listed = set()
    for name, channelids, channeltype in list_render_passes(view_layer):
        if name not in listed:
            engine.register_pass(scene, view_layer, name, len(channelids), channelids, channeltype)
            listed.add(name)

def detect_conflicting_passes(view_layer):
    # Detect conflicting render pass names for UI.
    counter = {}
    for name, _, _ in list_render_passes(view_layer):
        counter[name] = counter.get(name, 0) + 1

    for aov in view_layer.cycles.aovs:
        if counter[aov.name] > 1:
            aov.conflict = "Conflicts with another render pass with the same name"
        else:
            aov.conflict = ""
