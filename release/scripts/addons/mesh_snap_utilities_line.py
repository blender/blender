# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 3
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####

# Contact for more information about the Addon:
# Email:    germano.costa@ig.com.br
# Twitter:  wii_mano @mano_wii

bl_info = {
    "name": "Snap Utilities Line",
    "author": "Germano Cavalcante",
    "version": (5, 7, 2),
    "blender": (2, 75, 0),
    "location": "View3D > TOOLS > Snap Utilities > snap utilities",
    "description": "Extends Blender Snap controls",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Modeling/Snap_Utils_Line",
    "category": "Mesh"}

import bpy
import bgl
import bmesh
from mathutils import Vector
from mathutils.geometry import (
        intersect_point_line,
        intersect_line_line,
        intersect_line_plane,
        intersect_ray_tri
        )
from bpy.types import (
        Operator,
        Panel,
        AddonPreferences,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        FloatVectorProperty,
        StringProperty,
        )


def get_units_info(scale, unit_system, separate_units):
    if unit_system == 'METRIC':
        scale_steps = ((1000, 'km'), (1, 'm'), (1 / 100, 'cm'),
            (1 / 1000, 'mm'), (1 / 1000000, '\u00b5m'))
    elif unit_system == 'IMPERIAL':
        scale_steps = ((5280, 'mi'), (1, '\''),
            (1 / 12, '"'), (1 / 12000, 'thou'))
        scale /= 0.3048  # BU to feet
    else:
        scale_steps = ((1, ' BU'),)
        separate_units = False

    return (scale, scale_steps, separate_units)


def convert_distance(val, units_info, precision=5):
    scale, scale_steps, separate_units = units_info
    sval = val * scale
    idx = 0
    while idx < len(scale_steps) - 1:
        if sval >= scale_steps[idx][0]:
            break
        idx += 1
    factor, suffix = scale_steps[idx]
    sval /= factor
    if not separate_units or idx == len(scale_steps) - 1:
        dval = str(round(sval, precision)) + suffix
    else:
        ival = int(sval)
        dval = str(round(ival, precision)) + suffix
        fval = sval - ival
        idx += 1
        while idx < len(scale_steps):
            fval *= scale_steps[idx - 1][0] / scale_steps[idx][0]
            if fval >= 1:
                dval += ' ' \
                    + ("%.1f" % fval) \
                    + scale_steps[idx][1]
                break
            idx += 1

    return dval


def location_3d_to_region_2d(region, rv3d, coord):
    prj = rv3d.perspective_matrix * Vector((coord[0], coord[1], coord[2], 1.0))
    width_half = region.width / 2.0
    height_half = region.height / 2.0
    return Vector((width_half + width_half * (prj.x / prj.w),
                   height_half + height_half * (prj.y / prj.w),
                   prj.z / prj.w
                   ))


def fac_nearest_to_segment_2d(co2v, v2d0, v2d1):
    u = v2d1.xy - v2d0.xy
    h = co2v.xy - v2d0.xy
    return u.dot(h) / u.length_squared


def region_2d_to_orig_and_view_vector(region, rv3d, coord, clamp=None):
    viewinv = rv3d.view_matrix.inverted()
    persinv = rv3d.perspective_matrix.inverted()

    dx = (2.0 * coord[0] / region.width) - 1.0
    dy = (2.0 * coord[1] / region.height) - 1.0

    if rv3d.is_perspective:
        origin_start = viewinv.translation.copy()

        out = Vector((dx, dy, -0.5))

        w = out.dot(persinv[3].xyz) + persinv[3][3]

        view_vector = ((persinv * out) / w) - origin_start
    else:
        view_vector = -viewinv.col[2].xyz

        origin_start = ((persinv.col[0].xyz * dx) +
                        (persinv.col[1].xyz * dy) +
                        viewinv.translation)

        if clamp != 0.0:
            if rv3d.view_perspective != 'CAMERA':
                # this value is scaled to the far clip already
                origin_offset = persinv.col[2].xyz
                if clamp is not None:
                    if clamp < 0.0:
                        origin_offset.negate()
                        clamp = -clamp
                    if origin_offset.length > clamp:
                        origin_offset.length = clamp

                origin_start -= origin_offset

    view_vector.normalize()
    return origin_start, view_vector


def out_Location(rv3d, region, orig, vector):
    view_matrix = rv3d.view_matrix
    v1 = Vector((int(view_matrix[0][0] * 1.5), int(view_matrix[0][1] * 1.5), int(view_matrix[0][2] * 1.5)))
    v2 = Vector((int(view_matrix[1][0] * 1.5), int(view_matrix[1][1] * 1.5), int(view_matrix[1][2] * 1.5)))

    hit = intersect_ray_tri(Vector((1, 0, 0)), Vector((0, 1, 0)), Vector(), (vector), (orig), False)
    if hit is None:
        hit = intersect_ray_tri(v1, v2, Vector(), (vector), (orig), False)
    if hit is None:
        hit = intersect_ray_tri(v1, v2, Vector(), (-vector), (orig), False)
    if hit is None:
        hit = Vector()
    return hit


