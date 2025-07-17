
##########
Extensions
##########

Extensions Source Code Overview
===============================

Add-on: Blender Modules
-----------------------

- ``__init__.py``
  Add-on containing, preferences, ``bpy.app.handlers`` that respond to adding/removing repositories.

- ``bl_extension_ui.py``
  Defines the extensions UI, select between add-ons, themes, tag-filtering.

- ``bl_extension_ops.py``
  Defines extension operators, this is the main entry point for extension logic (except notifications, see below).

  This module defines a mechanism for a modal operator to run commands
  defined in ``cli/blender_ext.py`` as sub-processes, monitoring their progress
  (via STDOUT, see :ref:`Inter process communication (IPC) <IPC>`),
  see the ``_ExtCmdMixIn`` class.
  Actions such a as downloading, installing, updating are supported by calling into lower level functions,
  ``cli/blender_ext.py`` does the actual work.

  There are also some operators for the UI (changing tags, allowing online access).

- ``bl_extension_notify.py``

  This module checks for updates and is intended to run in the background.

  Checking for updates uses ``bl_extension_utils.CommandBatch`` from a timer.
  Checking for updates from a modal-operator is avoided since Blender may cancel
  modal operators when loading a file for example.

  The status bar is refreshed if/when updates are found.

- ``bl_extension_cli.py``

  The command line interface to support: ``blender -c extension ...``

  Some commands operate on Blender's preferences (for adding/removing repositories),
  other commands such as building packages are forwarded to ``cli/blender_ext.py``.

Add-on: Other Scripts
---------------------

- ``bl_extension_utils.py``

  This module contains various utilities,

  Note that use of ``bpy`` is intentionally avoided here,
  state from preferences or operators is passed in.

  - Generic shared utility functions.

  - Command line sub-process supervisor (``CommandBatch``)
    used by Blender operators to call ``cli/blender_ext.py`` and its sub-commands (see doc-string for details).

  - A view on the repositories JSON/TOML data what abstracts the file IO (``RepoCacheStore``).

  - A locking context to prevent multiple Blender instances operating on the same repository at once.

- ``cli/blender_ext.py``
  This command is responsible for operations on the repository,
  primarily downloading & installing extensions.

  It also contains functions to build & validate packages & create a static repository.

  This script typically runs as an external process using the ``subprocess`` module
  called from ``bl_extension_utils.py``.
  In some cases ``bl_extension_cli.py`` forwards sub-commands directly to this script.

  :ref:`Inter process communication (IPC) <IPC>` via Python's ``subprocess`` module
  which runs this script using Blender's bundled Python executable.
  Signals are used to interrupt the process, pipes are used to read it's output.

  This is done so Blender's UI can show the status of each command.

- ``extensions_map_from_legacy_addons``

  This is more of a data file used so users with legacy add-ons can update them to extensions.

Other Modules
-------------

Some functionality that's relevant to the extensions system is implemented in other modules,
as it relates to how extensions are loaded by Blender.

*Paths are relative to Blender's source tree.*

