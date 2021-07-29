import bpy

ng = bpy.data.node_groups


class SomeScriptNodeProperties(bpy.types.PropertyGroup):


    @classmethod
    def register(cls):

        def exp_update(self, context):
            if 'NodeTree' in ng:
                print('here')
                n = ng['NodeTree'].nodes.get('Scripted Node Lite')
                if n:
                    n.process()

        Scn = bpy.types.Scene

        Scn.some_scriptnode_props = bpy.props.PointerProperty(
            name="some scripts internal global properties",
            type=cls,
        )

        cls.custom_1 = bpy.props.FloatProperty(name="My Float", min=0, max=2, update=exp_update)
        cls.custom_2 = bpy.props.IntProperty(name="My Int")


    @classmethod
    def unregister(cls):
        del bpy.types.Scene.some_scriptnode_props


def register():
    bpy.utils.register_class(SomeScriptNodeProperties)

def unregister():
    bpy.utils.unregister_class(SomeScriptNodeProperties)


if __name__ == '__main__':
    register()