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

import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
    StringProperty,
)

class DXFExporter(bpy.types.Operator):
    """
    Export to the Autocad model format (.dxf)
    """
    bl_idname = "export.dxf"
    bl_label = "Export DXF"
    filepath = StringProperty(subtype='FILE_PATH')

    entitylayer_from_items = (
        ('default_LAYER', 'Default Layer', ''),
        ('obj.name', 'Object name', ''),
        ('obj.layer', 'Object layer', ''),
        ('obj.material', 'Object material', ''),
        ('obj.data.name', 'Object\' data name', ''),
#        ('obj.data.material', 'Material of data', ''),
        ('..vertexgroup', 'Vertex Group', ''),
        ('..group', 'Group', ''),
        ('..map_table', 'Table', '')
    )
    layerColorFromItems = (
        ('default_COLOR', 'Vertex Group', ''),
        ('BYLAYER', 'BYLAYER', ''),
        ('BYBLOCK', 'BYBLOCK', ''),
        ('obj.layer', 'Object Layer', ''),
        ('obj.color', 'Object Color', ''),
        ('obj.material', 'Object material', ''),
        # I don'd know ?
#        ('obj.data.material', 'Vertex Group', ''),
#        ('..map_table', 'Vertex Group', ''),
    )
    layerNameFromItems = (
        ('LAYERNAME_DEF', 'Default Name', ''),
        ('drawing_name', 'From Drawing name', ''),
        ('scene_name', 'From scene name', '')
    )

    exportModeItems = (
        ('ALL', 'All Objects', ''),
        ('SELECTION', 'Selected Objects', ''),
    )
#    spaceItems = (
#        ('1', 'Paper-Space', ''),
#        ('2', 'Model-Space', '')
#    )

    entityltype_fromItems = (
        ('default_LTYPE', 'default_LTYPE', ''),
        ('BYLAYER', 'BYLAYER', ''),
        ('BYBLOCK', 'BYBLOCK', ''),
        ('CONTINUOUS', 'CONTINUOUS', ''),
        ('DOT', 'DOT', ''),
        ('DASHED', 'DASHED', ''),
        ('DASHDOT', 'DASHDOT', ''),
        ('BORDER', 'BORDER', ''),
        ('HIDDEN', 'HIDDEN', '')
    )
    material_toItems = (
        ('NO', 'Do not export', ''),
        ('COLOR', 'COLOR', ''),
        ('LAYER', 'LAYER', ''),
        ('..LINESTYLE', '..LINESTYLE', ''),
        ('..BLOCK', '..BLOCK', ''),
        ('..XDATA', '..XDATA', ''),
        ('..INI-File', '..INI-File', '')
    )
    projectionItems=(
        ('NO', 'No projection', 'Export 3D scene without any 2D projection'),
        ('TOP', 'TOP view', 'Use TOP view for projection'),
        ('BOTTOM', 'BOTTOM view', 'Use BOTTOM view for projection'),
        ('LEFT', 'LEFT view', 'Use LEFT view for projection'),
        ('RIGHT', 'RIGHT view', 'Use RIGHT view for projection'),
        ('FRONT', 'FRONT view', 'Use FRONT view for projection'),
        ('REAR', 'REAR view', 'Use REAR view for projection')
    )
    mesh_asItems = (
        ('NO', 'Do not export', ''),
        ('3DFACEs', '3DFACEs', ''),
        ('POLYFACE', 'POLYFACE', ''),
        ('POLYLINE', 'POLYLINE', ''),
        ('LINEs', 'LINEs', 'export Mesh as multiple LINEs'),
        ('POINTs', 'POINTs', 'Export Mesh as multiple POINTs')
    )
