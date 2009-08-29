import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

from netrender.utils import *
import netrender.model

		
class MRenderSlave(netrender.model.RenderSlave):
	def __init__(self, name, adress, stats):
		super().__init__()
		self.id = hashlib.md5(bytes(repr(name) + repr(adress), encoding='utf8')).hexdigest()
		self.name = name
		self.adress = adress
		self.stats = stats
		self.last_seen = time.time()
		
		self.job = None
		self.frame = None
		
		netrender.model.RenderSlave._slave_map[self.id] = self

	def seen(self):
		self.last_seen = time.time()

# sorting key for jobs
def groupKey(job):
	return (job.framesLeft() > 0, job.priority, job.credits)

class MRenderJob(netrender.model.RenderJob):
	def __init__(self, job_id, name, path, chunks = 1, priority = 1, credits = 100.0, blacklist = []):
		super().__init__()
		self.id = job_id
		self.name = name
		self.path = path
		self.frames = []
		self.chunks = chunks
		self.priority = priority
		self.credits = credits
		self.blacklist = blacklist
		self.last_dispatched = time.time()
	
	def update(self):
		self.credits -= 5 # cost of one frame
		self.credits += (time.time() - self.last_dispatched) / 60
		self.last_dispatched = time.time()
	
	def addFrame(self, frame_number):
		frame = MRenderFrame(frame_number)
		self.frames.append(frame)
		return frame
	
	def framesLeft(self):
		total = 0
		for j in self.frames:
			if j.status == QUEUED:
				total += 1
		
		return total
		
	def reset(self, all):
		for f in self.frames:
			f.reset(all)
	
	def getFrames(self):
		frames = []
		for f in self.frames:
			if f.status == QUEUED:
				self.update()
				frames.append(f)
				if len(frames) == self.chunks:
					break
		
		return frames

class MRenderFrame(netrender.model.RenderFrame):
	def __init__(self, frame):
		super().__init__()
		self.number = frame
		self.slave = None
		self.time = 0
		self.status = QUEUED
		
	def reset(self, all):
		if all or self.status == ERROR:
			self.slave = None
			self.time = 0
			self.status = QUEUED


# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

