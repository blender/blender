# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import re

from typing import TYPE_CHECKING, Optional, Any, Sequence, Iterable

from bpy.types import (bpy_prop_collection, Material, Object, PoseBone, Driver, FCurve,
                       DriverTarget, ID, bpy_struct, FModifierGenerator, Constraint, AnimData,
                       ArmatureConstraint)

from rna_prop_ui import rna_idprop_ui_create
from rna_prop_ui import rna_idprop_quote_path as quote_property

from .misc import force_lazy, ArmatureObject, Lazy, OptionalLazy

if TYPE_CHECKING:
    from ..base_rig import BaseRig


##############################################
# Constraint creation utilities
##############################################

_TRACK_AXIS_MAP = {
    'X': 'TRACK_X', '-X': 'TRACK_NEGATIVE_X',
    'Y': 'TRACK_Y', '-Y': 'TRACK_NEGATIVE_Y',
    'Z': 'TRACK_Z', '-Z': 'TRACK_NEGATIVE_Z',
}


def _set_default_attr(obj, options, attr, value):
    if hasattr(obj, attr):
        options.setdefault(attr, value)


def make_constraint(
        owner: Object | PoseBone, con_type: str,
        target: Optional[Object] = None,
        subtarget: OptionalLazy[str] = None, *,
        insert_index: Optional[int] = None,
        space: Optional[str] = None,
        track_axis: Optional[str] = None,
        use_xyz: Optional[Sequence[bool]] = None,
        use_limit_xyz: Optional[Sequence[bool]] = None,
        invert_xyz: Optional[Sequence[bool]] = None,
        targets: Optional[list[Lazy[str | tuple | dict]]] = None,
        **options):
    """
    Creates and initializes constraint of the specified type for the owner bone.

    Specially handled keyword arguments:

      target, subtarget: if both not None, passed through to the constraint
      insert_index     : insert at the specified index in the stack, instead of at the end
      space            : assigned to both owner_space and target_space
      track_axis       : allows shorter X, Y, Z, -X, -Y, -Z notation
      use_xyz          : list of 3 items is assigned to use_x, use_y and use_z options
      use_limit_xyz    : list of 3 items is assigned to use_limit_x/y/z options
      invert_xyz       : list of 3 items is assigned to invert_x, invert_y and invert_z options
      min/max_x/y/z    : a corresponding use_(min/max/limit)_(x/y/z) option is set to True
      targets          : list of strings, tuples or dicts describing Armature constraint targets

    Other keyword arguments are directly assigned to the constraint options.
    Returns the newly created constraint.

    Target bone names can be provided via 'lazy' callable closures without arguments.
    """
    con = owner.constraints.new(con_type)

    # For Armature constraints, allow passing a "targets" list as a keyword argument.
    if targets is not None:
        assert isinstance(con, ArmatureConstraint)
        for target_info in targets:
            con_target = con.targets.new()
            con_target.target = owner.id_data
            # List element can be a string, a tuple or a dictionary.
            target_info = force_lazy(target_info)
            if isinstance(target_info, str):
                con_target.subtarget = target_info
            elif isinstance(target_info, tuple):
                if len(target_info) == 2:
                    con_target.subtarget, con_target.weight = map(force_lazy, target_info)
                else:
                    con_target.target, con_target.subtarget, con_target.weight = map(force_lazy, target_info)
            else:
                assert isinstance(target_info, dict)
                for key, val in target_info.items():
                    setattr(con_target, key, force_lazy(val))

    if insert_index is not None:
        owner.constraints.move(len(owner.constraints)-1, insert_index)

    if target is not None and hasattr(con, 'target'):
        con.target = target

    if subtarget is not None:
        con.subtarget = force_lazy(subtarget)

    if space is not None:
        _set_default_attr(con, options, 'owner_space', space)
        _set_default_attr(con, options, 'target_space', space)

    if track_axis is not None:
        con.track_axis = _TRACK_AXIS_MAP.get(track_axis, track_axis)

    if use_xyz is not None:
        con.use_x, con.use_y, con.use_z = use_xyz[0:3]

    if use_limit_xyz is not None:
        con.use_limit_x, con.use_limit_y, con.use_limit_z = use_limit_xyz[0:3]

    if invert_xyz is not None:
        con.invert_x, con.invert_y, con.invert_z = invert_xyz[0:3]

    for key in ['min_x', 'max_x', 'min_y', 'max_y', 'min_z', 'max_z']:
        if key in options:
            _set_default_attr(con, options, 'use_'+key, True)
            _set_default_attr(con, options, 'use_limit_'+key[-1], True)

    for p, v in options.items():
        setattr(con, p, force_lazy(v))

    return con


