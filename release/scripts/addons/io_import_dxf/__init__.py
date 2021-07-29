# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
import os
from bpy.props import StringProperty, BoolProperty, EnumProperty, IntProperty, FloatProperty
from .dxfimport.do import Do, Indicator
from .transverse_mercator import TransverseMercator


try:
    from pyproj import Proj, transform
    PYPROJ = True
except:
    PYPROJ = False

bl_info = {
    "name": "Import AutoCAD DXF Format (.dxf)",
    "author": "Lukas Treyer, Manfred Moitzi (support + dxfgrabber library), Vladimir Elistratov, Bastien Montagne",
    "version": (0, 8, 6),
    "blender": (2, 7, 1),
    "location": "File > Import > AutoCAD DXF",
    "description": "Import files in the Autocad DXF format (.dxf)",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Import-Export/DXF_Importer",
    "category": "Import-Export",
}


proj_none_items = (
    ('NONE', "None", "No Coordinate System is available / will be set"),
)
proj_user_items = (
    ('USER', "User Defined", "Define the EPSG code"),
)
proj_tmerc_items = (
    ('TMERC', "Transverse Mercator", "Mercator Projection using a lat/lon coordinate as its geo-reference"),
)
proj_epsg_items = (
    ('EPSG:4326', "WGS84", "World Geodetic System 84; default for lat / lon; EPSG:4326"),
    ('EPSG:3857', "Spherical Mercator", "Webbrowser mapping service standard (Google, OpenStreetMap, ESRI); EPSG:3857"),
    ('EPSG:27700', "National Grid U.K",
                   "Ordnance Survey National Grid reference system used in Great Britain; EPSG:27700"),
    ('EPSG:2154', "France (Lambert 93)", "Lambert Projection for France; EPSG:2154"),
    ('EPSG:5514', "Czech Republic & Slovakia", "Coordinate System for Czech Republic and Slovakia; EPSG:5514"),
    ('EPSG:5243', "LLC Germany", "Projection for Germany; EPSG:5243"),
    ('EPSG:28992', "Amersfoort Netherlands", "Amersfoort / RD New -- Netherlands; EPSG:28992"),
    ('EPSG:21781', "Swiss CH1903 / LV03", "Switzerland and Lichtenstein; EPSG:21781"),
    ('EPSG:5880', "Brazil Polyconic", "Cartesian 2D; Central, South America; EPSG:5880 "),
    ('EPSG:42103', "LCC USA", "Lambert Conformal Conic Projection; EPSG:42103"),
    ('EPSG:3350', "Russia: Pulkovo 1942 / CS63 zone C0", "Russian Federation - onshore and offshore; EPSG:3350"),
    ('EPSG:22293', "Cape / Lo33 South Africa", "South Africa; EPSG:22293"),
    ('EPSG:27200', "NZGD49 / New Zealand Map Grid", "NZGD49 / New Zealand Map Grid; EPSG:27200"),
    ('EPSG:3112', "GDA94 Australia Lambert", "GDA94 / Geoscience Australia Lambert; EPSG:3112"),
    ('EPSG:24378', "India zone I", "Kalianpur 1975 / India zone I; EPSG:24378"),
    ('EPSG:2326', "Hong Kong 1980 Grid System", "Hong Kong 1980 Grid System; EPSG:2326"),
    ('EPSG:3414', "SVY21 / Singapore TM", "SVY21 / Singapore TM; EPSG:3414"),
)

proj_epsg_dict = {e[0]: e[1] for e in proj_epsg_items}

__version__ = '.'.join([str(s) for s in bl_info['version']])

BY_LAYER = 0
BY_DXFTYPE = 1
BY_CLOSED_NO_BULGE_POLY = 2
SEPARATED = 3
LINKED_OBJECTS = 4
GROUP_INSTANCES = 5
BY_BLOCKS = 6

merge_map = {"BY_LAYER": BY_LAYER, "BY_TYPE": BY_DXFTYPE,
             "BY_CLOSED_NO_BULGE_POLY": BY_CLOSED_NO_BULGE_POLY, "BY_BLOCKS": BY_BLOCKS}

