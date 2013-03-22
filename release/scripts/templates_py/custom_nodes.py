import bpy
# XXX these don't work yet ...
#from bpy_types import NodeTree, Node, NodeSocket

# Implementation of custom nodes from Python


# Shortcut for node type menu
def add_nodetype(layout, type):
    layout.operator("node.add_node", text=type.bl_label).type = type.bl_rna.identifier

# Derived from the NodeTree base type, similar to Menu, Operator, Panel, etc.
class MyCustomTree(bpy.types.NodeTree):
    # Description string
    '''A custom node tree type that will show up in the node editor header'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomTreeType'
    # Label for nice name display
    bl_label = 'Custom Node Tree'
    # Icon identifier
    # NOTE: If no icon is defined, the node tree will not show up in the editor header!
    #       This can be used to make additional tree types for groups and similar nodes (see below)
    #       Only one base tree class is needed in the editor for selecting the general category
    bl_icon = 'NODETREE'

    def draw_add_menu(self, context, layout):
        layout.label("Hello World!")
        add_nodetype(layout, bpy.types.CustomNodeType)
        add_nodetype(layout, bpy.types.MyCustomGroup)


# Custom socket type
class MyCustomSocket(bpy.types.NodeSocket):
    # Description string
    '''Custom node socket type'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomSocketType'
    # Label for nice name display
    bl_label = 'Custom Node Socket'

    # Enum items list
    my_items = [
        ("DOWN", "Down", "Where your feet are"),
        ("UP", "Up", "Where your head should be"),
        ("LEFT", "Left", "Not right"),
        ("RIGHT", "Right", "Not left")
    ]

    myEnumProperty = bpy.props.EnumProperty(name="Direction", description="Just an example", items=my_items, default='UP')

    # Optional function for drawing the socket input value
    def draw(self, context, layout, node):
        layout.prop(self, "myEnumProperty", text=self.name)

    # Socket color
    def draw_color(self, context, node):
        return (1.0, 0.4, 0.216, 0.5)

# Base class for all custom nodes in this tree type.
# Defines a poll function to enable instantiation.
class MyCustomTreeNode :
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'CustomTreeType'

# Derived from the Node base type.
class MyCustomNode(bpy.types.Node, MyCustomTreeNode):
    # === Basics ===
    # Description string
    '''A custom node'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomNodeType'
    # Label for nice name display
    bl_label = 'Custom Node'
    # Icon identifier
    bl_icon = 'SOUND'

    # === Custom Properties ===
    # These work just like custom properties in ID data blocks
    # Extensive information can be found under
    # http://wiki.blender.org/index.php/Doc:2.6/Manual/Extensions/Python/Properties
    myStringProperty = bpy.props.StringProperty()
    myFloatProperty = bpy.props.FloatProperty(default=3.1415926)

    # === Optional Functions ===
    # Initialization function, called when a new node is created.
    # This is the most common place to create the sockets for a node, as shown below.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, context):
        self.inputs.new('CustomSocketType', "Hello")
        self.inputs.new('NodeSocketFloat', "World")
        self.inputs.new('NodeSocketVector', "!")

        self.outputs.new('NodeSocketColor', "How")
        self.outputs.new('NodeSocketColor', "are")
        self.outputs.new('NodeSocketFloat', "you")

    # Copy function to initialize a copied node from an existing one.
    def copy(self, node):
        print("Copying from node ", node)

    # Free function to clean up on removal.
    def free(self):
        print("Removing node ", self, ", Goodbye!")

    # Additional buttons displayed on the node.
    def draw_buttons(self, context, layout):
        layout.label("Node settings")
        layout.prop(self, "myFloatProperty")

    # Detail buttons in the sidebar.
    # If this function is not defined, the draw_buttons function is used instead
    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "myFloatProperty")
        # myStringProperty button will only be visible in the sidebar
        layout.prop(self, "myStringProperty")


# A customized group-like node.
class MyCustomGroup(bpy.types.NodeGroup, MyCustomTreeNode):
    # === Basics ===
    # Description string
    '''A custom group node'''
    # Label for nice name display
    bl_label = 'Custom Group Node'
    bl_group_tree_idname = 'CustomTreeType'

    orks = bpy.props.IntProperty(default=3)
    dwarfs = bpy.props.IntProperty(default=12)
    wizards = bpy.props.IntProperty(default=1)

    # Additional buttons displayed on the node.
    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, "orks")
        col.prop(self, "dwarfs")
        col.prop(self, "wizards")

        layout.label("The Node Tree:")
        layout.prop(self, "node_tree", text="")


def register():
    bpy.utils.register_class(MyCustomTree)
    bpy.utils.register_class(MyCustomSocket)
    bpy.utils.register_class(MyCustomNode)
    bpy.utils.register_class(MyCustomGroup)


def unregister():
    bpy.utils.unregister_class(MyCustomTree)
    bpy.utils.unregister_class(MyCustomSocket)
    bpy.utils.unregister_class(MyCustomNode)
    bpy.utils.unregister_class(MyCustomGroup)


if __name__ == "__main__":
    register()

