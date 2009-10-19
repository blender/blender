import bpy
import sys, os
import re
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.model

VERSION = b"0.3"

QUEUED = 0
DISPATCHED = 1
DONE = 2
ERROR = 3

def clientConnection(scene):
		netrender = scene.network_render
		
		conn = http.client.HTTPConnection(netrender.server_address, netrender.server_port)
		
		if clientVerifyVersion(conn):
			return conn
		else:
			conn.close()
			return None

def clientVerifyVersion(conn):
	conn.request("GET", "version")
	response = conn.getresponse()
	
	if response.status != http.client.OK:
		conn.close()
		return False
	
	server_version = response.read()
	
	if server_version != VERSION:
		print("Incorrect server version!")
		print("expected", VERSION, "received", server_version)
		return False
	
	return True

def clientSendJob(conn, scene, anim = False, chunks = 5):
	netsettings = scene.network_render
	job = netrender.model.RenderJob()
	
	if anim:
		for f in range(scene.start_frame, scene.end_frame + 1):
			job.addFrame(f)
	else:
		job.addFrame(scene.current_frame)
	
	filename = bpy.data.filename
	job.files.append(filename)
	
	job_name = netsettings.job_name
	path, name = os.path.split(filename)
	if job_name == "[default]":
		job_name = name
	
	for lib in bpy.data.libraries:
		lib_path = lib.filename
		
		if lib_path.startswith("//"):
			lib_path = path + os.sep + lib_path[2:]
			
		job.files.append(lib_path)
	
	root, ext = os.path.splitext(name)
	cache_path = path + os.sep + "blendcache_" + root + os.sep # need an API call for that
	
	print("cache:", cache_path)
	
	if os.path.exists(cache_path):
		pattern = re.compile("[a-zA-Z0-9]+_([0-9]+)_[0-9]+\.bphys")
		for cache_name in sorted(os.listdir(cache_path)):
			match = pattern.match(cache_name)
			
			if match:
				print("Frame:", int(match.groups()[0]), cache_name)
			
			job.files.append(cache_path + cache_name)
		
	#print(job.files)
	
	job.name = job_name
	
	for slave in scene.network_render.slaves_blacklist:
		job.blacklist.append(slave.id)
	
	job.chunks = netsettings.chunks
	job.priority = netsettings.priority
	
	# try to send path first
	conn.request("POST", "job", repr(job.serialize()))
	response = conn.getresponse()
	
	job_id = response.getheader("job-id")
	
	# if not ACCEPTED (but not processed), send files
	if response.status == http.client.ACCEPTED:
		for filepath in job.files:
			f = open(filepath, "rb")
			conn.request("PUT", "file", f, headers={"job-id": job_id, "job-file": filepath})
			f.close()
			response = conn.getresponse()
	
	# server will reply with NOT_FOUD until all files are found
	
	return job_id

def clientRequestResult(conn, scene, job_id):
	conn.request("GET", "render", headers={"job-id": job_id, "job-frame":str(scene.current_frame)})


def prefixPath(prefix_directory, file_path, prefix_path):
	if os.path.isabs(file_path):
		# if an absolute path, make sure path exists, if it doesn't, use relative local path
		full_path = file_path
		if not os.path.exists(full_path):
			p, n = os.path.split(full_path)
			
			if main_path and p.startswith(main_path):
				directory = prefix_directory + p[len(main_path):]
				full_path = directory + n
				if not os.path.exists(directory):
					os.mkdir(directory)
			else:
				full_path = prefix_directory + n
	else:
		full_path = prefix_directory + file_path
	
	return full_path