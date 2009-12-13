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

import time

from netrender.utils import *
import netrender.model

class RatingRule:
	def rate(self, job):
		return 0

class ExclusionRule:
	def test(self, job):
		return False

class PriorityRule:
	def test(self, job):
		return False

class Balancer:
	def __init__(self):
		self.rules = []
		self.priorities = []
		self.exceptions = []
	
	def addRule(self, rule):
		self.rules.append(rule)
	
	def addPriority(self, priority):
		self.priorities.append(priority)
		
	def addException(self, exception):
		self.exceptions.append(exception)
	
	def applyRules(self, job):
		return sum((rule.rate(job) for rule in self.rules))
	
	def applyPriorities(self, job):
		for priority in self.priorities:
			if priority.test(job):
				return True # priorities are first
		
		return False
	
	def applyExceptions(self, job):
		for exception in self.exceptions:
			if exception.test(job):
				return True # exceptions are last
			
		return False
	
	def sortKey(self, job):
		return (1 if self.applyExceptions(job) else 0, # exceptions after
						0 if self.applyPriorities(job) else 1, # priorities first
						self.applyRules(job))
	
	def balance(self, jobs):
		if jobs:
			# use inline copy to make sure the list is still accessible while sorting
			jobs[:] = sorted(jobs, key=self.sortKey)
			return jobs[0]
		else:
			return None
	
# ==========================

class RatingUsage(RatingRule):
	def rate(self, job):
		# less usage is better
		return job.usage / job.priority

class RatingUsageByCategory(RatingRule):
	def __init__(self, get_jobs):
		self.getJobs = get_jobs
	def rate(self, job):
		total_category_usage = sum([j.usage for j in self.getJobs() if j.category == job.category])
		maximum_priority = max([j.priority for j in self.getJobs() if j.category == job.category])
		
		# less usage is better
		return total_category_usage / maximum_priority
	
class NewJobPriority(PriorityRule):
	def __init__(self, limit = 1):
		self.limit = limit
		
	def test(self, job):
		return job.countFrames(status = DONE) < self.limit

class MinimumTimeBetweenDispatchPriority(PriorityRule):
	def __init__(self, limit = 10):
		self.limit = limit
		
	def test(self, job):
		return job.countFrames(status = DISPATCHED) == 0 and (time.time() - job.last_dispatched) / 60 > self.limit

class ExcludeQueuedEmptyJob(ExclusionRule):
	def test(self, job):
		return job.status != JOB_QUEUED or job.countFrames(status = QUEUED) == 0
	
class ExcludeSlavesLimit(ExclusionRule):
	def __init__(self, count_jobs, count_slaves, limit = 0.75):
		self.count_jobs = count_jobs
		self.count_slaves = count_slaves
		self.limit = limit
		
	def test(self, job):
		return not ( self.count_jobs() == 1 or self.count_slaves() <= 1 or float(job.countSlaves() + 1) / self.count_slaves() <= self.limit )
