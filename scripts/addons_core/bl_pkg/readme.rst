
##################
Blender Extensions
##################

Directory Layout
================

``./blender_addon/bl_pkg/cli/``
   The stand-alone command line utility to manage extensions.

``./blender_addon/bl_pkg/``
   The Blender add-on which wraps the command line utility
   (abstracts details of interacting with the package manager & repositories).

``./tests/``
   Automated tests.

   To run tests via the ``Makefile``.

   Test the command line application.

   .. code-block::

      make test PYTHON_BIN=/path/to/bin/python3.11

   If your system Python is v3.11 or newer you may omit ``PYTHON_BIN``.

   .. code-block::

      make test_blender BLENDER_BIN=/path/to/blender


GUI
===

This GUI is work-in-progress, currently it's been made to work with an un-modified Blender 4.1.

- Link ``blender_addon/bl_pkg`` into your add-ons directly.
- Enable the blender extensions add-on from Blender.
- Enable the blender extensions checkbox in the add-ons preference (this is a temporary location).
- Repositories can be added/removed from the "Files" section in the preferences.


Hacking
=======

Some useful hints.

When developing the command line interface, these tests can be setup to run on file-change, run:

.. code-block::

   make watch_test

To run Blender tests.

.. code-block::

   make watch_test_blender BLENDER_BIN=/path/to/blender

How to Setup a Test Environment
===============================

Most of the options here are using the command-line tools. For a comprehensive list of commands check the help:

.. code-block::

   ./blender_addon/bl_pkg/cli/blender_ext.py --help


Dummy server
------------

The simple way to get started is by creating a dummy asset library.

.. code-block::

   ./blender_addon/bl_pkg/cli/blender_ext.py dummy-repo \
      --repo-dir=/path/to/host/my/repo/files \
      --package-names="blue,red,green,purple,orange"

This will populate the directory specified as ``--repo-dir`` with dummy assets packages (``.zip``),
and an index (``bl_ext_repo.json``).


Setup an Extensions Repository
==============================

First you need to create individual packages for the individual extension:

- Go to the directory of the extension you want to package.
- Create a ``bl_ext_pkg.toml`` file with your configuration.
- Run the command ``blender_ext.py build``.

You can look at an example of a dummy extension in the ``example_extension`` directory.

.. code-block::

   cd ./example_extension
   ../blender_addon/bl_pkg/cli/blender_ext.py build

This will create a ``my_example_package.zip`` (as specified in the .toml file).

Now you can move all your ``*.zip`` packages to where they will be hosted in the server.
The final step is to create an index file to serve all your packages.

.. code-block::

   mkdir -p /path/to/host/my/repo/files
   cp ./example_extension/my_example_package.zip /path/to/host/my/repo/files
   ./blender_addon/bl_pkg/cli/blender_ext.py server-generate --repo-dir /path/to/host/my/repo/files

This will generate a new file ``bl_ext_repo.json`` in your repository directory.
This file is to be used the entry point to your remote server.

Alternatively, if you are doing tests locally,
you can point the directory containing this file as the ``Remote Path`` to your Extensions Repository.


.. This section could go elsewhere, for now there is only a single note.

Requirement: Add-Ons
====================

Add-ons packaged as extensions must use relative imports when importing its own sub-modules.
This is a requirement of Python module name-spacing.


Requirement: Blender 4.2
========================

This add-on requires an yet-to-be released version of Blender.

You can download a `daily build <https://builder.blender.org>`__ of Blender 4.2 for testing and development purposes.
