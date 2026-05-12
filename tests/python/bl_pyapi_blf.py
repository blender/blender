# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Tests for the BLF (Blender Font) and VFont (3D Text) Python APIs.

BLF tests cover the buffer and GPU rendering pipelines (--mode=BUFFER, --mode=GPU).
VFont tests cover the 3D text object pipeline (--mode=VFONT).

Usage:
  ./blender.bin --background --factory-startup --python tests/python/bl_pyapi_blf.py -- --mode=BUFFER
  ./blender.bin --background --factory-startup --python tests/python/bl_pyapi_blf.py -- --mode=VFONT

To regenerate reference images:
  ./blender.bin ... -- --mode=ALL --generate

To generate an HTML report on failure:
  ./blender.bin ... -- --mode=ALL --show-html /tmp/blf_report.html

To run a single test class:
  ./blender.bin ... -- --mode=ALL -k TestCombiningKerning

Adding Tests
============

Adding texts involves adding a class with a list of "cases",
there are many examples in this file.

When adding the test case, getting the proper framing (offset & size) can be a chore.
To make this more straightforward - test cases may have prepare=True set,
so they can print out framing which can be used as input.

- Add a case with ``prepare=True`` and placeholder values::

  CaseVFont(
      name="my_test",
      text="Hello",
      size=(0, 0),
      expected_dimensions=(0.0, 0.0),
      prepare=True,
  )

   For generic tests, set ``prepare=True`` on the ``GenericParamsBuffer`` and/or ``GenericParamsVFont``.

- Run the test to get proposed framing values::

  ./blender.bin ... -- --mode=ALL -k my_test

  Output::

     prepare my_test.buffer:
         expected_dimensions=(164, 53),
         size=(184, 73),
         position_offset=(10, 0),
     prepare my_test.vfont:
         expected_dimensions=(2.012345, 0.567890),
         size=(164, 60),
         position_offset=(6.7, 10.6),

- Copy the values back, remove ``prepare=True``, and generate references::

  ./blender.bin ... -- --mode=ALL --generate -k my_test