def get_closest_edge(bm, point, dist):
    r_edge = None
    for edge in bm.edges:
        v1 = edge.verts[0].co
        v2 = edge.verts[1].co
        # Test the BVH (AABB) first
        for i in range(3):
            if v1[i] <= v2[i]:
                isect = v1[i] - dist <= point[i] <= v2[i] + dist
            else:
                isect = v2[i] - dist <= point[i] <= v1[i] + dist

            if not isect:
                break
        else:
            ret = intersect_point_line(point, v1, v2)

            if ret[1] < 0.0:
                tmp = v1
            elif ret[1] > 1.0:
                tmp = v2
            else:
                tmp = ret[0]

            new_dist = (point - tmp).length
            if new_dist <= dist:
                dist = new_dist
                r_edge = edge

    return r_edge


class SnapCache():
        bvert = None
        vco = None

        bedge = None
        v0 = None
        v1 = None
        vmid = None
        vperp = None
        v2d0 = None
        v2d1 = None
        v2dmid = None
        v2dperp = None

        bface = None
        fmid = None
        fnor = None

        out_obj = None
        out_obmat = None
        out_obimat = None


def snap_utilities(
            cache, context, obj_matrix_world,
            bm, mcursor,
            outer_verts=False,
            constrain=None,
            previous_vert=None,
            ignore_obj=None,
            increment=0.0):

    rv3d = context.region_data
    region = context.region
    scene = context.scene
    is_increment = False
    r_loc = None
    r_type = None
    r_len = 0.0
    bm_geom = None

    if bm.select_history:
        bm.select_history[0].select = False
        bm.select_history.clear()

    bpy.ops.view3d.select(location=(int(mcursor.x), int(mcursor.y)))

    if bm.select_history:
        bm_geom = bm.select_history[0]

    if isinstance(bm_geom, bmesh.types.BMVert):
        r_type = 'VERT'

        if cache.bvert != bm_geom:
            cache.bvert = bm_geom
            cache.vco = obj_matrix_world * cache.bvert.co
            # cache.v2d = location_3d_to_region_2d(region, rv3d, cache.vco)

        if constrain:
            location = intersect_point_line(cache.vco, constrain[0], constrain[1])
            # factor = location[1]
            r_loc = location[0]
        else:
            r_loc = cache.vco

    elif isinstance(bm_geom, bmesh.types.BMEdge):
        if cache.bedge != bm_geom:
            cache.bedge = bm_geom
            cache.v0 = obj_matrix_world * bm_geom.verts[0].co
            cache.v1 = obj_matrix_world * bm_geom.verts[1].co
            cache.vmid = 0.5 * (cache.v0 + cache.v1)
            cache.v2d0 = location_3d_to_region_2d(region, rv3d, cache.v0)
            cache.v2d1 = location_3d_to_region_2d(region, rv3d, cache.v1)
            cache.v2dmid = location_3d_to_region_2d(region, rv3d, cache.vmid)

            if previous_vert and previous_vert not in bm_geom.verts:
                pvert_co = obj_matrix_world * previous_vert.co
                perp_point = intersect_point_line(pvert_co, cache.v0, cache.v1)
                cache.vperp = perp_point[0]
                # factor = point_perpendicular[1]
                cache.v2dperp = location_3d_to_region_2d(region, rv3d, perp_point[0])

            # else: cache.v2dperp = None

        if constrain:
            location = intersect_line_line(constrain[0], constrain[1], cache.v0, cache.v1)
            if location is None:
                is_increment = True
                orig, view_vector = region_2d_to_orig_and_view_vector(region, rv3d, mcursor)
                end = orig + view_vector
                location = intersect_line_line(constrain[0], constrain[1], orig, end)
            r_loc = location[0]

        elif cache.v2dperp and \
          abs(cache.v2dperp[0] - mcursor[0]) < 10 and abs(cache.v2dperp[1] - mcursor[1]) < 10:
            r_type = 'PERPENDICULAR'
            r_loc = cache.vperp

        elif abs(cache.v2dmid[0] - mcursor[0]) < 10 and abs(cache.v2dmid[1] - mcursor[1]) < 10:
            r_type = 'CENTER'
            r_loc = cache.vmid

        else:
            if increment and previous_vert in cache.bedge.verts:
                is_increment = True

            r_type = 'EDGE'
            orig, view_vector = region_2d_to_orig_and_view_vector(region, rv3d, mcursor)
            r_loc = intersect_line_line(cache.v0, cache.v1, orig, orig + view_vector)[0]

    elif isinstance(bm_geom, bmesh.types.BMFace):
        is_increment = True
        r_type = 'FACE'

        if cache.bface != bm_geom:
            cache.bface = bm_geom
            cache.fmid = obj_matrix_world * bm_geom.calc_center_median()
            cache.fnor = bm_geom.normal * obj_matrix_world

        orig, view_vector = region_2d_to_orig_and_view_vector(region, rv3d, mcursor)
        end = orig + view_vector
        r_loc = intersect_line_plane(orig, end, cache.fmid, cache.fnor, False)

        if constrain:
            is_increment = False
            r_loc = intersect_point_line(r_loc, constrain[0], constrain[1])[0]

    else:  # OUT
        is_increment = True
        r_type = 'OUT'

        orig, view_vector = region_2d_to_orig_and_view_vector(region, rv3d, mcursor)

        face_index = -1
        if cache.out_obj is None:
            ## TODO: The Scene.raycast also tests the edited object.
            #  This is highly inefficient because the edited object
            #  does not keep DerivedMesh which forces rebuilding the bvhtree.
            result, r_loc, normal, face_index, cache.out_obj, cache.out_obmat = scene.ray_cast(orig, view_vector)
            if result:
                r_type = 'FACE'
                cache.out_obimat = cache.out_obmat.inverted()
            else:
                face_index = -1
                r_loc = None

        if cache.out_obj and cache.out_obj != ignore_obj:
            if not r_loc or outer_verts:
                location = None
                if face_index == -1:
                    # get the ray relative to the cache.out_obj
                    ray_origin_obj = cache.out_obimat * orig
                    end = orig + view_vector * 1000
                    ray_target_obj = cache.out_obimat * end
                    result, location, normal, face_index = cache.out_obj.ray_cast(ray_origin_obj, ray_target_obj)

                if face_index == -1:
                    cache.out_obj = None

                elif outer_verts:
                    vloc = None
                    try:
                        me = cache.out_obj.data
                        verts = me.polygons[face_index].vertices
                        v_dist = 100

                        for i in verts:
                            v_co = cache.out_obmat * me.vertices[i].co
                            v_2d = location_3d_to_region_2d(region, rv3d, v_co)
                            dist = (Vector(mcursor) - v_2d.xy).length_squared
                            if dist < v_dist:
                                v_dist = dist
                                vloc = v_co
                    except Exception as e:
                        print('Fail', e)

                    if vloc:
                        is_increment = False
                        r_type = 'VERT'
                        r_loc = vloc

                if not r_loc:
                    r_type = 'FACE'
                    r_loc = cache.out_obmat * location

        if constrain:
            if r_loc:
                is_increment = False
                r_loc = intersect_point_line(r_loc, constrain[0], constrain[1])[0]
            else:
                r_loc = intersect_line_line(constrain[0], constrain[1], orig, orig + view_vector)[0]

        elif not r_loc:
            r_loc = out_Location(rv3d, region, orig, view_vector)

    if previous_vert:
        pv_co = obj_matrix_world * previous_vert.co
        vec = r_loc - pv_co
        if is_increment and increment:
            r_len = round((1 / increment) * vec.length) * increment
            r_loc = r_len * vec.normalized() + pv_co
        else:
            r_len = vec.length

    return r_loc, r_type, bm_geom, r_len


