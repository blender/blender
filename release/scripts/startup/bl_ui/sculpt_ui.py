import bpy
import os
import sys
from bpy.props import *

from bl_ui.space_toolsystem_common import ToolSelectPanelHelper

class WM_MT_button_context(bpy.types.Menu):
    bl_label = "Unused"

    def draw(self, context):
        pass

def getToolSlotName(brush):
    slot = brush.rna_type.properties["sculpt_tool"].enum_items
    return "builtin_brush." + slot[brush.sculpt_tool].name

def getToolSlotIndex(brush):
    for i,  enum in enumerate(brush.rna_type.properties["sculpt_tool"].enum_items):
        if enum.identifier == brush.sculpt_tool:
            return i

def getPalette(autocreate=False):
    key = "SCULPT_PALETTE"
    
    if key not in bpy.data.palettes:
        if autocreate:
            pal = bpy.data.palettes.new(key)
            for i in range(9):
                ref = pal.brush_palette.brushes.add()
                ref.type = "EMPTY"
        else:
            return None
    
    return bpy.data.palettes[key].brush_palette

def menu_func(self, context):
    layout = self.layout
    layout.separator()
    
    if hasattr(context, "button_operator"):
        opname = context.button_operator.rna_type.identifier
        if opname == "SCULPT_OT_select_brush_tool":
            props = layout.operator("sculpt.remove_brush_tool")
            props.slot = context.button_operator.ref
            
    layout.operator("sculpt.add_to_palette")
    layout.operator("sculpt.call_brush_palette")

    #layout.operator(WM_MT_button_context_sculpt_palette.bl_idname)
class BrushRef(bpy.types.PropertyGroup):
    type_items = [("BRUSH", "Brush","", 0),
        ("TOOL", "Tool", "",1),
        ("EMPTY", "Empty", "", 2)]
    
    type : EnumProperty(items=type_items)
    brush : PointerProperty(type=bpy.types.Brush)
    tool : StringProperty()

class BrushPalette(bpy.types.PropertyGroup):
    brushes : CollectionProperty(type=BrushRef)

    def brushCount(self):
        count = 0

        for i, ref in enumerate(self.brushes):
            if ref.type != "EMPTY" and (ref.type == "TOOL" or (ref.type == "BRUSH" and ref.brush)):
                count += 1

        return count

    def addBrush(self, brush):
        for ref in self.brushes:
            if ref.brush == brush:
                print("Brush already in palette!")
                return None
            if ref.type == "EMPTY" or (ref.type == "BRUSH" and not ref.brush):
                ref.type = "BRUSH"
                ref.brush = brush
                return ref

        ref = self.brushes.add()
        ref.type = "BRUSH"
        ref.brush = brush
        return ref
    
class BrushPaletteSet(bpy.types.PropertyGroup):
    palettes : CollectionProperty(type=BrushPalette)
    active : IntProperty()
    
    def getOrCreateActive(self):
        if len(self.palettes) == 0:
            self.palettes.add()
            
        return self.palettes[self.active]
   
    def getActive(self):
        if len(self.palettes) == 0:
            return None
        return self.palettes[self.active]
     
def sculpt_poll(cls, context):
    return context.mode == "SCULPT" and context.active_object

whitelist = ["mesh_filter", "cloth_filter", "line_project"
    "box_face_set", "box_mask", "box_hide",
    "ipmask_filter", "color_filter", "mask_by_color",
    "face_set_edit", "move", "rotate", "scale",
    "transform", "annotate"]

whitelist = set(map(lambda item: "builtin." + item, whitelist))

class AddToPalette(bpy.types.Operator):
    "Add brush/tool to palette"
    
    bl_idname = "sculpt.add_to_palette"
    bl_label = "Add To Palette"
    
    @classmethod
    def poll(cls, context):
        if not sculpt_poll(cls, context):
            return False
        
        ok = False
        
        for k in ["button_pointer", "button_prop", "button_operator"]:
            if not hasattr(context, k):
                continue
            v = getattr(context, k)
            
            print(v, dir(v))
            if hasattr(v, "name"):
                print(v.name)            
            
            if k == "button_operator":
                ok = v.name.startswith("builtin_brush") or v.name in whitelist
                break
            print("=====" + k + "=====")
            """
            for k in dir(context):
                print(k)
            #"""
            
        return ok 
    
    def execute(self, context):
        pal = getPalette(True)
        
        ok = False
        key = None
        
        for k in ["button_pointer", "button_prop", "button_operator"]:
            if not hasattr(context, k):
                continue
            v = getattr(context, k)
            
            print(v, dir(v))
            if hasattr(v, "name"):
                print(v.name)            
            
            if k == "button_operator":
                ok = v.name.startswith("builtin_brush") or v.name in whitelist
    
                if ok:
                    key = v.name
                    break
        
        if not ok:
            return {'CANCELLED'}
        
        print("found brush or tool", key)
        
        if key.startswith("builtin_brush"):
            self.do_brush(context, key)
        else:
            self.do_tool(context, key)
            
        return {'FINISHED'}
    
    def do_brush(self, context, key):
        key = key.split(".")
        type = key[1].upper()
        print(type)
        slots = context.tool_settings.sculpt.tool_slots
        
        slot = None 
        
        for slot2 in slots:
            if not slot2.brush: continue
            if slot2.brush.sculpt_tool == type:
                slot = slot2
                break
        
        if slot is None:
            print("error!", type)
            return
        
        print("found", type)
        
        pal = getPalette(True)
        pal.addBrush(slot.brush)
        
    def do_tool(self, context, key):
        pass
    
