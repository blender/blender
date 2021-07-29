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

bl_info = {
    "name": "Export: Adobe After Effects (.jsx)",
    "description": "Export cameras, selected objects & camera solution "
        "3D Markers to Adobe After Effects CS3 and above",
    "author": "Bartek Skorupa",
    "version": (0, 64),
    "blender": (2, 69, 0),
    "location": "File > Export > Adobe After Effects (.jsx)",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/Adobe_After_Effects",
    "category": "Import-Export",
}


import bpy
import datetime
from math import degrees, floor
from mathutils import Matrix


# create list of static blender's data
def get_comp_data(context):
    scene = context.scene
    aspect_x = scene.render.pixel_aspect_x
    aspect_y = scene.render.pixel_aspect_y
    aspect = aspect_x / aspect_y
    start = scene.frame_start
    end = scene.frame_end
    active_cam_frames = get_active_cam_for_each_frame(scene, start, end)
    fps = floor(scene.render.fps / (scene.render.fps_base) * 1000.0) / 1000.0

    return {
        'scn': scene,
        'width': scene.render.resolution_x,
        'height': scene.render.resolution_y,
        'aspect': aspect,
        'fps': fps,
        'start': start,
        'end': end,
        'duration': (end - start + 1.0) / fps,
        'active_cam_frames': active_cam_frames,
        'curframe': scene.frame_current,
        }


# create list of active camera for each frame in case active camera is set by markers
def get_active_cam_for_each_frame(scene, start, end):
    active_cam_frames = []
    sorted_markers = []
    markers = scene.timeline_markers
    if markers:
        for marker in markers:
            if marker.camera:
                sorted_markers.append([marker.frame, marker])
        sorted_markers = sorted(sorted_markers)

        if sorted_markers:
            for frame in range(start, end + 1):
                for m, marker in enumerate(sorted_markers):
                    if marker[0] > frame:
                        if m != 0:
                            active_cam_frames.append(sorted_markers[m - 1][1].camera)
                        else:
                            active_cam_frames.append(marker[1].camera)
                        break
                    elif m == len(sorted_markers) - 1:
                        active_cam_frames.append(marker[1].camera)
    if not active_cam_frames:
        if scene.camera:
            # in this case active_cam_frames array will have legth of 1. This will indicate that there is only one active cam in all frames
            active_cam_frames.append(scene.camera)

    return(active_cam_frames)


# create managable list of selected objects
def get_selected(context):
    cameras = []  # list of selected cameras
    solids = []  # list of all selected meshes that can be exported as AE's solids
    lights = []  # list of all selected lamps that can be exported as AE's lights
    nulls = []  # list of all selected objects exept cameras (will be used to create nulls in AE)
    obs = context.selected_objects

    for ob in obs:
        if ob.type == 'CAMERA':
            cameras.append([ob, convert_name(ob.name)])

        elif is_plane(ob):
            # not ready yet. is_plane(object) returns False in all cases. This is temporary
            solids.append([ob, convert_name(ob.name)])

        elif ob.type == 'LAMP':
            lights.append([ob, ob.data.type + convert_name(ob.name)])  # Type of lamp added to name

        else:
            nulls.append([ob, convert_name(ob.name)])

    selection = {
        'cameras': cameras,
        'solids': solids,
        'lights': lights,
        'nulls': nulls,
        }

    return selection


# check if object is plane and can be exported as AE's solid
def is_plane(object):
    # work in progress. Not ready yet
    return False


# convert names of objects to avoid errors in AE.
def convert_name(name):
    name = "_" + name
    '''
    # Digits are not allowed at beginning of AE vars names.
    # This section is commented, as "_" is added at beginning of names anyway.
    # Placeholder for this name modification is left so that it's not ignored if needed
    if name[0].isdigit():
        name = "_" + name
    '''
    name = bpy.path.clean_name(name)
    name = name.replace("-", "_")

    return name