def get_loose_linked_edges(bmvert):
    linked = [e for e in bmvert.link_edges if not e.link_faces]
    for e in linked:
        linked += [le for v in e.verts if not v.link_faces for le in v.link_edges if le not in linked]
    return linked


def draw_line(self, obj, bm, bm_geom, location):
    if not hasattr(self, 'list_verts'):
        self.list_verts = []

    if not hasattr(self, 'list_edges'):
        self.list_edges = []

    split_faces = set()

    update_edit_mesh = False
    tessface = False

    if bm_geom is None:
        vert = bm.verts.new(location)
        self.list_verts.append(vert)

    elif isinstance(bm_geom, bmesh.types.BMVert):
        if (bm_geom.co - location).length_squared < .001:
            if self.list_verts == [] or self.list_verts[-1] != bm_geom:
                self.list_verts.append(bm_geom)
        else:
            vert = bm.verts.new(location)
            self.list_verts.append(vert)

    elif isinstance(bm_geom, bmesh.types.BMEdge):
        self.list_edges.append(bm_geom)
        ret = intersect_point_line(location, bm_geom.verts[0].co, bm_geom.verts[1].co)

        if (ret[0] - location).length_squared < .001:
            if ret[1] == 0.0:
                vert = bm_geom.verts[0]
            elif ret[1] == 1.0:
                vert = bm_geom.verts[1]
            else:
                edge, vert = bmesh.utils.edge_split(bm_geom, bm_geom.verts[0], ret[1])
            self.list_verts.append(vert)
            # self.list_edges.append(edge)

        else:  # constrain point is near
            vert = bm.verts.new(location)
            self.list_verts.append(vert)

    elif isinstance(bm_geom, bmesh.types.BMFace):
        split_faces.add(bm_geom)
        vert = bm.verts.new(location)
        self.list_verts.append(vert)

    # draw, split and create face
    if len(self.list_verts) >= 2:
        v1, v2 = self.list_verts[-2:]
        # v2_link_verts = [x for y in [a.verts for a in v2.link_edges] for x in y if x != v2]
        for edge in v2.link_edges:
            if v1 in edge.verts:
                self.list_edges.append(edge)
                break
        else:  # if v1 not in v2_link_verts:
            if not v2.link_edges:
                edge = bm.edges.new([v1, v2])
                self.list_edges.append(edge)
            else:  # split face
                if v1.link_faces and v2.link_faces:
                    split_faces.update({f for f in v2.link_faces if f in v1.link_faces})

                else:
                    if v1.link_faces:
                        faces = v1.link_faces
                        co2 = v2.co.copy()
                    else:
                        faces = v2.link_faces
                        co2 = v1.co.copy()

                    for face in faces:
                        if bmesh.geometry.intersect_face_point(face, co2):
                            co = co2 - face.calc_center_median()
                            if co.dot(face.normal) < 0.001:
                                split_faces.add(face)

                if split_faces:
                    edge = bm.edges.new([v1, v2])
                    self.list_edges.append(edge)
                    ed_list = get_loose_linked_edges(v2)
                    for face in split_faces:
                        facesp = bmesh.utils.face_split_edgenet(face, ed_list)
                    del split_faces
                    update_edit_mesh = True
                    tessface = True
                else:
                    if self.intersect:
                        facesp = bmesh.ops.connect_vert_pair(bm, verts=[v1, v2], verts_exclude=bm.verts)
                        # print(facesp)
                    if not self.intersect or not facesp['edges']:
                        edge = bm.edges.new([v1, v2])
                        self.list_edges.append(edge)
                    else:
                        for edge in facesp['edges']:
                            self.list_edges.append(edge)

        # create face
        if self.create_face:
            ed_list = set(self.list_edges)
            for edge in v2.link_edges:
                for vert in edge.verts:
                    if vert != v2 and vert in self.list_verts:
                        ed_list.add(edge)
                        break
                else:
                    continue
                # Inner loop was broken, break the outer
                break

            ed_list.update(get_loose_linked_edges(v2))

            bmesh.ops.edgenet_fill(bm, edges=list(ed_list))
            update_edit_mesh = True
            tessface = True
            # print('face created')
    if update_edit_mesh:
        bmesh.update_edit_mesh(obj.data, tessface = tessface, destructive=True)
    return [obj.matrix_world * v.co for v in self.list_verts]


