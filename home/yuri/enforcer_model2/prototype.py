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
		self.intro = intro
		self.id = Key.next_id
		Key.next_id += 1
		self.alg = alg

	def isOmn(self, i): return self.state[i] == OMN
	def isHid(self, i): return self.state[i] == HID
	def isRum(self, i): return self.state[i] == RUM or self.isOmn(i)
	def isUnr(self, i): return self.state[i] == UNR or self.isHid(i)

	def isBetterOrEq(self, i, state):
		return [self.isHid, self.isRum, self.isOmn, self.isUnr][state](i)

	def __repr__(self):
		s = ",".join(map(lambda x: STATE[x], self.state))
		return "Key %d in=%d A,B,C,D = %s"%(self.id, self.intro, s)

def exist_key(states, keylist, alg):
	for key in keylist:
		if not (alg == -1 or alg == key.alg): continue
		match = True
		for i, target, keystate in zip(range(4), states, key.state):
			if target == NOCARE: continue #geen eis
			if not key.isBetterOrEq(i, target):
				match = False
				break
		if match:
			return True
	return False

def eval_rule0(keylist, key):
	return exist_key([RUM, NOCARE, NOCARE, NOCARE], keylist, -1)

def eval_rule1(keylist, key):
	return not exist_key([RUM, NOCARE, NOCARE, NOCARE], keylist, key.alg) and (key.isOmn(DK) and key.isOmn(RD) or key.isHid(DS)) or \
			exist_key([RUM, OMN, OMN, NOCARE], keylist, key.alg) or \
			exist_key([OMN, RUM, RUM, NOCARE], keylist, key.alg) and \
			exist_key([OMN, UNR, UNR, NOCARE], keylist, key.alg)

def eval_rule2(keylist, key):
	return  not exist_key([NOCARE, RUM, NOCARE, NOCARE], keylist, key.alg) and (key.isOmn(RS) or key.isHid(DK)) or \
			exist_key([NOCARE, RUM, NOCARE, OMN], keylist, key.alg) or \
			exist_key([NOCARE, OMN, NOCARE, RUM], keylist, key.alg) and \
			exist_key([NOCARE, OMN, NOCARE, UNR], keylist, key.alg)

#overwrites keylist[ki].state[ri] = st
def evaluate(keylist, ki, ri, st):
	key = keylist[ki]
	oldstate = key.state[ri]
	# see if rule is true, substitute, see if rule is still true
	
	rule0_pre = eval_rule0(keylist, key)
	rule1_pre = eval_rule1(keylist, key)
	rule2_pre = eval_rule2(keylist, key)
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
	return [[HID, UNR, UNR, HID], [RUM, OMN, OMN, RUM]][intro][cur_state]

def updaterecord(keylist, keyindex, recordindex):
	key = keylist[keyindex]
	name = NAME[recordindex]
	# do I want to move?
	next_state = nextState(key.intro, key.state[recordindex])
	if next_state == key.state[recordindex]:
		print >> stderr, "%11s\tskip: stable state"%name
		return False
	DNSSEC_OK = evaluate(keylist, keyindex, recordindex, next_state)
	if not DNSSEC_OK:
		print >> stderr, "%11s\tskip: not all preconditions are met"%name
		return False
	print >> stderr, "%11s\tNext state is DNSSEC OK"%name
	#do something with time
	#~ if recordindex == DK:
		#~ print >> stderr, "%11s\tnot enough time passed"%name
		#~ return False
	#~ else:
	print >> stderr, "%11s\tenough time passed"%name
	key.state[recordindex] = next_state
	print >> stderr, "%11s\tset state to %s"%(name, STATE[next_state])
	return True

def updatekey(keylist, keyindex):
	change = False
	for recordindex, recordstate in enumerate(keylist[keyindex].state):
		if recordstate == NOCARE: continue
		change |= updaterecord(keylist, keyindex, recordindex)
	return change

def updatezone(keylist):
	change = True
	while change:
		change = False
		for keyindex, key in enumerate(keylist):
			print >> stderr
			print " "*50*key.alg, key
			change |= updatekey(keylist, keyindex)

keylist = []
keylist.append(Key([OMN, OMN, OMN, NOCARE], False, 0)) #KSK, omnipresent, outroducing
keylist.append(Key([NOCARE, OMN, NOCARE, OMN], False, 0)) #ZSK, omnipresent, outroducing
#~ 
#~ keylist.append(Key([HID, HID, HID, NOCARE], True, 1)) #KSK
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK

#~ keylist.append(Key([OMN, OMN, OMN, OMN], False, 0)) #CSK
#~ keylist.append(Key([NOCARE, HID, NOCARE, HID], True, 0)) #ZSK

keylist.append(Key([HID, HID, HID, HID], True, 0)) #CSK, hidden, introducing

updatezone(keylist)
