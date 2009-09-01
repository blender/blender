import bpy
import sys, os
import http, http.client, http.server, urllib

from netrender.utils import *
import netrender.model

class RENDER_OT_netclientsend(bpy.types.Operator):
	'''
	Operator documentation text, will be used for the operator tooltip and python docs.
	'''
	__idname__ = "render.netclientsend"
	__label__ = "Net Render Client Send"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		scene = context.scene
		
		conn = clientConnection(scene)
		
		if conn:
			# Sending file
			scene.network_render.job_id = clientSendJob(conn, scene, True)
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

class RENDER_OT_netclientstatus(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientstatus"
	__label__ = "Net Render Client Status"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)

		if conn:
			conn.request("GET", "status")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			jobs = (netrender.model.RenderJob.materialize(j) for j in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)
				
			for j in jobs:
				netsettings.jobs.add()
				job = netsettings.jobs[-1]
				
				job_results = j.framesStatus()
				
				job.id = j.id
				job.name = j.name
				job.length = len(j)
				job.done = job_results[DONE]
				job.error = job_results[ERROR]
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

class RENDER_OT_netclientblacklistslave(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientblacklistslave"
	__label__ = "Net Render Client Blacklist Slave"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_slave_index >= 0:
			
			slave = netrender.slaves[netsettings.active_slave_index]
			
			netsettings.slaves_blacklist.add()
			
			netsettings.slaves_blacklist[-1].id = slave.id
			netsettings.slaves_blacklist[-1].name = slave.name
			netsettings.slaves_blacklist[-1].address = slave.address
			netsettings.slaves_blacklist[-1].last_seen = slave.last_seen
			netsettings.slaves_blacklist[-1].stats = slave.stats
			
			netsettings.slaves.remove(netsettings.active_slave_index)
			netsettings.active_slave_index = -1
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

class RENDER_OT_netclientwhitelistslave(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientwhitelistslave"
	__label__ = "Net Render Client Whitelist Slave"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_blacklisted_slave_index >= 0:
			
			slave = netsettings.slaves_blacklist[netsettings.active_blacklisted_slave_index]
			
			netsettings.slaves.add()
			
			netsettings.slaves[-1].id = slave.id
			netsettings.slaves[-1].name = slave.name
			netsettings.slaves[-1].address = slave.address
			netsettings.slaves[-1].last_seen = slave.last_seen
			netsettings.slaves[-1].stats = slave.stats
			
			netsettings.slaves_blacklist.remove(netsettings.active_blacklisted_slave_index)
			netsettings.active_blacklisted_slave_index = -1
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)


class RENDER_OT_netclientslaves(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientslaves"
	__label__ = "Net Render Client Slaves"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			conn.request("GET", "slave")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			slaves = (netrender.model.RenderSlave.materialize(s) for s in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.slaves) > 0):
				netsettings.slaves.remove(0)
			
			for s in slaves:
				for slave in netsettings.slaves_blacklist:
					if slave.id == s.id:
						break
					
				netsettings.slaves.add()
				slave = netsettings.slaves[-1]
				
				slave.id = s.id
				slave.name = s.name
				slave.stats = s.stats
				slave.address = s.address[0]
				slave.last_seen = time.ctime(s.last_seen)
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

class RENDER_OT_netclientcancel(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientcancel"
	__label__ = "Net Render Client Cancel"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		netsettings = context.scene.network_render
		return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0
		
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			job = netsettings.jobs[netsettings.active_job_index]
			
			conn.request("POST", "cancel", headers={"job-id":job.id})
			
			response = conn.getresponse()
			print( response.status, response.reason )
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)
	
bpy.ops.add(RENDER_OT_netclientsend)
bpy.ops.add(RENDER_OT_netclientstatus)
bpy.ops.add(RENDER_OT_netclientslaves)
bpy.ops.add(RENDER_OT_netclientblacklistslave)
bpy.ops.add(RENDER_OT_netclientwhitelistslave)
bpy.ops.add(RENDER_OT_netclientcancel)