# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
import sys, os
import http, http.client, http.server, urllib, socket
import webbrowser

import netrender
from netrender.utils import *
import netrender.client as client
import netrender.model

@rnaType
class RENDER_OT_netslave_bake(bpy.types.Operator):
    '''NEED DESCRIPTION'''
    bl_idname = "render.netslavebake"
    bl_label = "Bake all in file"

    def poll(self, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        filename = bpy.data.filename
        path, name = os.path.split(filename)
        root, ext = os.path.splitext(name)
        default_path = path + os.sep + "blendcache_" + root + os.sep # need an API call for that
        relative_path = os.sep + os.sep + "blendcache_" + root + os.sep

        # Force all point cache next to the blend file
        for object in bpy.data.objects:
            for modifier in object.modifiers:
                if modifier.type == 'FLUID_SIMULATION' and modifier.settings.type == "DOMAIN":
                    modifier.settings.path = relative_path
                    bpy.ops.fluid.bake({"active_object": object, "scene": scene})
                elif modifier.type == "CLOTH":
                    modifier.point_cache.step = 1
                    modifier.point_cache.disk_cache = True
                    modifier.point_cache.external = False
                elif modifier.type == "SOFT_BODY":
                    modifier.point_cache.step = 1
                    modifier.point_cache.disk_cache = True
                    modifier.point_cache.external = False
                elif modifier.type == "SMOKE" and modifier.smoke_type == "TYPE_DOMAIN":
                    modifier.domain_settings.point_cache_low.step = 1
                    modifier.domain_settings.point_cache_low.disk_cache = True
                    modifier.domain_settings.point_cache_low.external = False
                    modifier.domain_settings.point_cache_high.step = 1
                    modifier.domain_settings.point_cache_high.disk_cache = True
                    modifier.domain_settings.point_cache_high.external = False

            # particles modifier are stupid and don't contain data
            # we have to go through the object property
            for psys in object.particle_systems:
                psys.point_cache.step = 1
                psys.point_cache.disk_cache = True
                psys.point_cache.external = False
                psys.point_cache.filepath = relative_path

        bpy.ops.ptcache.bake_all()

        #bpy.ops.wm.save_mainfile(path = path + os.sep + root + "_baked.blend")

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientanim(bpy.types.Operator):
    '''Start rendering an animation on network'''
    bl_idname = "render.netclientanim"
    bl_label = "Animation on network"

    def poll(self, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            # Sending file
            scene.network_render.job_id = client.clientSendJob(conn, scene, True)
            conn.close()

        bpy.ops.render.render('INVOKE_AREA', animation=True)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientrun(bpy.types.Operator):
    '''Start network rendering service'''
    bl_idname = "render.netclientstart"
    bl_label = "Start Service"

    def poll(self, context):
        return True

    def execute(self, context):
        bpy.ops.render.render('INVOKE_AREA', animation=True)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientsend(bpy.types.Operator):
    '''Send Render Job to the Network'''
    bl_idname = "render.netclientsend"
    bl_label = "Send job"

    def poll(self, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        try:
            conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

            if conn:
                # Sending file
                scene.network_render.job_id = client.clientSendJob(conn, scene, True)
                conn.close()
                self.report('INFO', "Job sent to master")
        except Exception as err:
            self.report('ERROR', str(err))


        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientsendframe(bpy.types.Operator):
    '''Send Render Job with current frame to the Network'''
    bl_idname = "render.netclientsendframe"
    bl_label = "Send current frame job"

    def poll(self, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        try:
            conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

            if conn:
                # Sending file
                scene.network_render.job_id = client.clientSendJob(conn, scene, False)
                conn.close()
                self.report('INFO', "Job sent to master")
        except Exception as err:
            self.report('ERROR', str(err))


        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientstatus(bpy.types.Operator):
    '''Refresh the status of the current jobs'''
    bl_idname = "render.netclientstatus"
    bl_label = "Client Status"

    def poll(self, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            conn.request("GET", "/status")

            response = conn.getresponse()
            print( response.status, response.reason )

            jobs = (netrender.model.RenderJob.materialize(j) for j in eval(str(response.read(), encoding='utf8')))

            while(len(netsettings.jobs) > 0):
                netsettings.jobs.remove(0)

            netrender.jobs = []

            for j in jobs:
                netrender.jobs.append(j)
                netsettings.jobs.add()
                job = netsettings.jobs[-1]

                j.results = j.framesStatus() # cache frame status

                job.name = j.name

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientblacklistslave(bpy.types.Operator):
    '''Operator documentation text, will be used for the operator tooltip and python docs.'''
    bl_idname = "render.netclientblacklistslave"
    bl_label = "Client Blacklist Slave"

    def poll(self, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render

        if netsettings.active_slave_index >= 0:

            # deal with data
            slave = netrender.slaves.pop(netsettings.active_slave_index)
            netrender.blacklist.append(slave)

            # deal with rna
            netsettings.slaves_blacklist.add()
            netsettings.slaves_blacklist[-1].name = slave.name

            netsettings.slaves.remove(netsettings.active_slave_index)
            netsettings.active_slave_index = -1

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientwhitelistslave(bpy.types.Operator):
    '''Operator documentation text, will be used for the operator tooltip and python docs.'''
    bl_idname = "render.netclientwhitelistslave"
    bl_label = "Client Whitelist Slave"

    def poll(self, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render

        if netsettings.active_blacklisted_slave_index >= 0:

            # deal with data
            slave = netrender.blacklist.pop(netsettings.active_blacklisted_slave_index)
            netrender.slaves.append(slave)

            # deal with rna
            netsettings.slaves.add()
            netsettings.slaves[-1].name = slave.name

            netsettings.slaves_blacklist.remove(netsettings.active_blacklisted_slave_index)
            netsettings.active_blacklisted_slave_index = -1

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)


@rnaType
class RENDER_OT_netclientslaves(bpy.types.Operator):
    '''Refresh status about available Render slaves'''
    bl_idname = "render.netclientslaves"
    bl_label = "Client Slaves"

    def poll(self, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            conn.request("GET", "/slaves")

            response = conn.getresponse()
            print( response.status, response.reason )

            slaves = (netrender.model.RenderSlave.materialize(s) for s in eval(str(response.read(), encoding='utf8')))

            while(len(netsettings.slaves) > 0):
                netsettings.slaves.remove(0)

            netrender.slaves = []

            for s in slaves:
                for i in range(len(netrender.blacklist)):
                    slave = netrender.blacklist[i]
                    if slave.id == s.id:
                        netrender.blacklist[i] = s
                        netsettings.slaves_blacklist[i].name = s.name
                        break
                else:
                    netrender.slaves.append(s)

                    netsettings.slaves.add()
                    slave = netsettings.slaves[-1]
                    slave.name = s.name

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientcancel(bpy.types.Operator):
    '''Cancel the selected network rendering job.'''
    bl_idname = "render.netclientcancel"
    bl_label = "Client Cancel"

    def poll(self, context):
        netsettings = context.scene.network_render
        return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            job = netrender.jobs[netsettings.active_job_index]

            conn.request("POST", cancelURL(job.id))

            response = conn.getresponse()
            print( response.status, response.reason )

            netsettings.jobs.remove(netsettings.active_job_index)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class RENDER_OT_netclientcancelall(bpy.types.Operator):
    '''Cancel all running network rendering jobs.'''
    bl_idname = "render.netclientcancelall"
    bl_label = "Client Cancel All"

    def poll(self, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            conn.request("POST", "/clear")

            response = conn.getresponse()
            print( response.status, response.reason )

            while(len(netsettings.jobs) > 0):
                netsettings.jobs.remove(0)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class netclientdownload(bpy.types.Operator):
    '''Download render results from the network'''
    bl_idname = "render.netclientdownload"
    bl_label = "Client Download"

    def poll(self, context):
        netsettings = context.scene.network_render
        return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0

    def execute(self, context):
        netsettings = context.scene.network_render
        rd = context.scene.render

        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            job = netrender.jobs[netsettings.active_job_index]

            for frame in job.frames:
                client.requestResult(conn, job.id, frame.number)
                response = conn.getresponse()

                if response.status != http.client.OK:
                    print("missing", frame.number)
                    continue

                print("got back", frame.number)

                f = open(netsettings.path + "%06d" % frame.number + ".exr", "wb")
                buf = response.read(1024)

                while buf:
                    f.write(buf)
                    buf = response.read(1024)

                f.close()

            conn.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class netclientscan(bpy.types.Operator):
    '''Listen on network for master server broadcasting its address and port.'''
    bl_idname = "render.netclientscan"
    bl_label = "Client Scan"

    def poll(self, context):
        return True

    def execute(self, context):
        address, port = clientScan(self.report)

        if address:
            scene = context.scene
            netsettings = scene.network_render
            netsettings.server_address = address
            netsettings.server_port = port

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

@rnaType
class netclientweb(bpy.types.Operator):
    '''Open new window with information about running rendering jobs'''
    bl_idname = "render.netclientweb"
    bl_label = "Open Master Monitor"

    def poll(self, context):
        netsettings = context.scene.network_render
        return netsettings.server_address != "[default]"

    def execute(self, context):
        netsettings = context.scene.network_render


        # open connection to make sure server exists
        conn = clientConnection(netsettings.server_address, netsettings.server_port, self.report)

        if conn:
            conn.close()

            webbrowser.open("http://%s:%i" % (netsettings.server_address, netsettings.server_port))

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)