class NavigationKeys:
    def __init__(self, context):
        # TO DO:
        # 'View Orbit', 'View Pan', 'NDOF Orbit View', 'NDOF Pan View'
        self._rotate = set()
        self._move = set()
        self._zoom = set()
        for key in context.window_manager.keyconfigs.user.keymaps['3D View'].keymap_items:
            if key.idname == 'view3d.rotate':
                #self.keys_rotate[key.id]={'Alt': key.alt, 'Ctrl': key.ctrl, 'Shift':key.shift, 'Type':key.type, 'Value':key.value}
                self._rotate.add((key.alt, key.ctrl, key.shift, key.type, key.value))
            if key.idname == 'view3d.move':
                self._move.add((key.alt, key.ctrl, key.shift, key.type, key.value))
            if key.idname == 'view3d.zoom':
                if key.type == 'WHEELINMOUSE':
                    self._zoom.add((key.alt, key.ctrl, key.shift, 'WHEELUPMOUSE', key.value, key.properties.delta))
                elif key.type == 'WHEELOUTMOUSE':
                    self._zoom.add((key.alt, key.ctrl, key.shift, 'WHEELDOWNMOUSE', key.value, key.properties.delta))
                else:
                    self._zoom.add((key.alt, key.ctrl, key.shift, key.type, key.value, key.properties.delta))


class CharMap:
    ascii = {
        ".", ",", "-", "+", "1", "2", "3",
        "4", "5", "6", "7", "8", "9", "0",
        "c", "m", "d", "k", "h", "a",
        " ", "/", "*", "'", "\""
        # "="
        }
    type = {
        'BACK_SPACE', 'DEL',
        'LEFT_ARROW', 'RIGHT_ARROW'
        }

    @staticmethod
    def modal(self, context, event):
        c = event.ascii
        if c:
            if c == ",":
                c = "."
            self.length_entered = self.length_entered[:self.line_pos] + c + self.length_entered[self.line_pos:]
            self.line_pos += 1
        if self.length_entered:
            if event.type == 'BACK_SPACE':
                self.length_entered = self.length_entered[:self.line_pos - 1] + self.length_entered[self.line_pos:]
                self.line_pos -= 1

            elif event.type == 'DEL':
                self.length_entered = self.length_entered[:self.line_pos] + self.length_entered[self.line_pos + 1:]

            elif event.type == 'LEFT_ARROW':
                self.line_pos = (self.line_pos - 1) % (len(self.length_entered) + 1)

            elif event.type == 'RIGHT_ARROW':
                self.line_pos = (self.line_pos + 1) % (len(self.length_entered) + 1)


