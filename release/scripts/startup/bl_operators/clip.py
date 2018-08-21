# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
import os
from bpy.types import Operator
from bpy.props import FloatProperty
from mathutils import (
    Vector,
    Matrix,
)


def CLIP_spaces_walk(context, all_screens, tarea, tspace, callback, *args):
    screens = bpy.data.screens if all_screens else [context.screen]

    for screen in screens:
        for area in screen.areas:
            if area.type == tarea:
                for space in area.spaces:
                    if space.type == tspace:
                        callback(space, *args)


def CLIP_set_viewport_background(context, clip, clip_user):
    def set_background(cam, clip, user):
        bgpic = None

        for x in cam.background_images:
            if x.source == 'MOVIE_CLIP':
                bgpic = x
                break

        if not bgpic:
            bgpic = cam.background_images.new()

        bgpic.source = 'MOVIE_CLIP'
        bgpic.clip = clip
        bgpic.clip_user.proxy_render_size = user.proxy_render_size
        bgpic.clip_user.use_render_undistorted = True
        bgpic.use_camera_clip = False

        cam.show_background_images = True

    scene_camera = context.scene.camera
    if (not scene_camera) or (scene_camera.type != 'CAMERA'):
        return
    set_background(scene_camera.data, clip, clip_user)


def CLIP_camera_for_clip(context, clip):
    scene = context.scene
    camera = scene.camera

    for ob in scene.objects:
        if ob.type == 'CAMERA':
            for con in ob.constraints:
                if con.type == 'CAMERA_SOLVER':
                    cur_clip = scene.active_clip if con.use_active_clip else con.clip

                    if cur_clip == clip:
                        return ob

    return camera


def CLIP_track_view_selected(sc, track):
    if track.select_anchor:
        return True

    if sc.show_marker_pattern and track.select_pattern:
        return True

    if sc.show_marker_search and track.select_search:
        return True

    return False


def CLIP_default_settings_from_track(clip, track, framenr):
    settings = clip.tracking.settings

    width = clip.size[0]
    height = clip.size[1]

    marker = track.markers.find_frame(framenr, False)
    pattern_bb = marker.pattern_bound_box

    pattern = Vector(pattern_bb[1]) - Vector(pattern_bb[0])
    search = marker.search_max - marker.search_min

    pattern[0] = pattern[0] * width
    pattern[1] = pattern[1] * height

    search[0] = search[0] * width
    search[1] = search[1] * height

    settings.default_correlation_min = track.correlation_min
    settings.default_pattern_size = max(pattern[0], pattern[1])
    settings.default_search_size = max(search[0], search[1])
    settings.default_frames_limit = track.frames_limit
    settings.default_pattern_match = track.pattern_match
    settings.default_margin = track.margin
    settings.default_motion_model = track.motion_model
    settings.use_default_brute = track.use_brute
    settings.use_default_normalization = track.use_normalization
    settings.use_default_mask = track.use_mask
    settings.use_default_red_channel = track.use_red_channel
    settings.use_default_green_channel = track.use_green_channel
    settings.use_default_blue_channel = track.use_blue_channel
    settings.default_weight = track.weight