T_Merge = True
T_ImportText = True
T_ImportLight = True
T_ExportAcis = False
T_MergeLines = True
T_OutlinerGroups = True
T_Bbox = True
T_CreateNewScene = False
T_Recenter = False
T_ThicknessBevel = True
T_import_atts = True

RELEASE_TEST = False
DEBUG = False


def is_ref_scene(scene):
    return "latitude" in scene and "longitude" in scene


def read(report, filename, obj_merge=BY_LAYER, import_text=True, import_light=True, export_acis=True, merge_lines=True,
         do_bbox=True, block_rep=LINKED_OBJECTS, new_scene=None, recenter=False, projDXF=None, projSCN=None,
         thicknessWidth=True, but_group_by_att=True, dxf_unit_scale=1.0):
    # import dxf and export nurbs types to sat/sab files
    # because that's how autocad stores nurbs types in a dxf...
    do = Do(filename, obj_merge, import_text, import_light, export_acis, merge_lines, do_bbox, block_rep, recenter,
            projDXF, projSCN, thicknessWidth, but_group_by_att, dxf_unit_scale)

    errors = do.entities(os.path.basename(filename).replace(".dxf", ""), new_scene)

    # display errors
    for error in errors:
        report({'ERROR', 'INFO'}, error)

    # inform the user about the sat/sab files
    if len(do.acis_files) > 0:
        report({'INFO'}, "Exported %d NURBS objects to sat/sab files next to your DXF file" % len(do.acis_files))


def display_groups_in_outliner():
    outliners = (a for a in bpy.context.screen.areas if a.type == "OUTLINER")
    for outliner in outliners:
        outliner.spaces[0].display_mode = "GROUPS"


# Update helpers (must be globals to be re-usable).
def _update_use_georeferencing_do(self, context):
    if not self.create_new_scene:
        scene = context.scene
        # Try to get Scene SRID (ESPG) data from current scene.
        srid = scene.get("SRID", None)
        if srid is not None:
            self.internal_using_scene_srid = True
            srid = srid.upper()
            if srid == 'TMERC':
                self.proj_scene = 'TMERC'
                self.merc_scene_lat = scene.get('latitude', 0)
                self.merc_scene_lon = scene.get('longitude', 0)
            else:
                if srid in (p[0] for p in proj_epsg_items):
                    self.proj_scene = srid
                else:
                    self.proj_scene = 'USER'
                    self.epsg_scene_user = srid
        else:
            self.internal_using_scene_srid = False
    else:
        self.internal_using_scene_srid = False


def _recenter_allowed(self):
    scene = bpy.context.scene
    conditional_requirement = self.proj_scene == 'TMERC' if PYPROJ else self.dxf_indi == "SPHERICAL"
    return not (
                    self.use_georeferencing and
                    (
                        conditional_requirement or
                        (not self.create_new_scene and is_ref_scene(scene))
                    )
                    )


def _set_recenter(self, value):
    self.recenter = value if _recenter_allowed(self) else False


def _update_proj_scene_do(self, context):
    # make sure scene EPSG is not None if DXF EPSG is not None
    if self.proj_scene == 'NONE' and self.proj_dxf != 'NONE':
        self.proj_scene = self.proj_dxf


def _update_import_atts_do(self, context):
    mo = merge_map[self.merge_options]
    if mo == BY_CLOSED_NO_BULGE_POLY or mo == BY_BLOCKS:
        self.import_atts = False
        self.represent_thickness_and_width = False
    elif self.represent_thickness_and_width and self.merge:
        self.import_atts = True
    elif not self.merge:
        self.import_atts = False


