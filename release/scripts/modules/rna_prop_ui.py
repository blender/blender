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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy

def rna_idprop_ui_get(item, create=True):
    try:
        return item['_RNA_UI']
    except:
        if create:
            item['_RNA_UI'] = {}
            return item['_RNA_UI']
        else:
            return None


def rna_idprop_ui_prop_get(item, prop, create=True):
    
    rna_ui = rna_idprop_ui_get(item, create)
    
    if rna_ui == None:
        return None
    
    try:
        return rna_ui[prop]
    except:
        rna_ui[prop] = {}
        return rna_ui[prop]


def rna_idprop_ui_prop_clear(item, prop):
    rna_ui = rna_idprop_ui_get(item, False)
    
    if rna_ui == None:
        return

    try:
        del rna_ui[prop]
    except:
        pass


def draw(layout, context, context_member, use_edit = True):
    
    def assign_props(prop, val, key):
        prop.path = context_member
        prop.property = key
        
        try:
            prop.value = str(val)
        except:
            pass
    
    rna_item = eval("context." + context_member)

    items = rna_item.items()
    items.sort()
    
    if use_edit:
        row = layout.row()
        props = row.itemO("wm.properties_add", properties=True, text="Add")
        props.path = context_member
        del row

    for key, val in items:

        if key == '_RNA_UI':
            continue
        
        row = layout.row()
        convert_to_pyobject = getattr(val, "convert_to_pyobject", None)
        
        val_orig = val
        if convert_to_pyobject:
            val_draw = val = val.convert_to_pyobject()
            val_draw = str(val_draw)
        else:
            val_draw = val

        box = row.box()

        if use_edit:
            split = box.split(percentage=0.75)
            row = split.row()
        else:
            row = box.row()
        
        row.itemL(text=key)
        
        # explicit exception for arrays
        if convert_to_pyobject and not hasattr(val_orig, "len"):
            row.itemL(text=val_draw)
        else:
            row.itemR(rna_item, '["%s"]' % key, text="")
            
        if use_edit:
            row = split.row(align=True)
            prop = row.itemO("wm.properties_edit", properties=True, text="edit")
            assign_props(prop, val_draw, key)
            
            prop = row.itemO("wm.properties_remove", properties=True, text="", icon='ICON_ZOOMOUT')
            assign_props(prop, val_draw, key)
    

from bpy.props import *


rna_path = StringProperty(name="Property Edit",
    description="Property path edit", maxlen=1024, default="", hidden=True)

rna_value = StringProperty(name="Property Value",
    description="Property value edit", maxlen=1024, default="")

rna_property = StringProperty(name="Property Name",
    description="Property name edit", maxlen=1024, default="")

rna_min = FloatProperty(name="Min", default=0.0, precision=3)
rna_max = FloatProperty(name="Max", default=1.0, precision=3)

class WM_OT_properties_edit(bpy.types.Operator):
    '''Internal use (edit a property path)'''
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property!"

    description = StringProperty(name="Tip", default="")
    path = rna_path
    value = rna_value
    property = rna_property
    
    min = rna_min
    max = rna_max
    
    # the class instance is not persistant, need to store in the class
    # not ideal but changes as the op runs.
    _last_prop = ['']

    def execute(self, context):
        path = self.properties.path
        value = self.properties.value
        prop = self.properties.property
        prop_old = self._last_prop[0]

        try:
            value_eval = eval(value)
        except:
            value_eval = value
        
        if type(value_eval) == str:
            value_eval = '"' + value_eval + '"'        
        
        # First remove
        item = eval("context.%s" % path)
        
        rna_idprop_ui_prop_clear(item, prop_old)
        exec_str = "del item['%s']" % prop_old
        # print(exec_str)
        exec(exec_str)
        
        
        # Reassign
        exec_str = "item['%s'] = %s" % (prop, value_eval)
        # print(exec_str)
        exec(exec_str)
        
        prop_type = type(item[prop])
        
        prop_ui = rna_idprop_ui_prop_get(item, prop)

        if prop_type in (float, int):
            
            prop_ui['soft_min'] = prop_ui['min'] = prop_type(self.properties.min)
            prop_ui['soft_max'] = prop_ui['max'] = prop_type(self.properties.max)
        
        prop_ui['description'] = self.properties.description
            
        return ('FINISHED',)

    def invoke(self, context, event):
        
        self._last_prop[:] = [self.properties.property]
        
        item = eval("context.%s" % self.properties.path)
        
        # setup defaults
        prop_ui = rna_idprop_ui_prop_get(item, self.properties.property, False) # dont create
        if prop_ui:
            self.properties.min = prop_ui.get("min", -1000000000)
            self.properties.max = prop_ui.get("max",  1000000000)
            self.properties.description = prop_ui.get("description",  "")
            
        if 0:
            _message= "PyConsole, press Ctrl+D to unlock the BGE" 
            import sys 
            
            # evaluate commands in current namespace 
            frame= sys._getframe() 
            namespace = frame.f_globals.copy() 
            namespace.update(frame.f_locals) 
             
            import code 

            # Autocomp in python, not as comprehensive as IPython
            import rlcompleter
            
            try: # ick, some pythons dont have this
                import readline
                readline.parse_and_bind("tab: complete")  
            except:
                pass

            code.interact(banner=_message, local=namespace)
        
        wm = context.manager
        wm.invoke_props_popup(self, event)
        return ('RUNNING_MODAL',)


class WM_OT_properties_add(bpy.types.Operator):
    '''Internal use (edit a property path)'''
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"

    path = rna_path

    def execute(self, context):
        item = eval("context.%s" % self.properties.path)
        
        def unique_name(names):
            prop = 'prop'
            prop_new = prop
            i = 1
            while prop_new in names:
                prop_new = prop + str(i)
                i+=1
        
            return prop_new
        
        property = unique_name(item.keys())
        
        item[property] = 1.0
        return ('FINISHED',)
        
class WM_OT_properties_remove(bpy.types.Operator):
    '''Internal use (edit a property path)'''
    bl_idname = "wm.properties_remove"
    bl_label = "Add Property"

    path = rna_path
    property = rna_property

    def execute(self, context):
        item = eval("context.%s" % self.properties.path)
        del item[self.properties.property]
        return ('FINISHED',)        

