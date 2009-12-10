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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import re

from netrender.utils import *


def get(handler):
	def output(text):
		handler.wfile.write(bytes(text, encoding='utf8'))
	
	def link(text, url):
		return "<a href='%s'>%s</a>" % (url, text)
	
	def startTable(border=1):
		output("<table border='%i'>" % border)
	
	def headerTable(*headers):
		output("<thead><tr>")
		
		for c in headers:
			output("<td>" + c + "</td>")
		
		output("</tr></thead>")
	
	def rowTable(*data):
		output("<tr>")
		
		for c in data:
			output("<td>" + str(c) + "</td>")
		
		output("</tr>")
	
	def endTable():
		output("</table>")
	
	if handler.path == "/html" or handler.path == "/":
		handler.send_head(content = "text/html")
		output("<html><head><title>NetRender</title></head><body>")
	
		output("<h2>Master</h2>")
	
		output("<h2>Slaves</h2>")
		
		startTable()
		headerTable("name", "address", "last seen", "stats", "job")
		
		for slave in handler.server.slaves:
			rowTable(slave.name, slave.address[0], time.ctime(slave.last_seen), slave.stats, link(slave.job.name, "/html/job" + slave.job.id) if slave.job else "None")
		
		endTable()
		
		output("<h2>Jobs</h2>")
		
		startTable()
		headerTable(	
									"name",
									"category",
									"priority",
									"usage",
									"wait",
									"length",
									"done",
									"dispatched",
									"error",
									"first",
									"exception"
								)

		handler.server.balance()
		
		for job in handler.server.jobs:
			results = job.framesStatus()
			rowTable(	
								link(job.name, "/html/job" + job.id),
								job.category,
								job.priority,
								"%0.1f%%" % (job.usage * 100),
								"%is" % int(time.time() - job.last_dispatched),
								len(job),
								results[DONE],
								results[DISPATCHED],
								results[ERROR],
								handler.server.balancer.applyPriorities(job), handler.server.balancer.applyExceptions(job)
							)
		
		endTable()
		
		output("</body></html>")
	
	elif handler.path.startswith("/html/job"):
		handler.send_head(content = "text/html")
		job_id = handler.path[9:]
		
		output("<html><head><title>NetRender</title></head><body>")
	
		job = handler.server.getJobID(job_id)
		
		if job:
			output("<h2>Frames</h2>")
		
			startTable()
			headerTable("no", "status", "render time", "slave", "log")
			
			for frame in job.frames:
				rowTable(frame.number, frame.statusText(), "%.1fs" % frame.time, frame.slave.name if frame.slave else "&nbsp;", link("view log", "/html/log%s_%i" % (job_id, frame.number)) if frame.log_path else "&nbsp;")
			
			endTable()
		else:
			output("no such job")
		
		output("</body></html>")
	
	elif handler.path.startswith("/html/log"):
		handler.send_head(content = "text/plain")
		pattern = re.compile("([a-zA-Z0-9]+)_([0-9]+)")
		
		match = pattern.match(handler.path[9:])
		if match:
			job_id = match.groups()[0]
			frame_number = int(match.groups()[1])
			
			job = handler.server.getJobID(job_id)
			
			if job:
				frame = job[frame_number]
				
				if frame:
						f = open(frame.log_path, 'rb')
						
						shutil.copyfileobj(f, handler.wfile)
						
						f.close()
				else:
					output("no such frame")
			else:
				output("no such job")
		else:
			output("malformed url")
