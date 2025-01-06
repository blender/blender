# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import os
import sys
import pathlib
import importlib.util
import contextlib
import traceback


# For some reasons, these two modules do not work with 'file-path-based' import method used by this script.
# So 'pre-load' them here instead.
# They are _not used_ directly  by this script.
import multiprocessing
import typing


# Allow-list of directories, relative to the given `--root-dir` argument.
INCLUDED_DIRS = {
    "build_files",
    # Used to generate the manual and API documentations.
    "doc",
}

# Block-list of paths to python modules, relative to the given `--root-dir` argument.
EXCLUDED_FILE_PATHS = {
    # Require `bpy` module.
    "doc/python_api/sphinx_doc_gen.py",

    # XXX These scripts execute on import! bad, need to be fixed or removed.
    # FIXME: Should be reasonably trivial to fix/cleanup for most of them.
    "doc/python_api/conf.py",
}


@contextlib.contextmanager
def add_to_sys_path(syspath):
    old_path = sys.path
    old_modules = sys.modules
    sys.modules = old_modules.copy()
    sys.path = sys.path[:]
    sys.path.insert(0, str(syspath))
    try:
        yield
    finally:
        sys.path = old_path
        sys.modules = old_modules


def import_module(file_path):
    """Returns `...` if not a python module, `True` if it's a package, `False` otherwise."""
    file_path = pathlib.Path(file_path)
    if file_path.suffix != ".py":
        return ...
    file_name = file_path.name
    if not file_name:
        return ...
    is_package = file_name == "__init__.py"
    if is_package:
        assert (len(file_path.parts) >= 2)
        module_name = file_path.parts[-2]
    else:
        module_name = file_path.stem

    with add_to_sys_path(file_path.parent):
        spec = importlib.util.spec_from_file_location(
            module_name, file_path, submodule_search_locations=file_path.parent)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)

    return module_name, is_package


def import_modules(root_dir):
    print("+++", sys.executable)

    included_directories = [os.path.join(root_dir, p) for p in INCLUDED_DIRS]
    excluded_file_paths = {os.path.join(root_dir, p) for p in EXCLUDED_FILE_PATHS}

    has_failures = False
    directories = included_directories[:]
    while directories:
        path = directories.pop(0)
        sub_directories = []
        is_package = False
        with os.scandir(path) as it:
            for entry in it:
                if entry.is_dir():
                    if entry.name.startswith('.'):
                        continue
                    sub_directories.append(entry.path)
                    continue
                if not entry.is_file():
                    continue
                if not entry.name.endswith(".py"):
                    continue
                if entry.path in excluded_file_paths:
                    continue
                try:
                    is_current_package = import_module(entry.path)
                    is_package = is_package or is_current_package
                except SystemExit as e:
                    has_failures = True
                    print(f"+++ Failed to import {entry.path} (module called `sys.exit`), {e}")
                except Exception as e:
                    has_failures = True
                    print(f"+++ Failed to import {entry.path} ({e.__class__}), {e}")
                    traceback.print_tb(e.__traceback__)
                    print("\n\n")
        # Do not attempt to import individual modules of a package. For now assume that if the top-level package can
        # be imported, it is good enough. This may have to be revisited at some point though. Currently there are
        # no packages in target directories anyway.
        if not is_package:
            directories += sub_directories

    if has_failures:
        raise Exception("Some module imports failed")


def main():
    import sys
    if sys.argv[1] == "--root-dir":
        root_dir = sys.argv[2]
        import_modules(root_dir=root_dir)
    else:
        raise Exception("Missing --root-dir parameter")


if __name__ == "__main__":
    main()
