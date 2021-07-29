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
import time

import netrender

from netrender.utils import *

from bpy.props import PointerProperty, StringProperty, BoolProperty, EnumProperty, IntProperty, CollectionProperty

VERSION = b"0.5"

PATH_PREFIX = "/tmp/"

LAST_ADDRESS_TEST = 0
ADDRESS_TEST_TIMEOUT = 30

def base_poll(cls, context):
    rd = context.scene.render
    return (rd.use_game_engine==False) and (rd.engine in cls.COMPAT_ENGINES)


def init_file():
    if netrender.init_file != bpy.data.filepath:
        netrender.init_file = bpy.data.filepath
        netrender.init_data = True
        netrender.valid_address = False

def init_data(netsettings):
    init_file()

    if netrender.init_data:
        netrender.init_data = False

        netsettings.active_slave_index = 0
        while(len(netsettings.slaves) > 0):
            netsettings.slaves.remove(0)

        netsettings.active_blacklisted_slave_index = 0
        while(len(netsettings.slaves_blacklist) > 0):
            netsettings.slaves_blacklist.remove(0)

        netsettings.active_job_index = 0
        while(len(netsettings.jobs) > 0):
            netsettings.jobs.remove(0)

def verify_address(netsettings, force=False):
    global LAST_ADDRESS_TEST
    init_file()

    if force or LAST_ADDRESS_TEST + ADDRESS_TEST_TIMEOUT < time.time():
        LAST_ADDRESS_TEST = time.time()

        try:
            conn = clientConnection(netsettings, scan = False, timeout = 1)
        except:
            conn = None

        if conn:
            netrender.valid_address = True
            conn.close()
        else:
            netrender.valid_address = False

    return netrender.valid_address

class NeedValidAddress():
    @classmethod
    def poll(cls, context):
        return super().poll(context) and verify_address(context.scene.network_render)

class NetRenderButtonsPanel():
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.engine == 'NET_RENDER' and rd.use_game_engine == False

