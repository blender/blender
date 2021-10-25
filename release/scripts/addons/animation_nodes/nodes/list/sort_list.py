import bpy
from bpy.props import *
from collections import defaultdict
from ... base_types import AnimationNode, ListTypeSelectorSocket

class SortingTemplate:
    properties = {}

    @classmethod
    def create(self, node, data):
        pass

    @classmethod
    def draw(self, layout, data):
        pass

    @classmethod
    def drawAdvanced(self, layout, data):
        pass

class SortingTemplateProperties(bpy.types.PropertyGroup):
    bl_idname = "an_SortingTemplateProperties"

class SortListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SortListNode"
    bl_label = "Sort List"
    bl_width_default = 200
    errorHandlingType = "MESSAGE"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Object List")

    templateData = PointerProperty(type = SortingTemplateProperties)

    def getSortingTemplateItems(self, context):
        items = []
        for template in getTemplatesForDataType(self.assignedType):
            item = (template.identifier, template.name, "")
            items.append(item)
        return items

    activeTemplateIdentifier = EnumProperty(name = "Template",
        update = AnimationNode.refresh, items = getSortingTemplateItems)

    def setup(self):
        self.activeTemplateIdentifier = "CUSTOM"

    def create(self):
        prop = ("assignedType", "LIST")

        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop, dataIsModified = True))
        self.newInput("Boolean", "Reverse", "reverseOutput", value = False)
        self.newOutput(ListTypeSelectorSocket(
            "Sorted List", "outList", "LIST", prop))

        self.activeTemplate.create(self, self.activeTemplateData)

    def draw(self, layout):
        layout.prop(self, "activeTemplateIdentifier", text = "")
        self.activeTemplate.draw(layout, self.activeTemplateData)

    def drawAdvanced(self, layout):
        self.activeTemplate.drawAdvanced(layout, self.activeTemplateData)

    def execute(self, *args):
        try:
            sortedList = self.activeTemplate.execute(self.activeTemplateData, *args)
            return self.outputs[0].correctValue(sortedList)[0]
        except Exception as e:
            self.setErrorMessage(str(e))
            return self.outputs[0].getDefaultValue()

    @property
    def activeTemplate(self):
        return getTemplateByIdentifer(self.activeTemplateIdentifier)

    @property
    def activeTemplateData(self):
        return getattr(self.templateData, self.activeTemplateIdentifier, None)



templates = []
templatesByDataType = defaultdict(list)
templateByIdentifier = {}

def updateSortingTemplates():
    global templates

    templates = list(SortingTemplate.__subclasses__())
    for template in templates:
        identifier = template.identifier
        if not hasattr(SortingTemplateProperties, identifier):
            registerSortingTemplate(template)

def registerSortingTemplate(template):
    templates.append(template)
    templatesByDataType[template.dataType].append(template)
    templateByIdentifier[template.identifier] = template

    propertyGroup = type("an_SortingTemplateProperties_" + template.identifier,
                         (bpy.types.PropertyGroup,),
                         {"bl_label" : template.name})

    bpy.utils.register_class(propertyGroup)
    for name, prop in template.properties.items():
        setattr(propertyGroup, name, prop)

    setattr(SortingTemplateProperties,
            template.identifier,
            PointerProperty(type = propertyGroup))

def getTemplatesForDataType(dataType):
    return templatesByDataType["All"] + templatesByDataType[dataType]

def getTemplateByIdentifer(identifier):
    return templateByIdentifier.get(identifier, None)
