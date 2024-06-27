# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import re
import time

from typing import Optional, TYPE_CHECKING

from .utils.errors import MetarigError
from .utils.bones import new_bone
from .utils.layers import (ORG_COLLECTION, MCH_COLLECTION, DEF_COLLECTION, ROOT_COLLECTION, set_bone_layers,
                           validate_collection_references)
from .utils.naming import (ORG_PREFIX, MCH_PREFIX, DEF_PREFIX, ROOT_NAME, make_original_name,
                           change_name_side, get_name_side, Side)
from .utils.widgets import WGT_PREFIX, WGT_GROUP_PREFIX
from .utils.widgets_special import create_root_widget
from .utils.mechanism import refresh_all_drivers
from .utils.misc import select_object, ArmatureObject, verify_armature_obj, choose_next_uid, flatten_children,\
    flatten_parents
from .utils.collections import (ensure_collection, list_layer_collections,
                                filter_layer_collections_by_object)
from .utils.rig import get_rigify_type, get_rigify_target_rig,\
    get_rigify_rig_basename, get_rigify_force_widget_update, get_rigify_finalize_script,\
    get_rigify_mirror_widgets, get_rigify_colors
from .utils.action_layers import ActionLayerBuilder
from .utils.objects import ArtifactManager

from . import base_generate
from . import rig_ui_template
from . import rig_lists

if TYPE_CHECKING:
    from . import RigifyColorSet


RIG_MODULE = "rigs"


class Timer:
    def __init__(self):
        self.time_val = time.time()

    def tick(self, string):
        t = time.time()
        print(string + "%.3f" % (t - self.time_val))
        self.time_val = t