class SnapUtilitiesLine(Operator):
    bl_idname = "mesh.snap_utilities_line"
    bl_label = "Line Tool"
    bl_description = "Draw edges. Connect them to split faces"
    bl_options = {'REGISTER', 'UNDO'}

    constrain_keys = {
        'X': Vector((1, 0, 0)),
        'Y': Vector((0, 1, 0)),
        'Z': Vector((0, 0, 1)),
        'RIGHT_SHIFT': 'shift',
        'LEFT_SHIFT': 'shift',
        }

    @classmethod
    def poll(cls, context):
        preferences = context.user_preferences.addons[__name__].preferences
        return (context.mode in {'EDIT_MESH', 'OBJECT'} and
                preferences.create_new_obj or
                (context.object is not None and
                context.object.type == 'MESH'))


    def draw_callback_px(self, context):
        # draw 3d point OpenGL in the 3D View
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glDisable(bgl.GL_DEPTH_TEST)
        # bgl.glPushMatrix()
        # bgl.glMultMatrixf(self.obj_glmatrix)

        if self.vector_constrain:
            vc = self.vector_constrain
            if hasattr(self, 'preloc') and self.type in {'VERT', 'FACE'}:
                bgl.glColor4f(1.0, 1.0, 1.0, 0.5)
                bgl.glPointSize(5)
                bgl.glBegin(bgl.GL_POINTS)
                bgl.glVertex3f(*self.preloc)
                bgl.glEnd()
            if vc[2] == 'X':
                Color4f = (self.axis_x_color + (1.0,))
            elif vc[2] == 'Y':
                Color4f = (self.axis_y_color + (1.0,))
            elif vc[2] == 'Z':
                Color4f = (self.axis_z_color + (1.0,))
            else:
                Color4f = self.constrain_shift_color
        else:
            if self.type == 'OUT':
                Color4f = self.out_color
            elif self.type == 'FACE':
                Color4f = self.face_color
            elif self.type == 'EDGE':
                Color4f = self.edge_color
            elif self.type == 'VERT':
                Color4f = self.vert_color
            elif self.type == 'CENTER':
                Color4f = self.center_color
            elif self.type == 'PERPENDICULAR':
                Color4f = self.perpendicular_color
            else: # self.type == None
                Color4f = self.out_color

        bgl.glColor4f(*Color4f)
        bgl.glPointSize(10)
        bgl.glBegin(bgl.GL_POINTS)
        bgl.glVertex3f(*self.location)
        bgl.glEnd()

        # draw 3d line OpenGL in the 3D View
        bgl.glEnable(bgl.GL_DEPTH_TEST)
        bgl.glDepthRange(0, 0.9999)
        bgl.glColor4f(1.0, 0.8, 0.0, 1.0)
        bgl.glLineWidth(2)
        bgl.glEnable(bgl.GL_LINE_STIPPLE)
        bgl.glBegin(bgl.GL_LINE_STRIP)
        for vert_co in self.list_verts_co:
            bgl.glVertex3f(*vert_co)
        bgl.glVertex3f(*self.location)
        bgl.glEnd()

        # restore opengl defaults
        # bgl.glPopMatrix()
        bgl.glDepthRange(0, 1)
        bgl.glPointSize(1)
        bgl.glLineWidth(1)
        bgl.glDisable(bgl.GL_BLEND)
        bgl.glDisable(bgl.GL_LINE_STIPPLE)
        bgl.glColor4f(0.0, 0.0, 0.0, 1.0)


    def modal_navigation(self, context, event):
        evkey = (event.alt, event.ctrl, event.shift, event.type, event.value)
        if evkey in self.navigation_keys._rotate:
            bpy.ops.view3d.rotate('INVOKE_DEFAULT')
            return True
        elif evkey in self.navigation_keys._move:
            if event.shift and self.vector_constrain and \
                self.vector_constrain[2] in {'RIGHT_SHIFT', 'LEFT_SHIFT', 'shift'}:
                self.vector_constrain = None
            bpy.ops.view3d.move('INVOKE_DEFAULT')
            return True
        else:
            for key in self.navigation_keys._zoom:
                if evkey == key[0:5]:
                    if True: #  TODO: Use Zoom to mouse position
                        v3d = context.space_data
                        dist_range = (v3d.clip_start, v3d.clip_end)
                        rv3d = context.region_data
                        if (key[5] < 0 and rv3d.view_distance < dist_range[1]) or\
                           (key[5] > 0 and rv3d.view_distance > dist_range[0]):
                                rv3d.view_location += key[5] * (self.location - rv3d.view_location) / 6
                                rv3d.view_distance -= key[5] * rv3d.view_distance / 6
                    else:
                        bpy.ops.view3d.zoom('INVOKE_DEFAULT', delta = key[5])
                    return True
                    #break

        return False


    def modal(self, context, event):
        if self.modal_navigation(context, event):
            return {'RUNNING_MODAL'}

        context.area.tag_redraw()

        if event.ctrl and event.type == 'Z' and event.value == 'PRESS':
            bpy.ops.ed.undo()
            self.vector_constrain = None
            self.list_verts_co = []
            self.list_verts = []
            self.list_edges = []
            self.list_faces = []
            self.obj = bpy.context.active_object
            self.obj_matrix = self.obj.matrix_world.copy()
            self.bm = bmesh.from_edit_mesh(self.obj.data)
            return {'RUNNING_MODAL'}

        if event.type == 'MOUSEMOVE' or self.bool_update:
            if self.rv3d.view_matrix != self.rotMat:
                self.rotMat = self.rv3d.view_matrix.copy()
                self.bool_update = True
                self.cache.bedge = None
            else:
                self.bool_update = False

            mval = Vector((event.mouse_region_x, event.mouse_region_y))

            self.location, self.type, self.geom, self.len = snap_utilities(
                    self.cache, context, self.obj_matrix,
                    self.bm, mval,
                    outer_verts=(self.outer_verts and not self.keytab),
                    constrain=self.vector_constrain,
                    previous_vert=(self.list_verts[-1] if self.list_verts else None),
                    ignore_obj=self.obj,
                    increment=self.incremental
            )
            if self.snap_to_grid and self.type == 'OUT':
                loc = self.location / self.rd
                self.location = Vector((round(loc.x),
                                        round(loc.y),
                                        round(loc.z))) * self.rd

            if self.keyf8 and self.list_verts_co:
                lloc = self.list_verts_co[-1]
                orig, view_vec = region_2d_to_orig_and_view_vector(self.region, self.rv3d, mval)
                location = intersect_point_line(lloc, orig, (orig + view_vec))
                vec = (location[0] - lloc)
                ax, ay, az = abs(vec.x), abs(vec.y), abs(vec.z)
                vec.x = ax > ay > az or ax > az > ay
                vec.y = ay > ax > az or ay > az > ax
                vec.z = az > ay > ax or az > ax > ay
                if vec == Vector():
                    self.vector_constrain = None
                else:
                    vc = lloc + vec
                    try:
                        if vc != self.vector_constrain[1]:
                            type = 'X' if vec.x else 'Y' if vec.y else 'Z' if vec.z else 'shift'
                            self.vector_constrain = [lloc, vc, type]
                    except:
                        type = 'X' if vec.x else 'Y' if vec.y else 'Z' if vec.z else 'shift'
                        self.vector_constrain = [lloc, vc, type]

        if event.value == 'PRESS':
            if self.list_verts_co and (event.ascii in CharMap.ascii or event.type in CharMap.type):
                CharMap.modal(self, context, event)

            elif event.type in self.constrain_keys:
                self.bool_update = True
                if self.vector_constrain and self.vector_constrain[2] == event.type:
                    self.vector_constrain = ()

                else:
                    if event.shift:
                        if isinstance(self.geom, bmesh.types.BMEdge):
                            if self.list_verts:
                                loc = self.list_verts_co[-1]
                                self.vector_constrain = (loc, loc + self.geom.verts[1].co -
                                                         self.geom.verts[0].co, event.type)
                            else:
                                self.vector_constrain = [self.obj_matrix * v.co for
                                                         v in self.geom.verts] + [event.type]
                    else:
                        if self.list_verts:
                            loc = self.list_verts_co[-1]
                        else:
                            loc = self.location
                        self.vector_constrain = [loc, loc + self.constrain_keys[event.type]] + [event.type]

            elif event.type == 'LEFTMOUSE':
                point = self.obj_matinv * self.location
                # with constraint the intersection can be in a different element of the selected one
                if self.vector_constrain and self.geom:
                    geom2 = get_closest_edge(self.bm, point, .001)
                else:
                    geom2 = self.geom

                self.vector_constrain = None
                self.list_verts_co = draw_line(self, self.obj, self.bm, geom2, point)
                bpy.ops.ed.undo_push(message="Undo draw line*")

            elif event.type == 'TAB':
                self.keytab = self.keytab is False
                if self.keytab:
                    context.tool_settings.mesh_select_mode = (False, False, True)
                else:
                    context.tool_settings.mesh_select_mode = (True, True, True)

            elif event.type == 'F8':
                self.vector_constrain = None
                self.keyf8 = self.keyf8 is False

        elif event.value == 'RELEASE':
            if event.type in {'RET', 'NUMPAD_ENTER'}:
                if self.length_entered != "" and self.list_verts_co:
                    try:
                        text_value = bpy.utils.units.to_value(self.unit_system, 'LENGTH', self.length_entered)
                        vector = (self.location - self.list_verts_co[-1]).normalized()
                        location = (self.list_verts_co[-1] + (vector * text_value))
                        G_location = self.obj_matinv * location
                        self.list_verts_co = draw_line(self, self.obj, self.bm, self.geom, G_location)
                        self.length_entered = ""
                        self.vector_constrain = None

                    except:  # ValueError:
                        self.report({'INFO'}, "Operation not supported yet")

            elif event.type in {'RIGHTMOUSE', 'ESC'}:
                if self.list_verts_co == [] or event.type == 'ESC':
                    bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                    context.tool_settings.mesh_select_mode = self.select_mode
                    context.area.header_text_set()
                    context.user_preferences.view.use_rotate_around_active = self.use_rotate_around_active
                    if not self.is_editmode:
                        bpy.ops.object.editmode_toggle()
                    return {'FINISHED'}
                else:
                    self.vector_constrain = None
                    self.list_verts = []
                    self.list_verts_co = []
                    self.list_faces = []

        a = ""
        if self.list_verts_co:
            if self.length_entered:
                pos = self.line_pos
                a = 'length: ' + self.length_entered[:pos] + '|' + self.length_entered[pos:]
            else:
                length = self.len
                length = convert_distance(length, self.uinfo)
                a = 'length: ' + length
        context.area.header_text_set(
                        "hit: %.3f %.3f %.3f %s" % (self.location[0],
                        self.location[1], self.location[2], a)
                        )

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.space_data.type == 'VIEW_3D':
            # print('name', __name__, __package__)
            preferences = context.user_preferences.addons[__name__].preferences
            create_new_obj = preferences.create_new_obj
            if context.mode == 'OBJECT' and \
              (create_new_obj or context.object is None or context.object.type != 'MESH'):

                mesh = bpy.data.meshes.new("")
                obj = bpy.data.objects.new("", mesh)
                context.scene.objects.link(obj)
                context.scene.objects.active = obj

            # bgl.glEnable(bgl.GL_POINT_SMOOTH)
            self.is_editmode = bpy.context.object.data.is_editmode
            bpy.ops.object.mode_set(mode='EDIT')
            context.space_data.use_occlude_geometry = True

            self.scale = context.scene.unit_settings.scale_length
            self.unit_system = context.scene.unit_settings.system
            self.separate_units = context.scene.unit_settings.use_separate
            self.uinfo = get_units_info(self.scale, self.unit_system, self.separate_units)

            grid = context.scene.unit_settings.scale_length / context.space_data.grid_scale
            relative_scale = preferences.relative_scale
            self.scale = grid / relative_scale
            self.rd = bpy.utils.units.to_value(self.unit_system, 'LENGTH', str(1 / self.scale))

            incremental = preferences.incremental
            self.incremental = bpy.utils.units.to_value(self.unit_system, 'LENGTH', str(incremental))

            self.use_rotate_around_active = context.user_preferences.view.use_rotate_around_active
            context.user_preferences.view.use_rotate_around_active = True

            self.select_mode = context.tool_settings.mesh_select_mode[:]
            context.tool_settings.mesh_select_mode = (True, True, True)

            self.region = context.region
            self.rv3d = context.region_data
            self.rotMat = self.rv3d.view_matrix.copy()
            self.obj = bpy.context.active_object
            self.obj_matrix = self.obj.matrix_world.copy()
            self.obj_matinv = self.obj_matrix.inverted()
            # self.obj_glmatrix = bgl.Buffer(bgl.GL_FLOAT, [4, 4], self.obj_matrix.transposed())
            self.bm = bmesh.from_edit_mesh(self.obj.data)
            self.cache = SnapCache()

            self.location = Vector()
            self.list_verts = []
            self.list_verts_co = []
            self.bool_update = False
            self.vector_constrain = ()
            self.navigation_keys = NavigationKeys(context)
            self.keytab = False
            self.keyf8 = False
            self.type = 'OUT'
            self.len = 0
            self.length_entered = ""
            self.line_pos = 0

            self.out_color = preferences.out_color
            self.face_color = preferences.face_color
            self.edge_color = preferences.edge_color
            self.vert_color = preferences.vert_color
            self.center_color = preferences.center_color
            self.perpendicular_color = preferences.perpendicular_color
            self.constrain_shift_color = preferences.constrain_shift_color

            self.axis_x_color = tuple(context.user_preferences.themes[0].user_interface.axis_x)
            self.axis_y_color = tuple(context.user_preferences.themes[0].user_interface.axis_y)
            self.axis_z_color = tuple(context.user_preferences.themes[0].user_interface.axis_z)

            self.intersect = preferences.intersect
            self.create_face = preferences.create_face
            self.outer_verts = preferences.outer_verts
            self.snap_to_grid = preferences.increments_grid

            self._handle = bpy.types.SpaceView3D.draw_handler_add(
                                                    self.draw_callback_px,
                                                    (context,),
                                                    'WINDOW', 'POST_VIEW'
                                                    )
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Active space must be a View3d")
            return {'CANCELLED'}