##############################################
# Custom property creation utilities
##############################################

# noinspection PyShadowingBuiltins
def make_property(
        owner: bpy_struct, name: str, default, *,
        min: float = 0, max: float = 1, soft_min=None, soft_max=None,
        description: Optional[str] = None, overridable=True, subtype: Optional[str] = None,
        **options):
    """
    Creates and initializes a custom property of owner.

    The soft_min and soft_max parameters default to min and max.
    Description defaults to the property name.
    """

    # Some keyword argument defaults differ
    rna_idprop_ui_create(
        owner, name, default=default,
        min=min, max=max, soft_min=soft_min, soft_max=soft_max,
        description=description or name,
        overridable=overridable,
        subtype=subtype,
        **options  # noqa
    )


##############################################
# Driver creation utilities
##############################################

def _init_driver_target(drv_target: DriverTarget, var_info, target_id: Optional[ID]):
    """Initialize a driver variable target from a specification."""

    # Parse the simple list format for the common case.
    if isinstance(var_info, tuple):
        # [ (target_id,) subtarget, ...path ]

        # If target_id is supplied as parameter, allow omitting it
        if target_id is None or isinstance(var_info[0], bpy.types.ID):
            target_id, subtarget, *refs = var_info
        else:
            subtarget, *refs = var_info

        subtarget = force_lazy(subtarget)

        # Simple path string case.
        if len(refs) == 0:
            # [ (target_id,) path_str ]
            path = subtarget
        else:
            # If subtarget is a string, look up a bone in the target
            if isinstance(subtarget, str):
                subtarget = target_id.pose.bones[subtarget]

            if subtarget == target_id:
                path = ''
            else:
                path = subtarget.path_from_id()

            # Use ".foo" type path items verbatim, otherwise quote
            for item in refs:
                if isinstance(item, str):
                    path += item if item[0] == '.' else quote_property(item)
                else:
                    path += f'[{repr(item)}]'

            if path[0] == '.':
                path = path[1:]

        drv_target.id = target_id
        drv_target.data_path = path

    else:
        # { 'id': ..., ... }
        target_id = var_info.get('id', target_id)

        if target_id is not None:
            drv_target.id = target_id

        for tp, tv in var_info.items():
            setattr(drv_target, tp, force_lazy(tv))


def _add_driver_variable(drv: Driver, var_name: str, var_info, target_id: Optional[ID]):
    """Add and initialize a driver variable."""

    var = drv.variables.new()
    var.name = var_name

    # Parse the simple list format for the common case.
    if isinstance(var_info, tuple):
        # [ (target_id,) subtarget, ...path ]
        var.type = "SINGLE_PROP"

        _init_driver_target(var.targets[0], var_info, target_id)

    else:
        # Variable info as generic dictionary - assign properties.
        # { 'type': 'SINGLE_PROP', 'targets':[...] }
        var.type = var_info['type']

        for p, v in var_info.items():
            if p == 'targets':
                for i, tdata in enumerate(v):
                    _init_driver_target(var.targets[i], tdata, target_id)
            elif p != 'type':
                setattr(var, p, force_lazy(v))