class CLIP_OT_filter_tracks(bpy.types.Operator):
    """Filter tracks which has weirdly looking spikes in motion curves"""
    bl_label = "Filter Tracks"
    bl_idname = "clip.filter_tracks"
    bl_options = {'UNDO', 'REGISTER'}

    track_threshold: FloatProperty(
        name="Track Threshold",
        description="Filter Threshold to select problematic tracks",
        default=5.0,
    )

    @staticmethod
    def _filter_values(context, threshold):

        def get_marker_coordinates_in_pixels(clip_size, track, frame_number):
            marker = track.markers.find_frame(frame_number)
            return Vector((marker.co[0] * clip_size[0], marker.co[1] * clip_size[1]))

        def marker_velocity(clip_size, track, frame):
            marker_a = get_marker_coordinates_in_pixels(clip_size, track, frame)
            marker_b = get_marker_coordinates_in_pixels(clip_size, track, frame - 1)
            return marker_a - marker_b

        scene = context.scene
        frame_start = scene.frame_start
        frame_end = scene.frame_end
        clip = context.space_data.clip
        clip_size = clip.size[:]

        bpy.ops.clip.clean_tracks(frames=10, action='DELETE_TRACK')

        tracks_to_clean = set()

        for frame in range(frame_start, frame_end + 1):

            # Find tracks with markers in both this frame and the previous one.
            relevant_tracks = [
                track for track in clip.tracking.tracks
                if (track.markers.find_frame(frame) and
                    track.markers.find_frame(frame - 1))
            ]

            if not relevant_tracks:
                continue

            # Get average velocity and deselect track.
            average_velocity = Vector((0.0, 0.0))
            for track in relevant_tracks:
                track.select = False
                average_velocity += marker_velocity(clip_size, track, frame)
            if len(relevant_tracks) >= 1:
                average_velocity = average_velocity / len(relevant_tracks)

            # Then find all markers that behave differently than the average.
            for track in relevant_tracks:
                track_velocity = marker_velocity(clip_size, track, frame)
                distance = (average_velocity - track_velocity).length

                if distance > threshold:
                    tracks_to_clean.add(track)

        for track in tracks_to_clean:
            track.select = True
        return len(tracks_to_clean)

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return (space.type == 'CLIP_EDITOR') and space.clip

    def execute(self, context):
        num_tracks = self._filter_values(context, self.track_threshold)
        self.report({'INFO'}, "Identified %d problematic tracks" % num_tracks)
        return {'FINISHED'}


class CLIP_OT_set_active_clip(bpy.types.Operator):
    bl_label = "Set Active Clip"
    bl_idname = "clip.set_active_clip"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.type == 'CLIP_EDITOR' and space.clip

    def execute(self, context):
        clip = context.space_data.clip
        scene = context.scene
        scene.active_clip = clip
        scene.render.resolution_x = clip.size[0]
        scene.render.resolution_y = clip.size[1]
        return {'FINISHED'}


class CLIP_OT_track_to_empty(Operator):
    """Create an Empty object which will be copying movement of active track"""

    bl_idname = "clip.track_to_empty"
    bl_label = "Link Empty to Track"
    bl_options = {'UNDO', 'REGISTER'}

    @staticmethod
    def _link_track(context, clip, tracking_object, track):
        sc = context.space_data
        constraint = None
        ob = None

        ob = bpy.data.objects.new(name=track.name, object_data=None)
        context.collection.objects.link(ob)
        ob.select_set(action='SELECT')
        context.view_layer.objects.active = ob

        for con in ob.constraints:
            if con.type == 'FOLLOW_TRACK':
                constraint = con
                break

        if constraint is None:
            constraint = ob.constraints.new(type='FOLLOW_TRACK')

        constraint.use_active_clip = False
        constraint.clip = sc.clip
        constraint.track = track.name
        constraint.use_3d_position = False
        constraint.object = tracking_object.name
        constraint.camera = CLIP_camera_for_clip(context, clip)

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.type == 'CLIP_EDITOR' and space.clip

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip
        tracking_object = clip.tracking.objects.active

        for track in tracking_object.tracks:
            if CLIP_track_view_selected(sc, track):
                self._link_track(context, clip, tracking_object, track)

        return {'FINISHED'}


class CLIP_OT_bundles_to_mesh(Operator):
    """Create vertex cloud using coordinates of reconstructed tracks"""

    bl_idname = "clip.bundles_to_mesh"
    bl_label = "3D Markers to Mesh"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        return (sc.type == 'CLIP_EDITOR') and sc.clip

    def execute(self, context):
        from bpy_extras.io_utils import unpack_list

        sc = context.space_data
        clip = sc.clip
        tracking_object = clip.tracking.objects.active

        new_verts = []

        scene = context.scene
        camera = scene.camera
        matrix = Matrix.Identity(4)
        if camera:
            reconstruction = tracking_object.reconstruction
            framenr = scene.frame_current - clip.frame_start + 1
            reconstructed_matrix = reconstruction.cameras.matrix_from_frame(framenr)
            matrix = camera.matrix_world @ reconstructed_matrix.inverted()

        for track in tracking_object.tracks:
            if track.has_bundle and track.select:
                new_verts.append(track.bundle)

        if new_verts:
            mesh = bpy.data.meshes.new(name="Tracks")
            mesh.vertices.add(len(new_verts))
            mesh.vertices.foreach_set("co", unpack_list(new_verts))
            ob = bpy.data.objects.new(name="Tracks", object_data=mesh)
            ob.matrix_world = matrix
            context.collection.objects.link(ob)
            ob.select_set('SELECT')
            context.view_layer.objects.active = ob
        else:
            self.report({'WARNING'}, "No usable tracks selected")

        return {'FINISHED'}