"""
from __future__ import annotations

__all__ = (
    "main",
)

import argparse
import contextlib
import itertools
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile
import unicodedata
import unittest

from collections.abc import Callable, Iterator
from typing import ContextManager, NamedTuple, Self

import blf  # type: ignore[import-not-found]
import bpy  # type: ignore[import-not-found]
import gpu  # type: ignore[import-not-found]
import gpu.matrix  # type: ignore[import-not-found]
import gpu.state  # type: ignore[import-not-found]
import imbuf  # type: ignore[import-not-found]
import mathutils  # type: ignore[import-not-found]
from gpu_extras.batch import batch_for_shader  # type: ignore[import-not-found]

# GPU initialization is deferred to `main()` so --mode=BUFFER doesn't pay for it.
# When init fails and `WITHOUT_GPU` is set, GPU test methods skip cleanly.


# ------------------------------------------------------------------------------
# Constants


class GlobalsBuffer(NamedTuple):
    """Default constants for the BLF buffer/GPU pipeline."""
    size: int = 72
    # Set to e.g. 4 to scale up rendering for visual inspection.
    test_scale: int = 1


class GlobalsVFont(NamedTuple):
    """Default constants for the VFont (3D Text) pipeline."""
    size: float = 1.0
    line_spacing: float = 1.0
    character_spacing: float = 1.0
    # Render at 2x then scale down for basic anti-aliasing without heavy geometry.
    scale_oversample: int = 2


globals_buffer = GlobalsBuffer()
globals_vfont = GlobalsVFont()
del GlobalsBuffer, GlobalsVFont


SOURCE_DIR: str = os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..")))
TEST_DIR: str = os.path.join(SOURCE_DIR, "tests", "files", "blenfont")
RENDER_DIR_BUFFER: str = os.path.join(TEST_DIR, "buffer_renders")
RENDER_DIR_VFONT: str = os.path.join(TEST_DIR, "vfont_renders")
IDIFF_BIN: str = os.environ.get("IDIFF_BIN") or shutil.which("idiff") or ""

FONT_PATH_REGULAR: str = os.path.join(TEST_DIR, "noto_fonts", "NotoSans-Regular.otf")
FONT_PATH_ITALIC: str = os.path.join(TEST_DIR, "noto_fonts", "NotoSans-Italic.otf")
FONT_PATH_BOLD: str = os.path.join(TEST_DIR, "noto_fonts", "NotoSans-Bold.otf")
FONT_PATH_BOLD_ITALIC: str = os.path.join(TEST_DIR, "noto_fonts", "NotoSans-BoldItalic.otf")

# Per-side padding (in pixels) around content in the final cropped reference image.
PREPARE_MARGIN_PX: int = 10
# Per-side padding used in the over-sized scratch buffer during the prepare search.
# Larger than the final margin so content has room to extend in any direction without
# clipping, allowing accurate measurement of where it lands.
PREPARE_SEARCH_MARGIN_PX: int = 200
# VFont units to pixels (at 1x scale). VFont rendering uses this * scale_oversample.
VFONT_PPU: float = 72.0
# Buffer pixel format used by `render_text_buffer`: opaque-black RGBA.
BUFFER_BYTES_PER_PIXEL: int = 4
BUFFER_CLEAR_PIXEL: bytes = b"\x00\x00\x00\xff"

# ------------------------------------------------------------------------------
# Globals

USE_GENERATE_TEST_DATA: bool = False
# When set, verify each VFont-rendered image is correctly framed (no clipping,
# no excessive margin). Enable via: `USE_TEST_BOUNDS_ERROR=1`.
USE_TEST_BOUNDS_ERROR: bool = bool(os.environ.get("USE_TEST_BOUNDS_ERROR"))
# Force prepare mode on every case, overriding per-case `prepare=True/False`.
# Useful after changing fonts to re-derive framing values for all tests.
USE_PREPARE_FORCE: bool = False

# When True, VFont images are rendered via Cycles with an orthographic camera instead of the GPU triangle path.
# Avoids `gpu.init()` so tests can run on headless build-bots (but is much slower).
USE_VFONT_RENDER: bool = False
VFONT_RENDER_DIR: str = ""

IDIFF_FAIL: float = 0.004
IDIFF_FAIL_PERCENT: float = 0.0

SHOW_HTML: str = ""
COMPARE_IMAGES: list["ComparedImage"] = []
OUTPUT_DIR: str = ""


class VFontSet(NamedTuple):
    """The font variants used by VFont text objects."""
    regular: bpy.types.VectorFont
    bold: bpy.types.VectorFont
    italic: bpy.types.VectorFont
    bold_italic: bpy.types.VectorFont


FONTS_VFONT: VFontSet | None = None


class VFontRenderContext:
    """
    Persistent Cycles render environment for ``USE_VFONT_RENDER``.

    Amortises the per-render overhead that is constant across all tests:
    scene-property saves/restores, camera creation/destruction, and hiding the
    pre-existing scene objects.  Created once before ``unittest.main()`` runs
    (alongside ``FONTS_VFONT``) and torn down in the same ``finally`` block.

    Each call to ``render_text_vfont_camera`` only needs to:
      - update the camera ``ortho_scale`` and ``location`` for the current size,
      - create/destroy the per-case text object and materials,
      - call ``bpy.ops.render.render(write_still=True)``.
    """

    def __init__(self) -> None:
        scene = bpy.context.scene
        self._scene = scene

        self._prev_engine = scene.render.engine
        scene.render.engine = 'CYCLES'

        changes = [
            (scene.render, "film_transparent", True),
            (scene.render.image_settings, "file_format", 'PNG'),
            (scene.render.image_settings, "color_mode", 'RGB'),
            (scene.render.image_settings, "color_depth", '8'),
            (scene.cycles, "samples", 1),
            (scene.cycles, "use_adaptive_sampling", False),
            (scene.cycles, "use_denoising", False),
            (scene.cycles, "max_bounces", 0),
            (scene.view_settings, "view_transform", 'Raw'),
            (scene.world.cycles, "sampling_method", 'NONE'),
            (scene.render, "dither_intensity", 0.0),
        ]
        self._prev = [(obj, attr, getattr(obj, attr)) for obj, attr, _ in changes]
        for obj, attr, val in changes:
            setattr(obj, attr, val)

        # Black world background - save/restore node value directly.
        world_bg = scene.world.node_tree.nodes["Background"]
        self._prev_world_color = tuple(world_bg.inputs[0].default_value)
        self._prev_world_strength = float(world_bg.inputs[1].default_value)
        world_bg.inputs[0].default_value = (0.0, 0.0, 0.0, 1.0)
        world_bg.inputs[1].default_value = 0.0

        self._prev_hide_render: dict[str, bool] = {
            obj.name: obj.hide_render for obj in scene.objects
        }
        for obj in scene.objects:
            obj.hide_render = True
        assert all(obj.hide_render for obj in scene.objects), "Scene has visible objects after hide"

        # Persistent orthographic camera - only ortho_scale and location change per render.
        cam_data = bpy.data.cameras.new("_test_cam")
        cam_data.type = 'ORTHO'
        cam_data.sensor_fit = 'HORIZONTAL'
        self.cam_data: bpy.types.Camera = cam_data
        self.cam_obj = bpy.data.objects.new("_test_cam_obj", cam_data)
        scene.collection.objects.link(self.cam_obj)

        self.tmp_path = os.path.join(VFONT_RENDER_DIR, "render.png")
        for obj, attr, val in [
            (scene, "camera", self.cam_obj),
            (scene.render, "filepath", self.tmp_path),
        ]:
            self._prev.append((obj, attr, getattr(obj, attr)))
            setattr(obj, attr, val)

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_: object) -> None:
        self.teardown()

    def teardown(self) -> None:
        """Destroy the camera and restore every saved scene property."""
        scene = self._scene

        bpy.data.objects.remove(self.cam_obj)
        bpy.data.cameras.remove(self.cam_data)

        self._scene.render.engine = self._prev_engine
        for obj, attr, val in self._prev:
            setattr(obj, attr, val)

        world_bg = self._scene.world.node_tree.nodes["Background"]
        world_bg.inputs[0].default_value = self._prev_world_color
        world_bg.inputs[1].default_value = self._prev_world_strength

        for obj_name, was_hidden in self._prev_hide_render.items():
            if obj_name in scene.objects:
                scene.objects[obj_name].hide_render = was_hidden


VFONT_RENDER_CTX: VFontRenderContext | None = None


# Test classes are registered here by TestImageComparison_MixIn.__init_subclass__
# and have their per-case test methods attached later by attach_test_methods(),
# once main() has parsed --mode. This avoids needing to know the mode at import
# time (when subclasses are defined).
TEST_CLASSES: list[type["TestImageComparison_MixIn"]] = []

# Per-mode set of "kinds" of test method to generate. The BUFFER kind also runs
# the dimensions check inline (as part of `_run_render_case`) since dimensions
# don't depend on the rendering pipeline and BUFFER is the reference-generating
# mode. CMake registers one test per mode (BUFFER, GPU, VFONT); ALL is a
# manual-run convenience that runs every kind in a single invocation.
MODE_KINDS: dict[str, tuple[str, ...]] = {
    'BUFFER': ("buffer",),
    'GPU': ("gpu",),
    'VFONT': ("vfont",),
    'ALL': ("buffer", "gpu", "vfont"),
}


def attach_test_methods(mode: str) -> None:
    """
    Generate test_<kind>_<case> methods on each registered test class.

    Called from main() after --mode is parsed. For each kind in the active mode,
    walks the matching case list on the class (``cases_buffer`` for buffer/gpu,
    ``cases_vfont`` for vfont) and skips kinds with no cases.
    """
    def make_runner(
            case: CaseBuffer | CaseVFont,
            kind: str,
    ) -> Callable[[TestImageComparison_MixIn], None]:
        """Build a unittest test method that runs ``case`` for the given kind."""
        if kind == "vfont":
            def runner(self: TestImageComparison_MixIn) -> None:
                self._run_case_vfont(case)  # type: ignore[arg-type]
        else:
            def runner(self: TestImageComparison_MixIn) -> None:
                self._run_render_case(case, kind)  # type: ignore[arg-type]
        return runner

    kinds = MODE_KINDS[mode]
    for cls in TEST_CLASSES:
        cases_buffer = cls.__dict__.get("cases_buffer", ())
        cases_vfont = cls.__dict__.get("cases_vfont", ())
        for kind in kinds:
            cases = cases_vfont if kind == "vfont" else cases_buffer
            for case in cases:
                method_name = "test_{:s}_{:s}".format(kind, case.name)
                if hasattr(cls, method_name):
                    raise TypeError(
                        "{:s}: generated test method '{:s}' collides with existing attribute".format(
                            cls.__name__, method_name,
                        )
                    )
                runner = make_runner(case, kind)
                runner.__name__ = method_name
                runner.__qualname__ = "{:s}.{:s}".format(cls.__name__, method_name)
                runner.__doc__ = "{:s} render of {:s}{:s}".format(kind, cls.name_prefix, case.name)
                setattr(cls, method_name, runner)


# ------------------------------------------------------------------------------
# Types

# Collected data used to populate the HTML.
class ComparedImage(NamedTuple):
    # Rendering kind: "buffer", "gpu", or "vfont". Each kind gets its own section in the HTML report.
    kind: str
    #: Test class name, used to group entries in the HTML report.
    group: str
    # Name, used for the `subTest` & output filename.
    name: str
    # The text being tested.
    text: str
    # "Golden" reference image, if we differ from this - the test fails.
    ref_path: str
    # The generated output to test against.
    test_path: str
    # Difference image from idiff (empty string if not generated).
    diff_path: str
    passed: bool
    # Text output from `idiff`.
    idiff_output: str


# Data needed for a render test.
# Large width sentinel for tests that want WORD_WRAP enabled (so newlines are honored as line
# breaks) but with no realistic chance of a width-based wrap occurring.
WRAP_WIDTH_LARGE: int = 99999


class CaseBuffer(NamedTuple):
    name: str
    text: str
    size: tuple[int, int]
    expected_dimensions: tuple[int, int]
    position_offset: tuple[int, int]
    font_size: int = globals_buffer.size
    # BLF word-wrap width.
    # - `-1` (the default) leaves WORD_WRAP disabled.
    # - `0` and positive : values are passed through to `blf.word_wrap`;
    # - use `WRAP_WIDTH_LARGE` to enable WORD_WRAP without a meaningful width-based break.
    wrap_width: int = -1
    # When True, skip validation and print proposed values for size,
    # expected_dimensions, and position_offset.
    prepare: bool = False


class StyleSpan(NamedTuple):
    bold: bool
    italic: bool
    underline: bool
    smallcaps: bool
    material: int
    kerning: float


class TextFormatCompose:
    """
    Builder for styled text, records per-character formatting for VFont.

    Example::

        TextFormatCompose().style(bold=True).text("hello").style(bold=False).text(" world")
    """

    def __init__(self) -> None:
        self._body: list[str] = []
        self._spans: list[StyleSpan] = []
        self._bold: bool = False
        self._italic: bool = False
        self._underline: bool = False
        self._smallcaps: bool = False
        self._material: int = 0
        self._kerning: float = 0.0

    def style(
            self, *,
            bold: bool | None = None,
            italic: bool | None = None,
            underline: bool | None = None,
            smallcaps: bool | None = None,
            material: int | None = None,
            kerning: float | None = None,
    ) -> TextFormatCompose:
        if bold is not None:
            self._bold = bold
        if italic is not None:
            self._italic = italic
        if underline is not None:
            self._underline = underline
        if smallcaps is not None:
            self._smallcaps = smallcaps
        if material is not None:
            self._material = material
        if kerning is not None:
            self._kerning = kerning
        return self

    def text(self, value: str) -> TextFormatCompose:
        span = StyleSpan(
            self._bold, self._italic, self._underline,
            self._smallcaps, self._material, self._kerning,
        )
        for ch in value:
            self._body.append(ch)
            self._spans.append(span)
        return self

    @property
    def body(self) -> str:
        return "".join(self._body)

    @property
    def spans(self) -> list[StyleSpan]:
        return list(self._spans)


class TextBox(NamedTuple):
    """
    Mirror of :class:`bpy.types.TextBox` (every field a VFont text box exposes).

    Coordinates are in font units; ``y`` and ``height`` follow vfont's y-up convention,
    so a width-only box at the curve origin is ``TextBox(x=0, y=0, width=W, height=0)``.
    """
    x: float = 0.0
    y: float = 0.0
    width: float = 0.0
    height: float = 0.0


class CaseVFont(NamedTuple):
    name: str
    text: str | TextFormatCompose
    size: tuple[int, int]
    expected_dimensions: tuple[float, float]
    position_offset: tuple[float, float] = (0.0, 0.0)
    font_size: float = globals_vfont.size
    text_boxes: list[TextBox] | None = None
    overflow: str = 'NONE'
    align_x: str = 'LEFT'
    align_y: str = 'TOP_BASELINE'
    space_word: float = 1.0
    space_line: float = 1.0
    space_character: float = 1.0
    geometry_offset: float = 0.0
    geometry_extrude: float = 0.0
    materials: list[tuple[float, float, float]] | None = None
    # Optional context-manager factory invoked after the text Object has been created
    # and before the depsgraph evaluates it. Receives the Object as its only argument
    # and must return a context manager: setup runs in __enter__, teardown in __exit__.
    with_context_fn: Callable[[bpy.types.Object], ContextManager[None]] | None = None
    # curve.resolution_u -- subdivision resolution of each glyph outline.
    resolution: int = 4
    # When True, skip validation and print proposed values for size,
    # expected_dimensions, and position_offset.
    prepare: bool = False


class GenericParamsBuffer(NamedTuple):
    """Per-pipeline parameters for the BLF buffer/gpu renderer."""
    size: tuple[int, int]
    expected_dimensions: tuple[int, int]
    position_offset: tuple[int, int] = (10, 0)
    font_size: int = globals_buffer.size
    wrap_width: int = -1
    prepare: bool = False


class GenericParamsVFont(NamedTuple):
    """Per-pipeline parameters for the VFont (3D Text) renderer."""
    size: tuple[int, int]
    expected_dimensions: tuple[float, float]
    position_offset: tuple[float, float] = (0.0, 0.0)
    font_size: float = globals_vfont.size
    prepare: bool = False


class CaseGeneric(NamedTuple):
    """
    Shared test case that produces both a CaseBuffer and a CaseVFont.

    Holds the test text once, plus per-pipeline params.
    Use ``case_generic_to_buffer`` / ``case_generic_to_vfont`` to convert.
    """
    name: str
    text: str
    buffer: GenericParamsBuffer
    vfont: GenericParamsVFont


def case_generic_to_buffer(cases: list[CaseGeneric]) -> list[CaseBuffer]:
    """Convert a list of CaseGeneric to CaseBuffer."""
    return [CaseBuffer(name=c.name, text=c.text, **c.buffer._asdict()) for c in cases]


def case_generic_to_vfont(cases: list[CaseGeneric]) -> list[CaseVFont]:
    """Convert a list of CaseGeneric to CaseVFont."""
    return [CaseVFont(name=c.name, text=c.text, **c.vfont._asdict()) for c in cases]


# ------------------------------------------------------------------------------
# Internal Utilities

def generate_lorem_combining(word_count: int, seed: int) -> str:
    """Generate deterministic lorem ipsum text with combining characters."""
    words = [
        "lo\u0300rem", "i\u0301psum", "dolo\u0301r", "si\u0300t", "a\u0300met",
        "conse\u0301ctetur", "adipi\u0300scing", "e\u0300lit", "se\u0301d",
        "ei\u0300usmod", "tempo\u0301r", "incidi\u0301dunt", "labo\u0300re",
        "dolo\u0300re", "ma\u0301gna", "ali\u0300qua",
    ]
    rng = random.Random(seed)
    return " ".join(rng.choice(words) for _ in range(word_count))


def generate_stress_text(word_size: int = 8) -> str:
    """Generate a stress-test string from various Unicode blocks, grouped into words."""
    codepoints: list[int] = []

    # Latin block: U+0001 - U+00FF (skip null terminator).
    codepoints.extend(range(0x01, 0x100))

    # Combining Diacriticals: U+0300 - U+036F.
    codepoints.extend(range(0x0300, 0x0370))

    # Number Forms / Roman Numerals: U+2150 - U+218F.
    codepoints.extend(range(0x2150, 0x2190))

    # CJK Unified Ideographs (small sample): U+4E00 - U+4E20.
    codepoints.extend(range(0x4E00, 0x4E20))

    # Halfwidth / Fullwidth Forms (sample): U+FF01 - U+FF20.
    codepoints.extend(range(0xFF01, 0xFF20))

    # Noncharacters: U+FDD0 - U+FDEF.
    codepoints.extend(range(0xFDD0, 0xFDF0))

    # Supplementary plane (4-byte UTF-8): Emoji U+1F600 - U+1F610.
    codepoints.extend(range(0x1F600, 0x1F610))

    # Mathematical Alphanumeric (sample): U+1D400 - U+1D410.
    codepoints.extend(range(0x1D400, 0x1D410))

    # Build words of `word_size` characters separated by spaces.
    words = []
    for i in range(0, len(codepoints), word_size):
        word = "".join(chr(c) for c in codepoints[i:i + word_size])
        words.append(word)
    return " ".join(words)


# Unicode canonical combining classes (no named constants in Python's `unicodedata`).
COMBINING_CLASS_BELOW: int = 220
COMBINING_CLASS_ABOVE: int = 230


def generate_alphabet_combiners(combining_classes: set[int], *, line_count: int) -> str:
    """Generate ``AaBbCc...Zz`` cycling through combiners of the given classes.

    Each upper-lower pair uses the next combiner, wrapping around.
    The full cycle is repeated over ``line_count`` lines.
    """
    combiners = [
        chr(c) for c in range(0x0300, 0x0370)
        if unicodedata.combining(chr(c)) in combining_classes
    ]
    lines = []
    for line in range(line_count):
        pairs = []
        for i in range(26):
            cm = combiners[(line * 26 + i) % len(combiners)]
            pairs.append(chr(ord('A') + i) + cm + chr(ord('a') + i) + cm)
        lines.append("".join(pairs))
    return "\n".join(lines)


def generate_alphabet_above_and_below_combiners(*, line_count: int) -> str:
    """Generate ``AaBbCc...Zz`` with above combiners then below combiners."""
    above = generate_alphabet_combiners({COMBINING_CLASS_ABOVE}, line_count=line_count)
    below = generate_alphabet_combiners({COMBINING_CLASS_BELOW}, line_count=line_count)
    return above + "\n\n" + below


def generate_combining_stress(base: str, marks: str, count: int) -> str:
    """Generate a string with ``count`` combining marks stacked on ``base``."""
    return base + (marks * count)


def load_font_blf() -> int:
    """Load the test font into BLF and return the font handle."""
    font_id: int = blf.load(FONT_PATH_REGULAR)
    if font_id < 0:
        raise RuntimeError("Could not load font: {:s}".format(FONT_PATH_REGULAR))
    blf.size(font_id, globals_buffer.size)
    blf.enable(font_id, blf.NO_FALLBACK)
    return font_id


def imbuf_rgb_new(size: tuple[int, int]) -> imbuf.types.ImBuf:
    """
    Allocate a fresh RGB-only PNG imbuf with a BYTE pixel buffer.

    ``planes=24`` makes the saved PNG RGB-only (no alpha channel), which keeps
    the reference images as straight RGB and lets idiff produce visible diffs.
    """
    ibuf = imbuf.new(size, planes=24)
    ibuf.file_type = "PNG"
    ibuf.compress = 100
    ibuf.ensure_buffer("BYTE")
    return ibuf


@contextlib.contextmanager
def _blf_wrap_width(font_id: int, wrap_width: int) -> Iterator[None]:
    """
    Enable BLF WORD_WRAP for the body of the context, restore on exit.

    A negative ``wrap_width`` is a sentinel meaning "leave WORD_WRAP untouched".
    The try/finally ensures WORD_WRAP is disabled even if the body raises.
    """
    if wrap_width < 0:
        yield
        return
    blf.enable(font_id, blf.WORD_WRAP)
    blf.word_wrap(font_id, wrap_width)
    try:
        yield
    finally:
        blf.disable(font_id, blf.WORD_WRAP)


def render_text_buffer(font_id: int, case: CaseBuffer) -> imbuf.types.ImBuf:
    """Render text into an imbuf via BLF buffer drawing."""
    blf.size(font_id, case.font_size)
    ibuf = imbuf_rgb_new(case.size)
    # Opaque-black background. The internal buffer is RGBA; alpha must be 1.0
    # so BLF's draw_buffer composites text onto opaque pixels (otherwise
    # blending against alpha=0 produces darker text).
    with ibuf.with_buffer("BYTE", write=True) as buf:
        buf[:] = BUFFER_CLEAR_PIXEL * (case.size[0] * case.size[1])
    blf.color(font_id, 1.0, 1.0, 1.0, 1.0)
    with _blf_wrap_width(font_id, case.wrap_width), blf.bind_imbuf(font_id, ibuf, display_name="sRGB"):
        blf.position(
            font_id,
            case.position_offset[0],
            (case.size[1] - case.font_size) + case.position_offset[1],
            0,
        )
        blf.draw_buffer(font_id, case.text)
    return ibuf


def render_text_gpu(font_id: int, case: CaseBuffer) -> imbuf.types.ImBuf:
    """Render text via the GPU draw path into an imbuf."""
    blf.size(font_id, case.font_size)
    blf.color(font_id, 1.0, 1.0, 1.0, 1.0)

    w, h = case.size
    offscreen = gpu.types.GPUOffScreen(w, h)
    with _blf_wrap_width(font_id, case.wrap_width), offscreen.bind():
        fb = gpu.state.active_framebuffer_get()
        fb.clear(color=(0.0, 0.0, 0.0, 1.0))
        gpu.state.blend_set("ALPHA")
        gpu.state.viewport_set(0, 0, w, h)
        gpu.matrix.load_matrix(mathutils.Matrix.Identity(4))
        gpu.matrix.load_projection_matrix(
            mathutils.Matrix(
                ((2 / w, 0, 0, -1),
                 (0, 2 / h, 0, -1),
                 (0, 0, -1, 0),
                 (0, 0, 0, 1)),
            )
        )
        blf.position(
            font_id,
            case.position_offset[0],
            (h - case.font_size) + case.position_offset[1],
            0,
        )
        blf.draw(font_id, case.text)
        gpu.state.blend_set("NONE")

    pixel_buf = offscreen.texture_color.read()
    offscreen.free()

    ibuf = imbuf_rgb_new(case.size)
    # The internal buffer is still RGBA; force alpha to opaque so imbuf.write
    # doesn't treat anti-aliased GPU pixels as partially-transparent and
    # composite/discard them on save.
    gpu_bytes = bytearray(pixel_buf)
    gpu_bytes[3::4] = b"\xff" * (w * h)
    with ibuf.with_buffer("BYTE", write=True) as pixels:
        pixels[:] = gpu_bytes
    return ibuf


# ---------------------------------------------------------------------------
# VFont utilities

PHONETIC_ALPHABET: dict[str, str] = {
    "a": "alpha", "b": "bravo", "c": "charlie", "d": "delta",
    "e": "echo", "f": "foxtrot", "g": "golf", "h": "hotel",
    "i": "india", "j": "juliet", "k": "kilo", "l": "lima",
    "m": "mike", "n": "november", "o": "oscar", "p": "papa",
    "q": "quebec", "r": "romeo", "s": "sierra", "t": "tango",
    "u": "uniform", "v": "victor", "w": "whiskey", "x": "x-ray",
    "y": "yankee", "z": "zulu",
}

PHONETIC_WORDS: list[str] = list(PHONETIC_ALPHABET.values())


def generate_phonetic_alphabet(
        *,
        letters: tuple[str, str] = ("a", "z"),
        titlecase: bool = True,
) -> list[str]:
    """Return NATO phonetic alphabet words for a range of letters."""
    start = ord(letters[0].lower())
    end = ord(letters[1].lower())
    words = [PHONETIC_ALPHABET[chr(c)] for c in range(start, end + 1)]
    if titlecase:
        words = [w.title() for w in words]
    return words


def generate_random_words(word_count: int, seed: int, *, titlecase: bool = True) -> list[str]:
    """
    Return *word_count* pseudo-random NATO phonetic words from *seed*.
    """
    # Uses a simple generator so the output is deterministic and does not depend on `random`.
    state = seed & 0xFFFFFFFF
    words: list[str] = []
    for _ in range(word_count):
        state = (state * 1103515245 + 12345) & 0xFFFFFFFF
        words.append(PHONETIC_WORDS[state % len(PHONETIC_WORDS)])
    if titlecase:
        words = [w.title() for w in words]
    return words


def vfont_create_text_object_from_case(
        case: CaseVFont,
        fonts: VFontSet,
        collection: bpy.types.Collection,
        default_material: tuple[float, float, float] | None = None,
) -> tuple[bpy.types.Curve, bpy.types.Object, list[bpy.types.Material]]:
    """Create a FONT curve object from a test case. Caller must remove on cleanup."""
    body = case.text.body if isinstance(case.text, TextFormatCompose) else case.text

    curve = bpy.data.curves.new(name="_test_text", type='FONT')
    curve.body = body
    curve.size = case.font_size
    curve.font = fonts.regular
    curve.font_bold = fonts.bold
    curve.font_italic = fonts.italic
    curve.font_bold_italic = fonts.bold_italic
    curve.fill_mode = 'BOTH'
    curve.resolution_u = case.resolution
    curve.overflow = case.overflow
    curve.align_x = case.align_x
    curve.align_y = case.align_y
    curve.space_word = case.space_word
    curve.space_line = case.space_line
    curve.space_character = case.space_character
    curve.offset = case.geometry_offset
    curve.extrude = case.geometry_extrude

    materials_rgb = case.materials if case.materials is not None else (
        [default_material] if default_material is not None else []
    )
    created_materials: list[bpy.types.Material] = []
    for i, (r, g, b) in enumerate(materials_rgb):
        mat = bpy.data.materials.new(name="_test_mat_{:d}".format(i))
        nodes = mat.node_tree.nodes
        nodes.clear()
        emission = nodes.new('ShaderNodeEmission')
        emission.inputs["Color"].default_value = (r, g, b, 1.0)
        emission.inputs["Strength"].default_value = 1.0
        out = nodes.new('ShaderNodeOutputMaterial')
        mat.node_tree.links.new(emission.outputs["Emission"], out.inputs["Surface"])
        curve.materials.append(mat)
        created_materials.append(mat)

    obj = bpy.data.objects.new(name="_test_text_obj", object_data=curve)
    collection.objects.link(obj)

    if isinstance(case.text, TextFormatCompose):
        bpy.context.view_layer.objects.active = obj
        bpy.context.view_layer.update()
        for i, span in enumerate(case.text.spans):
            ci = curve.body_format[i]
            ci.use_bold = span.bold
            ci.use_italic = span.italic
            ci.use_underline = span.underline
            ci.use_small_caps = span.smallcaps
            ci.material_index = span.material
            ci.kerning = span.kerning

    if case.text_boxes is not None:
        bpy.context.view_layer.objects.active = obj
        for i, tb_data in enumerate(case.text_boxes):
            if i >= len(curve.text_boxes):
                bpy.ops.font.textbox_add()
            tb = curve.text_boxes[i]
            tb.x = tb_data.x
            tb.y = tb_data.y
            tb.width = tb_data.width
            tb.height = tb_data.height

    return curve, obj, created_materials


def vfont_to_triangles(
        case: CaseVFont,
        fonts: VFontSet,
) -> tuple[
    list[tuple[float, float]],
    list[tuple[int, int, int]],
    list[int],
]:
    """
    Build a VFont text object from the given case, convert to mesh, and extract 2D triangles.

    Returns ``(vertices, indices, material_indices)`` where vertices are ``(x, y)`` pairs,
    indices are triangle index triples, and material_indices is a per-triangle material slot.
    """
    curve, obj, created_materials = vfont_create_text_object_from_case(
        case, fonts, bpy.context.collection,
    )
    cm = case.with_context_fn(obj) if case.with_context_fn is not None else contextlib.nullcontext()
    try:
        with cm:
            bpy.context.view_layer.update()

            depsgraph = bpy.context.evaluated_depsgraph_get()
            eval_obj = obj.evaluated_get(depsgraph)
            mesh = eval_obj.to_mesh()
            mesh.calc_loop_triangles()

            verts = [(v.co.x, v.co.y) for v in mesh.vertices]
            tris = [(lt.vertices[0], lt.vertices[1], lt.vertices[2]) for lt in mesh.loop_triangles]
            tri_materials = [lt.material_index for lt in mesh.loop_triangles]

            eval_obj.to_mesh_clear()
    finally:
        bpy.data.objects.remove(obj)
        bpy.data.curves.remove(curve)
        for mat in created_materials:
            bpy.data.materials.remove(mat)

    return verts, tris, tri_materials


def vfont_dimensions_from_verts(
        verts: list[tuple[float, float]],
) -> tuple[float, float]:
    """Return the (width, height) from pre-computed triangle vertices."""
    if not verts:
        return (0.0, 0.0)
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    return (max(xs) - min(xs), max(ys) - min(ys))


def check_image_bounds(ibuf: imbuf.types.ImBuf, name: str) -> str | None:
    """
    Check that the image is correctly framed.

    Returns an error message string, or ``None`` if the image is OK.
    """
    w, h = ibuf.size
    with ibuf.with_buffer("BYTE") as pixels:
        data = bytes(pixels)

    def is_black(x: int, y: int) -> bool:
        off = (y * w + x) * 4
        return bool(data[off] == 0 and data[off + 1] == 0 and data[off + 2] == 0)

    for x in range(w):
        if not is_black(x, 0):
            return "{:s}: non-black pixel at min_y edge (x={:d}), image=({:d}x{:d})".format(name, x, w, h)
        if not is_black(x, h - 1):
            return "{:s}: non-black pixel at max_y edge (x={:d}), image=({:d}x{:d})".format(name, x, w, h)
    for y in range(h):
        if not is_black(0, y):
            return "{:s}: non-black pixel at min_x edge (y={:d}), image=({:d}x{:d})".format(name, y, w, h)
        if not is_black(w - 1, y):
            return "{:s}: non-black pixel at max_x edge (y={:d}), image=({:d}x{:d})".format(name, y, w, h)

    margin_limit = 13
    margin_min_y = 0
    for y in range(h):
        if all(is_black(x, y) for x in range(w)):
            margin_min_y += 1
        else:
            break
    margin_max_y = 0
    for y in range(h - 1, -1, -1):
        if all(is_black(x, y) for x in range(w)):
            margin_max_y += 1
        else:
            break
    margin_min_x = 0
    for x in range(w):
        if all(is_black(x, y) for y in range(h)):
            margin_min_x += 1
        else:
            break
    margin_max_x = 0
    for x in range(w - 1, -1, -1):
        if all(is_black(x, y) for y in range(h)):
            margin_max_x += 1
        else:
            break

    for edge, margin in (
            ("min_x", margin_min_x),
            ("max_x", margin_max_x),
            ("min_y", margin_min_y),
            ("max_y", margin_max_y),
    ):
        if margin >= margin_limit:
            return (
                "{:s}: excessive margin at {:s}, "
                "{:d} empty pixels (limit {:d}), "
                "margins=(min_x={:d}, max_x={:d}, min_y={:d}, max_y={:d}), "
                "image=({:d}x{:d})"
            ).format(
                name, edge, margin, margin_limit,
                margin_min_x, margin_max_x, margin_min_y, margin_max_y,
                w, h,
            )

    return None


def render_text_vfont(
        verts: list[tuple[float, float]],
        tris: list[tuple[int, int, int]],
        tri_materials: list[int],
        image_size: tuple[int, int],
        position_offset: tuple[float, float],
        materials: list[tuple[float, float, float]] | None = None,
) -> imbuf.types.ImBuf:
    """
    Render pre-computed VFont triangles into an imbuf via GPU.

    Renders at ``globals_vfont.scale_oversample`` times the output resolution then scales down
    for basic anti-aliasing without increasing curve tessellation.
    """
    w, h = image_size
    s = globals_vfont.scale_oversample
    sw, sh = w * s, h * s

    ppu = VFONT_PPU * s
    ox, oy = position_offset[0] * s, position_offset[1] * s
    pixel_verts = [(v[0] * ppu + ox, v[1] * ppu + oy) for v in verts]

    offscreen = gpu.types.GPUOffScreen(sw, sh)
    with offscreen.bind():
        fb = gpu.state.active_framebuffer_get()
        fb.clear(color=(0.0, 0.0, 0.0, 1.0))
        gpu.state.blend_set("ALPHA")
        gpu.state.viewport_set(0, 0, sw, sh)
        gpu.matrix.load_matrix(mathutils.Matrix.Identity(4))
        gpu.matrix.load_projection_matrix(
            mathutils.Matrix(
                ((2 / sw, 0, 0, -1),
                 (0, 2 / sh, 0, -1),
                 (0, 0, -1, 0),
                 (0, 0, 0, 1)),
            )
        )

        if pixel_verts and tris:
            shader = gpu.shader.from_builtin('UNIFORM_COLOR')
            if materials is not None:
                groups: dict[int, list[tuple[int, int, int]]] = {}
                for tri, mat_idx in zip(tris, tri_materials):
                    groups.setdefault(mat_idx, []).append(tri)
                for mat_idx, group_tris in groups.items():
                    r, g, b = materials[mat_idx] if mat_idx < len(materials) else (1.0, 1.0, 1.0)
                    batch = batch_for_shader(
                        shader, 'TRIS', {"pos": pixel_verts}, indices=group_tris,
                    )
                    shader.uniform_float("color", (r, g, b, 1.0))
                    batch.draw(shader)
            else:
                batch = batch_for_shader(shader, 'TRIS', {"pos": pixel_verts}, indices=tris)
                shader.uniform_float("color", (1.0, 1.0, 1.0, 1.0))
                batch.draw(shader)

        gpu.state.blend_set("NONE")

    pixel_buf = offscreen.texture_color.read()
    offscreen.free()

    ibuf_hi = imbuf_rgb_new((sw, sh))
    gpu_bytes = bytearray(pixel_buf)
    gpu_bytes[3::4] = b"\xff" * (sw * sh)
    with ibuf_hi.with_buffer("BYTE", write=True) as pixels:
        pixels[:] = gpu_bytes

    ibuf_hi.resize(image_size, method='BILINEAR')
    return ibuf_hi


def render_text_vfont_camera(
        case: CaseVFont,
        fonts: VFontSet,
        image_size: tuple[int, int],
        position_offset: tuple[float, float],
) -> imbuf.types.ImBuf:
    """
    Render a VFont text case using Cycles with an orthographic camera.

    Requires ``VFONT_RENDER_CTX`` to be initialized (done by ``main()`` before
    ``unittest.main()`` runs).  Session-level setup (scene properties, camera,
    hidden objects, temp file) lives in the context; this function only creates
    and destroys the per-case text object.

    Camera is orthographic, positioned so that VFont world-space coordinates map to
    pixels identically to the GPU triangle path::

       pixel_x = vertex_x * VFONT_PPU + position_offset[0]
       pixel_y = vertex_y * VFONT_PPU + position_offset[1]
    """
    ctx = VFONT_RENDER_CTX
    assert ctx is not None, "VFONT_RENDER_CTX must be initialised before rendering"

    w, h = image_size
    s = globals_vfont.scale_oversample
    sw, sh = w * s, h * s
    scene = ctx._scene

    # Render at 2x then bilinear-downscale: matches the supersampling in render_text_vfont.
    # Camera framing is in VFont world-space, independent of render resolution.
    scene.render.resolution_x = sw
    scene.render.resolution_y = sh
    scene.render.resolution_percentage = 100
    ctx.cam_data.ortho_scale = w / VFONT_PPU
    ctx.cam_obj.location = (
        (w / 2.0 - position_offset[0]) / VFONT_PPU,
        (h / 2.0 - position_offset[1]) / VFONT_PPU,
        10.0,
    )

    curve, text_obj, created_materials = vfont_create_text_object_from_case(
        case, fonts, scene.collection, default_material=(1.0, 1.0, 1.0),
    )
    cm = case.with_context_fn(text_obj) if case.with_context_fn is not None else contextlib.nullcontext()
    try:
        with cm:
            bpy.ops.render.render(write_still=True)
        ibuf = imbuf.load(ctx.tmp_path)
        ibuf.resize(image_size, method='BILINEAR')
    finally:
        bpy.data.objects.remove(text_obj)
        bpy.data.curves.remove(curve)
        for mat in created_materials:
            bpy.data.materials.remove(mat)

    return ibuf


# ---------------------------------------------------------------------------
# HTML report (only used with --show-html)

def write_html_report(html_path: str) -> None:
    """
    Write an HTML report showing all compared images with idiff results.

    With --mode=ALL the report contains one top-level section per kind
    (BUFFER / GPU / VFONT), each visually separated as if they were independent
    reports concatenated into a single document.
    """
    import base64
    from html import escape as html_escape

    def img_uri(path: str) -> str:
        if not os.path.exists(path):
            return ""
        with open(path, "rb") as fh_img:
            b64 = base64.b64encode(fh_img.read()).decode("ascii")
        return "data:image/png;base64,{:s}".format(b64)

    # Preserve insertion order: collect entries per kind in the order they were
    # produced, then walk kinds in that order so the report matches the test
    # execution order.
    by_kind: dict[str, list[ComparedImage]] = {}
    for entry in COMPARE_IMAGES:
        by_kind.setdefault(entry.kind, []).append(entry)

    count_failed_total = 0
    count_total = len(COMPARE_IMAGES)
    with open(html_path, "w") as fh:
        fh.write(
            "<html><head><meta charset='utf-8'>\n"
            "<title>BLF Test Report</title>\n"
            "</head><body bgcolor='#333' text='white'>\n"
            "<h1>BLF Test Report</h1>\n"
        )

        # Summary.
        fh.write("<ul>\n")
        for kind, entries in by_kind.items():
            count_kind = len(entries)
            count_passed = sum(1 for e in entries if e.passed)
            color = "green" if count_passed == count_kind else "red"
            fh.write(
                "<li><a href='#kind-{kind_lower:s}'>{kind:s}</a>"
                " - <font color='{color:s}'>{passed:d}/{total:d} passed</font></li>\n".format(
                    kind_lower=kind,
                    kind=kind.upper(),
                    color=color,
                    passed=count_passed,
                    total=count_kind,
                )
            )
        fh.write("</ul>\n")

        for kind_idx, (kind, entries) in enumerate(by_kind.items()):
            if kind_idx > 0:
                fh.write("<hr>\n")
            count_failed_kind = sum(1 for e in entries if not e.passed)
            count_kind = len(entries)
            fh.write(
                "<h1><a name='kind-{kind_lower:s}'>{kind:s}</a>"
                " ({passed:d}/{total:d} passed)</h1>\n".format(
                    kind_lower=kind,
                    kind=kind.upper(),
                    passed=count_kind - count_failed_kind,
                    total=count_kind,
                )
            )
            count_failed_total += count_failed_kind

            current_group = ""
            for entry in entries:
                if entry.group != current_group:
                    if current_group:
                        fh.write("</table>\n<hr>\n")
                    fh.write("<h2>{:s}</h2>\n<table>\n".format(entry.group))
                    current_group = entry.group
                if not entry.passed:
                    status = "<b><font color='red'>FAIL</font></b>"
                else:
                    status = "<b><font color='green'>PASS</font></b>"

                # Strip redundant lines from idiff output, keep only the stats.
                idiff_stats = "\n".join(
                    line for line in entry.idiff_output.splitlines()
                    if not (line in {"PASS", "FAILURE"} or line.startswith("Comparing "))
                )
                text_escaped = html_escape(entry.text).replace("\n", "<br>")
                diff_uri = img_uri(entry.diff_path) if entry.diff_path else ""
                diff_cell = (
                    "  <td><small>Difference (10x)</small><br><img src='{:s}'></td>\n".format(diff_uri)
                    if diff_uri else ""
                )
                fh.write(
                    "<tr><td colspan='3'><h3>{name:s} {status:s}</h3>"
                    "<table bgcolor='black' cellpadding='4'><tr><td><code>{text:s}</code></td></tr></table>"
                    "<pre>{output:s}</pre></td></tr>\n"
                    "<tr>\n"
                    "  <td><small>Before (reference)</small><br><img src='{ref:s}'></td>\n"
                    "  <td><small>After (test output)</small><br><img src='{test:s}'></td>\n"
                    "{diff:s}"
                    "</tr>\n".format(
                        name=html_escape(entry.name),
                        status=status,
                        text=text_escaped,
                        output=html_escape(idiff_stats),
                        ref=img_uri(entry.ref_path),
                        test=img_uri(entry.test_path),
                        diff=diff_cell,
                    )
                )
            if entries:
                fh.write("</table>\n")

        fh.write(
            "<hr class='section'>\n"
            "<p>{count_passed:d}/{count_total:d} passed (across all kinds)</p>\n"
            "</body></html>\n".format(
                count_passed=count_total - count_failed_total,
                count_total=count_total,
            )
        )
    print("HTML report: {:s}".format(html_path))


# ---------------------------------------------------------------------------
# Test MixIn

class TestImageComparison_MixIn:
    """
    Base class for tests that compare rendered images against references.

    Sub-classes must define ``name_prefix`` and at least one of ``cases_buffer``
    or ``cases_vfont`` class attributes, and inherit from both this class and
    :class:`unittest.TestCase`. ``cases_buffer`` drives the BLF buffer/gpu render
    pipeline; ``cases_vfont`` drives the bpy VectorFont (3D Text) pipeline. The
    same class can define both lists - case names live in separate namespaces
    per kind, so ``test_buffer_xv`` and ``test_vfont_xv`` coexist.

    For each case in ``cases_buffer`` test methods are attached after main()
    parses --mode by attach_test_methods(): ``test_buffer_<name>`` (which also
    runs the dimensions check) and ``test_gpu_<name>``. ``cases_vfont`` produces
    ``test_vfont_<name>``. They are selectable via the standard unittest CLI:

      ./blender.bin --background --factory-startup --python <test_file>.py -- TestX.test_buffer_ascii_sweep
      ./blender.bin ... -- -k ascii_sweep
    """

    # To be overridden.
    name_prefix: str = ""
    cases_buffer: list[CaseBuffer] = []
    cases_vfont: list[CaseVFont] = []

    def __init_subclass__(cls, **kwargs: object) -> None:
        super().__init_subclass__(**kwargs)
        has_buffer = "cases_buffer" in cls.__dict__
        has_vfont = "cases_vfont" in cls.__dict__
        if not (has_buffer or has_vfont):
            raise TypeError(
                "{:s} must define cases_buffer and/or cases_vfont".format(cls.__name__),
            )
        if "name_prefix" not in cls.__dict__:
            raise TypeError("{:s} must define 'name_prefix'".format(cls.__name__))
        # Catch typos like "combining_kerning" (no dot) - without the trailing
        # dot, generated filenames concatenate the case name without a separator.
        if not cls.name_prefix.endswith("."):
            raise TypeError(
                "{:s}.name_prefix {!r} must end with '.'".format(cls.__name__, cls.name_prefix),
            )
        TEST_CLASSES.append(cls)

    def setUp(self) -> None:
        assert isinstance(self, unittest.TestCase)
        self.font_id = load_font_blf()
        self.fonts_vfont = FONTS_VFONT

    @staticmethod
    def _scale_case(case: CaseBuffer) -> CaseBuffer:
        """Apply globals_buffer.test_scale to a case's pixel-dependent fields."""
        s = globals_buffer.test_scale
        return case._replace(
            size=(case.size[0] * s, case.size[1] * s),
            font_size=case.font_size * s,
            wrap_width=case.wrap_width * s if case.wrap_width >= 0 else -1,
            position_offset=(case.position_offset[0] * s, case.position_offset[1] * s),
        )

    # Map of kind -> render function. Looked up by _run_render_case.
    _RENDER_FUNCS = {
        "buffer": render_text_buffer,
        "gpu": render_text_gpu,
    }

    def _run_render_case(self, case: CaseBuffer, kind: str) -> None:
        """
        Render ``case`` via the kind's pipeline ('buffer' or 'gpu') and compare.

        For ``kind == "buffer"`` this also asserts ``blf.dimensions`` matches
        ``case.expected_dimensions``. The dimensions check is pipeline-independent
        so it doesn't run again for ``"gpu"``.
        """
        assert isinstance(self, unittest.TestCase)
        if case.prepare or USE_PREPARE_FORCE:
            if kind == "buffer":
                self._print_prepare_buffer(case)
            # GPU shares buffer references, so prepare only prints for buffer.
            return
        if kind == "gpu" and gpu is None:
            self.skipTest("GPU not available")
        if kind == "buffer":
            self._check_dimensions(case)
        case = self._scale_case(case)
        render_fn = self._RENDER_FUNCS[kind]
        ibuf = render_fn(self.font_id, case)
        self._compare_image(
            case.name,
            case.text,
            ibuf,
            kind,
            idiff_fail=IDIFF_FAIL,
            idiff_fail_percent=IDIFF_FAIL_PERCENT,
        )

    def _compute_prepare_buffer(
            self, case: CaseBuffer,
    ) -> tuple[tuple[int, int], tuple[int, int], tuple[int, int]]:
        """
        Derive ``(size, position_offset, expected_dimensions)`` for a buffer case.

        Renders into an over-sized scratch buffer (margin = ``PREPARE_SEARCH_MARGIN_PX``)
        so content can extend in any direction without clipping, then crops to the final
        margin (``PREPARE_MARGIN_PX``) and translates the offset accordingly.

        BLF buffer rendering interprets ``position.y`` differently depending on the
        text (single line vs wrapped); the default offset is tried first, then a
        search runs only if content was clipped at an edge.
        """
        assert isinstance(self, unittest.TestCase)
        blf.size(self.font_id, case.font_size)
        with _blf_wrap_width(self.font_id, case.wrap_width):
            w, h = blf.dimensions(self.font_id, case.text)
        dims = (math.ceil(w), math.ceil(h))

        search_margin_both = 2 * PREPARE_SEARCH_MARGIN_PX
        search_w = dims[0] + search_margin_both
        search_h = dims[1] + search_margin_both
        row_size = search_w * BUFFER_BYTES_PER_PIXEL
        blank_row = BUFFER_CLEAR_PIXEL * search_w

        def render_extent(offset: int) -> tuple[int, int, int]:
            """Render at the given offset, return ``(extent, first_row, last_row)``."""
            scratch_case = case._replace(
                size=(search_w, search_h),
                position_offset=(PREPARE_SEARCH_MARGIN_PX, offset),
            )
            ibuf = render_text_buffer(self.font_id, scratch_case)
            with ibuf.with_buffer("BYTE") as pixels:
                # Walk in from each edge; row equality is a single C-level compare.
                first_row = -1
                for y in range(search_h):
                    if pixels[y * row_size:(y + 1) * row_size] != blank_row:
                        first_row = y
                        break
                if first_row == -1:
                    return 0, -1, -1
                # `first_row` is non-blank, so stop one past it.
                for y in range(search_h - 1, first_row, -1):
                    if pixels[y * row_size:(y + 1) * row_size] != blank_row:
                        return (y - first_row) + 1, first_row, y
                # No non-blank row beyond `first_row` - content is exactly that one row.
                return 1, first_row, first_row

        # Thanks to the over-sized scratch buffer, the default offset usually works.
        # Only search if content was clipped at an edge.
        best_offset = 0
        best_extent, best_first_row, best_last_row = render_extent(0)
        if best_extent < dims[1]:
            # Aim for ~50 search points across content height, but never finer than 5px.
            search_steps_target = 50
            search_step_min_px = 5
            search_step = max(search_step_min_px, dims[1] // search_steps_target)
            # Search up to one content height (plus search margin) in either direction.
            # Clipped at top -> positive offset; clipped at bottom -> negative offset.
            search_reach = dims[1] + PREPARE_SEARCH_MARGIN_PX
            clipped_top = best_first_row == 0
            clipped_bottom = best_last_row == search_h - 1
            if clipped_top and not clipped_bottom:
                search_range: range | itertools.chain[int] = range(
                    search_step, search_reach + 1, search_step,
                )
            elif clipped_bottom and not clipped_top:
                search_range = range(-search_step, -search_reach - 1, -search_step)
            else:
                # Ambiguous (or no content) - search both directions.
                search_range = itertools.chain(
                    range(-search_step, -search_reach - 1, -search_step),
                    range(search_step, search_reach + 1, search_step),
                )
            for offset in search_range:
                extent, first_row, last_row = render_extent(offset)
                if extent > best_extent:
                    best_extent = extent
                    best_offset = offset
                    best_first_row = first_row
                    best_last_row = last_row
                    if best_extent >= dims[1]:
                        break

        # Crop to the final margin. The final buffer is just large enough to hold the
        # measured content extent, with `PREPARE_MARGIN_PX` padding on each side.
        # The offset translation preserves where content lands relative to the buffer:
        # shifting `position_y` by N shifts content rows by N (translation invariance).
        final_margin_both = 2 * PREPARE_MARGIN_PX
        final_size_w = dims[0] + final_margin_both
        final_size_h = best_extent + final_margin_both
        # search_position_y = (search_h - font_size) + best_offset
        # final_position_y = search_position_y + (PREPARE_MARGIN_PX - best_first_row)
        # final_offset = final_position_y - (final_size_h - font_size)
        final_offset = (
            (search_h - final_size_h)
            + best_offset
            + (PREPARE_MARGIN_PX - best_first_row)
        )

        return (final_size_w, final_size_h), (PREPARE_MARGIN_PX, final_offset), dims

    def _print_prepare_buffer(self, case: CaseBuffer) -> None:
        """Print proposed values for a buffer test case (used with ``prepare=True``)."""
        size, offset, dims = self._compute_prepare_buffer(case)
        print("  prepare {:s}.{:s}:".format(case.name, "buffer"))
        print("    expected_dimensions={!r},".format(dims))
        print("    size=({:d}, {:d}),".format(size[0], size[1]))
        print("    position_offset=({:d}, {:d}),".format(offset[0], offset[1]))

    def _check_dimensions(self, case: CaseBuffer) -> None:
        """
        Assert ``blf.dimensions(text)`` matches ``case.expected_dimensions``.

        Skipped during ``--generate`` so reference regeneration isn't blocked
        by stale expected_dimensions in the source.
        """
        assert isinstance(self, unittest.TestCase)
        if USE_GENERATE_TEST_DATA:
            return
        blf.size(self.font_id, case.font_size)
        with _blf_wrap_width(self.font_id, case.wrap_width):
            w, h = blf.dimensions(self.font_id, case.text)
        self.assertEqual(
            (int(w), int(h)), case.expected_dimensions,
            "Dimensions mismatch for {:s}: got ({:.0f}, {:.0f}) expected {!r}".format(
                case.name, w, h, case.expected_dimensions,
            ),
        )

    def assertAlmostEqualForEach(
            self,
            actual: tuple[float, ...],
            expected: tuple[float, ...],
            places: int = 4,
            msg: str | None = None,
    ) -> None:
        """Element-wise approximate equality for two same-length sequences."""
        assert isinstance(self, unittest.TestCase)
        for i, (a, e) in enumerate(zip(actual, expected, strict=True)):
            if round(abs(a - e), places) != 0:
                self.fail(
                    (msg + " " if msg else "")
                    + "Mismatch at index {:d} ({:.{p}f} vs {:.{p}f}) in {!r} vs {!r}".format(
                        i, a, e, actual, expected, p=places,
                    )
                )

    def _run_case_vfont(self, case: CaseVFont) -> None:
        """VFont pipeline: bpy curve -> triangles -> render. Bundles dims + render."""
        assert isinstance(self, unittest.TestCase)
        assert self.fonts_vfont is not None
        verts, tris, tri_materials = vfont_to_triangles(case, self.fonts_vfont)

        w, h = vfont_dimensions_from_verts(verts)

        if case.prepare or USE_PREPARE_FORCE:
            self._print_prepare_vfont(case, verts, w, h)
            return

        if not USE_GENERATE_TEST_DATA:
            self.assertAlmostEqualForEach(
                (w, h), case.expected_dimensions, places=4,
                msg="Dimensions mismatch for {:s}:".format(case.name),
            )

        if USE_VFONT_RENDER:
            ibuf = render_text_vfont_camera(case, self.fonts_vfont, case.size, case.position_offset)
        else:
            ibuf = render_text_vfont(verts, tris, tri_materials, case.size, case.position_offset, case.materials)

        if USE_TEST_BOUNDS_ERROR:
            bounds_err = check_image_bounds(ibuf, case.name)
            self.assertIsNone(bounds_err, bounds_err)

        text_display = case.text.body if isinstance(case.text, TextFormatCompose) else case.text
        # Camera render uses a slightly looser threshold: Cycles rasterizes glyph
        # boundaries differently from the GPU uniform_color shader, leaving O(1-5) pixels
        # per image just above 0.01. Allow up to 0.1 % of pixels to exceed the threshold.
        idiff_fail = 0.01 if USE_VFONT_RENDER else 0.004
        idiff_fail_percent = 0.11 if USE_VFONT_RENDER else 0.0
        self._compare_image(
            case.name, text_display, ibuf, "vfont",
            idiff_fail=idiff_fail, idiff_fail_percent=idiff_fail_percent,
        )

    @staticmethod
    def _compute_prepare_vfont(
            verts: list[tuple[float, float]],
            w: float, h: float,
    ) -> tuple[tuple[int, int], tuple[float, float], tuple[float, float]]:
        """Derive ``(size, position_offset, expected_dimensions)`` for a VFont case."""
        if verts:
            min_x = min(v[0] for v in verts)
            min_y = min(v[1] for v in verts)
            offset_x = -min_x * VFONT_PPU + PREPARE_MARGIN_PX
            offset_y = -min_y * VFONT_PPU + PREPARE_MARGIN_PX
            size_w = math.ceil(w * VFONT_PPU) + (2 * PREPARE_MARGIN_PX)
            size_h = math.ceil(h * VFONT_PPU) + (2 * PREPARE_MARGIN_PX)
        else:
            offset_x = offset_y = float(PREPARE_MARGIN_PX)
            size_w = size_h = 2 * PREPARE_MARGIN_PX
        return (size_w, size_h), (offset_x, offset_y), (w, h)

    @staticmethod
    def _print_prepare_vfont(
            case: CaseVFont,
            verts: list[tuple[float, float]],
            w: float, h: float,
    ) -> None:
        """Print proposed values for a VFont test case (used with ``prepare=True``)."""
        size, offset, dims = TestImageComparison_MixIn._compute_prepare_vfont(verts, w, h)
        print("  prepare {:s}.{:s}:".format(case.name, "vfont"))
        print("    expected_dimensions=({:.6f}, {:.6f}),".format(dims[0], dims[1]))
        print("    size=({:d}, {:d}),".format(size[0], size[1]))
        print("    position_offset=({:.1f}, {:.1f}),".format(offset[0], offset[1]))

    # Maps rendering kind to the directory holding reference images. Buffer and
    # GPU share a single set of references (GPU must match buffer output).
    _RENDER_DIRS: dict[str, str] = {
        "buffer": RENDER_DIR_BUFFER,
        "gpu": RENDER_DIR_BUFFER,
        "vfont": RENDER_DIR_VFONT,
    }

    def _compare_image(
            self, name: str, text: str, ibuf: imbuf.types.ImBuf, kind: str, *,
            idiff_fail: float,
            idiff_fail_percent: float,
    ) -> None:
        """
        Compare rendered image against a reference using idiff, or generate if --generate.

        ``kind`` is the rendering pipeline name ("buffer", "gpu", or "vfont"); it
        tags the ComparedImage entry for HTML grouping and disambiguates the on-disk
        ``_test.png`` filename so the two pipelines don't overwrite each other.
        The reference image has no kind suffix (one golden file per case).
        ``idiff_fail`` sets the per-channel failure threshold passed to idiff.
        ``idiff_fail_percent`` sets the allowable fraction of pixels (percent) that may
        exceed the threshold before the comparison fails.
        """
        assert isinstance(self, unittest.TestCase)
        name_full = self.name_prefix + name
        render_dir = self._RENDER_DIRS[kind]
        ref_path = os.path.join(render_dir, "{:s}.png".format(name_full))
        out_path = os.path.join(OUTPUT_DIR, "{:s}_{:s}_test.png".format(name_full, kind))

        if USE_GENERATE_TEST_DATA and kind != "gpu":
            # Reference images are generated from the buffer/vfont paths only.
            # GPU must match the same buffer reference - any difference is a bug.
            imbuf.write(ibuf, filepath=ref_path)
            print("  Generated: {:s}".format(ref_path))
            passed = True
            test_path = ref_path
            diff_path = ""
            idiff_output = "generated"
        else:
            self.assertTrue(os.path.exists(ref_path), "Reference image missing: {:s}".format(ref_path))
            imbuf.write(ibuf, filepath=out_path)
            self.assertTrue(IDIFF_BIN, "idiff not found, set IDIFF_BIN or install OpenImageIO")
            diff_path = os.path.join(OUTPUT_DIR, "{:s}_{:s}_diff.png".format(name_full, kind))
            result = subprocess.run(
                [IDIFF_BIN,
                 "-fail", "{:.4f}".format(idiff_fail),
                 "-failpercent", "{:.4f}".format(idiff_fail_percent),
                 "-o", diff_path, "-abs", "-scale", "10",
                 ref_path, out_path],
                capture_output=True,
                text=True,
            )
            # 0 = identical, 1 = within warn/failpercent tolerance, 2 = fail, 3+ = error.
            passed = result.returncode in {0, 1}
            test_path = out_path
            idiff_output = result.stdout.rstrip()

        COMPARE_IMAGES.append(ComparedImage(
            kind=kind,
            group=type(self).__name__,
            name="{:s}_{:s}".format(name_full, kind),
            text=text,
            ref_path=ref_path,
            test_path=test_path,
            diff_path=diff_path,
            passed=passed,
            idiff_output=idiff_output,
        ))

        if not passed:
            self.fail(
                "Image {:s}_{:s} differs from reference:\n{:s}".format(
                    name_full, kind, idiff_output,
                )
            )


# ---------------------------------------------------------------------------
# Generic Tests (both buffer & VFont)


class TestCombiningKerning(TestImageComparison_MixIn, unittest.TestCase):
    """Test that kerning is correct after combining characters."""

    name_prefix = "combining_kerning."
    _cases_generic = [
        CaseGeneric(
            name="Yolanda",
            text=unicodedata.normalize("NFD", "\u0178olanda"),
            buffer=GenericParamsBuffer(
                size=(293, 84),
                expected_dimensions=(273, 65),
            ),
            vfont=GenericParamsVFont(
                size=(204, 65),
                expected_dimensions=(2.543269, 0.623712),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="XV",
            text="X\u0308V",
            buffer=GenericParamsBuffer(
                size=(106, 83),
                expected_dimensions=(86, 64),
            ),
            vfont=GenericParamsVFont(
                size=(79, 65),
                expected_dimensions=(0.811813, 0.616844),
                position_offset=(9.8, 10.0),
            ),
        ),
        CaseGeneric(
            name="ro",
            text="r\u0308o",
            buffer=GenericParamsBuffer(
                size=(94, 73),
                expected_dimensions=(74, 53),
            ),
            vfont=GenericParamsVFont(
                size=(65, 57),
                expected_dimensions=(0.616758, 0.508328),
                position_offset=(6.7, 10.5),
            ),
        ),
        CaseGeneric(
            name="Avatar",
            text=unicodedata.normalize("NFD", "\u00C0vatar"),
            buffer=GenericParamsBuffer(
                size=(239, 89),
                expected_dimensions=(219, 69),
            ),
            vfont=GenericParamsVFont(
                size=(170, 69),
                expected_dimensions=(2.079670, 0.667668),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="Eve",
            text=unicodedata.normalize("NFD", "\u00C8ve"),
            buffer=GenericParamsBuffer(
                size=(138, 89),
                expected_dimensions=(118, 69),
            ),
            vfont=GenericParamsVFont(
                size=(94, 68),
                expected_dimensions=(1.016484, 0.665608),
                position_offset=(5.2, 10.5),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStrangeCharacters(TestImageComparison_MixIn, unittest.TestCase):
    """Test that control characters, zero-width characters, and edge cases don't crash."""

    name_prefix = "strange_characters."

    _cases_generic = [
        CaseGeneric(
            name="tab",
            text="A\tB",
            buffer=GenericParamsBuffer(
                size=(132, 72),
                expected_dimensions=(112, 52),
            ),
            vfont=GenericParamsVFont(
                size=(194, 56),
                expected_dimensions=(2.409341, 0.492445),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="carriage_return",
            text="A\rB",
            buffer=GenericParamsBuffer(
                size=(113, 72),
                expected_dimensions=(93, 52),
            ),
            vfont=GenericParamsVFont(
                size=(94, 56),
                expected_dimensions=(1.026786, 0.492445),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="line_feed",
            text="A\nB",
            buffer=GenericParamsBuffer(
                size=(67, 170),
                expected_dimensions=(47, 150),
                wrap_width=WRAP_WIDTH_LARGE,
            ),
            vfont=GenericParamsVFont(
                size=(52, 128),
                expected_dimensions=(0.438187, 1.492445),
                position_offset=(10.0, 82.0),
            ),
        ),
        CaseGeneric(
            name="cr_lf",
            text="A\r\nB",
            buffer=GenericParamsBuffer(
                size=(67, 170),
                expected_dimensions=(47, 150),
                wrap_width=WRAP_WIDTH_LARGE,
            ),
            vfont=GenericParamsVFont(
                size=(52, 128),
                expected_dimensions=(0.438187, 1.492445),
                position_offset=(10.0, 82.0),
            ),
        ),
        CaseGeneric(
            name="bom",
            text="A\ufeffB",
            buffer=GenericParamsBuffer(
                size=(113, 72),
                expected_dimensions=(93, 57),
            ),
            vfont=GenericParamsVFont(
                size=(82, 56),
                expected_dimensions=(0.848214, 0.492445),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="zero_width_joiner",
            text="A\u200dB",
            buffer=GenericParamsBuffer(
                size=(113, 72),
                expected_dimensions=(93, 57),
            ),
            vfont=GenericParamsVFont(
                size=(82, 56),
                expected_dimensions=(0.848214, 0.492445),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="replacement_char",
            text="A\ufffdB",
            buffer=GenericParamsBuffer(
                size=(185, 87),
                expected_dimensions=(165, 67),
            ),
            vfont=GenericParamsVFont(
                size=(131, 66),
                expected_dimensions=(1.535028, 0.629121),
                position_offset=(10.0, 17.7),
            ),
        ),
        CaseGeneric(
            name="empty_string",
            text="",
            buffer=GenericParamsBuffer(
                size=(20, 20),
                expected_dimensions=(0, 0),
            ),
            vfont=GenericParamsVFont(
                size=(20, 20),
                expected_dimensions=(0.000000, 0.000000),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="only_combining",
            text="\u0308\u0301",
            buffer=GenericParamsBuffer(
                size=(42, 33),
                expected_dimensions=(22, 13),
            ),
            vfont=GenericParamsVFont(
                size=(34, 28),
                expected_dimensions=(0.192995, 0.109890),
                position_offset=(16.9, -20.0),
            ),
        ),
        CaseGeneric(
            name="double_combining",
            text="U\u0308\u0301B",
            buffer=GenericParamsBuffer(
                size=(120, 100),
                expected_dimensions=(100, 81),
            ),
            vfont=GenericParamsVFont(
                size=(82, 78),
                expected_dimensions=(0.849588, 0.792067),
                position_offset=(5.5, 10.5),
            ),
        ),
        CaseGeneric(
            name="ascii_sweep",
            text="".join((
                *(chr(c) for c in range(1, 256)),
                "\n\n",
                *(chr(c) for c in range(255, 0, -1)),
            )),
            buffer=GenericParamsBuffer(
                size=(1941, 76),
                expected_dimensions=(1921, 76),
                position_offset=(10, 9),
                font_size=globals_buffer.size // 5,
                wrap_width=WRAP_WIDTH_LARGE,
            ),
            vfont=GenericParamsVFont(
                size=(2221, 88),
                expected_dimensions=(30.562490, 0.937958),
                position_offset=(10.0, 86.0),
                font_size=globals_vfont.size / 3.0,
            ),
        ),
        CaseGeneric(
            name="tiny_buffer",
            text="A\u0308B",
            buffer=GenericParamsBuffer(
                size=(113, 83),
                expected_dimensions=(93, 64),
            ),
            vfont=GenericParamsVFont(
                size=(82, 65),
                expected_dimensions=(0.848214, 0.618905),
                position_offset=(10.0, 10.0),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestCombiningWordWrap(TestImageComparison_MixIn, unittest.TestCase):
    """
    Test that word-wrap works correctly with combining characters.
    """

    name_prefix = "combining_word_wrap."
    cases_buffer = [
        CaseBuffer(
            name="combining",
            text=unicodedata.normalize("NFD", "\u00C8ve \u00C0vatar \u0178olanda r\u00E9sum\u00E9 na\u00EFve"),
            size=(293, 481),
            expected_dimensions=(273, 461),
            position_offset=(10, -6),
            wrap_width=230,
        ),
        # Combining mark right at the wrap boundary.
        CaseBuffer(
            name="boundary",
            text=unicodedata.normalize("NFD", "\u0178olanda \u0178olanda \u0178olanda"),
            size=(293, 280),
            expected_dimensions=(273, 261),
            wrap_width=280,
            position_offset=(10, -1),
        ),
        # Single long word that can't wrap, with many combining marks.
        CaseBuffer(
            name="no_break",
            text=unicodedata.normalize("NFD", "\u00C0v\u00E0t\u00E0r\u00E0v\u00E0t\u00E0r"),
            size=(452, 89),
            expected_dimensions=(432, 69),
            wrap_width=180,
            position_offset=(10, -6),
        ),
        # Lorem ipsum paragraph with combining characters at small font size.
        CaseBuffer(
            name="lorem",
            text=generate_lorem_combining(280, seed=42),
            size=(408, 481),
            expected_dimensions=(388, 461),
            font_size=12,
            wrap_width=400,
            position_offset=(10, -10),
        ),
        # Stress test: Latin block + Roman Numerals, grouped into 8-char words.
        CaseBuffer(
            name="stress",
            text=generate_stress_text(word_size=8),
            size=(404, 190),
            expected_dimensions=(384, 176),
            font_size=12,
            wrap_width=400,
            position_offset=(10, 8),
        ),
        # Tiny font size with word wrap, combining lorem ipsum in a 100x100 image.
        CaseBuffer(
            name="font_size_1",
            text=generate_lorem_combining(640, seed=7),
            size=(81, 61),
            expected_dimensions=(61, 42),
            font_size=1,
            wrap_width=62,
            position_offset=(10, -12),
        ),
        # Wrap width of 1 forces a break after every space.
        CaseBuffer(
            name="width_1",
            text=unicodedata.normalize("NFD", "\u00C0v \u00C8x"),
            size=(103, 186),
            expected_dimensions=(83, 166),
            wrap_width=1,
            position_offset=(10, -6),
        ),
    ]


class TestMultiline(TestImageComparison_MixIn, unittest.TestCase):
    """Test multi-line text with literal newlines and combining characters."""

    name_prefix = "multiline."
    cases_buffer = [
        CaseBuffer(
            name="combining",
            text=unicodedata.normalize("NFD", "\u00C8ve\n\u0178olanda"),
            size=(293, 187),
            expected_dimensions=(273, 167),
            wrap_width=9999,
            position_offset=(10, -6),
        ),
        CaseBuffer(
            name="plain",
            text="Hello\nWorld",
            size=(224, 174),
            expected_dimensions=(204, 154),
            wrap_width=9999,
            position_offset=(10, 7),
        ),
        # Combining mark on the last character before a newline.
        CaseBuffer(
            name="combining_before_newline",
            text="X\u0308\nY\u0308",
            size=(62, 181),
            expected_dimensions=(42, 162),
            wrap_width=9999,
            position_offset=(10, -1),
        ),
        # Combining mark on the first character after a newline.
        CaseBuffer(
            name="combining_after_newline",
            text="A\nU\u0308B",
            size=(120, 171),
            expected_dimensions=(100, 151),
            wrap_width=9999,
            position_offset=(10, 10),
        ),
        # Newline-only strings with wrap enabled should have zero dimensions.
        CaseBuffer(
            name="newline_only",
            text="\n",
            size=(20, 20),
            expected_dimensions=(0, 0),
            wrap_width=9999,
            position_offset=(10, 391),
        ),
    ]


class TestCombiningPosition(TestImageComparison_MixIn, unittest.TestCase):
    """
    Test that combining marks are visually positioned over their base character.

    Renders text into an image and compares against reference PNGs using idiff.
    Failed comparisons leave ``*_test.png`` files in the render directory.
    """

    name_prefix = "combining_position."
    _cases_generic = [
        CaseGeneric(
            name="diaeresis_Y",
            text="Y\u0308",
            buffer=GenericParamsBuffer(
                size=(61, 83),
                expected_dimensions=(41, 64),
            ),
            vfont=GenericParamsVFont(
                size=(48, 65),
                expected_dimensions=(0.388736, 0.616844),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="grave_A",
            text="A\u0300",
            buffer=GenericParamsBuffer(
                size=(66, 88),
                expected_dimensions=(46, 68),
            ),
            vfont=GenericParamsVFont(
                size=(52, 68),
                expected_dimensions=(0.438187, 0.660800),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="acute_e",
            text="e\u0301",
            buffer=GenericParamsBuffer(
                size=(61, 77),
                expected_dimensions=(41, 57),
            ),
            vfont=GenericParamsVFont(
                size=(43, 60),
                expected_dimensions=(0.314560, 0.550223),
                position_offset=(7.3, 10.5),
            ),
        ),
        CaseGeneric(
            name="phrase_combining",
            text=unicodedata.normalize("NFD", "\u00C0vatar \u00C8ve \u0178olanda"),
            buffer=GenericParamsBuffer(
                size=(668, 89),
                expected_dimensions=(648, 69),
            ),
            vfont=GenericParamsVFont(
                size=(460, 69),
                expected_dimensions=(6.108516, 0.667668),
                position_offset=(10.0, 10.5),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressCombining(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for combining mark handling: stacking, enclosing, half marks."""

    name_prefix = "stress_combining."
    _cases_generic = [
        CaseGeneric(
            name="all_diacriticals",
            text="X" + "".join(chr(c) for c in range(0x0300, 0x0370)),
            buffer=GenericParamsBuffer(
                size=(102, 1669),
                expected_dimensions=(82, 1649),
                position_offset=(10, -697),
            ),
            vfont=GenericParamsVFont(
                size=(76, 1246),
                expected_dimensions=(0.771978, 17.023011),
                position_offset=(9.8, 674.6),
            ),
        ),
        CaseGeneric(
            name="zalgo_100",
            text=generate_combining_stress("H", "\u0308\u0301\u0300\u0302\u0303", 20) + "ello",
            buffer=GenericParamsBuffer(
                size=(196, 1543),
                expected_dimensions=(176, 1524),
            ),
            vfont=GenericParamsVFont(
                size=(133, 1177),
                expected_dimensions=(1.562500, 16.069021),
                position_offset=(5.2, 10.5),
            ),
        ),
        CaseGeneric(
            name="cgj",
            text="A\u0308\u034f\u0301B",
            buffer=GenericParamsBuffer(
                size=(116, 156),
                expected_dimensions=(96, 136),
            ),
            vfont=GenericParamsVFont(
                size=(83, 117),
                expected_dimensions=(0.873970, 1.336109),
                position_offset=(11.9, 49.5),
            ),
        ),
        CaseGeneric(
            name="half_marks",
            text="A\ufe20B\ufe21C",
            buffer=GenericParamsBuffer(
                size=(159, 90),
                expected_dimensions=(139, 71),
            ),
            vfont=GenericParamsVFont(
                size=(114, 70),
                expected_dimensions=(1.298077, 0.686899),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="enclosing",
            text="A\u20dd B\u20de C\u20e3",
            buffer=GenericParamsBuffer(
                size=(368, 85),
                expected_dimensions=(348, 78),
            ),
            vfont=GenericParamsVFont(
                size=(239, 64),
                expected_dimensions=(3.029671, 0.598971),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="alphabet_above_and_below_combiners",
            text=generate_alphabet_above_and_below_combiners(line_count=8),
            buffer=GenericParamsBuffer(
                size=(658, 519),
                expected_dimensions=(638, 498),
                font_size=21,
                wrap_width=WRAP_WIDTH_LARGE,
            ),
            vfont=GenericParamsVFont(
                size=(544, 452),
                expected_dimensions=(7.268991, 5.991646),
                position_offset=(10.0, 422.5),
                font_size=globals_vfont.size * 0.35,
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressComplexScripts(TestImageComparison_MixIn, unittest.TestCase):
    """Test complex scripts with combining marks in context."""

    name_prefix = "stress_complex_scripts."
    _cases_generic = [
        # RTL script with combining vowel marks.
        # NOTE: The default font has no Arabic glyphs; rendered as placeholders.
        CaseGeneric(
            name="arabic",
            text="\u0628\u064e\u0633\u0650\u0645\u064f \u0627\u0644\u0644\u0651\u0647\u0650",
            buffer=GenericParamsBuffer(
                size=(723, 85),
                expected_dimensions=(703, 77),
            ),
            vfont=GenericParamsVFont(
                size=(432, 63),
                expected_dimensions=(5.717823, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        # Indic script with combining vowel signs and conjunct-forming virama.
        # NOTE: The default font has no Devanagari glyphs; rendered as placeholders.
        CaseGeneric(
            name="devanagari",
            text="\u0928\u092e\u0938\u094d\u0924\u0947",
            buffer=GenericParamsBuffer(
                size=(362, 85),
                expected_dimensions=(342, 77),
            ),
            vfont=GenericParamsVFont(
                size=(217, 63),
                expected_dimensions=(2.732933, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        # Combining above/below marks with no inter-word spaces.
        # NOTE: The default font has no Thai glyphs; rendered as placeholders.
        CaseGeneric(
            name="thai",
            text="\u0e2a\u0e27\u0e31\u0e2a\u0e14\u0e35\u0e04\u0e23\u0e31\u0e1a",
            buffer=GenericParamsBuffer(
                size=(590, 85),
                expected_dimensions=(570, 77),
            ),
            vfont=GenericParamsVFont(
                size=(352, 63),
                expected_dimensions=(4.603812, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        # Composing syllables from separate jamo components.
        # NOTE: The default font has no Hangul glyphs; rendered as placeholders.
        CaseGeneric(
            name="hangul_jamo",
            text="\u1100\u1161\u11a8 \u1102\u1161\u11bc",
            buffer=GenericParamsBuffer(
                size=(381, 85),
                expected_dimensions=(361, 77),
            ),
            vfont=GenericParamsVFont(
                size=(230, 63),
                expected_dimensions=(2.911504, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressWidths(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for wide and variable-width characters."""

    name_prefix = "stress_widths."
    _cases_generic = [
        # NOTE: The default font has no CJK glyphs; rendered as placeholders.
        CaseGeneric(
            name="cjk",
            text="\u4e16\u754c\u4f60\u597d",
            buffer=GenericParamsBuffer(
                size=(248, 85),
                expected_dimensions=(228, 77),
            ),
            vfont=GenericParamsVFont(
                size=(150, 63),
                expected_dimensions=(1.797493, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        CaseGeneric(
            name="fullwidth_latin",
            text="\uff21\uff22\uff23",
            buffer=GenericParamsBuffer(
                size=(191, 85),
                expected_dimensions=(171, 77),
            ),
            vfont=GenericParamsVFont(
                size=(116, 63),
                expected_dimensions=(1.329773, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        CaseGeneric(
            name="halfwidth_kana",
            text="\uff76\uff77\uff78",
            buffer=GenericParamsBuffer(
                size=(191, 85),
                expected_dimensions=(171, 77),
            ),
            vfont=GenericParamsVFont(
                size=(116, 63),
                expected_dimensions=(1.329773, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        CaseGeneric(
            name="fullwidth_space",
            text="\u4e16\u3000\u754c",
            buffer=GenericParamsBuffer(
                size=(191, 85),
                expected_dimensions=(171, 77),
            ),
            vfont=GenericParamsVFont(
                size=(116, 63),
                expected_dimensions=(1.329773, 0.592103),
                position_offset=(8.3, 10.0),
            ),
        ),
        CaseGeneric(
            name="mixed",
            text="\u4e16\u0308Hello",
            buffer=GenericParamsBuffer(
                size=(253, 110),
                expected_dimensions=(233, 90),
                position_offset=(10, -27),
            ),
            vfont=GenericParamsVFont(
                size=(170, 64),
                expected_dimensions=(2.073455, 0.598971),
                position_offset=(8.3, 10.5),
            ),
        ),
        CaseGeneric(
            name="unicode_spaces",
            text="A\u2007B\u2009C\u200aD",
            buffer=GenericParamsBuffer(
                size=(272, 74),
                expected_dimensions=(252, 54),
            ),
            vfont=GenericParamsVFont(
                size=(190, 57),
                expected_dimensions=(2.354396, 0.504121),
                position_offset=(10.0, 10.5),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressZeroWidth(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for zero-width and invisible characters."""

    name_prefix = "stress_zero_width."
    _cases_generic = [
        CaseGeneric(
            name="var_selector",
            text="A\ufe00B\ufe01C",
            buffer=GenericParamsBuffer(
                size=(216, 85),
                expected_dimensions=(196, 82),
            ),
            vfont=GenericParamsVFont(
                size=(148, 64),
                expected_dimensions=(1.765797, 0.598971),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="bom_start",
            text="\ufeffHello",
            buffer=GenericParamsBuffer(
                size=(196, 76),
                expected_dimensions=(176, 56),
            ),
            vfont=GenericParamsVFont(
                size=(133, 59),
                expected_dimensions=(1.562500, 0.528846),
                position_offset=(5.2, 10.5),
            ),
        ),
        CaseGeneric(
            name="word_joiner",
            text="Hello\u2060World",
            buffer=GenericParamsBuffer(
                size=(443, 76),
                expected_dimensions=(423, 56),
            ),
            vfont=GenericParamsVFont(
                size=(301, 59),
                expected_dimensions=(3.890797, 0.528846),
                position_offset=(5.2, 10.5),
            ),
        ),
        CaseGeneric(
            name="soft_hyphen",
            text="break\u00adable",
            buffer=GenericParamsBuffer(
                size=(380, 76),
                expected_dimensions=(360, 56),
            ),
            vfont=GenericParamsVFont(
                size=(261, 59),
                expected_dimensions=(3.345467, 0.528846),
                position_offset=(5.8, 10.5),
            ),
        ),
        CaseGeneric(
            name="sequence",
            text="A\u200b\u200c\u200d\u200eB",
            buffer=GenericParamsBuffer(
                size=(113, 150),
                expected_dimensions=(93, 130),
            ),
            vfont=GenericParamsVFont(
                size=(82, 113),
                expected_dimensions=(0.848214, 1.291552),
                position_offset=(10.0, 67.5),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressBidi(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for bidirectional and layout control characters."""

    name_prefix = "stress_bidi."
    _cases_generic = [
        CaseGeneric(
            name="lrm_rlm",
            text="A\u200eB\u200fC",
            buffer=GenericParamsBuffer(
                size=(159, 138),
                expected_dimensions=(139, 117),
            ),
            vfont=GenericParamsVFont(
                size=(114, 101),
                expected_dimensions=(1.298077, 1.120965),
                position_offset=(10.0, 54.9),
            ),
        ),
        CaseGeneric(
            name="rtl_override",
            text="\u202eHello\u202c",
            buffer=GenericParamsBuffer(
                size=(205, 133),
                expected_dimensions=(185, 113),
            ),
            vfont=GenericParamsVFont(
                size=(144, 99),
                expected_dimensions=(1.708791, 1.089372),
                position_offset=(15.7, 16.6),
            ),
        ),
        CaseGeneric(
            name="nested_embed",
            text="\u202aHello \u202bWorld\u202c\u202c",
            buffer=GenericParamsBuffer(
                size=(428, 207),
                expected_dimensions=(408, 187),
            ),
            vfont=GenericParamsVFont(
                size=(294, 151),
                expected_dimensions=(3.803572, 1.816192),
                position_offset=(15.7, 100.2),
            ),
        ),
        CaseGeneric(
            name="mixed_script",
            text="Hello \u200f\u0645\u0631\u062d\u0628\u0627\u200e World",
            buffer=GenericParamsBuffer(
                size=(723, 149),
                expected_dimensions=(703, 141),
            ),
            vfont=GenericParamsVFont(
                size=(465, 66),
                expected_dimensions=(6.174450, 0.630580),
                position_offset=(5.2, 10.5),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestStressInvalidCodepoints(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for invalid and undefined codepoints."""

    name_prefix = "stress_invalid_codepoints."
    _cases_generic = [
        CaseGeneric(
            name="nonchar_fdd0",
            text="A\ufdd0B",
            buffer=GenericParamsBuffer(
                size=(170, 85),
                expected_dimensions=(150, 77),
            ),
            vfont=GenericParamsVFont(
                size=(115, 63),
                expected_dimensions=(1.315934, 0.592103),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="nonchar_ffff",
            text="A\uffffB",
            buffer=GenericParamsBuffer(
                size=(170, 85),
                expected_dimensions=(150, 77),
            ),
            vfont=GenericParamsVFont(
                size=(115, 63),
                expected_dimensions=(1.315934, 0.592103),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="specials",
            text="\ufff0\ufff1\ufffd",
            buffer=GenericParamsBuffer(
                size=(206, 96),
                expected_dimensions=(186, 89),
            ),
            vfont=GenericParamsVFont(
                size=(134, 71),
                expected_dimensions=(1.570021, 0.699246),
                position_offset=(8.3, 17.7),
            ),
        ),
        CaseGeneric(
            name="replacement",
            text="\ufffd\ufffd\ufffd",
            buffer=GenericParamsBuffer(
                size=(236, 87),
                expected_dimensions=(216, 67),
            ),
            vfont=GenericParamsVFont(
                size=(165, 66),
                expected_dimensions=(2.003434, 0.629121),
                position_offset=(8.0, 17.7),
            ),
        ),
        CaseGeneric(
            name="max_codepoint",
            text="A\U0010ffffB",
            buffer=GenericParamsBuffer(
                size=(170, 85),
                expected_dimensions=(150, 77),
            ),
            vfont=GenericParamsVFont(
                size=(115, 63),
                expected_dimensions=(1.315934, 0.592103),
                position_offset=(10.0, 10.0),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)

    def test_surrogates_raise(self) -> None:
        """Lone surrogates should raise UnicodeEncodeError, not crash."""
        for text in ["\ud800", "\udfff", "\ud800\udc00"]:
            with self.subTest(text=repr(text)):
                with self.assertRaises(UnicodeEncodeError):
                    blf.dimensions(self.font_id, text)

    def test_embedded_null_raises(self) -> None:
        """Embedded null should raise ValueError, not silently truncate."""
        with self.assertRaises(ValueError):
            blf.dimensions(self.font_id, "A\x00B")


class TestStressSupplementary(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for 4-byte UTF-8 (codepoints above U+FFFF)."""

    name_prefix = "stress_supplementary."
    cases_buffer = [
        # Emoji (supplementary plane).
        CaseBuffer(
            name="emoji",
            text="A\U0001f600B\U0001f64fC",
            size=(273, 85),
            expected_dimensions=(253, 78),
            position_offset=(10, -2),
        ),
        # Mathematical Alphanumeric Symbols.
        CaseBuffer(
            name="math_alpha",
            text="\U0001d400\U0001d401\U0001d402",
            size=(191, 85),
            expected_dimensions=(171, 77),
            position_offset=(10, -2),
        ),
        # Emoji Zero Width Joiner (ZWJ) sequence.
        CaseBuffer(
            name="emoji_zwj",
            text="\U0001f468\u200d\U0001f469\u200d\U0001f467",
            size=(191, 85),
            expected_dimensions=(171, 82),
            position_offset=(10, -2),
        ),
        # Mixed BMP and supplementary.
        CaseBuffer(
            name="mixed",
            text="Hello \U0001f600 World \U0001d400",
            size=(571, 85),
            expected_dimensions=(551, 78),
            position_offset=(10, -2),
        ),
    ]


class TestCombiningEdgePositions(TestImageComparison_MixIn, unittest.TestCase):
    """Test combining marks at edge positions: start, end, after space/zero-width."""

    name_prefix = "combining_edge_positions."
    _cases_generic = [
        CaseGeneric(
            name="combining_first",
            text="\u0308Hello",
            buffer=GenericParamsBuffer(
                size=(207, 76),
                expected_dimensions=(187, 56),
            ),
            vfont=GenericParamsVFont(
                size=(145, 59),
                expected_dimensions=(1.725618, 0.528846),
                position_offset=(16.9, 10.5),
            ),
        ),
        CaseGeneric(
            name="combining_after_space",
            text="A \u0308B",
            buffer=GenericParamsBuffer(
                size=(132, 72),
                expected_dimensions=(112, 52),
            ),
            vfont=GenericParamsVFont(
                size=(94, 56),
                expected_dimensions=(1.026786, 0.492445),
                position_offset=(10.0, 10.0),
            ),
        ),
        CaseGeneric(
            name="combining_at_end",
            text="Hello\u0308",
            buffer=GenericParamsBuffer(
                size=(196, 76),
                expected_dimensions=(176, 56),
            ),
            vfont=GenericParamsVFont(
                size=(133, 59),
                expected_dimensions=(1.562500, 0.528846),
                position_offset=(5.2, 10.5),
            ),
        ),
        CaseGeneric(
            name="many_combining_no_base",
            text="\u0308\u0301\u0300\u0302\u0303\u0304\u0305\u0306\u0307\u0309",
            buffer=GenericParamsBuffer(
                size=(51, 39),
                expected_dimensions=(31, 19),
            ),
            vfont=GenericParamsVFont(
                size=(41, 33),
                expected_dimensions=(0.286401, 0.173077),
                position_offset=(20.3, -19.2),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestCombiningSequences(TestImageComparison_MixIn, unittest.TestCase):
    """Test combining mark sequences: bursts, alternating, repeated."""

    name_prefix = "combining_sequences."
    _cases_generic = [
        CaseGeneric(
            name="base_then_burst",
            text="ABCDE\u0308\u0301\u0300",
            buffer=GenericParamsBuffer(
                size=(252, 116),
                expected_dimensions=(232, 97),
            ),
            vfont=GenericParamsVFont(
                size=(176, 90),
                expected_dimensions=(2.161401, 0.960422),
                position_offset=(10.0, 10.5),
            ),
        ),
        CaseGeneric(
            name="alternating",
            text="a\u0308b\u0308c\u0308d\u0308",
            buffer=GenericParamsBuffer(
                size=(183, 87),
                expected_dimensions=(163, 68),
            ),
            vfont=GenericParamsVFont(
                size=(126, 68),
                expected_dimensions=(1.469780, 0.655306),
                position_offset=(7.7, 10.5),
            ),
        ),
        CaseGeneric(
            name="repeated_mark",
            text="A\u0308\u0308\u0308",
            buffer=GenericParamsBuffer(
                size=(66, 106),
                expected_dimensions=(46, 87),
            ),
            vfont=GenericParamsVFont(
                size=(52, 83),
                expected_dimensions=(0.438187, 0.871824),
                position_offset=(10.0, 10.0),
            ),
        ),
    ]
    cases_buffer = case_generic_to_buffer(_cases_generic)
    cases_vfont = case_generic_to_vfont(_cases_generic)


class TestWrapCombiningEdge(TestImageComparison_MixIn, unittest.TestCase):
    """Test word wrap when combining marks fall near the wrap boundary."""

    name_prefix = "wrap_combining_edge."
    cases_buffer = [
        # Combining mark on the last char before wrap boundary.
        CaseBuffer(
            name="at_combining",
            text="WWWWW\u0308 Next",
            size=(355, 182),
            expected_dimensions=(335, 163),
            wrap_width=300,
            position_offset=(10, -1),
        ),
        # Tighter wrap forcing break right after base+combining.
        CaseBuffer(
            name="combining_tight",
            text="WWWW\u0308 Next",
            size=(288, 182),
            expected_dimensions=(268, 163),
            wrap_width=200,
            position_offset=(10, -1),
        ),
    ]


class TestStressEdgeParams(TestImageComparison_MixIn, unittest.TestCase):
    """Stress tests for edge-case parameter values."""

    name_prefix = "stress_edge_params."
    cases_buffer = [
        # Font size 1.
        CaseBuffer(
            name="font_size_1",
            text="Hello",
            size=(23, 21),
            expected_dimensions=(3, 4),
            font_size=1,
            position_offset=(10, -10),
        ),
        # Font size 2.
        CaseBuffer(
            name="font_size_2",
            text="Hello",
            size=(26, 22),
            expected_dimensions=(6, 3),
            font_size=2,
            position_offset=(10, -10),
        ),
        # wrap_width=0 (wraps every character).
        CaseBuffer(
            name="wrap_0",
            text="Hello World",
            size=(224, 174),
            expected_dimensions=(204, 154),
            wrap_width=0,
            position_offset=(10, 7),
        ),
        # Negative position (text off-screen, should not crash).
        CaseBuffer(
            name="negative_pos",
            text="Hello",
            size=(196, 76),
            expected_dimensions=(176, 56),
            position_offset=(10, 7),
        ),
    ]


# ---------------------------------------------------------------------------
# VFont Tests

class TestBasic(TestImageComparison_MixIn, unittest.TestCase):
    """Basic VFont text rendering."""

    name_prefix = "basic."
    cases_vfont = [
        CaseVFont(
            name="alpha_bravo",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "b"))),
            size=(299, 70),
            expected_dimensions=(3.869505, 0.686813),
            position_offset=(10.0, 21.9),
        ),
        CaseVFont(
            name="charlie_delta",
            text=" ".join(generate_phonetic_alphabet(letters=("c", "e"))),
            size=(438, 59),
            expected_dimensions=(5.798077, 0.528846),
            position_offset=(7.0, 10.5),
        ),
    ]


class TestTextOnCurve(TestImageComparison_MixIn, unittest.TestCase):
    """Test rendering of vfont text laid out along a curve via ``follow_curve``."""

    name_prefix = "text_on_curve."

    @staticmethod
    @contextlib.contextmanager
    def _circle(obj: bpy.types.Object) -> Iterator[None]:
        """
        Create a Bezier circle and assign it as the text object's ``follow_curve``.

        Setup runs before depsgraph evaluation; ``finally`` removes the path object and
        curve so cleanup happens even if evaluation raises.
        """
        path_curve = bpy.data.curves.new(name="_test_path", type='CURVE')
        path_curve.dimensions = '3D'
        spline = path_curve.splines.new('BEZIER')
        spline.bezier_points.add(3)  # 4 points total.
        r = 2.0
        h = r * 0.5523  # Standard 4-point Bezier circle handle ratio.
        points = (
            ((r, 0.0, 0.0), (r, -h, 0.0), (r, h, 0.0)),
            ((0.0, r, 0.0), (h, r, 0.0), (-h, r, 0.0)),
            ((-r, 0.0, 0.0), (-r, h, 0.0), (-r, -h, 0.0)),
            ((0.0, -r, 0.0), (-h, -r, 0.0), (h, -r, 0.0)),
        )
        for i, (co, hl, hr) in enumerate(points):
            bp = spline.bezier_points[i]
            bp.co = co
            bp.handle_left = hl
            bp.handle_right = hr
        spline.use_cyclic_u = True

        path_obj = bpy.data.objects.new(name="_test_path_obj", object_data=path_curve)
        bpy.context.collection.objects.link(path_obj)
        obj.data.follow_curve = path_obj
        # Shift the text body down 1.0 in its local frame so it sits below the
        # path baseline; on a tightly-curved circle this gives characters more
        # arc length per unit, eliminating overlap.
        obj.data.offset_y = -1.0
        try:
            yield
        finally:
            bpy.data.objects.remove(path_obj)
            bpy.data.curves.remove(path_curve)

    cases_vfont = [
        CaseVFont(
            name="circle",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "m"))),
            font_size=0.6,
            size=(457, 455),
            expected_dimensions=(6.060367, 6.036401),
            position_offset=(230.5, 228.8),
            with_context_fn=_circle,
        ),
    ]


class TestMaterials(TestImageComparison_MixIn, unittest.TestCase):
    """Test per-character material assignment with colored rendering."""

    name_prefix = "materials."
    cases_vfont = [
        # Two materials: red and blue.
        CaseVFont(
            name="two_colors",
            text=(
                TextFormatCompose()
                .style(material=0).text("Red")
                .text(" ")
                .style(material=1).text("Blue")
            ),
            size=(218, 59),
            expected_dimensions=(2.749313, 0.528846),
            position_offset=(5.2, 10.5),
            materials=[(1.0, 0.2, 0.2), (0.2, 0.4, 1.0)],
        ),
        # Three materials: red, green, blue.
        CaseVFont(
            name="three_colors",
            text=(
                TextFormatCompose()
                .style(material=0).text("Red ")
                .style(material=1).text("Green ")
                .style(material=2).text("Blue")
            ),
            size=(374, 59),
            expected_dimensions=(4.910715, 0.528846),
            position_offset=(5.2, 10.5),
            materials=[(1.0, 0.2, 0.2), (0.2, 1.0, 0.2), (0.2, 0.4, 1.0)],
        ),
        # Material with bold styling.
        CaseVFont(
            name="colored_bold",
            text=(
                TextFormatCompose()
                .style(material=0, bold=True).text("Yellow")
                .style(material=1, bold=False).text(" Cyan")
            ),
            size=(307, 70),
            expected_dimensions=(3.980408, 0.686097),
            position_offset=(10.0, 21.9),
            materials=[(1.0, 0.8, 0.2), (0.2, 0.8, 1.0)],
        ),
        # CMYK: each letter underlined and colored with its corresponding material slot.
        # K (Key) is a dark gray so it stays visible against the black background.
        CaseVFont(
            name="cmyk",
            text=(
                TextFormatCompose()
                .style(material=0, underline=True).text("C")
                .style(material=1, underline=True).text("M")
                .style(material=2, underline=True).text("Y")
                .style(material=3, underline=True).text("K")
            ),
            size=(155, 64),
            expected_dimensions=(1.870879, 0.597253),
            position_offset=(10.0, 17.2),
            materials=[
                (0.0, 1.0, 1.0),  # C - Cyan
                (1.0, 0.0, 1.0),  # M - Magenta
                (1.0, 1.0, 0.0),  # Y - Yellow
                (0.4, 0.4, 0.4),  # K - Key (dark gray)
            ],
        ),
    ]


class TestTextBoxes(TestImageComparison_MixIn, unittest.TestCase):
    """Test text layout with text boxes of various configurations."""

    name_prefix = "text_boxes."
    cases_vfont = [
        # Single narrow text box, forces word wrap.
        CaseVFont(
            name="narrow_wrap",
            text=" ".join(generate_random_words(3, seed=10)),
            size=(263, 214),
            expected_dimensions=(3.364011, 2.686813),
            position_offset=(7.5, 165.9),
            text_boxes=[TextBox(0.0, 0.0, 1.5, 0.0)],
        ),
        # Tall box with limited height, text should be clipped.
        CaseVFont(
            name="height_limit",
            text="\n".join(generate_random_words(4, seed=20)),
            size=(191, 275),
            expected_dimensions=(2.368818, 3.528846),
            position_offset=(13.9, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 4.0, 2.0)],
        ),
        # Two text boxes side by side, text flows from first to second.
        CaseVFont(
            name="two_boxes",
            text=" ".join(generate_random_words(8, seed=30)),
            size=(424, 502),
            expected_dimensions=(5.607142, 6.686813),
            position_offset=(5.2, 453.9),
            text_boxes=[
                TextBox(0.0, 0.0, 2.5, 1.5),
                TextBox(3.0, 0.0, 2.5, 1.5),
            ],
        ),
        # Offset text box (non-zero x, y origin).
        CaseVFont(
            name="offset_origin",
            text=" ".join(generate_random_words(1, seed=40)),
            size=(149, 68),
            expected_dimensions=(1.783654, 0.655220),
            position_offset=(-62.2, 93.9),
            text_boxes=[TextBox(1.0, -1.0, 3.0, 0.0)],
        ),
        # Very narrow box with combining characters.
        CaseVFont(
            name="narrow_combining",
            text=unicodedata.normalize("NFD", "\u00C0vatar \u00C8ve"),
            size=(170, 141),
            expected_dimensions=(2.079670, 1.667668),
            position_offset=(10.0, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 1.0, 0.0)],
        ),
    ]


class TestStyles(TestImageComparison_MixIn, unittest.TestCase):
    """Test per-character styling: bold, italic, underline, smallcaps."""

    name_prefix = "styles."
    cases_vfont = [
        CaseVFont(
            name="underline",
            text=(
                TextFormatCompose()
                .text("Golf ")
                .style(underline=True).text("Hotel")
                .style(underline=False).text(" India")
            ),
            size=(378, 66),
            expected_dimensions=(4.967720, 0.625412),
            position_offset=(7.0, 17.2),
        ),
        CaseVFont(
            name="smallcaps",
            text=(
                TextFormatCompose()
                .text("Juliet ")
                .style(smallcaps=True).text("Kilo")
                .style(smallcaps=False).text(" Lima")
            ),
            size=(366, 67),
            expected_dimensions=(4.793270, 0.652473),
            position_offset=(13.9, 19.4),
        ),
        CaseVFont(
            name="mixed",
            text=(
                TextFormatCompose()
                .style(bold=True).text("Mike")
                .style(bold=False).text(" ")
                .style(italic=True).text("November")
                .style(italic=False).text(" ")
                .style(underline=True).text("Oscar")
                .style(underline=False).text(" ")
                .style(smallcaps=True).text("Papa")
            ),
            size=(634, 65),
            expected_dimensions=(8.523564, 0.621948),
            position_offset=(5.6, 17.2),
        ),
        # All four font variants in a single string.
        CaseVFont(
            name="mixed_fonts",
            text=(
                TextFormatCompose()
                .style(bold=False, italic=False).text("Regular")
                .style(bold=True).text("Bold")
                .style(bold=False, italic=True).text("Italic")
                .style(bold=True, italic=True).text("BoldItalic")
            ),
            size=(640, 70),
            expected_dimensions=(8.606053, 0.689301),
            position_offset=(5.2, 21.9),
        ),
        # Tight kerning.
        CaseVFont(
            name="kern_tight",
            text=(
                TextFormatCompose()
                .style(kerning=-5.0).text("AV")
                .style(kerning=0.0).text("atar")
            ),
            size=(167, 56),
            expected_dimensions=(2.036487, 0.499313),
            position_offset=(10.0, 10.5),
        ),
        # Wide kerning.
        CaseVFont(
            name="kern_wide",
            text=(
                TextFormatCompose()
                .style(kerning=10.0).text("Hello")
            ),
            size=(156, 59),
            expected_dimensions=(1.875172, 0.528846),
            position_offset=(5.2, 10.5),
        ),
    ]


class TestSize(TestImageComparison_MixIn, unittest.TestCase):
    """Test font size variations."""

    name_prefix = "size."
    cases_vfont = [
        CaseVFont(
            name="half",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "b"))),
            size=(160, 45),
            expected_dimensions=(1.934753, 0.343407),
            position_offset=(10.0, 15.9),
            font_size=0.5,
        ),
        CaseVFont(
            name="double",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "b"))),
            size=(578, 119),
            expected_dimensions=(7.739011, 1.373626),
            position_offset=(10.0, 33.7),
            font_size=2.0,
        ),
    ]


class TestSpacing(TestImageComparison_MixIn, unittest.TestCase):
    """Test word, line, and character spacing variations."""

    name_prefix = "spacing."
    cases_vfont = [
        CaseVFont(
            name="word_tight",
            text=" ".join(generate_phonetic_alphabet(letters=("e", "h"))),
            size=(521, 59),
            expected_dimensions=(6.956732, 0.532280),
            position_offset=(5.2, 10.5),
            space_word=0.25,
        ),
        CaseVFont(
            name="word_wide",
            text=" ".join(generate_phonetic_alphabet(letters=("e", "h"))),
            size=(579, 59),
            expected_dimensions=(7.760303, 0.532280),
            position_offset=(5.2, 10.5),
            space_word=1.75,
        ),
        CaseVFont(
            name="line_tight",
            text="\n".join(generate_phonetic_alphabet(letters=("a", "d"))),
            size=(181, 167),
            expected_dimensions=(2.234203, 2.028846),
            position_offset=(10.0, 118.5),
            space_line=0.5,
        ),
        CaseVFont(
            name="line_wide",
            text="\n".join(generate_phonetic_alphabet(letters=("a", "d"))),
            size=(181, 491),
            expected_dimensions=(2.234203, 6.528846),
            position_offset=(10.0, 442.5),
            space_line=2.0,
        ),
        CaseVFont(
            name="character_tight",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "d"))),
            size=(468, 70),
            expected_dimensions=(6.214801, 0.686813),
            position_offset=(10.0, 21.9),
            space_character=0.75,
        ),
        CaseVFont(
            name="character_wide",
            text=" ".join(generate_phonetic_alphabet(letters=("a", "d"))),
            size=(1041, 70),
            expected_dimensions=(14.176510, 0.686813),
            position_offset=(10.0, 21.9),
            space_character=1.5,
        ),
    ]


class TestGeometry(TestImageComparison_MixIn, unittest.TestCase):
    """
    Test geometry offset and extrude.
    """

    name_prefix = "geometry."
    cases_vfont = [
        CaseVFont(
            name="offset_positive",
            text=" ".join(generate_random_words(4, seed=50)),
            size=(599, 84),
            expected_dimensions=(7.873791, 0.726813),
            position_offset=(12.4, 29.1),
            geometry_offset=0.02,
        ),
        CaseVFont(
            name="offset_negative",
            text=" ".join(generate_random_words(4, seed=50)),
            size=(591, 65),
            expected_dimensions=(7.793791, 0.646813),
            position_offset=(8.0, 15.5),
            geometry_offset=-0.02,
        ),
    ]


class TestGridLayouts(TestImageComparison_MixIn, unittest.TestCase):
    """
    4x4 grid of text boxes that text flows through, with non-standard word spacing
    and a very small font. Exercises multi-text-box layout in a single curve.
    """

    name_prefix = "grid_layouts."
    cases_vfont = [
        CaseVFont(
            name="grid_4x4",
            # Enough words to overflow through several boxes without making the
            # curve-fill triangulation pass slow on the test build.
            text=" ".join(generate_random_words(170, seed=42)),
            # 4x4 grid where each cell is 1.4 wide / 0.8 tall, laid out on a stride of
            # 2.0 / 1.4 - leaves a 0.6 horizontal and 0.6 vertical gap between boxes.
            text_boxes=[
                TextBox(col * 2.0, -row * 1.4, 1.4, 0.8)
                for row in range(4)
                for col in range(4)
            ],
            align_x='JUSTIFY',
            align_y='TOP',
            space_word=1.25,
            font_size=globals_vfont.size / 5.0,
            resolution=2,
            size=(553, 376),
            expected_dimensions=(7.397940, 4.938049),
            position_offset=(10.0, 354.1),
        ),
    ]


class TestOverflowWillOverflow(TestImageComparison_MixIn, unittest.TestCase):
    """Test overflow mode (text spills outside the text box)."""

    name_prefix = "overflow_will_overflow."
    cases_vfont = [
        # Text overflows a small box.
        CaseVFont(
            name="spill_single",
            text=" ".join(generate_random_words(4, seed=50)),
            size=(179, 272),
            expected_dimensions=(2.206731, 3.497253),
            position_offset=(8.0, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.0, 1.0)],
            overflow='NONE',
        ),
        # Multiline overflow past box height.
        CaseVFont(
            name="spill_multiline",
            text="\n".join(generate_random_words(5, seed=60)),
            size=(262, 345),
            expected_dimensions=(3.357143, 4.513050),
            position_offset=(7.0, 298.5),
            text_boxes=[TextBox(0.0, 0.0, 3.0, 1.5)],
            overflow='NONE',
        ),
        # Combining characters overflow.
        CaseVFont(
            name="spill_combining",
            text=unicodedata.normalize("NFD", "\u00C0vatar \u00C8ve \u0178olanda"),
            size=(204, 213),
            expected_dimensions=(2.543269, 2.667668),
            position_offset=(10.0, 154.5),
            text_boxes=[TextBox(0.0, 0.0, 2.0, 1.0)],
            overflow='NONE',
        ),
        # Alignment variations.
        CaseVFont(
            name="align_left",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 0.0)],
            overflow='NONE',
            align_x='LEFT',
        ),
        CaseVFont(
            name="align_center",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(205, 275),
            expected_dimensions=(2.563874, 3.528846),
            position_offset=(12.1, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 0.0)],
            overflow='NONE',
            align_x='CENTER',
        ),
        CaseVFont(
            name="align_right",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.584478, 3.528846),
            position_offset=(18.6, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 0.0)],
            overflow='NONE',
            align_x='RIGHT',
        ),
        CaseVFont(
            name="align_justify",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 0.0)],
            overflow='NONE',
            align_x='JUSTIFY',
        ),
        CaseVFont(
            name="align_flush",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(195, 275),
            expected_dimensions=(2.423077, 3.528846),
            position_offset=(7.0, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 0.0)],
            overflow='NONE',
            align_x='FLUSH',
        ),
        # Vertical alignment variations.
        CaseVFont(
            name="valign_top",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 207.4),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='NONE',
            align_y='TOP',
        ),
        CaseVFont(
            name="valign_top_baseline",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 226.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='NONE',
            align_y='TOP_BASELINE',
        ),
        CaseVFont(
            name="valign_center",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 155.7),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='NONE',
            align_y='CENTER',
        ),
        CaseVFont(
            name="valign_bottom_baseline",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 118.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='NONE',
            align_y='BOTTOM_BASELINE',
        ),
        CaseVFont(
            name="valign_bottom",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(207, 275),
            expected_dimensions=(2.583791, 3.528846),
            position_offset=(7.0, 104.0),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='NONE',
            align_y='BOTTOM',
        ),
    ]


class TestOverflowWillScaleToFit(TestImageComparison_MixIn, unittest.TestCase):
    """Test scale-to-fit overflow mode (text scales down to fit the box)."""

    name_prefix = "overflow_will_scale_to_fit."
    cases_vfont = [
        # Long text scaled to fit a narrow box.
        CaseVFont(
            name="narrow",
            text=" ".join(generate_random_words(6, seed=80)),
            size=(236, 73),
            expected_dimensions=(2.987429, 0.735459),
            position_offset=(9.1, 44.9),
            text_boxes=[TextBox(0.0, 0.0, 3.0, 1.0)],
            overflow='SCALE',
        ),
        # Multiline text scaled down.
        CaseVFont(
            name="multiline",
            text="\n".join(generate_random_words(4, seed=90)),
            size=(112, 115),
            expected_dimensions=(1.274640, 1.317394),
            position_offset=(10.0, 91.2),
            text_boxes=[TextBox(0.0, 0.0, 3.0, 1.5)],
            overflow='SCALE',
        ),
        # Text that already fits (no scaling needed).
        CaseVFont(
            name="fits",
            text=" ".join(generate_random_words(1, seed=100)),
            size=(232, 54),
            expected_dimensions=(2.932313, 0.465350),
            position_offset=(5.8, 10.4),
            text_boxes=[TextBox(0.0, 0.0, 3.0, 2.0)],
            overflow='SCALE',
        ),
        # Combining characters with scale-to-fit.
        CaseVFont(
            name="combining",
            text=unicodedata.normalize("NFD", "\u00C0vatar \u00C8ve \u0178olanda"),
            size=(141, 81),
            expected_dimensions=(1.675824, 0.833834),
            position_offset=(10.0, 46.2),
            text_boxes=[TextBox(0.0, 0.0, 2.0, 1.0)],
            overflow='SCALE',
        ),
        # Alignment variations.
        CaseVFont(
            name="align_left",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_x='LEFT',
        ),
        CaseVFont(
            name="align_center",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.640968, 0.882212),
            position_offset=(-57.0, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_x='CENTER',
        ),
        CaseVFont(
            name="align_right",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.646119, 0.882212),
            position_offset=(-122.8, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_x='RIGHT',
        ),
        CaseVFont(
            name="align_justify",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_x='JUSTIFY',
        ),
        CaseVFont(
            name="align_flush",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(199, 84),
            expected_dimensions=(2.480769, 0.882212),
            position_offset=(9.2, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_x='FLUSH',
        ),
        # Vertical alignment variations.
        CaseVFont(
            name="valign_top",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 59.3),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_y='TOP',
        ),
        CaseVFont(
            name="valign_top_baseline",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_y='TOP_BASELINE',
        ),
        CaseVFont(
            name="valign_center",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 59.9),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_y='CENTER',
        ),
        CaseVFont(
            name="valign_bottom_baseline",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 64.1),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_y='BOTTOM_BASELINE',
        ),
        CaseVFont(
            name="valign_bottom",
            text="\n".join(generate_random_words(4, seed=70)),
            size=(67, 84),
            expected_dimensions=(0.645948, 0.882212),
            position_offset=(9.2, 60.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 1.0)],
            overflow='SCALE',
            align_y='BOTTOM',
        ),
    ]


class TestOverflowWillTruncate(TestImageComparison_MixIn, unittest.TestCase):
    """Test truncate overflow mode (text beyond the box is not rendered)."""

    name_prefix = "overflow_will_truncate."
    cases_vfont = [
        # Explicit newlines, box tall enough for 3 lines, truncates the rest.
        CaseVFont(
            name="cut_lines",
            text="\n".join(generate_random_words(6, seed=110)),
            size=(168, 214),
            expected_dimensions=(2.046017, 2.686813),
            position_offset=(13.9, 165.9),
            text_boxes=[TextBox(0.0, 0.0, 3.0, 3.0)],
            overflow='TRUNCATE',
        ),
        # Long single line wraps into multiple lines, box truncates after 2 lines.
        CaseVFont(
            name="cut_wrap",
            text=" ".join(generate_random_words(12, seed=120)),
            size=(195, 136),
            expected_dimensions=(2.423077, 1.607143),
            position_offset=(7.0, 90.4),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
        ),
        # Combining characters wrap then truncate.
        CaseVFont(
            name="cut_combining",
            text=unicodedata.normalize(
                "NFD",
                "\u00C0vatar \u00C8ve \u0178olanda r\u00E9sum\u00E9 na\u00EFve caf\u00E9",
            ),
            size=(170, 141),
            expected_dimensions=(2.079670, 1.667668),
            position_offset=(10.0, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
        ),
        # Alignment variations (wraps then truncates).
        CaseVFont(
            name="align_left",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_x='LEFT',
        ),
        CaseVFont(
            name="align_center",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(150, 128),
            expected_dimensions=(1.805289, 1.497253),
            position_offset=(-14.3, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_x='CENTER',
        ),
        CaseVFont(
            name="align_right",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(154, 128),
            expected_dimensions=(1.859204, 1.497253),
            position_offset=(-36.1, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_x='RIGHT',
        ),
        CaseVFont(
            name="align_justify",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_x='JUSTIFY',
        ),
        CaseVFont(
            name="align_flush",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(200, 128),
            expected_dimensions=(2.497253, 1.497253),
            position_offset=(9.8, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_x='FLUSH',
        ),
        # Vertical alignment variations (wraps then truncates).
        CaseVFont(
            name="valign_top",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 63.4),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_y='TOP',
        ),
        CaseVFont(
            name="valign_top_baseline",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 82.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_y='TOP_BASELINE',
        ),
        CaseVFont(
            name="valign_center",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 83.7),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_y='CENTER',
        ),
        CaseVFont(
            name="valign_bottom_baseline",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 118.5),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_y='BOTTOM_BASELINE',
        ),
        CaseVFont(
            name="valign_bottom",
            text=" ".join(generate_random_words(10, seed=130)),
            size=(153, 128),
            expected_dimensions=(1.835852, 1.497253),
            position_offset=(9.8, 104.0),
            text_boxes=[TextBox(0.0, 0.0, 2.5, 2.5)],
            overflow='TRUNCATE',
            align_y='BOTTOM',
        ),
    ]


# ------------------------------------------------------------------------------
# Argument Parser

def argparse_create() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--generate",
        action="store_true",
        help="Generate reference images instead of comparing (valid with --mode=BUFFER, VFONT, or ALL)",
    )
    parser.add_argument(
        "--show-html",
        type=str,
        default=None,
        metavar="PATH",
        help="Write an HTML report of failures to PATH",
    )
    parser.add_argument(
        "-k", "--keyword",
        type=str,
        default=None,
        metavar="PATTERN",
        help="Filter tests by name pattern (passed as -k to unittest)",
    )
    parser.add_argument(
        "--use-vfont-render",
        action="store_true",
        help="Render VFont tests via an orthographic Cycles camera instead of the GPU triangle path",
    )
    parser.add_argument(
        "--mode",
        choices=tuple(MODE_KINDS),
        default="BUFFER",
        help=(
            "Which rendering pipeline to test. CMake registers one test per mode "
            "(BUFFER, GPU, VFONT). ALL runs every kind in a single invocation - useful for manual runs."
        ),
    )
    return parser


# ---------------------------------------------------------------------------
# Main

def main() -> None:
    global USE_GENERATE_TEST_DATA, USE_VFONT_RENDER, VFONT_RENDER_DIR, SHOW_HTML, OUTPUT_DIR, FONTS_VFONT
    global VFONT_RENDER_CTX, gpu

    if "--" in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index("--") + 1:]
    else:
        argv = sys.argv

    parser = argparse_create()
    args, remaining = parser.parse_known_args(argv)

    # Reference PNGs are authored by the BUFFER or VFONT pipelines; GPU mode
    # only ever validates against them, so --generate is not meaningful for GPU
    # alone.  ALL is allowed: buffer/vfont references are written, then GPU
    # tests run as normal comparisons against the freshly generated references.
    if args.generate and args.mode == 'GPU':
        sys.exit("--generate is not valid with --mode=GPU (GPU validates against buffer references)")

    active_kinds = set(MODE_KINDS[args.mode])
    attach_test_methods(args.mode)

    if args.keyword:
        remaining.extend(["-k", args.keyword])

    USE_GENERATE_TEST_DATA = args.generate
    if args.use_vfont_render:
        USE_VFONT_RENDER = True
    SHOW_HTML = args.show_html or ""

    # GPU is needed for the GPU and VFont pipelines (off-screen rendering).
    gpu_needed = False
    if "gpu" in active_kinds:
        gpu_needed = True
    elif "vfont" in active_kinds:
        if not USE_VFONT_RENDER:
            gpu_needed = True
    if gpu_needed:
        # Expect failure.
        try:
            gpu.init()
        except SystemError as ex:
            if os.environ.get("WITHOUT_GPU"):
                gpu = None
                # Without this VFont won't work.
                if "vfont" in active_kinds:
                    USE_VFONT_RENDER = True
            else:
                sys.exit("GPU initialization failed: {:s}".format(str(ex)))
    del gpu_needed

    # Load the bpy VectorFont once if any vfont tests are active.
    if "vfont" in active_kinds:
        FONTS_VFONT = VFontSet(
            regular=bpy.data.fonts.load(FONT_PATH_REGULAR),
            bold=bpy.data.fonts.load(FONT_PATH_BOLD),
            italic=bpy.data.fonts.load(FONT_PATH_ITALIC),
            bold_italic=bpy.data.fonts.load(FONT_PATH_BOLD_ITALIC),
        )

    # Persistent dir for test images (--show-html) or a throwaway temp dir.
    output_ctx: contextlib.AbstractContextManager[str]
    if SHOW_HTML:
        output_dir_path = os.path.join(TEST_DIR, "output_" + args.mode.lower())
        os.makedirs(output_dir_path, exist_ok=True)
        output_ctx = contextlib.nullcontext(output_dir_path)
    else:
        output_ctx = tempfile.TemporaryDirectory()

    # Temp dir in Blender's temp space for the Cycles render output PNG.
    vfont_tmp_ctx = (
        tempfile.TemporaryDirectory(dir=bpy.app.tempdir)
        if (USE_VFONT_RENDER and "vfont" in active_kinds)
        else contextlib.nullcontext("")
    )
    with output_ctx as output_dir, vfont_tmp_ctx as tmpdir:
        OUTPUT_DIR = output_dir
        VFONT_RENDER_DIR = tmpdir
        # Separate: VFontRenderContext.__init__ reads VFONT_RENDER_DIR.
        # Scene setup/teardown for Cycles camera renders.
        vfont_render_ctx = (
            VFontRenderContext()
            if (USE_VFONT_RENDER and "vfont" in active_kinds)
            else contextlib.nullcontext(None)
        )
        with vfont_render_ctx as VFONT_RENDER_CTX:
            unittest.main(argv=remaining, exit=False)
    if FONTS_VFONT is not None:
        for font in FONTS_VFONT:
            bpy.data.fonts.remove(font)
        FONTS_VFONT = None
    if SHOW_HTML:
        write_html_report(SHOW_HTML)


if __name__ == "__main__":
    main()