# get object's blender's location rotation and scale and return AE's Position, Rotation/Orientation and scale
# this function will be called for every object for every frame
def convert_transform_matrix(matrix, width, height, aspect, x_rot_correction=False):

    # get blender transform data for ob
    b_loc = matrix.to_translation()
    b_rot = matrix.to_euler('ZYX')  # ZYX euler matches AE's orientation and allows to use x_rot_correction
    b_scale = matrix.to_scale()

    # convert to AE Position Rotation and Scale
    # Axes in AE are different. AE's X is blender's X, AE's Y is negative Blender's Z, AE's Z is Blender's Y
    x = (b_loc.x * 100.0) / aspect + width / 2.0  # calculate AE's X position
    y = (-b_loc.z * 100.0) + (height / 2.0)  # calculate AE's Y position
    z = b_loc.y * 100.0  # calculate AE's Z position
    # Convert rotations to match AE's orientation.
    rx = degrees(b_rot.x)  # if not x_rot_correction - AE's X orientation = blender's X rotation if 'ZYX' euler.
    ry = -degrees(b_rot.y)  # AE's Y orientation is negative blender's Y rotation if 'ZYX' euler
    rz = -degrees(b_rot.z)  # AE's Z orientation is negative blender's Z rotation if 'ZYX' euler
    if x_rot_correction:
        rx -= 90.0  # In blender - ob of zero rotation lay on floor. In AE layer of zero orientation "stands"
    # Convert scale to AE scale
    sx = b_scale.x * 100.0  # scale of 1.0 is 100% in AE
    sy = b_scale.z * 100.0  # scale of 1.0 is 100% in AE
    sz = b_scale.y * 100.0  # scale of 1.0 is 100% in AE

    return x, y, z, rx, ry, rz, sx, sy, sz

# get camera's lens and convert to AE's "zoom" value in pixels
# this function will be called for every camera for every frame
#
#
# AE's lens is defined by "zoom" in pixels. Zoom determines focal angle or focal length.
#
# ZOOM VALUE CALCULATIONS:
#
# Given values:
#     - sensor width (camera.data.sensor_width)
#     - sensor height (camera.data.sensor_height)
#     - sensor fit (camera.data.sensor_fit)
#     - lens (blender's lens in mm)
#     - width (width of the composition/scene in pixels)
#     - height (height of the composition/scene in pixels)
#     - PAR (pixel aspect ratio)
#
# Calculations are made using sensor's size and scene/comp dimension (width or height).
# If camera.sensor_fit is set to 'AUTO' or 'HORIZONTAL' - sensor = camera.data.sensor_width, dimension = width.
# If camera.sensor_fit is set to 'VERTICAL' - sensor = camera.data.sensor_height, dimension = height
#
# zoom can be calculated using simple proportions.
#
#                             |
#                           / |
#                         /   |
#                       /     | d
#       s  |\         /       | i
#       e  |  \     /         | m
#       n  |    \ /           | e
#       s  |    / \           | n
#       o  |  /     \         | s
#       r  |/         \       | i
#                       \     | o
#          |     |        \   | n
#          |     |          \ |
#          |     |            |
#           lens |    zoom
#
#    zoom / dimension = lens / sensor   =>
#    zoom = lens * dimension / sensor
#
#    above is true if square pixels are used. If not - aspect compensation is needed, so final formula is:
#    zoom = lens * dimension / sensor * aspect


def convert_lens(camera, width, height, aspect):
    if camera.data.sensor_fit == 'VERTICAL':
        sensor = camera.data.sensor_height
        dimension = height
    else:
        sensor = camera.data.sensor_width
        dimension = width

    zoom = camera.data.lens * dimension / sensor * aspect

    return zoom

# convert object bundle's matrix. Not ready yet. Temporarily not active
#def get_ob_bundle_matrix_world(cam_matrix_world, bundle_matrix):
#    matrix = cam_matrix_basis
#    return matrix


