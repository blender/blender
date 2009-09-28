import bpy
import sys, os, re
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.slave as slave
import netrender.master as master
from netrender.utils import *


def clientSendJob(conn, scene, anim = False, chunks = 5):
	netsettings = scene.network_render
	job = netrender.model.RenderJob()
	
	if anim:
		for f in range(scene.start_frame, scene.end_frame + 1):
			job.addFrame(f)
	else:
		job.addFrame(scene.current_frame)
	
	filename = bpy.data.filename
	job.addFile(filename)
	
	job_name = netsettings.job_name
	path, name = os.path.split(filename)
	if job_name == "[default]":
		job_name = name
	
	###########################
	# LIBRARIES
	###########################
	for lib in bpy.data.libraries:
		lib_path = lib.filename
		
		if lib_path.startswith("//"):
			lib_path = path + os.sep + lib_path[2:]
			
		job.addFile(lib_path)
	
	###########################
	# POINT CACHES
	###########################
	
	root, ext = os.path.splitext(name)
	cache_path = path + os.sep + "blendcache_" + root + os.sep # need an API call for that
	
	if os.path.exists(cache_path):
		caches = {}
		pattern = re.compile("([a-zA-Z0-9]+)_([0-9]+)_[0-9]+\.bphys")
		for cache_file in sorted(os.listdir(cache_path)):
			match = pattern.match(cache_file)
			
			if match:
				cache_id = match.groups()[0]
				cache_frame = int(match.groups()[1])
					
				cache_files = caches.get(cache_id, [])
				cache_files.append((cache_frame, cache_file))
				caches[cache_id] = cache_files
				
		for cache in caches.values():
			cache.sort()
			
			if len(cache) == 1:
				cache_frame, cache_file = cache[0]
				job.addFile(cache_path + cache_file, cache_frame, cache_frame)
			else:
				for i in range(len(cache)):
					current_item = cache[i]
					next_item = cache[i+1] if i + 1 < len(cache) else None
					previous_item = cache[i - 1] if i > 0 else None
					
					current_frame, current_file = current_item
					
					if  not next_item and not previous_item:
						job.addFile(cache_path + current_file, current_frame, current_frame)
					elif next_item and not previous_item:
						next_frame = next_item[0]
						job.addFile(cache_path + current_file, current_frame, next_frame - 1)
					elif not next_item and previous_item:
						previous_frame = previous_item[0]
						job.addFile(cache_path + current_file, previous_frame + 1, current_frame)
					else:
						next_frame = next_item[0]
						previous_frame = previous_item[0]
						job.addFile(cache_path + current_file, previous_frame + 1, next_frame - 1)
		
	###########################
	# IMAGES
	###########################
	for image in bpy.data.images:
		if image.source == "FILE" and not image.packed_file:
			job.addFile(image.filename)
	
	# print(job.files)
	
	job.name = job_name
	
	for slave in scene.network_render.slaves_blacklist:
		job.blacklist.append(slave.id)
	
	job.chunks = netsettings.chunks
	job.priority = netsettings.priority
	
	# try to send path first
	conn.request("POST", "/job", repr(job.serialize()))
	response = conn.getresponse()
	
	job_id = response.getheader("job-id")
	
	# if not ACCEPTED (but not processed), send files
	if response.status == http.client.ACCEPTED:
		for filepath, start, end in job.files:
			f = open(filepath, "rb")
			conn.request("PUT", "/file", f, headers={"job-id": job_id, "job-file": filepath})
			f.close()
			response = conn.getresponse()
	
	# server will reply with NOT_FOUD until all files are found
	
	return job_id

def requestResult(conn, job_id, frame):
	conn.request("GET", "/render", headers={"job-id": job_id, "job-frame":str(frame)})

@rnaType
class NetworkRenderEngine(bpy.types.RenderEngine):
	__idname__ = 'NET_RENDER'
	__label__ = "Network Render"
	def render(self, scene):
		if scene.network_render.mode == "RENDER_CLIENT":
			self.render_client(scene)
		elif scene.network_render.mode == "RENDER_SLAVE":
			self.render_slave(scene)
		elif scene.network_render.mode == "RENDER_MASTER":
			self.render_master(scene)
		else:
			print("UNKNOWN OPERATION MODE")
	
	def render_master(self, scene):
		netsettings = scene.network_render
		
		address = "" if netsettings.server_address == "[default]" else netsettings.server_address
		
		master.runMaster((address, netsettings.server_port), netsettings.server_broadcast, netsettings.path, self.update_stats, self.test_break)


	def render_slave(self, scene):
		slave.render_slave(self, scene)
	
	def render_client(self, scene):
		netsettings = scene.network_render
		self.update_stats("", "Network render client initiation")
		
		
		conn = clientConnection(scene)
		
		if conn:
			# Sending file
			
			self.update_stats("", "Network render exporting")
			
			job_id = netsettings.job_id
			
			# reading back result
			
			self.update_stats("", "Network render waiting for results")
			
			requestResult(conn, job_id, scene.current_frame)
			response = conn.getresponse()
			
			if response.status == http.client.NO_CONTENT:
				netsettings.job_id = clientSendJob(conn, scene)
				requestResult(conn, job_id, scene.current_frame)
			
			while response.status == http.client.ACCEPTED and not self.test_break():
				time.sleep(1)
				requestResult(conn, job_id, scene.current_frame)
				response = conn.getresponse()
	
			if response.status != http.client.OK:
				conn.close()
				return
			
			r = scene.render_data
			x= int(r.resolution_x*r.resolution_percentage*0.01)
			y= int(r.resolution_y*r.resolution_percentage*0.01)
			
			f = open(netsettings.path + "output.exr", "wb")
			buf = response.read(1024)
			
			while buf:
				f.write(buf)
				buf = response.read(1024)
			
			f.close()
			
			result = self.begin_result(0, 0, x, y)
			result.load_from_file(netsettings.path + "output.exr", 0, 0)
			self.end_result(result)
			
			conn.close()