``./scripts/modules/_bpy_internal/extensions/junction_module.py``
   This stand-alone module allows extensions to appear as if they are all loaded
   from a single *package* independent of their file-system location.

   This is done so extensions don't pollute the module name-space
   (avoiding naming collisions with packages downloaded from ``https://pypi.org``).

   So each extension's add-on ID follows this format: ``bl_ext.{repository_id}.{extension_id}``.

``./scripts/modules/_bpy_internal/extensions/wheel_manager.py``
   Extensions may include Python modules as wheels,
   these are extracted into an a site-packages directory that is specific to the extensions for this version of Blender.
   ``~/.config/blender/X.X/extensions/.local/lib/python3.XX/site-packages/``.

   Once extensions have been installed the list of wheels from each extensions ``blender_manifest.toml``
   is combined and passed in to the main "apply_action" function which will install/uninstall wheels as needed.

   Unfortunately there is no special handling for version conflicts.
   When different versions of the same wheel are found, the latest version is installed.
   This may break any extensions depending on the old version of a wheel.

``./scripts/modules/_bpy_internal/extensions/stale_file_manager.py``
   On MS-Windows it's common that files are locked and can't be deleted
   (any DLL's loaded into memory),
   although it can also happen if other processes are scanning the file system.

   In this case, the file is marked as stale and queued for removal when Blender next starts.

   Unfortunately **upgrading** extensions that use DLL's on MS-Windows isn't
   reliable because it is necessary to remove then re-create the extension.
   This is an area that could use further development it may be necessary to
   support installing on restart.

``./scripts/modules/_bpy_internal/extensions/{tags,permissions}.py``
   These are definition lists used when building packages,
   they are ``https://extensions.blender.org`` specific.


C++ Sources
-----------

``./source/blender/makesdna/DNA_userdef_types.h``
   The repository definition (``bUserExtensionRepo``).

``./source/blender/editors/space_userpref/userpref_ops.cc``
   Operators for adding/removing repositories as well as dropping URL's to initiate installation.

``./source/blender/python/intern/bpy_app_handlers.cc``
   Handlers for extensions ``bpy.app.handlers._extension_repos_*``,
   *note the leading underscore as they are not part of the public API.*

   Unfortunately these handlers were needed as a way for Python to hook into lower level code paths,
   so it's possible (for example) to refresh the extensions from an RNA update function
   (``rna_userdef.cc`` and the operators in some cases).

``wmWindowManager::extensions_updates`` & ``extensions_blocked``
   Status bar drawing uses these values set by ``bl_extension_notify.py``.


Functionality Described
=======================

This section describes how functionality has been implemented.


Extension Pre-Flight Compatibility Check
----------------------------------------

Since extensions may be from a shared system directory or imported from an older installation
it's necessary to ensure the extension is compatible with Blender on startup.

An extension may be incompatible for various reasons,

- Unsupported Blender version.
- Unsupported platform.
- Binary incompatibility (from it's wheels).
- The extension may also have been block-listed.

Since this runs on every startup, expensive checks are avoided if at all possible.

In ``./scripts/modules/addon_utils.py`` the private function ``_initialize_extensions_compat_ensure_up_to_date``
is responsible for ensuring extensions are compatible before loading.

- On startup run: ``_initialize_extensions_repos_once`` which sets up repositories and handlers.
- If the "compatibility cache" doesn't exist it is created (each extension's TOML file is inspected for compatibility).
  A dictionary of incompatible extensions is stored in the compatibility cache which is checked
  whenever ``addon_utils.enable(..)`` is used to enable an extension.
- If the "compatibility cache" exists it is validated by each extensions TOML modification-time & size,
  re-generating upon any changes.
- The compatibility data stores the reason the extension is disabled,
  this is reported if the user attempts to enable it.


The details of the compatibility cache are documented in ``addon_utils``,
it's a simple format that stores the Blender version & a magic number that can be bumped at any time,
changes to these files cause the cache to be re-generated.


Dragging & Dropping a URL
-------------------------

Extensions drag & drop is handled with Blender's drop-boxes.
This works in much the same way as dropping images in the 3D viewport or blend files.

There are two drop-boxes used, one for file-paths another for URL's.
Both check the path contains a ``.zip`` extension,
where the URL logic needs to strips the query string and the fragment from the URL.

The drop action runs the operator ``extensions.package_install`` (from ``bl_extension_ops.py``)
which checks if the ``url`` property has been set.
If so, the code-path for dropping a URL is activated.

Once drop is activated:

- A URL is scanned for blender version range & platform compatibility
  to prevent downloading & attempting to install extensions which aren't compatible.

  Note that the query parameters in the URL are optional and serve as a convenient way to fail-early,
  if they're not present the user may be prompted to add a new repository only to discover
  later that extension they dropped isn't compatible with their Blender version.

  Both ``extensions.blender.org`` and static sites
  generated by ``blender -c extension server-generate`` set these parameters.

- A file-path is considered "local" so its manifest is inspected to check it's compatible.

Other checks are performed to ensure the repository exists locally.
If the extension isn't found to be incompatible, the user may install it.

Dropping a URL may prompt the user for actions that need to be done before the extensions may be installed.

- Dropping a URL with "Online Access" disabled prompts the user to enable online-access.
- Dropping a URL from an unknown remote repository prompts the user to add the repository.
- Otherwise, dropping the URL of a compatible extension will prompt the user to install the extension.

Unfortunately chaining popups together (setup wizard) or merging popups together is not well supported in Blender.
Causing some fairly bad worst-case scenarios when dropping a URL which isn't part of a known repository.


Implementation Details
======================

Extension Format
----------------

Extensions are intended to be created with the ``blender -c extension build``
command which creates a ZIP file and performs some checks to catch errors early.

The ZIP file must contain a ``blender_manifest.toml`` (which may be in a directory),
as well as files for a Python package for add-ons or an XML for themes.


Repositories
------------

Information about repositories is stored in user preferences.
The main values are a unique name, module path & optionally a remote URL.

There are 2 kinds of repositories:

- **Remote** which can be synchronized for updates.
- **Local** where the repository is a file-system location.

  Local repositories may define a source:

  - **User** the user may add/remove extensions to this location.
  - **System** this treated as read-only and may be used when extensions
    are shared on a network file-system for example.

Synchronizing a **Remote** repository simply downloads the JSON listing from the remote URL.

Once extensions have been installed their TOML files are compared with the repository to check for updates.

.. _IPC:

Inter Process Communication (IPC)
---------------------------------

- Commands that manipulate extensions (such as updating/installing/removing)
  are performed by the stand-alone script ``cli/blender_ext.py``.
- Using IPC means these commands can run in the background without blocking Blender's GUI.
- The state of the extensions repository (repository location, blender-version, API tokens etc)
  are passed in via command line arguments.
- This can be configured to only output JSON messages to the STDOUT which Blender parses and uses
  to send feedback to the user.
- Progress (such as percentage of a file downloaded) is sent to the STDOUT
  so the GUI and command-line interface can show progress.
- Input is limited to the request to cancel
  (if the user cancels the operator or presses Control-C on the command line).
- Internally functions are responsible for checking if the user has requested to exit.
  This is especially important before IO or anything that could cause the process to wait
  so as to avoid "hanging" once the user has requested to exit.

All IPC is handled by ``bl_extension_utils.CommandBatch`` which can run multiple commands,
a common case is running multiple updates at once.

The caller can use methods on the CommandBatch to access the status and report any problems.


Tooling
=======

The tests are not yet integrated into CTest
because some tests depend on the ``wheel`` module (not distributed with Blender's Python).

Tests can be run via the ``Makefile`` using the local Python::

   make -C scripts/addons_core/bl_pkg test

Run the ``help`` target for a list of convenience targets to run checkers & tests.
