import bpy
from bpy.props import *
from .. utils.blender_ui import getDpiFactor
from .. utils.id_reference import tryToFindObjectReference

triggerTypeItems = [
    ("MONITOR_PROPERTY", "Monitor Property", "", "", 0)]

idTypeItems = [
    ("OBJECT", "Object", "", "OBJECT_DATA", 0),
    ("SCENE", "Scene", "", "SCENE_DATA", 1)]

class AutoExecutionTrigger_MonitorProperty(bpy.types.PropertyGroup):
    bl_idname = "an_AutoExecutionTrigger_MonitorProperty"

    idType = EnumProperty(name = "ID Type", default = "OBJECT",
        items = idTypeItems)

    idObjectName = StringProperty(name = "ID Object Name", default = "")
    dataPath = StringProperty(name = "Data Path", default = "")

    lastState = StringProperty(default = "")
    enabled = BoolProperty(default = True)
    hasError = BoolProperty(default = False)

    def update(self):
        lastState = self.lastState
        newState = self.getPropertyState()
        if newState == lastState:
            return False
        else:
            self.lastState = newState
            return self.enabled and lastState != ""

    def getPropertyState(self):
        prop = self.getProperty()
        if prop is None: return ""
        if hasattr(prop, "__iter__"):
            return " ".join(str(part) for part in prop)
        return str(prop)

    def getProperty(self):
        self.hasError = False
        object = self.getObject()
        if object is None or self.dataPath is "":
            return None

        try: return object.path_resolve(self.dataPath)
        except:
            self.hasError = True
            return None

    def getObject(self):
        if self.idType == "OBJECT":
            return bpy.data.objects.get(self.idObjectName)
        elif self.idType == "SCENE":
            return bpy.data.scenes.get(self.idObjectName)

    def updateNameProperty(self):
        if self.idType == "OBJECT":
            object = tryToFindObjectReference(self.idObjectName)
            self.idObjectName = getattr(object, "name", "")

    def draw(self, layout, index):
        row = layout.row(align = True)
        if self.hasError:
            row.label("", icon = "ERROR")

        icon = "LAYER_ACTIVE" if self.enabled else "LAYER_USED"
        row.prop(self, "enabled", icon = icon, text = "")

        row.active = self.enabled

        props = row.operator("an.assign_active_object_to_auto_execution_trigger", icon = "EYEDROPPER", text = "")
        props.index = index

        if self.idType == "OBJECT":
            row.prop_search(self, "idObjectName", bpy.context.scene, "objects", text = "")
        elif self.idType == "SCENE":
            row.prop_search(self, "idObjectName", bpy.data, "scenes", text = "")

        row.prop(self, "dataPath", text = "")

        props = row.operator("an.remove_auto_execution_trigger", icon = "X", text = "")
        props.triggerType = "MONITOR_PROPERTY"
        props.index = index


class CustomAutoExecutionTriggers(bpy.types.PropertyGroup):
    bl_idname = "an_CustomAutoExecutionTriggers"

    monitorPropertyTriggers = CollectionProperty(type = AutoExecutionTrigger_MonitorProperty)

    def new(self, type, **kwargs):
        if type == "MONITOR_PROPERTY":
            item = self.monitorPropertyTriggers.add()
        for key, value in kwargs.items():
            setattr(item, key, value)
        return item

    def update(self):
        triggers = [trigger.update() for trigger in self.monitorPropertyTriggers]
        return any(triggers)

    def updateProperties(self):
        for trigger in self.monitorPropertyTriggers:
            trigger.updateNameProperty()


class AutoExecutionProperties(bpy.types.PropertyGroup):

    customTriggers = PointerProperty(type = CustomAutoExecutionTriggers)

    enabled = BoolProperty(default = True, name = "Enabled",
        description = "Enable auto execution for this node tree")

    sceneUpdate = BoolProperty(default = True, name = "Scene Update",
        description = "Execute many times per second to react on all changes in real time (deactivated during preview rendering)")

    frameChanged = BoolProperty(default = False, name = "Frame Changed",
        description = "Execute after the frame changed")

    propertyChanged = BoolProperty(default = False, name = "Property Changed",
        description = "Execute when a attribute in a animation node tree changed")

    treeChanged = BoolProperty(default = False, name = "Tree Changed",
        description = "Execute when the node tree changes (create/remove links and nodes)")

    minTimeDifference = FloatProperty(name = "Min Time Difference",
        description = "Auto execute not that often; E.g. only every 0.5 seconds",
        default = 0.0, min = 0.0, soft_max = 1.0)

    lastExecutionTimestamp = FloatProperty(default = 0.0)


class AddAutoExecutionTrigger(bpy.types.Operator):
    bl_idname = "an.add_auto_execution_trigger"
    bl_label = "Add Auto Execution Trigger"
    bl_options = {"UNDO"}

    triggerType = EnumProperty(name = "Trigger Type", default = "MONITOR_PROPERTY",
        items = triggerTypeItems)

    idType = EnumProperty(name = "ID Type", default = "OBJECT",
        items = idTypeItems)

    @classmethod
    def poll(cls, context):
        return context.getActiveAnimationNodeTree() is not None

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width = 250 * getDpiFactor())

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "triggerType")
        if self.triggerType == "MONITOR_PROPERTY":
            layout.prop(self, "idType")

    def check(self, context):
        return True

    def execute(self, context):
        tree = context.space_data.node_tree
        trigger = tree.autoExecution.customTriggers.new(self.triggerType)
        if self.triggerType == "MONITOR_PROPERTY":
            trigger.idType = self.idType
        context.area.tag_redraw()
        return {"FINISHED"}


class RemoveAutoExecutionTrigger(bpy.types.Operator):
    bl_idname = "an.remove_auto_execution_trigger"
    bl_label = "Remove Auto Execution Trigger"
    bl_options = {"UNDO"}

    triggerType = EnumProperty(items = triggerTypeItems)
    index = IntProperty()

    @classmethod
    def poll(cls, context):
        return context.getActiveAnimationNodeTree() is not None

    def execute(self, context):
        tree = context.space_data.node_tree
        customTriggers = tree.autoExecution.customTriggers

        if self.triggerType == "MONITOR_PROPERTY":
            customTriggers.monitorPropertyTriggers.remove(self.index)
        return {"FINISHED"}


class AssignActiveObjectToAutoExecutionTrigger(bpy.types.Operator):
    bl_idname = "an.assign_active_object_to_auto_execution_trigger"
    bl_label = "Assign Active Object to Auto Execution Trigger"
    bl_options = {"UNDO"}

    index = IntProperty()

    @classmethod
    def poll(cls, context):
        return context.getActiveAnimationNodeTree() is not None

    def execute(self, context):
        tree = context.space_data.node_tree
        trigger = tree.autoExecution.customTriggers.monitorPropertyTriggers[self.index]
        if trigger.idType == "OBJECT":
            trigger.idObjectName = getattr(context.active_object, "name", "")
        if trigger.idType == "SCENE":
            trigger.idObjectName = getattr(context.scene, "name", "")
        return {"FINISHED"}
