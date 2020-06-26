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
from mathutils import (
    Euler,
    Matrix,
    Vector,
)


OBJECT_TYPES_RENDER = {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'}


def ids_nolib(bids):
    return (bid for bid in bids if not bid.library)


def rna_backup_gen(data, include_props=None, exclude_props=None, root=()):
    # only writable properties...
    for p in data.bl_rna.properties:
        pid = p.identifier
        if pid == "rna_type" or pid == "original":
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


def do_previews(do_objects, do_collections, do_scenes, do_data_intern):
    import collections

    # Helpers.
    RenderContext = collections.namedtuple("RenderContext", (
        "scene", "world", "camera", "light", "camera_data", "light_data", "image",  # All those are names!
        "backup_scene", "backup_world", "backup_camera", "backup_light", "backup_camera_data", "backup_light_data",
    ))

    RENDER_PREVIEW_SIZE = bpy.app.render_preview_size

    def render_context_create(engine, objects_ignored):
        if engine == '__SCENE':
            backup_scene, backup_world, backup_camera, backup_light, backup_camera_data, backup_light_data = [()] * 6
            scene = bpy.context.window.scene
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
                scene.collection.objects.link(camera)
            # TODO: add light if none found in scene?
            light = None
            light_data = None
        else:
            backup_scene, backup_world, backup_camera, backup_light, backup_camera_data, backup_light_data = [None] * 6

            scene = bpy.data.scenes.new("TEMP_preview_render_scene")
            world = bpy.data.worlds.new("TEMP_preview_render_world")
            camera_data = bpy.data.cameras.new("TEMP_preview_render_camera")
            camera = bpy.data.objects.new("TEMP_preview_render_camera", camera_data)
            light_data = bpy.data.lights.new("TEMP_preview_render_light", 'SPOT')
            light = bpy.data.objects.new("TEMP_preview_render_light", light_data)

            objects_ignored.add((camera.name, light.name))

            scene.world = world

            camera.rotation_euler = Euler((1.1635528802871704, 0.0, 0.7853981852531433), 'XYZ')  # (66.67, 0.0, 45.0)
            scene.camera = camera
            scene.collection.objects.link(camera)

            light.rotation_euler = Euler((0.7853981852531433, 0.0, 1.7453292608261108), 'XYZ')  # (45.0, 0.0, 100.0)
            light_data.falloff_type = 'CONSTANT'
            light_data.spot_size = 1.0471975803375244  # 60
            scene.collection.objects.link(light)

            scene.render.engine = 'CYCLES'
            scene.render.film_transparent = True
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
            scene.name, world.name if world else None, camera.name, light.name if light else None,
            camera_data.name, light_data.name if light_data else None, image.name,
            backup_scene, backup_world, backup_camera, backup_light, backup_camera_data, backup_light_data,
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
                    scene.collection.objects.unlink(bpy.data.objects[render_context.camera, None])
                if render_context.light:
                    scene.collection.objects.unlink(bpy.data.objects[render_context.light, None])
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
                        scene.collection.objects.unlink(camera)
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

        if render_context.light:
            try:
                light = bpy.data.objects[render_context.light, None]
                if render_context.backup_light is None:
                    if scene is not None:
                        scene.collection.objects.unlink(light)
                    light.user_clear()
                    bpy.data.objects.remove(light)
                    bpy.data.lights.remove(bpy.data.lights[render_context.light_data, None])
                else:
                    rna_backup_restore(light, render_context.backup_light)
                    rna_backup_restore(bpy.data.lights[render_context.light_data, None], render_context.backup_light_data)
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

    def object_bbox_merge(bbox, ob, ob_space, offset_matrix):
        # Take collections instances into account (including linked one in this case).
        if ob.type == 'EMPTY' and ob.instance_type == 'COLLECTION':
            grp_objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in ob.instance_collection.all_objects)
            if (len(grp_objects) == 0):
                ob_bbox = ob.bound_box
            else:
                coords = objects_bbox_calc(ob_space, grp_objects,
                                           Matrix.Translation(ob.instance_collection.instance_offset).inverted())
                ob_bbox = ((coords[0], coords[1], coords[2]), (coords[21], coords[22], coords[23]))
        elif ob.bound_box:
            ob_bbox = ob.bound_box
        else:
            ob_bbox = ((-ob.scale.x, -ob.scale.y, -ob.scale.z), (ob.scale.x, ob.scale.y, ob.scale.z))

        for v in ob_bbox:
            v = offset_matrix @ Vector(v) if offset_matrix is not None else Vector(v)
            v = ob_space.matrix_world.inverted() @ ob.matrix_world @ v
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
        bbox[0][:] = camera.matrix_world @ bbox[0]
        bbox[1][:] = camera.matrix_world @ bbox[1]
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
            light = bpy.data.objects[render_context.light, None] if render_context.light is not None else None
            cos = objects_bbox_calc(camera, objects, offset_matrix)
            depsgraph = bpy.context.evaluated_depsgraph_get()
            loc, _ortho_scale = camera.camera_fit_coords(depsgraph, cos)
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
            if light:
                loc, _ortho_scale = light.camera_fit_coords(depsgraph, cos)
                light.location = loc
        bpy.context.view_layer.update()

        bpy.ops.render.render(write_still=True)

        image = bpy.data.images[render_context.image, None]
        item = getattr(bpy.data, item_container)[item_name, None]
        image.reload()
        item.preview.image_size = (RENDER_PREVIEW_SIZE, RENDER_PREVIEW_SIZE)
        item.preview.image_pixels_float[:] = image.pixels

    # And now, main code!
    do_save = True

    if do_data_intern:
        bpy.ops.wm.previews_clear(id_type='SHADING')
        bpy.ops.wm.previews_ensure()

    render_contexts = {}

    objects_ignored = set()
    collections_ignored = set()

    prev_scenename = bpy.context.window.scene.name

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

            render_context = render_contexts.get('CYCLES', None)
            if render_context is None:
                render_context = render_context_create('CYCLES', objects_ignored)
                render_contexts['CYCLES'] = render_context

            scene = bpy.data.scenes[render_context.scene, None]
            bpy.context.window.scene = scene

            for obname, libpath in objects:
                ob = bpy.data.objects[obname, libpath]
                if obname not in scene.objects:
                    scene.collection.objects.link(ob)
                ob.hide_render = False
            bpy.context.view_layer.update()

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
                scene.collection.objects.unlink(ob)
                ob.hide_render = True

        for ob in ids_nolib(bpy.data.objects):
            is_rendered = prev_shown.get(ob.name, ...)
            if is_rendered is not ...:
                ob.hide_render = is_rendered

    if do_collections:
        for grp in ids_nolib(bpy.data.collections):
            if grp.name in collections_ignored:
                continue
            # Here too, we do want to keep linked objects members of local collection...
            objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in grp.objects)

            render_context = render_contexts.get('CYCLES', None)
            if render_context is None:
                render_context = render_context_create('CYCLES', objects_ignored)
                render_contexts['CYCLES'] = render_context

            scene = bpy.data.scenes[render_context.scene, None]
            bpy.context.window.scene = scene

            bpy.ops.object.collection_instance_add(collection=grp.name)
            grp_ob = next((ob for ob in scene.objects if ob.instance_collection and ob.instance_collection.name == grp.name))
            grp_obname = grp_ob.name
            bpy.context.view_layer.update()

            offset_matrix = Matrix.Translation(grp.instance_offset).inverted()

            preview_render_do(render_context, 'collections', grp.name, objects, offset_matrix)

            scene = bpy.data.scenes[render_context.scene, None]
            scene.collection.objects.unlink(bpy.data.objects[grp_obname, None])

    bpy.context.window.scene = bpy.data.scenes[prev_scenename, None]
    for render_context in render_contexts.values():
        if not render_context_delete(render_context):
            do_save = False  # Do not save file if something went wrong here, we could 'pollute' it with temp data...

    if do_scenes:
        for scene in ids_nolib(bpy.data.scenes):
            has_camera = scene.camera is not None
            bpy.context.window.scene = scene
            render_context = render_context_create('__SCENE', objects_ignored)
            bpy.context.view_layer.update()

            objects = None
            if not has_camera:
                # We had to add a temp camera, now we need to place it to see interesting objects!
                objects = tuple((ob.name, ob.library.filepath if ob.library else None) for ob in scene.objects
                                if (not ob.hide_render) and (ob.type in OBJECT_TYPES_RENDER))

            preview_render_do(render_context, 'scenes', scene.name, objects)

            if not render_context_delete(render_context):
                do_save = False

    bpy.context.window.scene = bpy.data.scenes[prev_scenename, None]
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


