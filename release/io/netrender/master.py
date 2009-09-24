import sys, os
import http, http.client, http.server, urllib, socket
import subprocess, shutil, time, hashlib

from netrender.utils import *
import netrender.model
import netrender.balancing
import netrender.master_html

class MRenderFile:
	def __init__(self, filepath, start, end):
		self.filepath = filepath
		self.start = start
		self.end = end
		self.found = False
	
	def test(self):
		self.found = os.path.exists(self.filepath)
		return self.found


class MRenderSlave(netrender.model.RenderSlave):
	def __init__(self, name, address, stats):
		super().__init__()
		self.id = hashlib.md5(bytes(repr(name) + repr(address), encoding='utf8')).hexdigest()
		self.name = name
		self.address = address
		self.stats = stats
		self.last_seen = time.time()
		
		self.job = None
		self.job_frames = []
		
		netrender.model.RenderSlave._slave_map[self.id] = self

	def seen(self):
		self.last_seen = time.time()

class MRenderJob(netrender.model.RenderJob):
	def __init__(self, job_id, name, files, chunks = 1, priority = 1, credits = 100.0, blacklist = []):
		super().__init__()
		self.id = job_id
		self.name = name
		self.files = files
		self.frames = []
		self.chunks = chunks
		self.priority = priority
		self.credits = credits
		self.blacklist = blacklist
		self.last_dispatched = time.time()
	
		# special server properties
		self.usage = 0.0
		self.last_update = 0
		self.save_path = ""
		self.files_map = {path: MRenderFile(path, start, end) for path, start, end in files}
		self.status = JOB_WAITING
	
	def save(self):
		if self.save_path:
			f = open(self.save_path + "job.txt", "w")
			f.write(repr(self.serialize()))
			f.close()
	
	def testStart(self):
		for f in self.files_map.values():
			if not f.test():
				return False
		
		self.start()
		return True
	
	def testFinished(self):
		for f in self.frames:
			if f.status == QUEUED or f.status == DISPATCHED:
				break
		else:
			self.status = JOB_FINISHED

	def start(self):
		self.status = JOB_QUEUED

	def update(self):
		if self.last_update == 0:
			self.credits += (time.time() - self.last_dispatched) / 60
		else:
			self.credits += (time.time() - self.last_update) / 60

		self.last_update = time.time()
	
	def addLog(self, frames):
		log_name = "_".join(("%04d" % f for f in frames)) + ".log"
		log_path = self.save_path + log_name
		
		for number in frames:
			frame = self[number]
			if frame:
				frame.log_path = log_path
	
	def addFrame(self, frame_number):
		frame = MRenderFrame(frame_number)
		self.frames.append(frame)
		return frame
		
	def reset(self, all):
		for f in self.frames:
			f.reset(all)
	
	def getFrames(self):
		frames = []
		for f in self.frames:
			if f.status == QUEUED:
				self.credits -= 1 # cost of one frame
				self.last_dispatched = time.time()
				frames.append(f)
				if len(frames) >= self.chunks:
					break
		
		return frames

