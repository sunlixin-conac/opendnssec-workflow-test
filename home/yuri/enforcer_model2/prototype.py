from sys import stderr

NAME  = ["DS", "DNSKEY", "RRSIGDNSKEY", "RRSIG"]
STATE = ["HID", "RUM", "OMN", "UNR", "---"]
HID = 0; RUM = 1; OMN = 2; UNR = 3; NOCARE = -1
DS = 0;  DK = 1;  RD = 2;  RS = 3

#~ ALLOW_UNSIGNED = True
ALLOW_UNSIGNED = False

class Key:
	next_id = 0

	def __init__(self, state = [HID,HID,HID,HID], intro = True, alg = 0, minds = False, minkey = False, minsig = False):
		self.state = state
		self.lastchange = [0,0,0,0]
		self.intro = intro
		self.id = Key.next_id
		Key.next_id += 1
		self.alg = alg
		self.minds = minds
		self.minkey = minkey
		self.minsig = minsig

	def isOmn(self, i): return self.state[i] == OMN
	def isHid(self, i): return self.state[i] == HID
	def isRum(self, i): return self.state[i] == RUM
	def isUnr(self, i): return self.state[i] == UNR

	def isEq(self, i, state):
		return [self.isHid, self.isRum, self.isOmn, self.isUnr][state](i)

	def __repr__(self):
		s = ",".join(map(lambda x: STATE[x], self.state))
		return "Key %d in=%d A,B,C,D = %s"%(self.id, self.intro, s)
	def prettyheader(self):
		return "%16s"%("key %d (%s)"%(self.id, ["out", "in"][self.intro]))
	def prettystate(self):
		return  "%16s"%(",".join(map(lambda x: STATE[x], self.state)))
		
def exist_key(states, keylist, alg):
	for key in keylist:
		if not (alg == -1 or alg == key.alg): continue
		match = True
		for i, target, keystate in zip(range(4), states, key.state):
			if target == NOCARE: continue #geen eis
			if not key.isEq(i, target):
				match = False
				break
		if match: return True
	return False

def special_ds2(keylist, alg):
	for key in keylist:
		if not (alg == -1 or alg == key.alg): continue
		if not (key.isRum(DS) or key.isOmn(DS) or key.isUnr(DS)): continue
		#exist k?
		e = False
		for k in keylist:
			if not (alg == -1 or alg == k.alg): continue
			if k.state[DS] == key.state[DS] and k.isOmn(DK) and k.isOmn(RD):
				e = True
				break
		if not e: return False
	return True

def special_dk2(keylist, alg):
	for key in keylist:
		if not (alg == -1 or alg == key.alg): continue
		if key.isHid(DK): continue
		#exist k?
		e = False
		for k in keylist:
			if not (alg == -1 or alg == k.alg): continue
			if k.state[DK] == key.state[DK] and k.isOmn(RS):
				e = True
				break
		if not e: return False
	return True
		
def eval_rule0(keylist, key):
	return exist_key([RUM, NOCARE, NOCARE, NOCARE], keylist, -1) or exist_key([OMN, NOCARE, NOCARE, NOCARE], keylist, -1)

def eval_rule1(keylist, key):
		return \
		special_ds2(keylist, key.alg) or \
		\
		exist_key([RUM, OMN, OMN, NOCARE], keylist, key.alg) and \
		exist_key([UNR, OMN, OMN, NOCARE], keylist, key.alg) or \
		\
		exist_key([OMN, OMN, OMN, NOCARE], keylist, key.alg) or \
		\
		(exist_key([OMN, RUM, RUM, NOCARE], keylist, key.alg) or exist_key([OMN, OMN, RUM, NOCARE], keylist, key.alg) ) and \
		(exist_key([OMN, UNR, UNR, NOCARE], keylist, key.alg) or exist_key([OMN, UNR, OMN, NOCARE], keylist, key.alg) )

