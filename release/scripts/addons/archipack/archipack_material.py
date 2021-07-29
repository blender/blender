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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
import os
# noinspection PyUnresolvedReferences
from bpy.types import (
    Panel, PropertyGroup,
    Object, Operator
    )
from bpy.props import (
    EnumProperty, CollectionProperty,
    StringProperty
    )


setman = None
libman = None


class MatLib():
    """
        A material library .blend file
        Store material name
        Apply material to objects
    """
    def __init__(self, matlib_path, name):
        self.name = name
        try:
            self.path = os.path.join(matlib_path, name)
        except:
            pass
        self.materials = []

    def cleanup(self):
        self.materials.clear()

    def load_list(self, sort=False):
        """
            list material names
        """
        # print("MatLib.load_list(%s)" % (self.name))
        self.materials.clear()
        try:
            with bpy.data.libraries.load(self.path) as (data_from, data_to):
                for mat in data_from.materials:
                    self.materials.append(mat)
            if sort:
                self.materials = list(sorted(self.materials))
        except:
            pass

    def has(self, name):
        return name in self.materials

    def load_mat(self, name, link):
        """
            Load a material from library
        """
        try:
            # print("MatLib.load_mat(%s) linked:%s" % (name, link))
            with bpy.data.libraries.load(self.path, link, False) as (data_from, data_to):
                data_to.materials = [name]
        except:
            pass

    def get_mat(self, name, link):
        """
            apply a material by name to active_object
            into slot index
            lazy load material list on demand
            return material or None
        """

        # Lazy load material names
        if len(self.materials) < 1:
            self.load_list()

        # material belongs to this libraray
        if self.has(name):

            # load material
            self.load_mat(name, link)

            return bpy.data.materials.get(name)

        return None


class MatlibsManager():
    """
        Manage multiple library
        Lazy load
    """
    def __init__(self):
        self.matlibs = []

    def cleanup(self):
        for lib in self.matlibs:
            lib.cleanup()
        self.matlibs.clear()

    def get_prefs(self, context):
        """
            let raise error if any
        """
        global __name__
        prefs = None
        # retrieve addon name from imports
        addon_name = __name__.split('.')[0]
        prefs = context.user_preferences.addons[addon_name].preferences
        return prefs

    @property
    def loaded_path(self):
        """
            Loaded matlibs filenames
        """
        return [lib.path for lib in self.matlibs]

    def from_data(self, name):
        return bpy.data.materials.get(name)

    def add_to_list(self, path):
        """
            Add material library to list
            only store name of lib
            reloading here dosent make sense
        """
        loaded_path = self.loaded_path

        if os.path.exists(path):
            self.matlibs.extend(
                    [
                    MatLib(path, f) for f in os.listdir(path)
                    if f.endswith(".blend") and os.path.join(path, f) not in loaded_path
                    ]
            )

    def load_list(self, context):
        """
            list available library path
        """
        # default library
        dir_path = os.path.dirname(os.path.realpath(__file__))
        mat_path = os.path.join(dir_path, "materials")
        self.add_to_list(mat_path)

        # user def library path from addon prefs
        try:
            prefs = self.get_prefs(context)
            self.add_to_list(prefs.matlib_path)
        except:
            print("Archipack: Unable to load default material library, please check path in addon prefs")
            pass

    def apply(self, context, slot_index, name, link=False):

        o = context.active_object
        o.select = True

        # material with same name exist in scene
        mat = self.from_data(name)

        # mat not in scene: try to load from lib
        if mat is None:
            # print("mat %s not found in scene, loading" % (name))
            # Lazy build matlibs list
            if len(self.matlibs) < 1:
                self.load_list(context)

            for lib in self.matlibs:
                mat = lib.get_mat(name, link)
                if mat is not None:
                    break

        # nothing found, build a default mat
        if mat is None:
            mat = bpy.data.materials.new(name)

        if slot_index < len(o.material_slots):
            o.material_slots[slot_index].material = None
            o.material_slots[slot_index].material = mat
            o.active_material_index = slot_index

        if not link:
            # break link
            bpy.ops.object.make_local(type="SELECT_OBDATA_MATERIAL")