# noinspection PyShadowingBuiltins
def make_driver(owner: bpy_struct, prop: str, *, index=-1, type='SUM',
                expression: Optional[str] = None,
                variables: Iterable | dict = (),
                polynomial: Optional[list[float]] = None,
                target_id: Optional[ID] = None) -> FCurve:
    """
    Creates and initializes a driver for the 'prop' property of owner.

    Arguments:
      owner           : object to add the driver to
      prop            : property of the object to add the driver to
      index           : item index for vector properties
      type            : built-in driver math operation (incompatible with expression)
      expression      : custom driver expression
      variables       : either a list or dictionary of variable specifications.
      polynomial      : coefficients of the POLYNOMIAL driver modifier
      target_id       : specifies the target ID of variables implicitly

    Specification format:
        If the variables argument is a dictionary, keys specify variable names.
        Otherwise, names are set to var, var1, var2, ... etc.:

          variables = [ ..., ..., ... ]
          variables = { 'var': ..., 'var1': ..., 'var2': ... }

        Variable specifications are constructed as nested dictionaries and lists that
        follow the property structure of the original Blender objects, but the most
        common case can be abbreviated as a simple tuple.

        The following specifications are equivalent:

          ( target, subtarget, '.foo', 'bar' )

          { 'type': 'SINGLE_PROP', 'targets':[( target, subtarget, '.foo', 'bar' )] }

          { 'type': 'SINGLE_PROP',
            'targets':[{ 'id': target, 'data_path': subtarget.path_from_id() + '.foo["bar"]' }] }

        If subtarget is as string, it is automatically looked up within target as a bone.

        It is possible to specify path directly as a simple string without following items:

          ( target, 'path' )

          { 'type': 'SINGLE_PROP', 'targets':[{ 'id': target, 'data_path': 'path' }] }

        If the target_id parameter is not None, it is possible to omit target:

          ( subtarget, '.foo', 'bar' )

          { 'type': 'SINGLE_PROP',
            'targets':[{ 'id': target_id, 'data_path': subtarget.path_from_id() + '.foo["bar"]' }] }

    Returns the newly created driver FCurve.

    Target bone names can be provided via 'lazy' callable closures without arguments.
    """
    fcu = owner.driver_add(prop, index)
    drv = fcu.driver

    if expression is not None:
        drv.type = 'SCRIPTED'
        drv.expression = expression
    else:
        drv.type = type

    # In case the driver already existed, remove contents
    for var in list(drv.variables):
        drv.variables.remove(var)

    for mod in list(fcu.modifiers):
        fcu.modifiers.remove(mod)

    # Fill in new data
    if not isinstance(variables, dict):
        # variables = [ info, ... ]
        for i, var_info in enumerate(variables):
            var_name = 'var' if i == 0 else 'var' + str(i)
            _add_driver_variable(drv, var_name, var_info, target_id)
    else:
        # variables = { 'varname': info, ... }
        for var_name, var_info in variables.items():
            _add_driver_variable(drv, var_name, var_info, target_id)

    if polynomial is not None:
        drv_modifier = fcu.modifiers.new('GENERATOR')
        assert isinstance(drv_modifier, FModifierGenerator)
        drv_modifier.mode = 'POLYNOMIAL'
        drv_modifier.poly_order = len(polynomial)-1
        for i, v in enumerate(polynomial):
            drv_modifier.coefficients[i] = v

    return fcu


##############################################
# Driver variable utilities
##############################################

# noinspection PyShadowingBuiltins
def driver_var_transform(target: ID, bone: Optional[str] = None, *,
                         type='LOC_X', space='WORLD', rotation_mode='AUTO'):
    """
    Create a Transform Channel driver variable specification.

    Usage:
        make_driver(..., variables=[driver_var_transform(...)])

    Target bone name can be provided via a 'lazy' callable closure without arguments.
    """

    assert space in {'WORLD', 'TRANSFORM', 'LOCAL'}

    target_map = {
        'id': target,
        'transform_type': type,
        'transform_space': space + '_SPACE',
        'rotation_mode': rotation_mode,
    }

    if bone is not None:
        target_map['bone_target'] = bone

    return {'type': 'TRANSFORMS', 'targets': [target_map]}


