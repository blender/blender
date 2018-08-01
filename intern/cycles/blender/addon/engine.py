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
        regex = re.compile(".*Compatibility Profile Context ([0-9]+(\.[0-9]+)+)$")
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
    args, unknown = parser.parse_known_args(argv[argv.index("--") + 1:])

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
    userpref = bpy.context.user_preferences.as_pointer()
    if region:
        region = region.as_pointer()
    if v3d:
        v3d = v3d.as_pointer()
    if rv3d:
        rv3d = rv3d.as_pointer()

    engine.session = _cycles.create(
            engine.as_pointer(), userpref, data, region, v3d, rv3d, preview_osl)


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


def bake(engine, depsgraph, obj, pass_type, pass_filter, object_id, pixel_array, num_pixels, depth, result):
    import _cycles
    session = getattr(engine, "session", None)
    if session is not None:
        _cycles.bake(engine.session, depsgraph.as_pointer(), obj.as_pointer(), pass_type, pass_filter, object_id, pixel_array.as_pointer(), num_pixels, depth, result.as_pointer())


def reset(engine, data, depsgraph):
    import _cycles
    import bpy

    if bpy.app.debug_value == 256:
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


def register_passes(engine, scene, srl):
    engine.register_pass(scene, srl, "Combined", 4, "RGBA", 'COLOR')

    if srl.use_pass_z:                     engine.register_pass(scene, srl, "Depth",         1, "Z",    'VALUE')
    if srl.use_pass_mist:                  engine.register_pass(scene, srl, "Mist",          1, "Z",    'VALUE')
    if srl.use_pass_normal:                engine.register_pass(scene, srl, "Normal",        3, "XYZ",  'VECTOR')
    if srl.use_pass_vector:                engine.register_pass(scene, srl, "Vector",        4, "XYZW", 'VECTOR')
    if srl.use_pass_uv:                    engine.register_pass(scene, srl, "UV",            3, "UVA",  'VECTOR')
    if srl.use_pass_object_index:          engine.register_pass(scene, srl, "IndexOB",       1, "X",    'VALUE')
    if srl.use_pass_material_index:        engine.register_pass(scene, srl, "IndexMA",       1, "X",    'VALUE')
    if srl.use_pass_shadow:                engine.register_pass(scene, srl, "Shadow",        3, "RGB",  'COLOR')
    if srl.use_pass_ambient_occlusion:     engine.register_pass(scene, srl, "AO",            3, "RGB",  'COLOR')
    if srl.use_pass_diffuse_direct:        engine.register_pass(scene, srl, "DiffDir",       3, "RGB",  'COLOR')
    if srl.use_pass_diffuse_indirect:      engine.register_pass(scene, srl, "DiffInd",       3, "RGB",  'COLOR')
    if srl.use_pass_diffuse_color:         engine.register_pass(scene, srl, "DiffCol",       3, "RGB",  'COLOR')
    if srl.use_pass_glossy_direct:         engine.register_pass(scene, srl, "GlossDir",      3, "RGB",  'COLOR')
    if srl.use_pass_glossy_indirect:       engine.register_pass(scene, srl, "GlossInd",      3, "RGB",  'COLOR')
    if srl.use_pass_glossy_color:          engine.register_pass(scene, srl, "GlossCol",      3, "RGB",  'COLOR')
    if srl.use_pass_transmission_direct:   engine.register_pass(scene, srl, "TransDir",      3, "RGB",  'COLOR')
    if srl.use_pass_transmission_indirect: engine.register_pass(scene, srl, "TransInd",      3, "RGB",  'COLOR')
    if srl.use_pass_transmission_color:    engine.register_pass(scene, srl, "TransCol",      3, "RGB",  'COLOR')
    if srl.use_pass_subsurface_direct:     engine.register_pass(scene, srl, "SubsurfaceDir", 3, "RGB",  'COLOR')
    if srl.use_pass_subsurface_indirect:   engine.register_pass(scene, srl, "SubsurfaceInd", 3, "RGB",  'COLOR')
    if srl.use_pass_subsurface_color:      engine.register_pass(scene, srl, "SubsurfaceCol", 3, "RGB",  'COLOR')
    if srl.use_pass_emit:                  engine.register_pass(scene, srl, "Emit",          3, "RGB",  'COLOR')
    if srl.use_pass_environment:           engine.register_pass(scene, srl, "Env",           3, "RGB",  'COLOR')

    crl = srl.cycles
    if crl.pass_debug_render_time:             engine.register_pass(scene, srl, "Debug Render Time",             1, "X",   'VALUE')
    if crl.pass_debug_bvh_traversed_nodes:     engine.register_pass(scene, srl, "Debug BVH Traversed Nodes",     1, "X",   'VALUE')
    if crl.pass_debug_bvh_traversed_instances: engine.register_pass(scene, srl, "Debug BVH Traversed Instances", 1, "X",   'VALUE')
    if crl.pass_debug_bvh_intersections:       engine.register_pass(scene, srl, "Debug BVH Intersections",       1, "X",   'VALUE')
    if crl.pass_debug_ray_bounces:             engine.register_pass(scene, srl, "Debug Ray Bounces",             1, "X",   'VALUE')
    if crl.use_pass_volume_direct:             engine.register_pass(scene, srl, "VolumeDir",                     3, "RGB", 'COLOR')
    if crl.use_pass_volume_indirect:           engine.register_pass(scene, srl, "VolumeInd",                     3, "RGB", 'COLOR')

    cscene = scene.cycles
    if crl.use_denoising and crl.denoising_store_passes and not cscene.use_progressive_refine:
        engine.register_pass(scene, srl, "Denoising Normal",          3, "XYZ", 'VECTOR')
        engine.register_pass(scene, srl, "Denoising Normal Variance", 3, "XYZ", 'VECTOR')
        engine.register_pass(scene, srl, "Denoising Albedo",          3, "RGB", 'COLOR')
        engine.register_pass(scene, srl, "Denoising Albedo Variance", 3, "RGB", 'COLOR')
        engine.register_pass(scene, srl, "Denoising Depth",           1, "Z",   'VALUE')
        engine.register_pass(scene, srl, "Denoising Depth Variance",  1, "Z",   'VALUE')
        engine.register_pass(scene, srl, "Denoising Shadow A",        3, "XYV", 'VECTOR')
        engine.register_pass(scene, srl, "Denoising Shadow B",        3, "XYV", 'VECTOR')
        engine.register_pass(scene, srl, "Denoising Image",           3, "RGB", 'COLOR')
        engine.register_pass(scene, srl, "Denoising Image Variance",  3, "RGB", 'COLOR')