def do_clear_previews(do_objects, do_collections, do_scenes, do_data_intern):
    if do_data_intern:
        bpy.ops.wm.previews_clear(id_type='SHADING')

    if do_objects:
        for ob in ids_nolib(bpy.data.objects):
            ob.preview.image_size = (0, 0)

    if do_collections:
        for grp in ids_nolib(bpy.data.collections):
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
    parser.add_argument('--no_collections', default=True, action="store_false",
                        help="Do not generate/clear previews for collection IDs.")
    parser.add_argument('--no_objects', default=True, action="store_false",
                        help="Do not generate/clear previews for object IDs.")
    parser.add_argument('--no_data_intern', default=True, action="store_false",
                        help="Do not generate/clear previews for mat/tex/image/etc. IDs (those handled by core Blender code).")
    args = parser.parse_args(argv)

    orig_save_version = bpy.context.preferences.filepaths.save_version
    if args.no_backups:
        bpy.context.preferences.filepaths.save_version = 0
    elif orig_save_version < 1:
        bpy.context.preferences.filepaths.save_version = 1

    if args.clear:
        print("clear!")
        do_clear_previews(do_objects=args.no_objects, do_collections=args.no_collections, do_scenes=args.no_scenes,
                          do_data_intern=args.no_data_intern)
    else:
        print("render!")
        do_previews(do_objects=args.no_objects, do_collections=args.no_collections, do_scenes=args.no_scenes,
                    do_data_intern=args.no_data_intern)

    # Not really necessary, but better be consistent.
    bpy.context.preferences.filepaths.save_version = orig_save_version


if __name__ == "__main__":
    print("\n\n *** Running %s *** \n" % __file__)
    print(" *** Blend file %s *** \n" % bpy.data.filepath)
    main()
    bpy.ops.wm.quit_blender()
