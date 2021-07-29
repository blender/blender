from bpy.props import *
from ... events import propertyChanged
from . sort_list import SortingTemplate, updateSortingTemplates

class SortByKeyList(SortingTemplate):
    identifier = "KEY_LIST"
    dataType = "All"
    name = "Key List"

    keyListTypeItems = [
        ("FLOAT", "Float", "", "NONE", 0),
        ("TEXT", "Text", "", "NONE", 1)]

    properties = dict(
        keyListType = EnumProperty(name = "Key List Type", default = "FLOAT",
            items = keyListTypeItems, update = propertyChanged)
    )

    @classmethod
    def draw(cls, layout, data):
        layout.prop(data, "keyListType", text = "Type")

    @classmethod
    def create(cls, node, data):
        dataType = "Float List" if data.keyListType == "FLOAT" else "Text List"
        node.newInput(dataType, "Key List", "keyList")

    @classmethod
    def execute(cls, data, inList, reverse, keyList):
        if len(inList) != len(keyList):
            raise ValueError("lists have different lengths")

        zippedLists = zip(inList, keyList)
        sortedZippedList = sorted(zippedLists, key = lambda x: x[1], reverse = reverse)

        if isinstance(inList, list):
            return [item[0] for item in sortedZippedList]
        else:
            return type(inList).fromValues(item[0] for item in sortedZippedList)

class CustomSort(SortingTemplate):
    identifier = "CUSTOM"
    dataType = "All"
    name = "Custom"

    properties = dict(
        elementName = StringProperty(name = "Element Name", default = "e",
            update = propertyChanged),
        sortKey = StringProperty(name = "Sort Key", default = "e",
            update = propertyChanged)
    )

    @classmethod
    def draw(cls, layout, data):
        col = layout.column(align = True)
        col.label("Key (use {} for the element)".format(repr(data.elementName)))
        col.prop(data, "sortKey", text = "")

    @classmethod
    def drawAdvanced(cls, layout, data):
        layout.prop(data, "elementName")

    @classmethod
    def execute(cls, data, inList, reverse):
        keyFunction = eval("lambda {}: {}".format(data.elementName, data.sortKey))
        return sorted(inList, key = keyFunction, reverse = reverse)


updateSortingTemplates()