class Generator(base_generate.BaseGenerator):
    usable_collections: list[bpy.types.LayerCollection]
    action_layers: ActionLayerBuilder

    def __init__(self, context, metarig):
        super().__init__(context, metarig)

        self.id_store = context.window_manager
        self.saved_visible_layers = {}

    def find_rig_class(self, rig_type):
        rig_module = rig_lists.rigs[rig_type]["module"]

        return rig_module.Rig

    def __switch_to_usable_collection(self, obj, fallback=False):
        collections = filter_layer_collections_by_object(self.usable_collections, obj)

        if collections:
            self.layer_collection = collections[0]
        elif fallback:
            self.layer_collection = self.view_layer.layer_collection

        self.collection = self.layer_collection.collection

    def ensure_rig_object(self) -> tuple[bool, ArmatureObject]:
        """Check if the generated rig already exists, so we can
        regenerate in the same object. If not, create a new
        object to generate the rig in.
        """
        print("Fetch rig.")
        meta_data = self.metarig.data

        target_rig = get_rigify_target_rig(meta_data)
        found = bool(target_rig)

        if not found:
            rig_basename = get_rigify_rig_basename(meta_data)

            if rig_basename:
                rig_new_name = rig_basename
            elif "metarig" in self.metarig.name:
                rig_new_name = self.metarig.name.replace("metarig", "rig")
            elif "META" in self.metarig.name:
                rig_new_name = self.metarig.name.replace("META", "RIG")
            else:
                rig_new_name = "RIG-" + self.metarig.name

            arm = bpy.data.armatures.new(rig_new_name)
            target_rig = verify_armature_obj(bpy.data.objects.new(rig_new_name, arm))
            target_rig.display_type = 'WIRE'

        # If the object is already added to the scene, switch to its collection
        if target_rig in list(self.context.scene.collection.all_objects):
            self.__switch_to_usable_collection(target_rig)
        else:
            # Otherwise, add to the selected collection or the metarig collection if unusable
            if (self.layer_collection not in self.usable_collections
                    or self.layer_collection == self.view_layer.layer_collection):
                self.__switch_to_usable_collection(self.metarig, True)

            self.collection.objects.link(target_rig)

        # Configure and remember the object
        meta_data.rigify_target_rig = target_rig
        target_rig.data.pose_position = 'POSE'

        return found, target_rig

    def __unhide_rig_object(self, obj: bpy.types.Object):
        # Ensure the object is visible and selectable
        obj.hide_set(False, view_layer=self.view_layer)
        obj.hide_viewport = False

        if not obj.visible_get(view_layer=self.view_layer):
            raise Exception('Could not generate: Target rig is not visible')

        obj.select_set(True, view_layer=self.view_layer)

        if not obj.select_get(view_layer=self.view_layer):
            raise Exception('Could not generate: Cannot select target rig')

        if self.layer_collection not in self.usable_collections:
            raise Exception('Could not generate: Could not find a usable collection.')

    def __save_rig_data(self, obj: ArmatureObject, obj_found: bool):
        if obj_found:
            self.saved_visible_layers = {coll.name: coll.is_visible for coll in obj.data.collections_all}

            self.artifacts.generate_init_existing(obj)

    def __find_legacy_collection(self) -> bpy.types.Collection:
        """For backwards comp, matching by name to find a legacy collection.
        (For before there was a Widget Collection PointerProperty)
        """
        widgets_group_name = WGT_GROUP_PREFIX + self.obj.name
        old_collection = bpy.data.collections.get(widgets_group_name)

        if old_collection and old_collection.library:
            old_collection = None

        if not old_collection:
            # Update the old 'Widgets' collection
            legacy_collection = bpy.data.collections.get('Widgets')

            if legacy_collection and widgets_group_name in legacy_collection.objects\
                    and not legacy_collection.library:
                legacy_collection.name = widgets_group_name
                old_collection = legacy_collection

        if old_collection:
            # Rename the collection
            old_collection.name = widgets_group_name

        return old_collection

    def ensure_widget_collection(self):
        # Create/find widget collection
        self.widget_collection = self.metarig.data.rigify_widgets_collection
        if not self.widget_collection:
            self.widget_collection = self.__find_legacy_collection()
        if not self.widget_collection:
            widgets_group_name = WGT_GROUP_PREFIX + self.obj.name.replace("RIG-", "")
            self.widget_collection = ensure_collection(
                self.context, widgets_group_name, hidden=True)

        self.metarig.data.rigify_widgets_collection = self.widget_collection

        self.use_mirror_widgets = get_rigify_mirror_widgets(self.metarig.data)

        # Build tables for existing widgets
        self.old_widget_table = {}
        self.new_widget_table = {}
        self.widget_mirror_mesh = {}

        if get_rigify_force_widget_update(self.metarig.data):
            # Remove widgets if force update is set
            for obj in list(self.widget_collection.objects):
                bpy.data.objects.remove(obj)
        elif self.obj.pose:
            # Find all widgets from the collection referenced by the old rig
            known_widgets = set(obj.name for obj in self.widget_collection.objects)

            for bone in self.obj.pose.bones:
                if bone.custom_shape and bone.custom_shape.name in known_widgets:
                    self.old_widget_table[bone.name] = bone.custom_shape

            # Rename widgets in case the rig was renamed
            name_prefix = WGT_PREFIX + self.obj.name + "_"

            for bone_name, widget in self.old_widget_table.items():
                old_data_name = change_name_side(widget.name, get_name_side(widget.data.name))

                widget.name = name_prefix + bone_name

                # If the mesh name is the same as the object, rename it too
                if widget.data.name == old_data_name:
                    widget.data.name = change_name_side(
                        widget.name, get_name_side(widget.data.name))

            # Find meshes for mirroring
            if self.use_mirror_widgets:
                for bone_name, widget in self.old_widget_table.items():
                    mid_name = change_name_side(bone_name, Side.MIDDLE)
                    if bone_name != mid_name:
                        assert isinstance(widget.data, bpy.types.Mesh)
                        self.widget_mirror_mesh[mid_name] = widget.data

    def ensure_root_bone_collection(self):
        collections = self.metarig.data.collections_all

        validate_collection_references(self.metarig)

        coll = collections.get(ROOT_COLLECTION)

        if not coll:
            coll = self.metarig.data.collections.new(ROOT_COLLECTION)

        if coll.rigify_ui_row <= 0:
            coll.rigify_ui_row = 2 + choose_next_uid(collections, 'rigify_ui_row', min_value=1)

    def __duplicate_rig(self):
        obj = self.obj
        metarig = self.metarig
        context = self.context

        # Remove all bones from the generated rig armature.
        bpy.ops.object.mode_set(mode='EDIT')
        for bone in obj.data.edit_bones:
            obj.data.edit_bones.remove(bone)
        bpy.ops.object.mode_set(mode='OBJECT')

        # Remove all bone collections from the target armature.
        for coll in list(obj.data.collections_all):
            obj.data.collections.remove(coll)

        # Select and duplicate metarig
        select_object(context, metarig, deselect_all=True)

        bpy.ops.object.duplicate()

        # Rename org bones in the temporary object
        temp_obj = verify_armature_obj(context.view_layer.objects.active)

        assert temp_obj and temp_obj != metarig

        self.__freeze_driver_vars(temp_obj)
        self.__rename_org_bones(temp_obj)

        # Select the target rig and join
        select_object(context, obj)

        saved_matrix = obj.matrix_world.copy()
        obj.matrix_world = metarig.matrix_world

        bpy.ops.object.join()

        obj.matrix_world = saved_matrix

        # Select the generated rig
        select_object(context, obj, deselect_all=True)

        # Clean up animation data
        if obj.animation_data:
            obj.animation_data.action = None

            for track in obj.animation_data.nla_tracks:
                obj.animation_data.nla_tracks.remove(track)

    @staticmethod
    def __freeze_driver_vars(obj: bpy.types.Object):
        if obj.animation_data:
            # Freeze drivers referring to custom properties
            for d in obj.animation_data.drivers:
                for var in d.driver.variables:
                    for tar in var.targets:
                        # If a custom property
                        if var.type == 'SINGLE_PROP' \
                                and re.match(r'^pose.bones\["[^"\]]*"]\["[^"\]]*"]$',
                                             tar.data_path):
                            tar.data_path = "RIGIFY-" + tar.data_path

    def __rename_org_bones(self, obj: ArmatureObject):
        # Make a list of the original bones, so we can keep track of them.
        original_bones = [bone.name for bone in obj.data.bones]

        # Add the ORG_PREFIX to the original bones.
        for i in range(0, len(original_bones)):
            bone = obj.pose.bones[original_bones[i]]

            # Preserve the root bone as is if present
            if bone.name == ROOT_NAME:
                if bone.parent:
                    raise MetarigError('Root bone must have no parent')
                if get_rigify_type(bone) not in ('', 'basic.raw_copy'):
                    raise MetarigError('Root bone must have no rig, or use basic.raw_copy')
                continue

            # This rig type is special in that it preserves the name of the bone.
            if get_rigify_type(bone) != 'basic.raw_copy':
                bone.name = make_original_name(original_bones[i])
                original_bones[i] = bone.name

        self.original_bones = original_bones

    def __create_root_bone(self):
        obj = self.obj
        metarig = self.metarig

        if ROOT_NAME in obj.data.bones:
            # Use the existing root bone
            root_bone = ROOT_NAME
        else:
            # Create the root bone.
            root_bone = new_bone(obj, ROOT_NAME)
            spread = get_xy_spread(metarig.data.bones) or metarig.data.bones[0].length
            spread = float('%.3g' % spread)
            scale = spread/0.589
            obj.data.edit_bones[root_bone].head = (0, 0, 0)
            obj.data.edit_bones[root_bone].tail = (0, scale, 0)
            obj.data.edit_bones[root_bone].roll = 0

        self.root_bone = root_bone
        self.bone_owners[root_bone] = None
        self.noparent_bones.add(root_bone)

    def __parent_bones_to_root(self):
        eb = self.obj.data.edit_bones

        # Parent loose bones to root
        for bone in eb:
            if bone.name in self.noparent_bones:
                continue
            elif bone.parent is None:
                bone.use_connect = False
                bone.parent = eb[self.root_bone]

    def __lock_transforms(self):
        # Lock transforms on all non-control bones
        r = re.compile("[A-Z][A-Z][A-Z]-")
        for pb in self.obj.pose.bones:
            if r.match(pb.name):
                pb.lock_location = (True, True, True)
                pb.lock_rotation = (True, True, True)
                pb.lock_rotation_w = True
                pb.lock_scale = (True, True, True)

    def ensure_bone_collection(self, name):
        coll = self.obj.data.collections_all.get(name)

        if not coll:
            coll = self.obj.data.collections.new(name)

        return coll

    def __assign_layers(self):
        pose_bones = self.obj.pose.bones

        root_coll = self.ensure_bone_collection(ROOT_COLLECTION)
        org_coll = self.ensure_bone_collection(ORG_COLLECTION)
        mch_coll = self.ensure_bone_collection(MCH_COLLECTION)
        def_coll = self.ensure_bone_collection(DEF_COLLECTION)

        set_bone_layers(pose_bones[self.root_bone].bone, [root_coll])

        # Every bone that has a name starting with "DEF-" make deforming.  All the
        # others make non-deforming.
        for pbone in pose_bones:
            bone = pbone.bone
            name = bone.name
            layers = None

            bone.use_deform = name.startswith(DEF_PREFIX)

            # Move all the original bones to their layer.
            if name.startswith(ORG_PREFIX):
                layers = [org_coll]
            # Move all the bones with names starting with "MCH-" to their layer.
            elif name.startswith(MCH_PREFIX):
                layers = [mch_coll]
            # Move all the bones with names starting with "DEF-" to their layer.
            elif name.startswith(DEF_PREFIX):
                layers = [def_coll]

            if layers is not None:
                set_bone_layers(bone, layers)

                # Remove custom shapes from non-control bones
                pbone.custom_shape = None

            bone.bbone_x = bone.bbone_z = bone.length * 0.05

    def __restore_driver_vars(self):
        obj = self.obj

        # Alter marked driver targets
        if obj.animation_data:
            for d in obj.animation_data.drivers:
                for v in d.driver.variables:
                    for tar in v.targets:
                        if tar.data_path.startswith("RIGIFY-"):
                            temp, bone, prop = tuple(
                                [x.strip('"]') for x in tar.data_path.split('["')])
                            if bone in obj.data.bones and prop in obj.pose.bones[bone].keys():
                                tar.data_path = tar.data_path[7:]
                            else:
                                org_name = make_original_name(bone)
                                org_name = self.org_rename_table.get(org_name, org_name)
                                tar.data_path = 'pose.bones["%s"]["%s"]' % (org_name, prop)

    def __assign_widgets(self):
        obj_table = {obj.name: obj for obj in self.scene.objects}

        # Assign shapes to bones
        # Object's with name WGT-<bone_name> get used as that bone's shape.
        for bone in self.obj.pose.bones:
            # First check the table built by create_widget
            if bone.name in self.new_widget_table:
                bone.custom_shape = self.new_widget_table[bone.name]
                continue

            # Object names are limited to 63 characters... arg
            wgt_name = (WGT_PREFIX + self.obj.name + '_' + bone.name)[:63]

            if wgt_name in obj_table:
                bone.custom_shape = obj_table[wgt_name]

    def __compute_visible_layers(self):
        has_ui_buttons = set().union(*[
            {p.name for p in flatten_parents(coll)}
            for coll in self.obj.data.collections_all
            if coll.rigify_ui_row > 0
        ])

        # Hide all layers without UI buttons
        for coll in self.obj.data.collections_all:
            user_visible = self.saved_visible_layers.get(coll.name, coll.is_visible)
            coll.is_visible = user_visible and coll.name in has_ui_buttons

    def generate(self):
        context = self.context
        metarig = self.metarig
        view_layer = self.view_layer
        t = Timer()

        self.usable_collections = list_layer_collections(
            view_layer.layer_collection, selectable=True)

        bpy.ops.object.mode_set(mode='OBJECT')

        ###########################################
        # Create/find the rig object and set it up
        obj_found, obj = self.ensure_rig_object()

        self.obj = obj
        self.__unhide_rig_object(obj)

        # Collect data from the existing rig
        self.artifacts = ArtifactManager(self)

        self.__save_rig_data(obj, obj_found)

        # Select the chosen working collection in case it changed
        self.view_layer.active_layer_collection = self.layer_collection

        self.ensure_root_bone_collection()

        # Get rid of anim data in case the rig already existed
        print("Clear rig animation data.")

        obj.animation_data_clear()
        obj.data.animation_data_clear()

        select_object(context, obj, deselect_all=True)

        ###########################################
        # Create Widget Collection
        self.ensure_widget_collection()

        t.tick("Create widgets collection: ")

        ###########################################
        # Get parented objects to restore later

        child_parent_bones = {}  # {object: bone}

        for child in obj.children:
            child_parent_bones[child] = child.parent_bone

        ###########################################
        # Copy bones from metarig to obj (adds ORG_PREFIX)
        self.__duplicate_rig()

        obj.data.use_mirror_x = False

        t.tick("Duplicate rig: ")

        ###########################################
        # Put the rig_name in the armature custom properties
        obj.data["rig_id"] = self.rig_id

        self.script = rig_ui_template.ScriptGenerator(self)
        self.action_layers = ActionLayerBuilder(self)

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.instantiate_rig_tree()

        t.tick("Instantiate rigs: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_initialize()

        t.tick("Initialize rigs: ")

        ###########################################
        bpy.ops.object.mode_set(mode='EDIT')

        self.invoke_prepare_bones()

        t.tick("Prepare bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')

        self.__create_root_bone()

        self.invoke_generate_bones()

        t.tick("Generate bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')

        self.invoke_parent_bones()

        self.__parent_bones_to_root()

        t.tick("Parent bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_configure_bones()

        t.tick("Configure bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_preapply_bones()

        t.tick("Preapply bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='EDIT')

        self.invoke_apply_bones()

        t.tick("Apply bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_rig_bones()

        t.tick("Rig bones: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_generate_widgets()

        # Generate the default root widget last in case it's rigged with raw_copy
        create_root_widget(obj, self.root_bone)

        t.tick("Generate widgets: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.__lock_transforms()
        self.__assign_layers()
        self.__compute_visible_layers()
        self.__restore_driver_vars()

        t.tick("Assign layers: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.invoke_finalize()

        t.tick("Finalize: ")

        ###########################################
        bpy.ops.object.mode_set(mode='OBJECT')

        self.__assign_widgets()

        # Create Selection Sets
        create_selection_sets(obj, metarig)

        # Create Bone Groups
        apply_bone_colors(obj, metarig, self.layer_group_priorities)

        t.tick("The rest: ")

        ###########################################
        # Restore state

        bpy.ops.object.mode_set(mode='OBJECT')
        obj.data.pose_position = 'POSE'

        # Restore parent to bones
        for child, sub_parent in child_parent_bones.items():
            if sub_parent in obj.pose.bones:
                mat = child.matrix_world.copy()
                child.parent_bone = sub_parent
                child.matrix_world = mat

        # Clear any transient errors in drivers
        refresh_all_drivers()

        ###########################################
        # Execute the finalize script

        finalize_script = get_rigify_finalize_script(metarig.data)

        if finalize_script:
            bpy.ops.object.mode_set(mode='OBJECT')
            exec(finalize_script.as_string(), {})
            bpy.ops.object.mode_set(mode='OBJECT')

        obj.data.collections.active_index = 0

        self.artifacts.generate_cleanup()

        ###########################################
        # Restore active collection
        view_layer.active_layer_collection = self.layer_collection


def generate_rig(context, metarig):
    """ Generates a rig from a metarig.

    """
    # Initial configuration
    rest_backup = metarig.data.pose_position
    metarig.data.pose_position = 'REST'

    try:
        generator = Generator(context, metarig)

        base_generate.BaseGenerator.instance = generator

        generator.generate()

        metarig.data.pose_position = rest_backup

    except Exception as e:
        # Cleanup if something goes wrong
        print("Rigify: failed to generate rig.")

        bpy.ops.object.mode_set(mode='OBJECT')
        metarig.data.pose_position = rest_backup

        # Continue the exception
        raise e

    finally:
        base_generate.BaseGenerator.instance = None


def create_selection_set_for_rig_layer(rig: ArmatureObject, set_name: str, coll: bpy.types.BoneCollection) -> None:
    """Create a single selection set on a rig.

    The set will contain all bones on the rig layer with the given index.
    """
    sel_set = rig.selection_sets.add()  # noqa
    sel_set.name = set_name

    for b in rig.pose.bones:
        if coll not in b.bone.collections or b.name in sel_set.bone_ids:
            continue

        bone_id = sel_set.bone_ids.add()
        bone_id.name = b.name


def create_selection_sets(obj: ArmatureObject, _metarig: ArmatureObject):
    """Create selection sets if the Selection Sets addon is enabled.

    Whether a selection set for a rig layer is created is controlled in the
    Rigify Layer Names panel.
    """
    # Check if selection sets addon is installed
    if 'bone_selection_groups' not in bpy.context.preferences.addons \
            and 'bone_selection_sets' not in bpy.context.preferences.addons:
        return

    obj.selection_sets.clear()  # noqa

    for coll in obj.data.collections_all:
        if not coll.rigify_sel_set:
            continue

        create_selection_set_for_rig_layer(obj, coll.name, coll)


def apply_bone_colors(obj, metarig, priorities: Optional[dict[str, dict[str, float]]] = None):
    bpy.ops.object.mode_set(mode='OBJECT')
    pb = obj.pose.bones

    color_sets = get_rigify_colors(metarig.data)
    color_map = {i+1: cset for i, cset in enumerate(color_sets)}

    collection_table: dict[str, tuple[int, 'RigifyColorSet']] = {
        coll.name: (i, color_map[coll.rigify_color_set_id])
        for i, coll in enumerate(flatten_children(obj.data.collections))
        if coll.rigify_color_set_id in color_map
    }

    priorities = priorities or {}
    dummy = {}

    for b in pb:
        bone_priorities = priorities.get(b.name, dummy)
        cset_collections = [coll.name for coll in b.bone.collections if coll.name in collection_table]
        if cset_collections:
            best_name = max(
                cset_collections,
                key=lambda n: (bone_priorities.get(n, 0), -collection_table[n][0])
            )
            _, cset = collection_table[best_name]
            cset.apply(b.bone.color)
            cset.apply(b.color)


def get_xy_spread(bones):
    x_max = 0
    y_max = 0
    for b in bones:
        x_max = max((x_max, abs(b.head[0]), abs(b.tail[0])))
        y_max = max((y_max, abs(b.head[1]), abs(b.tail[1])))

    return max((x_max, y_max))
