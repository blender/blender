# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import collections

from bpy.types import PoseBone, UILayout, Context
from typing import TYPE_CHECKING, Any, Callable, Optional, TypeVar, Generic

from .utils.errors import RaiseErrorMixin
from .utils.bones import BoneDict, BoneUtilityMixin, TypedBoneDict, BaseBoneDict
from .utils.mechanism import MechanismUtilityMixin
from .utils.metaclass import BaseStagedClass
from .utils.misc import ArmatureObject
from .utils.rig import get_rigify_params

if TYPE_CHECKING:
    from .base_generate import BaseGenerator
    from .rig_ui_template import ScriptGenerator


##############################################
# Base Rig
##############################################

class GenerateCallbackHost(BaseStagedClass, define_stages=True):
    """
    Standard set of callback methods to redefine.
    Shared between BaseRig and GeneratorPlugin.

    These callbacks are called in this order; every one is
    called for all rigs before proceeding to the next stage.

    Switching modes is not allowed in rigs for performance
    reasons. Place code in the appropriate callbacks to use
    the mode set by the main engine.

    After each callback, all other methods decorated with
    @stage.<method_name> are called, for instance:

    def generate_bones(self):
        print('first')

    @stage.generate_bones
    def foo(self):
        print('second')

    Will print 'first', then 'second'. Multiple methods in the
    same stage are called in the order they are first defined;
    in case of inheritance, the class bodies are scanned in
    reverse MRO order. E.g.:

    class Base(...):
        @stage.generate_bones
        def first(self):...

        @stage.generate_bones
        def second(self):...

    class Derived(Base):
        @stage.generate_bones
        def third(self):...

        # Was first defined in Base so still first:
        @stage.generate_bones
        def first(self):...

        @stage.generate_bones
        def fourth(self):...

    Multiple inheritance can make this ordering confusing, so it
    is best to avoid it.

    When overriding such methods in a subclass the appropriate
    decorator should be repeated for code clarity reasons;
    a warning is printed if this is not done.
    """
    def initialize(self):
        """
        Initialize processing after all rig classes are constructed.
        Called in Object mode. May not change the armature.
        """
        pass

    def prepare_bones(self):
        """
        Prepare ORG bones for generation, e.g. align them.
        Called in Edit mode. May not add bones.
        """
        pass

    def generate_bones(self):
        """
        Create all bones.
        Called in Edit mode.
        """
        pass

    def parent_bones(self):
        """
        Parent all bones and set other edit mode properties.
        Called in Edit mode. May not add bones.
        """
        pass

    def configure_bones(self):
        """
        Configure bone properties, e.g. transform locks, layers etc.
        Called in Object mode. May not do Edit mode operations.
        """
        pass

    def preapply_bones(self):
        """
        Read bone matrices for applying to edit mode.
        Called in Object mode. May not do Edit mode operations.
        """
        pass

    def apply_bones(self):
        """
        Can be used to apply some constraints to rest pose, and for final parenting.
        Called in Edit mode. May not add bones.
        """
        pass

    def rig_bones(self):
        """
        Create and configure all constraints, drivers etc.
        Called in Object mode. May not do Edit mode operations.
        """
        pass

    def generate_widgets(self):
        """
        Create all widget objects.
        Called in Object mode. May not do Edit mode operations.
        """
        pass

    def finalize(self):
        """
        Finishing touches to the construction of the rig.
        Called in Object mode. May not do Edit mode operations.
        """
        pass


_Org = TypeVar('_Org', bound=str | list[str] | BaseBoneDict)
_Ctrl = TypeVar('_Ctrl', bound=str | list[str] | BaseBoneDict)
_Mch = TypeVar('_Mch', bound=str | list[str] | BaseBoneDict)
_Deform = TypeVar('_Deform', bound=str | list[str] | BaseBoneDict)


class BaseRigMixin(RaiseErrorMixin, BoneUtilityMixin, MechanismUtilityMixin):
    generator: 'BaseGenerator'

    obj: ArmatureObject
    script: 'ScriptGenerator'
    base_bone: str
    params: Any

    rigify_parent: Optional['BaseRig']
    rigify_children: list['BaseRig']
    rigify_org_bones: set[str]
    rigify_child_bones: set[str]
    rigify_new_bones: dict[str, Optional[str]]
    rigify_derived_bones: dict[str, set[str]]

    ##############################################
    # Annotated bone containers

    class ToplevelBones(TypedBoneDict, Generic[_Org, _Ctrl, _Mch, _Deform]):
        org: _Org
        ctrl: _Ctrl
        mch: _Mch
        deform: _Deform

    class CtrlBones(TypedBoneDict):
        pass

    class MchBones(TypedBoneDict):
        pass

    # Subclass and use the above CtrlBones and MchBones classes in overrides.
    # It is necessary to reference them via absolute strings, e.g. 'Rig.CtrlBones',
    # because when using just CtrlBones the annotation won't work fully in subclasses
    # of the rig class in PyCharm (no warnings about unknown attribute access).
    bones: ToplevelBones[str | list[str] | BoneDict,
                         str | list[str] | BoneDict,
                         str | list[str] | BoneDict,
                         str | list[str] | BoneDict]


