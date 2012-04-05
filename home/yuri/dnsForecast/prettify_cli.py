#!/usr/bin/python

from sys import stdin
from collections import namedtuple
from string import strip
from time import time, localtime, strftime, sleep

Key = namedtuple("Key", "id goal ds dk rd rs")

TIMEDISP = 1 #0: notime, 1:reltime, 2:prettytime, 3:unixtime

class Key:
	state2str = {"N":" ", "H":" ", "R":"+", "O":"|", "U":"-"}
	colors = ["\033[93m", "\033[94m", "\033[95m", "\033[96m", "\033[97m"]
	colind = 0
	colmap = {}
	
	@staticmethod
	def alg2color(alg):
		if alg not in Key.colmap:
			Key.colmap[alg] = Key.colors[Key.colind]
			Key.colind = (Key.colind+1)%len(Key.colors)
		c = Key.colmap[alg]
		return c
	
	def __init__(self, id, alg, role, goal, ds, dk, rd, rs):
		self.id = id; self.alg = alg; 
		self.role = role; self.goal = goal
		self.ds = ds; self.dk = dk;	self.rd = rd; self.rs = rs
	
	def __repr__(self):
		return str(self)
		
	def get_str_id(self):
		s = Key.alg2color(self.alg) + "% 4s\033[39m"% str(self.id)
		#~ s = "\033[42m%s\033[40m"%s
		return s
	
	def __str__(self):
		states = Key.state2str[self.ds]+\
				Key.state2str[self.dk]+\
				Key.state2str[self.rd]+\
					Key.state2str[self.rs]
		if self.goal == "O":
			return "\033[92m%s\033[39m"%states
		else:
			return "\033[91m%s\033[39m"%states

def offset2timestr(offset, now):
	global TIMEDISP
	if TIMEDISP == 0:
		return ""
	elif  TIMEDISP == 1:
		return "% 8s"%str(offset)
	elif  TIMEDISP == 2:
		return strftime("%Y%m%d %H:%M:%S", localtime(now+offset))
	elif  TIMEDISP == 3:
		return str(int(now+offset))

timeline = []
#~ print "\033[40m"
## first read all input
keycount = 0
maxkeycount = 0
keys = {}
curtime = None
lines = map(strip, stdin.readlines())
for line in lines+[""]:
	if not len(line):
		timeline.append((keys, curtime, reason))
		keys = {}
		if keycount > maxkeycount: maxkeycount = keycount
		keycount = 0
	elif line.startswith("Time"):
		t, reason = line.split('\t')
		curtime = int(t.split(':')[1])
	else:
		i,a, r, g,(ds,dk,rd,rs) = line.split()
		keys[i] = Key(i,a, r, g, ds,dk,rd,rs)
		keycount += 1

now = time()
cur = [None]*maxkeycount
ids = []
## create output
ptime = 0
#~ print (" Time |\t%% %ds |\tReason"%(5*maxkeycount-2))%"Keys"
for keys, curtime, reason in timeline:
	#~ sleep(.05)
	print "\033[97m%s\033[90m"%offset2timestr(ptime, now),
	ptime = curtime
	old = []
	new = []
	for keyid in keys.iterkeys():
		if keyid in ids:
			old.append(keyid)
		else:
			new.append(keyid)
	ids = old+new
	for i in range(len(cur)):
		if cur[i] == None: 
			if new:
				cur[i] = keys[new.pop()]
				print cur[i].get_str_id(),
			else:
				print "    ",
		elif cur[i].id in old: #update
			print str(cur[i]),
			cur[i] = keys[cur[i].id]
		else: #dropped
			print str(cur[i]),
			cur[i] = None
	print "\033[39m%s"%reason
#finally print last state
print "\033[97m%s\033[90m"%offset2timestr(ptime, now),
for i in range(len(cur)):
	if cur[i] == None: 
		print "    ",
	else: #dropped
		print str(cur[i]),

#~ print "\033[0m"
