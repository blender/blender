# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Populate a template file (POT format currently) from Blender RNA/py/C data.
# Note: This script is meant to be used from inside Blender!

import os

import bpy
from mathutils import Vector, Euler, Matrix


INTERN_PREVIEW_TYPES = {'MATERIAL', 'LAMP', 'WORLD', 'TEXTURE', 'IMAGE'}
OBJECT_TYPES_RENDER = {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'}


def ids_nolib(bids):
    return (bid for bid in bids if not bid.library)


def rna_backup_gen(data, include_props=None, exclude_props=None, root=()):
    # only writable properties...
    for p in data.bl_rna.properties:
        pid = p.identifier
        if pid in {'rna_type', }:
            continue
        path = root + (pid,)
        if include_props is not None and path not in include_props:
            continue
        if exclude_props is not None and path in exclude_props:
            continue
        val = getattr(data, pid)
        if val is not None and p.type == 'POINTER':
            # recurse!
            yield from rna_backup_gen(val, include_props, exclude_props, root=path)
        elif data.is_property_readonly(pid):
            continue
        else:
            yield path, val


def rna_backup_restore(data, backup):
    for path, val in backup:
        dt = data
        for pid in path[:-1]:
            dt = getattr(dt, pid)
        setattr(dt, path[-1], val)


def do_previews(do_objects, do_groups, do_scenes, do_data_intern):
    import collections

    # Helpers.
    RenderContext = collections.namedtuple("RenderContext", (
        "scene", "world", "camera", "lamp", "camera_data", "lamp_data", "image",  # All those are names!
        "backup_scene", "backup_world", "backup_camera", "backup_lamp", "backup_camera_data", "backup_lamp_data",
    ))

    RENDER_PREVIEW_SIZE = bpy.app.render_preview_size

    def render_context_create(engine, objects_ignored):
        if engine == '__SCENE':
            backup_scene, backup_world, backup_camera, backup_lamp, backup_camera_data, backup_lamp_data = [()] * 6
            scene = bpy.context.screen.scene
            exclude_props = {('world',), ('camera',), ('tool_settings',), ('preview',)}
            backup_scene = tuple(rna_backup_gen(scene, exclude_props=exclude_props))
            world = scene.world
            camera = scene.camera
            if camera:
                camera_data = camera.data
            else:
                backup_camera, backup_camera_data = [None] * 2
                camera_data = bpy.data.cameras.new("TEMP_preview_render_camera")
                camera = bpy.data.objects.new("TEMP_preview_render_camera", camera_data)
                camera.rotation_euler = Euler((1.1635528802871704, 0.0, 0.7853981852531433), 'XYZ')  # (66.67, 0.0, 45.0)
                scene.camera = camera
                scene.objects.link(camera)
            # TODO: add lamp if none found in scene?
            lamp = None
            lamp_data = None
        else:
            backup_scene, backup_world, backup_camera, backup_lamp, backup_camera_data, backup_lamp_data = [None] * 6

            scene = bpy.data.scenes.new("TEMP_preview_render_scene")
            world = bpy.data.worlds.new("TEMP_preview_render_world")
            camera_data = bpy.data.cameras.new("TEMP_preview_render_camera")
            camera = bpy.data.objects.new("TEMP_preview_render_camera", camera_data)
            lamp_data = bpy.data.lamps.new("TEMP_preview_render_lamp", 'SPOT')
            lamp = bpy.data.objects.new("TEMP_preview_render_lamp", lamp_data)

            objects_ignored.add((camera.name, lamp.name))

            scene.world = world

            camera.rotation_euler = Euler((1.1635528802871704, 0.0, 0.7853981852531433), 'XYZ')  # (66.67, 0.0, 45.0)
            scene.camera = camera
            scene.objects.link(camera)

            lamp.rotation_euler = Euler((0.7853981852531433, 0.0, 1.7453292608261108), 'XYZ')  # (45.0, 0.0, 100.0)
            lamp_data.falloff_type = 'CONSTANT'
            lamp_data.spot_size = 1.0471975803375244  # 60
            scene.objects.link(lamp)

            if engine == 'BLENDER_RENDER':
                scene.render.engine = 'BLENDER_RENDER'
                scene.render.alpha_mode = 'TRANSPARENT'

                world.use_sky_blend = True
                world.horizon_color = 0.9, 0.9, 0.9
                world.zenith_color = 0.5, 0.5, 0.5
                world.ambient_color = 0.1, 0.1, 0.1
                world.light_settings.use_environment_light = True
                world.light_settings.environment_energy = 1.0
                world.light_settings.environment_color = 'SKY_COLOR'
            elif engine == 'CYCLES':
                scene.render.engine = 'CYCLES'
                scene.cycles.film_transparent = True
                # TODO: define Cycles world?

        scene.render.image_settings.file_format = 'PNG'
        scene.render.image_settings.color_depth = '8'
        scene.render.image_settings.color_mode = 'RGBA'
        scene.render.image_settings.compression = 25
        scene.render.resolution_x = RENDER_PREVIEW_SIZE
        scene.render.resolution_y = RENDER_PREVIEW_SIZE
        scene.render.resolution_percentage = 100
        scene.render.filepath = os.path.join(bpy.app.tempdir, 'TEMP_preview_render.png')
        scene.render.use_overwrite = True
        scene.render.use_stamp = False

        image = bpy.data.images.new("TEMP_render_image", RENDER_PREVIEW_SIZE, RENDER_PREVIEW_SIZE, alpha=True)
        image.source = 'FILE'
        image.filepath = scene.render.filepath

        return RenderContext(
            scene.name, world.name if world else None, camera.name, lamp.name if lamp else None,
            camera_data.name, lamp_data.name if lamp_data else None, image.name,
            backup_scene, backup_world, backup_camera, backup_lamp, backup_camera_data, backup_lamp_data,
        )

    def render_context_delete(render_context):
        # We use try/except blocks here to avoid crash, too much things can go wrong, and we want to leave the current
        # .blend as clean as possible!
        success = True

        scene = bpy.data.scenes[render_context.scene, None]
        try:
            if render_context.backup_scene is None:
                scene.world = None
                scene.camera = None
                if render_context.camera:
                    scene.objects.unlink(bpy.data.objects[render_context.camera, None])
                if render_context.lamp:
                    scene.objects.unlink(bpy.data.objects[render_context.lamp, None])
                bpy.data.scenes.remove(scene, do_unlink=True)
                scene = None
            else:
                rna_backup_restore(scene, render_context.backup_scene)
        except Exception as e:
            print("ERROR:", e)
            success = False

        if render_context.world is not None:
            try:
                world = bpy.data.worlds[render_context.world, None]
                if render_context.backup_world is None:
                    if scene is not None:
                        scene.world = None
                    world.user_clear()
                    bpy.data.worlds.remove(world)
                else:
                    rna_backup_restore(world, render_context.backup_world)
            except Exception as e:
                print("ERROR:", e)
                success = False

        if render_context.camera:
            try:
                camera = bpy.data.objects[render_context.camera, None]
                if render_context.backup_camera is None:
                    if scene is not None:
                        scene.camera = None
                        scene.objects.unlink(camera)
                    camera.user_clear()
                    bpy.data.objects.remove(camera)
                    bpy.data.cameras.remove(bpy.data.cameras[render_context.camera_data, None])
                else:
                    rna_backup_restore(camera, render_context.backup_camera)
                    rna_backup_restore(bpy.data.cameras[render_context.camera_data, None],
                                       render_context.backup_camera_data)
            except Exception as e:
                print("ERROR:", e)
                success = False

        if render_context.lamp:
            try:
                lamp = bpy.data.objects[render_context.lamp, None]
                if render_context.backup_lamp is None:
                    if scene is not None:
                        scene.objects.unlink(lamp)
                    lamp.user_clear()
                    bpy.data.objects.remove(lamp)
                    bpy.data.lamps.remove(bpy.data.lamps[render_context.lamp_data, None])
                else:
                    rna_backup_restore(lamp, render_context.backup_lamp)
                    rna_backup_restore(bpy.data.lamps[render_context.lamp_data, None], render_context.backup_lamp_data)
            except Exception as e:
                print("ERROR:", e)
                success = False

        try:
            image = bpy.data.images[render_context.image, None]
            image.user_clear()
            bpy.data.images.remove(image)
        except Exception as e:
            print("ERROR:", e)
            success = False

        return success

    def objects_render_engine_guess(obs):
        for obname, libpath in obs:
            ob = bpy.data.objects[obname, libpath]
            for matslot in ob.material_slots:
                mat = matslot.material
                if mat and mat.use_nodes and mat.node_tree:
                    for nd in mat.node_tree.nodes:
                        if nd.shading_compatibility == {'NEW_SHADING'}:
                            return 'CYCLES'
        return 'BLENDER_RENDER'

    def object_bbox_merge(bbox, ob, ob_space, offset_matrix):
        # Take group instances into account (including linked one in this case).
        if ob.type == 'EMPTY' and ob.dupli_type == 'GROUP':
            grp_objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in ob.dupli_group.objects)
            if (len(grp_objects) == 0):
                ob_bbox = ob.bound_box
            else:
                coords = objects_bbox_calc(ob_space, grp_objects,
                                           Matrix.Translation(ob.dupli_group.dupli_offset).inverted())
                ob_bbox = ((coords[0], coords[1], coords[2]), (coords[21], coords[22], coords[23]))
        elif ob.bound_box:
            ob_bbox = ob.bound_box
        else:
            ob_bbox = ((-ob.scale.x, -ob.scale.y, -ob.scale.z), (ob.scale.x, ob.scale.y, ob.scale.z))

        for v in ob_bbox:
            v = offset_matrix * Vector(v) if offset_matrix is not None else Vector(v)
            v = ob_space.matrix_world.inverted() * ob.matrix_world * v
            if bbox[0].x > v.x:
                bbox[0].x = v.x
            if bbox[0].y > v.y:
                bbox[0].y = v.y
            if bbox[0].z > v.z:
                bbox[0].z = v.z
            if bbox[1].x < v.x:
                bbox[1].x = v.x
            if bbox[1].y < v.y:
                bbox[1].y = v.y
            if bbox[1].z < v.z:
                bbox[1].z = v.z

    def objects_bbox_calc(camera, objects, offset_matrix):
        bbox = (Vector((1e24, 1e24, 1e24)), Vector((-1e24, -1e24, -1e24)))
        for obname, libpath in objects:
            ob = bpy.data.objects[obname, libpath]
            object_bbox_merge(bbox, ob, camera, offset_matrix)
        # Our bbox has been generated in camera local space, bring it back in world one
        bbox[0][:] = camera.matrix_world * bbox[0]
        bbox[1][:] = camera.matrix_world * bbox[1]
        cos = (
            bbox[0].x, bbox[0].y, bbox[0].z,
            bbox[0].x, bbox[0].y, bbox[1].z,
            bbox[0].x, bbox[1].y, bbox[0].z,
            bbox[0].x, bbox[1].y, bbox[1].z,
            bbox[1].x, bbox[0].y, bbox[0].z,
            bbox[1].x, bbox[0].y, bbox[1].z,
            bbox[1].x, bbox[1].y, bbox[0].z,
            bbox[1].x, bbox[1].y, bbox[1].z,
        )
        return cos

    def preview_render_do(render_context, item_container, item_name, objects, offset_matrix=None):
        scene = bpy.data.scenes[render_context.scene, None]
        if objects is not None:
            camera = bpy.data.objects[render_context.camera, None]
            lamp = bpy.data.objects[render_context.lamp, None] if render_context.lamp is not None else None
            cos = objects_bbox_calc(camera, objects, offset_matrix)
            loc, ortho_scale = camera.camera_fit_coords(scene, cos)
            camera.location = loc
            # Set camera clipping accordingly to computed bbox.
            min_dist = 1e24
            max_dist = -1e24
            for co in zip(*(iter(cos),) * 3):
                dist = (Vector(co) - loc).length
                if dist < min_dist:
                    min_dist = dist
                if dist > max_dist:
                    max_dist = dist
            camera.data.clip_start = min_dist / 2
            camera.data.clip_end = max_dist * 2
            if lamp:
                loc, ortho_scale = lamp.camera_fit_coords(scene, cos)
                lamp.location = loc
        scene.update()

        bpy.ops.render.render(write_still=True)

        image = bpy.data.images[render_context.image, None]
        item = getattr(bpy.data, item_container)[item_name, None]
        image.reload()
        item.preview.image_size = (RENDER_PREVIEW_SIZE, RENDER_PREVIEW_SIZE)
        item.preview.image_pixels_float[:] = image.pixels

    # And now, main code!
    do_save = True

    if do_data_intern:
        bpy.ops.wm.previews_clear(id_type=INTERN_PREVIEW_TYPES)
        bpy.ops.wm.previews_ensure()

    render_contexts = {}

    objects_ignored = set()
    groups_ignored = set()

    prev_scenename = bpy.context.screen.scene.name

    if do_objects:
        prev_shown = {ob.name: ob.hide_render for ob in ids_nolib(bpy.data.objects)}
        for ob in ids_nolib(bpy.data.objects):
            if ob in objects_ignored:
                continue
            ob.hide_render = True
        for root in ids_nolib(bpy.data.objects):
            if root.name in objects_ignored:
                continue
            if root.type not in OBJECT_TYPES_RENDER:
                continue
            objects = ((root.name, None),)

            render_engine = objects_render_engine_guess(objects)
            render_context = render_contexts.get(render_engine, None)
            if render_context is None:
                render_context = render_context_create(render_engine, objects_ignored)
                render_contexts[render_engine] = render_context

            scene = bpy.data.scenes[render_context.scene, None]
            bpy.context.screen.scene = scene

            for obname, libpath in objects:
                ob = bpy.data.objects[obname, libpath]
                if obname not in scene.objects:
                    scene.objects.link(ob)
                ob.hide_render = False
            scene.update()

            preview_render_do(render_context, 'objects', root.name, objects)

            # XXX Hyper Super Uber Suspicious Hack!
            #     Without this, on windows build, script excepts with following message:
            #         Traceback (most recent call last):
            #         File "<string>", line 1, in <module>
            #         File "<string>", line 451, in <module>
            #         File "<string>", line 443, in main
            #         File "<string>", line 327, in do_previews
            #         OverflowError: Python int too large to convert to C long
            #    ... :(
            scene = bpy.data.scenes[render_context.scene, None]
            for obname, libpath in objects:
                ob = bpy.data.objects[obname, libpath]
                scene.objects.unlink(ob)
                ob.hide_render = True

        for ob in ids_nolib(bpy.data.objects):
            is_rendered = prev_shown.get(ob.name, ...)
            if is_rendered is not ...:
                ob.hide_render = is_rendered

    if do_groups:
        for grp in ids_nolib(bpy.data.groups):
            if grp.name in groups_ignored:
                continue
            # Here too, we do want to keep linked objects members of local group...
            objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in grp.objects)

            render_engine = objects_render_engine_guess(objects)
            render_context = render_contexts.get(render_engine, None)
            if render_context is None:
                render_context = render_context_create(render_engine, objects_ignored)
                render_contexts[render_engine] = render_context

            scene = bpy.data.scenes[render_context.scene, None]
            bpy.context.screen.scene = scene

            bpy.ops.object.group_instance_add(group=grp.name)
            grp_ob = next((ob for ob in scene.objects if ob.dupli_group and ob.dupli_group.name == grp.name))
            grp_obname = grp_ob.name
            scene.update()

            offset_matrix = Matrix.Translation(grp.dupli_offset).inverted()

            preview_render_do(render_context, 'groups', grp.name, objects, offset_matrix)

            scene = bpy.data.scenes[render_context.scene, None]
            scene.objects.unlink(bpy.data.objects[grp_obname, None])

    bpy.context.screen.scene = bpy.data.scenes[prev_scenename, None]
    for render_context in render_contexts.values():
        if not render_context_delete(render_context):
            do_save = False  # Do not save file if something went wrong here, we could 'pollute' it with temp data...

    if do_scenes:
        for scene in ids_nolib(bpy.data.scenes):
            has_camera = scene.camera is not None
            bpy.context.screen.scene = scene
            render_context = render_context_create('__SCENE', objects_ignored)
            scene.update()

            objects = None
            if not has_camera:
                # We had to add a temp camera, now we need to place it to see interesting objects!
                objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in scene.objects
                                        if (not ob.hide_render) and (ob.type in OBJECT_TYPES_RENDER))

            preview_render_do(render_context, 'scenes', scene.name, objects)

            if not render_context_delete(render_context):
                do_save = False

    bpy.context.screen.scene = bpy.data.scenes[prev_scenename, None]
    if do_save:
        print("Saving %s..." % bpy.data.filepath)
        try:
            bpy.ops.wm.save_mainfile()
        except Exception as e:
            # Might fail in some odd cases, like e.g. in regression files we have glsl/ram_glsl.blend which
            # references an inexistent texture... Better not break in this case, just spit error to console.
            print("ERROR:", e)
    else:
        print("*NOT* Saving %s, because some error(s) happened while deleting temp render data..." % bpy.data.filepath)


