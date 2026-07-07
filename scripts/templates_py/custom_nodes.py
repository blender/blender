import bpy
from bpy.types import NodeTree, Node, NodeSocket, NodeTreeInterfaceSocket
from bl_ui import node_add_menu

# Implementation of custom nodes from Python


# Derived from the NodeTree base type, similar to Menu, Operator, Panel, etc.
class MyCustomTree(NodeTree):
    # Description string
    '''A custom node tree type that will show up in the editor type list'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomTreeType'
    # Label for nice name display
    bl_label = "Custom Node Tree"
    # Icon identifier
    bl_icon = 'NODETREE'


# Custom socket type
class MyCustomSocket(NodeSocket):
    # Description string
    """Custom node socket type"""
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomSocketType'
    # Label for nice name display
    bl_label = "Custom Node Socket"

    input_value: bpy.props.FloatProperty(
        name="Value",
        description="Value when the socket is not connected",
    )

    # Optional function for drawing the socket input value
    def draw(self, context, layout, node, text):
        if self.is_output or self.is_linked:
            layout.label(text=text)
        else:
            layout.prop(self, "input_value", text=text)

    # Socket color
    @classmethod
    def draw_color_simple(cls):
        return (1.0, 0.4, 0.216, 0.5)


# Customizable interface properties to generate a socket from.
class MyCustomInterfaceSocket(NodeTreeInterfaceSocket):
    # The type of socket that is generated.
    bl_socket_idname = 'CustomSocketType'

    default_value: bpy.props.FloatProperty(default=1.0, description="Default input value for new sockets",)

    def draw(self, context, layout):
        # Display properties of the interface.
        layout.prop(self, "default_value")

    # Set properties of newly created sockets
    def init_socket(self, node, socket, data_path):
        socket.input_value = self.default_value

    # Use an existing socket to initialize the group interface
    def from_socket(self, node, socket):
        # Current value of the socket becomes the default
        self.default_value = socket.input_value


# Mix-in class for all custom nodes in this tree type.
# Defines a poll function to enable instantiation.
class MyCustomTreeNode:
    @classmethod
    def poll(cls, ntree):
        return ntree.bl_idname == 'CustomTreeType'


# Derived from the Node base type.
class MyCustomNode(MyCustomTreeNode, Node):
    # === Basics ===
    # Description string
    '''A custom node'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomNodeType'
    # Label for nice name display
    bl_label = "Custom Node"
    # Icon identifier
    bl_icon = 'SOUND'

    # === Custom Properties ===
    # These work just like custom properties in ID data blocks
    # Extensive information can be found under
    # https://docs.blender.org/api/current/bpy.props.html
    my_string_prop: bpy.props.StringProperty()
    my_float_prop: bpy.props.FloatProperty(default=3.1415926)

    # === Optional Functions ===
    # Initialization function, called when a new node is created.
    # This is the most common place to create the sockets for a node, as shown below.
    # NOTE: this is not the same as the standard __init__ function in Python, which is
    #       a purely internal Python method and unknown to the node system!
    def init(self, context):
        self.inputs.new('CustomSocketType', "Hello")
        self.inputs.new('NodeSocketFloat', "World")
        self.inputs.new('NodeSocketVector', "!", use_multi_input=True)

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
        layout.label(text="Node settings")
        layout.prop(self, "my_float_prop")

    # Detail buttons in the sidebar.
    # If this function is not defined, the draw_buttons function is used instead
    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "my_float_prop")
        # my_string_prop button will only be visible in the sidebar
        layout.prop(self, "my_string_prop")

    # Optional: custom label
    # Explicit user label overrides this, but here we can define a label dynamically
    def draw_label(self):
        return "I am a custom node"


# Add custom nodes to the Add & Swap menu.
def draw_add_menu(self, context):
    layout = self.layout
    if context.space_data.tree_type != MyCustomTree.bl_idname:
        # Avoid adding nodes to built-in node tree
        return
    # Add nodes to the layout. Can use submenus, separators, etc. as in any other menu.
    self.node_operator(layout, "CustomNodeType")


classes = (
    MyCustomTree,
    MyCustomSocket,
    MyCustomInterfaceSocket,
    MyCustomNode,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

    bpy.types.NODE_MT_add.append(draw_add_menu)
    bpy.types.NODE_MT_swap.append(draw_add_menu)


def unregister():
    bpy.types.NODE_MT_add.remove(draw_add_menu)
    bpy.types.NODE_MT_swap.remove(draw_add_menu)

    from bpy.utils import unregister_class
    for cls in reversed(classes):
        unregister_class(cls)


if __name__ == "__main__":
    register()