class RenderHandler(http.server.BaseHTTPRequestHandler):
	def send_head(self, code = http.client.OK, headers = {}):
		self.send_response(code)
		self.send_header("Content-type", "application/octet-stream")
		
		for key, value in headers.items():
			self.send_header(key, value)
		
		self.end_headers()

	def do_HEAD(self):
		print(self.path)
	
		if self.path == "status":
			job_id = self.headers.get('job-id', "")
			job_frame = int(self.headers.get('job-frame', -1))
			
			if job_id:
				print("status:", job_id, "\n")
				
				job = self.server.getJobByID(job_id)
				if job:
					if job_frame != -1:
						frame = job[frame]
						
						if not frame:
							# no such frame
							self.send_heat(http.client.NOT_FOUND)
							return
				else:
					# no such job id
					self.send_head(http.client.NOT_FOUND)
					return
			
			self.send_head()
	
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	
	def do_GET(self):
		print(self.path)
		
		if self.path == "version":
			self.send_head()
			self.server.stats("", "New client connection")
			self.wfile.write(VERSION)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "render":
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			print("render:", job_id, job_frame)
			
			job = self.server.getJobByID(job_id)
			
			if job:
				frame = job[job_frame]
				
				if frame:
					if frame.status in (QUEUED, DISPATCHED):
						self.send_head(http.client.PROCESSING)
					elif frame.status == DONE:
						self.server.stats("", "Sending result back to client")
						f = open(PATH_PREFIX + job_id + "%04d" % job_frame + ".exr", 'rb')
						
						self.send_head()
						
						shutil.copyfileobj(f, self.wfile)
						
						f.close()
					elif frame.status == ERROR:
						self.send_head(http.client.NO_CONTENT)
				else:
					# no such frame
					self.send_head(http.client.NOT_FOUND)
			else:
				# no such job id
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "log":
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			print("log:", job_id, job_frame)
			
			job = self.server.getJobByID(job_id)
			
			if job:
				frame = job[job_frame]
				
				if frame:
					if frame.status in (QUEUED, DISPATCHED):
						self.send_head(http.client.PROCESSING)
					elif frame.status == DONE:
						self.server.stats("", "Sending log back to client")
						f = open(PATH_PREFIX + job_id + "%04d" % job_frame + ".log", 'rb')
						
						self.send_head()
						
						shutil.copyfileobj(f, self.wfile)
						
						f.close()
					elif frame.status == ERROR:
						self.send_head(http.client.NO_CONTENT)
				else:
					# no such frame
					self.send_head(http.client.NOT_FOUND)
			else:
				# no such job id
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "status":
			job_id = self.headers.get('job-id', "")
			job_frame = int(self.headers.get('job-frame', -1))
			
			if job_id:
				print("status:", job_id, "\n")
				
				job = self.server.getJobByID(job_id)
				if job:
					if job_frame != -1:
						frame = job[frame]
						
						if frame:
							message = frame.serialize()
						else:
							# no such frame
							self.send_heat(http.client.NOT_FOUND)
							return
					else:
						message = job.serialize()
				else:
					# no such job id
					self.send_head(http.client.NOT_FOUND)
					return
			else: # status of all jobs
				message = []
				
				for job in self.server:
					results = job.status()
					
					message.append(job.serialize())
			
			self.send_head()
			self.wfile.write(bytes(repr(message), encoding='utf8'))
			
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "job":
			self.server.update()
			
			slave_id = self.headers['slave-id']
			
			print("slave-id", slave_id)
			
			self.server.getSlave(slave_id)
			
			slave = self.server.updateSlave(slave_id)
			
			if slave: # only if slave id is valid
				job, frames = self.server.getNewJob(slave_id)
				
				if job and frames:
					for f in frames:
						f.status = DISPATCHED
						f.slave = slave
					
					self.send_head(headers={"job-id": job.id})
					
					message = job.serialize(frames)
					
					self.wfile.write(bytes(repr(message), encoding='utf8'))
					
					self.server.stats("", "Sending job frame to render node")
				else:
					# no job available, return error code
					self.send_head(http.client.NO_CONTENT)
			else: # invalid slave id
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "file":
			job_id = self.headers['job-id']
			print("file:", job_id, "\n")
			
			job = self.server.getJobByID(job_id)
			
			if job:
				self.send_head(headers={"job-id": job.id})
				
				self.server.stats("", "Sending file to render node")
				f = open(PATH_PREFIX + job.id + ".blend", 'rb')
				
				shutil.copyfileobj(f, self.wfile)
				
				f.close()
			else:
				# no such job id
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "slave":
			message = []
			
			for slave in self.server.slaves:
				message.append(slave.serialize())
			
			self.send_head()
			
			self.wfile.write(bytes(repr(message), encoding='utf8'))
			

	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	def do_POST(self):
		print(self.path)
	
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		if self.path == "job":
			print("posting job info")
			self.server.stats("", "Receiving job")
			
			length = int(self.headers['content-length'])
			job_frame_string = self.headers['job-frame']
			job_name = self.headers.get('job-name', "")
			job_chunks = int(self.headers.get('job-chunks', "1"))
			blacklist = self.headers.get('slave-blacklist', '').split()
			
			print("blacklist", blacklist)
			
			job_path = str(self.rfile.read(length), encoding='utf8')
			
			if os.path.exists(job_path):
				f = open(job_path, "rb")
				buf = f.read()
				f.close()
				
				job_id = hashlib.md5(buf).hexdigest()
				
				del buf
				
				job = self.server.getJobByID(job_id)
				
				if job == None:
					job = MRenderJob(job_id, job_name, job_path, chunks = job_chunks, blacklist = blacklist)
					self.server.addJob(job)
					
				if ":" in job_frame_string:
					frame_start, frame_end = [int(x) for x in job_frame_string.split(":")]
					
					for job_frame in range(frame_start, frame_end + 1):
						frame = job.addFrame(job_frame)
				else:
					job_frame = int(job_frame_string)
					frame = job.addFrame(job_frame)
		
				self.send_head(headers={"job-id": job_id})
			else:
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "cancel":
			job_id = self.headers.get('job-id', "")
			if job_id:
				print("cancel:", job_id, "\n")
				self.server.removeJob(job_id)
			else: # cancel all jobs
				self.server.clear()
				
			self.send_head()
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "reset":
			job_id = self.headers.get('job-id', "")
			job_frame = int(self.headers.get('job-frame', "-1"))
			all = bool(self.headers.get('reset-all', "False"))
			
			job = self.server.getJobByID(job_id)
			
			if job:
				if job_frame != -1:
					job[job_frame].reset(all)
				else:
					job.reset(all)
					
				self.send_head()
			else: # job not found
				self.send_head(http.client.NOT_FOUND)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "slave":
			length = int(self.headers['content-length'])
			job_frame_string = self.headers['job-frame']
			
			name, stats = eval(str(self.rfile.read(length), encoding='utf8'))
			
			slave_id = self.server.addSlave(name, self.client_address, stats)
			
			self.send_head(headers = {"slave-id": slave_id})
	
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	def do_PUT(self):
		print(self.path)
		
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		if self.path == "file":
			print("writing blend file")
			self.server.stats("", "Receiving job")
			
			length = int(self.headers['content-length'])
			job_frame_string = self.headers['job-frame']
			job_name = self.headers.get('job-name', "")
			job_chunks = int(self.headers.get('job-chunks', "1"))
			blacklist = self.headers.get('slave-blacklist', '').split()
			
			buf = self.rfile.read(length)
			
			job_id = hashlib.md5(buf).hexdigest()
			
			job_path = job_id + ".blend"
			
			f = open(PATH_PREFIX + job_path, "wb")
			f.write(buf)
			f.close()
			
			del buf
			
			job = self.server.getJobByID(job_id)
			
			if job == None:
				job = MRenderJob(job_id, job_name, job_path, chunks = job_chunks, blacklist = blacklist)
				self.server.addJob(job)
				
			if ":" in job_frame_string:
				frame_start, frame_end = [int(x) for x in job_frame_string.split(":")]
				
				for job_frame in range(frame_start, frame_end + 1):
					frame = job.addFrame(job_frame)
			else:
				job_frame = int(job_frame_string)
				frame = job.addFrame(job_frame)
	
			self.send_head(headers={"job-id": job_id})
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "render":
			print("writing result file")
			self.server.stats("", "Receiving render result")
			
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			job_result = int(self.headers['job-result'])
			job_time = float(self.headers['job-time'])
			
			if job_result == DONE:
				length = int(self.headers['content-length'])
				buf = self.rfile.read(length)
				f = open(PATH_PREFIX + job_id + "%04d" % job_frame + ".exr", 'wb')
				f.write(buf)
				f.close()
				
				del buf
				
			job = self.server.getJobByID(job_id)
			frame = job[job_frame]
			frame.status = job_result
			frame.time = job_time
	
			self.server.updateSlave(self.headers['slave-id'])
			
			self.send_head()
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "log":
			print("writing log file")
			self.server.stats("", "Receiving log file")
			
			length = int(self.headers['content-length'])
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			
			print("log length:", length)
			
			buf = self.rfile.read(length)
			f = open(PATH_PREFIX + job_id + "%04d" % job_frame + ".log", 'wb')
			f.write(buf)
			f.close()
				
			del buf
			
			self.server.updateSlave(self.headers['slave-id'])
			
			self.send_head()


