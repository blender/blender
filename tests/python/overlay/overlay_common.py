# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import os
from pathlib import Path
import argparse

"""Common functionality for Overlay render tests.

The intended usage is setting up a Permutations instance containing all the variants/permutations for a given blend file,
then passing it to the `run_test(permutations)` function.

The `run_test` function also checks for a `--test` argument in `sys.argv`.
When set, it will reproduce the state of a given test number.
So:
`blender "(...)/tests/data/overlay/<test>.blend" -P "(...)/tests/python/overlay/<test>.py" -- --test <test-number>`
will open the blend file and set its state to the <test-number> permutation, instead of running the tests.

Common permutations can also be declared inside this file.
See `ob_modes_permutations` for an example on how to setup permutations.
"""


class Permutations:
    """Container class for the permutations of a given test file.

    Can be combined with other Permutations.
    """

    def __init__(self, reset_key=None, variants_dict={}):
        """Setup the initial set of permutations.

        :reset_key: str - The variants_dict key that resets the test to its default state.
        :variants_dict: {str: lambda}
            - Where the key is the name of the state variation (for example "xray-on" or "xray-off")
              and the lambda sets the blend to that state.
        """
        reset = []
        if reset_key:
            reset = [variants_dict[reset_key]]
        self.dict = {k: Permutation([v], reset) for k, v in variants_dict.items()}

    def add(self, key_filter_cb=None, permutations_array=[]):
        """Combine two sets of permutations.

        Replaces the current permutations with every possible combination of current and incoming permutations,
        unless key_filter_cb is set and returns False for a given key, in which case they're kept as-is.

        :key_filter_cb: lambda (key: str): :bool:
            - A callback for deciding which keys the permutations should be applied to.
        :permutations: [Permutations]
            - An array of Permutations instances. Each Permutations instance is applied sequentially,
              so a.add(None, [b]); a.add(None, [c]) is equivalent to a.add(None, [b, c])
        """
        for permutations in permutations_array:
            dict_copy = self.dict.copy()
            self.dict = {}
            for key, permutation in dict_copy.items():
                if key_filter_cb and not key_filter_cb(key):
                    self.dict[key] = permutation
                    continue
                for key_in, permutation_in in permutations.dict.items():
                    self.dict[f"{key}_{key_in}"] = permutation.combine(permutation_in)

    def loop(self):
        """Loop through each permutation, applying its state before yielding its key."""
        for key, permutation in self.dict.items():
            permutation.apply()
            yield key
            permutation.reset()


class Permutation:
    """A single test permutation.

    A permutation containing all the callbacks needed for setting up its state (_apply),
    and for resetting the test back to its default state (_reset).

    This class is meant to be used internally by the Permutations class.
    """

    def __init__(self, apply, reset):
        self._apply = apply
        self._reset = reset

    def combine(self, other):
        return Permutation(self._apply + other._apply, self._reset + other._reset)

    def apply(self):
        for cb in self._apply:
            cb()

    def reset(self):
        for cb in self._reset:
            cb()


