#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script generates a graphic of Blender's code layout:

./blender.bin -b --factory-startup --python ./tools/utils_doc/code_layout_diagram.py
"""
# This is an update to the historic: https://download.blender.org/ftp/ideasman42/pics/code_layout_historic.jpg
# Originally located at: `www.blender.org/bf/codelayout.jpg` (now broken).

__all__ = (
    "main",
)

from dataclasses import dataclass

import os
import sys
import bpy

from mathutils import (
    Euler,
    Vector,
)


# -----------------------------------------------------------------------------
# Generic Globals

BASE_DIR = os.path.normpath(os.path.dirname(__file__))
ROOT_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", ".."))


# -----------------------------------------------------------------------------
# Data

PAGE_WIDTH = 4.5
PAGE_WIDTH_HALF = PAGE_WIDTH / 2.0

# RESOLUTION_SCALE = 0.25  # For quick test renders.
RESOLUTION_SCALE = 1.0
RESOLUTION_X = 3200

OUTPUT_IMAGE_FILE = True
OUTPUT_BLEND_FILE = False

# Can't use `Inter.woff2` as it contains overlapping glyphs.
FONT_FILE_DEFAULT = os.path.join(ROOT_DIR, "release", "datafiles", "fonts", "Noto Sans CJK Regular.woff2")
FONT_FILE_MONO = os.path.join(ROOT_DIR, "release", "datafiles", "fonts", "DejaVuSansMono.woff2")

# Apply some offsets, needed as the "default" font has strange scaling.
FONT_STYLE_SETTINGS = {
    "default": {"scale": 1.8, "space_line": 0.5},
    "mono": {"scale": 1.0, "space_line": 1.0},
}

VFONT_FROM_STYLE = {
    "default": None,
    "mono": None,
}

MATERIAL_FROM_COLOR = {
    "black": None,
    "grey": None,
}

MATERIAL_INDEX_BLACK = 0
MATERIAL_INDEX_GREY = 1

# -----------------------------------------------------------------------------
# Graphics

ARROW_DOWN = ((0, 0), (-1, 1.5), (-0.3, 1.5), (-0.3, 3.3), (0.3, 3.3), (0.3, 1.5), (1, 1.5),)
ARROW_DOWN_AND_SIDEWAYS = (
    (0, 0), (-1, 1.5), (-0.3, 1.5), (-0.3, 2.7), (-1.5, 2.7), (-1.5, 2), (-3, 3), (-1.5, 4), (-1.5, 3.3), (1.5, 3.3),
    (1.5, 4), (3, 3), (1.5, 2), (1.5, 2.7), (0.3, 2.7), (0.3, 1.5), (1, 1.5),
)


# -----------------------------------------------------------------------------
# Directory Layout Definition

@dataclass
class SectionData:
    heading: str
    source_code_base: str
    # Don't draw any arrows when None.
    source_code_call_sibling_modules: bool | None
    source_code_dirs: list


SECTIONS = (
    SectionData(
        heading="Application startup",
        source_code_call_sibling_modules=False,
        source_code_base="source/",
        source_code_dirs=[
            ("creator", "Blender's main() function, initialization & argument handling."),
            ("blender", "Contains the majority of Blender's functionality."),
        ],
    ),
    SectionData(
        heading="Editor definitions, drawing, interaction",
        source_code_call_sibling_modules=True,
        source_code_base="source/blender/editors/",
        source_code_dirs=[
            ("space_action", "Animation editor for actions."),
            ("space_buttons", "Properties editor."),
            ("space_clip", "Movie clip editor."),
            ("space_console", "Python interactive console."),
            ("space_file", "File selection."),
            ("space_graph", "Animation editor for F-Curves."),
            ("space_image", "Image editor & texture painting."),
            ("space_info", "Info space (for logging)."),
            ("space_nla", "Animation non-linear-action editor."),
            ("space_node", "Node editor for compositor, geometry and other nodes."),
            ("space_outliner", "Outliner editor."),
            ("space_script", "Unused script space (historic)."),
            ("space_sequencer", "Video sequence editor."),
            ("space_spreadsheet", "Spreadsheet data editor."),
            ("space_statusbar", "Window status bar."),
            ("space_text", "Text editor."),
            ("space_topbar", "Window file menu."),
            ("space_userpref", "User preferences."),
            ("space_view3d", "3D viewport."),
        ],
    ),
    SectionData(
        heading="Editor utilities",
        source_code_call_sibling_modules=True,
        source_code_base="source/blender/editors/",
        source_code_dirs=[
            ("datafiles", "Definitions for data-files used by editors: icons, material previews & startup file."),
            ("gizmo_library", "Shared shapes for gizmo drawing."),
            ("screen", "Screen operators & manipulation, including windowing operations."),
            ("space_api", "Space shared API's."),
            ("undo", "Undo operators and high-level API."),
            ("util", "Shared utilities including numeric input, image access, gizmo drawing & selection logic."),
        ],
    ),
    SectionData(
        heading="Editor Tools",
        source_code_call_sibling_modules=True,
        source_code_base="source/blender/editors/",
        source_code_dirs=[
            ("animation", "Utilities for key-framing and animation editors."),
            ("armature", "Tools for editing armature object data."),
            ("asset", "Tools for the asset editors."),
            ("curve", "Legacy curve object support."),
            ("curves", "Curve object support."),
            ("geometry", "Generic utilities for dealing with geometry (meshes, curves ... etc)."),
            ("gpencil_legacy", "Tools for editing grease pencil, legacy now only used for \"Annotations\"."),
            ("grease_pencil", "Operators and utilities to manipulate grease mencil."),
            ("id_management", "Operators and utilities for managing ID data blocks."),
            ("include", "Shared includes for editor modules which share logic with other editors."),
            ("interface", "Graphical user interface logic for widgets and their interactions."),
            ("io", "Operators for importing & exporting 3D content such as geometry, grease pencil & animation."),
            ("lattice", "Tools for editing lattice object data."),
            ("mask", "Tools for editing and animating 2D masks."),
            ("mesh", "Tools for editing mesh object data."),
            ("metaball", "Tools for editing meta-ball object data."),
            ("object", "Tools for editing objects and collections."),
            ("physics", "Tools for physics including particles, rigidbody & dynamic paint."),
            ("pointcloud", "Tools for point-cloud manipulation."),
            ("render", "Render operators, viewing & viewport preview."),
            ("scene", "Tools for scene manipulation (add/copy/remove)."),
            ("sculpt_paint", "Tools for sculpting and painting."),
            ("sound", "Tools for sound manipulation, packing, unpacking & mixing down audio."),
            ("transform", "Interactive transform and transform implementations (rotate, scale, move ... etc)."),
            ("uvedit", "Mesh UV editing tools for copy/paste, selection & packing."),
        ],
    ),
    SectionData(
        heading="Window, events, operators, core interaction",
        source_code_call_sibling_modules=False,
        source_code_base="source/blender/",
        source_code_dirs=[
            ("windowmanager", "General window, event handling."),
        ],
    ),
    SectionData(
        heading="General Blender API's",
        source_code_call_sibling_modules=True,
        source_code_base="source/blender/",
        source_code_dirs=[
            ("asset_system", "Asset system back-end (asset representations, asset libraries, catalogs, etc.)."),
            ("blendthumb", "Thumbnail extraction for the BLEND file-format."),
            ("blenfont", "Font loading and rendering. Used by the user-interface and stamping text into images."),
            ("animrig", "Animation & rigging functionality including key-framing, drivers NLA & actions."),
            ("blenkernel",
             "Kernel functions "
             "(data structure manipulation, allocation, free. No interactive tools or UI logic, very low level)."),
            ("blenlib",
             "Internal misc libraries: "
             "math functions, lists, random, noise, memory pools, file operations (platform agnostic)."),
            ("blenloader", "Blend file loading and writing as well as in memory undo file system."),
            ("blenloader_core", "Core blend file access (shared by Blender & blend-thumbnail extraction)."),
            ("blentranslation", "Internal support for non-English translations (gettext)."),
            ("bmesh", "Mesh-data manipulation. Used by mesh edit-mode."),
            ("compositor", "Image compositor which implements the compositor and various nodes."),
            ("cpucheck", "Support detecting if the CPU is capable of running Blender."),
            ("datatoc", "Utility used by the build-system to convert data-files into source."),
            ("depsgraph",
             "Dependency graph API. Used to track relations between various pieces of data in a Blender file."),
            ("draw", "Viewport drawing and Eevee."),
            ("editors", "Logic for (expanded on in other sections)."),
            ("freestyle", "Freestyle NPR rendering engine (scene graph, Python code, stroke, geometry.. etc)."),
            ("functions",
             "Run-time type system that implements lazy functions, multi-function network & generic functions. "
             "Used by geometry nodes."),
            ("geometry", "Low level geometry functionality. Used by object-modifiers and geometry nodes."),
            ("gpu", "Abstract graphics API's (OpenGL, Metal & Vulkan)."),
            ("ikplugin", "Generic API for inverse-kinematics (IK) solvers. Used by the IK constraint."),
            ("imbuf", "Image buffer API for image manipulation."),
            ("io", "Input/output libraries for various format (including 2D & 3D file formats)."),
            ("makesdna", "Extracting data-definitions stored in the BLEND file-format."),
            ("makesrna", "Defines access method for DNA. Used by the user-interface, Python API & animation system."),
            ("modifiers", "Object modifiers."),
            ("nodes", "Nodes code: `CMP`: composite, `GEO`: geometry, `SHD`: material, `TEX`: texture."),
            ("python", "Python API integration."),
            ("render", "Rendering & baking, integrates various rendering engines, outputs to image & video formats."),
            ("sequencer", "Video sequence editor."),
            ("shader_fx", "Grease pencil effects."),
            ("simulation", "Hair simulation."),
        ],
    ),
    SectionData(
        heading="Internal Utilities (maintained internally)",
        source_code_call_sibling_modules=False,
        source_code_base="intern/",
        source_code_dirs=[
            ("atomic", "Low level operations lockless concurrent programming."),
            ("audaspace", "Wrapper (see extern)."),
            ("clog", "C-logging library."),
            ("uriconvert", "Support encoding strings (typically paths) as URI's."),
            ("cycles", "Cycles rendering engine."),
            ("dualcon", "Re-meshing "),
            ("eigen", "Wrapper (see extern/Eigen3/)"),
            ("ghost", "Platform abstraction for windowing and user (mouse, keyboard) input."),
            ("guardedalloc", "Memory allocator used across most of Blender's internal code."),
            ("iksolver", "An IK-solver (useful for character animation)"),
            ("itasc", "An IK-solver (useful for robotics)."),
            ("libc_compat", "C-library compatibility (Linux only)."),
            ("libmv", "Movie clip motion tracking solver."),
            ("mantaflow", "Wrapper (see extern)."),
            ("memutil", "Memory allocation utilities."),
            ("mikktspace", "Calculation for mesh tangents from normals & UV's."),
            ("opensubdiv", "Wrapper (see libraries)."),
            ("openvdb", "OpenVDB wrapper for volumetric data support."),
            ("quadriflow", "Wrapper for quadriflow re-meshing."),
            ("renderdoc_dynload", "Dynamic loader (see libraries)."),
            ("rigidbody", "Wrapper for bullet physics (see extern)."),
            ("sky", "Calculate skylight & solar radiance models."),
            ("slim", "Calculate UV unwrapping, used for the \"Minimum Stretch\" method."),
            ("utfconv", "Unicode text conversion."),
            ("wayland_dynload", "Dynamic loader for wayland (Unix only)."),
        ],
    ),
    SectionData(
        heading="External Utilities (maintained externally)",
        source_code_call_sibling_modules=False,
        source_code_base="extern/",
        source_code_dirs=[
            ("audaspace", "A high level audio library for portable audio output."),
            ("binreloc", "Support access binary locations (Linux only)."),
            ("bullet2", "Physics solver with rigid-body support."),
            ("ceres", "A library for modeling and solving large, complicated optimization problems."),
            ("cuew", "Extension wrangler for CUDA."),
            ("curve_fit_nd", "Fit bezier-splines to sampled points. Used for free-hand drawing."),
            ("draco", "Compressed 3D file format support."),
            ("Eigen3", "A C++ template library for linear algebra."),
            ("fast_float", "Fast floating point number parsing."),
            ("fmtlib", "A modern C++ string formatting library."),
            ("gflags", "A library to parse command-line flags. Used by glog."),
            ("glew-es", "Extension wrangler for GLEW-ES."),
            ("glog", "Google C++ logging library. Used by gtest & ceres."),
            ("gmock", "Google C++ mocking library. Used by gtest."),
            ("gtest", "Google C++ testing framework. Used for C & C++ tests."),
            ("hipew", "Wrapper for the HIP C++ library, portability library for AMD & NVIDIA GPUs."),
            ("json", "JSON file format support."),
            ("lzma", "High quality but slower (de)compression library."),
            ("lzo", "Fast (de)compression library."),
            ("mantaflow", "Fluid simulation."),
            ("nanosvg", "Salable vector graphics support."),
            ("quadriflow", "Remeshing that rebuilds the geometry with a more uniform topology."),
            ("rangetree", "Efficient range storage."),
            ("renderdoc", "Graphical GPU debugger."),
            ("tinygltf", "GLTF 3D file format support."),
            ("ufbx", "FBX 3D file format loading support, used by the C++ FBX importer."),
            ("vulkan_memory_allocator", "Memory allocation utilities for Vulkan GPU back-end."),
            ("wcwidth", "Unicode character width."),
            ("xdnd", "Drag & drop support for the X11 graphical environment."),
            ("wintab", "Tablet input device support for MS-Windows."),
            ("xxhash", "Fast non-cryptographic hashing functions."),
        ],
    ),
    SectionData(
        heading="Pre-compiled Libraries (in SVN, or require install)",
        source_code_call_sibling_modules=False,
        source_code_base="lib/{platform}/",
        source_code_dirs=[
            ("alembic", "An interchangeable computer graphics file format for baked mesh data."),
            ("brotli", "Compression library used to decompress WOFF2 fonts."),
            ("dpcpp", "OneAPI DPC++/C++ Compiler. Used by Cycles oneAPI."),
            ("embree", "A collection of high-performance ray tracing kernels. Used by Cycles."),
            ("epoxy", "Extension wrangler for OpenGL."),
            ("ffmpeg", "Movie encoding/decoding library."),
            ("fftw3", "Fast fourier transformation library."),
            ("freetype", "Font reading & rendering library."),
            ("fribidi",
             "Bi-directional text support (right-to-left) for Arabic & Hebrew script. "
             "Intended for complex text shaping (not yet supported)."),
            ("gmp", "Arbitrary precision arithmetic library."),
            ("harfbuzz", "Text shaping engine for complex script."),
            ("haru", "PDF generation library."),
            ("hiprt", "Ray-tracing for AMD GPU's. Used by Cycles."),
            ("imath", "Library used by OpenEXR image-format."),
            ("jemalloc", "An improved memory allocator."),
            ("jpeg", "JPEG image-format support."),
            ("level-zero", "OneAPI loader & validation. Used by Cycles oneAPI."),
            ("llvm", "Low level virtual machine. Used by OSL."),
            ("manifold", "A library for operating on manifold meshes. Used as a boolean solver."),
            ("materialx", "A standard for representing materials. Used by USD, Hydra & Blender's shader nodes."),
            ("mesa", "Used for it's software OpenGL implementation."),
            ("openal", "Cross platform audio output."),
            ("opencolorio", "A solution for highly precise, performant, and consistent color management."),
            ("openexr", "EXR image-format support."),
            ("openimagedenoise", "Denoising filters for images rendered with ray tracing. Used by Cycles."),
            ("openimageio", "A library for reading, writing, and processing images in a wide variety of file formats."),
            ("openjpeg", "JPEG image-format support."),
            ("openpgl", "Intel open path guiding library. Used by Cycles."),
            ("opensubdiv", "Subdivision surface support for meshes."),
            ("openvdb", "Volumetric file-format support."),
            ("osl", "Open Shading Language. Used by Cycles."),
            ("png", "PNG image-format support."),
            ("potrace", "Trace bitmap images to vectors."),
            ("pugixml", "Light-weight C++ XML processing library."),
            ("python", "Python scripting language."),
            ("sdl", "Simple DirectMedia Layer to abstract platform specific code."),
            ("shaderc", "Shader compilation utilities. Used for GLSL shaders to be used with Vulkan."),
            ("sndfile", "Sound encoding/decoding library."),
            ("spnav", "3D mouse support, also known as NDOF. (Unix only)."),
            ("tbb", "Threading Building Blocks (C++ library)."),
            ("tiff", "TIFF image-format support."),
            ("usd", "Universal Scene Description for describing, composing, simulating, & collaborating 3D worlds."),
            ("wayland", "Wayland scanner (Unix only)."),
            ("wayland-protocols", "Wayland protocol definitions (Unix only)."),
            ("wayland_libdecor", "Wayland windowing (Unix only)."),
            ("wayland_weston", "Wayland compositor, used for running headless UI tests (Unix only)."),
            ("webp", "Support for the WEBP image format."),
            ("xml2", "XML encoding / decoding library."),
            ("xr_openxr_sdk", "Virtual reality library."),
            ("zlib", "ZLIB (GZIP) compression library."),
            ("zstd", "Z-standard compression library."),
            ("vulkan", "The Vulkan graphics API. Used by the Vulkan GPU back-end.")
        ],
    ),
    SectionData(
        heading="Operating system",
        source_code_call_sibling_modules=None,
        source_code_base="",
        source_code_dirs=[
            ("GPU Driver", "OpenGL (or Metal on Apple)."),
            ("Standard C", "Also known as libc."),
            ("C++ & STL", "C++ Standard Library."),
            ("Windowing", "Cocoa on Apple.\nX11/Wayland on Unix.\nWIN32 on MS-Window."),
        ],
    ),

    SectionData(
        heading="Other Directories",
        source_code_call_sibling_modules=None,
        source_code_base="",
        source_code_dirs=[
            ("assets", "Bundled assets such as brushes & nodes, accessible from the default installation."),
            ("build_files", "Files used by CMake, the build-bot & utilities for packaging blender builds."),
            ("doc", "Scripts for building Blender's Python's API docs, man-page and DOXYGEN documentation."),
            ("locale", "Translations for Blender's interface."),
            ("release", "Files bundled with Blender including fonts, icons, desktop files."),
            ("tests", "Files to run tests & the GIT-LFS test data."),
            ("scripts", "Bundled Python scripts for UI layout, key-map & some operators."),
            ("tools", "Utilities to help with Blender development."),
        ],
    ),

)


# -----------------------------------------------------------------------------
# Generic Utilities

def _function_id() -> str:
    '''
    Create a string naming the function n frames up on the stack.
    '''
    # pylint: disable-next=protected-access
    co = sys._getframe(1).f_code
    return '{:s}:{:d}:'.format(co.co_name, co.co_firstlineno)


def object_data_materials_setup_default(object_data):
    object_data.materials.append(MATERIAL_FROM_COLOR["black"])
    object_data.materials.append(MATERIAL_FROM_COLOR["grey"])


# -----------------------------------------------------------------------------
# Internal Utility Classes

@dataclass
class Box2D:
    min_x: float
    max_x: float
    min_y: float
    max_y: float

    def size(self):
        return self.max_x - self.min_x, self.max_y - self.min_y

    def union(self, *rest):
        min_x, max_x, min_y, max_y = self.min_x, self.max_x, self.min_y, self.max_y
        for other in rest:
            min_x = min(min_x, other.min_x)
            max_x = max(max_x, other.max_x)
            min_y = min(min_y, other.min_y)
            max_y = max(max_y, other.max_y)
        return Box2D(min_x=min_x, max_x=max_x, min_y=min_y, max_y=max_y)

    def expanded(self, value):
        result = self.copy()
        result.min_x -= value
        result.max_x += value
        result.min_y -= value
        result.max_y += value
        return result

    def expanded_x(self, value):
        result = self.copy()
        result.min_x -= value
        result.max_x += value
        return result

    def expanded_y(self, value):
        result = self.copy()
        result.min_y -= value
        result.max_y += value
        return result

    def copy(self, *, min_x=None, max_x=None, min_y=None, max_y=None):
        result = self.union()
        if min_x is not None:
            result.min_x = min_x
        if max_x is not None:
            result.max_x = max_x
        if min_y is not None:
            result.min_y = min_y
        if max_y is not None:
            result.max_y = max_y
        return result

    def isect_x(self, other):
        if other.max_x < self.min_x:
            return False
        if self.max_x < other.min_x:
            return False
        return True


@dataclass
class TextStyle:
    font: str
    size: float

    @property
    def line_height(self):
        return self.size * 0.01

    def apply(self, txt_data):
        settings = FONT_STYLE_SETTINGS[self.font]

        scale = 0.01 * settings["scale"]
        space_line = settings["space_line"]

        txt_data.size = self.size * scale
        txt_data.space_line = space_line
        txt_data.font = VFONT_FROM_STYLE[self.font]


text_style_title = TextStyle(font="default", size=20.0)
text_style_subheading = TextStyle(font="default", size=14.0)
text_style_body = TextStyle(font="default", size=8.0)
text_style_dir_title = TextStyle(font="mono", size=8.0)
text_style_dir_body = TextStyle(font="default", size=6.0)


# -----------------------------------------------------------------------------
# Generic Drawing Utilities

def box_2d_from_vectors(vectors):
    min_x = max_x = vectors[0][0]
    min_y = max_y = vectors[0][1]
    for i in range(1, len(vectors)):
        x, y = vectors[i][0:2]
        min_x = min(x, min_x)
        max_x = max(x, max_x)
        min_y = min(y, min_y)
        max_y = max(y, max_y)
    return Box2D(min_x=min_x, max_x=max_x, min_y=min_y, max_y=max_y)


def box_2d_from_vectors_with_matrix(vectors, matrix):
    return box_2d_from_vectors([matrix @ Vector(v) for v in vectors])


def draw_text_centered(scene, *, location, text, style):
    name_gen = _function_id() + repr(text)
    txt_data = bpy.data.curves.new(name=name_gen, type='FONT')
    object_data_materials_setup_default(txt_data)

    # Text Object
    txt_data.body = text
    style.apply(txt_data)
    txt_data.align_x = 'CENTER'
    txt_ob = bpy.data.objects.new(name=name_gen, object_data=txt_data)
    txt_ob.location.xy = location

    scene.collection.objects.link(txt_ob)

    bpy.context.view_layer.update()
    return box_2d_from_vectors_with_matrix(txt_ob.bound_box, txt_ob.matrix_world.copy()), txt_ob


def draw_text_left(scene, *, location, text, style):
    name_gen = _function_id() + repr(text)
    txt_data = bpy.data.curves.new(name=name_gen, type='FONT')
    object_data_materials_setup_default(txt_data)

    # Text Object
    txt_data.body = text
    style.apply(txt_data)
    txt_data.align_x = 'LEFT'
    txt_ob = bpy.data.objects.new(name=name_gen, object_data=txt_data)
    txt_ob.location.xy = location

    scene.collection.objects.link(txt_ob)

    bpy.context.view_layer.update()
    return box_2d_from_vectors_with_matrix(txt_ob.bound_box, txt_ob.matrix_world.copy()), txt_ob


def draw_text_left_with_bounds(scene, *, box, text, style):
    name_gen = _function_id() + repr(text)
    txt_data = bpy.data.curves.new(name=name_gen, type='FONT')
    object_data_materials_setup_default(txt_data)

    # Text Object
    txt_data.body = text
    style.apply(txt_data)
    txt_data.align_x = 'LEFT'
    txt_box = txt_data.text_boxes[0]
    txt_box.x = box.min_x
    txt_box.y = box.max_y - style.line_height
    txt_box.width, txt_box.height = box.size()

    txt_ob = bpy.data.objects.new(name=name_gen, object_data=txt_data)

    scene.collection.objects.link(txt_ob)

    bpy.context.view_layer.update()
    return box_2d_from_vectors_with_matrix(txt_ob.bound_box, txt_ob.matrix_world.copy()), txt_ob


def draw_arrow(scene, *, location, size, line_width, arrow_data):
    name_gen = _function_id() + repr(location)

    import bmesh
    bm = bmesh.new()

    verts = [
        bm.verts.new(((co[0] * size) + location[0], (co[1] * size) + location[1], 0.0))
        for co in arrow_data
    ]

    f = bm.faces.new(verts)
    bm.normal_update()

    bmesh.ops.inset_individual(
        bm,
        faces=[f],
        use_even_offset=True,
        use_relative_offset=False,
        thickness=line_width,
    )

    bm.faces.remove(f)

    me = bpy.data.meshes.new(name=name_gen)
    object_data_materials_setup_default(me)
    bm.to_mesh(me)
    bm.free()

    line_ob = bpy.data.objects.new(name=name_gen, object_data=me)
    scene.collection.objects.link(line_ob)


# -----------------------------------------------------------------------------
# Setup

def setup_page():
    pass


# -----------------------------------------------------------------------------
# Drawing

def draw_mesh_dashed_line(bm, *, min_x, max_x, y, dash_on, dash_off, line_width, skip_box):
    x = min_x + (dash_off / 2)
    line_width_half = line_width / 2
    while x < max_x:
        beg_x = x
        end_x = x + dash_on
        if not skip_box.isect_x(Box2D(min_x=beg_x, max_x=end_x, min_y=0.0, max_y=0.0)):
            quad = [bm.verts.new() for i in range(4)]
            quad[0].co.xy = beg_x, y + line_width_half
            quad[1].co.xy = beg_x, y - line_width_half
            quad[2].co.xy = end_x, y - line_width_half
            quad[3].co.xy = end_x, y + line_width_half
            bm.faces.new(quad)
        x += dash_on + dash_off


def draw_mesh_box(bm, *, box, material_index=MATERIAL_INDEX_BLACK):
    quad = [bm.verts.new() for i in range(4)]
    quad[0].co.xy = box.min_x, box.min_y
    quad[1].co.xy = box.max_x, box.min_y
    quad[2].co.xy = box.max_x, box.max_y
    quad[3].co.xy = box.min_x, box.max_y
    f = bm.faces.new(quad)
    if material_index > 0:
        f.material_index = material_index


def draw_mesh_box_wire(bm, *, box, line_width):
    draw_mesh_box(bm, box=box.copy(max_x=box.min_x + line_width))
    draw_mesh_box(bm, box=box.copy(min_x=box.max_x - line_width))
    draw_mesh_box(bm, box=box.copy(max_y=box.min_y + line_width))
    draw_mesh_box(bm, box=box.copy(min_y=box.max_y - line_width))


def draw_mesh_box_drop_shadow(bm, *, box, size):
    # Right hand side.
    draw_mesh_box(
        bm,
        box=box.copy(
            min_x=box.max_x,
            max_x=box.max_x + size,
            min_y=box.min_y - size,
            max_y=box.max_y - size,
        ),
        material_index=MATERIAL_INDEX_GREY,
    )
    # Bottom.
    draw_mesh_box(
        bm,
        box=box.copy(
            min_x=box.min_x + size,
            max_x=box.max_x + size,
            min_y=box.min_y - size,
            max_y=box.min_y,
        ),
        material_index=MATERIAL_INDEX_GREY,
    )


# -----------------------------------------------------------------------------
# Drawing

def draw_centered_title(scene, step_y, text):
    box, ob = draw_text_centered(scene, location=(0.0, step_y), text=text, style=text_style_title)
    return box, ob


def sub_section_line(scene, box, step_y):
    name_gen = _function_id() + repr(box)

    import bmesh

    bm = bmesh.new()

    arrow_h = 0.075
    arrow_w = 0.1

    for sign in (-1.0, 1.0):
        # Left triangle.
        vs = [bm.verts.new() for i in range(3)]
        l = PAGE_WIDTH_HALF * sign
        vs[0].co.xy = l, step_y
        vs[1].co.xy = l - (0.1 * sign), step_y - (arrow_h / 2)
        vs[2].co.xy = l - (0.1 * sign), step_y + (arrow_h / 2)
        bm.faces.new(vs)

    # Horizontal line.
    draw_mesh_dashed_line(
        bm,
        min_x=-(PAGE_WIDTH_HALF - arrow_w),
        max_x=+(PAGE_WIDTH_HALF - arrow_w),
        y=step_y,
        dash_on=0.0375,
        dash_off=0.0375,
        line_width=0.015,
        skip_box=box.expanded_x(0.0375),
    )

    me = bpy.data.meshes.new(name=name_gen)
    object_data_materials_setup_default(me)
    bm.to_mesh(me)
    bm.free()

    line_ob = bpy.data.objects.new(name=name_gen, object_data=me)
    scene.collection.objects.link(line_ob)


def draw_directories(scene, box_body, source_code_dirs):
    name_gen = _function_id() + repr(source_code_dirs)

    import bmesh

    box_result = box_body.copy()

    box_line_width = 0.01
    box_size = 0.75, 0.3
    box_gap = 0.075, 0.075
    box_inner_margin = 0.04
    bm = bmesh.new()
    box_dropshadow_size = 0.025

    me = bpy.data.meshes.new(name=name_gen)

    step_x = box_body.min_x
    step_y = box_body.min_y

    # Adjust the box height based on the bounds of the body-text
    # (requires postponing drawing the boxes so the size is known).
    USE_FLEXIBLE_SIZE_X = True
    # Use double-width boxes when the directory name doesn't fit.
    USE_FLEXIBLE_SIZE_Y = True

    boxes_pending = []

    def draw_directory_boxes_pending():
        if not boxes_pending:
            return None
        y = boxes_pending[0][1].min_y
        for _, box_text in boxes_pending:
            y = min(y, box_text.min_y)
        y -= box_inner_margin

        for box_draw, _ in boxes_pending:
            box = box_draw.copy(min_y=y)
            draw_mesh_box_wire(bm, box=box, line_width=box_line_width)
            draw_mesh_box_drop_shadow(bm, box=box, size=box_dropshadow_size)

        bounds = boxes_pending[0][0].union(*[box_draw for box_draw, _ in boxes_pending])
        bounds.min_y = y

        boxes_pending.clear()
        return bounds

    for dir_name, description in source_code_dirs:
        box_size_flex = box_size

        # Draw text.
        box_dir, box_dir_ob = draw_text_left(
            scene,
            # First get the bounds.
            location=(0.0, 0.0),
            text=dir_name,
            style=text_style_dir_title,
        )
        if USE_FLEXIBLE_SIZE_X:
            if box_size_flex[0] < (box_dir.size()[0] + box_inner_margin * 2.0):
                box_size_flex = box_size_flex[0] + box_gap[0] + box_size[0], box_size[1]

        if step_x + box_size_flex[0] + box_gap[0] > box_body.max_x:
            step_y_size = box_size_flex[1]
            if USE_FLEXIBLE_SIZE_Y:
                box_pending = draw_directory_boxes_pending()
                if box_pending is not None:
                    step_y_size = box_pending.size()[1]
            step_y -= step_y_size + box_gap[1]
            step_x = box_body.min_x

        box_draw = Box2D(min_x=step_x, max_x=step_x + box_size_flex[0], min_y=step_y - box_size_flex[1], max_y=step_y)
        if not USE_FLEXIBLE_SIZE_Y:
            draw_mesh_box_wire(bm, box=box_draw, line_width=box_line_width)
            draw_mesh_box_drop_shadow(bm, box=box_draw, size=box_dropshadow_size)

        next_y = step_y - ((text_style_dir_title.line_height * 0.75) + box_inner_margin)
        next_y_body_begin = step_y - (text_style_dir_title.line_height + box_inner_margin)

        box_dir_ob.location.xy = (step_x + box_inner_margin, next_y)

        if description:
            box_text_bounds, _ = draw_text_left_with_bounds(
                scene,
                box=Box2D(
                    min_x=box_draw.min_x + box_inner_margin,
                    max_x=box_draw.max_x - box_inner_margin,
                    min_y=box_draw.min_y,
                    max_y=next_y_body_begin,
                ),
                text=description,
                style=text_style_dir_body,
            )
        if USE_FLEXIBLE_SIZE_Y:
            if description:
                boxes_pending.append((box_draw, box_text_bounds))
            else:
                boxes_pending.append((box_draw, box_draw.copy(min_y=next_y_body_begin)))

        step_x += box_size_flex[0] + box_gap[0]

    step_y_size = box_size[1]
    if USE_FLEXIBLE_SIZE_Y:
        box_pending = draw_directory_boxes_pending()
        if box_pending is not None:
            step_y_size = box_pending.size()[1]

    box_result.min_y = step_y - (step_y_size + box_gap[1])

    object_data_materials_setup_default(me)
    bm.to_mesh(me)
    bm.free()

    line_ob = bpy.data.objects.new(name=name_gen, object_data=me)
    scene.collection.objects.link(line_ob)

    return box_result


def draw_legend(scene, *, step_y, legend_data, text):

    unit = PAGE_WIDTH / 16.0

    x_left = -PAGE_WIDTH_HALF + (unit * 3)

    draw_arrow(
        scene,
        location=(x_left, step_y - 0.25),
        size=0.0575,
        line_width=0.0075,
        arrow_data=legend_data,
    )

    # Draw text.
    box_dir, _box_dir_ob = draw_text_left(
        scene,
        # First get the bounds.
        location=(x_left + 0.25, step_y - 0.175),
        text=text,
        style=text_style_subheading,
    )

    box_body = Box2D(
        min_x=-PAGE_WIDTH_HALF + unit * 3,
        max_x=box_dir.max_x,
        min_y=step_y - 0.25,
        max_y=step_y,
    )

    return box_body


def draw_sub_section(scene, *, step_y, section_data,):

    box_sub_heading, _ = draw_text_centered(
        scene,
        # The 0.025 tweak is needed to be vertically centered.
        # This depends on the exact font used.
        location=(0.0, step_y + 0.025),
        text=section_data.heading,
        style=text_style_subheading,
    )

    # Add dashes and arrows around.
    sub_section_line(scene, box_sub_heading, step_y + 0.05)

    step_y -= 0.2

    box_dir, _ = draw_text_left(
        scene,
        location=(-PAGE_WIDTH_HALF, step_y),
        text=section_data.source_code_base,
        style=text_style_body,
    )

    step_y += 0.1

    # Box with no height to draw mini boxes into.
    unit = PAGE_WIDTH / 16.0
    box_body = Box2D(
        min_x=-PAGE_WIDTH_HALF + unit * 3,
        max_x=PAGE_WIDTH_HALF - unit,
        min_y=step_y,
        max_y=step_y,
    )

    box_dirs = draw_directories(scene, box_body, section_data.source_code_dirs)

    # Draw an arrow on the right hand side.
    if section_data.source_code_call_sibling_modules is not None:
        draw_arrow(
            scene,
            location=(PAGE_WIDTH_HALF - 0.175, step_y - 0.25),
            size=0.0575,
            line_width=0.0075,
            arrow_data=(
                ARROW_DOWN_AND_SIDEWAYS if section_data.source_code_call_sibling_modules else
                ARROW_DOWN
            ),
        )

    return box_sub_heading.union(box_dir, box_body, box_dirs)


# -----------------------------------------------------------------------------
# Render Output

def render_output(scene, bounds, filepath):
    name_gen = _function_id()

    scene.render.filepath = filepath

    world = bpy.data.worlds.new(name_gen)
    world.node_tree.nodes.clear()
    output = world.node_tree.nodes.new("ShaderNodeOutputWorld")
    background = world.node_tree.nodes.new("ShaderNodeBackground")
    world.node_tree.links.new(output.outputs["Surface"], background.outputs["Surface"])
    background.inputs["Color"].default_value = 1.0, 1.0, 1.0, 1.0
    scene.world = world

    # Some space around the edges.
    bounds_size = bounds.expanded(0.15).size()

    camera_data = bpy.data.cameras.new(name_gen)
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = max(bounds_size)

    camera = bpy.data.objects.new(name_gen, camera_data)
    camera.rotation_euler = Euler((0.0, 0.0, 0.0), 'XYZ')

    scene.camera = camera
    camera.location = (0.0, (bounds.min_y + bounds.max_y) / 2.0, 10.0)
    scene.collection.objects.link(camera)

    render = scene.render
    render.image_settings.media_type = 'IMAGE'
    render.image_settings.file_format = 'JPEG'
    render.image_settings.color_depth = '8'
    render.image_settings.color_mode = 'RGB'
    render.image_settings.quality = 75
    render.use_file_extension = False
    render.resolution_x = RESOLUTION_X
    render.resolution_y = int(RESOLUTION_X * (bounds_size[1] / bounds_size[0]))
    render.resolution_percentage = int(100 * RESOLUTION_SCALE)

    bpy.context.view_layer.update()

    if OUTPUT_BLEND_FILE:
        bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=os.path.splitext(filepath)[0] + ".blend")

    if OUTPUT_IMAGE_FILE:
        print("Rendering:", filepath)
        bpy.ops.render.render(write_still=True)

# -----------------------------------------------------------------------------
# Validate Sections


def validate_sections():
    sections_shared = {section.source_code_base: [] for section in SECTIONS}

    for section in SECTIONS:
        if section.heading == "Operating system":
            continue
        sections_shared[section.source_code_base].append(section)

    # Consider the parent directories "documented".
    # Because their children are included.
    dirs_parent_documented = set()
    for dirpath in sections_shared.keys():
        if dirpath == "":
            continue
        if not dirpath.endswith("/"):
            raise Exception("Directories must end with a \"/\", found {:s}".format(dirpath))
        dirpath_split = dirpath.strip("/").split("/")
        for i in range(1, len(dirpath_split) + 1):
            dirpath = "/".join(dirpath_split[0:i])
            dirs_parent_documented.add(dirpath)

    this_platform = ""
    for e in os.scandir(os.path.join(ROOT_DIR, "lib")):
        dirpath_full = os.path.join(ROOT_DIR, "lib", e.name)
        if os.path.isdir(os.path.join(dirpath_full, "jpeg")):
            this_platform = e.name
            break

    dirs_fs_ignore = {
        ".git",
        ".github",
        ".gitea",
        ".well-known",
    }

    for source_code_base, sections in sections_shared.items():

        if "{platform}" in source_code_base:
            source_code_base = source_code_base.replace("{platform}", this_platform)

        dirs_fs = {
            e.name for e in os.scandir(os.path.join(ROOT_DIR, source_code_base))
            if e.is_dir()
        }

        for section in sections:
            for dir_docs, _ in section.source_code_dirs:
                if dir_docs in dirs_fs:
                    dirs_fs.remove(dir_docs)
                    continue

                print("Directory no longer exists:", os.path.join(source_code_base, dir_docs))

        for dir_fs in sorted(dirs_fs):
            if dir_fs in dirs_fs_ignore:
                continue
            if dir_fs in dirs_parent_documented:
                continue
            print("Directory has no docs:", os.path.join(source_code_base, dir_fs))


def argparse_create():
    import argparse

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "--only-validate",
        dest="only_validate",
        action='store_true',
        help="Validate the directory listing and exit.",
    )
    parser.add_argument(
        "--output",
        dest="output",
        default="code_layout.jpg",
        type=str,
        help="The output path to write the JPEG to.",
    )

    return parser


# -----------------------------------------------------------------------------
# Main

def main():
    args = argparse_create().parse_args((sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))

    # Always validate as it's cheap and notifies of incomplete docs.
    validate_sections()
    if args.only_validate:
        print("Validation complete, exiting!")
        return

    bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)

    # Setup materials.
    material = bpy.data.materials.new("Flat Black")
    MATERIAL_FROM_COLOR["black"] = material
    del material
    material = bpy.data.materials.new("Flat Grey")
    nodes = material.node_tree.nodes
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    output = nodes.new("ShaderNodeOutputMaterial")
    material.node_tree.links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    bsdf.inputs['Base Color'].default_value = (0.4, 0.4, 0.4, 1.0)
    MATERIAL_FROM_COLOR["grey"] = material
    del material

    # Setup fonts.
    VFONT_FROM_STYLE["default"] = bpy.data.fonts.load(FONT_FILE_DEFAULT)
    VFONT_FROM_STYLE["mono"] = bpy.data.fonts.load(FONT_FILE_MONO)

    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'

    # Without this, the whites are gray.
    scene.view_settings.view_transform = "Standard"

    sub_section_gap_y = 0.1

    setup_page()
    step_y = 0.0
    box, _ = draw_centered_title(scene, step_y, "Blender Code Layout")
    bounds_max_y = box.max_y

    step_y -= box.max_y - box.min_y

    # Draw the legends for each kind of arrow.
    box = draw_legend(
        scene,
        step_y=step_y,
        legend_data=ARROW_DOWN,
        text="Modules only call lower level code.",
    )
    step_y = box.min_y - 0.175
    box = draw_legend(
        scene,
        step_y=step_y,
        legend_data=ARROW_DOWN_AND_SIDEWAYS,
        text="Modules call each other and lower level code.",
    )
    step_y = box.min_y - (0.175 + 0.15)

    # Draw the sections.
    for section_data in SECTIONS:
        box = draw_sub_section(
            scene,
            step_y=step_y,
            section_data=section_data,
        )
        step_y = box.min_y - sub_section_gap_y

    # Setup Y resolutions.
    bounds = Box2D(min_x=-PAGE_WIDTH_HALF, max_x=PAGE_WIDTH_HALF, min_y=box.min_y, max_y=bounds_max_y)

    print(args.output)
    render_output(scene, bounds, args.output)


if __name__ == "__main__":
    main()