#    curve_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('LINEs', 'LINEs', ''),
#        ('POLYLINE', 'POLYLINE', ''),
#        ('..LWPOLYLINE r14', '..LWPOLYLINE r14', ''),
#        ('..SPLINE r14', '..SPLINE r14', ''),
#        ('POINTs', 'POINTs',  '')
#    )
#    surface_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('..3DFACEs', '..3DFACEs', ''),
#        ('..POLYFACE', '..POLYFACE', ''),
#        ('..POINTs', '..POINTs', ''),
#        ('..NURBS', '..NURBS', '')
#    )
#    meta_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('..3DFACEs', '..3DFACEs', ''),
#        ('..POLYFACE', '..POLYFACE', ''),
#        ('..3DSOLID', '..3DSOLID', '')
#    )
#    text_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('TEXT', 'TEXT', ''),
#        ('..MTEXT', '..MTEXT', ''),
#        ('..ATTRIBUT', '..ATTRIBUT', '')
#    )
#    empty_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('POINT', 'POINT', ''),
#        ('..INSERT', '..INSERT', ''),
#        ('..XREF', '..XREF', '')
#    )
#    group_asItems  = (
#        ('NO', 'Do not export', ''),
#        ('..GROUP', '..GROUP', ''),
#        ('..BLOCK', '..BLOCK', ''),
#        ('..ungroup', '..ungroup', '')
#    )
##    parent_asItems = ['..BLOCK','..ungroup'] # ???
#    proxy_asItems = (
#        ('NO', 'Do not export', ''),
#        ('..BLOCK','..BLOCK', ''),
#        ('..XREF', '..XREF', ''),
#        ('..ungroup', '..ungroup', ''),
#        ('..POINT', '..POINT', '')
#    )
#    camera_asItems = (
#        ('NO', 'Do not export', ''),
#        ('..BLOCK', '..BLOCK', ''),
#        ('..A_CAMERA', '..A_CAMERA', ''),
#        ('VPORT', 'VPORT', ''),
#        ('VIEW', 'VIEW', ''),
#        ('POINT', 'POINT', '')
#    )
#    lamp_asItems = (
#        ('NO', 'Do not export', ''),
#        ('..BLOCK', '..BLOCK', ''),
#        ('..A_LAMP', '..A_LAMP', ''),
#        ('POINT', 'POINT', '')
#    )
    # --------- CONTROL PROPERTIES --------------------------------------------
    projectionThrough = EnumProperty(name="Projection", default="NO",
                                    description="Select camera for use to 2D projection",
                                    items=projectionItems)

    onlySelected = BoolProperty(name="Only selected", default=True,
                              description="What object will be exported? Only selected / all objects")

    apply_modifiers = BoolProperty(name="Apply modifiers", default=True,
                           description="Shall be modifiers applied during export?")
    # GUI_B -----------------------------------------
    mesh_as = EnumProperty( name="Export Mesh As", default='3DFACEs',
                            description="Select representation of a mesh",
                            items=mesh_asItems)
#    curve_as = EnumProperty( name="Export Curve As:", default='NO',
#                            description="Select representation of a curve",
#                            items=curve_asItems)
#    surface_as = EnumProperty( name="Export Surface As:", default='NO',
#                            description="Select representation of a surface",
#                            items=surface_asItems)
#    meta_as = EnumProperty( name="Export meta As:", default='NO',
#                            description="Select representation of a meta",
#                            items=meta_asItems)
#    text_as = EnumProperty( name="Export text As:", default='NO',
#                            description="Select representation of a text",
#                            items=text_asItems)
#    empty_as = EnumProperty( name="Export empty As:", default='NO',
#                            description="Select representation of a empty",
#                            items=empty_asItems)
#    group_as = EnumProperty( name="Export group As:", default='NO',
#                            description="Select representation of a group",
#                            items=group_asItems)
##    parent_as = EnumProperty( name="Export parent As:", default='NO',
##                            description="Select representation of a parent",
##                            items=parent_asItems)
#    proxy_as = EnumProperty( name="Export proxy As:", default='NO',
#                            description="Select representation of a proxy",
#                            items=proxy_asItems)
#    camera_as = EnumProperty( name="Export camera As:", default='NO',
#                            description="Select representation of a camera",
#                            items=camera_asItems)
#    lamp_as = EnumProperty( name="Export lamp As:", default='NO',
#                            description="Select representation of a lamp",
#                            items=lamp_asItems)
    # ----------------------------------------------------------
    entitylayer_from = EnumProperty(name="Entity Layer", default="obj.data.name",
                                    description="Entity LAYER assigned to?",
                                    items=entitylayer_from_items)
    entitycolor_from = EnumProperty(name="Entity Color", default="default_COLOR",
                                    description="Entity COLOR assigned to?",
                                    items=layerColorFromItems)
    entityltype_from = EnumProperty(name="Entity Linetype", default="CONTINUOUS",
                                    description="Entity LINETYPE assigned to?",
                                    items=entityltype_fromItems)

    layerName_from = EnumProperty(name="Layer Name", default="LAYERNAME_DEF",
                                    description="From where will layer name be taken?",
                                    items=layerNameFromItems)
    # GUI_A -----------------------------------------
