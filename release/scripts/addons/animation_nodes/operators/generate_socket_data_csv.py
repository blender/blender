import bpy
from .. sockets.info import getSocketClasses, isList, isBase

class GenerateSocketDataCSV(bpy.types.Operator):
    bl_idname = "an.generate_socket_data_csv"
    bl_label = "Generate Socket Data CSV"

    def execute(self, context):
        sockets = [socket for socket in getSocketClasses() if not isList(socket.bl_idname)]
        sockets.sort(key = lambda x: x.dataType)
        output = []
        for socket in sockets:
            data = []
            data.append(socket.dataType)
            data.append("``{}``".format(socket.bl_idname))
            data.append("Yes" if isBase(socket.bl_idname) else "No")
            output.append(", ".join(data))

        text = bpy.data.texts.get("Socket Data.cvs")
        if text is None:
            text = bpy.data.texts.new("Socket Data.cvs")
        text.clear()
        text.write("\n".join(output))
        return {"FINISHED"}
