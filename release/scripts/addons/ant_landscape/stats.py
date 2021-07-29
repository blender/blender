from time import time

try:
    import psutil
    print('psutil available')
    psutil_available=True
except ImportError:
    psutil_available=False

class Stats:
    def __init__(self):
        self.memstats_available = False
        if psutil_available:
            self.process=psutil.Process()
            self.memstats_available = True
        self.reset()

    def reset(self):
        self.lasttime = self._gettime()
        self.lastmem = self._getmem()
        self.basemem = self.lastmem
        self.maxmem = 0
        self.elapsedtime = 0

    def _gettime(self):
        """return the time in seconds used by the current process."""
        if psutil_available:
            m=self.process.get_cpu_times()
            return m.user + m.system
        return time()

    def _getmem(self):
        """return the resident set size in bytes used by the current process."""
        if psutil_available:
            m = self.process.get_memory_info()
            return m.rss
        return 0

    def time(self):
        """return the time since the last call in seconds used by the current process."""
        old = self.lasttime
        self.lasttime = self._gettime()
        self.elapsedtime = self.lasttime - old
        return self.elapsedtime

    def memory(self):
        """return the maximum resident set size since the first call in bytes used by the current process."""
        self.lastmem = self._getmem()
        d = self.lastmem - self.basemem
        if d > self.maxmem:
            self.maxmem = d
        return self.maxmem
