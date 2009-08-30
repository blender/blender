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
		netprops = context.scene.network_render
		conn = clientConnection(context.scene)

		if conn:
			conn.request("GET", "status")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			jobs = (netrender.model.RenderJob.materialize(j) for j in eval(str(response.read(), encoding='utf8')))
			
			while(len(netprops.jobs) > 0):
				netprops.jobs.remove(0)
				
			for j in jobs:
				netprops.jobs.add()
				job = netprops.jobs[-1]
				
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
		netprops = context.scene.network_render
		
		if netprops.active_slave_index >= 0:
			
			slave = netrender.slaves[netprops.active_slave_index]
			
			netprops.slaves_blacklist.add()
			
			netprops.slaves_blacklist[-1].id = slave.id
			netprops.slaves_blacklist[-1].name = slave.name
			netprops.slaves_blacklist[-1].adress = slave.adress
			netprops.slaves_blacklist[-1].last_seen = slave.last_seen
			netprops.slaves_blacklist[-1].stats = slave.stats
			
			netprops.slaves.remove(netprops.active_slave_index)
			netprops.active_slave_index = -1
		
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
		netprops = context.scene.network_render
		
		if netprops.active_blacklisted_slave_index >= 0:
			
			slave = netprops.slaves_blacklist[netprops.active_blacklisted_slave_index]
			
			netprops.slaves.add()
			
			netprops.slaves[-1].id = slave.id
			netprops.slaves[-1].name = slave.name
			netprops.slaves[-1].adress = slave.adress
			netprops.slaves[-1].last_seen = slave.last_seen
			netprops.slaves[-1].stats = slave.stats
			
			netprops.slaves_blacklist.remove(netprops.active_blacklisted_slave_index)
			netprops.active_blacklisted_slave_index = -1
		
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
		netprops = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			conn.request("GET", "slave")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			slaves = (netrender.model.RenderSlave.materialize(s) for s in eval(str(response.read(), encoding='utf8')))
			
			while(len(netprops.slaves) > 0):
				netprops.slaves.remove(0)
			
			for s in slaves:
				for slave in netprops.slaves_blacklist:
					if slave.id == s.id:
						break
					
				netprops.slaves.add()
				slave = netprops.slaves[-1]
				
				slave.id = s.id
				slave.name = s.name
				slave.stats = s.stats
				slave.adress = s.adress[0]
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
		netprops = context.scene.network_render
		return netprops.active_job_index >= 0 and len(netprops.jobs) > 0
		
	def execute(self, context):
		netprops = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			job = netprops.jobs[netprops.active_job_index]
			
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