class CLIP_OT_delete_proxy(Operator):
    """Delete movie clip proxy files from the hard drive"""

    bl_idname = "clip.delete_proxy"
    bl_label = "Delete Proxy"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data

        return sc.clip

    def invoke(self, context, event):
        wm = context.window_manager

        return wm.invoke_confirm(self, event)

    @staticmethod
    def _rmproxy(abspath):
        import shutil

        if not os.path.exists(abspath):
            return

        if os.path.isdir(abspath):
            shutil.rmtree(abspath)
        else:
            os.remove(abspath)

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip
        if clip.use_proxy_custom_directory:
            proxydir = clip.proxy.directory
        else:
            clipdir = os.path.dirname(clip.filepath)
            proxydir = os.path.join(clipdir, "BL_proxy")

        clipfile = os.path.basename(clip.filepath)
        proxy = os.path.join(proxydir, clipfile)
        absproxy = bpy.path.abspath(proxy)

        # proxy_<quality>[_undistorted]
        for x in (25, 50, 75, 100):
            d = os.path.join(absproxy, "proxy_%d" % x)

            self._rmproxy(d)
            self._rmproxy(d + "_undistorted")
            self._rmproxy(os.path.join(absproxy, "proxy_%d.avi" % x))

        tc = ("free_run.blen_tc",
              "interp_free_run.blen_tc",
              "record_run.blen_tc")

        for x in tc:
            self._rmproxy(os.path.join(absproxy, x))

        # Remove proxy per-clip directory.
        try:
            os.rmdir(absproxy)
        except OSError:
            pass

        # Remove [custom] proxy directory if empty.
        try:
            absdir = bpy.path.abspath(proxydir)
            os.rmdir(absdir)
        except OSError:
            pass

        return {'FINISHED'}


class CLIP_OT_set_viewport_background(Operator):
    """Set current movie clip as a camera background in 3D view-port """ \
        """(works only when a 3D view-port is visible)"""

    bl_idname = "clip.set_viewport_background"
    bl_label = "Set as Background"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data

        return sc.clip

    def execute(self, context):
        sc = context.space_data
        CLIP_set_viewport_background(context, sc.clip, sc.clip_user)

        return {'FINISHED'}


class CLIP_OT_constraint_to_fcurve(Operator):
    """Create F-Curves for object which will copy """ \
        """object's movement caused by this constraint"""

    bl_idname = "clip.constraint_to_fcurve"
    bl_label = "Constraint to F-Curve"
    bl_options = {'UNDO', 'REGISTER'}

    def _bake_object(self, scene, ob):
        con = None
        clip = None
        sfra = None
        efra = None
        frame_current = scene.frame_current
        matrices = []

        # Find constraint which would be converting
        # TODO: several camera solvers and track followers would fail,
        #       but can't think about real work-flow where it'll be useful
        for x in ob.constraints:
            if x.type in {'CAMERA_SOLVER', 'FOLLOW_TRACK', 'OBJECT_SOLVER'}:
                con = x

        if not con:
            self.report({'ERROR'},
                        "Motion Tracking constraint to be converted not found")

            return {'CANCELLED'}

        # Get clip used for parenting.
        if con.use_active_clip:
            clip = scene.active_clip
        else:
            clip = con.clip

        if not clip:
            self.report({'ERROR'},
                        "Movie clip to use tracking data from isn't set")

            return {'CANCELLED'}

        if con.type == 'FOLLOW_TRACK' and con.use_3d_position:
            mat = ob.matrix_world.copy()
            ob.constraints.remove(con)
            ob.matrix_world = mat

            return {'FINISHED'}

        # Find start and end frames.
        for track in clip.tracking.tracks:
            if sfra is None:
                sfra = track.markers[0].frame
            else:
                sfra = min(sfra, track.markers[0].frame)

            if efra is None:
                efra = track.markers[-1].frame
            else:
                efra = max(efra, track.markers[-1].frame)

        if sfra is None or efra is None:
            return

        # Store object matrices.
        for x in range(sfra, efra + 1):
            scene.frame_set(x)
            matrices.append(ob.matrix_world.copy())

        ob.animation_data_create()

        # Apply matrices on object and insert key-frames.
        i = 0
        for x in range(sfra, efra + 1):
            scene.frame_set(x)
            ob.matrix_world = matrices[i]

            ob.keyframe_insert("location")

            if ob.rotation_mode == 'QUATERNION':
                ob.keyframe_insert("rotation_quaternion")
            else:
                ob.keyframe_insert("rotation_euler")

            i += 1

        ob.constraints.remove(con)

        scene.frame_set(frame_current)

    def execute(self, context):
        scene = context.scene
        # XXX, should probably use context.selected_editable_objects
        # since selected objects can be from a lib or in hidden layer!
        for ob in scene.objects:
            if ob.select_set(action='SELECT'):
                self._bake_object(scene, ob)

        return {'FINISHED'}


