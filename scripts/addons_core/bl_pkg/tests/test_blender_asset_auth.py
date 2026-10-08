# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Test authentication for asset libraries provided by extensions.

This file contains a single test, see the note in ``TestAssetAuth.test_asset_auth``.

Command to run this test directly:
   env BLENDER_BIN=$PWD/blender.bin python ./scripts/addons_core/bl_pkg/tests/test_blender_asset_auth.py
"""

import contextlib
import inspect
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

from typing import (
    NamedTuple,
)
from collections.abc import (
    Callable,
    Iterator,
    Sequence,
)


BASE_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(BASE_DIR, "modules"))
from http_server_context import HTTPServerContext  # noqa: E402


# For more useful output that isn't clipped.
# pylint: disable-next=protected-access
unittest.util._MAX_LENGTH = 10_000  # type: ignore


VERBOSE_CMD = False

# Keep the temporary directories & the server running when the test finishes,
# so the state it created can be inspected.
USE_PAUSE_BEFORE_EXIT = False

# Time to wait for a download, so a stalled request fails instead of hanging the test.
TIMEOUT_SEC = 10.0


def blender_bin_from_env() -> str:
    if (blender_bin := os.environ.get("BLENDER_BIN")) is None:
        raise Exception("BLENDER_BIN: environment variable not defined")
    return blender_bin


BLENDER_BIN: str = blender_bin_from_env()

# Arguments to ensure extensions are enabled (currently it's an experimental feature).
BLENDER_ENABLE_EXTENSION_ARGS = [
    "--online-mode",
    "--python-exit-code", "1",
]

PKG_EXT = ".zip"
# NOTE: `extern_fn_assets_create` writes this extension too, it can't use this constant.
BLEND_EXT = ".blend"
PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"

# The repository added to Blender's preferences, serving the extension.
REPO_ID = "my_test_repo"
REPO_NAME = "My Test Repo"

# The manifest for the asset-library extension.
PKG_MANIFEST_TEMPLATE_TOML = '''\
schema_version = "1.0.0"
id = "{pkg_idname:s}"
name = "{pkg_name:s}"
type = "asset-library"
maintainer = "Maintainer Name <username@addr.com>"
license = ["SPDX:GPL-2.0-or-later"]
version = "1.0.0"
tagline = "{pkg_tagline:s}"
blender_version_min = "0.0.0"

[asset_library]
remote_url = "{remote_url:s}"
'''

PKG_MANIFEST_TEMPLATE_AUTH_TOML = '''
[asset_library.auth]
required = true
auth_method = "TOKEN"
'''

# The asset library & the extensions repository are served over HTTP.
# NOTE: other tests in this directory use 8001 & 8002.
HTTP_PORT = 8003
URL_BASE = "http://localhost:{:d}".format(HTTP_PORT)

# Directories under the "remote" directory, each is also the leading component of its URL
# & the name the server looks up its token by, so all three have to agree.
REMOTE_DIRNAME_ASSET_LIBRARY_A = "asset_library_a"
REMOTE_DIRNAME_ASSET_LIBRARY_B = "asset_library_b"
REMOTE_DIRNAME_REPO = "repo"


def url_from_remote_dirname(dirname: str) -> str:
    return "{:s}/{:s}/".format(URL_BASE, dirname)


# The URL the extensions repository is served from.
REPO_URL = url_from_remote_dirname(REMOTE_DIRNAME_REPO)


class PkgInfo(NamedTuple):
    """
    An "asset-library" extension & the library it installs.
    """
    idname: str
    name: str
    tagline: str
    asset_library_url: str
    # Whether the manifest declares "[asset_library.auth]".
    # Without it no token is used for the library, even from its repository.
    use_auth: bool

    def manifest_toml(self) -> str:
        return PKG_MANIFEST_TEMPLATE_TOML.format(
            pkg_idname=self.idname,
            pkg_name=self.name,
            pkg_tagline=self.tagline,
            remote_url=self.asset_library_url,
        ) + (PKG_MANIFEST_TEMPLATE_AUTH_TOML if self.use_auth else "")


# Two extensions, so the token has to be looked up by the libraries URL.
# Only one of them declares authentication, which is the difference under test.
PKG_INFO = (
    PkgInfo(
        idname="my_test_asset_library_a",
        name="My Test Asset Library A",
        tagline="An asset library served for testing",
        asset_library_url=url_from_remote_dirname(REMOTE_DIRNAME_ASSET_LIBRARY_A),
        use_auth=True,
    ),
    PkgInfo(
        idname="my_test_asset_library_b",
        name="My Test Asset Library B",
        tagline="An asset library needing no token",
        # Only used to check the token is looked up by URL, this library is never served.
        asset_library_url=url_from_remote_dirname(REMOTE_DIRNAME_ASSET_LIBRARY_B),
        use_auth=False,
    ),
)

PKG_A, PKG_B = PKG_INFO

# The asset files are served from their own address, only the listing is behind the token.
HTTP_PORT_ASSET_FILES = 8004
ASSET_FILES_URL = "http://localhost:{:d}/".format(HTTP_PORT_ASSET_FILES)

# The server rejects requests without these tokens,
# so accessing them can only succeed when the token is used.
REPO_TOKEN = "SECRET_REPO_TOKEN"
ASSET_LIBRARY_TOKEN = "SECRET_ASSET_LIBRARY_TOKEN"

# One asset per blend file, each a different ID type, the file is named after the asset.
ASSET_TYPE_AND_NAME = (
    ("MESH", "my_test_asset_mesh"),
    ("BRUSH", "my_test_asset_brush"),
    ("IMAGE", "my_test_asset_image"),
)

user_dirs: tuple[str, ...] = (
    "config",
    "datafiles",
    "extensions",
    "scripts",
)


class TempDirs(NamedTuple):
    """
    The directories used by this test, all located under a single temporary directory.
    """
    # Everything below is served over HTTP, this systems Blender only reaches it by URL.
    remote: str
    # The blend files making up the asset library.
    remote_asset_library: str
    # The extensions repository, holding the extension which installs the asset library.
    remote_repo: str
    # The asset files, served separately from the listing which references them.
    remote_asset_files: str

    # Everything below is accessed as local files by this systems Blender.
    local: str
    # The local copy of the asset library, downloaded from the server.
    local_asset_library: str
    # Blender's user resources (`BLENDER_USER_RESOURCES`).
    local_blender_user: str
    # Stands in for the users home directory.
    local_home: str
    # The cache directory within the home directory (`XDG_CACHE_HOME`).
    local_home_cache: str
    # Don't leave temporary files in TMP: `/tmp` (since it's only cleared on restart).
    local_tmpdir: str

    @staticmethod
    def from_root(root: str) -> "TempDirs":
        remote = os.path.join(root, "remote")
        local = os.path.join(root, "local")
        local_home = os.path.join(local, "home")
        return TempDirs(
            remote=remote,
            remote_asset_library=os.path.join(remote, REMOTE_DIRNAME_ASSET_LIBRARY_A),
            remote_repo=os.path.join(remote, REMOTE_DIRNAME_REPO),
            remote_asset_files=os.path.join(remote, "asset_files"),

            local=local,
            local_asset_library=os.path.join(local, "asset_library"),
            local_blender_user=os.path.join(local, "blender_user"),
            local_home=local_home,
            local_home_cache=os.path.join(local_home, ".cache"),
            local_tmpdir=os.path.join(local, "tmp"),
        )


# Initialized from `main()`.
TEMP_DIRS: TempDirs


# -----------------------------------------------------------------------------
# External Functions
#
# These functions exist to be passed into Blender, they are not run from this process.

def extern_fn_assets_create() -> None:
    # Create a blend file for each "{TYPE}:{NAME}" argument,
    # each holding a single data-block marked as an asset.
    # pylint: disable=reimported,redefined-outer-name
    import bpy  # type: ignore[import-not-found]
    import os
    import sys
    directory, *args = sys.argv[sys.argv.index("--") + 1:]
    for arg in args:
        asset_type, _, name = arg.partition(":")
        bpy.ops.wm.read_homefile(use_empty=True)
        if asset_type == "MESH":
            id_data = bpy.data.meshes.new(name)
            id_data.from_pydata(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)), (), ((0, 1, 2, 3),))
        elif asset_type == "BRUSH":
            id_data = bpy.data.brushes.new(name)
        elif asset_type == "IMAGE":
            id_data = bpy.data.images.new(name, 4, 4)
        else:
            raise Exception("Unknown asset type {:s}".format(asset_type))
        id_data.asset_mark()
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(directory, name + ".blend"))


def extern_fn_asset_library_registered() -> None:
    # Fail unless each asset library URL is registered or not, as the "1" or "0" after it says.
    # pylint: disable=reimported,redefined-outer-name
    # NOTE: MYPY reports the missing module once, the first import suppresses it.
    import bpy
    import sys
    args = sys.argv[sys.argv.index("--") + 1:]
    remote_urls = [
        asset_lib.remote_url
        for asset_lib in bpy.context.preferences.filepaths.asset_libraries
        if asset_lib.use_remote_url
    ]
    for remote_url, expect in zip(args[0::2], args[1::2], strict=True):
        if (remote_url in remote_urls) != (expect == "1"):
            raise Exception("Remote asset library {!r} registered={!r}, found {!r}".format(
                remote_url, expect == "1", remote_urls,
            ))


def extern_fn_asset_library_enabled_toggle() -> None:
    # Disable then enable an extensions asset library, checking the entry is kept either way.
    # pylint: disable=reimported,redefined-outer-name
    import bpy
    import sys
    repo_id, pkg_idname, remote_url = sys.argv[sys.argv.index("--") + 1:]

    repo_index = next(
        index for index, repo in enumerate(bpy.context.preferences.extensions.repos)
        if repo.module == repo_id
    )

    def asset_library_enabled() -> bool:
        for asset_lib in bpy.context.preferences.filepaths.asset_libraries:
            if asset_lib.use_remote_url and asset_lib.remote_url == remote_url:
                return bool(asset_lib.enabled)
        raise Exception("Remote asset library {!r} no longer registered".format(remote_url))

    for op, enabled_expect in (
            (bpy.ops.extensions.package_asset_library_disable, False),
            (bpy.ops.extensions.package_asset_library_enable, True),
    ):
        op(pkg_id=pkg_idname, repo_index=repo_index)
        if (enabled := asset_library_enabled()) != enabled_expect:
            raise Exception("Expected asset library enabled={!r}, found {!r}".format(enabled_expect, enabled))


def extern_fn_asset_auth_token_matches() -> None:
    # Fail unless each asset library URL resolves the token which follows it.
    # pylint: disable=reimported,redefined-outer-name
    import sys
    from bl_pkg import asset_auth_token_from_url  # type: ignore

    args = sys.argv[sys.argv.index("--") + 1:]
    for remote_url, token_expect in zip(args[0::2], args[1::2], strict=True):
        token = asset_auth_token_from_url(remote_url) or ""
        if token != token_expect:
            raise Exception("Expected {:s} token {!r}, found {!r}".format(remote_url, token_expect, token))


def extern_fn_repo_use_access_token_for_asset_libraries_matches() -> None:
    # Fail unless the repository flag mirrored from the listing matches the expected value.
    # Sync in-process as the CLI doesn't write preferences after syncing.
    # pylint: disable=reimported,redefined-outer-name
    import sys
    import bpy

    repo_name, expect_str = sys.argv[sys.argv.index("--") + 1:]
    bpy.ops.extensions.repo_sync_all()
    repo = bpy.context.preferences.extensions.repos[repo_name]
    if repo.use_access_token_for_asset_libraries != (expect_str == "1"):
        raise Exception("Expected use_access_token_for_asset_libraries={:s}".format(expect_str))


def extern_fn_asset_listing_download() -> None:
    # Download the asset library listing, raising on any resulting error.
    # pylint: disable=reimported,redefined-outer-name
    import sys
    import time
    from pathlib import Path
    from _bpy_internal.assets.remote_library import listing_downloader  # type: ignore[import-not-found]

    remote_url, auth_token, local_path = sys.argv[sys.argv.index("--") + 1:]

    is_done = False

    def on_done(_downloader: object) -> None:
        nonlocal is_done
        is_done = True

    downloader = listing_downloader.RemoteAssetListingDownloader(
        remote_url,
        auth_token,
        Path(local_path),
        lambda *args: None,
        on_done,
    )
    downloader.download_and_process()
    time_end = time.time() + TIMEOUT_SEC
    while not is_done:
        if time.time() > time_end:
            raise Exception("Timeout downloading the listing")
        # Blender's timer system would do this, it doesn't run in background mode.
        downloader.on_timer_event()
        time.sleep(0.01)

    if downloader.status != listing_downloader.DownloadStatus.FINISHED_SUCCESSFULLY:
        raise Exception("Downloading the listing {:s}: {:s}".format(
            downloader.status.value,
            downloader.error_message,
        ))


def extern_fn_asset_file_download() -> None:
    # Download a single asset file, raising on any resulting error.
    # pylint: disable=reimported,redefined-outer-name,protected-access
    import sys
    import time
    from pathlib import Path
    import _bpy_internal.assets.remote_library.asset_downloader as asset_dl  # type: ignore

    library_url, auth_token, local_path, asset_url, asset_hash, save_to = sys.argv[sys.argv.index("--") + 1:]

    asset_dl.download_asset_file(
        library_url,
        auth_token,
        Path(local_path),
        asset_url,
        asset_hash,
        Path(save_to),
    )

    downloader = asset_dl._asset_downloaders[library_url]
    time_end = time.time() + TIMEOUT_SEC
    while downloader.status == asset_dl.DownloadStatus.DOWNLOADING:
        if time.time() > time_end:
            raise Exception("Timeout downloading the asset")
        # Blender's timer system would do this, it doesn't run in background mode.
        downloader.on_timer_event()
        time.sleep(0.01)

    if downloader.status != asset_dl.DownloadStatus.FINISHED:
        raise Exception("Downloading the asset {:s}: {:s}".format(
            downloader.status.value,
            downloader.error_message,
        ))


# -----------------------------------------------------------------------------
# Utility Functions

def run_blender(args: Sequence[str]) -> tuple[int, str, str]:
    cmd: tuple[str, ...] = (
        BLENDER_BIN,
        # Needed while extensions is experimental.
        *BLENDER_ENABLE_EXTENSION_ARGS,
        *args,
    )

    if VERBOSE_CMD:
        print(shlex.join(cmd))

    env_overlay = {
        "TMPDIR": TEMP_DIRS.local_tmpdir,
        "BLENDER_USER_RESOURCES": TEMP_DIRS.local_blender_user,
        # Keep this test out of the users home directory, remote asset libraries
        # cache their contents there (under "remote-assets").
        # NOTE: WIN32 & macOS look the cache directory up through OS API's,
        # which these don't redirect.
        "HOME": TEMP_DIRS.local_home,
        "XDG_CACHE_HOME": TEMP_DIRS.local_home_cache,
        # Needed for ASAN builds.
        # NOTE: leaks are not checked, the result is ignored anyway (see "exitcode=0")
        # and the check adds over a second to every command.
        "ASAN_OPTIONS": "log_path={:s}:exitcode=0:leak_check_at_exit=0:{:s}".format(
            # Needed so the `stdout` & `stderr` aren't mixed in with ASAN messages.
            os.path.join(TEMP_DIRS.local_tmpdir, "blender_asan.txt"),
            # Support using existing configuration (if set).
            os.environ.get("ASAN_OPTIONS", ""),
        ),
    }

    output = subprocess.run(
        cmd,
        cwd=TEMP_DIRS.local,
        env={
            **os.environ,
            **env_overlay,
        },
        stderr=subprocess.PIPE,
        stdout=subprocess.PIPE,
        # Allow the caller to read a non-zero return-code.
        check=False,
    )
    stdout = output.stdout.decode("utf-8")
    stderr = output.stderr.decode("utf-8")

    if VERBOSE_CMD:
        print(stdout)
        print(stderr)

    return (
        output.returncode,
        stdout,
        stderr,
    )


def run_blender_no_errors(args: Sequence[str]) -> str:
    returncode, stdout, stderr = run_blender(args)
    if returncode != 0:
        if stdout:
            sys.stdout.write("STDOUT:\n")
            sys.stdout.write(stdout + "\n")
        if stderr:
            sys.stdout.write("STDERR:\n")
            sys.stdout.write(stderr + "\n")
        raise Exception("Expected zero returncode, got {:d}".format(returncode))
    if stderr:
        raise Exception("Expected empty stderr, got {:s}".format(stderr))
    return stdout


def run_blender_extensions(args: Sequence[str]) -> tuple[int, str, str]:
    return run_blender(("--command", "extension", *args))


def run_blender_extensions_no_errors(args: Sequence[str]) -> str:
    return run_blender_no_errors(("--command", "extension", *args))


def pause_until_keyboard_interrupt() -> None:
    print("Waiting for keyboard interrupt...")
    try:
        time.sleep(100_000)
    except KeyboardInterrupt:
        pass
    print("Exiting!")


def python_expr_from_fn(
        fn: Callable[[], None],
        namespace: dict[str, object] | None = None,
) -> str:
    """
    Return ``fn`` as text which declares then calls it, to be run by ``--python-expr``.

    Writing these scripts as functions keeps them syntax highlighted & checked,
    instead of being hidden away in string literals.

    ``namespace`` values are written using ``repr`` & declared before ``fn``,
    nothing else from this module is available to it.
    """
    return "{:s}{:s}\n{:s}()\n".format(
        "".join("{:s} = {!r}\n".format(key, value) for key, value in (namespace or {}).items()),
        inspect.getsource(fn),
        fn.__name__,
    )


def run_blender_python_fn(
        fn: Callable[[], None],
        args: Sequence[str] = (),
        *,
        # Disable to run with the preferences this test has set up.
        factory_startup: bool = True,
) -> str:
    """
    Run ``fn`` inside Blender, ``args`` are passed to it after ``--``.
    """
    return run_blender_no_errors((
        "--background",
        *(("--factory-startup",) if factory_startup else ()),
        "--python-expr", python_expr_from_fn(fn),
        "--",
        *args,
    ))


def run_blender_python_fn_result(
        fn: Callable[[], None],
        args: Sequence[str] = (),
        *,
        namespace: dict[str, object] | None = None,
) -> tuple[int, str]:
    """
    Run ``fn`` inside Blender, returning the exit-code & ``stderr``.

    Unlike ``run_blender_python_fn`` output on ``stderr`` isn't an error, the downloaders
    log there. The preferences this test set up are always used.
    """
    returncode, _stdout, stderr = run_blender((
        "--background",
        "--python-expr", python_expr_from_fn(fn, namespace),
        "--",
        *args,
    ))
    return returncode, stderr


class TestAssetAuth(unittest.TestCase):
    # Don't clip the difference when comparing command output.
    maxDiff = None

    def _assert_extension_listed(self, status_info: str) -> None:
        """
        Check the extension is listed under its repository,
        ``status_info`` follows the ID once it has been installed.
        """
        stdout = run_blender_extensions_no_errors(("list",))
        self.assertEqual(
            stdout,
            "".join(line + "\n" for line in (
                '''Repository: "{:s}" (id={:s})'''.format(REPO_NAME, REPO_ID),
                *[
                    '''  {:s}{:s}: "{:s}", {:s}'''.format(pkg.idname, status_info, pkg.name, pkg.tagline)
                    for pkg in PKG_INFO
                ],
            )),
        )

    def _step_assets_create(self) -> None:
        """
        Create the blend files which make up the asset library.
        """
        run_blender_python_fn(extern_fn_assets_create, (
            TEMP_DIRS.remote_asset_library,
            *["{:s}:{:s}".format(asset_type, name) for asset_type, name in ASSET_TYPE_AND_NAME],
        ))

    def _step_asset_listing_generate(self) -> None:
        """
        Generate the listing which describes the library, this is what a client downloads.
        """
        # NOTE: the generator logs to `stderr`, so only the exit-code can be checked.
        returncode, _stdout, stderr = run_blender((
            "--factory-startup",
            "--command", "asset_listing", "generate",
            TEMP_DIRS.remote_asset_library,
        ))
        self.assertEqual(returncode, 0, stderr)

    def _step_asset_files_relocate(self) -> str:
        """
        Move the blend files onto their own server, returning the name of one.

        The listing isn't updated to point at them, it takes no part in downloading a
        file: Blender reads the URL from the asset & hands it to the downloader, which
        is what `_step_asset_file_download` does. Moving rather than copying means the
        file can only be served by its own address.
        """
        for _asset_type, name in ASSET_TYPE_AND_NAME:
            filename = name + BLEND_EXT
            shutil.move(
                os.path.join(TEMP_DIRS.remote_asset_library, filename),
                os.path.join(TEMP_DIRS.remote_asset_files, filename),
            )

        return ASSET_TYPE_AND_NAME[0][1] + BLEND_EXT

    def _step_extension_build(self) -> None:
        """
        Build an "asset-library" extension which installs the library.
        """
        for pkg in PKG_INFO:
            output_filepath = os.path.join(TEMP_DIRS.remote_repo, pkg.idname + PKG_EXT)
            with tempfile.TemporaryDirectory(dir=TEMP_DIRS.local) as pkg_src_dir:
                with open(os.path.join(pkg_src_dir, PKG_MANIFEST_FILENAME_TOML), "w", encoding="utf-8") as fh:
                    fh.write(pkg.manifest_toml())
                run_blender_extensions_no_errors((
                    "build",
                    "--source-dir", pkg_src_dir,
                    "--output-filepath", output_filepath,
                ))

    def _step_repo_generate(self, assetlib_auth_method: str | None = None) -> None:
        """
        Generate the repository listing, so the extension can be installed from it.
        """
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIRS.remote_repo,
            *(("--assetlib-auth-method", assetlib_auth_method) if assetlib_auth_method is not None else ()),
        ))
        self.assertEqual(stdout, "found {:d} packages.\n".format(len(PKG_INFO)))

    def _step_repo_add(self, access_token: str | None = None) -> None:
        """
        Add the repository to Blender's preferences, replacing it when already added.
        """
        stdout = run_blender_extensions_no_errors((
            "repo-add",
            "--name", REPO_NAME,
            "--url", REPO_URL + "index.json",
            *(("--access-token", access_token) if access_token is not None else ()),
            # Remove the repositories which ship with Blender,
            # otherwise "sync" accesses the real extensions server.
            "--clear-all",
            REPO_ID,
        ))
        self.assertEqual(stdout, "")

    def _step_repo_sync_expect_failure(self) -> None:
        """
        The repository must not be readable without its access token.
        """
        # NOTE: the command exits successfully even when the sync fails, so check the output.
        _returncode, stdout, _stderr = run_blender_extensions(("sync",))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("FATAL_ERROR")],
            [
                "FATAL_ERROR sync: HTTP error (HTTP Error 401: Unauthorized) "
                "reading \'{:s}index.json\'!".format(REPO_URL),
            ],
        )

    def _step_repo_sync(self, *, is_installed: bool) -> None:
        """
        Download the repository listing, so its packages can be installed.
        """
        stdout = run_blender_extensions_no_errors(("sync",))
        self.assertEqual(
            stdout.rstrip("\n").split("\n")[-1],
            "STATUS Extensions list for \"{:s}\" updated".format(REPO_NAME),
        )

        # The extension must now be known, listed under the repository which serves it.
        self._assert_extension_listed(" [installed]" if is_installed else "")

    def _step_extension_install(self) -> None:
        """
        Install & enable the extension, registering the asset library it provides.
        """
        stdout = run_blender_extensions_no_errors((
            "install", ",".join(pkg.idname for pkg in PKG_INFO), "--enable",
        ))
        self.assertEqual(
            sorted(line for line in stdout.split("\n") if line.startswith("STATUS Installed")),
            sorted("STATUS Installed \"{:s}\"".format(pkg.idname) for pkg in PKG_INFO),
        )

        self._assert_extension_listed(" [installed]")

        # Installing registers the asset library each extension provides.
        self._step_asset_libraries_registered(True)

    def _step_asset_libraries_registered(self, registered: bool) -> None:
        """
        Check every extensions asset library is registered in the preferences, or that none are.
        """
        run_blender_python_fn(
            extern_fn_asset_library_registered,
            [
                value
                for pkg in PKG_INFO
                for value in (pkg.asset_library_url, "1" if registered else "0")
            ],
            factory_startup=False,
        )

    def _step_asset_library_enabled_toggle(self) -> None:
        """
        Disabling an extensions asset library keeps its entry, so it can be enabled again.
        """
        run_blender_python_fn(
            extern_fn_asset_library_enabled_toggle,
            (REPO_ID, PKG_A.idname, PKG_A.asset_library_url),
            factory_startup=False,
        )

    def _step_extension_remove(self) -> None:
        """
        Remove the extensions, which removes the asset libraries they registered.

        NOTE: only the preferences entry is removed, the libraries cache directory under
        "remote-assets" is left behind, so anything downloaded for it stays on disk.
        Removing an extension doesn't remove the assets it pulled in.
        """
        stdout = run_blender_extensions_no_errors((
            "remove", ",".join(pkg.idname for pkg in PKG_INFO),
        ))
        self.assertEqual(
            sorted(line for line in stdout.split("\n") if line.startswith("STATUS Removed")),
            sorted("STATUS Removed \"{:s}\"".format(pkg.idname) for pkg in PKG_INFO),
        )

        self._step_asset_libraries_registered(False)

        # The extensions remain available from the repository, they're no longer installed.
        self._assert_extension_listed("")

    def _step_repo_use_access_token_for_asset_libraries(self, expect: bool) -> None:
        run_blender_python_fn(
            extern_fn_repo_use_access_token_for_asset_libraries_matches,
            (REPO_NAME, "1" if expect else "0"),
            factory_startup=False,
        )

    def _step_asset_auth_tokens(self, *url_token_pairs: tuple[str, str]) -> None:
        """
        Check the token Blender resolves for each asset library, empty for no token.
        """
        run_blender_python_fn(
            extern_fn_asset_auth_token_matches,
            [value for pair in url_token_pairs for value in pair],
            factory_startup=False,
        )

    def _asset_listing_download(self, auth_token: str) -> tuple[int, str]:
        return run_blender_python_fn_result(extern_fn_asset_listing_download, (
            PKG_A.asset_library_url,
            auth_token,
            TEMP_DIRS.local_asset_library,
        ), namespace={"TIMEOUT_SEC": TIMEOUT_SEC})

    def _step_asset_listing_download_expect_failure(self) -> None:
        """
        The asset library must not be readable without its token.
        """
        returncode, stderr = self._asset_listing_download("")
        self.assertNotEqual(returncode, 0)
        self.assertIn("401 Client Error: Unauthorized", stderr)

    def _step_asset_listing_download(self) -> None:
        """
        The asset library must be readable with its token.
        """
        returncode, stderr = self._asset_listing_download(ASSET_LIBRARY_TOKEN)
        self.assertEqual(returncode, 0, stderr)

    def _step_asset_file_download(self, filename: str) -> None:
        """
        The asset file must download from its own server, which needs no token.
        """
        returncode, stderr = run_blender_python_fn_result(extern_fn_asset_file_download, (
            PKG_A.asset_library_url,
            ASSET_LIBRARY_TOKEN,
            TEMP_DIRS.local_asset_library,
            ASSET_FILES_URL + filename,
            # No hash, it's only used as a query string.
            "",
            filename,
        ), namespace={"TIMEOUT_SEC": TIMEOUT_SEC})
        self.assertEqual(returncode, 0, stderr)
        self.assertTrue(os.path.exists(os.path.join(TEMP_DIRS.local_asset_library, filename)))

    @contextlib.contextmanager
    def _servers_running(self) -> Iterator[None]:
        """
        Serve the "remote" directory & the asset files, from here on this systems
        Blender reaches them over HTTP. Only the "remote" directory needs tokens,
        the asset files are served from their own address without one.
        """
        def server(directory: str, port: int, auth_tokens: dict[str, str] | None) -> HTTPServerContext:
            return HTTPServerContext(
                directory=directory,
                port=port,
                verbose=VERBOSE_CMD,
                auth_tokens=auth_tokens,
                # Avoid an error when running tests quickly,
                # sometimes the port isn't available yet.
                wait_tries=10,
                wait_delay=0.05,
            )

        with (
                server(TEMP_DIRS.remote, HTTP_PORT, {
                    REMOTE_DIRNAME_REPO: REPO_TOKEN,
                    REMOTE_DIRNAME_ASSET_LIBRARY_A: ASSET_LIBRARY_TOKEN,
                }),
                server(TEMP_DIRS.remote_asset_files, HTTP_PORT_ASSET_FILES, None),
        ):
            yield

    def test_asset_auth(self) -> None:
        # NOTE: this reads more like a shell script than a unit-test, it's a single sequence
        # of commands where each step depends on the state the one before it left behind.
        # Splitting it up would mean repeating the whole sequence for each part,
        # and every step runs Blender so that gets slow quickly. Keep it as one test.

        self._step_assets_create()
        self._step_asset_listing_generate()
        asset_filename = self._step_asset_files_relocate()
        self._step_extension_build()

        with self._servers_running():
            self._step_repo_generate()
            # Without the access token the repository can't be read.
            self._step_repo_add()
            self._step_repo_sync_expect_failure()

            # With the access token it can.
            self._step_repo_add(REPO_TOKEN)
            self._step_repo_sync(is_installed=False)
            self._step_extension_install()

            # The repository doesn't share its token, so neither library resolves one.
            self._step_repo_use_access_token_for_asset_libraries(False)
            self._step_asset_auth_tokens(
                (PKG_A.asset_library_url, ""),
                (PKG_B.asset_library_url, ""),
            )

            # Without the token the asset library can't be read.
            self._step_asset_listing_download_expect_failure()

            # With the token it can.
            self._step_asset_listing_download()

            # The files the listing references are on the other server, without a token.
            self._step_asset_file_download(asset_filename)

            # The repository now declares itself the origin of the token for the asset
            # libraries its extensions provide.
            self._step_repo_generate("FROM_REPOSITORY")
            self._step_repo_sync(is_installed=True)

            # Only the library whose extension declares authentication uses the token,
            # which is looked up from the URL, not from the repository alone.
            self._step_repo_use_access_token_for_asset_libraries(True)
            self._step_asset_auth_tokens(
                (PKG_A.asset_library_url, REPO_TOKEN),
                (PKG_B.asset_library_url, ""),
            )

            # Disabling a library keeps its entry so it can be enabled again.
            self._step_asset_library_enabled_toggle()

            # Removing the extensions removes the libraries they registered.
            self._step_extension_remove()

            if USE_PAUSE_BEFORE_EXIT:
                print(TEMP_DIRS)
                pause_until_keyboard_interrupt()


def main() -> None:
    # pylint: disable-next=global-statement
    global TEMP_DIRS

    with tempfile.TemporaryDirectory() as temp_dir:
        TEMP_DIRS = TempDirs.from_root(temp_dir)

        for directory in TEMP_DIRS:
            os.makedirs(directory, exist_ok=True)

        for dirname in user_dirs:
            os.makedirs(os.path.join(TEMP_DIRS.local_blender_user, dirname), exist_ok=True)

        unittest.main()


if __name__ == "__main__":
    main()
