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

"""
This module translates XML into blender/ui function calls.
"""

import xml.dom.minidom
import bpy as _bpy

def parse_rna(prop, value):
    if prop.type == 'FLOAT':
        value = float(value)
    elif prop.type == 'INT':
        value = int(value)
    elif prop.type == 'BOOLEAN':
        if value not in ("true", "false"):
            raise Exception("invalid bool value: %s", value)
        value = bool(value == "true")
    elif prop.type in ('STRING', 'ENUM'):
        pass
    elif prop.type == 'POINTER':
        value = eval("_bpy." + value)
    else:
        raise Exception("type not supported %s.%s" % (prop.identifier, prop.type))
    return value
    
def parse_args(base, xml_node):
    args = {}
    rna_params = base.bl_rna.functions[xml_node.tagName].parameters
    for key, value in xml_node.attributes.items():
        args[key] = parse_rna(rna_params[key], value)
    return args

def ui_xml(base, xml_node):
    name = xml_node.tagName
    prop = base.bl_rna.properties.get(name)
    if name in base.bl_rna.properties:
        attr = xml_node.attributes.get("expr")
        if attr:
            value = attr.value
            value = eval(value, {"context": _bpy.context})
            setattr(base, name, value)
        else:
            attr = xml_node.attributes['value']
            value = attr.value
            value = parse_rna(prop, value)
            setattr(base, name, value)
    else:
        func_new = getattr(base, name)
        kw_args = parse_args(base, xml_node)
        base_new = func_new(**kw_args) # call blender func
        if xml_node.hasChildNodes():
            ui_xml_list(base_new, xml_node.childNodes)

def ui_xml_list(base, xml_nodes):
    import bpy
    for node in xml_nodes:
        if node.nodeType not in (node.TEXT_NODE, node.COMMENT_NODE):
            ui_xml(base, node)
            bpy.N = node

def test(layout):
    uixml = xml.dom.minidom.parseString(open("/mnt/test/blender-svn/blender/release/scripts/ui/test.xml", 'r').read())
    panel = uixml.getElementsByTagName('panel')[0]
    ui_xml_list(layout, panel.childNodes)

def load_xml(filepath):
    classes = []
    fn = open(filepath, 'r')
    data = fn.read()
    uixml = xml.dom.minidom.parseString(data).getElementsByTagName("ui")[0]
    fn.close()
    
    def draw_xml(self, context):
        node = self._xml_node.getElementsByTagName("draw")[0]
        ui_xml_list(self.layout, node.childNodes)
        
    def draw_header_xml(self, context):
        node = self._xml_node.getElementsByTagName("draw_header")[0]
        ui_xml_list(self.layout, node.childNodes)
    
    for node in uixml.childNodes:
        if node.nodeType not in (node.TEXT_NODE, node.COMMENT_NODE):
            name = node.tagName
            class_name = node.attributes["identifier"].value

            if name == "panel":
                class_dict = {
                    "bl_label": node.attributes["label"].value,
                    "bl_region_type": node.attributes["region_type"].value,
                    "bl_space_type": node.attributes["space_type"].value,
                    "bl_context": node.attributes["context"].value,
                    "bl_default_closed": ((node.attributes["default_closed"].value == "true") if "default_closed" in node.attributes else False),

                    "draw": draw_xml,
                    "_xml_node": node
                }
                
                if node.getElementsByTagName("draw_header"):
                    class_dict["draw_header"] = draw_header_xml

                # will register instantly
                class_new = type(class_name, (_bpy.types.Panel,), class_dict)

            elif name == "menu":
                class_dict = {
                    "bl_label": node.attributes["label"].value,

                    "draw": draw_xml,
                    "_xml_node": node
                }

                # will register instantly
                class_new = type(class_name, (_bpy.types.Menu,), class_dict)

            elif name == "header":
                class_dict = {
                    "bl_label": node.attributes["label"].value,
                    "bl_space_type": node.attributes["space_type"].value,

                    "draw": draw_xml,
                    "_xml_node": node
                }

                # will register instantly
                class_new = type(class_name, (_bpy.types.Header,), class_dict)
            else:
                raise Exception("invalid id found '%s': expected a value in ('header', 'panel', 'menu)'" % name)

            classes.append(class_new)
            

    return classes