class CLIP_OT_setup_tracking_scene(Operator):
    """Prepare scene for compositing 3D objects into this footage"""
    # TODO: it will be great to integrate with other engines (other than Cycles)

    bl_idname = "clip.setup_tracking_scene"
    bl_label = "Setup Tracking Scene"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        if sc.type != 'CLIP_EDITOR':
            return False

        clip = sc.clip

        return clip and clip.tracking.reconstruction.is_valid

    @staticmethod
    def _setupScene(context):
        scene = context.scene
        scene.active_clip = context.space_data.clip
        scene.render.use_motion_blur = True

    @staticmethod
    def _setupWorld(context):
        scene = context.scene
        world = scene.world

        if not world:
            world = bpy.data.worlds.new(name="World")
            scene.world = world

        # Having AO enabled is nice for shadow catcher.
        world.light_settings.use_ambient_occlusion = True
        world.light_settings.distance = 1.0
        if hasattr(scene, "cycles"):
            world.light_settings.ao_factor = 0.05

    @staticmethod
    def _findOrCreateCamera(context):
        scene = context.scene

        if scene.camera:
            return scene.camera

        cam = bpy.data.cameras.new(name="Camera")
        camob = bpy.data.objects.new(name="Camera", object_data=cam)
        scene.collection.objects.link(camob)

        scene.camera = camob

        camob.matrix_local = (
            Matrix.Translation((7.481, -6.508, 5.344)) @
            Matrix.Rotation(0.815, 4, 'Z') @
            Matrix.Rotation(0.011, 4, 'Y') @
            Matrix.Rotation(1.109, 4, 'X')
        )

        return camob

    @staticmethod
    def _setupCamera(context):
        sc = context.space_data
        clip = sc.clip
        tracking = clip.tracking

        camob = CLIP_OT_setup_tracking_scene._findOrCreateCamera(context)
        cam = camob.data

        # Remove all constraints to be sure motion is fine.
        camob.constraints.clear()

        # Append camera solver constraint.
        con = camob.constraints.new(type='CAMERA_SOLVER')
        con.use_active_clip = True
        con.influence = 1.0

        cam.sensor_width = tracking.camera.sensor_width
        cam.lens = tracking.camera.focal_length

    @staticmethod
    def _setupViewport(context):
        sc = context.space_data
        CLIP_set_viewport_background(context, sc.clip, sc.clip_user)

    @staticmethod
    def _setupViewLayers(context):
        scene = context.scene
        view_layers = scene.view_layers

        if not view_layers.get("Foreground"):
            if len(view_layers) == 1:
                fg = view_layers[0]
                fg.name = 'Foreground'
            else:
                fg = view_layers.new("Foreground")

            fg.use_sky = True

        if not view_layers.get("Background"):
            bg = view_layers.new("Background")

    @staticmethod
    def createCollection(context, collection_name):
        def collection_in_collection(collection, collection_to_query):
            """Return true if collection is in any of the children or """
            """grandchildren of collection_to_query"""
            for child in collection_to_query.children:
                if collection == child:
                    return True

                if collection_in_collection(collection, child):
                    return True

        master_collection = context.scene.collection
        collection = bpy.data.collections.get(collection_name)

        if collection and collection.library:
            # We need a local collection instead.
            collection = None

        if not collection:
            collection = bpy.data.collections.new(name=collection_name)
            master_collection.children.link(collection)
        else:
            # see if collection is in the scene
            if not collection_in_collection(collection, master_collection):
                master_collection.children.link(collection)

    def _setupCollections(self, context):
        def setup_collection_recursively(collections, collection_name, attr_name):
            for collection in collections:
                if collection.collection.name == collection_name:
                    setattr(collection, attr_name, True)
                    break
                else:
                    setup_collection_recursively(collection.children, collection_name, attr_name)

        collection = context.collection
        collections = context.scene.collection.children
        vlayers = context.scene.view_layers

        if len(collections) == 1:
            collections[0].name = "foreground"

        self.createCollection(context, "foreground")
        self.createCollection(context, "background")

        # rendersettings
        setup_collection_recursively(
                vlayers["Foreground"].collections[0].children,
                "background",
                "holdout")

        setup_collection_recursively(
                vlayers["Background"].collections[0].children,
                "foreground",
                "indirect_only")

    @staticmethod
    def _wipeDefaultNodes(tree):
        if len(tree.nodes) != 2:
            return False
        types = [node.type for node in tree.nodes]
        types.sort()

        if types[0] == 'COMPOSITE' and types[1] == 'R_LAYERS':
            while tree.nodes:
                tree.nodes.remove(tree.nodes[0])

    @staticmethod
    def _findNode(tree, type):
        for node in tree.nodes:
            if node.type == type:
                return node

        return None

    @staticmethod
    def _findOrCreateNode(tree, type):
        node = CLIP_OT_setup_tracking_scene._findNode(tree, type)

        if not node:
            node = tree.nodes.new(type=type)

        return node

    @staticmethod
    def _needSetupNodes(context):
        scene = context.scene
        tree = scene.node_tree

        if not tree:
            # No compositor node tree found, time to create it!
            return True

        for node in tree.nodes:
            if node.type in {'MOVIECLIP', 'MOVIEDISTORTION'}:
                return False

        return True

    @staticmethod
    def _offsetNodes(tree):
        for a in tree.nodes:
            for b in tree.nodes:
                if a != b and a.location == b.location:
                    b.location += Vector((40.0, 20.0))

    def _setupNodes(self, context):
        if not self._needSetupNodes(context):
            # Compositor nodes were already setup or even changes already
            # do nothing to prevent nodes damage.
            return

        # Enable backdrop for all compositor spaces.
        def setup_space(space):
            space.show_backdrop = True

        CLIP_spaces_walk(context, True, 'NODE_EDITOR', 'NODE_EDITOR',
                         setup_space)

        sc = context.space_data
        scene = context.scene
        scene.use_nodes = True
        tree = scene.node_tree
        clip = sc.clip

        need_stabilization = False

        # Remove all the nodes if they came from default node setup.
        # This is simplest way to make it so final node setup is correct.
        self._wipeDefaultNodes(tree)

        # Create nodes.
        rlayer_fg = self._findOrCreateNode(tree, 'CompositorNodeRLayers')
        rlayer_bg = tree.nodes.new(type='CompositorNodeRLayers')
        composite = self._findOrCreateNode(tree, 'CompositorNodeComposite')

        movieclip = tree.nodes.new(type='CompositorNodeMovieClip')
        distortion = tree.nodes.new(type='CompositorNodeMovieDistortion')

        if need_stabilization:
            stabilize = tree.nodes.new(type='CompositorNodeStabilize2D')

        scale = tree.nodes.new(type='CompositorNodeScale')
        shadowcatcher = tree.nodes.new(type='CompositorNodeAlphaOver')
        alphaover = tree.nodes.new(type='CompositorNodeAlphaOver')
        viewer = tree.nodes.new(type='CompositorNodeViewer')

        # Setup nodes.
        movieclip.clip = clip

        distortion.clip = clip
        distortion.distortion_type = 'UNDISTORT'

        if need_stabilization:
            stabilize.clip = clip

        scale.space = 'RENDER_SIZE'

        rlayer_bg.scene = scene
        rlayer_bg.layer = "Background"

        rlayer_fg.scene = scene
        rlayer_fg.layer = "Foreground"

        # Create links.
        tree.links.new(movieclip.outputs["Image"], distortion.inputs["Image"])

        if need_stabilization:
            tree.links.new(distortion.outputs["Image"],
                           stabilize.inputs["Image"])
            tree.links.new(stabilize.outputs["Image"], scale.inputs["Image"])
        else:
            tree.links.new(distortion.outputs["Image"], scale.inputs["Image"])

        tree.links.new(scale.outputs["Image"], shadowcatcher.inputs[1])

        tree.links.new(rlayer_bg.outputs["Image"], shadowcatcher.inputs[2])

        tree.links.new(rlayer_fg.outputs["Image"], alphaover.inputs[2])

        tree.links.new(shadowcatcher.outputs["Image"], alphaover.inputs[1])

        tree.links.new(alphaover.outputs["Image"], composite.inputs["Image"])
        tree.links.new(alphaover.outputs["Image"], viewer.inputs["Image"])

        # Place nodes.
        movieclip.location = Vector((-300.0, 350.0))

        distortion.location = movieclip.location
        distortion.location += Vector((200.0, 0.0))

        if need_stabilization:
            stabilize.location = distortion.location
            stabilize.location += Vector((200.0, 0.0))

            scale.location = stabilize.location
            scale.location += Vector((200.0, 0.0))
        else:
            scale.location = distortion.location
            scale.location += Vector((200.0, 0.0))

        rlayer_bg.location = movieclip.location
        rlayer_bg.location -= Vector((0.0, 350.0))

        rlayer_fg.location = rlayer_bg.location
        rlayer_fg.location -= Vector((0.0, 500.0))

        shadowcatcher.location = scale.location
        shadowcatcher.location += Vector((250.0, 0.0))

        alphaover.location = shadowcatcher.location
        alphaover.location += Vector((250.0, -250.0))

        composite.location = alphaover.location
        composite.location += Vector((300.0, -100.0))

        viewer.location = composite.location
        composite.location += Vector((0.0, 200.0))

        # Ensure no nodes were created on the position of existing node.
        self._offsetNodes(tree)

        if hasattr(scene, "cycles"):
            scene.cycles.film_transparent = True

    @staticmethod
    def _createMesh(collection, name, vertices, faces):
        from bpy_extras.io_utils import unpack_list

        mesh = bpy.data.meshes.new(name=name)

        mesh.vertices.add(len(vertices))
        mesh.vertices.foreach_set("co", unpack_list(vertices))

        nbr_loops = len(faces)
        nbr_polys = nbr_loops // 4
        mesh.loops.add(nbr_loops)
        mesh.polygons.add(nbr_polys)

        mesh.polygons.foreach_set("loop_start", range(0, nbr_loops, 4))
        mesh.polygons.foreach_set("loop_total", (4,) * nbr_polys)
        mesh.loops.foreach_set("vertex_index", faces)

        mesh.update()

        ob = bpy.data.objects.new(name=name, object_data=mesh)
        collection.objects.link(ob)

        return ob

    @staticmethod
    def _getPlaneVertices(half_size, z):

        return [(-half_size, -half_size, z),
                (half_size, -half_size, z),
                (half_size, half_size, z),
                (-half_size, half_size, z)]

    def _createGround(self, collection):
        vertices = self._getPlaneVertices(4.0, 0.0)
        faces = [0, 1, 2, 3]

        ob = self._createMesh(collection, "Ground", vertices, faces)
        ob["is_ground"] = True

        return ob

    @staticmethod
    def _findGround(context):
        scene = context.scene

        for ob in scene.objects:
            if ob.type == 'MESH' and "is_ground" in ob:
                return ob

        return None

    @staticmethod
    def _createLight(scene):
        light = bpy.data.lights.new(name="Light", type='POINT')
        lightob = bpy.data.objects.new(name="Light", object_data=light)

        lightob.matrix_local = Matrix.Translation((4.076, 1.005, 5.904))

        return lightob

    def _createSampleObject(self, collection):
        vertices = self._getPlaneVertices(1.0, -1.0) + \
            self._getPlaneVertices(1.0, 1.0)
        faces = (0, 1, 2, 3,
                 4, 7, 6, 5,
                 0, 4, 5, 1,
                 1, 5, 6, 2,
                 2, 6, 7, 3,
                 3, 7, 4, 0)

        return self._createMesh(collection, "Cube", vertices, faces)

    def _setupObjects(self, context):

        def setup_shadow_catcher_objects(collection):
            """Make all the newly created and the old objects of a collection """ \
                """to be properly setup for shadow catch"""
            for ob in collection.objects:
                ob.cycles.is_shadow_catcher = True
                for child in collection.children:
                    setup_shadow_catcher_collection(child)

        scene = context.scene
        fg_coll = bpy.data.collections["foreground", None]
        bg_coll = bpy.data.collections["background", None]

        # Ensure all lights are active on foreground and background.
        has_light = False
        has_mesh = False
        for ob in scene.objects:
            if ob.type == 'LIGHT':
                has_light = True
            elif ob.type == 'MESH' and "is_ground" not in ob:
                has_mesh = True

        # Create sample light if there is no lights in the scene.
        if not has_light:
            light = self._createLight(scene)
            fg_coll.objects.link(light)
            bg_coll.objects.link(light)

        # Create sample object if there's no meshes in the scene.
        if not has_mesh:
            ob = self._createSampleObject(fg_coll)

        # Create ground object if needed.
        ground = self._findGround(context)
        if not ground:
            ground = self._createGround(bg_coll)

        # And set everything on background layer to shadow catcher.
        if hasattr(scene, "cycles"):
            setup_shadow_catcher_objects(bg_coll)

    def execute(self, context):
        scene = context.scene
        current_active_layer = scene.active_layer

        self._setupScene(context)
        self._setupWorld(context)
        self._setupCamera(context)
        self._setupViewport(context)
        self._setupViewLayers(context)
        self._setupCollections(context)
        self._setupNodes(context)
        self._setupObjects(context)

        return {'FINISHED'}