class SelectBrushTool(bpy.types.Operator):
    "Select brush/tool"
    
    bl_idname = "sculpt.select_brush_tool"
    bl_label = "Select Brush/Tool"
    
    ref : IntProperty()
    
    @classmethod
    def poll(cls, context):
        return sculpt_poll(cls, context)

    def execute(self, context):
        pal = getPalette(True)

        ref = pal.brushes[self.ref]
        if ref.type == "TOOL":
            bpy.ops.wm.tool_set_by_id(name="builtin." + ref.tool)
        elif ref.type == "BRUSH":
            print("REF", ref.brush, ref.type, ref)
            if not ref.brush:
                return {'CANCELLED'}
            
            tool = getToolSlotName(ref.brush)
            #bpy.ops.wm.tool_set_by_id(name=tool)
            bpy.ops.paint.brush_select(sculpt_tool=ref.brush.sculpt_tool)
            
            sloti = getToolSlotIndex(ref.brush)
            slots = bpy.context.tool_settings.sculpt.tool_slots
            slots[sloti].brush = ref.brush
            
            
        return {'FINISHED'}


class SetBrushTool(bpy.types.Operator):
    "Set brush/tool"
    
    bl_idname = "sculpt.set_brush_tool"
    bl_label = "Set Palette Entry"
    
    ref : IntProperty()
    slot : IntProperty()
    
    @classmethod
    def poll(cls, context):
        return sculpt_poll(cls, context)

    def execute(self, context):
        pal = getPalette(True)
        slot = self.slot

        while len(pal.brushes) <= slot:
            ref = pal.brushes.add()
            ref.type = "EMPTY"
        ref = pal.brushes[slot]
        
        ref.type = "BRUSH"
        ref.brush = context.tool_settings.sculpt.brush
            
        return {'FINISHED'}


class RemoveBrushTool(bpy.types.Operator):
    "Remove brush/tool"
    
    bl_idname = "sculpt.remove_brush_tool"
    bl_label = "Remove Brush/Tool From Palette"
    
    ref : IntProperty()
    slot : IntProperty()
    
    @classmethod
    def poll(cls, context):
        return sculpt_poll(cls, context)

    def execute(self, context):
        pal = getPalette(True)
        slot = self.slot
        
        pal.brushes[slot].type = "EMPTY"
        pal.brushes[slot].brush = None
            
        return {'FINISHED'}

class CallBrushMenu(bpy.types.Operator):
    "Select brush/tool"
    
    bl_idname = "sculpt.call_brush_palette"
    bl_label = "Open Brush Palette"
    
    @classmethod
    def poll(cls, context):
        ok = context.active_object is not None
        ok = ok and context.mode == "SCULPT"
        ok = ok and context.tool_settings is not None
        ok = ok and context.tool_settings.sculpt is not None
        return ok
    
    def invoke(cls, context, b):
        getPalette(True)
        bpy.ops.wm.call_panel(name="VIEW3D_PT_SculptPalette", keep_open=False)
        #bpy.ops.wm.call_menu(name="VIEW3D_MT_SculptPalette")

        return {'CANCELLED'}
        
    def execute(self, context):
        return {'CANCELLED'}

