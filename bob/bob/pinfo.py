import os

class ProcessInfo(object):
    def __init__(self, info):
        self.info = info

def for_pid(pid):
    try:
        with open('/proc/{}/stat'.format(pid), 'r') as f:
            return ProcessInfo(f.read())
    except IOError:
        return None