class MaterialSetManager():
    """
        Manage material sets for objects
        Store material names for each set
        Lazy load at enumerate time
    """
    def __init__(self):
        """
            Store sets for each object type
        """
        self.objects = {}

    def get_filename(self, object_type):

        target_path = os.path.join("presets", "archipack_materials")
        target_path = bpy.utils.user_resource('SCRIPTS',
                                                target_path,
                                                create=True)
        return os.path.join(target_path, object_type) + '.txt'

    def cleanup(self):
        self.objects.clear()

    def register_set(self, object_type, set_name, materials_names):

        if object_type not in self.objects.keys():
            self.objects[object_type] = {}

        self.objects[object_type][set_name.upper()] = materials_names

    def load(self, object_type):

        filename = self.get_filename(object_type)

        # preset not found in user prefs, load from archipack's default
        if not os.path.exists(filename):
            rel_filepath = \
                os.path.sep + "presets" + os.path.sep + \
                "archipack_materials" + os.path.sep + object_type + '.txt'

            filename = os.path.dirname(os.path.realpath(__file__)) + rel_filepath

        # print("load filename %s" % filename)

        material_sets = {}

        # create file object, and set open mode
        if os.path.exists(filename):
            try:
                f = open(filename, 'r')
                lines = f.readlines()

                for line in lines:
                    s_key, mat_name = line.split("##|##")
                    if str(s_key) not in material_sets.keys():
                        material_sets[s_key] = []
                    material_sets[s_key].append(mat_name.strip())
            except:
                print("Archipack: An error occured while loading {}".format(filename))
                pass
            finally:
                f.close()

            for s_key in material_sets.keys():
                self.register_set(object_type, s_key, material_sets[s_key])

    def save(self, object_type):
        # always save in user prefs
        filename = self.get_filename(object_type)
        # print("filename:%s" % filename)
        o_dict = self.objects[object_type]
        lines = []
        for s_key in o_dict.keys():
            for mat in o_dict[s_key]:
                lines.append("{}##|##{}\n".format(s_key, mat))
        try:
            f = open(filename, 'w')
            f.writelines(lines)
        except:
            print("Archipack: An error occured while saving {}".format(filename))
            pass
        finally:
            f.close()

    def add(self, context, set_name):
        o = context.active_object
        if "archipack_material" in o:
            object_type = o.archipack_material[0].category
            materials_names = [slot.name for slot in o.material_slots if slot.name != '']
            # print("%s " % materials_names)
            self.register_set(object_type, set_name, materials_names)
            self.save(object_type)

    def remove(self, context):
        o = context.active_object
        if "archipack_material" in o:
            d = o.archipack_material[0]
            object_type = d.category
            set_name = d.material
            if set_name in self.objects[object_type].keys():
                self.objects[object_type].pop(set_name)
                self.save(object_type)

    def get_materials(self, object_type, set_name):
        if object_type not in self.objects.keys():
            self.load(object_type)
        if object_type not in self.objects.keys():
            print("Archipack: Unknown object type {}".format(object_type))
            return None
        if set_name not in self.objects[object_type].keys():
            print("Archipack: set {} not found".format(set_name))
            return None
        return self.objects[object_type][set_name]

    def make_enum(self, object_type):

        if object_type not in self.objects.keys():
            self.load(object_type)

        if object_type not in self.objects.keys():
            self.objects[object_type] = {}

        s_keys = self.objects[object_type].keys()

        if len(s_keys) < 1:
            return [('DEFAULT', 'Default', '', 0)]

        return [(s.upper(), s.capitalize(), '', i) for i, s in enumerate(s_keys)]


def material_enum(self, context):
    global setman
    if setman is None:
        setman = MaterialSetManager()
    return setman.make_enum(self.category)


def update(self, context):
    self.update(context)


class archipack_material(PropertyGroup):

    category = StringProperty(
        name="Category",
        description="Archipack object name",
        default=""
        )
    material = EnumProperty(
        name="Material",
        description="Material Set name",
        items=material_enum,
        update=update
        )

    def apply_material(self, context, slot_index, name):
        global libman

        if libman is None:
            libman = MatlibsManager()

        libman.apply(context, slot_index, name, link=False)

    def update(self, context):
        global setman

        if setman is None:
            setman = MaterialSetManager()

        o = context.active_object
        sel = [
            c for c in o.children
            if 'archipack_material' in c and c.archipack_material[0].category == self.category]

        # handle wall's holes
        if o.data and "archipack_wall2" in o.data:
            if o.parent is not None:
                for child in o.parent.children:
                    if ('archipack_hybridhole' in child or
                            'archipack_robusthole' in child or
                            'archipack_hole' in child):
                        sel.append(child)

        sel.append(o)

        mats = setman.get_materials(self.category, self.material)

        if mats is None:
            return False

        for ob in sel:
            context.scene.objects.active = ob
            for slot_index, mat_name in enumerate(mats):
                if slot_index >= len(ob.material_slots):
                    bpy.ops.object.material_slot_add()
                self.apply_material(context, slot_index, mat_name)

        context.scene.objects.active = o

        return True


