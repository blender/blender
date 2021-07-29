# -*- coding:utf-8 -*-

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

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
import bpy
from bpy.types import Operator
from bpy.props import EnumProperty
from mathutils import Vector


class ArchipackBoolManager():
    """
        Handle three methods for booleans
        - interactive: one modifier for each hole right on wall
        - robust: one single modifier on wall and merge holes in one mesh
        - mixed: merge holes with boolean and use result on wall
            may be slow, but is robust
    """
    def __init__(self, mode, solver_mode='CARVE'):
        """
            mode in 'ROBUST', 'INTERACTIVE', 'HYBRID'
        """
        self.mode = mode
        self.solver_mode = solver_mode
        # internal variables
        self.itM = None
        self.min_x = 0
        self.min_y = 0
        self.min_z = 0
        self.max_x = 0
        self.max_y = 0
        self.max_z = 0

    def _get_bounding_box(self, wall):
        self.itM = wall.matrix_world.inverted()
        x, y, z = wall.bound_box[0]
        self.min_x = x
        self.min_y = y
        self.min_z = z
        x, y, z = wall.bound_box[6]
        self.max_x = x
        self.max_y = y
        self.max_z = z
        self.center = Vector((
            self.min_x + 0.5 * (self.max_x - self.min_x),
            self.min_y + 0.5 * (self.max_y - self.min_y),
            self.min_z + 0.5 * (self.max_z - self.min_z)))

    def _contains(self, pt):
        p = self.itM * pt
        return (p.x >= self.min_x and p.x <= self.max_x and
            p.y >= self.min_y and p.y <= self.max_y and
            p.z >= self.min_z and p.z <= self.max_z)

    def filter_wall(self, wall):
        d = wall.data
        return (d is None or
               'archipack_window' in d or
               'archipack_window_panel' in d or
               'archipack_door' in d or
               'archipack_doorpanel' in d or
               'archipack_hole' in wall or
               'archipack_robusthole' in wall or
               'archipack_handle' in wall)

    def datablock(self, o):
        """
            get datablock from windows and doors
            return
                datablock if found
                None when not found
        """
        d = None
        if o.data is None:
            return
        if "archipack_window" in o.data:
            d = o.data.archipack_window[0]
        elif "archipack_door" in o.data:
            d = o.data.archipack_door[0]
        return d

    def prepare_hole(self, hole):
        hole.lock_location = (True, True, True)
        hole.lock_rotation = (True, True, True)
        hole.lock_scale = (True, True, True)
        hole.draw_type = 'WIRE'
        hole.hide_render = True
        hole.hide_select = True
        hole.select = True
        hole.cycles_visibility.camera = False
        hole.cycles_visibility.diffuse = False
        hole.cycles_visibility.glossy = False
        hole.cycles_visibility.shadow = False
        hole.cycles_visibility.scatter = False
        hole.cycles_visibility.transmission = False

    def get_child_hole(self, o):
        for hole in o.children:
            if "archipack_hole" in hole:
                return hole
        return None

    def _generate_hole(self, context, o):
        # use existing one
        if self.mode != 'ROBUST':
            hole = self.get_child_hole(o)
            if hole is not None:
                # print("_generate_hole Use existing hole %s" % (hole.name))
                return hole
        # generate single hole from archipack primitives
        d = self.datablock(o)
        hole = None
        if d is not None:
            if (self.itM is not None and (
                    self._contains(o.location) or
                    self._contains(o.matrix_world * Vector((0, 0, 0.5 * d.z))))
                    ):
                if self.mode != 'ROBUST':
                    hole = d.interactive_hole(context, o)
                else:
                    hole = d.robust_hole(context, o.matrix_world)
                # print("_generate_hole Generate hole %s" % (hole.name))
            else:
                hole = d.interactive_hole(context, o)
        return hole

    def partition(self, array, begin, end):
        pivot = begin
        for i in range(begin + 1, end + 1):
            if array[i][1] <= array[begin][1]:
                pivot += 1
                array[i], array[pivot] = array[pivot], array[i]
        array[pivot], array[begin] = array[begin], array[pivot]
        return pivot

    def quicksort(self, array, begin=0, end=None):
        if end is None:
            end = len(array) - 1

        def _quicksort(array, begin, end):
            if begin >= end:
                return
            pivot = self.partition(array, begin, end)
            _quicksort(array, begin, pivot - 1)
            _quicksort(array, pivot + 1, end)
        return _quicksort(array, begin, end)

    def sort_holes(self, wall, holes):
        """
            sort hole from center to borders by distance from center
            may improve nested booleans
        """
        center = wall.matrix_world * self.center
        holes = [(o, (o.matrix_world.translation - center).length) for o in holes]
        self.quicksort(holes)
        return [o[0] for o in holes]

    def difference(self, basis, hole, solver=None):
        # print("difference %s" % (hole.name))
        m = basis.modifiers.new('AutoBoolean', 'BOOLEAN')
        m.operation = 'DIFFERENCE'
        if solver is None:
            m.solver = self.solver_mode
        else:
            m.solver = solver
        m.object = hole

    def union(self, basis, hole):
        # print("union %s" % (hole.name))
        m = basis.modifiers.new('AutoMerge', 'BOOLEAN')
        m.operation = 'UNION'
        m.solver = self.solver_mode
        m.object = hole

    def remove_modif_and_object(self, context, o, to_delete):
        # print("remove_modif_and_object removed:%s" % (len(to_delete)))
        for m, h in to_delete:
            if m is not None:
                if m.object is not None:
                    m.object = None
                o.modifiers.remove(m)
            if h is not None:
                context.scene.objects.unlink(h)
                bpy.data.objects.remove(h, do_unlink=True)

    # Mixed
    def create_merge_basis(self, context, wall):
        # print("create_merge_basis")
        h = bpy.data.meshes.new("AutoBoolean")
        hole_obj = bpy.data.objects.new("AutoBoolean", h)
        context.scene.objects.link(hole_obj)
        hole_obj['archipack_hybridhole'] = True
        if wall.parent is not None:
            hole_obj.parent = wall.parent
        hole_obj.matrix_world = wall.matrix_world.copy()
        for mat in wall.data.materials:
            hole_obj.data.materials.append(mat)
        # MaterialUtils.add_wall2_materials(hole_obj)
        return hole_obj

    def update_hybrid(self, context, wall, childs, holes):
        """
            Update all holes modifiers
            remove holes not found in childs

            robust -> mixed:
                there is only one object taged with "archipack_robusthole"
            interactive -> mixed:
                many modifisers on wall taged with "archipack_hole"
                keep objects
        """
        existing = []
        to_delete = []

        # robust/interactive -> mixed
        for m in wall.modifiers:
            if m.type == 'BOOLEAN':
                if m.object is None:
                    to_delete.append([m, None])
                elif 'archipack_hole' in m.object:
                    h = m.object
                    if h in holes:
                        to_delete.append([m, None])
                    else:
                        to_delete.append([m, h])
                elif 'archipack_robusthole' in m.object:
                    to_delete.append([m, m.object])

        # remove modifier and holes not found in new list
        self.remove_modif_and_object(context, wall, to_delete)

        m = wall.modifiers.get("AutoMixedBoolean")
        if m is None:
            m = wall.modifiers.new('AutoMixedBoolean', 'BOOLEAN')
            m.solver = self.solver_mode
            m.operation = 'DIFFERENCE'

        if m.object is None:
            hole_obj = self.create_merge_basis(context, wall)
        else:
            hole_obj = m.object

        m.object = hole_obj
        self.prepare_hole(hole_obj)

        to_delete = []

        # mixed-> mixed
        for m in hole_obj.modifiers:
            h = m.object
            if h in holes:
                existing.append(h)
            else:
                to_delete.append([m, h])

        # remove modifier and holes not found in new list
        self.remove_modif_and_object(context, hole_obj, to_delete)

        # add modifier and holes not found in existing
        for h in holes:
            if h not in existing:
                self.union(hole_obj, h)

    # Interactive
    def update_interactive(self, context, wall, childs, holes):

        existing = []

        to_delete = []

        hole_obj = None

        # mixed-> interactive
        for m in wall.modifiers:
            if m.type == 'BOOLEAN':
                if m.object is not None and 'archipack_hybridhole' in m.object:
                    hole_obj = m.object
                    break

        if hole_obj is not None:
            for m in hole_obj.modifiers:
                h = m.object
                if h not in holes:
                    to_delete.append([m, h])
            # remove modifier and holes not found in new list
            self.remove_modif_and_object(context, hole_obj, to_delete)
            context.scene.objects.unlink(hole_obj)
            bpy.data.objects.remove(hole_obj, do_unlink=True)

        to_delete = []

        # interactive/robust -> interactive
        for m in wall.modifiers:
            if m.type == 'BOOLEAN':
                if m.object is None:
                    to_delete.append([m, None])
                elif 'archipack_hole' in m.object:
                    h = m.object
                    if h in holes:
                        existing.append(h)
                    else:
                        to_delete.append([m, h])
                elif 'archipack_robusthole' in m.object:
                    to_delete.append([m, m.object])

        # remove modifier and holes not found in new list
        self.remove_modif_and_object(context, wall, to_delete)

        # add modifier and holes not found in existing
        for h in holes:
            if h not in existing:
                self.difference(wall, h)

    # Robust
    def update_robust(self, context, wall, childs):

        modif = None

        to_delete = []

        # robust/interactive/mixed -> robust
        for m in wall.modifiers:
            if m.type == 'BOOLEAN':
                if m.object is None:
                    to_delete.append([m, None])
                elif 'archipack_robusthole' in m.object:
                    modif = m
                    to_delete.append([None, m.object])
                elif 'archipack_hole' in m.object:
                    to_delete.append([m, m.object])
                elif 'archipack_hybridhole' in m.object:
                    to_delete.append([m, m.object])
                    o = m.object
                    for m in o.modifiers:
                        to_delete.append([None, m.object])

        # remove modifier and holes
        self.remove_modif_and_object(context, wall, to_delete)

        if bool(len(context.selected_objects) > 0):
            # more than one hole : join, result becomes context.object
            if len(context.selected_objects) > 1:
                bpy.ops.object.join()
                context.object['archipack_robusthole'] = True

            hole = context.object
            hole.name = 'AutoBoolean'

            childs.append(hole)

            if modif is None:
                self.difference(wall, hole)
            else:
                modif.object = hole
        elif modif is not None:
            wall.modifiers.remove(modif)

    def autoboolean(self, context, wall):
        """
            Entry point for multi-boolean operations like
            in T panel autoBoolean and RobustBoolean buttons
        """

        if wall.data is not None and "archipack_wall2" in wall.data:
            # ensure wall modifier is there before any boolean
            # to support "revival" of applied modifiers
            m = wall.modifiers.get("Wall")
            if m is None:
                wall.select = True
                context.scene.objects.active = wall
                wall.data.archipack_wall2[0].update(context)

        bpy.ops.object.select_all(action='DESELECT')
        context.scene.objects.active = None
        childs = []
        holes = []
        # get wall bounds to find what's inside
        self._get_bounding_box(wall)

        # either generate hole or get existing one
        for o in context.scene.objects:
            h = self._generate_hole(context, o)
            if h is not None:
                holes.append(h)
                childs.append(o)

        self.sort_holes(wall, holes)

        # hole(s) are selected and active after this one
        for hole in holes:
            # copy wall material to hole
            hole.data.materials.clear()
            for mat in wall.data.materials:
                hole.data.materials.append(mat)

            self.prepare_hole(hole)

        # update / remove / add  boolean modifier
        if self.mode == 'INTERACTIVE':
            self.update_interactive(context, wall, childs, holes)
        elif self.mode == 'ROBUST':
            self.update_robust(context, wall, childs)
        else:
            self.update_hybrid(context, wall, childs, holes)

        bpy.ops.object.select_all(action='DESELECT')
        # parenting childs to wall reference point
        if wall.parent is None:
            x, y, z = wall.bound_box[0]
            context.scene.cursor_location = wall.matrix_world * Vector((x, y, z))
            # fix issue #9
            context.scene.objects.active = wall
            bpy.ops.archipack.reference_point()
        else:
            wall.parent.select = True
            context.scene.objects.active = wall.parent

        wall.select = True
        for o in childs:
            if 'archipack_robusthole' in o:
                o.hide_select = False
            o.select = True

        bpy.ops.archipack.parent_to_reference()

        for o in childs:
            if 'archipack_robusthole' in o:
                o.hide_select = True

    def detect_mode(self, context, wall):
        for m in wall.modifiers:
            if m.type == 'BOOLEAN' and m.object is not None:
                if 'archipack_hole' in m.object:
                    self.mode = 'INTERACTIVE'
                if 'archipack_hybridhole' in m.object:
                    self.mode = 'HYBRID'
                if 'archipack_robusthole' in m.object:
                    self.mode = 'ROBUST'

    def singleboolean(self, context, wall, o):
        """
            Entry point for single boolean operations
            in use in draw door and windows over wall
            o is either a window or a door
        """

        # generate holes for crossing window and doors
        self.itM = wall.matrix_world.inverted()
        d = self.datablock(o)

        hole = None
        hole_obj = None
        # default mode defined by __init__
        self.detect_mode(context, wall)

        if d is not None:
            if self.mode != 'ROBUST':
                hole = d.interactive_hole(context, o)
            else:
                hole = d.robust_hole(context, o.matrix_world)
        if hole is None:
            return

        hole.data.materials.clear()
        for mat in wall.data.materials:
            hole.data.materials.append(mat)

        self.prepare_hole(hole)

        if self.mode == 'INTERACTIVE':
            # update / remove / add  boolean modifier
            self.difference(wall, hole)

        elif self.mode == 'HYBRID':
            m = wall.modifiers.get('AutoMixedBoolean')

            if m is None:
                m = wall.modifiers.new('AutoMixedBoolean', 'BOOLEAN')
                m.operation = 'DIFFERENCE'
                m.solver = self.solver_mode

            if m.object is None:
                hole_obj = self.create_merge_basis(context, wall)
                m.object = hole_obj
            else:
                hole_obj = m.object
            self.union(hole_obj, hole)

        bpy.ops.object.select_all(action='DESELECT')

        # parenting childs to wall reference point
        if wall.parent is None:
            x, y, z = wall.bound_box[0]
            context.scene.cursor_location = wall.matrix_world * Vector((x, y, z))
            # fix issue #9
            context.scene.objects.active = wall
            bpy.ops.archipack.reference_point()
        else:
            context.scene.objects.active = wall.parent

        if hole_obj is not None:
            hole_obj.select = True

        wall.select = True
        o.select = True
        bpy.ops.archipack.parent_to_reference()
        wall.select = True
        context.scene.objects.active = wall
        if "archipack_wall2" in wall.data:
            d = wall.data.archipack_wall2[0]
            g = d.get_generator()
            d.setup_childs(wall, g)
            d.relocate_childs(context, wall, g)
        elif "archipack_roof" in wall.data:
            pass
        if hole_obj is not None:
            self.prepare_hole(hole_obj)