class CLIP_OT_track_settings_as_default(Operator):
    """Copy tracking settings from active track to default settings"""

    bl_idname = "clip.track_settings_as_default"
    bl_label = "Track Settings As Default"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        if sc.type != 'CLIP_EDITOR':
            return False

        clip = sc.clip

        return clip and clip.tracking.tracks.active

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip

        track = clip.tracking.tracks.active
        framenr = context.scene.frame_current - clip.frame_start + 1

        CLIP_default_settings_from_track(clip, track, framenr)

        return {'FINISHED'}


class CLIP_OT_track_settings_to_track(bpy.types.Operator):
    """Copy tracking settings from active track to selected tracks"""

    bl_label = "Copy Track Settings"
    bl_idname = "clip.track_settings_to_track"
    bl_options = {'UNDO', 'REGISTER'}

    _attrs_track = (
        "correlation_min",
        "frames_limit",
        "pattern_match",
        "margin",
        "motion_model",
        "use_brute",
        "use_normalization",
        "use_mask",
        "use_red_channel",
        "use_green_channel",
        "use_blue_channel",
        "weight"
    )

    _attrs_marker = (
        "pattern_corners",
        "search_min",
        "search_max",
    )

    @classmethod
    def poll(cls, context):
        space = context.space_data
        if space.type != 'CLIP_EDITOR':
            return False
        clip = space.clip
        return clip and clip.tracking.tracks.active

    def execute(self, context):
        space = context.space_data
        clip = space.clip
        track = clip.tracking.tracks.active

        framenr = context.scene.frame_current - clip.frame_start + 1
        marker = track.markers.find_frame(framenr, False)

        for t in clip.tracking.tracks:
            if t.select and t != track:
                marker_selected = t.markers.find_frame(framenr, False)
                for attr in self._attrs_track:
                    setattr(t, attr, getattr(track, attr))
                for attr in self._attrs_marker:
                    setattr(marker_selected, attr, getattr(marker, attr))

        return {'FINISHED'}


classes = (
    CLIP_OT_bundles_to_mesh,
    CLIP_OT_constraint_to_fcurve,
    CLIP_OT_delete_proxy,
    CLIP_OT_filter_tracks,
    CLIP_OT_set_active_clip,
    CLIP_OT_set_viewport_background,
    CLIP_OT_setup_tracking_scene,
    CLIP_OT_track_settings_as_default,
    CLIP_OT_track_settings_to_track,
    CLIP_OT_track_to_empty,
)
