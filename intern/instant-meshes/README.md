# Instant Meshes
[![Build Status](https://travis-ci.org/wjakob/instant-meshes.svg?branch=master)](https://travis-ci.org/wjakob/instant-meshes)
[![Build status](https://ci.appveyor.com/api/projects/status/dm4kqxhin5uxiey0/branch/master?svg=true)](https://ci.appveyor.com/project/wjakob/instant-meshes/branch/master)

<img width="170" height="166" src="https://github.com/wjakob/instant-meshes/raw/master/resources/icon.png">

This repository contains the interactive meshing software developed as part of the publication

> **Instant Field-Aligned Meshes**<br/>
> Wenzel Jakob, Marco Tarini, Daniele Panozzo, Olga Sorkine-Hornung<br/>
> In *ACM Transactions on Graphics (Proceedings of SIGGRAPH Asia 2015)*<br/>
> [PDF](http://igl.ethz.ch/projects/instant-meshes/instant-meshes-SA-2015-jakob-et-al.pdf),
> [Video](https://www.youtube.com/watch?v=U6wtw6W4x3I),
> [Project page](http://igl.ethz.ch/projects/instant-meshes/)


##### In commercial software

Since version 10.2, Modo uses the Instant Meshes algorithm to implement its
automatic retopology feature. An interview discussing this technique and more
recent projects is available [here](https://www.foundry.com/trends/design-visualisation/mitsuba-renderer-instant-meshes).

## Screenshot

![Instant Meshes logo](https://github.com/wjakob/instant-meshes/raw/master/resources/screenshot.jpg)

## Pre-compiled binaries

The following binaries (Intel, 64 bit) are automatically generated from the latest GitHub revision.

> [Microsoft Windows](https://instant-meshes.s3.eu-central-1.amazonaws.com/Release/instant-meshes-windows.zip)<br/>
> [Mac OS X](https://instant-meshes.s3.eu-central-1.amazonaws.com/instant-meshes-macos.zip)<br/>
> [Linux](https://instant-meshes.s3.eu-central-1.amazonaws.com/instant-meshes-linux.zip)

Please also fetch the following dataset ZIP file and extract it so that the
``datasets`` folder is in the same directory as ``Instant Meshes``, ``Instant Meshes.app``,
or ``Instant Meshes.exe``.

> [Datasets](https://instant-meshes.s3.eu-central-1.amazonaws.com/instant-meshes-datasets.zip)

Note: On Linux, Instant Meshes relies on the program ``zenity``, which must be installed.

## Compiling

Compiling from scratch requires CMake and a recent version of XCode on Mac,
Visual Studio 2015 on Windows, and GCC on Linux. 

On MacOS, compiling should be as simple as

    git clone --recursive https://github.com/wjakob/instant-meshes
    cd instant-meshes
    cmake .
    make -j 4

To build on Linux, please install the prerequisites ``libxrandr-dev``,
``libxinerama-dev``, ``libxcursor-dev``, and ``libxi-dev`` and then use the
same sequence of commands shown above for MacOS.

On Windows, open the generated file ``InstantMeshes.sln`` after step 3 and proceed building as usual from within Visual Studio.

## Usage

To get started, launch the binary and select a dataset using the "Open mesh" button on the top left (the application must be located in the same directory as the 'datasets' folder, otherwise the panel will be empty).

The standard workflow is to solve for an orientation field (first blue button) and a position field (second blue button) in sequence, after which the 'Export mesh' button becomes active. Many user interface elements display a descriptive message when hovering the mouse cursor above for a second.

A range of additional information about the input mesh, the computed fields,
and the output mesh can be visualized using the check boxes accessible via the
'Advanced' panel.

Clicking the left mouse button and dragging rotates the object; right-dragging
(or shift+left-dragging) translates, and the mouse wheel zooms. The fields can also be manipulated using brush tools that are accessible by clicking the first icon in each 'Tool' row.
