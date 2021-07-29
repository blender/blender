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
import os
import http, http.client, http.server
import webbrowser
import json

import netrender
from netrender.utils import *
import netrender.client as client
import netrender.model
import netrender.versioning as versioning

class RENDER_OT_netclientsendbake(bpy.types.Operator):
    """Send a baking job to the Network"""
    bl_idname = "render.netclientsendbake"
    bl_label = "Bake on network"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        try:
            conn = clientConnection(netsettings, report = self.report)

            if conn:
                # Sending file
                client.sendJobBaking(conn, scene)
                conn.close()
                self.report({'INFO'}, "Job sent to master")
        except Exception as err:
            self.report({'ERROR'}, str(err))

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientanim(bpy.types.Operator):
    """Start rendering an animation on network"""
    bl_idname = "render.netclientanim"
    bl_label = "Animation on network"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        conn = clientConnection(netsettings, report = self.report)

        if conn:
            # Sending file
            scene.network_render.job_id = client.sendJob(conn, scene, True)
            conn.close()

        bpy.ops.render.render('INVOKE_AREA', animation=True)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientrun(bpy.types.Operator):
    """Start network rendering service"""
    bl_idname = "render.netclientstart"
    bl_label = "Start Service"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        bpy.ops.render.render('INVOKE_AREA', animation=True)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientsend(bpy.types.Operator):
    """Send Render Job to the Network"""
    bl_idname = "render.netclientsend"
    bl_label = "Send job"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        try:
            conn = clientConnection(netsettings, report = self.report)

            if conn:
                # Sending file
                scene.network_render.job_id = client.sendJob(conn, scene, True)
                conn.close()
                self.report({'INFO'}, "Job sent to master")
        except Exception as err:
            self.report({'ERROR'}, str(err))


        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientsendframe(bpy.types.Operator):
    """Send Render Job with current frame to the Network"""
    bl_idname = "render.netclientsendframe"
    bl_label = "Send current frame job"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        scene = context.scene
        netsettings = scene.network_render

        try:
            conn = clientConnection(netsettings, report = self.report)

            if conn:
                # Sending file
                scene.network_render.job_id = client.sendJob(conn, scene, False)
                conn.close()
                self.report({'INFO'}, "Job sent to master")
        except Exception as err:
            self.report({'ERROR'}, str(err))


        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientstatus(bpy.types.Operator):
    """Refresh the status of the current jobs"""
    bl_idname = "render.netclientstatus"
    bl_label = "Client Status"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings, report = self.report)

        if conn:
            with ConnectionContext():
                conn.request("GET", "/status")
            response = conn.getresponse()
            content = response.read()
            print( response.status, response.reason )

            jobs = (netrender.model.RenderJob.materialize(j) for j in json.loads(str(content, encoding='utf8')))

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

class RENDER_OT_netclientblacklistslave(bpy.types.Operator):
    """Exclude from rendering, by adding slave to the blacklist"""
    bl_idname = "render.netclientblacklistslave"
    bl_label = "Client Blacklist Slave"

    @classmethod
    def poll(cls, context):
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

class RENDER_OT_netclientwhitelistslave(bpy.types.Operator):
    """Remove slave from the blacklist"""
    bl_idname = "render.netclientwhitelistslave"
    bl_label = "Client Whitelist Slave"

    @classmethod
    def poll(cls, context):
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