class ARCHIPACK_OT_single_boolean(Operator):
    bl_idname = "archipack.single_boolean"
    bl_label = "SingleBoolean"
    bl_description = "Add single boolean for doors and windows"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    mode = EnumProperty(
        name="Mode",
        items=(
            ('INTERACTIVE', 'INTERACTIVE', 'Interactive, fast but may fail', 0),
            ('ROBUST', 'ROBUST', 'Not interactive, robust', 1),
            ('HYBRID', 'HYBRID', 'Interactive, slow but robust', 2)
            ),
        default='HYBRID'
        )
    solver_mode = EnumProperty(
        name="Solver",
        items=(
            ('CARVE', 'CARVE', 'Slow but robust (could be slow in hybrid mode with many holes)', 0),
            ('BMESH', 'BMESH', 'Fast but more prone to errors', 1)
            ),
        default='BMESH'
        )
    """
        Wall must be active object
        window or door must be selected
    """

    @classmethod
    def poll(cls, context):
        w = context.active_object
        return (w.data is not None and
            ("archipack_wall2" in w.data or
            "archipack_wall" in w.data or
            "archipack_roof" in w.data) and
            len(context.selected_objects) == 2
            )

    def draw(self, context):
        pass

    def execute(self, context):
        if context.mode == "OBJECT":
            wall = context.active_object
            manager = ArchipackBoolManager(mode=self.mode, solver_mode=self.solver_mode)
            for o in context.selected_objects:
                if o != wall:
                    manager.singleboolean(context, wall, o)
                    break
            o.select = False
            wall.select = True
            context.scene.objects.active = wall
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_auto_boolean(Operator):
    bl_idname = "archipack.auto_boolean"
    bl_label = "AutoBoolean"
    bl_description = "Automatic boolean for doors and windows"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    mode = EnumProperty(
        name="Mode",
        items=(
            ('INTERACTIVE', 'INTERACTIVE', 'Interactive, fast but may fail', 0),
            ('ROBUST', 'ROBUST', 'Not interactive, robust', 1),
            ('HYBRID', 'HYBRID', 'Interactive, slow but robust', 2)
            ),
        default='HYBRID'
        )
    solver_mode = EnumProperty(
        name="Solver",
        items=(
            ('CARVE', 'CARVE', 'Slow but robust (could be slow in hybrid mode with many holes)', 0),
            ('BMESH', 'BMESH', 'Fast but more prone to errors', 1)
            ),
        default='BMESH'
        )

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.prop(self, 'mode')
        row.prop(self, 'solver_mode')

    def execute(self, context):
        if context.mode == "OBJECT":
            manager = ArchipackBoolManager(mode=self.mode, solver_mode=self.solver_mode)
            active = context.scene.objects.active
            walls = [wall for wall in context.selected_objects if not manager.filter_wall(wall)]
            bpy.ops.object.select_all(action='DESELECT')
            for wall in walls:
                manager.autoboolean(context, wall)
                bpy.ops.object.select_all(action='DESELECT')
                wall.select = True
                context.scene.objects.active = wall
                if wall.data is not None and 'archipack_wall2' in wall.data:
                    bpy.ops.archipack.wall2_manipulate('EXEC_DEFAULT')
            # reselect walls
            bpy.ops.object.select_all(action='DESELECT')
            for wall in walls:
                wall.select = True
            context.scene.objects.active = active
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_generate_hole(Operator):
    bl_idname = "archipack.generate_hole"
    bl_label = "Generate hole"
    bl_description = "Generate interactive hole for doors and windows"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.mode == "OBJECT":
            manager = ArchipackBoolManager(mode='HYBRID')
            o = context.active_object

            d = manager.datablock(o)
            if d is None:
                self.report({'WARNING'}, "Archipack: active object must be a door, a window or a roof")
                return {'CANCELLED'}
            bpy.ops.object.select_all(action='DESELECT')
            o.select = True
            context.scene.objects.active = o
            hole = manager._generate_hole(context, o)
            manager.prepare_hole(hole)
            hole.select = False
            o.select = True
            context.scene.objects.active = o
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


def register():
    bpy.utils.register_class(ARCHIPACK_OT_generate_hole)
    bpy.utils.register_class(ARCHIPACK_OT_single_boolean)
    bpy.utils.register_class(ARCHIPACK_OT_auto_boolean)


def unregister():
    bpy.utils.unregister_class(ARCHIPACK_OT_generate_hole)
    bpy.utils.unregister_class(ARCHIPACK_OT_single_boolean)
    bpy.utils.unregister_class(ARCHIPACK_OT_auto_boolean)