def eval_rule2(keylist, key):
		return \
		special_dk2(keylist, key.alg) or \
		\
		exist_key([NOCARE, RUM, NOCARE, OMN], keylist, key.alg) and \
		exist_key([NOCARE, UNR, NOCARE, OMN], keylist, key.alg) or \
		\
		exist_key([NOCARE, OMN, NOCARE, OMN], keylist, key.alg) or \
		\
		exist_key([NOCARE, OMN, NOCARE, RUM], keylist, key.alg) and \
		exist_key([NOCARE, OMN, NOCARE, UNR], keylist, key.alg)

#overwrites keylist[ki].state[ri] = st
def evaluate(keylist, ki, ri, st):
	print >> stderr, "want to move %s to %s"%(NAME[ri], STATE[st])
	key = keylist[ki]
	oldstate = key.state[ri]
	# see if rule is true, substitute, see if rule is still true
	
	rule0_pre = eval_rule0(keylist, key)
	rule1_pre = eval_rule1(keylist, key)
	rule2_pre = eval_rule2(keylist, key)
	#~ if not ((rule0_pre or ALLOW_UNSIGNED) and rule1_pre and rule2_pre):
		#~ print "ERRRRRRRRRRRRRRRR", NAME[ri], STATE[st]
	print >> stderr, NAME[ri], rule0_pre, rule1_pre, rule2_pre
	
	key.state[ri] = st
	
	rule0 = not rule0_pre or eval_rule0(keylist, key) or ALLOW_UNSIGNED
	rule1 = not rule1_pre or eval_rule1(keylist, key)
	rule2 = not rule2_pre or eval_rule2(keylist, key)
	print >> stderr, NAME[ri], rule0, rule1, rule2
	
	key.state[ri] = oldstate
	
	return rule0 and rule1 and rule2

# next desired state given current and goal
def nextState(intro, cur_state):
	return [[HID,UNR,UNR,HID], [RUM,OMN,OMN,RUM]][intro][cur_state]

def policy(key, ri, nextstate):
	if nextstate != RUM: return True
	if ri == DS:
		if key.minds and key.state[DK] != OMN: return False
	elif ri == DK:
		if key.minkey and ((key.state[DS] != OMN and key.state[DS] != NOCARE) or (key.state[RS] != OMN and key.state[RS] != NOCARE)): return False
	elif ri == RD:
		if key.state[DK] == HID: return False
	elif ri == RS:
		if key.minsig and key.state[DK] != OMN: return False
	return True

def time(key, ri, now):
	TTL = [11, 1, 1, 3]
	last = key.lastchange[ri]
	t = last + TTL[ri]
	t_ok = (t <= now) or last == 0;
	return t_ok, t
	

def updaterecord(keylist, keyindex, recordindex, now):
	key = keylist[keyindex]
	name = NAME[recordindex]
	# do I want to move?
	next_state = nextState(key.intro, key.state[recordindex])
	if next_state == key.state[recordindex]:
		print >> stderr, "%11s\tskip: stable state"%name
		return False, 0
	if not policy(key, recordindex, next_state):
		print >> stderr, "%11s\tskip: want to do swap"%name
		return False, 0
	
	DNSSEC_OK = evaluate(keylist, keyindex, recordindex, next_state)
	if not DNSSEC_OK:
		print >> stderr, "%11s\tskip: not all preconditions are met"%name
		return False, 0
	print >> stderr, "%11s\tNext state is DNSSEC OK"%name
	#do something with time
	t_ok, t = time(key, recordindex, now)
	if not t_ok:
		print >> stderr, "%11s\tnot enough time passed " \
				"(now %d, scheduled %d)"%(name, now, t)
		return False, t
	print >> stderr, "%11s\tenough time passed"%name
	key.state[recordindex] = next_state
	key.lastchange[recordindex] = now
	print >> stderr, "%11s\tset state to %s"%(name, STATE[next_state])
	return True, 0

def updatekey(keylist, keyindex, now):
	change = False
	t_min = 0;
	for recordindex, recordstate in enumerate(keylist[keyindex].state):
		if recordstate == NOCARE: continue
		c, t = updaterecord(keylist, keyindex, recordindex, now)
		change |= c
		if t>0 and (t_min == 0 or t < t_min):
			t_min = t
	return change, t_min

