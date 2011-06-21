from sys import stderr

NAME  = ["DS", "DNSKEY", "RRSIGDNSKEY", "RRSIG"]
STATE = ["HID", "RUM", "OMN", "UNR", "---"]
HID = 0; RUM = 1; OMN = 2; UNR = 3; NOCARE = -1
DS = 0;  DK = 1;  RD = 2;  RS = 3

#~ ALLOW_UNSIGNED = True
ALLOW_UNSIGNED = False

class Key:
	next_id = 0

	def __init__(self, state = [HID,HID,HID,HID], intro = True, alg = 0):
		self.state = state
		self.lastchange = [0,0,0,0]
		self.intro = intro
		self.id = Key.next_id
		Key.next_id += 1
		self.alg = alg
		self.minds = False
		self.minkey = False
		self.minsig = True
		#~ self.minsig = False

	def isOmn(self, i): return self.state[i] == OMN
	def isHid(self, i): return self.state[i] == HID
	def isRum(self, i): return self.state[i] == RUM or self.isOmn(i)
	def isUnr(self, i): return self.state[i] == UNR or self.isHid(i)

	def isBetterOrEq(self, i, state):
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
			if not key.isBetterOrEq(i, target):
				match = False
				break
		if match: return True
	return False

def eval_rule0(keylist, key):
	return exist_key([RUM, NOCARE, NOCARE, NOCARE], keylist, -1)

def eval_rule1(keylist, key):
	return not exist_key([RUM, NOCARE, NOCARE, NOCARE], keylist, key.alg) and \
		(key.isOmn(DK) and key.isOmn(RD) or key.isHid(DS)) or \
		exist_key([RUM, OMN, OMN, NOCARE], keylist, key.alg) or \
		exist_key([OMN, RUM, RUM, NOCARE], keylist, key.alg) and \
		exist_key([OMN, UNR, UNR, NOCARE], keylist, key.alg)

def eval_rule2(keylist, key):
	return not exist_key([NOCARE, RUM, NOCARE, NOCARE], keylist, key.alg) and \
		(key.isOmn(RS) or key.isHid(DK)) or \
		exist_key([NOCARE, RUM, NOCARE, OMN], keylist, key.alg) or \
		exist_key([NOCARE, OMN, NOCARE, RUM], keylist, key.alg) and \
		exist_key([NOCARE, OMN, NOCARE, UNR], keylist, key.alg)

def eval_rule3(keylist, key):
	return key.state[1] >= key.state[2]

#overwrites keylist[ki].state[ri] = st
def evaluate(keylist, ki, ri, st):
	key = keylist[ki]
	oldstate = key.state[ri]
	# see if rule is true, substitute, see if rule is still true
	
	rule0_pre = eval_rule0(keylist, key)
	rule1_pre = eval_rule1(keylist, key)
	rule2_pre = eval_rule2(keylist, key)
	rule3_pre = eval_rule3(keylist, key)
	print >> stderr, NAME[ri], rule0_pre, rule1_pre, rule2_pre, rule3_pre
	#~ print >> stderr, NAME[ri], rule0_pre, rule1_pre, rule2_pre
	
	key.state[ri] = st
	
	rule0 = not rule0_pre or eval_rule0(keylist, key) or ALLOW_UNSIGNED
	rule1 = not rule1_pre or eval_rule1(keylist, key)
	rule2 = not rule2_pre or eval_rule2(keylist, key)
	rule3 = not rule3_pre or eval_rule3(keylist, key)
	print >> stderr, NAME[ri], rule0, rule1, rule2, rule3
	#~ print >> stderr, NAME[ri], rule0, rule1, rule2
	
	key.state[ri] = oldstate
	
	return rule0 and rule1 and rule2 and rule3
	#~ return rule0 and rule1 and rule2

# next desired state given current and goal
def nextState(intro, cur_state):
	return [[HID,UNR,UNR,HID], [RUM,OMN,OMN,RUM]][intro][cur_state]

def policy(key, ri, nextstate):
	if nextstate != RUM: return True
	if ri == DS:
		if key.minds and key.state[DK] != OMN: return False
	elif ri == DK:
		if key.minkey and (key.state[DS] != OMN or key.state[RS] != OMN): return False
	elif ri == RS:
		if key.minsig and key.state[DK] != OMN: return False
	return True

def time(key, ri, now):
	TTL = [11, 2, 2, 5]
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
	#~ if recordindex == RS and key.state[recordindex] == HID and \
			#~ key.state[DK] != OMN: # FAKE ATOMIC SIG
		#~ return False
	
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
	return " |".join(map(lambda key: key.prettyheader(), keylist)) + \
			" | T\n" + "-"*58
	
def prettystate(keylist, now):
	return " |".join(map(lambda key: key.prettystate(), keylist)) + \
			" | " + str(now)

def simulate():
	keylist = []
	
	## split to single algorithm roll
	keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
	keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
	keylist.append(Key([HID, HID, HID, HID], True, 1)) #CSK, hidden, introducing

	#~ ## unsigned to signed split
	#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK
	#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK

	#~ ## unsigned to signed csk
	#~ keylist.append(Key([HID, HID, HID, HID], True, 0)) #CSK

	#~ ## zsk roll no KSK
	#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
	#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing
 
	#~ ## zsk roll
	#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
	#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
	#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing

	#~ ## Ksk roll
	#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
	#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, omnipresent, outroducing
	#~ keylist.append(Key([NOCARE, OMN, NOCARE, OMN], True, 0)) #ZSK, omnipresent, outroducing

	#~ ## ksk roll with broken zsk
	#~ keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
	#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 0)) #KSK, omnipresent, outroducing
	#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK, omnipresent, outroducing

	#~ ## zsk into to csk
	#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK
	#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK


	print keystostr(keylist)
	history = []
	history.append( prettyheader(keylist) )
	history.append( prettystate(keylist, None) )

	epoch = 42 # 0 has a special meaning

	now = epoch;
	while True:
		t = updatezone(keylist, now)
		print "\n", keystostr(keylist)
		history.append( prettystate(keylist, now) )
		if t == 0:
			print "nothing more to do. stop."
			break
		if t < now: 
			print "event for past! stop!"
			break
		print "TIME: advancing from %d to %d"%(now, t)
		now = t
		
	print "\n" + "\n".join(history)
	
if __name__ == "__main__":
	simulate()
