# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
JunctionModuleHandle creates a module whose sub-modules are not located
in the same directory on the file-system as usual. Instead the sub-modules are
added into the package from different locations on the file-system.

The ``JunctionModuleHandle`` class is used to manipulate sub-modules at run-time.
This is needed to implement package management functionality, repositories can be added/removed at run-time.
"""

__all__ = (
    "JunctionModuleHandle",
)

import sys
from types import ModuleType
from typing import (
    Dict,
    Optional,
    Sequence,
    Tuple,
)


def _module_file_set(module: ModuleType, name_full: str) -> None:
    # File is just an identifier, as this doesn't reference an actual file,
    # it just needs to be descriptive.
    module.__name__ = name_full
    module.__package__ = name_full
    module.__file__ = "[{:s}]".format(name_full)


def _module_create(
        name: str,
        *,
        parent: Optional[ModuleType] = None,
        doc: Optional[str] = None,
) -> ModuleType:
    if parent is not None:
        name_full = parent.__name__ + "." + name
    else:
        name_full = name

    module = ModuleType(name, doc)
    _module_file_set(module, name_full)
    if parent is not None:
        setattr(parent, name, module)
    return module


class JunctionModuleHandle:
    __slots__ = (
        "_module_name",
        "_module",
        "_submodules",
    )

    def __init__(self, module_name: str):
        self._module_name: str = module_name
        self._module: Optional[ModuleType] = None
        self._submodules: Dict[str, ModuleType] = {}

    def submodule_items(self) -> Sequence[Tuple[str, ModuleType]]:
        return tuple(self._submodules.items())

    def register_module(self) -> ModuleType:
        """
        Register the base module in ``sys.modules``.
        """
        if self._module is not None:
            raise Exception("Module {!r} already registered!".format(self._module))
        if self._module_name in sys.modules:
            raise Exception("Module {:s} already in 'sys.modules'!".format(self._module_name))

        module = _module_create(self._module_name)
        sys.modules[self._module_name] = module

        # Differentiate this, and allow access to the factory (may be useful).
        # `module.__module_factory__ = self`

        self._module = module
        return module

    def unregister_module(self) -> None:
        """
        Unregister the base module in ``sys.modules``.
        Keep everything except the modules name (allowing re-registration).
        """
        # Cleanup `sys.modules`.
        sys.modules.pop(self._module_name, None)
        for submodule_name in self._submodules.keys():
            sys.modules.pop("{:s}.{:s}".format(self._module_name, submodule_name), None)

        # Remove from self.
        self._submodules.clear()
        self._module = None

    def register_submodule(self, submodule_name: str, dirpath: str) -> ModuleType:
        name_full = self._module_name + "." + submodule_name
        if self._module is None:
            raise Exception("Module not registered, cannot register a submodule!")
        if submodule_name in self._submodules:
            raise Exception("Module \"{:s}\" already registered!".format(submodule_name))
        # Register.
        submodule = _module_create(submodule_name, parent=self._module)
        sys.modules[name_full] = submodule

        submodule.__path__ = [dirpath]
        setattr(self._module, submodule_name, submodule)
        self._submodules[submodule_name] = submodule
        return submodule

    def unregister_submodule(self, submodule_name: str) -> None:
        name_full = self._module_name + "." + submodule_name
        if self._module is None:
            raise Exception("Module not registered, cannot register a submodule!")
        # Unregister.
        submodule = self._submodules.pop(submodule_name, None)
        if submodule is None:
            raise Exception("Module \"{:s}\" not registered!".format(submodule_name))
        delattr(self._module, submodule_name)
        del sys.modules[name_full]

        # Remove all sub-modules, to prevent them being reused in the future.
        #
        # While it might not seem like a problem to keep these around it means if a module
        # with the same name is registered later, importing sub-modules uses the cached values
        # from `sys.modules` and does *not* assign the module to the name-space of the new `submodule`.
        # This isn't exactly a bug, it's often assumed that inspecting a module
        # is a way to find its sub-modules, using `dir(submodule)` for example.
        # For more technical example `sys.modules["foo.bar"] == sys.modules["foo"].bar`
        # which can fail with and attribute error unless the modules are cleared here.
        #
        # An alternative solution could be re-attach sub-modules to the modules name-space when its re-registered.
        # This has some advantages since the module doesn't have to be re-imported however it has the down
        # side that stale data would be kept in `sys.modules` unnecessarily in many cases.
        name_full_prefix = name_full + "."
        submodule_name_list = [
            submodule_name for submodule_name in sys.modules.keys()
            if submodule_name.startswith(name_full_prefix)
        ]
        for submodule_name in submodule_name_list:
            del sys.modules[submodule_name]

    def rename_submodule(self, submodule_name_src: str, submodule_name_dst: str) -> None:
        name_full_prev = self._module_name + "." + submodule_name_src
        name_full_next = self._module_name + "." + submodule_name_dst

        submodule = self._submodules.pop(submodule_name_src)
        self._submodules[submodule_name_dst] = submodule

        delattr(self._module, submodule_name_src)
        setattr(self._module, submodule_name_dst, submodule)

        _module_file_set(submodule, name_full_next)

        del sys.modules[name_full_prev]
        sys.modules[name_full_next] = submodule

    def rename_directory(self, submodule_name: str, dirpath: str) -> None:
        # TODO: how to deal with existing loaded modules?
        # In practice this is mostly users setting up directories for the first time.
        submodule = self._submodules[submodule_name]
        submodule.__path__ = [dirpath]