def do_clear_previews(do_objects, do_groups, do_scenes, do_data_intern):
    if do_data_intern:
        bpy.ops.wm.previews_clear(id_type=INTERN_PREVIEW_TYPES)

    if do_objects:
        for ob in ids_nolib(bpy.data.objects):
            ob.preview.image_size = (0, 0)

    if do_groups:
        for grp in ids_nolib(bpy.data.groups):
            grp.preview.image_size = (0, 0)

    if do_scenes:
        for scene in ids_nolib(bpy.data.scenes):
            scene.preview.image_size = (0, 0)

    print("Saving %s..." % bpy.data.filepath)
    bpy.ops.wm.save_mainfile()


def main():
    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    import sys
    import argparse

    # Get rid of Blender args!
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    parser = argparse.ArgumentParser(description="Use Blender to generate previews for currently open Blender file's items.")
    parser.add_argument('--clear', default=False, action="store_true",
                        help="Clear previews instead of generating them.")
    parser.add_argument('--no_backups', default=False, action="store_true",
                        help="Do not generate a backup .blend1 file when saving processed ones.")
    parser.add_argument('--no_scenes', default=True, action="store_false",
                        help="Do not generate/clear previews for scene IDs.")
    parser.add_argument('--no_groups', default=True, action="store_false",
                        help="Do not generate/clear previews for group IDs.")
    parser.add_argument('--no_objects', default=True, action="store_false",
                        help="Do not generate/clear previews for object IDs.")
    parser.add_argument('--no_data_intern', default=True, action="store_false",
                        help="Do not generate/clear previews for mat/tex/image/etc. IDs (those handled by core Blender code).")
    args = parser.parse_args(argv)

    orig_save_version = bpy.context.user_preferences.filepaths.save_version
    if args.no_backups:
        bpy.context.user_preferences.filepaths.save_version = 0
    elif orig_save_version < 1:
        bpy.context.user_preferences.filepaths.save_version = 1

    if args.clear:
        print("clear!")
        do_clear_previews(do_objects=args.no_objects, do_groups=args.no_groups, do_scenes=args.no_scenes,
                          do_data_intern=args.no_data_intern)
    else:
        print("render!")
        do_previews(do_objects=args.no_objects, do_groups=args.no_groups, do_scenes=args.no_scenes,
                    do_data_intern=args.no_data_intern)

    # Not really necessary, but better be consistent.
    bpy.context.user_preferences.filepaths.save_version = orig_save_version


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    print(" *** Blend file {} *** \n".format(bpy.data.filepath))
    main()
    bpy.ops.wm.quit_blender()
