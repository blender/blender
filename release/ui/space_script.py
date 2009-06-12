import sys
import os
import imp
# import glob
import bpy

operators = []

def register_op(opclass):
    if (hasattr(bpy.ops, opclass.__name__)):
        bpy.ops.remove(getattr(bpy.ops, opclass.__name__))

    bpy.ops.add(opclass)

    global operators
    if opclass.__name__ not in operators:
        operators.append(opclass.__name__)


# hint for myself: for interface api documentation, see source/blender/editors/interface/interface_api.c
# another hint: reloading ui scripts in scripts window is Shift + P

class SCRIPT_HT_header(bpy.types.Header):
	__space_type__ = "SCRIPTS_WINDOW"
	__idname__ = "SCRIPT_HT_header"

	def draw(self, context):
		st = context.space_data
		layout = self.layout

		layout.template_header(context)

		if context.area.show_menus:
			row = layout.row(align=True)
			row.itemM(context, "SCRIPT_MT_scripts")

class SCRIPT_MT_scripts(bpy.types.Menu):
    __space_type__ = "SCRIPTS_WINDOW"
    __label__ = "Scripts"

    def draw(self, context):
        layout = self.layout
        layout.column()
        layout.itemM(context, "SCRIPT_MT_export")
        layout.itemO("SCRIPT_OT_reload_scripts")

class SCRIPT_MT_export(bpy.types.Menu):
    __space_type__ = "SCRIPTS_WINDOW"
    __label__ = "Export"

    def draw(self, context):
        global operators

        print("drawing {0} operators: {1}".format(len(operators), operators))

        layout = self.layout
        layout.column()
        for opname in operators:
            layout.itemO(opname)

class SCRIPT_OT_reload_scripts(bpy.types.Operator):
    __label__ = 'Reload Scripts'

    def exec(self, context):
        print("SCRIPT_OT_reload_scripts: exec")

        # add ../io to sys.path

        # this module's absolute path
        abspath = os.path.abspath(sys.modules[__name__].__file__)
        print("Current abspath: {0}".format(abspath))

        # ../io
        io = os.path.normpath(os.path.dirname(abspath) + "/../io")
        print("abspath = " + io)

        if io not in sys.path:
            sys.path.append(io)

        # for each .py file in release/io,
        # import/reload module, in the module:
        # find subclasses of bpy.types.Operator,
        # for each subclass create menus under "Export"
        # with (row.)itemO

        global operators
        operators = []

        # glob unavailable :(
#         for path in glob.glob("../io/*.py"):
        for path in os.listdir(io):
            modname, ext = os.path.splitext(os.path.basename(path))

            if ext != ".py":
                continue

            print("Found module {0}.".format(modname))

            if modname in sys.modules:
                mod = imp.reload(sys.modules[modname])
                print("Reloaded it.")
            else:
                mod = __import__(modname)
                print("Imported it.")

            for attr in dir(mod):
                cls = getattr(mod, attr)
                
                # XXX is there a better way to check that cls is a class?
                if type(cls) == bpy.types.Operator.__class__ and issubclass(cls, bpy.types.Operator):
                    print("Found class {0}.".format(cls.__name__))
                    register_op(cls)
                    print("Registered it.")

        return 'FINISHED'

    def invoke(self, context, event):
        print("SCRIPT_OT_reload_scripts: invoke")
        return self.exec(context)

    def poll(self, context):
        pass

bpy.types.register(SCRIPT_HT_header)
bpy.types.register(SCRIPT_MT_scripts)
bpy.types.register(SCRIPT_MT_export)

if (hasattr(bpy.ops, "SCRIPT_OT_reload_scripts")):
    bpy.ops.remove(bpy.ops.SCRIPT_OT_reload_scripts)

bpy.ops.add(SCRIPT_OT_reload_scripts)