def driver_var_distance(target: ID, *,
                        bone1: Optional[str] = None,
                        target2: Optional[ID] = None,
                        bone2: Optional[str] = None,
                        space1='WORLD', space2='WORLD'):
    """
    Create a Distance driver variable specification.

    Usage:
        make_driver(..., variables=[driver_var_distance(...)])

    Target bone name can be provided via a 'lazy' callable closure without arguments.
    """

    assert space1 in {'WORLD', 'TRANSFORM', 'LOCAL'}
    assert space2 in {'WORLD', 'TRANSFORM', 'LOCAL'}

    target1_map = {
        'id': target,
        'transform_space': space1 + '_SPACE',
    }

    if bone1 is not None:
        target1_map['bone_target'] = bone1

    target2_map = {
        'id': target2 or target,
        'transform_space': space2 + '_SPACE',
    }

    if bone2 is not None:
        target2_map['bone_target'] = bone2

    return {'type': 'LOC_DIFF', 'targets': [target1_map, target2_map]}


##############################################
# Constraint management
##############################################

def move_constraint(source: Object | PoseBone, target: Object | PoseBone | str, con: Constraint):
    """
    Move a constraint from one owner to another, together with drivers.
    """

    assert source.constraints[con.name] == con

    if isinstance(target, str):
        target = con.id_data.pose.bones[target]

    con_tgt = target.constraints.copy(con)

    if target.id_data == con.id_data:
        adt = con.id_data.animation_data
        if adt:
            prefix = con.path_from_id()
            new_prefix = con_tgt.path_from_id()
            for fcu in adt.drivers:
                if fcu.data_path.startswith(prefix):
                    fcu.data_path = new_prefix + fcu.data_path[len(prefix):]

    source.constraints.remove(con)


def move_all_constraints(obj: Object,
                         source: Object | PoseBone | str,
                         target: Object | PoseBone | str, *,
                         prefix=''):
    """
    Move all constraints with the specified name prefix from one bone to another.
    """

    if isinstance(source, str):
        source = obj.pose.bones[source]
    if isinstance(target, str):
        target = obj.pose.bones[target]

    for con in list(source.constraints):
        if con.name.startswith(prefix):
            move_constraint(source, target, con)


##############################################
# Custom property management
##############################################

def deactivate_custom_properties(obj: bpy_struct, *, reset=True):
    """Disable drivers on custom properties and reset values to default."""

    prefix = '["'

    if obj != obj.id_data:
        prefix = obj.path_from_id() + prefix

    adt = obj.id_data.animation_data
    if adt:
        for fcu in adt.drivers:
            if fcu.data_path.startswith(prefix):
                fcu.mute = True

    if reset:
        for key, value in obj.items():
            val_type = type(value)
            if val_type in {int, float}:
                ui_data = obj.id_properties_ui(key)
                rna_data = ui_data.as_dict()
                obj[key] = val_type(rna_data.get("default", 0))


def reactivate_custom_properties(obj: bpy_struct):
    """Re-enable drivers on custom properties."""

    prefix = '["'

    if obj != obj.id_data:
        prefix = obj.path_from_id() + prefix

    adt = obj.id_data.animation_data
    if adt:
        for fcu in adt.drivers:
            if fcu.data_path.startswith(prefix):
                fcu.mute = False


def copy_custom_properties(src, dest, *, prefix='', dest_prefix='',
                           link_driver=False, overridable=True) -> list[tuple[str, str, Any]]:
    """Copy custom properties with filtering by prefix. Optionally link using drivers."""
    res = []

    # Exclude addon-defined properties.
    exclude = {prop.identifier for prop in src.bl_rna.properties if prop.is_runtime}

    for key, value in src.items():
        if key.startswith(prefix) and key not in exclude:
            new_key = dest_prefix + key[len(prefix):]

            try:
                ui_data_src = src.id_properties_ui(key)
            except TypeError:
                # Some property types, e.g. Python dictionaries
                # don't support id_properties_ui.
                continue

            if src != dest or new_key != key:
                dest[new_key] = value

                dest.id_properties_ui(new_key).update_from(ui_data_src)

                if link_driver:
                    make_driver(src, quote_property(key), variables=[(dest.id_data, dest, new_key)])

            if overridable:
                dest.property_overridable_library_set(quote_property(new_key), True)

            res.append((key, new_key, value))

    return res


