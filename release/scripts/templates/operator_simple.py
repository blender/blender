def main(context):
	for ob in context.scene.objects:
		print(ob)
	
class SimpleOperator(bpy.types.Operator):
	''''''
	__idname__ = "object.simple_operator"
	__label__ = "Simple Object Operator"
	
	def poll(self, context):
		return context.active_object != None
	
	def execute(self, context):
		main(context)
		return ('FINISHED',)

bpy.ops.add(SimpleOperator)

if __name__ == "__main__":
	bpy.ops.object.simple_operator()