class ARCHIPACK_PT_material(Panel):
    bl_idname = "ARCHIPACK_PT_material"
    bl_label = "Archipack Material"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    # bl_category = 'ArchiPack'

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and 'archipack_material' in context.active_object

    def draw(self, context):
        layout = self.layout
        props = context.active_object.archipack_material[0]
        row = layout.row(align=True)
        row.prop(props, 'material', text="")
        row.operator('archipack.material_add', icon="ZOOMIN", text="")
        row.operator('archipack.material_remove', icon="ZOOMOUT", text="")


class ARCHIPACK_OT_material(Operator):
    bl_idname = "archipack.material"
    bl_label = "Material"
    bl_description = "Add archipack material"
    bl_options = {'REGISTER', 'UNDO'}

    category = StringProperty(
        name="Category",
        description="Archipack object name",
        default=""
        )
    material = StringProperty(
        name="Material",
        description="Material Set name",
        default=""
        )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):

        o = context.active_object

        if 'archipack_material' in o:
            m = o.archipack_material[0]
        else:
            m = o.archipack_material.add()

        m.category = self.category
        try:
            m.material = self.material
            res = m.update(context)
        except:
            res = False
            pass

        if res:
            # print("ARCHIPACK_OT_material.apply {} {}".format(self.category, self.material))
            return {'FINISHED'}
        else:
            print("Archipack: unable to add material {} for {}".format(self.material, self.category))
            self.report({'WARNING'}, 'Material {} for {} not found'.format(self.material, self.category))
            return {'CANCELLED'}


class ARCHIPACK_OT_material_add(Operator):
    bl_idname = "archipack.material_add"
    bl_label = "Material"
    bl_description = "Add a set of archipack material"
    bl_options = {'REGISTER', 'UNDO'}

    material = StringProperty(
        name="Material",
        description="Material Set name",
        default=""
        )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):

        global setman

        if setman is None:
            setman = MaterialSetManager()

        setman.add(context, self.material)

        return {'FINISHED'}


class ARCHIPACK_OT_material_remove(Operator):
    bl_idname = "archipack.material_remove"
    bl_label = "Material"
    bl_description = "Remove a set of archipack material"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):

        global setman

        if setman is None:
            setman = MaterialSetManager()

        setman.remove(context)

        return {'FINISHED'}


class ARCHIPACK_OT_material_library(Operator):
    bl_idname = "archipack.material_library"
    bl_label = "Material Library"
    bl_description = "Add all archipack materials on a single object"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):

        global setman

        if setman is None:
            setman = MaterialSetManager()

        o = context.active_object

        if 'archipack_material' in o:
            m = o.archipack_material[0]
        else:
            m = o.archipack_material.add()
        o.data.materials.clear()

        for category in setman.objects.keys():
            prefix = category.capitalize() + "_"
            for part in setman.objects[category]["DEFAULT"]:
                name = prefix + part
                mat = m.get_material(name)
                o.data.materials.append(mat)

        return {'FINISHED'}


def register():
    bpy.utils.register_class(archipack_material)
    Object.archipack_material = CollectionProperty(type=archipack_material)
    bpy.utils.register_class(ARCHIPACK_OT_material)
    bpy.utils.register_class(ARCHIPACK_OT_material_add)
    bpy.utils.register_class(ARCHIPACK_OT_material_remove)
    bpy.utils.register_class(ARCHIPACK_OT_material_library)
    bpy.utils.register_class(ARCHIPACK_PT_material)


def unregister():
    global libman
    global setman
    if libman is not None:
        libman.cleanup()
    if setman is not None:
        setman.cleanup()
    bpy.utils.unregister_class(ARCHIPACK_PT_material)
    bpy.utils.unregister_class(ARCHIPACK_OT_material)
    bpy.utils.unregister_class(ARCHIPACK_OT_material_add)
    bpy.utils.unregister_class(ARCHIPACK_OT_material_remove)
    bpy.utils.unregister_class(ARCHIPACK_OT_material_library)
    bpy.utils.unregister_class(archipack_material)
    del Object.archipack_material