class BaseRig(GenerateCallbackHost, BaseRigMixin):
    """
    Base class for all rigs.

    The main weak areas in the legacy (pre-2.76b) Rigify rig class structure
    was that there were no provisions for intelligent interactions
    between rigs, and all processing was done via one generate
    method, necessitating frequent expensive mode switches.

    This structure fixes those problems by providing a mandatory
    base class that hold documented connections between rigs, bones,
    and the common generator object. The generation process is also
    split into multiple stages.
    """
    def __init__(self, generator: 'BaseGenerator', pose_bone: PoseBone):
        self.generator = generator

        self.obj = generator.obj
        self.script = generator.script
        self.base_bone = pose_bone.name
        self.params = get_rigify_params(pose_bone)

        # Collection of bone names for use in implementing the rig
        self.bones = self.ToplevelBones(
            # ORG bone names
            org=self.find_org_bones(pose_bone),
            # Control bone names
            ctrl=BoneDict(),
            # MCH bone names
            mch=BoneDict(),
            # DEF bone names
            deform=BoneDict(),
        )

        # Data useful for complex rig interaction:
        # Parent-child links between rigs.
        self.rigify_parent = None
        self.rigify_children = []
        # ORG bones directly owned by the rig.
        self.rigify_org_bones = set(self.bones.flatten('org'))
        # Children of bones owned by the rig.
        self.rigify_child_bones = set()
        # Bones created by the rig (mapped to original names)
        self.rigify_new_bones = dict()
        self.rigify_derived_bones = collections.defaultdict(set)

    def register_new_bone(self, new_name: str, old_name: Optional[str] = None):
        """Registers this rig as the owner of this new bone."""
        self.rigify_new_bones[new_name] = old_name
        self.generator.bone_owners[new_name] = self
        if old_name:
            self.rigify_derived_bones[old_name].add(new_name)
            self.generator.derived_bones[old_name].add(new_name)

    ###########################################################
    # Bone ownership

    def find_org_bones(self, pose_bone: PoseBone) -> str | list[str] | BaseBoneDict:
        """
        Select bones directly owned by the rig. Returning the
        same bone from multiple rigs is an error.

        May return a single name, a list, or a BoneDict.

        Called in Object mode, may not change the armature.
        """
        return [pose_bone.name]

    ###########################################################
    # Parameters and UI

    @classmethod
    def add_parameters(cls, params):
        """
        This method add more parameters to params
        :param params: rigify_parameters of a pose_bone
        :return:
        """
        pass

    @classmethod
    def parameters_ui(cls, layout: UILayout, params):
        """
        This method draws the UI of the rigify_parameters defined on the pose_bone
        :param layout:
        :param params:
        :return:
        """
        pass

    @classmethod
    def on_parameter_update(cls, context: Context, pose_bone: PoseBone, params, param_name: str):
        """
        A callback invoked whenever a parameter value is changed by the user.
        """


##############################################
# Rig Utility
##############################################


class RigUtility(BoneUtilityMixin, MechanismUtilityMixin):
    """Base class for utility classes that generate part of a rig."""
    def __init__(self, owner):
        self.owner = owner
        self.obj = owner.obj

    def register_new_bone(self, new_name: str, old_name: Optional[str] = None):
        self.owner.register_new_bone(new_name, old_name)


class LazyRigComponent(GenerateCallbackHost, RigUtility):
    """Base class for utility classes that generate part of a rig using callbacks. Starts as disabled."""
    def __init__(self, owner):
        super().__init__(owner)

        self.is_component_enabled = False

    def enable_component(self):
        if not self.is_component_enabled:
            self.is_component_enabled = True
            self.owner.rigify_sub_objects = objects = self.owner.rigify_sub_objects or []
            objects.append(self)


class RigComponent(LazyRigComponent):
    """Base class for utility classes that generate part of a rig using callbacks."""
    def __init__(self, owner):
        super().__init__(owner)
        self.enable_component()


##############################################
# Rig Stage Decorators
##############################################

# noinspection PyPep8Naming
@GenerateCallbackHost.stage_decorator_container
class stage:
    """Contains @stage.<...> decorators for all valid stages."""
    # Declare stages for auto-completion - doesn't affect execution.
    initialize: Callable
    prepare_bones: Callable
    generate_bones: Callable
    parent_bones: Callable
    configure_bones: Callable
    preapply_bones: Callable
    apply_bones: Callable
    rig_bones: Callable
    generate_widgets: Callable
    finalize: Callable
