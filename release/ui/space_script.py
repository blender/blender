import bpy

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

        # draw menu item to reload scripts from
        # release/io
        #
        # it should call operator or
        # a func that will:
        # for each .py file in the dir,
        # import/reload module, in the module:
        # find subclasses of bpy.types.Operator,
        # for each subclass create menus under "Export"
        # with (row.)itemO
        #
        # for interface api documentation, see
        # see source/blender/editors/interface/interface_api.c
        #
        # hint: reloading ui scripts in scripts window is Shift+P


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
        pass

class SCRIPT_OT_reload_scripts(bpy.types.Operator):
    __label__ = 'Reload Scripts'

    def exec(self, context):
        print("SCRIPT_OT_reload_scripts: exec")
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