class VIEW3D_PT_SculptPalette(bpy.types.Panel):
    bl_label = "Sculpt Palette"
    bl_idname = "VIEW3D_PT_SculptPalette"
    bl_space_type = "VIEW_3D"
    bl_region_type = "WINDOW"
    
    def draw(self, context):
        layout = self.layout #.menu_pie()#.column(align=True)
        row = layout.column(align=1)
        col = None #row.column(align=True)
        row.alignment = 'LEFT'
        
        scale = 2.0
        
        pal = getPalette(False)

        if pal is None:
            print("no palettes")
            return

        #rows and cols as identifiers became logically swapped during
        #development, unwap them

        j = 0
        rows = min(int(len(pal.brushes) / 3) + 1, 3)
        cols = 3

        if rows * cols == pal.brushCount():
            rows += 1

        for i in range(cols * rows):
            if i % cols == 0:
                col = row.row(align=1)
                col.scale_x = scale
                col.scale_y = scale
                col.alignment = 'LEFT'
                
                j += 1
                if j == rows:
                    row = layout.column(align=1)
                    j = 0
                    
            id = ((i + 0) % cols)
            id = ((j + 2) % cols) * cols + id

            n = 0
            icon = 0
            if id < len(pal.brushes) and pal.brushes[id].type != "EMPTY" and pal.brushes[id].brush:
                    
                n = getToolSlotIndex(pal.brushes[id].brush)
                brush = pal.brushes[id].brush

                vname = brush.sculpt_tool.lower()
                vname = "brush.sculpt." + vname
                print("ICON", vname)
                
                icon = ToolSelectPanelHelper._icon_value_from_icon_handle(vname)
            
            if id > 10:
                keyt = ''
            else:
                keyt = str(id)
            
            keyt = ''
            
            empty = id >= len(pal.brushes)
            empty = empty or pal.brushes[id].type == "EMPTY"
            empty = empty or pal.brushes[id].type == "BRUSH" and not pal.brushes[id]
            
            if empty:       
                icon = 9
                props = col.operator("sculpt.set_brush_tool", text='', icon_value=icon)
                props.slot = id
            else:
                props = col.operator("sculpt.select_brush_tool", text=keyt, icon_value=icon)
                props.ref = id
            
            #col.label(text="")
            #col.template_icon(i, scale=1)
        """
preview_collections = {}

my_icons_dir = os.path.dirname(bpy.data.filepath)
my_icons_path = ""
my_icon_name = "my_icon"

class SubObj (dict):
    pass

def savedata(obj, subkeys={}, is_sub=False):
    props = obj.bl_rna.properties

    ret = {} if not is_sub else SubObj()
    
    for p in props:
        if p.is_readonly:
            continue
        if p.type in ["ENUM", "FLOAT", "INT", "BOOLEAN", "STRING"]:
            ret[p.identifier] = getattr(obj, p.identifier)
    
    return ret

def loaddata(obj, data):
    for k in data:
        v = data[k]
        if type(v) == SubObj:
            loaddata(getattr(obj), v)
            continue
        
        try:
            setattr(obj, k, v)
        except:
            print("failed to set property", k, "on", obj);
        
def pushRender():
    render = bpy.context.scene.render
    return savedata(render)

def popRender(state):
    render = bpy.context.scene.render
    loaddata(render, state)

state = pushRender()
popRender(state)

class RenderPreview (bpy.types.Operator):
    "Render preview icon"
    
    bl_idname = "render.my_render_preview"
    bl_label = "Render Icon"

    filepath: bpy.props.StringProperty()
    use_viewport: bpy.props.BoolProperty()
    
    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        state = pushRender()
        render = bpy.context.scene.render
        
        print("Render!")
        
        print("file", self.filepath)
        render.filepath = self.filepath
        res = 256
        render.resolution_x = res
        render.resolution_y = res
        render.film_transparent = True
        render.filter_size = 0.75
        
        if self.use_viewport:
            bpy.ops.render.opengl(write_still=True, view_context=True)
        else:
            bpy.ops.render.render(write_still=True)
            
        popRender(state)
        
        pcoll = preview_collections["main"]
        print(pcoll) #.update.__doc__, pcoll.load.__doc__)
        #pcoll.clear(m
        if my_icon_name in pcoll:
            pcoll[my_icon_name].reload()
        else:
            pcoll.load(my_icon_name, my_icon_path, 'IMAGE', force_reload=True)

        return {'FINISHED'}

    
class PreviewsExamplePanel(bpy.types.Panel):
    "Creates a Panel in the Object properties window"
    bl_label = "Previews Example Panel"
    bl_idname = "OBJECT_PT_previews"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        pcoll = preview_collections["main"]

        row = layout.row()
        my_icon = pcoll[my_icon_name]
        
        print(my_icon_name, my_icon.icon_id)
        row.template_icon(my_icon.icon_id, scale=2)
        
        props = row.operator("render.my_render_preview", icon_value=my_icon.icon_id)
        props["filepath"] = my_icon_path
        props["use_viewport"] = True

        # my_icon.icon_id can be used in any UI function that accepts
        # icon_value # try also setting text=""
        # to get an icon only operator button
    
#"""

classes = [VIEW3D_PT_SculptPalette,
    #RenderPreview,
    WM_MT_button_context,
    SelectBrushTool,
    CallBrushMenu,
    BrushRef,
    SetBrushTool,
    AddToPalette,
    RemoveBrushTool,
    BrushPalette,
    BrushPaletteSet]

def post_register():
    bpy.types.Palette.brush_palette = PointerProperty(type=BrushPalette)
    bpy.types.WM_MT_button_context.append(menu_func)

if __name__ == "__main__":
    if not hasattr(bpy, "sculpt_global"):
        bpy.sculpt_global = []
    
    for cls in bpy.sculpt_global:
        bpy.utils.unregister_class(cls)
    
    bpy.sculpt_global = []
    
    for cls in classes:
        bpy.utils.register_class(cls)
        bpy.sculpt_global.append(cls)
    
    post_register()
