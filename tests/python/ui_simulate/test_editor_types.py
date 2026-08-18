"""
This file does not run anything; its methods are accessed by run_blender_setup.py.
"""

import math
import modules.ui_test_utils as ui


UNSETTABLE_EDITOR_TYPES = {
    'EMPTY',
    'TOPBAR',
    'STATUSBAR',
}


def editor_variants():
    import bpy

    variants = []

    variant_specs = {
        'NODE_EDITOR': (
            bpy.types.SpaceNodeEditor,
            'tree_type',
            'NODE',
        ),
        'IMAGE_EDITOR': (
            bpy.types.SpaceImageEditor,
            'ui_mode',
            'IMAGE',
        ),
        'DOPESHEET_EDITOR': (
            bpy.types.SpaceDopeSheetEditor,
            'ui_mode',
            'DOPESHEET',
        ),
        'GRAPH_EDITOR': (
            bpy.types.SpaceGraphEditor,
            'mode',
            'GRAPH',
        ),
        'FILE_BROWSER': (
            bpy.types.SpaceFileBrowser,
            'browse_mode',
            'FILE_BROWSER',
        ),
    }

    for item in bpy.types.Area.bl_rna.properties["type"].enum_items:
        area_type = item.identifier

        if area_type in UNSETTABLE_EDITOR_TYPES:
            continue

        # Base variant (default mode/state for this editor type)
        variants.append({
            "name": area_type,
            "type": area_type,
            "settings": {},
        })

        spec = variant_specs.get(area_type)
        if spec is None:
            continue

        rna_type, prop_name, prefix = spec
        prop = rna_type.bl_rna.properties[prop_name]

        for enum_item in prop.enum_items:
            identifier = enum_item.identifier

            # Skip duplicate/default IMAGE_EDITOR mode (already covered above)
            if area_type == 'IMAGE_EDITOR' and identifier == 'VIEW':
                continue

            variants.append({
                "name": f"{prefix}:{identifier}",
                "type": area_type,
                "settings": {prop_name: identifier},
            })

    # The UV editor uses `area.ui_type`, not `space.xxx`. It's the only variant handled this way.
    variants.append({
        "name": "IMAGE:UV",
        "type": 'IMAGE_EDITOR',
        "settings": {"ui_type": 'UV'},
    })

    # Remove exact duplicates (same type + same settings dict)
    seen = set()
    unique = []
    for item in variants:
        key = (item["type"], tuple(sorted(item["settings"].items())))
        if key in seen:
            continue
        seen.add(key)
        unique.append(item)

    return unique


def sorted_grid_areas(screen):
    """
    Return screen areas sorted top-to-bottom, left-to-right,
    so they map predictably onto the variants list.
    """
    return sorted(screen.areas, key=lambda a: (-a.y, a.x))


def apply_variant(area, variant):
    """Set an area's editor type and any sub-mode settings."""
    import bpy
    with bpy.context.temp_override(area=area):
        area.type = variant["type"]
    yield
    with bpy.context.temp_override(area=area):
        space = area.spaces.active
        for key, value in variant["settings"].items():
            # ui_type is a property of Area, everything else is on the Space
            target = area if key == "ui_type" else space
            setattr(target, key, value)


def test_open_editor_types():
    import bpy

    e, t, window = ui.test_window()
    screen = window.screen
    variants = editor_variants()
    count = len(variants)

    # Build a grid with at least `count` areas, yielding after each split
    yield from ui.build_grid(screen, count)

    areas = sorted_grid_areas(screen)

    # Part 1: Apply all editor variants
    apply_failures = set()   # Track names that failed during apply
    part1_failures = []

    for area, variant in zip(areas, variants):
        name = variant["name"]
        try:
            yield from apply_variant(area, variant)
            yield  # Give Blender a tick to process the editor switch

            if len(area.regions) < 1:
                part1_failures.append(f"{name}: no regions after open")
                apply_failures.add(name)

        except Exception as ex:
            part1_failures.append(f"{name}: exception during apply — {repr(ex)}")
            apply_failures.add(name)

    yield

    # Part 2: Validate all successfully opened variants
    part2_failures = []

    for area, variant in zip(areas, variants):
        name = variant["name"]

        # Skip areas that already failed in the apply phase to avoid double-reporting
        if name in apply_failures:
            continue

        # Editor type must have survived
        if area.type != variant["type"]:
            part2_failures.append(
                f"{name}: wrong editor type "
                f"(expected {variant['type']!r}, got {area.type!r})"
            )
            continue

        # Must have at least one region
        if len(area.regions) < 1:
            part2_failures.append(f"{name}: missing regions")
            continue

        # Must have a WINDOW region
        if not any(r.type == 'WINDOW' for r in area.regions):
            part2_failures.append(f"{name}: missing WINDOW region")

        # Each configured setting must have survived the editor switch
        space = area.spaces.active
        for key, value in variant["settings"].items():
            try:
                target = area if key == "ui_type" else space
                actual = getattr(target, key)
                if actual != value:
                    part2_failures.append(
                        f"{name}: setting {key!r} not retained "
                        f"(expected {value!r}, got {actual!r})"
                    )
            except Exception as ex:
                part2_failures.append(
                    f"{name}: error reading setting {key!r} — {repr(ex)}"
                )

    # Combine and report all failures
    all_failures = part1_failures + part2_failures
    t.assertEqual(
        len(all_failures),
        0,
        msg=f"{len(all_failures)} editor variant(s) failed:\n" + "\n".join(all_failures),
    )

    yield
