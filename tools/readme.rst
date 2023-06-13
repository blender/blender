
Blender Dev Tools
#################

This repository is intended for miscellaneous tools, utilities, configurations and
anything that helps with Blender development, but aren't directly related to building Blender.

Some of the tools included may be used stand-alone, others expect Blenders source code to be available.


Usage
=====

While this is a stand-alone repository,
some of the scripts which access Blenders source code assume this repository will be located at
``tools`` within Blenders source code repository. At some point this may be included as a submodule.

Some tools also rely on the ``blender`` binary, this is assumed to be located at: ``../../blender.bin``.
*The root directory of Blender's git repository*


Categories
==========

Check Source
------------

Any tools for scanning source files to report issues with code, style, conventions, deprecated features etc.


Config
------

Configuration for 3rd party applications (IDE's, code-analysis, debugging tools... etc).


Git
---

Scripts and utilities for working with git.


Utils
-----

Programs (scripts) to help with development
(currently for converting formats, creating mouse cursor, updating themes).
