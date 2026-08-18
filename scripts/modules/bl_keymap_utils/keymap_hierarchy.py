# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "generate",
)


def _km_expand_from_toolsystem(space_type, context_mode):
    def _fn():
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        for cls in ToolSelectPanelHelper.__subclasses__():
            if cls.bl_space_type == space_type:
                return cls.keymap_ui_hierarchy(context_mode)
        raise Exception("keymap not found")
    return _fn


def _km_hierarchy_iter_recursive(items):
    for sub in items:
        if callable(sub):
            yield from sub()
        else:
            yield (*sub[:3], list(_km_hierarchy_iter_recursive(sub[3])))


def generate():
    import bpy

    if bpy.app.background:
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        for cls in ToolSelectPanelHelper.__subclasses__():
            cls.register_ensure()

    return list(_km_hierarchy_iter_recursive(_km_hierarchy))


# bpy.type.KeyMap: (km.name, km.space_type, km.region_type, [...])

#    ('Script', 'EMPTY', 'WINDOW', []),


# Access via 'km_hierarchy'.
_km_hierarchy = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # re-sizing, action corners
        ('Region Context Menu', 'EMPTY', 'WINDOW', []),      # header/footer/navigation_bar stuff (per region)
    ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []),  # view 2d with buttons navigation

    ('User Interface', 'EMPTY', 'WINDOW', []),
    ('Toolbar Popup', 'EMPTY', 'TEMPORARY', []),  # tool switching popup

    ('3D View', 'VIEW_3D', 'WINDOW', [  # view 3d navigation and generic stuff (select, transform)
        ('Object Mode', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'OBJECT'),
        ]),
        ('Mesh', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_MESH'),
        ]),
        ('Curve', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_CURVE'),
        ]),
        ('Curves', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_CURVES'),
        ]),
        ('Armature', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_ARMATURE'),
        ]),
        ('Metaball', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_METABALL'),
        ]),
        ('Lattice', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_LATTICE'),
        ]),
        ('Font', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_TEXT'),
        ]),
        ('Grease Pencil', 'EMPTY', 'WINDOW', []),
        ('Point Cloud', 'EMPTY', 'WINDOW', []),
        ('Pose', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'POSE'),
        ]),

        ('Vertex Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_VERTEX'),
        ]),
        ('Weight Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_WEIGHT'),
        ]),
        ('Paint Vertex Selection (Weight, Vertex)', 'EMPTY', 'WINDOW', []),
        ('Paint Face Mask (Weight, Vertex, Texture)', 'EMPTY', 'WINDOW', []),
        # image and view3d
        ('Image Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_TEXTURE'),
        ]),
        ('Sculpt', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'SCULPT'),
        ]),

        ('Sculpt Curves', 'EMPTY', 'WINDOW', []),

        ('Particle', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PARTICLE'),
        ]),

        ('Primitive Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Pen Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Knife Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Custom Normals Modal Map', 'EMPTY', 'WINDOW', []),
        ('Bevel Modal Map', 'EMPTY', 'WINDOW', []),
        ('Paint Stroke Modal', 'EMPTY', 'WINDOW', []),
        ('Sculpt Expand Modal', 'EMPTY', 'WINDOW', []),
        ('Paint Curve', 'EMPTY', 'WINDOW', []),
        ('Curve Pen Modal Map', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []),  # mode change

        ('View3D Placement Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Walk Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Dolly Modal', 'EMPTY', 'WINDOW', []),
        ('View3D VR Location Scouting Capture Review Modal', 'EMPTY', 'WINDOW', []),

        # toolbar and properties
        ('3D View Generic', 'VIEW_3D', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', None),
        ]),
    ]),

    ('Graph Editor', 'GRAPH_EDITOR', 'WINDOW', [
        ('Graph Editor Generic', 'GRAPH_EDITOR', 'WINDOW', []),
    ]),
    ('Dopesheet', 'DOPESHEET_EDITOR', 'WINDOW', [
        ('Dopesheet Generic', 'DOPESHEET_EDITOR', 'WINDOW', []),
    ]),
    ('NLA Editor', 'NLA_EDITOR', 'WINDOW', [
        ('NLA Tracks', 'NLA_EDITOR', 'WINDOW', []),
        ('NLA Generic', 'NLA_EDITOR', 'WINDOW', []),
    ]),

    ('Image', 'IMAGE_EDITOR', 'WINDOW', [
        # Image (reverse order, UVEdit before Image).
        ('UV Editor', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('IMAGE_EDITOR', 'UV'),
        ]),
        # Image and view3d.
        ('Image Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('IMAGE_EDITOR', 'PAINT'),
        ]),
        _km_expand_from_toolsystem('IMAGE_EDITOR', 'VIEW'),
        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', [
            _km_expand_from_toolsystem('IMAGE_EDITOR', None),
        ]),
    ]),

    ('Outliner', 'OUTLINER', 'WINDOW', []),

    ('Node Editor', 'NODE_EDITOR', 'WINDOW', [
        ('Node Generic', 'NODE_EDITOR', 'WINDOW', [
            # Nodes don't use modes to access different tools, so there is only ever a single context.
            _km_expand_from_toolsystem('NODE_EDITOR', None),
        ]),
        ('Node Link Modal Map', 'EMPTY', 'WINDOW', []),
        ('Node Resize Modal Map', 'EMPTY', 'WINDOW', []),
    ]),

    ('Spreadsheet Generic', 'SPREADSHEET', 'WINDOW', []),

    ('Video Sequence Editor', 'SEQUENCE_EDITOR', 'WINDOW', [
        ('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', [
            _km_expand_from_toolsystem('SEQUENCE_EDITOR', 'SEQUENCER'),
        ]),
        ('Preview', 'SEQUENCE_EDITOR', 'WINDOW', [
            _km_expand_from_toolsystem('SEQUENCE_EDITOR', 'PREVIEW'),
        ]),
        ('Sequencer Channels', 'SEQUENCE_EDITOR', 'WINDOW', []),
        ('Slip Modal', 'EMPTY', 'WINDOW', []),
    ]),

    ('File Browser', 'FILE_BROWSER', 'WINDOW', [
        ('File Browser Main', 'FILE_BROWSER', 'WINDOW', []),
        ('File Browser Buttons', 'FILE_BROWSER', 'WINDOW', []),
    ]),

    ('Info', 'INFO', 'WINDOW', []),

    ('Property Editor', 'PROPERTIES', 'WINDOW', []),  # align context menu

    ('Preferences', 'PREFERENCES', 'WINDOW', [
        ('Preferences_nav', 'PREFERENCES', 'UI', []),  # navigation bar
    ]),

    ('Text', 'TEXT_EDITOR', 'WINDOW', [
        ('Text Generic', 'TEXT_EDITOR', 'WINDOW', []),
    ]),
    ('Console', 'CONSOLE', 'WINDOW', []),
    ('Clip', 'CLIP_EDITOR', 'WINDOW', [
        ('Clip Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Graph Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Dopesheet Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Time Scrub', 'CLIP_EDITOR', 'PREVIEW', []),
    ]),

    ('Grease Pencil', 'EMPTY', 'WINDOW', [
        # Grease Pencil
        ('Grease Pencil Draw Mode', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_GREASE_PENCIL'),
        ]),
        ('Grease Pencil Brush Stroke', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Edit Mode', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_GREASE_PENCIL'),
        ]),
        ('Grease Pencil Sculpt Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Weight Paint', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Vertex Paint', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Selection', 'EMPTY', 'WINDOW', []),
        # Grease Pencil Fill Tool
        ('Grease Pencil Fill Tool', 'EMPTY', 'WINDOW', []),
    ]),
    ('Mask Editing', 'EMPTY', 'WINDOW', [
        _km_expand_from_toolsystem('IMAGE_EDITOR', 'MASK'),
    ]),
    ('Frames', 'EMPTY', 'WINDOW', []),    # frame navigation (per region)
    ('Markers', 'EMPTY', 'WINDOW', []),    # markers (per region)
    ('Animation', 'EMPTY', 'WINDOW', []),    # frame change on click, preview range (per region)
    ('Animation Channels', 'EMPTY', 'WINDOW', []),
    ('Time Scrub', 'EMPTY', 'WINDOW', []),    # frame change by dragging the scrub-bar (per region)

    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
    ('Gesture Straight Line', 'EMPTY', 'WINDOW', []),
    ('Gesture Zoom Border', 'EMPTY', 'WINDOW', []),
    ('Gesture Box', 'EMPTY', 'WINDOW', []),
    ('Gesture Lasso', 'EMPTY', 'WINDOW', []),
    ('Gesture Polyline', 'EMPTY', 'WINDOW', []),

    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Axis Target Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper ColorRamp PointSampling Map', 'EMPTY', 'WINDOW', []),
    ('Mesh Filter Modal Map', 'EMPTY', 'WINDOW', []),

    # Grease Pencil Fill Tool
    ('Fill Tool Modal Map', 'EMPTY', 'WINDOW', []),
    # Grease Pencil Interpolate Tool
    ('Interpolate Tool Modal Map', 'EMPTY', 'WINDOW', []),

    ('Generic Gizmo', 'EMPTY', 'WINDOW', [
        ('Generic Gizmo Drag', 'EMPTY', 'WINDOW', []),
        ('Generic Gizmo Click Drag', 'EMPTY', 'WINDOW', []),
        ('Generic Gizmo Maybe Drag', 'EMPTY', 'WINDOW', []),
        ('Generic Gizmo Select', 'EMPTY', 'WINDOW', []),
        ('Generic Gizmo Tweak Modal Map', 'EMPTY', 'WINDOW', []),
    ]),
]