class PanelSnapUtilities(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Snap Utilities"
    bl_label = "Snap Utilities"

    @classmethod
    def poll(cls, context):
        preferences = context.user_preferences.addons[__name__].preferences
        return (context.mode in {'EDIT_MESH', 'OBJECT'} and
                preferences.create_new_obj or
                (context.object is not None and
                context.object.type == 'MESH'))

    def draw(self, context):
        layout = self.layout
        TheCol = layout.column(align=True)
        TheCol.operator("mesh.snap_utilities_line", text="Line", icon="GREASEPENCIL")

        addon_prefs = context.user_preferences.addons[__name__].preferences
        expand = addon_prefs.expand_snap_settings
        icon = "TRIA_DOWN" if expand else "TRIA_RIGHT"

        box = layout.box()
        box.prop(addon_prefs, "expand_snap_settings", icon=icon,
            text="Settings:", emboss=False)
        if expand:
            box.prop(addon_prefs, "outer_verts")
            box.prop(addon_prefs, "incremental")
            box.prop(addon_prefs, "increments_grid")
            if addon_prefs.increments_grid:
                box.prop(addon_prefs, "relative_scale")
            box.label(text="Line Tool:")
            box.prop(addon_prefs, "intersect")
            box.prop(addon_prefs, "create_face")
            box.prop(addon_prefs, "create_new_obj")


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        PanelSnapUtilities,
        )


