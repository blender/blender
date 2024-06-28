# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import collections

from types import FunctionType
from itertools import chain
from typing import Collection, Callable


##############################################
# Class With Stages
##############################################

def rigify_stage(stage):
    """Decorates the method with the specified stage."""
    def process(method: FunctionType):
        if not isinstance(method, FunctionType):
            raise ValueError("Stage decorator must be applied to a method definition")
        method._rigify_stage = stage
        return method
    return process


class StagedMetaclass(type):
    """
    Metaclass for rigs that manages assignment of methods to stages via @stage.* decorators.

    Using define_stages=True in the class definition will register all non-system
    method names from that definition as valid stages. After that, subclasses can
    register methods to those stages, to be called via rigify_invoke_stage.
    """
    def __new__(mcs, class_name, bases, namespace, define_stages=None, **kwargs):
        # suppress keyword args to avoid issues with __init_subclass__
        return super().__new__(mcs, class_name, bases, namespace, **kwargs)

    def __init__(cls, class_name, bases, namespace, define_stages=None, **kwargs):
        super().__init__(class_name, bases, namespace, **kwargs)

        # Compute the set of stages defined by this class
        if not define_stages:
            define_stages = []

        elif define_stages is True:
            define_stages = [
                name for name, item in namespace.items()
                if name[0] != '_' and isinstance(item, FunctionType)
            ]

        cls.rigify_own_stages = frozenset(define_stages)

        # Compute complete set of inherited stages
        staged_bases = [cls for cls in reversed(cls.__mro__) if isinstance(cls, StagedMetaclass)]

        cls.rigify_stages = stages = frozenset(chain.from_iterable(
            cls.rigify_own_stages for cls in staged_bases
        ))

        # Compute the inherited stage to method mapping
        stage_map = collections.defaultdict(collections.OrderedDict)
        own_stage_map = collections.defaultdict(collections.OrderedDict)
        method_map = {}

        cls.rigify_own_stage_map = own_stage_map

        for base in staged_bases:
            for stage_name, methods in base.rigify_own_stage_map.items():
                for method_name, method_class in methods.items():
                    if method_name in stages:
                        raise ValueError(
                            f"Stage method '{method_name}' inherited @stage.{stage_name} "
                            f"in class {class_name} ({cls.__module__})")

                    # Check consistency of inherited stage assignment to methods
                    if method_name in method_map:
                        if method_map[method_name] != stage_name:
                            print(f"RIGIFY CLASS {class_name} ({cls.__module__}): "
                                  f"method '{method_name}' has inherited both "
                                  f"@stage.{method_map[method_name]} and @stage.{stage_name}\n")
                    else:
                        method_map[method_name] = stage_name

                    stage_map[stage_name][method_name] = method_class

        # Scan newly defined methods for stage decorations
        for method_name, item in namespace.items():
            if isinstance(item, FunctionType):
                stage = getattr(item, '_rigify_stage', None)

                if stage and method_name in stages:
                    print(f"RIGIFY CLASS {class_name} ({cls.__module__}): "
                          f"cannot use stage decorator on the stage method '{method_name}' "
                          f"(@stage.{stage} ignored)")
                    continue

                # Ensure that decorators aren't lost when redefining methods
                if method_name in method_map:
                    if not stage:
                        stage = method_map[method_name]
                        print(f"RIGIFY CLASS {class_name} ({cls.__module__}): "
                              f"missing stage decorator on method '{method_name}' "
                              f"(should be @stage.{stage})")
                    # Check that the method is assigned to only one stage
                    elif stage != method_map[method_name]:
                        print(f"RIGIFY CLASS {class_name} ({cls.__module__}): "
                              f"method '{method_name}' has decorator @stage.{stage}, "
                              f"but inherited base has @stage.{method_map[method_name]}")

                # Assign the method to the stage, verifying that it's valid
                if stage:
                    if stage not in stages:
                        raise ValueError(
                            f"Invalid stage name '{stage}' for method '{method_name}' "
                            f"in class {class_name} ({cls.__module__})")
                    else:
                        stage_map[stage][method_name] = cls
                        own_stage_map[stage][method_name] = cls

        cls.rigify_stage_map = stage_map

    def make_stage_decorators(self) -> list[tuple[str, Callable]]:
        return [(name, rigify_stage(name)) for name in self.rigify_stages]

    def stage_decorator_container(self, cls):
        for name, stage in self.make_stage_decorators():
            setattr(cls, name, stage)
        return cls


class BaseStagedClass(object, metaclass=StagedMetaclass):
    rigify_sub_objects: Collection['BaseStagedClass'] = tuple()
    rigify_sub_object_run_late = False

    def rigify_invoke_stage(self, stage: str):
        """Call all methods decorated with the given stage, followed by the callback."""
        cls = self.__class__
        assert isinstance(cls, StagedMetaclass)
        assert stage in cls.rigify_stages

        getattr(self, stage)()

        for sub in self.rigify_sub_objects:
            if not sub.rigify_sub_object_run_late:
                sub.rigify_invoke_stage(stage)

        for method_name in cls.rigify_stage_map[stage]:
            getattr(self, method_name)()

        for sub in self.rigify_sub_objects:
            if sub.rigify_sub_object_run_late:
                sub.rigify_invoke_stage(stage)


##############################################
# Per-owner singleton class
##############################################

class SingletonPluginMetaclass(StagedMetaclass):
    """Metaclass for maintaining one instance per owner object per constructor arg set."""
    def __call__(cls, owner, *constructor_args):
        key = (cls, *constructor_args)
        try:
            return owner.plugin_map[key]
        except KeyError:
            new_obj = super().__call__(owner, *constructor_args)
            owner.plugin_map[key] = new_obj
            owner.plugin_list.append(new_obj)
            owner.plugin_list.sort(key=lambda obj: obj.priority, reverse=True)
            return new_obj