class MRenderFrame(netrender.model.RenderFrame):
	def __init__(self, frame):
		super().__init__()
		self.number = frame
		self.slave = None
		self.time = 0
		self.status = QUEUED
		self.log_path = None
		
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
	def send_head(self, code = http.client.OK, headers = {}, content = "application/octet-stream"):
		self.send_response(code)
		self.send_header("Content-type", content)
		
		for key, value in headers.items():
			self.send_header(key, value)
		
		self.end_headers()

	def do_HEAD(self):
		print(self.path)
	
		if self.path == "/status":
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
							self.send_heat(http.client.NO_CONTENT)
							return
				else:
					# no such job id
					self.send_head(http.client.NO_CONTENT)
					return
			
			self.send_head()
	
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	
	def do_GET(self):
		print(self.path)
		
		if self.path == "/version":
			self.send_head()
			self.server.stats("", "New client connection")
			self.wfile.write(VERSION)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/render":
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			print("render:", job_id, job_frame)
			
			job = self.server.getJobByID(job_id)
			
			if job:
				frame = job[job_frame]
				
				if frame:
					if frame.status in (QUEUED, DISPATCHED):
						self.send_head(http.client.ACCEPTED)
					elif frame.status == DONE:
						self.server.stats("", "Sending result back to client")
						f = open(job.save_path + "%04d" % job_frame + ".exr", 'rb')
						
						self.send_head()
						
						shutil.copyfileobj(f, self.wfile)
						
						f.close()
					elif frame.status == ERROR:
						self.send_head(http.client.PARTIAL_CONTENT)
				else:
					# no such frame
					self.send_head(http.client.NO_CONTENT)
			else:
				# no such job id
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/log":
			job_id = self.headers['job-id']
			job_frame = int(self.headers['job-frame'])
			print("log:", job_id, job_frame)
			
			job = self.server.getJobByID(job_id)
			
			if job:
				frame = job[job_frame]
				
				if frame:
					if not frame.log_path or frame.status in (QUEUED, DISPATCHED):
						self.send_head(http.client.PROCESSING)
					else:
						self.server.stats("", "Sending log back to client")
						f = open(frame.log_path, 'rb')
						
						self.send_head()
						
						shutil.copyfileobj(f, self.wfile)
						
						f.close()
				else:
					# no such frame
					self.send_head(http.client.NO_CONTENT)
			else:
				# no such job id
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/status":
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
							self.send_heat(http.client.NO_CONTENT)
							return
					else:
						message = job.serialize()
				else:
					# no such job id
					self.send_head(http.client.NO_CONTENT)
					return
			else: # status of all jobs
				message = []
				
				for job in self.server:
					message.append(job.serialize())
			
			self.send_head()
			self.wfile.write(bytes(repr(message), encoding='utf8'))
			
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/job":
			self.server.update()
			
			slave_id = self.headers['slave-id']
			
			print("slave-id", slave_id)
			
			slave = self.server.updateSlave(slave_id)
			
			if slave: # only if slave id is valid
				job, frames = self.server.getNewJob(slave_id)
				
				if job and frames:
					for f in frames:
						print("dispatch", f.number)
						f.status = DISPATCHED
						f.slave = slave
					
					slave.job = job
					slave.job_frames = [f.number for f in frames]
					
					self.send_head(headers={"job-id": job.id})
					
					message = job.serialize(frames)
					
					self.wfile.write(bytes(repr(message), encoding='utf8'))
					
					self.server.stats("", "Sending job frame to render node")
				else:
					# no job available, return error code
					self.send_head(http.client.ACCEPTED)
			else: # invalid slave id
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/file":
			slave_id = self.headers['slave-id']
			
			slave = self.server.updateSlave(slave_id)
			
			if slave: # only if slave id is valid
				job_id = self.headers['job-id']
				job_file = self.headers['job-file']
				print("job:", job_id, "\n")
				print("file:", job_file, "\n")
				
				job = self.server.getJobByID(job_id)
				
				if job:
					render_file = job.files_map.get(job_file, None)
					
					if render_file:
						self.server.stats("", "Sending file to render node")
						f = open(render_file.filepath, 'rb')
						
						self.send_head()
						shutil.copyfileobj(f, self.wfile)
						
						f.close()
					else:
						# no such file
						self.send_head(http.client.NO_CONTENT)
				else:
					# no such job id
					self.send_head(http.client.NO_CONTENT)
			else: # invalid slave id
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/slave":
			message = []
			
			for slave in self.server.slaves:
				message.append(slave.serialize())
			
			self.send_head()
			
			self.wfile.write(bytes(repr(message), encoding='utf8'))
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		else:
			# hand over the rest to the html section
			netrender.master_html.get(self)

	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	def do_POST(self):
		print(self.path)
	
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		if self.path == "/job":
			print("posting job info")
			self.server.stats("", "Receiving job")
			
			length = int(self.headers['content-length'])
			
			job_info = netrender.model.RenderJob.materialize(eval(str(self.rfile.read(length), encoding='utf8')))
			
			job_id = self.server.nextJobID()
			
			print(job_info.files)
			
			job = MRenderJob(job_id, job_info.name, job_info.files, chunks = job_info.chunks, priority = job_info.priority, blacklist = job_info.blacklist)
			
			for frame in job_info.frames:
				frame = job.addFrame(frame.number)
			
			self.server.addJob(job)
			
			headers={"job-id": job_id}
			
			if job.testStart():
				self.send_head(headers=headers)
			else:
				self.send_head(http.client.ACCEPTED, headers=headers)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/cancel":
			job_id = self.headers.get('job-id', "")
			if job_id:
				print("cancel:", job_id, "\n")
				self.server.removeJob(job_id)
			else: # cancel all jobs
				self.server.clear()
				
			self.send_head()
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/reset":
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
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/slave":
			length = int(self.headers['content-length'])
			job_frame_string = self.headers['job-frame']
			
			slave_info = netrender.model.RenderSlave.materialize(eval(str(self.rfile.read(length), encoding='utf8')))
			
			slave_id = self.server.addSlave(slave_info.name, self.client_address, slave_info.stats)
			
			self.send_head(headers = {"slave-id": slave_id})
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/log":
			slave_id = self.headers['slave-id']
			
			slave = self.server.updateSlave(slave_id)
			
			if slave: # only if slave id is valid
				length = int(self.headers['content-length'])
				
				log_info = netrender.model.LogFile.materialize(eval(str(self.rfile.read(length), encoding='utf8')))
				
				job = self.server.getJobByID(log_info.job_id)
				
				if job:
					job.addLog(log_info.frames)
					self.send_head(http.client.OK)
				else:
					# no such job id
					self.send_head(http.client.NO_CONTENT)
			else: # invalid slave id
				self.send_head(http.client.NO_CONTENT)	
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	def do_PUT(self):
		print(self.path)
		
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		if self.path == "/file":
			print("writing blend file")
			self.server.stats("", "Receiving job")
			
			length = int(self.headers['content-length'])
			job_id = self.headers['job-id']
			job_file = self.headers['job-file']
			
			job = self.server.getJobByID(job_id)
			
			if job:
				
				render_file = job.files_map.get(job_file, None)
				
				if render_file:
					main_file = job.files[0][0] # filename of the first file
					
					main_path, main_name = os.path.split(main_file)
					
					if job_file != main_file:
						file_path = prefixPath(job.save_path, job_file, main_path)
					else:
						file_path = job.save_path + main_name
					
					buf = self.rfile.read(length)
					
					# add same temp file + renames as slave
					
					f = open(file_path, "wb")
					f.write(buf)
					f.close()
					del buf
					
					render_file.filepath = file_path # set the new path
					
					if job.testStart():
						self.send_head(http.client.OK)
					else:
						self.send_head(http.client.ACCEPTED)
				else: # invalid file
					self.send_head(http.client.NO_CONTENT)
			else: # job not found
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/render":
			print("writing result file")
			self.server.stats("", "Receiving render result")
			
			slave_id = self.headers['slave-id']
			
			slave = self.server.updateSlave(slave_id)
			
			if slave: # only if slave id is valid
				job_id = self.headers['job-id']
				
				job = self.server.getJobByID(job_id)
				
				if job:
					job_frame = int(self.headers['job-frame'])
					job_result = int(self.headers['job-result'])
					job_time = float(self.headers['job-time'])
					
					frame = job[job_frame]

					if job_result == DONE:
						length = int(self.headers['content-length'])
						buf = self.rfile.read(length)
						f = open(job.save_path + "%04d" % job_frame + ".exr", 'wb')
						f.write(buf)
						f.close()
						
						del buf
					elif job_result == ERROR:
						# blacklist slave on this job on error
						job.blacklist.append(slave.id)
					
					slave.job_frames.remove(job_frame)
					if not slave.job_frames:
						slave.job = None
					
					frame.status = job_result
					frame.time = job_time

					job.testFinished()
			
					self.server.updateSlave(self.headers['slave-id'])
					
					self.send_head()
				else: # job not found
					self.send_head(http.client.NO_CONTENT)
			else: # invalid slave id
				self.send_head(http.client.NO_CONTENT)
		# =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
		elif self.path == "/log":
			print("writing log file")
			self.server.stats("", "Receiving log file")
			
			job_id = self.headers['job-id']
			
			job = self.server.getJobByID(job_id)
			
			if job:
				job_frame = int(self.headers['job-frame'])
				
				frame = job[job_frame]
				
				if frame and frame.log_path:
					length = int(self.headers['content-length'])
					buf = self.rfile.read(length)
					f = open(frame.log_path, 'ab')
					f.write(buf)
					f.close()
						
					del buf
					
					self.server.updateSlave(self.headers['slave-id'])
					
					self.send_head()
				else: # frame not found
					self.send_head(http.client.NO_CONTENT)
			else: # job not found
				self.send_head(http.client.NO_CONTENT)

