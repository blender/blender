# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "classes",
)

from collections import namedtuple

import bpy
from bpy.types import (
    Operator,
)
from bpy.props import (
    StringProperty,
    BoolProperty,
    EnumProperty,
    FloatProperty,
    CollectionProperty,
)

from bpy.app.translations import (
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)
from mathutils import Vector


from bpy_extras.object_utils import (
    AddObjectHelper,
    world_to_camera_view,
)

from bpy_extras.io_utils import ImportHelper

# -----------------------------------------------------------------------------
# Constants

COMPATIBLE_ENGINES = {'CYCLES', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

# -----------------------------------------------------------------------------
# Image loading

ImageSpec = namedtuple(
    "ImageSpec", (
        "image",
        "size",
        "frame_start",
        "frame_offset",
        "frame_duration",
    ),
)


def find_image_sequences(files):
    # From a group of files, detect image sequences.
    #
    # This returns a generator of tuples, which contain the filename,
    # start frame, and length of the detected sequence
    #
    # >>> list(find_image_sequences([
    # ...     "test2-001.jp2", "test2-002.jp2",
    # ...     "test3-003.jp2", "test3-004.jp2", "test3-005.jp2", "test3-006.jp2",
    # ...     "blah"]))
    # [("blah", 1, 1), ("test2-001.jp2", 1, 2), ("test3-003.jp2", 3, 4)]

    from itertools import count
    import re
    num_regex = re.compile("[0-9]")  # Find a single number.
    nums_regex = re.compile("[0-9]+")  # Find a set of numbers.

    files = iter(sorted(files))
    prev_file = None
    pattern = ""
    matches = []
    segment = None
    length = 1
    for filename in files:
        new_pattern = num_regex.sub("#", filename)
        new_matches = list(map(int, nums_regex.findall(filename)))
        if new_pattern == pattern:
            # This file looks like it may be in sequence from the previous.

            # If there are multiple sets of numbers, figure out what changed.
            if segment is None:
                for i, prev, cur in zip(count(), matches, new_matches):
                    if prev != cur:
                        segment = i
                        break

            # Did it only change by one?
            for i, prev, cur in zip(count(), matches, new_matches):
                if i == segment:
                    # We expect this to increment.
                    prev = prev + length
                if prev != cur:
                    break

            # All good!
            else:
                length += 1
                continue

        # No continuation -> spit out what we found and reset counters.
        if prev_file:
            if length > 1:
                yield prev_file, matches[segment], length
            else:
                yield prev_file, 1, 1

        prev_file = filename
        matches = new_matches
        pattern = new_pattern
        segment = None
        length = 1

    if prev_file:
        if length > 1:
            yield prev_file, matches[segment], length
        else:
            yield prev_file, 1, 1


def load_images(filenames, directory, force_reload=False, frame_start=1, find_sequences=False):
    # Wrapper for `bpy_extras.image_utils.load_image`.

    # Loads a set of images, movies, or even image sequences
    # Returns a generator of ImageSpec wrapper objects later used for texture setup
    import os
    from itertools import repeat
    from bpy_extras.image_utils import load_image

    if find_sequences:  # If finding sequences, we need some pre-processing first.
        file_iter = find_image_sequences(filenames)
    else:
        file_iter = zip(filenames, repeat(1), repeat(1))

    for filename, offset, frames in file_iter:
        if not os.path.isfile(bpy.path.abspath(os.path.join(directory, filename))):
            continue

        image = load_image(filename, directory, check_existing=True, force_reload=force_reload)

        # Size is unavailable for sequences, so we grab it early.
        size = tuple(image.size)

        if image.source == 'MOVIE':
            # Blender BPY BUG!
            # This number is only valid when read a second time in 2.77
            # This repeated line is not a mistake.
            frames = image.frame_duration
            frames = image.frame_duration

        elif frames > 1:  # Not movie, but multiple frames -> image sequence.
            image.source = 'SEQUENCE'

        yield ImageSpec(image, size, frame_start, offset - 1, frames)


# -----------------------------------------------------------------------------
# Position & Size Helpers

def offset_planes(planes, gap, axis):
    # Offset planes from each other by `gap` amount along a _local_ vector `axis`
    #
    # For example, offset_planes([obj1, obj2], 0.5, Vector(0, 0, 1)) will place
    # obj2 0.5 blender units away from obj1 along the local positive Z axis.
    #
    # This is in local space, not world space, so all planes should share
    # a common scale and rotation.
    prior = planes[0]
    offset = Vector()
    for current in planes[1:]:
        local_offset = abs((prior.dimensions + current.dimensions).dot(axis)) / 2.0 + gap

        offset += local_offset * axis
        current.location = current.matrix_world @ offset

        prior = current


def compute_camera_size(context, center, fill_mode, aspect):
    # Determine how large an object needs to be to fit or fill the camera's field of view.
    scene = context.scene
    camera = scene.camera
    view_frame = camera.data.view_frame(scene=scene)
    frame_size = (
        Vector([max(v[i] for v in view_frame) for i in range(3)]) -
        Vector([min(v[i] for v in view_frame) for i in range(3)])
    )
    camera_aspect = frame_size.x / frame_size.y

    # Convert the frame size to the correct sizing at a given distance.
    if camera.type == 'ORTHO':
        frame_size = frame_size.xy
    else:
        # Perspective transform.
        distance = world_to_camera_view(scene, camera, center).z
        frame_size = distance * frame_size.xy / (-view_frame[0].z)

    # Determine what axis to match to the camera.
    match_axis = 0  # Match the Y axis size.
    match_aspect = aspect
    if (
        (fill_mode == 'FILL' and aspect > camera_aspect) or
        (fill_mode == 'FIT' and aspect < camera_aspect)
    ):
        match_axis = 1  # Match the X axis size.
        match_aspect = 1.0 / aspect

    # Scale the other axis to the correct aspect.
    frame_size[1 - match_axis] = frame_size[match_axis] / match_aspect

    return frame_size


def center_in_camera(camera, ob, axis=(1, 1)):
    # Center object along specified axis of the camera.
    camera_matrix_col = camera.matrix_world.col
    location = ob.location

    # Vector from the camera's world coordinate center to the object's center.
    delta = camera_matrix_col[3].xyz - location

    # How far off center we are along the camera's local X
    camera_x_mag = delta.dot(camera_matrix_col[0].xyz) * axis[0]
    # How far off center we are along the camera's local Y
    camera_y_mag = delta.dot(camera_matrix_col[1].xyz) * axis[1]

    # Now offset only along camera local axis.
    offset = camera_matrix_col[0].xyz * camera_x_mag + camera_matrix_col[1].xyz * camera_y_mag

    ob.location = location + offset


def get_ref_object_space_coord(ob):
    size = ob.empty_display_size
    x, y = ob.empty_image_offset
    img = ob.data

    res_x, res_y = img.size
    scaling = 1.0 / max(res_x, res_y)

    corners = [
        (0.0, 0.0),
        (res_x, 0.0),
        (0.0, res_y),
        (res_x, res_y),
    ]

    obj_space_corners = []
    for co in corners:
        nco_x = ((co[0] + (x * res_x)) * size) * scaling
        nco_y = ((co[1] + (y * res_y)) * size) * scaling
        obj_space_corners.append((nco_x, nco_y, 0))
    return obj_space_corners


# -----------------------------------------------------------------------------
# Cycles/EEVEE utils

class MaterialProperties_MixIn:
    shader: EnumProperty(
        name="Shader",
        items=(
            ('PRINCIPLED', "Principled", "Principled shader"),
            ('SHADELESS', "Shadeless", "Only visible to camera and reflections"),
            ('EMISSION', "Emission", "Emission shader"),
        ),
        default='PRINCIPLED',
        description="Node shader to use",
    )

    emit_strength: FloatProperty(
        name="Emission Strength",
        min=0.0,
        default=1.0,
        soft_max=10.0,
        step=100,
        description="Strength of emission",
    )

    use_transparency: BoolProperty(
        name="Use Alpha",
        default=True,
        description="Use alpha channel for transparency",
    )

    render_method: EnumProperty(
        name="Render Method",
        items=(
            ('DITHERED',
             "Dithered",
             "Allows for grayscale hashed transparency, and compatible with render passes and ray-tracing. "
             "Also known as deferred rendering."),
            ('BLENDED',
             "Blended",
             "Allows for colored transparency, but incompatible with render passes and ray-tracing. "
             "Also known as forward rendering."),
        ),
    )

    use_backface_culling: BoolProperty(
        name="Backface Culling",
        default=False,
        description="Use backface culling to hide the back side of faces",
    )

    show_transparent_back: BoolProperty(
        name="Show Backface",
        default=True,
        description="Render multiple transparent layers (may introduce transparency sorting problems)",
    )

    overwrite_material: BoolProperty(
        name="Overwrite Material",
        default=True,
        description="Overwrite existing material with the same name",
    )

    def draw_material_config(self, context):
        # --- Material / Rendering Properties --- #
        layout = self.layout

        header, body = layout.panel("import_image_plane_material", default_closed=False)
        header.label(text="Material")
        if body:
            body.prop(self, "shader")
            if self.shader == 'EMISSION':
                body.prop(self, "emit_strength")

            body.prop(self, "render_method")
            if self.render_method == 'BLENDED':
                body.prop(self, "show_transparent_back")

            body.prop(self, "use_backface_culling")

            engine = context.scene.render.engine
            if engine not in COMPATIBLE_ENGINES:
                body.label(text=rpt_("{:s} is not supported").format(engine), icon='ERROR')

            body.prop(self, "overwrite_material")


class TextureProperties_MixIn:
    interpolation: EnumProperty(
        name="Interpolation",
        items=(
            ('Linear', "Linear", "Linear interpolation"),
            ('Closest', "Closest", "No interpolation (sample closest texel)"),
            ('Cubic', "Cubic", "Cubic interpolation"),
            ('Smart', "Smart", "Bicubic when magnifying, else bilinear (OSL only)"),
        ),
        default='Linear',
        description="Texture interpolation",
    )

    extension: EnumProperty(
        name="Extension",
        translation_context=i18n_contexts.id_image,
        items=(
            ('CLIP', "Clip", "Clip to image size and set exterior pixels as transparent"),
            ('EXTEND', "Extend", "Extend by repeating edge pixels of the image"),
            ('REPEAT', "Repeat", "Cause the image to repeat horizontally and vertically"),
        ),
        default='CLIP',
        description="How the image is extrapolated past its original bounds",
    )

    _ImageUser_use_auto_refresh = bpy.types.ImageUser.bl_rna.properties["use_auto_refresh"]
    use_auto_refresh: BoolProperty(
        name=_ImageUser_use_auto_refresh.name,
        default=True,
        description=_ImageUser_use_auto_refresh.description,
    )

    relative: BoolProperty(
        name="Relative Paths",
        default=True,
        description="Use relative file paths",
    )

    def draw_texture_config(self, context):
        # --- Texture Properties --- #
        layout = self.layout

        header, body = layout.panel("import_image_plane_texture", default_closed=False)
        header.label(text="Texture")
        if body:
            body.prop(self, "interpolation")
            body.prop(self, "extension")

            row = body.row(align=False, heading="Alpha")
            row.prop(self, "use_transparency", text="")

            body.prop(self, "use_auto_refresh")


def apply_texture_options(self, texture, img_spec):
    # Shared by both Cycles and Blender Internal.
    image_user = texture.image_user
    image_user.use_auto_refresh = self.use_auto_refresh
    image_user.frame_start = img_spec.frame_start
    image_user.frame_offset = img_spec.frame_offset
    image_user.frame_duration = img_spec.frame_duration

    # Image sequences need auto refresh to display reliably.
    if img_spec.image.source == 'SEQUENCE':
        image_user.use_auto_refresh = True


def create_cycles_texnode(self, node_tree, img_spec):
    tex_image = node_tree.nodes.new('ShaderNodeTexImage')
    tex_image.image = img_spec.image
    tex_image.show_texture = True
    tex_image.interpolation = self.interpolation
    tex_image.extension = self.extension
    apply_texture_options(self, tex_image, img_spec)
    return tex_image


def create_cycles_material(self, context, img_spec, name):
    material = None
    if self.overwrite_material:
        material = bpy.data.materials.get((name, None))
    if material is None:
        material = bpy.data.materials.new(name=name)
        material.node_tree.nodes.clear()

    material.surface_render_method = self.render_method
    material.use_backface_culling = self.use_backface_culling
    material.use_transparency_overlap = self.show_transparent_back

    node_tree = material.node_tree
    out_node = node_tree.nodes.new("ShaderNodeOutputMaterial")

    tex_image = create_cycles_texnode(self, node_tree, img_spec)

    if self.shader == 'PRINCIPLED':
        core_shader = node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    elif self.shader == 'SHADELESS':
        core_shader = get_shadeless_node(node_tree)
    elif self.shader == 'EMISSION':
        core_shader = node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        core_shader.inputs["Emission Strength"].default_value = self.emit_strength
        core_shader.inputs["Base Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        core_shader.inputs["Specular IOR Level"].default_value = 0.0

    # Connect color from texture.
    if self.shader in {'PRINCIPLED', 'SHADELESS'}:
        node_tree.links.new(core_shader.inputs[0], tex_image.outputs["Color"])
    elif self.shader == 'EMISSION':
        node_tree.links.new(core_shader.inputs["Emission Color"], tex_image.outputs["Color"])

    if self.use_transparency:
        if self.shader in {'PRINCIPLED', 'EMISSION'}:
            node_tree.links.new(core_shader.inputs["Alpha"], tex_image.outputs["Alpha"])
        else:
            bsdf_transparent = node_tree.nodes.new('ShaderNodeBsdfTransparent')

            mix_shader = node_tree.nodes.new('ShaderNodeMixShader')
            node_tree.links.new(mix_shader.inputs["Fac"], tex_image.outputs["Alpha"])
            node_tree.links.new(mix_shader.inputs[1], bsdf_transparent.outputs["BSDF"])
            node_tree.links.new(mix_shader.inputs[2], core_shader.outputs[0])
            core_shader = mix_shader

    node_tree.links.new(out_node.inputs["Surface"], core_shader.outputs[0])

    auto_align_nodes(node_tree)
    return material


def get_input_nodes(node, links):
    # Get nodes that are a inputs to the given node.

    # Get all links going to node.
    input_links = {lnk for lnk in links if lnk.to_node == node}
    # Sort those links, get their input nodes (and avoid doubles!).
    sorted_nodes = []
    done_nodes = set()
    for socket in node.inputs:
        done_links = set()
        for link in input_links:
            nd = link.from_node
            if nd in done_nodes:
                # Node already treated!
                done_links.add(link)
            elif link.to_socket == socket:
                sorted_nodes.append(nd)
                done_links.add(link)
                done_nodes.add(nd)
        input_links -= done_links
    return sorted_nodes


def auto_align_nodes(node_tree):
    # Given a shader node tree, arrange nodes neatly relative to the output node.

    x_gap = 200
    y_gap = 180
    nodes = node_tree.nodes
    links = node_tree.links
    output_node = None
    for node in nodes:
        if node.type in {'OUTPUT_MATERIAL', 'GROUP_OUTPUT'}:
            output_node = node
            break

    else:  # Just in case there is no output.
        return

    def align(to_node):
        from_nodes = get_input_nodes(to_node, links)
        for i, node in enumerate(from_nodes):
            node.location.x = min(node.location.x, to_node.location.x - x_gap)
            node.location.y = to_node.location.y
            node.location.y -= i * y_gap
            node.location.y += (len(from_nodes) - 1) * y_gap / (len(from_nodes))
            align(node)

    align(output_node)


def clean_node_tree(node_tree):
    # Clear all nodes in a shader node tree except the output.
    #
    # Returns the output node

    nodes = node_tree.nodes
    for node in list(nodes):  # Copy to avoid altering the loop's data source.
        if not node.type == 'OUTPUT_MATERIAL':
            nodes.remove(node)

    return node_tree.nodes[0]


def get_shadeless_node(dest_node_tree):
    # Return a "shadeless" cycles/EEVEE node, creating a node group if nonexistent.

    # WARNING: using a hard coded name isn't fool proof!
    # Users could have this name already in a node-tree (albeit unlikely).
    node_group_name = "IAP_SHADELESS"
    if (node_tree := bpy.data.node_groups.get((node_group_name, None))) is None:
        # Need to build node shadeless node group.
        node_tree = bpy.data.node_groups.new(node_group_name, 'ShaderNodeTree')
        output_node = node_tree.nodes.new('NodeGroupOutput')
        input_node = node_tree.nodes.new('NodeGroupInput')

        node_tree.interface.new_socket('Shader', in_out='OUTPUT', socket_type='NodeSocketShader')
        node_tree.interface.new_socket('Color', in_out='INPUT', socket_type='NodeSocketColor')

        # This could be faster as a transparent shader, but then no ambient occlusion.
        diffuse_shader = node_tree.nodes.new('ShaderNodeBsdfDiffuse')
        node_tree.links.new(diffuse_shader.inputs[0], input_node.outputs[0])

        emission_shader = node_tree.nodes.new('ShaderNodeEmission')
        node_tree.links.new(emission_shader.inputs[0], input_node.outputs[0])

        light_path = node_tree.nodes.new('ShaderNodeLightPath')
        is_glossy_ray = light_path.outputs["Is Glossy Ray"]
        is_shadow_ray = light_path.outputs["Is Shadow Ray"]
        ray_depth = light_path.outputs["Ray Depth"]
        transmission_depth = light_path.outputs["Transmission Depth"]

        unrefracted_depth = node_tree.nodes.new('ShaderNodeMath')
        unrefracted_depth.operation = 'SUBTRACT'
        unrefracted_depth.label = 'Bounce Count'
        node_tree.links.new(unrefracted_depth.inputs[0], ray_depth)
        node_tree.links.new(unrefracted_depth.inputs[1], transmission_depth)

        refracted = node_tree.nodes.new('ShaderNodeMath')
        refracted.operation = 'SUBTRACT'
        refracted.label = 'Camera or Refracted'
        refracted.inputs[0].default_value = 1.0
        node_tree.links.new(refracted.inputs[1], unrefracted_depth.outputs[0])

        reflection_limit = node_tree.nodes.new('ShaderNodeMath')
        reflection_limit.operation = 'SUBTRACT'
        reflection_limit.label = 'Limit Reflections'
        reflection_limit.inputs[0].default_value = 2.0
        node_tree.links.new(reflection_limit.inputs[1], ray_depth)

        camera_reflected = node_tree.nodes.new('ShaderNodeMath')
        camera_reflected.operation = 'MULTIPLY'
        camera_reflected.label = 'Camera Ray to Glossy'
        node_tree.links.new(camera_reflected.inputs[0], reflection_limit.outputs[0])
        node_tree.links.new(camera_reflected.inputs[1], is_glossy_ray)

        shadow_or_reflect = node_tree.nodes.new('ShaderNodeMath')
        shadow_or_reflect.operation = 'MAXIMUM'
        shadow_or_reflect.label = 'Shadow or Reflection?'
        node_tree.links.new(shadow_or_reflect.inputs[0], camera_reflected.outputs[0])
        node_tree.links.new(shadow_or_reflect.inputs[1], is_shadow_ray)

        shadow_or_reflect_or_refract = node_tree.nodes.new('ShaderNodeMath')
        shadow_or_reflect_or_refract.operation = 'MAXIMUM'
        shadow_or_reflect_or_refract.label = 'Shadow, Reflect or Refract?'
        node_tree.links.new(shadow_or_reflect_or_refract.inputs[0], shadow_or_reflect.outputs[0])
        node_tree.links.new(shadow_or_reflect_or_refract.inputs[1], refracted.outputs[0])

        mix_shader = node_tree.nodes.new('ShaderNodeMixShader')
        node_tree.links.new(mix_shader.inputs[0], shadow_or_reflect_or_refract.outputs[0])
        node_tree.links.new(mix_shader.inputs[1], diffuse_shader.outputs[0])
        node_tree.links.new(mix_shader.inputs[2], emission_shader.outputs[0])

        node_tree.links.new(output_node.inputs[0], mix_shader.outputs[0])

        auto_align_nodes(node_tree)

    group_node = dest_node_tree.nodes.new("ShaderNodeGroup")
    group_node.node_tree = node_tree

    return group_node


# -----------------------------------------------------------------------------
# Operator

class IMAGE_OT_import_as_mesh_planes(
        AddObjectHelper, ImportHelper, MaterialProperties_MixIn, TextureProperties_MixIn, Operator,
):
    """Create mesh plane(s) from image files with the appropriate aspect ratio"""

    bl_idname = "image.import_as_mesh_planes"
    bl_label = "Import Images as Planes"
    bl_options = {'REGISTER', 'PRESET', 'UNDO'}

    # ----------------------
    # File dialog properties
    files: CollectionProperty(
        type=bpy.types.OperatorFileListElement,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    directory: StringProperty(
        maxlen=1024,
        subtype='FILE_PATH',
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    filter_image: BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})
    filter_movie: BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})
    filter_folder: BoolProperty(default=True, options={'HIDDEN', 'SKIP_SAVE'})

    # ----------------------
    # Properties - Importing
    force_reload: BoolProperty(
        name="Force Reload",
        default=False,
        description="Force reload the image if it is already opened elsewhere in Blender",
    )

    image_sequence: BoolProperty(
        name="Detect Image Sequences",
        default=False,
        description=(
            "Import sequentially numbered images as an animated "
            "image sequence instead of separate planes"
        ),
    )

    # -------------------------------------
    # Properties - Position and Orientation
    axis_id_to_vector = {
        '+X': Vector((1.0, 0.0, 0.0)),
        '+Y': Vector((0.0, 1.0, 0.0)),
        '+Z': Vector((0.0, 0.0, 1.0)),
        '-X': Vector((-1.0, 0.0, 0.0)),
        '-Y': Vector((0.0, -1.0, 0.0)),
        '-Z': Vector((0.0, 0.0, -1.0)),
    }

    offset: BoolProperty(
        name="Offset Planes",
        default=True,
        description=(
            "Offset planes from each other. "
            "If disabled, multiple planes will be created at the same location"
        )
    )

    offset_axis: EnumProperty(
        name="Offset Direction",
        default='+X',
        items=(
            ('+X', "+X", "Side by Side to the Left"),
            ('+Y', "+Y", "Side by Side, Downward"),
            ('+Z', "+Z", "Stacked Above"),
            ('-X', "-X", "Side by Side to the Right"),
            ('-Y', "-Y", "Side by Side, Upward"),
            ('-Z', "-Z", "Stacked Below"),
        ),
        description="How planes are oriented relative to each others' local axis",
    )

    offset_amount: FloatProperty(
        name="Offset Distance",
        soft_min=0,
        default=0.1,
        description="Set distance between each plane",
        subtype='DISTANCE',
        unit='LENGTH',
    )

    AXIS_MODES = (
        ('+X', "+X", "Facing positive X"),
        ('+Y', "+Y", "Facing positive Y"),
        ('+Z', "+Z", "Facing positive Z"),
        ('-X', "-X", "Facing negative X"),
        ('-Y', "-Y", "Facing negative Y"),
        ('-Z', "-Z", "Facing negative Z"),
        ('CAM', "Face Camera", "Facing camera"),
        ('CAM_AX', "Camera's Main Axis", "Facing the camera's dominant axis"),
    )
    align_axis: EnumProperty(
        name="Align",
        default='CAM_AX',
        items=AXIS_MODES,
        description="How to align the planes",
    )
    # Prev_align_axis is used only by update_size_model.
    prev_align_axis: EnumProperty(
        items=AXIS_MODES + (('NONE', '', ''),),
        default='NONE',
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    align_track: BoolProperty(
        name="Track Camera",
        default=False,
        description="Add a constraint to make the planes track the camera",
    )

    # -----------------
    # Properties - Size
    def update_size_mode(self, _context):
        # If sizing relative to the camera, always face the camera.

        if self.size_mode == 'CAMERA':
            self.prev_align_axis = self.align_axis
            self.align_axis = 'CAM'
        elif self.prev_align_axis != 'NONE':
            # If a different alignment was set revert to that when size mode is changed.
            self.align_axis = self.prev_align_axis
            self.prev_align_axis = 'NONE'

    size_mode: EnumProperty(
        name="Size Mode",
        default='ABSOLUTE',
        items=(
            ('ABSOLUTE', "Absolute", "Use absolute size"),
            ('CAMERA', "Scale to Camera Frame", "Scale to fit or fill the camera frame"),
            ('DPI', "Pixels per Inch", "Scale based on pixels per inch"),
            ('DPBU', "Pixels per Blender Unit", "Scale based on pixels per Blender Unit"),
        ),
        update=update_size_mode,
        description="Method for computing the plane size",
    )

    fill_mode: EnumProperty(
        name="Scale",
        default='FILL',
        items=(
            ('FILL', "Fill", "Fill camera frame, spilling outside the frame"),
            ('FIT', "Fit", "Fit entire image within the camera frame"),
        ),
        description="Method to scale the plane with the camera frame",
    )

    height: FloatProperty(
        name="Height",
        description="Height of the created plane",
        default=1.0,
        min=0.001,
        soft_min=0.001,
        subtype='DISTANCE',
        unit='LENGTH',
    )

    factor: FloatProperty(
        name="Definition",
        min=1.0,
        default=600.0,
        description="Number of pixels per inch or Blender Unit",
    )

    # -------
    # Draw UI

    def draw_import_config(self, _context):
        # --- Import Options --- #
        layout = self.layout

        header, body = layout.panel("import_image_plane_options", default_closed=False)
        header.label(text="Options")
        if body:
            row = body.row()
            row.active = bpy.data.is_saved
            row.prop(self, "relative")
            body.prop(self, "force_reload")
            body.prop(self, "image_sequence")

    def draw_spatial_config(self, _context):
        # --- Spatial Properties: Position, Size and Orientation --- #
        layout = self.layout

        header, body = layout.panel("import_image_plane_transform", default_closed=False)
        header.label(text="Transform")
        if body:
            body.prop(self, "size_mode")
            if self.size_mode == 'ABSOLUTE':
                body.prop(self, "height")
            elif self.size_mode == 'CAMERA':
                body.prop(self, "fill_mode")
            else:
                body.prop(self, "factor")

            row = body.row()
            row.enabled = 'CAM' not in self.size_mode
            row.prop(self, "align_axis")
            if 'CAM' in self.align_axis:
                body.prop(self, "align_track")

            body.prop(self, "offset")
            col = body.column()
            col.enabled = self.offset
            col.prop(self, "offset_axis")
            col.prop(self, "offset_amount", text="Distance")

    def draw(self, context):
        # Draw configuration sections.
        layout = self.layout
        layout.use_property_split = True

        self.draw_import_config(context)
        MaterialProperties_MixIn.draw_material_config(self, context)
        TextureProperties_MixIn.draw_texture_config(self, context)
        self.draw_spatial_config(context)

    # -------------------------------------------------------------------------
    # Core functionality
    def invoke(self, context, _event):
        engine = context.scene.render.engine
        if engine not in COMPATIBLE_ENGINES:
            self.report({'ERROR'}, rpt_("Cannot generate materials for unknown {:s} render engine").format(engine))
            return {'CANCELLED'}

        if engine == 'BLENDER_WORKBENCH':
            self.report(
                {'WARNING'},
                rpt_("Generating Cycles/EEVEE compatible material, but won't be visible with {:s} engine").format(
                    engine,
                ))

        return self.invoke_popup(context)

    def execute(self, context):
        if not bpy.data.is_saved:
            self.relative = False

        # This won't work in edit mode.
        editmode = context.preferences.edit.use_enter_edit_mode
        context.preferences.edit.use_enter_edit_mode = False
        if context.active_object and context.active_object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        ret_code = self.import_images(context)

        context.preferences.edit.use_enter_edit_mode = editmode

        return ret_code

    def import_images(self, context):

        # Load images / sequences.
        images = tuple(load_images(
            (fn.name for fn in self.files),
            self.directory,
            force_reload=self.force_reload,
            find_sequences=self.image_sequence,
        ))

        if not images:
            self.report({'WARNING'}, "Please select at least one image")
            return {'CANCELLED'}

        # Create individual planes.
        planes = [self.single_image_spec_to_plane(context, img_spec) for img_spec in images]

        context.view_layer.update()

        # Align planes relative to each other.
        if self.offset:
            offset_axis = self.axis_id_to_vector[self.offset_axis]
            offset_planes(planes, self.offset_amount, offset_axis)

            if self.size_mode == 'CAMERA' and offset_axis.z:
                for plane in planes:
                    x, y = compute_camera_size(
                        context, plane.location,
                        self.fill_mode, plane.dimensions.x / plane.dimensions.y,
                    )
                    plane.dimensions = x, y, 0.0

        # Setup new selection.
        for plane in planes:
            plane.select_set(True)

        # All done!
        self.report({'INFO'}, rpt_("Added {:d} Image Plane(s)").format(len(planes)))
        return {'FINISHED'}

    # Operate on a single image.
    def single_image_spec_to_plane(self, context, img_spec):

        name = bpy.path.display_name_from_filepath(img_spec.image.filepath)

        # Configure image.
        self.apply_image_options(img_spec.image)

        # Configure material.
        # TODO: check `context.scene.render.engine` and support other engines.
        material = create_cycles_material(self, context, img_spec, name)

        # Create and position plane object.
        plane = self.create_image_plane(context, name, img_spec)

        # Assign Material.
        plane.data.materials.append(material)

        return plane

    def apply_image_options(self, image):
        if not self.use_transparency:
            image.alpha_mode = 'NONE'

        if self.relative:
            try:  # Can't always find the relative path (between drive letters on windows).
                image.filepath = bpy.path.relpath(image.filepath)
            except ValueError:
                pass

    def apply_material_options(self, material, slot):
        shader = self.shader

        if self.use_transparency:
            material.alpha = 0.0
            material.specular_alpha = 0.0
            slot.use_map_alpha = True
        else:
            material.alpha = 1.0
            material.specular_alpha = 1.0
            slot.use_map_alpha = False

        material.specular_intensity = 0
        material.diffuse_intensity = 1.0
        material.use_transparency = self.use_transparency
        material.transparency_method = 'Z_TRANSPARENCY'
        material.use_shadeless = (shader == 'SHADELESS')
        material.use_transparent_shadows = (shader == 'DIFFUSE')
        material.emit = self.emit_strength if shader == 'EMISSION' else 0.0

    # -------------------------------------------------------------------------
    # Geometry Creation
    def create_image_plane(self, context, name, img_spec):

        width, height = self.compute_plane_size(context, img_spec)

        # Create new mesh.
        bpy.ops.mesh.primitive_plane_add('INVOKE_REGION_WIN')
        plane = context.active_object
        # Why does mesh.primitive_plane_add leave the object in edit mode???
        if plane.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        plane.dimensions = width, height, 0.0
        plane.data.name = plane.name = name
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

        # If sizing for camera, also insert into the camera's field of view.
        if self.size_mode == 'CAMERA':
            offset_axis = self.axis_id_to_vector[self.offset_axis]
            translate_axis = [0 if offset_axis[i] else 1 for i in (0, 1)]
            center_in_camera(context.scene.camera, plane, translate_axis)

        self.align_plane(context, plane)

        return plane

    def compute_plane_size(self, context, img_spec):
        # Given the image size in pixels and location, determine size of plane.

        px, py = img_spec.size

        # Can't load data.
        if px == 0 or py == 0:
            px = py = 1

        if self.size_mode == 'ABSOLUTE':
            y = self.height
            x = px / py * y

        elif self.size_mode == 'CAMERA':
            x, y = compute_camera_size(
                context, context.scene.cursor.location,
                self.fill_mode, px / py,
            )

        elif self.size_mode == 'DPI':
            fact = 1 / self.factor / context.scene.unit_settings.scale_length * 0.0254
            x = px * fact
            y = py * fact

        else:  # `elif self.size_mode == 'DPBU'`
            fact = 1 / self.factor
            x = px * fact
            y = py * fact

        return x, y

    def align_plane(self, context, plane):
        # Pick an axis and align the plane to it.

        from math import pi
        if 'CAM' in self.align_axis:
            # Camera-aligned.
            camera = context.scene.camera
            if camera is not None:
                # Find the axis that best corresponds to the camera's view direction.
                axis = camera.matrix_world @ Vector((0.0, 0.0, 1.0)) - camera.matrix_world.col[3].xyz
                # Pick the axis with the greatest magnitude.
                mag = max(map(abs, axis))
                # And use that axis & direction.
                axis = Vector([
                    n / mag if abs(n) == mag else 0.0
                    for n in axis
                ])
            else:
                # No camera? Just face Z axis.
                axis = Vector((0.0, 0.0, 1.0))
                self.align_axis = '+Z'
        else:
            # Axis-aligned.
            axis = self.axis_id_to_vector[self.align_axis]

        # Rotate accordingly for X/Y axis.
        if not axis.z:
            plane.rotation_euler.x = pi / 2

            if axis.y > 0:
                plane.rotation_euler.z = pi
            elif axis.y < 0:
                plane.rotation_euler.z = 0
            elif axis.x > 0:
                plane.rotation_euler.z = pi / 2
            elif axis.x < 0:
                plane.rotation_euler.z = -pi / 2

        # Or flip 180 degrees for negative Z.
        elif axis.z < 0:
            plane.rotation_euler.y = pi

        if self.align_axis == 'CAM':
            constraint = plane.constraints.new('COPY_ROTATION')
            constraint.target = camera
            constraint.use_x = constraint.use_y = constraint.use_z = True
            if not self.align_track:
                bpy.ops.object.visual_transform_apply()
                plane.constraints.clear()

        if self.align_axis == 'CAM_AX' and self.align_track:
            constraint = plane.constraints.new('LOCKED_TRACK')
            constraint.target = camera
            constraint.track_axis = 'TRACK_Z'
            constraint.lock_axis = 'LOCK_Y'


class IMAGE_OT_convert_to_mesh_plane(MaterialProperties_MixIn, TextureProperties_MixIn, Operator):
    """Convert selected reference images to textured mesh plane"""
    bl_idname = "image.convert_to_mesh_plane"
    bl_label = "Convert Image Empty to Mesh Plane"
    bl_options = {'REGISTER', 'PRESET', 'UNDO'}

    name_from: EnumProperty(
        name="Name After",
        items=[
            ('OBJECT', "Source Object", "Name after object source with a suffix"),
            ('IMAGE', "Source Image", "Name from loaded image"),
        ],
        default='OBJECT',
        description="Name for new mesh object and material",
    )

    delete_ref: BoolProperty(
        name="Delete Reference Object",
        default=True,
        description="Delete empty image object once mesh plane is created",
    )

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (
            ob is not None and
            ob.select_get() and
            cls._is_object_empty_image(ob)
        )

    @staticmethod
    def _is_object_empty_image(ob):
        return (
            (ob.type == 'EMPTY') and
            (ob.empty_display_type == 'IMAGE') and
            (ob.data is not None)
        )

    def invoke(self, context, _event):
        scene = context.scene
        engine = scene.render.engine

        if engine not in COMPATIBLE_ENGINES:
            self.report({'ERROR'}, rpt_("Cannot generate materials for unknown {:s} render engine").format(engine))
            return {'CANCELLED'}

        if engine == 'BLENDER_WORKBENCH':
            self.report(
                {'WARNING'},
                rpt_("Generating Cycles/EEVEE compatible material, but won't be visible with {:s} engine").format(
                    engine,
                ))

        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):

        selected_objects = [ob for ob in context.selected_objects]
        converted = 0

        for ob in selected_objects:
            if not self._is_object_empty_image(ob):
                continue

            img = ob.data
            img_user = ob.image_user
            ob_name = ob.name

            # Give Name.
            if self.name_from == 'IMAGE':
                name = bpy.path.display_name(img.name, title_case=False)
            else:
                assert self.name_from == 'OBJECT'
                name = ob.name

            # Create Mesh Plane.
            obj_space_corners = get_ref_object_space_coord(ob)

            face = [(0, 1, 3, 2)]
            mesh = bpy.data.meshes.new(name)
            mesh.from_pydata(obj_space_corners, [], face)
            plane = bpy.data.objects.new(name, mesh)
            mesh.uv_layers.new(name="UVMap")

            # Link in the Same Collections.
            users_collection = ob.users_collection
            for collection in users_collection:
                collection.objects.link(plane)

            # Assign Parent.
            plane.parent = ob.parent
            plane.matrix_local = ob.matrix_local
            plane.matrix_parent_inverse = ob.matrix_parent_inverse

            # Create Material.
            img_spec = ImageSpec(
                img,
                (img.size[0], img.size[1]),
                img_user.frame_start,
                img_user.frame_offset,
                img_user.frame_duration,
            )
            material = create_cycles_material(self, context, img_spec, name)
            plane.data.materials.append(material)

            # Delete Empty.
            if self.delete_ref:
                for collection in users_collection:
                    collection.objects.unlink(ob)
                mesh.name = ob_name
                plane.name = ob_name

            plane.select_set(True)
            converted += 1

        if not converted:
            self.report({'ERROR'}, "No images converted")
            return {'CANCELLED'}

        self.report({'INFO'}, rpt_("{:d} image(s) converted to mesh plane(s)").format(converted))
        return {'FINISHED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        # General.
        col = layout.column(align=False)
        col.prop(self, "name_from")
        col.prop(self, "delete_ref")
        layout.separator()

        # Material.
        MaterialProperties_MixIn.draw_material_config(self, context)
        TextureProperties_MixIn.draw_texture_config(self, context)


classes = (
    IMAGE_OT_import_as_mesh_planes,
    IMAGE_OT_convert_to_mesh_plane,
)