# jsx script for AE creation
def write_jsx_file(file, data, selection, include_animation, include_active_cam, include_selected_cams, include_selected_objects, include_cam_bundles):

    print("\n---------------------------\n- Export to After Effects -\n---------------------------")
    # store the current frame to restore it at the end of export
    curframe = data['curframe']
    # create array which will contain all keyframes values
    js_data = {
        'times': '',
        'cameras': {},
        'solids': {},  # not ready yet
        'lights': {},
        'nulls': {},
        'bundles_cam': {},
        'bundles_ob': {},  # not ready yet
        }

    # create structure for active camera/cameras
    active_cam_name = ''
    if include_active_cam and data['active_cam_frames'] != []:
        # check if more that one active cam exist (true if active cams set by markers)
        if len(data['active_cam_frames']) is 1:
            name_ae = convert_name(data['active_cam_frames'][0].name)  # take name of the only active camera in scene
        else:
            name_ae = 'Active_Camera'
        active_cam_name = name_ae  # store name to be used when creating keyframes for active cam.
        js_data['cameras'][name_ae] = {
            'position': '',
            'position_static': '',
            'position_anim': False,
            'orientation': '',
            'orientation_static': '',
            'orientation_anim': False,
            'zoom': '',
            'zoom_static': '',
            'zoom_anim': False,
            }

    # create camera structure for selected cameras
    if include_selected_cams:
        for i, cam in enumerate(selection['cameras']):  # more than one camera can be selected
            if cam[1] != active_cam_name:
                name_ae = selection['cameras'][i][1]
                js_data['cameras'][name_ae] = {
                    'position': '',
                    'position_static': '',
                    'position_anim': False,
                    'orientation': '',
                    'orientation_static': '',
                    'orientation_anim': False,
                    'zoom': '',
                    'zoom_static': '',
                    'zoom_anim': False,
                    }
    '''
    # create structure for solids. Not ready yet. Temporarily not active
    for i, obj in enumerate(selection['solids']):
        name_ae = selection['solids'][i][1]
        js_data['solids'][name_ae] = {
            'position': '',
            'orientation': '',
            'rotationX': '',
            'scale': '',
            }
    '''
    # create structure for lights
    for i, obj in enumerate(selection['lights']):
        if include_selected_objects:
            name_ae = selection['lights'][i][1]
            js_data['lights'][name_ae] = {
                'type': selection['lights'][i][0].data.type,
                'energy': '',
                'energy_static': '',
                'energy_anim': False,
                'cone_angle': '',
                'cone_angle_static': '',
                'cone_angle_anim': False,
                'cone_feather': '',
                'cone_feather_static': '',
                'cone_feather_anim': False,
                'color': '',
                'color_static': '',
                'color_anim': False,
                'position': '',
                'position_static': '',
                'position_anim': False,
                'orientation': '',
                'orientation_static': '',
                'orientation_anim': False,
                }

    # create structure for nulls
    for i, obj in enumerate(selection['nulls']):  # nulls representing blender's obs except cameras, lamps and solids
        if include_selected_objects:
            name_ae = selection['nulls'][i][1]
            js_data['nulls'][name_ae] = {
                'position': '',
                'position_static': '',
                'position_anim': False,
                'orientation': '',
                'orientation_static': '',
                'orientation_anim': False,
                'scale': '',
                'scale_static': '',
                'scale_anim': False,
                }

    # create structure for cam bundles including positions (cam bundles don't move)
    if include_cam_bundles:
        # go through each selected camera and active cameras
        selected_cams = []
        active_cams = []
        if include_active_cam:
            active_cams = data['active_cam_frames']
        if include_selected_cams:
            for cam in selection['cameras']:
                selected_cams.append(cam[0])
        # list of cameras that will be checked for 'CAMERA SOLVER'
        cams = list(set.union(set(selected_cams), set(active_cams)))

        for cam in cams:
            # go through each constraints of this camera
            for constraint in cam.constraints:
                # does the camera have a Camera Solver constraint
                if constraint.type == 'CAMERA_SOLVER':
                    # Which movie clip does it use
                    if constraint.use_active_clip:
                        clip = data['scn'].active_clip
                    else:
                        clip = constraint.clip

                    # go through each tracking point
                    for track in clip.tracking.tracks:
                        # Does this tracking point have a bundle (has its 3D position been solved)
                        if track.has_bundle:
                            # get the name of the tracker
                            name_ae = convert_name(str(cam.name) + '__' + str(track.name))
                            js_data['bundles_cam'][name_ae] = {
                                'position': '',
                                }
                            # bundles are in camera space. Transpose to world space
                            matrix = Matrix.Translation(cam.matrix_basis.copy() * track.bundle)
                            # convert the position into AE space
                            ae_transform = convert_transform_matrix(matrix, data['width'], data['height'], data['aspect'], x_rot_correction=False)
                            js_data['bundles_cam'][name_ae]['position'] += '[%f,%f,%f],' % (ae_transform[0], ae_transform[1], ae_transform[2])

    # get all keyframes for each object and store in dico
    if include_animation:
        end = data['end'] + 1
    else:
        end = data['start'] + 1
    for frame in range(data['start'], end):
        print("working on frame: " + str(frame))
        data['scn'].frame_set(frame)

        # get time for this loop
        js_data['times'] += '%f ,' % ((frame - data['start']) / data['fps'])

        # keyframes for active camera/cameras
        if include_active_cam and data['active_cam_frames'] != []:
            if len(data['active_cam_frames']) == 1:
                cur_cam_index = 0
            else:
                cur_cam_index = frame - data['start']
            active_cam = data['active_cam_frames'][cur_cam_index]
            # get cam name
            name_ae = active_cam_name
            # convert cam transform properties to AE space
            ae_transform = convert_transform_matrix(active_cam.matrix_world.copy(), data['width'], data['height'], data['aspect'], x_rot_correction=True)
            # convert Blender's lens to AE's zoom in pixels
            zoom = convert_lens(active_cam, data['width'], data['height'], data['aspect'])
            # store all values in dico
            position = '[%f,%f,%f],' % (ae_transform[0], ae_transform[1], ae_transform[2])
            orientation = '[%f,%f,%f],' % (ae_transform[3], ae_transform[4], ae_transform[5])
            zoom = '%f,' % (zoom)
            js_data['cameras'][name_ae]['position'] += position
            js_data['cameras'][name_ae]['orientation'] += orientation
            js_data['cameras'][name_ae]['zoom'] += zoom
            # Check if properties change values compared to previous frame
            # If property don't change through out the whole animation - keyframes won't be added
            if frame != data['start']:
                if position != js_data['cameras'][name_ae]['position_static']:
                    js_data['cameras'][name_ae]['position_anim'] = True
                if orientation != js_data['cameras'][name_ae]['orientation_static']:
                    js_data['cameras'][name_ae]['orientation_anim'] = True
                if zoom != js_data['cameras'][name_ae]['zoom_static']:
                    js_data['cameras'][name_ae]['zoom_anim'] = True
            js_data['cameras'][name_ae]['position_static'] = position
            js_data['cameras'][name_ae]['orientation_static'] = orientation
            js_data['cameras'][name_ae]['zoom_static'] = zoom

        # keyframes for selected cameras
        if include_selected_cams:
            for i, cam in enumerate(selection['cameras']):
                if cam[1] != active_cam_name:
                    # get cam name
                    name_ae = selection['cameras'][i][1]
                    # convert cam transform properties to AE space
                    ae_transform = convert_transform_matrix(cam[0].matrix_world.copy(), data['width'], data['height'], data['aspect'], x_rot_correction=True)
                    # convert Blender's lens to AE's zoom in pixels
                    zoom = convert_lens(cam[0], data['width'], data['height'], data['aspect'])
                    # store all values in dico
                    position = '[%f,%f,%f],' % (ae_transform[0], ae_transform[1], ae_transform[2])
                    orientation = '[%f,%f,%f],' % (ae_transform[3], ae_transform[4], ae_transform[5])
                    zoom = '%f,' % (zoom)
                    js_data['cameras'][name_ae]['position'] += position
                    js_data['cameras'][name_ae]['orientation'] += orientation
                    js_data['cameras'][name_ae]['zoom'] += zoom
                    # Check if properties change values compared to previous frame
                    # If property don't change through out the whole animation - keyframes won't be added
                    if frame != data['start']:
                        if position != js_data['cameras'][name_ae]['position_static']:
                            js_data['cameras'][name_ae]['position_anim'] = True
                        if orientation != js_data['cameras'][name_ae]['orientation_static']:
                            js_data['cameras'][name_ae]['orientation_anim'] = True
                        if zoom != js_data['cameras'][name_ae]['zoom_static']:
                            js_data['cameras'][name_ae]['zoom_anim'] = True
                    js_data['cameras'][name_ae]['position_static'] = position
                    js_data['cameras'][name_ae]['orientation_static'] = orientation
                    js_data['cameras'][name_ae]['zoom_static'] = zoom

        '''
        # keyframes for all solids. Not ready yet. Temporarily not active
        for i, ob in enumerate(selection['solids']):
            #get object name
            name_ae = selection['solids'][i][1]
            #convert ob position to AE space
        '''

        # keyframes for all lights.
        if include_selected_objects:
            for i, ob in enumerate(selection['lights']):
                #get object name
                name_ae = selection['lights'][i][1]
                type = selection['lights'][i][0].data.type
                # convert ob transform properties to AE space
                ae_transform = convert_transform_matrix(ob[0].matrix_world.copy(), data['width'], data['height'], data['aspect'], x_rot_correction=True)
                color = ob[0].data.color
                # store all values in dico
                position = '[%f,%f,%f],' % (ae_transform[0], ae_transform[1], ae_transform[2])
                orientation = '[%f,%f,%f],' % (ae_transform[3], ae_transform[4], ae_transform[5])
                energy = '[%f],' % (ob[0].data.energy * 100.0)
                color = '[%f,%f,%f],' % (color[0], color[1], color[2])
                js_data['lights'][name_ae]['position'] += position
                js_data['lights'][name_ae]['orientation'] += orientation
                js_data['lights'][name_ae]['energy'] += energy
                js_data['lights'][name_ae]['color'] += color
                # Check if properties change values compared to previous frame
                # If property don't change through out the whole animation - keyframes won't be added
                if frame != data['start']:
                    if position != js_data['lights'][name_ae]['position_static']:
                        js_data['lights'][name_ae]['position_anim'] = True
                    if orientation != js_data['lights'][name_ae]['orientation_static']:
                        js_data['lights'][name_ae]['orientation_anim'] = True
                    if energy != js_data['lights'][name_ae]['energy_static']:
                        js_data['lights'][name_ae]['energy_anim'] = True
                    if color != js_data['lights'][name_ae]['color_static']:
                        js_data['lights'][name_ae]['color_anim'] = True
                js_data['lights'][name_ae]['position_static'] = position
                js_data['lights'][name_ae]['orientation_static'] = orientation
                js_data['lights'][name_ae]['energy_static'] = energy
                js_data['lights'][name_ae]['color_static'] = color
                if type == 'SPOT':
                    cone_angle = '[%f],' % (degrees(ob[0].data.spot_size))
                    cone_feather = '[%f],' % (ob[0].data.spot_blend * 100.0)
                    js_data['lights'][name_ae]['cone_angle'] += cone_angle
                    js_data['lights'][name_ae]['cone_feather'] += cone_feather
                    # Check if properties change values compared to previous frame
                    # If property don't change through out the whole animation - keyframes won't be added
                    if frame != data['start']:
                        if cone_angle != js_data['lights'][name_ae]['cone_angle_static']:
                            js_data['lights'][name_ae]['cone_angle_anim'] = True
                        if orientation != js_data['lights'][name_ae]['cone_feather_static']:
                            js_data['lights'][name_ae]['cone_feather_anim'] = True
                    js_data['lights'][name_ae]['cone_angle_static'] = cone_angle
                    js_data['lights'][name_ae]['cone_feather_static'] = cone_feather

        # keyframes for all nulls
        if include_selected_objects:
            for i, ob in enumerate(selection['nulls']):
                # get object name
                name_ae = selection['nulls'][i][1]
                # convert ob transform properties to AE space
                ae_transform = convert_transform_matrix(ob[0].matrix_world.copy(), data['width'], data['height'], data['aspect'], x_rot_correction=True)
                # store all values in dico
                position = '[%f,%f,%f],' % (ae_transform[0], ae_transform[1], ae_transform[2])
                orientation = '[%f,%f,%f],' % (ae_transform[3], ae_transform[4], ae_transform[5])
                scale = '[%f,%f,%f],' % (ae_transform[6], ae_transform[7], ae_transform[8])
                js_data['nulls'][name_ae]['position'] += position
                js_data['nulls'][name_ae]['orientation'] += orientation
                js_data['nulls'][name_ae]['scale'] += scale
                # Check if properties change values compared to previous frame
                # If property don't change through out the whole animation - keyframes won't be added
                if frame != data['start']:
                    if position != js_data['nulls'][name_ae]['position_static']:
                        js_data['nulls'][name_ae]['position_anim'] = True
                    if orientation != js_data['nulls'][name_ae]['orientation_static']:
                        js_data['nulls'][name_ae]['orientation_anim'] = True
                    if scale != js_data['nulls'][name_ae]['scale_static']:
                        js_data['nulls'][name_ae]['scale_anim'] = True
                js_data['nulls'][name_ae]['position_static'] = position
                js_data['nulls'][name_ae]['orientation_static'] = orientation
                js_data['nulls'][name_ae]['scale_static'] = scale

        # keyframes for all object bundles. Not ready yet.
        #
        #
        #

    # ---- write JSX file
    jsx_file = open(file, 'w')

    # make the jsx executable in After Effects (enable double click on jsx)
    jsx_file.write('#target AfterEffects\n\n')
    # Script's header
    jsx_file.write('/**************************************\n')
    jsx_file.write('Scene : %s\n' % data['scn'].name)
    jsx_file.write('Resolution : %i x %i\n' % (data['width'], data['height']))
    jsx_file.write('Duration : %f\n' % (data['duration']))
    jsx_file.write('FPS : %f\n' % (data['fps']))
    jsx_file.write('Date : %s\n' % datetime.datetime.now())
    jsx_file.write('Exported with io_export_after_effects.py\n')
    jsx_file.write('**************************************/\n\n\n\n')

    # wrap in function
    jsx_file.write("function compFromBlender(){\n")
    # create new comp
    jsx_file.write('\nvar compName = prompt("Blender Comp\'s Name \\nEnter Name of newly created Composition","BlendComp","Composition\'s Name");\n')
    jsx_file.write('if (compName){')  # Continue only if comp name is given. If not - terminate
    jsx_file.write('\nvar newComp = app.project.items.addComp(compName, %i, %i, %f, %f, %f);' %
                   (data['width'], data['height'], data['aspect'], data['duration'], data['fps']))
    jsx_file.write('\nnewComp.displayStartTime = %f;\n\n\n' % ((data['start'] + 1.0) / data['fps']))

    # create camera bundles (nulls)
    jsx_file.write('// **************  CAMERA 3D MARKERS  **************\n\n\n')
    for i, obj in enumerate(js_data['bundles_cam']):
        name_ae = obj
        jsx_file.write('var %s = newComp.layers.addNull();\n' % (name_ae))
        jsx_file.write('%s.threeDLayer = true;\n' % name_ae)
        jsx_file.write('%s.source.name = "%s";\n' % (name_ae, name_ae))
        jsx_file.write('%s.property("position").setValue(%s);\n\n\n' % (name_ae, js_data['bundles_cam'][obj]['position']))

    # create object bundles (not ready yet)

    # create objects (nulls)
    jsx_file.write('// **************  OBJECTS  **************\n\n\n')
    for i, obj in enumerate(js_data['nulls']):
        name_ae = obj
        jsx_file.write('var %s = newComp.layers.addNull();\n' % (name_ae))
        jsx_file.write('%s.threeDLayer = true;\n' % name_ae)
        jsx_file.write('%s.source.name = "%s";\n' % (name_ae, name_ae))
        # Set values of properties, add kyeframes only where needed
        if include_animation and js_data['nulls'][name_ae]['position_anim']:
            jsx_file.write('%s.property("position").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['nulls'][obj]['position']))
        else:
            jsx_file.write('%s.property("position").setValue(%s);\n' % (name_ae, js_data['nulls'][obj]['position_static']))
        if include_animation and js_data['nulls'][name_ae]['orientation_anim']:
            jsx_file.write('%s.property("orientation").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['nulls'][obj]['orientation']))
        else:
            jsx_file.write('%s.property("orientation").setValue(%s);\n' % (name_ae, js_data['nulls'][obj]['orientation_static']))
        if include_animation and js_data['nulls'][name_ae]['scale_anim']:
            jsx_file.write('%s.property("scale").setValuesAtTimes([%s],[%s]);\n\n\n' % (name_ae, js_data['times'], js_data['nulls'][obj]['scale']))
        else:
            jsx_file.write('%s.property("scale").setValue(%s);\n\n\n' % (name_ae, js_data['nulls'][obj]['scale_static']))
    # create solids (not ready yet)

    # create lights
    jsx_file.write('// **************  LIGHTS  **************\n\n\n')
    for i, obj in enumerate(js_data['lights']):
        name_ae = obj
        jsx_file.write('var %s = newComp.layers.addLight("%s", [0.0, 0.0]);\n' % (name_ae, name_ae))
        jsx_file.write('%s.autoOrient = AutoOrientType.NO_AUTO_ORIENT;\n' % name_ae)
        # Set values of properties, add kyeframes only where needed
        if include_animation and js_data['lights'][name_ae]['position_anim']:
            jsx_file.write('%s.property("position").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['position']))
        else:
            jsx_file.write('%s.property("position").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['position_static']))
        if include_animation and js_data['lights'][name_ae]['orientation_anim']:
            jsx_file.write('%s.property("orientation").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['orientation']))
        else:
            jsx_file.write('%s.property("orientation").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['orientation_static']))
        if include_animation and js_data['lights'][name_ae]['energy_anim']:
            jsx_file.write('%s.property("intensity").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['energy']))
        else:
            jsx_file.write('%s.property("intensity").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['energy_static']))
        if include_animation and js_data['lights'][name_ae]['color_anim']:
            jsx_file.write('%s.property("Color").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['color']))
        else:
            jsx_file.write('%s.property("Color").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['color_static']))
            if js_data['lights'][obj]['type'] == 'SPOT':
                if include_animation and js_data['lights'][name_ae]['cone_angle_anim']:
                    jsx_file.write('%s.property("Cone Angle").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['cone_angle']))
                else:
                    jsx_file.write('%s.property("Cone Angle").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['cone_angle_static']))
                if include_animation and js_data['lights'][name_ae]['cone_feather_anim']:
                    jsx_file.write('%s.property("Cone Feather").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['lights'][obj]['cone_feather']))
                else:
                    jsx_file.write('%s.property("Cone Feather").setValue(%s);\n' % (name_ae, js_data['lights'][obj]['cone_feather_static']))
        jsx_file.write('\n\n')

    # create cameras
    jsx_file.write('// **************  CAMERAS  **************\n\n\n')
    for i, cam in enumerate(js_data['cameras']):  # more than one camera can be selected
        name_ae = cam
        jsx_file.write('var %s = newComp.layers.addCamera("%s",[0,0]);\n' % (name_ae, name_ae))
        jsx_file.write('%s.autoOrient = AutoOrientType.NO_AUTO_ORIENT;\n' % name_ae)
        # Set values of properties, add kyeframes only where needed
        if include_animation and js_data['cameras'][name_ae]['position_anim']:
            jsx_file.write('%s.property("position").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['cameras'][cam]['position']))
        else:
            jsx_file.write('%s.property("position").setValue(%s);\n' % (name_ae, js_data['cameras'][cam]['position_static']))
        if include_animation and js_data['cameras'][name_ae]['orientation_anim']:
            jsx_file.write('%s.property("orientation").setValuesAtTimes([%s],[%s]);\n' % (name_ae, js_data['times'], js_data['cameras'][cam]['orientation']))
        else:
            jsx_file.write('%s.property("orientation").setValue(%s);\n' % (name_ae, js_data['cameras'][cam]['orientation_static']))
        if include_animation and js_data['cameras'][name_ae]['zoom_anim']:
            jsx_file.write('%s.property("zoom").setValuesAtTimes([%s],[%s]);\n\n\n' % (name_ae, js_data['times'], js_data['cameras'][cam]['zoom']))
        else:
            jsx_file.write('%s.property("zoom").setValue(%s);\n\n\n' % (name_ae, js_data['cameras'][cam]['zoom_static']))

    # Exit import if no comp name given
    jsx_file.write('\n}else{alert ("Exit Import Blender animation data \\nNo Comp\'s name has been chosen","EXIT")};')
    # Close function
    jsx_file.write("}\n\n\n")
    # Execute function. Wrap in "undo group" for easy undoing import process
    jsx_file.write('app.beginUndoGroup("Import Blender animation data");\n')
    jsx_file.write('compFromBlender();\n')  # execute function
    jsx_file.write('app.endUndoGroup();\n\n\n')
    jsx_file.close()

    data['scn'].frame_set(curframe)  # set current frame of animation in blender to state before export

##########################################
# DO IT
##########################################


def main(file, context, include_animation, include_active_cam, include_selected_cams, include_selected_objects, include_cam_bundles):
    data = get_comp_data(context)
    selection = get_selected(context)
    write_jsx_file(file, data, selection, include_animation, include_active_cam, include_selected_cams, include_selected_objects, include_cam_bundles)
    print ("\nExport to After Effects Completed")
    return {'FINISHED'}

##########################################
# ExportJsx class register/unregister
##########################################

from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty


class ExportJsx(bpy.types.Operator, ExportHelper):
    """Export selected cameras and objects animation to After Effects"""
    bl_idname = "export.jsx"
    bl_label = "Export to Adobe After Effects"
    filename_ext = ".jsx"
    filter_glob = StringProperty(default="*.jsx", options={'HIDDEN'})

    include_animation = BoolProperty(
            name="Animation",
            description="Animate Exported Cameras and Objects",
            default=True,
            )
    include_active_cam = BoolProperty(
            name="Active Camera",
            description="Include Active Camera",
            default=True,
            )
    include_selected_cams = BoolProperty(
            name="Selected Cameras",
            description="Add Selected Cameras",
            default=True,
            )
    include_selected_objects = BoolProperty(
            name="Selected Objects",
            description="Export Selected Objects",
            default=True,
            )
    include_cam_bundles = BoolProperty(
            name="Camera 3D Markers",
            description="Include 3D Markers of Camera Motion Solution for selected cameras",
            default=True,
            )
#    include_ob_bundles = BoolProperty(
#            name="Objects 3D Markers",
#            description="Include 3D Markers of Object Motion Solution for selected cameras",
#            default=True,
#            )

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label('Animation:')
        box.prop(self, 'include_animation')
        box.label('Include Cameras and Objects:')
        box.prop(self, 'include_active_cam')
        box.prop(self, 'include_selected_cams')
        box.prop(self, 'include_selected_objects')
        box.label("Include Tracking Data:")
        box.prop(self, 'include_cam_bundles')
#        box.prop(self, 'include_ob_bundles')

    @classmethod
    def poll(cls, context):
        active = context.active_object
        selected = context.selected_objects
        camera = context.scene.camera
        ok = selected or camera
        return ok

    def execute(self, context):
        return main(self.filepath, context, self.include_animation, self.include_active_cam, self.include_selected_cams, self.include_selected_objects, self.include_cam_bundles)


def menu_func(self, context):
    self.layout.operator(ExportJsx.bl_idname, text="Adobe After Effects (.jsx)")


def register():
    bpy.utils.register_class(ExportJsx)
    bpy.types.INFO_MT_file_export.append(menu_func)


def unregister():
    bpy.utils.unregister_class(ExportJsx)
    bpy.types.INFO_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()