class IMPORT_OT_dxf(bpy.types.Operator):
    """Import from DXF file format (.dxf)"""
    bl_idname = "import_scene.dxf"
    bl_description = 'Import from DXF file format (.dxf)'
    bl_label = "Import DXf v." + __version__
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_options = {'UNDO'}

    filepath = StringProperty(
            name="input file",
            subtype='FILE_PATH'
            )

    filename_ext = ".dxf"

    filter_glob = StringProperty(
            default="*.dxf",
            options={'HIDDEN'},
            )

    def _update_merge(self, context):
        _update_import_atts_do(self, context)
    merge = BoolProperty(
            name="Merged Objects",
            description="Merge DXF entities to Blender objects",
            default=T_Merge,
            update=_update_merge
            )

    def _update_merge_options(self, context):
        _update_import_atts_do(self, context)

    merge_options = EnumProperty(
            name="Merge",
            description="Merge multiple DXF entities into one Blender object",
            items=[('BY_LAYER', "By Layer", "Merge DXF entities of a layer to an object"),
                   ('BY_TYPE', "By Layer AND DXF-Type", "Merge DXF entities by type AND layer"),
                   ('BY_CLOSED_NO_BULGE_POLY', "By Layer AND closed no-bulge polys", "Polys can have a transformation attribute that makes DXF polys resemble Blender mesh faces quite a bit. Merging them results in one MESH object."),
                   ('BY_BLOCKS', "By Layer AND DXF-Type AND Blocks", "Merging blocks results in all uniformly scaled blocks being referenced by a dupliface mesh instead of object containers. Non-uniformly scaled blocks will be imported as indicated by 'Blocks As'.")],
            default='BY_LAYER',
            update=_update_merge_options
            )

    merge_lines = BoolProperty(
            name="Combine LINE entities to polygons",
            description="Checks if lines are connect on start or end and merges them to a polygon",
            default=T_MergeLines
            )

    import_text = BoolProperty(
            name="Import Text",
            description="Import DXF Text Entities MTEXT and TEXT",
            default=T_ImportText,
            )

    import_light = BoolProperty(
            name="Import Lights",
            description="Import DXF Text Entity LIGHT",
            default=T_ImportLight
            )

    export_acis = BoolProperty(
            name="Export ACIS Entities",
            description="Export Entities consisting of ACIS code to ACIS .sat/.sab files",
            default=T_ExportAcis
            )

    outliner_groups = BoolProperty(
            name="Display Groups in Outliner(s)",
            description="Make all outliners in current screen layout show groups",
            default=T_OutlinerGroups
            )

    do_bbox = BoolProperty(
            name="Parent Blocks to Bounding Boxes",
            description="Create a bounding box for blocks with more than one object (faster without)",
            default=T_Bbox
            )



    block_options = EnumProperty(
            name="Blocks As",
            description="Select the representation of DXF blocks: linked objects or group instances",
            items=[('LINKED_OBJECTS', "Linked Objects", "Block objects get imported as linked objects"),
                   ('GROUP_INSTANCES', "Group Instances", "Block objects get imported as group instances")],
            default='LINKED_OBJECTS',

            )

    def _update_create_new_scene(self, context):
        _update_use_georeferencing_do(self, context)
        _set_recenter(self, self.recenter)
    create_new_scene = BoolProperty(
            name="Import DXF to new scene",
            description="Creates a new scene with the name of the imported file",
            default=T_CreateNewScene,
            update=_update_create_new_scene,
            )

    recenter = BoolProperty(
            name="Center geometry to scene",
            description="Moves geometry to the center of the scene",
            default=T_Recenter,
            )

    def _update_thickness_width(self, context):
        _update_import_atts_do(self, context)
    represent_thickness_and_width = BoolProperty(
            name="Represent line thickness/width",
            description="Map thickness and width of lines to Bevel objects and extrusion attribute",
            default=T_ThicknessBevel,
            update=_update_thickness_width
            )

    import_atts = BoolProperty(
            name="Merge by attributes",
            description="If 'Merge objects' is on but thickness and width are not chosen to be represented, with this "
                        "option object still can be merged by thickness, with, subd and extrusion attributes "
                        "(extrusion = transformation matrix of DXF objects)",
            default=T_import_atts
            )

    # geo referencing

    def _update_use_georeferencing(self, context):
        _update_use_georeferencing_do(self, context)
        _set_recenter(self, self.recenter)
    use_georeferencing = BoolProperty(
            name="Geo Referencing",
            description="Project coordinates to a given coordinate system or reference point",
            default=True,
            update=_update_use_georeferencing,
            )

    def _update_dxf_indi(self, context):
        _set_recenter(self, self.recenter)
    dxf_indi = EnumProperty(
            name="DXF coordinate type",
            description="Indication for spherical or euclidian coordinates",
            items=[('EUCLIDEAN', "Euclidean", "Coordinates in x/y"),
                   ('SPHERICAL', "Spherical", "Coordinates in lat/lon")],
            default='EUCLIDEAN',
            update=_update_dxf_indi,
            )

    # Note: FloatProperty is not precise enough, e.g. 1.0 becomes 0.999999999. Python is more precise here (it uses
    #       doubles internally), so we store it as string here and convert to number with py's float() func.
    dxf_scale = StringProperty(
            name="Unit Scale",
            description="Coordinates are assumed to be in meters; deviation must be indicated here",
            default="1.0"
            )

    def _update_proj(self, context):
        _update_proj_scene_do(self, context)
        _set_recenter(self, self.recenter)
    if PYPROJ:
        pitems = proj_none_items + proj_user_items + proj_epsg_items
        proj_dxf = EnumProperty(
            name="DXF SRID",
            description="The coordinate system for the DXF file (check http://epsg.io)",
            items=pitems,
            default='NONE',
            update=_update_proj,
            )

    epsg_dxf_user = StringProperty(name="EPSG-Code", default="EPSG")
    merc_dxf_lat = FloatProperty(name="Geo-Reference Latitude", default=0.0)
    merc_dxf_lon = FloatProperty(name="Geo-Reference Longitude", default=0.0)

    pitems = proj_none_items + ((proj_user_items + proj_tmerc_items + proj_epsg_items) if PYPROJ else proj_tmerc_items)
    proj_scene = EnumProperty(
            name="Scn SRID",
            description="The coordinate system for the Scene (check http://epsg.io)",
            items=pitems,
            default='NONE',
            update=_update_proj,
            )

    epsg_scene_user = StringProperty(name="EPSG-Code", default="EPSG")
    merc_scene_lat = FloatProperty(name="Geo-Reference Latitude", default=0.0)
    merc_scene_lon = FloatProperty(name="Geo-Reference Longitude", default=0.0)

    # internal use only!
    internal_using_scene_srid = BoolProperty(default=False, options={'HIDDEN'})

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # merge options
        layout.label("Merge Options:")
        box = layout.box()
        sub = box.row()
        #sub.enabled = merge_map[self.merge_options] != BY_BLOCKS
        sub.prop(self, "block_options")
        box.prop(self, "do_bbox")
        box.prop(self, "merge")
        sub = box.row()
        sub.enabled = self.merge
        sub.prop(self, "merge_options")
        box.prop(self, "merge_lines")

        # general options
        layout.label("Line thickness and width:")
        box = layout.box()
        box.enabled = not merge_map[self.merge_options] == BY_CLOSED_NO_BULGE_POLY
        box.prop(self, "represent_thickness_and_width")
        sub = box.row()
        sub.enabled = (not self.represent_thickness_and_width and self.merge)
        sub.prop(self, "import_atts")

        # optional objects
        layout.label("Optional Objects:")
        box = layout.box()
        box.prop(self, "import_text")
        box.prop(self, "import_light")
        box.prop(self, "export_acis")

        # view options
        layout.label("View Options:")
        box = layout.box()
        box.prop(self, "outliner_groups")
        box.prop(self, "create_new_scene")
        sub = box.row()
        sub.enabled = _recenter_allowed(self)
        sub.prop(self, "recenter")

        # geo referencing
        layout.prop(self, "use_georeferencing", text="Geo Referencing:")
        box = layout.box()
        box.enabled = self.use_georeferencing
        self.draw_pyproj(box, context.scene) if PYPROJ else self.draw_merc(box)

    def draw_merc(self, box):
        box.label("DXF File:")
        box.prop(self, "dxf_indi")
        box.prop(self, "dxf_scale")

        sub = box.column()
        sub.enabled = not _recenter_allowed(self)
        sub.label("Geo Reference:")
        sub = box.column()
        sub.enabled = not _recenter_allowed(self)
        if is_ref_scene(bpy.context.scene):
            sub.enabled = False
        sub.prop(self, "merc_scene_lat", text="Lat")
        sub.prop(self, "merc_scene_lon", text="Lon")

    def draw_pyproj(self, box, scene):
        valid_dxf_srid = True

        # DXF SCALE
        box.prop(self, "dxf_scale")

        # EPSG DXF
        box.alert = (self.proj_scene != 'NONE' and (not valid_dxf_srid or self.proj_dxf == 'NONE'))
        box.prop(self, "proj_dxf")
        box.alert = False
        if self.proj_dxf == 'USER':
            try:
                Proj(init=self.epsg_dxf_user)
            except:
                box.alert = True
                valid_dxf_srid = False
            box.prop(self, "epsg_dxf_user")
        box.alert = False

        box.separator()

        # EPSG SCENE
        col = box.column()
        # Only info in case of pre-defined EPSG from current scene.
        if self.internal_using_scene_srid:
            col.enabled = False

        col.prop(self, "proj_scene")

        if self.proj_scene == 'USER':
            try:
                Proj(init=self.epsg_scene_user)
            except Exception as e:
                col.alert = True
            col.prop(self, "epsg_scene_user")
            col.alert = False
            col.label("")  # Placeholder.
        elif self.proj_scene == 'TMERC':
            col.prop(self, "merc_scene_lat", text="Lat")
            col.prop(self, "merc_scene_lon", text="Lon")
        else:
            col.label("")  # Placeholder.
            col.label("")  # Placeholder.

        # user info
        if self.proj_scene != 'NONE':
            if not valid_dxf_srid:
                box.label("DXF SRID not valid", icon="ERROR")
            if self.proj_dxf == 'NONE':
                box.label("", icon='ERROR')
                box.label("DXF SRID must be set, otherwise")
                if self.proj_scene == 'USER':
                    code = self.epsg_scene_user
                else:
                    code = self.proj_scene
                box.label('Scene SRID %r is ignored!' % code)

    def execute(self, context):
        block_map = {"LINKED_OBJECTS": LINKED_OBJECTS, "GROUP_INSTANCES": GROUP_INSTANCES}
        merge_options = SEPARATED
        if self.merge:
            merge_options = merge_map[self.merge_options]
        scene = bpy.context.scene
        if self.create_new_scene:
            scene = bpy.data.scenes.new(os.path.basename(self.filepath).replace(".dxf", ""))

        proj_dxf = None
        proj_scn = None
        dxf_unit_scale = 1.0
        if self.use_georeferencing:
            dxf_unit_scale = float(self.dxf_scale.replace(",", "."))
            if PYPROJ:
                if self.proj_dxf != 'NONE':
                    if self.proj_dxf == 'USER':
                        proj_dxf = Proj(init=self.epsg_dxf_user)
                    else:
                        proj_dxf = Proj(init=self.proj_dxf)
                if self.proj_scene != 'NONE':
                    if self.proj_scene == 'USER':
                        proj_scn = Proj(init=self.epsg_scene_user)
                    elif self.proj_scene == 'TMERC':
                        proj_scn = TransverseMercator(lat=self.merc_scene_lat, lon=self.merc_scene_lon)
                    else:
                        proj_scn = Proj(init=self.proj_scene)
            else:
                proj_dxf = Indicator(self.dxf_indi)
                proj_scn = TransverseMercator(lat=self.merc_scene_lat, lon=self.merc_scene_lon)

        if RELEASE_TEST:
            # for release testing
            from . import test
            test.test()
        else:
            read(self.report, self.filepath, merge_options, self.import_text, self.import_light, self.export_acis,
                 self.merge_lines, self.do_bbox, block_map[self.block_options], scene, self.recenter,
                 proj_dxf, proj_scn, self.represent_thickness_and_width, self.import_atts, dxf_unit_scale)

        if self.outliner_groups:
            display_groups_in_outliner()

        return {'FINISHED'}

    def invoke(self, context, event):
        # Force first update...
        self._update_use_georeferencing(context)

        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(IMPORT_OT_dxf.bl_idname, text="AutoCAD DXF")


def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_import.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_import.remove(menu_func)


if __name__ == "__main__":
    register()