class RENDER_OT_netclientslaves(bpy.types.Operator):
    """Refresh status about available Render slaves"""
    bl_idname = "render.netclientslaves"
    bl_label = "Client Slaves"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings, report = self.report)

        if conn:
            with ConnectionContext():
                conn.request("GET", "/slaves")
            response = conn.getresponse()
            content = response.read()
            print( response.status, response.reason )

            slaves = (netrender.model.RenderSlave.materialize(s) for s in json.loads(str(content, encoding='utf8')))

            while(len(netsettings.slaves_blacklist) > 0):
                netsettings.slaves_blacklist.remove(0)

            while(len(netsettings.slaves) > 0):
                netsettings.slaves.remove(0)

            netrender.slaves = []

            for s in slaves:
                for i in range(len(netrender.blacklist)):
                    slave = netrender.blacklist[i]
                    if slave.id == s.id:
                        netrender.blacklist[i] = s
                        netsettings.slaves_blacklist.add()
                        slave = netsettings.slaves_blacklist[-1]
                        slave.name = s.name
                        break
                else:
                    netrender.slaves.append(s)

                    netsettings.slaves.add()
                    slave = netsettings.slaves[-1]
                    slave.name = s.name

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientcancel(bpy.types.Operator):
    """Cancel the selected network rendering job"""
    bl_idname = "render.netclientcancel"
    bl_label = "Client Cancel"

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings, report = self.report)

        if conn:
            job = netrender.jobs[netsettings.active_job_index]

            with ConnectionContext():
                conn.request("POST", cancelURL(job.id), json.dumps({'clear':False}))
            response = conn.getresponse()
            response.read()
            print( response.status, response.reason )

            netsettings.jobs.remove(netsettings.active_job_index)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class RENDER_OT_netclientcancelall(bpy.types.Operator):
    """Cancel all running network rendering jobs"""
    bl_idname = "render.netclientcancelall"
    bl_label = "Client Cancel All"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render
        conn = clientConnection(netsettings, report = self.report)

        if conn:
            with ConnectionContext():
                conn.request("POST", "/clear", json.dumps({'clear':False}))
            response = conn.getresponse()
            response.read()
            print( response.status, response.reason )

            while(len(netsettings.jobs) > 0):
                netsettings.jobs.remove(0)

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class netclientdownload(bpy.types.Operator):
    """Download render results from the network"""
    bl_idname = "render.netclientdownload"
    bl_label = "Client Download"

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return netsettings.active_job_index >= 0 and len(netsettings.jobs) > netsettings.active_job_index

    def execute(self, context):
        netsettings = context.scene.network_render

        conn = clientConnection(netsettings, report = self.report)

        if conn:
            job_id = netrender.jobs[netsettings.active_job_index].id

            with ConnectionContext():
                conn.request("GET", "/status", headers={"job-id":job_id})
            response = conn.getresponse()

            if response.status != http.client.OK:
                self.report({'ERROR'}, "Job ID %i not defined on master" % job_id)
                return {'ERROR'}

            content = response.read()

            job = netrender.model.RenderJob.materialize(json.loads(str(content, encoding='utf8')))

            conn.close()

            finished_frames = []

            nb_error = 0
            nb_missing = 0

            for frame in job.frames:
                if frame.status == netrender.model.FRAME_DONE:
                    finished_frames.append(frame.number)
                elif frame.status == netrender.model.FRAME_ERROR:
                    nb_error += 1
                else:
                    nb_missing += 1

            if not finished_frames:
                self.report({'ERROR'}, "Job doesn't have any finished frames")
                return {'ERROR'}

            frame_ranges = []

            first = None
            last = None

            for i in range(len(finished_frames)):
                current = finished_frames[i]

                if not first:
                    first = current
                    last = current
                elif last + 1 == current:
                    last = current

                if last + 1 < current or i + 1 == len(finished_frames):
                    if first < last:
                        frame_ranges.append((first, last))
                    else:
                        frame_ranges.append((first,))

                    first = current
                    last = current

            getResults(netsettings.server_address, netsettings.server_port, job_id, job.resolution[0], job.resolution[1], job.resolution[2], frame_ranges)

            if nb_error and nb_missing:
                self.report({'ERROR'}, "Results downloaded but skipped %i frames with errors and %i unfinished frames" % (nb_error, nb_missing))
            elif nb_error:
                self.report({'ERROR'}, "Results downloaded but skipped %i frames with errors" % nb_error)
            elif nb_missing:
                self.report({'WARNING'}, "Results downloaded but skipped %i unfinished frames" % nb_missing)
            else:
                self.report({'INFO'}, "All results downloaded")

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class netclientscan(bpy.types.Operator):
    """Listen on network for master server broadcasting its address and port"""
    bl_idname = "render.netclientscan"
    bl_label = "Client Scan"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        address, port = clientScan(self.report)

        if address:
            scene = context.scene
            netsettings = scene.network_render
            netsettings.server_address = address
            netsettings.server_port = port
            netrender.valid_address = True

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)

class netclientvcsguess(bpy.types.Operator):
    """Guess VCS setting for the current file"""
    bl_idname = "render.netclientvcsguess"
    bl_label = "VCS Guess"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        netsettings = context.scene.network_render

        system = versioning.SYSTEMS.get(netsettings.vcs_system, None)

        if system:
            wpath, name = os.path.split(os.path.abspath(bpy.data.filepath))

            rpath = system.path(wpath)
            revision = system.revision(wpath)

            netsettings.vcs_wpath = wpath
            netsettings.vcs_rpath = rpath
            netsettings.vcs_revision = revision



        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)


class netclientweb(bpy.types.Operator):
    """Open new window with information about running rendering jobs"""
    bl_idname = "render.netclientweb"
    bl_label = "Open Master Monitor"

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return netsettings.server_address != "[default]"

    def execute(self, context):
        netsettings = context.scene.network_render


        # open connection to make sure server exists
        conn = clientConnection(netsettings, report = self.report)

        if conn:
            conn.close()
            if netsettings.use_ssl:
               webbrowser.open("https://%s:%i" % (netsettings.server_address, netsettings.server_port))
            else:
               webbrowser.open("http://%s:%i" % (netsettings.server_address, netsettings.server_port))
        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)