def updatezone(keylist, now):
	change = True
	t_min = 0;
	while change:
		change = False
		for keyindex, key in enumerate(keylist):
			c, t = updatekey(keylist, keyindex, now)
			change |= c
			if t>0 and (t_min == 0 or t < t_min):
				t_min = t
			print >> stderr
			print >> stderr, " "*50*key.alg, key
	return t_min

def keystostr(keylist):
	return "\n".join(map(lambda key: str(key), keylist))

def prettyheader(keylist):
	bar = ("-"*17 + "+")*len(keylist) + "-"*6
	return " |".join(map(lambda key: key.prettyheader(), keylist)) + \
			" | T\n" + bar
	
def prettystate(keylist, now):
	return " |".join(map(lambda key: key.prettystate(), keylist)) + \
			" | " + str(now)

def simulate(keylist, allow_unsigned):
	#~ print keystostr(keylist)
	history = []
	history.append( prettyheader(keylist) )
	history.append( prettystate(keylist, None) )

	epoch = 42 # 0 has a special meaning

	global ALLOW_UNSIGNED 
	ALLOW_UNSIGNED = allow_unsigned

	now = epoch;
	while True:
		t = updatezone(keylist, now)
		print >> stderr, "\n", keystostr(keylist)
		history.append( prettystate(keylist, now-epoch) )
		if t == 0:
			#~ print "nothing more to do. stop."
			break
		if t < now: 
			print >> stderr, "event for past! stop!"
			break
		print >> stderr, "TIME: advancing from %d to %d"%(now, t)
		now = t
	return "\n".join(history)

scenarios = []
#~ 
#ZSK
#~ 
#~ keylist = []
#~ title = "zsk roll"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "zsk roll minkey"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0, minkey=True)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "zsk roll minsig"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0, minsig=True)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ #KSK
#~ 
#~ keylist = []
#~ title = "Ksk roll"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "Ksk roll minkey"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0, minkey=True)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "Ksk roll minds"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0, minds=True)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
# split to split
#~ 
#~ keylist = []
#~ title = "split roll"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "split roll diff alg"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 1)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 1)) #ZSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ #CSK
#~ 
#~ keylist = []
#~ title = "csk roll"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, HID], True, 0)) #CSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "csk roll diff alg"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, HID], True, 1)) #CSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ # CSK to split
#~ 
#~ keylist = []
#~ title = "csk roll to split"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "csk roll to split diff alg"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 1)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 1)) #ZSK, hidden, introducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ #split to CSK
#~ 
#~ keylist = []
#~ title = "split roll to csk"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, hidden, introducing
#~ keylist.append(Key([HID, HID, HID, HID], True, 0)) #CSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))

#~ keylist = []
#~ title = "split roll to csk diff alg"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, hidden, introducing
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, hidden, introducing
#~ keylist.append(Key([HID, HID, HID, HID], True, 1)) #CSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))

#--------------------------------

keylist = []
title = "unsigned to signed split"
keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK
keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK
scenarios.append((title, keylist, False))

#~ keylist = []
#~ title = "unsigned to signed csk"
#~ keylist.append(Key([HID, HID, HID, HID], True, 0)) #CSK
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "signed csk to unsigned"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK
#~ scenarios.append((title, keylist, True))







#~ 
#~ keylist = []
#~ title = "zsk roll no KSK"
#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
 #~ 
#~ 
#~ 
#~ 
#~ 
#~ keylist = []
#~ title = "ksk roll with broken zsk"
#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, omnipresent, outroducing
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing
#~ scenarios.append((title, keylist, False))
#~ 
#~ keylist = []
#~ title = "zsk into to csk"
#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK
#~ scenarios.append((title, keylist, False))

for t,k,a in scenarios:
	print t +"\n"
	print simulate(k,a) +"\n"

#~ if __name__ == "__main__":
	#~ simulate()