def set_permutation_from_args(permutations):
    """If the command line requested a specific permutation, set the blend state to it."""
    import sys
    if "--" not in sys.argv:
        return False
    parser = argparse.ArgumentParser()
    parser.add_argument("--test", type=int, default=0, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    if args.test == 0:
        return False
    else:
        key = list(permutations.dict.keys())[args.test]
        print(f"Set test permutation {args.test}: {key}")
        permutations.dict[key].apply()
        return True


def render_permutations(permutations):
    """Render each permutation and generate a list, following the conventions expected by render_report.py."""
    base_output_path = bpy.context.scene.render.filepath
    base_testname = Path(bpy.data.filepath).stem
    output_paths = []
    permutation_index = 0
    for key in permutations.loop():
        permutation_index += 1
        testname = f"{permutation_index:04d}_{key}"
        filepath = f"{base_output_path}_{testname}0001.png"
        testpath = f"{base_testname}_{testname}"
        output_paths.append(testpath)
        bpy.context.scene.render.filepath = filepath
        bpy.ops.render.opengl(write_still=True, view_context=True)

    output_list_txt = bpy.data.filepath.replace(".blend", "_permutations.txt")
    with open(output_list_txt, 'w') as file:
        file.write("\n".join(output_paths))


def run_test(permutations):
    """Check if the command line requested a specific permutation, otherwise run all tests and quit Blender."""
    if set_permutation_from_args(permutations):
        return

    def run():
        render_permutations(permutations)
        bpy.ops.wm.quit_blender()

    bpy.app.timers.register(run, first_interval=1)


def ob_modes_permutations(ob, space):
    """Returns permutations for every possible object mode and overlay settings."""
    shading = space.shading
    overlay = space.overlay

    ob_modes = Permutations(None, {
        "object": lambda: bpy.ops.object.mode_set(mode='OBJECT'),
        "edit": lambda: bpy.ops.object.mode_set(mode='EDIT'),
        "sculpt": lambda: bpy.ops.object.mode_set(mode='SCULPT'),
        "vertex-paint": lambda: bpy.ops.object.mode_set(mode='VERTEX_PAINT'),
        "weight-paint": lambda: bpy.ops.object.mode_set(mode='WEIGHT_PAINT'),
        "texture-paint": lambda: bpy.ops.object.mode_set(mode='TEXTURE_PAINT'),
    })

    ob_modes.add(lambda key: "sculpt" in key, [
        Permutations("mask-off", {
            "mask-off": lambda: setattr(overlay, "show_sculpt_mask", False),
            "mask-on": lambda: setattr(overlay, "show_sculpt_mask", True),
        }),
        Permutations("sets-off", {
            "sets-off": lambda: setattr(overlay, "show_sculpt_face_sets", False),
            "sets-on": lambda: setattr(overlay, "show_sculpt_face_sets", True),
        }),
    ])

    ob_modes.add(lambda key: "paint" in key, [
        Permutations("mask-off", {
            "mask-off": lambda: (
                setattr(ob.data, "use_paint_mask", False), setattr(ob.data, "use_paint_mask_vertex", False)),
            "mask-face": lambda: (
                setattr(ob.data, "use_paint_mask", True), setattr(ob.data, "use_paint_mask_vertex", False)),
            "mask-vert": lambda: (
                setattr(ob.data, "use_paint_mask", False), setattr(ob.data, "use_paint_mask_vertex", True)),
        }),
        Permutations("paint-wire-off", {
            "paint-wire-off": lambda: setattr(overlay, "show_paint_wire", False),
            "paint-wire-on": lambda: setattr(overlay, "show_paint_wire", True),
        }),
    ])

    ob_modes.add(lambda key: "weight-paint" in key, [
        Permutations("w-contours-off", {
            "w-contours-off": lambda: setattr(overlay, "show_wpaint_contours", False),
            "w-contours-on": lambda: setattr(overlay, "show_wpaint_contours", True),
        })
    ])

    # TODO: Edit mode variants.

    shading_modes = Permutations("solid", {
        "solid": lambda: setattr(shading, "type", 'SOLID'),
        "wireframe": lambda: setattr(shading, "type", 'WIREFRAME'),
    })

    shading_modes.add(lambda key: key == "solid", [
        Permutations("xray-off", {
            "xray-off": lambda: (
                setattr(shading, "show_xray", False)),
            "xray-on": lambda: (
                setattr(shading, "show_xray", True), setattr(shading, "xray_alpha", 0.5)),
            "xray-on-alpha-1": lambda: (
                setattr(shading, "show_xray", True), setattr(shading, "xray_alpha", 1.0)),
        })
    ])

    shading_modes.add(lambda key: key == "wireframe", [
        Permutations("xray-off", {
            "xray-off": lambda: (
                setattr(shading, "show_xray_wireframe", False)),
            "xray-on": lambda: (
                setattr(shading, "show_xray_wireframe", True), setattr(shading, "xray_alpha_wireframe", 0.5)),
            "xray-on-alpha-1": lambda: (
                setattr(shading, "show_xray_wireframe", True), setattr(shading, "xray_alpha_wireframe", 1.0)),
        })
    ])

    shading_modes.add(lambda key: "solid" in key, [
        Permutations("ob-solid", {
            "ob-solid": lambda: setattr(ob, "display_type", 'SOLID'),
            "ob-wire": lambda: setattr(ob, "display_type", 'WIRE'),
        })
    ])

    shading_modes.add(lambda key: "ob-solid" in key, [
        Permutations("ob-wire-off", {
            "ob-wire-off": lambda: setattr(ob, "show_wire", False),
            "ob-wire-on": lambda: setattr(ob, "show_wire", True),
        })
    ])

    in_front_modes = Permutations("in-front-off", {
        "in-front-off": lambda: setattr(ob, "show_in_front", False),
        "in-front-on": lambda: setattr(ob, "show_in_front", True),
    })

    ob_modes.add(None, [shading_modes, in_front_modes])

    return ob_modes