# Setting panel, use in the scene for now.
class RENDER_PT_network_settings(NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Network Settings"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        return super().poll(context)

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        verify_address(netsettings)

        layout.prop(netsettings, "mode", expand=True)

        if netsettings.mode in {'RENDER_MASTER', 'RENDER_SLAVE'}:
            layout.operator("render.netclientstart", icon='PLAY')

        layout.prop(netsettings, "path")

        row = layout.row()

        split = layout.split(percentage=0.5)

        col = split.column()
        col.prop(netsettings, "server_address", text="Address")

        col = split.column()
        row = col.row()
        row.prop(netsettings, "server_port", text="Port")
        row.prop(netsettings, "use_ssl", text="SSL")

        if netsettings.mode != "RENDER_MASTER":
            layout.operator("render.netclientscan", icon='FILE_REFRESH', text="")

        if not netrender.valid_address:
            layout.label(text="No master at specified address")


        if netsettings.use_ssl and netsettings.mode == "RENDER_MASTER":
            layout.prop(netsettings, "cert_path", text="Certificate")
            layout.prop(netsettings, "key_path", text="Key")

        layout.operator("render.netclientweb", icon='QUESTION')



class RENDER_PT_network_slave_settings(NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Slave Settings"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return super().poll(context) and scene.network_render.mode == "RENDER_SLAVE"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        netsettings = context.scene.network_render

        layout.prop(netsettings, "slave_tags", text="Tags")
        layout.prop(netsettings, "slave_render")
        layout.prop(netsettings, "slave_bake")
        layout.prop(netsettings, "use_slave_clear")
        layout.prop(netsettings, "use_slave_thumb")
        layout.prop(netsettings, "use_slave_output_log")
        layout.label(text="Threads:")
        layout.prop(rd, "threads_mode", expand=True)

        col = layout.column()
        col.enabled = rd.threads_mode == 'FIXED'
        col.prop(rd, "threads")

class RENDER_PT_network_master_settings(NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Master Settings"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return super().poll(context) and scene.network_render.mode == "RENDER_MASTER"

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        layout.prop(netsettings, "use_master_broadcast")
        layout.prop(netsettings, "use_master_force_upload")
        layout.prop(netsettings, "use_master_clear")

class RENDER_PT_network_job(NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Job Settings"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        verify_address(netsettings)

        if netsettings.server_address != "[default]":
            layout.operator("render.netclientanim", icon='RENDER_ANIMATION')
            layout.operator("render.netclientsend", icon='FILE_BLEND')
            layout.operator("render.netclientsendbake", icon='PHYSICS')
            layout.operator("render.netclientsendframe", icon='RENDER_STILL')
            if netsettings.job_id:
                row = layout.row()
                row.operator("render.render", text="Get Image", icon='RENDER_STILL')
                row.operator("render.render", text="Get Animation", icon='RENDER_ANIMATION').animation = True

        layout.prop(netsettings, "job_type", text="Type")
        layout.prop(netsettings, "job_name", text="Name")
        layout.prop(netsettings, "job_category", text="Category")
        layout.prop(netsettings, "job_tags", text="Tags")
        layout.prop(netsettings, "job_render_engine", text="Engine")

        if netsettings.job_render_engine == "OTHER":
            layout.prop(netsettings, "job_render_engine_other", text="Other Engine")

        row = layout.row()
        row.prop(netsettings, "priority")
        row.prop(netsettings, "chunks")

        if netsettings.job_type == "JOB_BLENDER":
            layout.prop(netsettings, "save_before_job")



class RENDER_PT_network_job_vcs(NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "VCS Job Settings"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (super().poll(context)
            and scene.network_render.mode == "RENDER_CLIENT"
            and scene.network_render.job_type == "JOB_VCS")

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        layout.operator("render.netclientvcsguess", icon='FILE_REFRESH', text="")

        layout.prop(netsettings, "vcs_system")
        layout.prop(netsettings, "vcs_revision")
        layout.prop(netsettings, "vcs_rpath")
        layout.prop(netsettings, "vcs_wpath")

class RENDER_PT_network_slaves(NeedValidAddress, NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Slaves Status"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return super().poll(context) and netsettings.mode == "RENDER_CLIENT"

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        row = layout.row()
        row.template_list("UI_UL_list", "net_render_slaves", netsettings, "slaves",
                          netsettings, "active_slave_index", rows=2)

        sub = row.column(align=True)
        sub.operator("render.netclientslaves", icon='FILE_REFRESH', text="")
        sub.operator("render.netclientblacklistslave", icon='ZOOMOUT', text="")

        if len(netrender.slaves) > netsettings.active_slave_index >= 0:
            layout.separator()

            slave = netrender.slaves[netsettings.active_slave_index]

            layout.label(text="Name: " + slave.name)
            layout.label(text="Address: " + slave.address[0])
            layout.label(text="Seen: " + time.ctime(slave.last_seen))
            layout.label(text="Stats: " + slave.stats)

class RENDER_PT_network_slaves_blacklist(NeedValidAddress, NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Slaves Blacklist"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return super().poll(context) and netsettings.mode == "RENDER_CLIENT"

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        row = layout.row()
        row.template_list("UI_UL_list", "net_render_slaves_blacklist", netsettings, "slaves_blacklist",
                          netsettings, "active_blacklisted_slave_index", rows=2)

        sub = row.column(align=True)
        sub.operator("render.netclientwhitelistslave", icon='ZOOMOUT', text="")

        if len(netrender.blacklist) > netsettings.active_blacklisted_slave_index >= 0:
            layout.separator()

            slave = netrender.blacklist[netsettings.active_blacklisted_slave_index]

            layout.label(text="Name: " + slave.name)
            layout.label(text="Address: " + slave.address[0])
            layout.label(text="Seen: " + time.ctime(slave.last_seen))
            layout.label(text="Stats: " + slave.stats)

class RENDER_PT_network_jobs(NeedValidAddress, NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Jobs"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return super().poll(context) and netsettings.mode == "RENDER_CLIENT"

    def draw(self, context):
        layout = self.layout

        netsettings = context.scene.network_render

        row = layout.row()
        row.template_list("UI_UL_list", "net_render", netsettings, "jobs", netsettings, "active_job_index", rows=2)

        sub = row.column(align=True)
        sub.operator("render.netclientstatus", icon='FILE_REFRESH', text="")
        sub.operator("render.netclientcancel", icon='ZOOMOUT', text="")
        sub.operator("render.netclientcancelall", icon='PANEL_CLOSE', text="")
        sub.operator("render.netclientdownload", icon='RENDER_ANIMATION', text="")

        if len(netrender.jobs) > netsettings.active_job_index >= 0:
            layout.separator()

            job = netrender.jobs[netsettings.active_job_index]

            layout.label(text="Name: %s" % job.name)
            layout.label(text="Length: %04i" % len(job))
            layout.label(text="Done: %04i" % job.results[netrender.model.FRAME_DONE])
            layout.label(text="Error: %04i" % job.results[netrender.model.FRAME_ERROR])

import bl_ui.properties_render as properties_render
class RENDER_PT_network_output(NeedValidAddress, NetRenderButtonsPanel, bpy.types.Panel):
    bl_label = "Output"
    COMPAT_ENGINES = {'NET_RENDER'}

    @classmethod
    def poll(cls, context):
        netsettings = context.scene.network_render
        return super().poll(context) and netsettings.mode == "RENDER_CLIENT"

    draw = properties_render.RENDER_PT_output.draw


class NetRenderSlave(bpy.types.PropertyGroup):
    @classmethod
    def register(NetRenderSlave):

        NetRenderSlave.name = StringProperty(
                        name="Name of the slave",
                        description="",
                        maxlen = 64,
                        default = "")

class NetRenderJob(bpy.types.PropertyGroup):
    @classmethod
    def register(NetRenderJob):

        NetRenderJob.name = StringProperty(
                        name="Name of the job",
                        description="",
                        maxlen = 128,
                        default = "")

class NetRenderSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(NetRenderSettings):

        def address_update_callback(self, context):
            netsettings = context.scene.network_render
            verify_address(netsettings, True)

        NetRenderSettings.server_address = StringProperty(
                        name="Server address",
                        description="IP or name of the master render server",
                        maxlen = 128,
                        default = "[default]",
                        update = address_update_callback)

        NetRenderSettings.server_port = IntProperty(
                        name="Server port",
                        description="port of the master render server",
                        default = 8000,
                        min=1,
                        max=65535)

        NetRenderSettings.use_master_broadcast = BoolProperty(
                        name="Broadcast",
                        description="broadcast master server address on local network",
                        default = True)
        NetRenderSettings.use_ssl = BoolProperty(
                        name="use ssl",
                        description="use ssl encryption for communication",
                        default = False)
        NetRenderSettings.cert_path = StringProperty(
                        name="CertPath",
                        description="Path to ssl certifcate",
                        maxlen = 128,
                        default = "",
                        subtype='FILE_PATH')
        NetRenderSettings.key_path = StringProperty(
                        name="key",
                        description="Path to ssl key file",
                        maxlen = 128,
                        default = "",
                        subtype='FILE_PATH')

        NetRenderSettings.use_slave_clear = BoolProperty(
                        name="Clear on exit",
                        description="delete downloaded files on exit",
                        default = True)

        NetRenderSettings.use_slave_thumb = BoolProperty(
                        name="Generate thumbnails",
                        description="Generate thumbnails on slaves instead of master",
                        default = False)

        NetRenderSettings.slave_tags = StringProperty(
                        name="Tags",
                        description="Tags to associate with the slave (semi-colon separated)",
                        maxlen = 256,
                        default = "")

        NetRenderSettings.use_slave_output_log = BoolProperty(
                        name="Output render log on console",
                        description="Output render text log to console as well as sending it to the master",
                        default = True)

        NetRenderSettings.slave_render = BoolProperty(
                        name="Render on slave",
                        description="Use slave for render jobs",
                        default = True)

        NetRenderSettings.slave_bake = BoolProperty(
                        name="Bake on slave",
                        description="Use slave for baking jobs",
                        default = True)

        NetRenderSettings.use_master_clear = BoolProperty(
                        name="Clear on exit",
                        description="Delete saved files on exit",
                        default = False)

        NetRenderSettings.use_master_force_upload = BoolProperty(
                        name="Force Dependency Upload",
                        description="Force client to upload dependency files to master",
                        default = False)

        default_path = os.environ.get("TEMP")

        if not default_path:
            if os.name == 'nt':
                default_path = "c:/tmp/"
            else:
                default_path = "/tmp/"
        elif not default_path.endswith(os.sep):
            default_path += os.sep

        NetRenderSettings.path = StringProperty(
                        name="Path",
                        description="Path for temporary files",
                        maxlen = 128,
                        default = default_path,
                        subtype='FILE_PATH')

        NetRenderSettings.job_type = EnumProperty(
                                items=(
                                                ("JOB_BLENDER", "Blender", "Standard Blender Job"),
                                                ("JOB_PROCESS", "Process", "Custom Process Job"),
                                                ("JOB_VCS", "VCS", "Version Control System Managed Job"),
                                            ),
                                name="Job Type",
                                description="Type of render job",
                                default="JOB_BLENDER")

        NetRenderSettings.job_name = StringProperty(
                        name="Job name",
                        description="Name of the job",
                        maxlen = 128,
                        default = "[default]")

        NetRenderSettings.job_category = StringProperty(
                        name="Job category",
                        description="Category of the job",
                        maxlen = 128,
                        default = "")

        NetRenderSettings.job_tags = StringProperty(
                        name="Tags",
                        description="Tags to associate with the job (semi-colon separated)",
                        maxlen = 256,
                        default = "")

        NetRenderSettings.job_render_engine = EnumProperty(
                                items = (
                                                ("BLENDER_RENDER", "BLENDER", "Standard Blender Render"),
                                                ("CYCLES", "CYCLES", "Cycle Render"),
                                                ("OTHER", "OTHER", "Other non-default Render"),
                                         ),
                                name="render",
                                description="Render engine used to render this job",
                                default="BLENDER_RENDER")

        NetRenderSettings.job_render_engine_other = StringProperty(
                        name="Render engine",
                        description="Render engine other than the builtin defaults (POVRAY_RENDER, ...)",
                        maxlen = 128,
                        default = "")

        NetRenderSettings.save_before_job = BoolProperty(
                        name="Save Before Job",
                        description="Save current file before sending a job",
                        default = False)

        NetRenderSettings.chunks = IntProperty(
                        name="Chunks",
                        description="Number of frame to dispatch to each slave in one chunk",
                        default = 5,
                        min=1,
                        max=65535)

        NetRenderSettings.priority = IntProperty(
                        name="Priority",
                        description="Priority of the job",
                        default = 1,
                        min=1,
                        max=10)

        NetRenderSettings.vcs_wpath = StringProperty(
                        name="Working Copy",
                        description="Path of the local working copy",
                        maxlen = 1024,
                        default = "")

        NetRenderSettings.vcs_rpath = StringProperty(
                        name="Remote Path",
                        description="Path of the server copy (protocol specific)",
                        maxlen = 1024,
                        default = "")

        NetRenderSettings.vcs_revision = StringProperty(
                        name="Revision",
                        description="Revision for this job",
                        maxlen = 256,
                        default = "")

        NetRenderSettings.vcs_system = EnumProperty(
                                items= netrender.versioning.ITEMS,
                                name="VCS mode",
                                description="Version Control System",
                                default=netrender.versioning.ITEMS[0][0])

        NetRenderSettings.job_id = StringProperty(
                        name="Network job id",
                        description="id of the last sent render job",
                        maxlen = 64,
                        default = "")

        NetRenderSettings.active_slave_index = IntProperty(
                        name="Index of the active slave",
                        description="",
                        default = -1,
                        min= -1,
                        max=65535)

        NetRenderSettings.active_blacklisted_slave_index = IntProperty(
                        name="Index of the active slave",
                        description="",
                        default = -1,
                        min= -1,
                        max=65535)

        NetRenderSettings.active_job_index = IntProperty(
                        name="Index of the active job",
                        description="",
                        default = -1,
                        min= -1,
                        max=65535)

        NetRenderSettings.mode = EnumProperty(
                                items=(
                                                ("RENDER_CLIENT", "Client", "Act as render client"),
                                                ("RENDER_MASTER", "Master", "Act as render master"),
                                                ("RENDER_SLAVE", "Slave", "Act as render slave"),
                                            ),
                                name="Network mode",
                                description="Mode of operation of this instance",
                                default="RENDER_CLIENT")

        NetRenderSettings.slaves = CollectionProperty(type=NetRenderSlave, name="Slaves", description="")
        NetRenderSettings.slaves_blacklist = CollectionProperty(type=NetRenderSlave, name="Slaves Blacklist", description="")
        NetRenderSettings.jobs = CollectionProperty(type=NetRenderJob, name="Job List", description="")

        bpy.types.Scene.network_render = PointerProperty(type=NetRenderSettings, name="Network Render", description="Network Render Settings")

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.network_render