class RenderMasterServer(http.server.HTTPServer):
	def __init__(self, address, handler_class, path):
		super().__init__(address, handler_class)
		self.jobs = []
		self.jobs_map = {}
		self.slaves = []
		self.slaves_map = {}
		self.job_id = 0
		self.path = path + "master_" + str(os.getpid()) + os.sep
		
		self.slave_timeout = 2
		
		self.first_usage = True
		
		self.balancer = netrender.balancing.Balancer()
		self.balancer.addRule(netrender.balancing.RatingCredit())
		self.balancer.addException(netrender.balancing.ExcludeQueuedEmptyJob())
		self.balancer.addException(netrender.balancing.ExcludeSlavesLimit(self.countJobs, self.countSlaves))
		self.balancer.addPriority(netrender.balancing.NewJobPriority())
		self.balancer.addPriority(netrender.balancing.MinimumTimeBetweenDispatchPriority(limit = 2))
		
		if not os.path.exists(self.path):
			os.mkdir(self.path)
	
	def nextJobID(self):
		self.job_id += 1
		return str(self.job_id)
	
	def addSlave(self, name, address, stats):
		slave = MRenderSlave(name, address, stats)
		self.slaves.append(slave)
		self.slaves_map[slave.id] = slave
		
		return slave.id
	
	def removeSlave(self, slave):
		self.slaves.remove(slave)
		self.slaves_map.pop(slave.id)
	
	def getSlave(self, slave_id):
		return self.slaves_map.get(slave_id, None)
	
	def updateSlave(self, slave_id):
		slave = self.getSlave(slave_id)
		if slave:
			slave.seen()
			
		return slave
	
	def timeoutSlaves(self):
		removed = []
		
		t = time.time()
		
		for slave in self.slaves:
			if (t - slave.last_seen) / 60 > self.slave_timeout:
				removed.append(slave)
				
				if slave.job:
					for f in slave.job_frames:
						slave.job[f].status = ERROR
				
		for slave in removed:
			self.removeSlave(slave)
	
	def updateUsage(self):
		m = 1.0
		
		if not self.first_usage:
			for job in self.jobs:
				job.usage *= 0.5
			
			m = 0.5
		else:
			self.first_usage = False
			
		if self.slaves:
			slave_usage = m / self.countSlaves()
			
			for slave in self.slaves:
				if slave.job:
					slave.job.usage += slave_usage
		
	
	def clear(self):
		removed = self.jobs[:]
		
		for job in removed:
			self.removeJob(job)
	
	def update(self):
		for job in self.jobs:
			job.update()
		self.balancer.balance(self.jobs)
	
	def countJobs(self, status = JOB_QUEUED):
		total = 0
		for j in self.jobs:
			if j.status == status:
				total += 1
		
		return total
	
	def countSlaves(self):
		return len(self.slaves)
	
	def removeJob(self, id):
		job = self.jobs_map.pop(id)

		if job:
			self.jobs.remove(job)
			
			for slave in self.slaves:
				if slave.job == job:
					slave.job = None
					slave.job_frames = []
	
	def addJob(self, job):
		self.jobs.append(job)
		self.jobs_map[job.id] = job
		
		# create job directory
		job.save_path = self.path + "job_" + job.id + os.sep
		if not os.path.exists(job.save_path):
			os.mkdir(job.save_path)
			
		job.save()
	
	def getJobByID(self, id):
		return self.jobs_map.get(id, None)
	
	def __iter__(self):
		for job in self.jobs:
			yield job
	
	def getNewJob(self, slave_id):
		if self.jobs:
			for job in self.jobs:
				if not self.balancer.applyExceptions(job) and slave_id not in job.blacklist:
					return job, job.getFrames()
		
		return None, None

def runMaster(address, broadcast, path, update_stats, test_break):
		httpd = RenderMasterServer(address, RenderHandler, path)
		httpd.timeout = 1
		httpd.stats = update_stats
		
		if broadcast:
			s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

			start_time = time.time()
			
		while not test_break():
			httpd.handle_request()
			
			if time.time() - start_time >= 10: # need constant here
				httpd.timeoutSlaves()
				
				httpd.updateUsage()
				
				if broadcast:
						print("broadcasting address")
						s.sendto(bytes("%i" % address[1], encoding='utf8'), 0, ('<broadcast>', 8000))
						start_time = time.time()