#    layFrozen_on = BoolProperty(name="LAYER.frozen status", description="(*todo) Support LAYER.frozen status   on/off", default=False)
#    materialFilter_on = BoolProperty(name="Material filtering", description="(*todo) Material filtering   on/off", default=False)
#    colorFilter_on = BoolProperty(name="Color filtering", description="(*todo) Color filtering   on/off", default=False)
#    groupFilter_on = BoolProperty(name="Group filtering", description="(*todo) Group filtering   on/off", default=False)
#    objectFilter_on = BoolProperty(name="Object filtering", description="(*todo) Object filtering   on/off", default=False)
#    paper_space_on = EnumProperty(name="Space of export:", default="2",
#                                    description="Select space that will be taken for export.",
#                                    items=spaceItems)
#    material_to = EnumProperty(name="Material assigned to?:", default="NO",
#                                    description="Material assigned to?.",
#                                    items=material_toItems)

#    prefix_def = StringProperty(name="Prefix for LAYERs", default="DX_",
#                                    description='Type Prefix for LAYERs')
#    layername_def = StringProperty(name="default LAYER name", default="DEF_LAY",
#                                    description='Type default LAYER name')
#    layercolor_def = StringProperty(name="Default layer color:", default="1",
#                                    description='Set default COLOR. (0=BYBLOCK,256=BYLAYER)')
#    layerltype_def = StringProperty(name="Default LINETYPE", default="DEF_LAY_TYPE",
#                                    description='Set default LINETYPE')

    verbose = BoolProperty(name="Verbose", default=False,
                           description="Run the exporter in debug mode.  Check the console for output")

    def execute(self, context):
        filePath = bpy.path.ensure_ext(self.filepath, ".dxf")
        config = {
            'projectionThrough' : self._checkNO(self.projectionThrough),
            'onlySelected' : self.onlySelected,
            'apply_modifiers' : self.apply_modifiers,
            # GUI B
            'mesh_as' : self._checkNO(self.mesh_as),
#            'curve_as' : self._checkNO(self.curve_as),
#            'surface_as' : self._checkNO(self.surface_as),
#            'meta_as' : self._checkNO(self.meta_as),
#            'text_as' : self._checkNO(self.text_as),
#            'empty_as' : self._checkNO(self.empty_as),
#            'group_as' : self._checkNO(self.group_as),
#            'proxy_as' : self._checkNO(self.proxy_as),
#            'camera_as' : self._checkNO(self.camera_as),
#            'lamp_as' : self._checkNO(self.lamp_as),

            'entitylayer_from' : self.entitylayer_from,
            'entitycolor_from' : self.entitycolor_from,
            'entityltype_from' : self.entityltype_from,
            'layerName_from' : self.layerName_from,

            # NOT USED
#            'layFrozen_on' : self.layFrozen_on,
#            'materialFilter_on' : self.materialFilter_on,
#            'colorFilter_on' : self.colorFilter_on,
#            'groupFilter_on' : self.groupFilter_on,
#            'objectFilter_on' : self.objectFilter_on,
#            'paper_space_on' : self.paper_space_on,
#            'layercolor_def' : self.layercolor_def,
#            'material_to' : self.material_to,

            'verbose' : self.verbose
        }

        from .export_dxf import exportDXF
        exportDXF(context, filePath, config)
        return {'FINISHED'}

    def _checkNO(self, val):
        if val == 'NO': return None
        else: return val

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = bpy.path.ensure_ext(bpy.data.filepath, ".dxf")
        WindowManager = context.window_manager
        WindowManager.fileselect_add(self)
        return {'RUNNING_MODAL'}