class RenderMasterServer(http.server.HTTPServer):
	def __init__(self, address, handler_class):
		super().__init__(address, handler_class)
		self.jobs = []
		self.jobs_map = {}
		self.slaves = []
		self.slaves_map = {}
	
	def addSlave(self, name, adress, stats):
		slave = MRenderSlave(name, adress, stats)
		self.slaves.append(slave)
		self.slaves_map[slave.id] = slave
		
		return slave.id
	
	def getSlave(self, slave_id):
		return self.slaves_map.get(slave_id, None)
	
	def updateSlave(self, slave_id):
		slave = self.getSlave(slave_id)
		if slave:
			slave.seen()
			
		return slave
	
	def clear(self):
		self.jobs_map = {}
		self.jobs = []
	
	def update(self):
		self.jobs.sort(key = groupKey)
		
	def removeJob(self, id):
		job = self.jobs_map.pop(id)

		if job:
			self.jobs.remove(job)
	
	def addJob(self, job):
		self.jobs.append(job)
		self.jobs_map[job.id] = job
	
	def getJobByID(self, id):
		return self.jobs_map.get(id, None)
	
	def __iter__(self):
		for job in self.jobs:
			yield job
	
	def getNewJob(self, slave_id):
		if self.jobs:
			for job in reversed(self.jobs):
				if job.framesLeft() > 0 and slave_id not in job.blacklist:
					return job, job.getFrames()
		
		return None, None