def update_panel(self, context):
    message = "Snap Utilities Line: Updating Panel locations has failed"
    addon_prefs = context.user_preferences.addons[__name__].preferences
    try:
        for panel in panels:
            if addon_prefs.category != panel.bl_category:
                if "bl_rna" in panel.__dict__:
                    bpy.utils.unregister_class(panel)

                panel.bl_category = addon_prefs.category
                bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class SnapAddonPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    intersect = BoolProperty(
            name="Intersect",
            description="Intersects created line with the existing edges, "
                        "even if the lines do not intersect",
            default=True
            )
    create_new_obj = BoolProperty(
            name="Create a new object",
            description="If have not a active object, or the active object "
                        "is not in edit mode, it creates a new object",
            default=False
            )
    create_face = BoolProperty(
            name="Create faces",
            description="Create faces defined by enclosed edges",
            default=False
            )
    outer_verts = BoolProperty(
            name="Snap to outer vertices",
            description="The vertices of the objects are not activated also snapped",
            default=True
            )
    expand_snap_settings = BoolProperty(
            name="Expand",
            description="Expand, to display the settings",
            default=False
            )
    expand_color_settings = BoolProperty(
            name="Color Settings",
            description="Expand, to display the color settings",
            default=False
            )
    increments_grid = BoolProperty(
            name="Increments of Grid",
            description="Snap to increments of grid",
            default=False
            )
    category = StringProperty(
            name="Category",
            description="Choose a name for the category of the panel",
            default="Tools",
            update=update_panel
            )
    incremental = FloatProperty(
            name="Incremental",
            description="Snap in defined increments",
            default=0,
            min=0,
            step=1,
            precision=3
            )
    relative_scale = FloatProperty(
            name="Relative Scale",
            description="Value that divides the global scale",
            default=1,
            min=0,
            step=1,
            precision=3
            )
    out_color = FloatVectorProperty(
            name="OUT",
            default=(0.0, 0.0, 0.0, 0.5),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    face_color = FloatVectorProperty(
            name="FACE",
            default=(1.0, 0.8, 0.0, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    edge_color = FloatVectorProperty(
            name="EDGE",
            default=(0.0, 0.8, 1.0, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    vert_color = FloatVectorProperty(
            name="VERT",
            default=(1.0, 0.5, 0.0, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    center_color = FloatVectorProperty(
            name="CENTER",
            default=(1.0, 0.0, 1.0, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    perpendicular_color = FloatVectorProperty(
            name="PERPENDICULAR",
            default=(0.1, 0.5, 0.5, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )
    constrain_shift_color = FloatVectorProperty(
            name="SHIFT CONSTRAIN",
            default=(0.8, 0.5, 0.4, 1.0),
            size=4,
            subtype="COLOR",
            min=0, max=1
            )

    def draw(self, context):
        layout = self.layout
        icon = "TRIA_DOWN" if self.expand_color_settings else "TRIA_RIGHT"

        box = layout.box()
        box.prop(self, "expand_color_settings", icon=icon, toggle=True, emboss=False)
        if self.expand_color_settings:
            split = box.split()

            col = split.column()
            col.prop(self, "out_color")
            col.prop(self, "constrain_shift_color")
            col = split.column()
            col.prop(self, "face_color")
            col.prop(self, "center_color")
            col = split.column()
            col.prop(self, "edge_color")
            col.prop(self, "perpendicular_color")
            col = split.column()
            col.prop(self, "vert_color")

        row = layout.row()

        col = row.column(align=True)
        box = col.box()
        box.label(text="Snap Items:")
        box.prop(self, "incremental")
        box.prop(self, "outer_verts")
        box.prop(self, "increments_grid")
        if self.increments_grid:
            box.prop(self, "relative_scale")
        else:
            box.separator()
            box.separator()

        col = row.column(align=True)
        box = col.box()
        box.label(text="Line Tool:")
        box.prop(self, "intersect")
        box.prop(self, "create_face")
        box.prop(self, "create_new_obj")
        box.separator()
        box.separator()

        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


def register():
    bpy.utils.register_class(SnapAddonPreferences)
    bpy.utils.register_class(SnapUtilitiesLine)
    bpy.utils.register_class(PanelSnapUtilities)
    update_panel(None, bpy.context)


def unregister():
    bpy.utils.unregister_class(PanelSnapUtilities)
    bpy.utils.unregister_class(SnapUtilitiesLine)
    bpy.utils.unregister_class(SnapAddonPreferences)


if __name__ == "__main__":
    __name__ = "mesh_snap_utilities_line"
    register()
