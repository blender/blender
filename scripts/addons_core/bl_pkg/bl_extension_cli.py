# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Command line access for extension operations see:

   blender --command extension --help
"""

__all__ = (
    "cli_extension_handler",
)

import argparse
import os
import sys

from typing import (
    Any,
)

from .bl_extension_utils import PkgManifest_Normalized

show_color = (
    False if os.environ.get("NO_COLOR") else
    sys.stdout.isatty()
)


if show_color:
    color_codes = {
        # Not colors, useful all the same.
        'bold': '\033[0;1m',
        'faint': '\033[0;2m',

        'black': '\033[0;30m',
        'bright_gray': '\033[0;37m',
        'blue': '\033[0;34m',
        'white': '\033[1;37m',
        'green': '\033[0;32m',
        'bright_blue': '\033[1;34m',
        'cyan': '\033[0;36m',
        'bright_green': '\033[1;32m',
        'red': '\033[0;31m',
        'bright_cyan': '\033[1;36m',
        'purple': '\033[0;35m',
        'bright_red': '\033[1;31m',
        'yellow': '\033[0;33m',
        'bright_purple': '\033[1;35m',
        'dark_gray': '\033[1;30m',
        'bright_yellow': '\033[1;33m',
        'normal': '\033[0m',
    }

    def colorize(text: str, color: str) -> str:
        return color_codes[color] + text + color_codes["normal"]
else:
    def colorize(text: str, color: str) -> str:
        _ = color
        return text

# -----------------------------------------------------------------------------
# Wrap Operators


def blender_preferences_write() -> bool:
    import bpy  # type: ignore
    try:
        ok = 'FINISHED' in bpy.ops.wm.save_userpref()
    except RuntimeError as ex:
        print("Failed to write preferences: {!r}".format(ex))
        ok = False
    return ok


# -----------------------------------------------------------------------------
# Argument Implementation (Utilities)

class subcmd_utils:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def sync(
            *,
            show_done: bool = True,
    ) -> bool:
        import bpy
        try:
            bpy.ops.extensions.repo_sync_all()
            if show_done:
                sys.stdout.write("Done...\n\n")
        except Exception:
            print("Error synchronizing")
            import traceback
            traceback.print_exc()
            return False
        return True

    @staticmethod
    def _expand_package_ids(
            packages: list[str],
            *,
            use_local: bool,
    ) -> list[tuple[int, str]] | str:
        # Takes a terse lists of package names and expands to repo index and name list,
        # returning an error string if any can't be resolved.
        from . import repo_cache_store_ensure
        from .bl_extension_ops import extension_repos_read

        repo_map = {}
        errors = []

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()
        for (
                repo_index,
                pkg_manifest,
        ) in enumerate(
            repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print)
            if use_local else
            repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print)
        ):
            # Show any exceptions created while accessing the JSON,
            repo = repos_all[repo_index]
            if pkg_manifest is None:
                if use_local and not repo.remote_url:
                    errors.append("Repository \"{:s}\" has no data, sync may be needed!".format(repo.module))
                repo_packages = set()
            else:
                repo_packages = set(pkg_manifest.keys())
            repo_map[repo.module] = (repo_index, repo_packages)

        repos_and_packages = []

        for pkg_id_full in packages:
            repo_id, pkg_id = pkg_id_full.rpartition(".")[0::2]
            if not pkg_id:
                errors.append("Malformed package name \"{:s}\", expected \"repo_id.pkg_id\"!".format(pkg_id_full))
                continue
            if repo_id:
                repo_index, _repo_packages = repo_map.get(repo_id, (-1, ()))
                if repo_index == -1:
                    errors.append("Repository \"{:s}\" not found in [{:s}]!".format(
                        repo_id,
                        ", ".join(sorted("\"{:s}\"".format(x) for x in repo_map.keys()))
                    ))
                    continue
            else:
                repo_index = -1
                for _repo_id_iter, (repo_index_iter, repo_packages_iter) in repo_map.items():
                    if pkg_id in repo_packages_iter:
                        repo_index = repo_index_iter
                        break
                if repo_index == -1:
                    if use_local:
                        errors.append("Package \"{:s}\" not installed in local repositories!".format(pkg_id))
                    else:
                        errors.append("Package \"{:s}\" not found in remote repositories!".format(pkg_id))
                    continue
            repos_and_packages.append((repo_index, pkg_id))

        if errors:
            return "\n".join(errors)

        return repos_and_packages

    @staticmethod
    def expand_package_ids_from_remote(packages: list[str]) -> list[tuple[int, str]] | str:
        return subcmd_utils._expand_package_ids(packages, use_local=False)

    @staticmethod
    def expand_package_ids_from_local(packages: list[str]) -> list[tuple[int, str]] | str:
        return subcmd_utils._expand_package_ids(packages, use_local=True)


# -----------------------------------------------------------------------------
# Argument Implementation (Queries)

class subcmd_query:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def list(
            *,
            sync: bool,
    ) -> bool:

        def list_item(
                pkg_id: str,
                item_local: PkgManifest_Normalized | None,
                item_remote: PkgManifest_Normalized | None,
                has_remote: bool,
                item_warnings: list[str],
        ) -> None:
            # Both can't be None.
            assert item_remote is not None or item_local is not None

            if item_remote is not None:
                item_version = item_remote.version
                if item_local is None:
                    item_local_version = None
                    is_outdated = False
                else:
                    item_local_version = item_local.version
                    is_outdated = item_local_version != item_version

                if item_local is not None:
                    if is_outdated:
                        status_info = " [{:s}]".format(colorize("outdated: {:s} -> {:s}".format(
                            item_local_version,
                            item_version,
                        ), "red"))
                    else:
                        status_info = " [{:s}]".format(colorize("installed", "green"))
                else:
                    status_info = ""
                item = item_remote
            else:
                # All local-only packages are installed,
                # if they're in a repository with a remote but no remote info - they're "orphan".
                status_info = " [{:s}]".format(
                    colorize("orphan", "yellow") if has_remote else
                    colorize("installed", "green")
                )
                assert isinstance(item_local, PkgManifest_Normalized)
                item = item_local

            print(
                "  {:s}{:s}: \"{:s}\", {:s}".format(
                    colorize(pkg_id, "bold"),
                    status_info,
                    item.name,
                    colorize(item.tagline or "<no tagline>", "faint"),
                ))

            if item_remote and item_remote.block:
                print("    Blocked:", colorize(item_remote.block.reason, "red"))

            if item_warnings:
                # Including all text on one line doesn't work well here,
                # add warnings below the package.
                for warning in item_warnings:
                    print("    " + colorize(warning, "red"))

        if sync:
            if not subcmd_utils.sync():
                return False

        # NOTE: exactly how this data is extracted is rather arbitrary.
        # This uses the same code paths as drawing code.
        from .bl_extension_ops import extension_repos_read
        from . import repo_cache_store_ensure

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        import addon_utils  # type: ignore

        # pylint: disable-next=protected-access
        extensions_warnings: dict[str, list[str]] = addon_utils._extensions_warnings_get()
        assert isinstance(extensions_warnings, dict)

        # Blocked and installed.
        blocked_and_installed_count = 0

        for repo_index, (
                pkg_manifest_local,
                pkg_manifest_remote,
        ) in enumerate(zip(
            repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
            repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print),
            strict=True,
        )):
            # Show any exceptions created while accessing the JSON,
            repo = repos_all[repo_index]

            print("Repository: \"{:s}\" (id={:s})".format(repo.name, repo.module))
            has_remote = repo.remote_url and (pkg_manifest_remote is not None)
            pkg_id_set = set((pkg_manifest_local or {}).keys()) | set((pkg_manifest_remote or {}).keys())
            for pkg_id in sorted(pkg_id_set):
                item_local = pkg_manifest_local.get(pkg_id) if (pkg_manifest_local is not None) else None
                item_remote = pkg_manifest_remote.get(pkg_id) if (pkg_manifest_remote is not None) else None
                item_warnings = extensions_warnings.get("bl_ext.{:s}.{:s}".format(repo.module, pkg_id), [])
                list_item(pkg_id, item_local, item_remote, has_remote, item_warnings)
                if item_local and item_remote and item_remote.block:
                    blocked_and_installed_count += 1
        sys.stdout.flush()

        if blocked_and_installed_count:
            sys.stderr.write("\n")
            sys.stderr.write(
                "  Warning: " +
                colorize("{:d} installed extension(s) are blocked!\n".format(blocked_and_installed_count), "red")
            )
            sys.stderr.write(
                "           " +
                colorize("Uninstall them to remove this message!\n", "red")
            )
            sys.stderr.write("\n")

        return True


# -----------------------------------------------------------------------------
# Argument Implementation (Packages)

class subcmd_pkg:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def update(
            *,
            sync: bool,
    ) -> bool:
        if sync:
            if not subcmd_utils.sync():
                return False

        import bpy
        try:
            bpy.ops.extensions.package_upgrade_all()
        except RuntimeError:
            return False  # The error will have been printed.
        return True

    @staticmethod
    def install(
            *,
            sync: bool,
            packages: list[str],
            enable_on_install: bool,
            no_prefs: bool,
    ) -> bool:
        if sync:
            if not subcmd_utils.sync():
                return False

        # Expand all package ID's.
        repos_and_packages = subcmd_utils.expand_package_ids_from_remote(packages)
        if isinstance(repos_and_packages, str):
            sys.stderr.write(repos_and_packages)
            sys.stderr.write("\n")
            return False

        import bpy
        for repo_index, pkg_id in repos_and_packages:
            bpy.ops.extensions.package_mark_set(
                repo_index=repo_index,
                pkg_id=pkg_id,
            )

        try:
            bpy.ops.extensions.package_install_marked(enable_on_install=enable_on_install)
        except RuntimeError:
            return False  # The error will have been printed.

        if not no_prefs:
            if enable_on_install:
                blender_preferences_write()

        return True

    @staticmethod
    def remove(
            *,
            packages: list[str],
            no_prefs: bool,
    ) -> bool:
        # Expand all package ID's.
        repos_and_packages = subcmd_utils.expand_package_ids_from_local(packages)
        if isinstance(repos_and_packages, str):
            sys.stderr.write(repos_and_packages)
            sys.stderr.write("\n")
            return False

        import bpy
        for repo_index, pkg_id in repos_and_packages:
            bpy.ops.extensions.package_mark_set(repo_index=repo_index, pkg_id=pkg_id)

        try:
            bpy.ops.extensions.package_uninstall_marked()
        except RuntimeError:
            return False  # The error will have been printed.

        if not no_prefs:
            blender_preferences_write()

        return True

    @staticmethod
    def install_file(
            *,
            filepath: str,
            repo_id: str,
            enable_on_install: bool,
            no_prefs: bool,
    ) -> bool:
        import bpy

        # Blender's operator requires an absolute path.
        filepath = os.path.abspath(filepath)

        try:
            bpy.ops.extensions.package_install_files(
                filepath=filepath,
                repo=repo_id,
                enable_on_install=enable_on_install,
            )
        except RuntimeError:
            return False  # The error will have been printed.
        except Exception as ex:
            sys.stderr.write(str(ex))
            sys.stderr.write("\n")

        if not no_prefs:
            if enable_on_install:
                blender_preferences_write()

        return True


# -----------------------------------------------------------------------------
# Argument Implementation (Repositories)

class subcmd_repo:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def list() -> bool:
        from .bl_extension_ops import extension_repos_read
        repos_all = extension_repos_read()
        for repo in repos_all:
            print("{:s}:".format(repo.module))
            print("    name: \"{:s}\"".format(repo.name))
            print("    directory: \"{:s}\"".format(repo.directory))
            if url := repo.remote_url:
                print("    url: \"{:s}\"".format(url))
                # As with the UI the access-token is replaced by `*`,
                # this is done to show which repositories use an access token.
                print("    access_token: {:s}".format(
                    "\"{:s}\"".format("*" * len(repo.access_token)) if repo.access_token else
                    "None"
                ))
            else:
                print("    source: \"{:s}\"".format(repo.source))

        return True

    @staticmethod
    def add(
            *,
            name: str,
            repo_id: str,
            directory: str,
            url: str,
            access_token: str,
            source: str,
            cache: bool,
            clear_all: bool,
            no_prefs: bool,
    ) -> bool:
        from bpy import context

        # This could be allowed the Python API doesn't prevent it.
        # However this is not going to do what the user would expect so disallow it.
        if url:
            if source == 'SYSTEM':
                sys.stderr.write("Cannot use \"--url\" and \"--source=SYSTEM\" together.\n")
                return False
        else:
            if access_token:
                sys.stderr.write("Cannot use \"--access-token\" without a \"--url\".\n")
                return False

        extension_repos = context.preferences.extensions.repos
        if clear_all:
            while extension_repos:
                extension_repos.remove(extension_repos[0])

        repo = extension_repos.new(
            name=name,
            module=repo_id,
            custom_directory=directory,
            remote_url=url,
            source=source,
        )
        repo.use_cache = cache

        if access_token:
            repo.use_access_token = True
            repo.access_token = access_token

        if not no_prefs:
            blender_preferences_write()

        return True

    @staticmethod
    def remove(
            *,
            repo_id: str,
            no_prefs: bool,
    ) -> bool:
        from bpy import context
        extension_repos = context.preferences.extensions.repos
        extension_repos_module_map = {repo.module: repo for repo in extension_repos}
        repo = extension_repos_module_map.get(repo_id)
        if repo is None:
            sys.stderr.write("Repository: \"{:s}\" not found in [{:s}]\n".format(
                repo_id,
                ", ".join(["\"{:s}\"".format(x) for x in sorted(extension_repos_module_map.keys())])
            ))
            return False
        extension_repos.remove(repo)
        print("Removed repo \"{:s}\"".format(repo_id))

        if not no_prefs:
            blender_preferences_write()

        return True


# -----------------------------------------------------------------------------
# Command Line Argument Definitions

def arg_handle_int_as_bool(value: str) -> bool:
    result = int(value)
    if result not in {0, 1}:
        raise argparse.ArgumentTypeError("Expected a 0 or 1")
    return bool(result)


def generic_arg_sync(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "-s",
        "--sync",
        dest="sync",
        action="store_true",
        default=False,
        help=(
            "Sync the remote directory before performing the action."
        ),
    )


def generic_arg_enable_on_install(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "-e",
        "--enable",
        dest="enable",
        action="store_true",
        default=False,
        help=(
            "Enable the extension after installation."
        ),
    )


def generic_arg_no_prefs(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--no-prefs",
        dest="no_prefs",
        action="store_true",
        default=False,
        help=(
            "Treat the user-preferences as read-only,\n"
            "preventing updates for operations that would otherwise modify them.\n"
            "This means removing extensions or repositories for example, wont update the user-preferences."
        ),
    )


def generic_arg_package_list_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="packages",
        metavar="PACKAGES",
        type=str,
        help=(
            "The packages to operate on (separated by ``,`` without spaces)."
        ),
    )


def generic_arg_package_file_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="file",
        metavar="FILE",
        type=str,
        help=(
            "The packages file."
        ),
    )


def generic_arg_repo_id(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "-r",
        "--repo",
        dest="repo",
        type=str,
        help=(
            "The repository identifier."
        ),
        required=True,
    )


def generic_arg_package_repo_id_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="repo_id",
        metavar="ID",
        type=str,
        help=(
            "The repository identifier."
        ),
    )


# -----------------------------------------------------------------------------
# Blender Package Manipulation

def cli_extension_args_list(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "list".
    subparse = subparsers.add_parser(
        "list",
        help="List all packages.",
        description=(
            "List packages from all enabled repositories."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_sync(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_query.list(
            sync=args.sync,
        ),
    )


def cli_extension_args_sync(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "sync".
    subparse = subparsers.add_parser(
        "sync",
        help="Synchronize with remote repositories.",
        description=(
            "Download package information for remote repositories."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    subparse.set_defaults(
        func=lambda args: subcmd_utils.sync(show_done=False),
    )


def cli_extension_args_upgrade(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "update".
    subparse = subparsers.add_parser(
        "update",
        help="Upgrade any outdated packages.",
        description=(
            "Download and update any outdated packages."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_sync(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_pkg.update(sync=args.sync),
    )


def cli_extension_args_install(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "install".
    subparse = subparsers.add_parser(
        "install",
        help="Install packages.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_sync(subparse)
    generic_arg_package_list_positional(subparse)

    generic_arg_enable_on_install(subparse)
    generic_arg_no_prefs(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_pkg.install(
            sync=args.sync,
            packages=args.packages.split(","),
            enable_on_install=args.enable,
            no_prefs=args.no_prefs,
        ),
    )


def cli_extension_args_install_file(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "install-file".
    subparse = subparsers.add_parser(
        "install-file",
        help="Install package from file.",
        description=(
            "Install a package file into a user repository."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )

    generic_arg_package_file_positional(subparse)
    generic_arg_repo_id(subparse)

    generic_arg_enable_on_install(subparse)
    generic_arg_no_prefs(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_pkg.install_file(
            filepath=args.file,
            repo_id=args.repo,
            enable_on_install=args.enable,
            no_prefs=args.no_prefs,
        ),
    )


def cli_extension_args_remove(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "remove".
    subparse = subparsers.add_parser(
        "remove",
        help="Remove packages.",
        description=(
            "Disable & remove package(s)."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_list_positional(subparse)
    generic_arg_no_prefs(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_pkg.remove(
            packages=args.packages.split(","),
            no_prefs=args.no_prefs,
        ),
    )


# -----------------------------------------------------------------------------
# Blender Repository Manipulation

def cli_extension_args_repo_list(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "repo-list".
    subparse = subparsers.add_parser(
        "repo-list",
        help="List repositories.",
        description=(
            "List all repositories stored in Blender's preferences."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    subparse.set_defaults(
        func=lambda args: subcmd_repo.list(),
    )


def cli_extension_args_repo_add(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "repo-add".
    subparse = subparsers.add_parser(
        "repo-add",
        help="Add repository.",
        description=(
            "Add a new local or remote repository."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_repo_id_positional(subparse)

    # Optional.
    subparse.add_argument(
        "--name",
        dest="name",
        type=str,
        default="",
        metavar="NAME",
        help=(
            "The name to display in the interface (optional)."
        ),
    )

    subparse.add_argument(
        "--directory",
        dest="directory",
        type=str,
        default="",
        help=(
            "The directory where the repository stores local files (optional).\n"
            "When omitted a directory in the users directory is automatically selected."
        ),
    )
    subparse.add_argument(
        "--url",
        dest="url",
        type=str,
        default="",
        metavar="URL",
        help=(
            "The URL, for remote repositories (optional).\n"
            "When omitted the repository is considered \"local\"\n"
            "as it is not connected to an external repository,\n"
            "where packages may be installed by file or managed manually."
        ),
    )

    subparse.add_argument(
        "--access-token",
        dest="access_token",
        type=str,
        default="",
        metavar="ACCESS_TOKEN",
        help=(
            "The access token to use for remote repositories which require a token."
        ),
    )

    subparse.add_argument(
        "--source",
        dest="source",
        choices=('USER', 'SYSTEM'),
        default='USER',
        metavar="SOURCE",
        help=(
            "The type of source in ('USER', 'SYSTEM').\n"
            "System repositories are managed outside of Blender and are considered read-only."
        ),
    )

    subparse.add_argument(
        "--cache",
        dest="cache",
        metavar="BOOLEAN",
        type=arg_handle_int_as_bool,
        default=True,
        help=(
            "Use package cache (default=1)."
        ),
    )

    subparse.add_argument(
        "--clear-all",
        dest="clear_all",
        action="store_true",
        help=(
            "Clear all repositories before adding, simplifies test setup."
        ),
    )

    generic_arg_no_prefs(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_repo.add(
            repo_id=args.repo_id,
            name=args.name,
            directory=args.directory,
            url=args.url,
            access_token=args.access_token,
            source=args.source,
            cache=args.cache,
            clear_all=args.clear_all,
            no_prefs=args.no_prefs,
        ),
    )


def cli_extension_args_repo_remove(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Implement "repo-remove".
    subparse = subparsers.add_parser(
        "repo-remove",
        help="Remove repository.",
        description=(
            "Remove a repository."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_repo_id_positional(subparse)
    generic_arg_no_prefs(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_repo.remove(
            repo_id=args.repo_id,
            no_prefs=args.no_prefs,
        ),
    )


# -----------------------------------------------------------------------------
# Implement Additional Arguments

def cli_extension_args_extra(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    # Package commands.
    cli_extension_args_list(subparsers)
    cli_extension_args_sync(subparsers)
    cli_extension_args_upgrade(subparsers)
    cli_extension_args_install(subparsers)
    cli_extension_args_install_file(subparsers)
    cli_extension_args_remove(subparsers)

    # Preference commands.
    cli_extension_args_repo_list(subparsers)
    cli_extension_args_repo_add(subparsers)
    cli_extension_args_repo_remove(subparsers)


def cli_extension_handler(args: list[str]) -> int:
    from .cli import blender_ext

    # Override the default valid tags with a file which Blender includes.
    blender_ext.ARG_DEFAULTS_OVERRIDE.build_valid_tags = os.path.join(
        os.path.dirname(__file__), "..", "..", "modules", "_bpy_internal", "extensions", "tags.py",
    )

    result = blender_ext.main(
        args,
        args_internal=False,
        args_extra_subcommands_fn=cli_extension_args_extra,
        prog="blender --command extension",
    )
    # Needed as the import isn't followed by `mypy`.
    assert isinstance(result, int)
    return result