def copy_custom_properties_with_ui(rig: 'BaseRig', src, dest_bone, *, ui_controls=None, **options):
    """Copy custom properties, and create rig UI for them."""
    if isinstance(src, str):
        src = rig.get_bone(src)

    bone: PoseBone = rig.get_bone(dest_bone)
    mapping = copy_custom_properties(src, bone, **options)

    if mapping:
        panel = rig.script.panel_with_selected_check(rig, ui_controls or rig.bones.flatten('ctrl'))

        for key, new_key, value in sorted(mapping, key=lambda item: item[1]):
            name = new_key

            # Replace delimiters with spaces
            if ' ' not in name:
                name = re.sub(r'[_.-]', ' ', name)
            # Split CamelCase
            if ' ' not in name:
                name = re.sub(r'([a-z])([A-Z])', r'\1 \2', name)
            # Capitalize
            if name.lower() == name:
                name = name.title()

            info = bone.id_properties_ui(new_key).as_dict()
            slider = type(value) is float and info and info.get("min", None) == 0 and info.get("max", None) == 1

            panel.custom_prop(dest_bone, new_key, text=name, slider=slider)

    return mapping


##############################################
# Driver management
##############################################

def refresh_drivers(obj):
    """Cause all drivers belonging to the object to be re-evaluated, clearing any errors."""

    # Refresh object's own drivers if any
    anim_data: Optional[AnimData] = getattr(obj, 'animation_data', None)

    if anim_data:
        for fcu in anim_data.drivers:
            # Make a fake change to the driver
            fcu.driver.type = fcu.driver.type

    # Material node trees aren't in any lists
    if isinstance(obj, Material):
        refresh_drivers(obj.node_tree)


def refresh_all_drivers():
    """Cause all drivers in the file to be re-evaluated, clearing any errors."""

    # Iterate over all data blocks in the file
    for attr in dir(bpy.data):
        coll = getattr(bpy.data, attr, None)

        if isinstance(coll, bpy_prop_collection):
            for item in coll:
                refresh_drivers(item)


##############################################
# Utility mixin
##############################################

class MechanismUtilityMixin(object):
    obj: ArmatureObject

    """
    Provides methods for more convenient creation of constraints, properties
    and drivers within an armature (by implicitly providing context).

    Requires self.obj to be the armature object being worked on.
    """

    def make_constraint(self, bone: str, con_type: str,
                        subtarget: OptionalLazy[str] = None, *,
                        insert_index: Optional[int] = None,
                        space: Optional[str] = None,
                        track_axis: Optional[str] = None,
                        use_xyz: Optional[Sequence[bool]] = None,
                        use_limit_xyz: Optional[Sequence[bool]] = None,
                        invert_xyz: Optional[Sequence[bool]] = None,
                        targets: Optional[list[Lazy[str | tuple | dict]]] = None,
                        **args):
        assert(self.obj.mode == 'OBJECT')
        return make_constraint(
            self.obj.pose.bones[bone], con_type, self.obj, subtarget,
            insert_index=insert_index, space=space, track_axis=track_axis,
            use_xyz=use_xyz, use_limit_xyz=use_limit_xyz, invert_xyz=invert_xyz,
            targets=targets,
            **args)

    # noinspection PyShadowingBuiltins
    def make_property(self, bone: str, name: str, default, *,
                      min: float = 0, max: float = 1, soft_min=None, soft_max=None,
                      description: Optional[str] = None, overridable=True,
                      subtype: Optional[str] = None,
                      **args):
        assert(self.obj.mode == 'OBJECT')
        return make_property(
            self.obj.pose.bones[bone], name, default,
            min=min, max=max, soft_min=soft_min, soft_max=soft_max,
            description=description, overridable=overridable, subtype=subtype,
            **args
        )

    # noinspection PyShadowingBuiltins
    def make_driver(self, owner: str | bpy_struct, prop: str,
                    index=-1, type='SUM',
                    expression: Optional[str] = None,
                    variables: Iterable | dict = (),
                    polynomial: Optional[list[float]] = None):
        assert(self.obj.mode == 'OBJECT')
        if isinstance(owner, str):
            owner = self.obj.pose.bones[owner]
        return make_driver(
            owner, prop, target_id=self.obj,
            index=index, type=type, expression=expression,
            variables=variables, polynomial=polynomial,
        )
