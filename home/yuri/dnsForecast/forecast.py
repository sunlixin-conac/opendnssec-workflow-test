#!/usr/bin/python

from random import randint, seed

seed(42) ## duh

from sys import argv, stderr
from collections import namedtuple
from time import time, localtime

N = 0; R = 1; O = 2; U = 3; H = 4
ZSK = 1; KSK = 2; CSK = 3
DS = 0; DK = 1; RD = 2; RS = 3
#~ Record = namedtuple('Record', 'state ttl timestamp minimize type')

def state2str(r):
	return ["N","R","O","U","H"][r]
def record2str(r):
	return ["DS","DK","RD","RS"][r]
def type2str(r):
	return ["#ERROR#", "ZSK","KSK","CSK"][r]
	
class Record:
	def __init__(self, state, ttl, timestamp, minimize, type):
		self.state = state
		self.ttl = ttl
		self.timestamp = timestamp
		self.minimize = minimize
		self.type = type
	def __str__(self):
		return state2str(self.state)

class Key:
	def __init__(self, alg, size, goal, inception, records):
		self.alg = alg
		self.size = size
		self.goal = goal
		self.records = records
		self.inception = inception
		self.id = randint(0, 999)

	def ds(self): return self.records[DS]
	def dk(self): return self.records[DK]
	def rd(self): return self.records[RD]
	def rs(self): return self.records[RS]

	def keyrole(self):
		return ((self.records[0].state != N)<<1) + (self.records[3].state != N)
	
	# x < 0: never. x >= 0: purge at time x
	def purge_time_record(self, record, policy):
		if record.state == N: return 0 # purge now
		if record.state != H: return -1 # not purgable
		return record.timestamp + policy.key_purgedelay
	
	def purge_time(self, policy):
		if self.goal != H: return -1
		times = []
		for record in self.records:
			t = self.purge_time_record(record, policy)
			if t == -1: return -1
			times.append( t )
		return max(times)

	def __repr__(self):
		return "[KEY id:%03d role:%s goal:%s alg:%s size:%d inception:%07d state:%s]" % \
			(self.id, type2str(self.keyrole()), state2str(self.goal), str(self.alg), self.size, self.inception, "".join(map(str, self.records)))

class KeyConfig:
	def __init__(self, algorithm, size, keyrole, lifetime, rolltype):
		self.algorithm = algorithm
		self.size = size
		self.keyrole = keyrole
		self.lifetime = lifetime
		self.rolltype = rolltype

class Policy:
	def __init__(self):
		self.zone_propdelay = 1800
		self.zone_ttl = 900
		self.parent_propdelay = 1800
		self.parent_regdelay = 1800
		self.parent_ttl = 3600
		self.sig_validity = 3600*24
		self.sig_jitter = 3600
		self.key_ttl = 7200
		self.key_retsafety = 1800
		self.key_pubsafety = 1800
		self.key_purgedelay = 1800
		self.keys = []

	def allow_unsigned(self):
		return not self.keys

	def match(self, keyconf, key):
		if keyconf.size != key.size: return False
		if keyconf.algorithm != key.alg: return False
		if keyconf.keyrole != key.keyrole(): return False
		return True

	def match_none(self, key):
		for k in self.keys:
			if self.match(k, key): return False
		return True

	def unmatching(self, keys):
		return filter(self.match_none, keys)

	def config2keys(self, config, keys):
		return filter(lambda k: self.match(config, k), keys)

class Zone:
	def __init__(self):
		self.enddate_ttl_ds = 0
		self.enddate_ttl_dk = 0
		self.enddate_ttl_rs = 0
	
	def update_ttls(self, now, policy):
		if self.enddate_ttl_ds <= now:
			self.enddate_ttl_ds = now + policy.parent_ttl
		if self.enddate_ttl_dk <= now:
			self.enddate_ttl_dk = now + policy.key_ttl
		if self.enddate_ttl_dk <= now:
			self.enddate_ttl_dk = now + policy.zone_ttl

def desired_state(state, goal):
	return ((N, O, O, R, R), (N, U, U, H, H))[goal==H][state]

def minTransitionTime(policy, record, next_state):
	if next_state == R or next_state == U:
		return record.timestamp
	
	if record.type == DS:
		return record.timestamp + record.ttl + \
				policy.parent_propdelay + policy.parent_regdelay
	elif record.type == RS:
		return record.timestamp + record.ttl + policy.zone_propdelay
	else: #DK or RD
		add_delay = [policy.key_pubsafety, policy.key_retsafety][next_state != O]
		return record.timestamp + record.ttl + \
				policy.zone_propdelay + add_delay

def exists(keys, key, record, next_state, require_same_algorithm, pretend_update, mask):
	for k in keys:
		if require_same_algorithm and k.alg != key.alg:
			continue
		sub_key = pretend_update and (key == k)
		match = True
		for r in [DS, DK, RD, RS]:
			if mask[r] == N: continue
			sub_rec = sub_key and (r == record.type)
			state = [k.records[r].state, next_state][sub_rec]
			if mask[r] != state:
				match = False
				break
		if match: 
			return True
	return False

def unsignedOk(keys, key, record, next_state, pretend_update, mask, mustHID):
	for k in keys:
		if k.alg != key.alg: continue
		substitute = pretend_update and key == k
		cmp_msk = []
		for r in [DS, DK, RD, RS]:
			if r != mustHID:
				cmp_msk.append(mask[r])
			elif substitute and record.type == r:
				cmp_msk.append(next_state)
			else:
				cmp_msk.append(k.records[r].state)
		if cmp_msk[mustHID] == H or cmp_msk[mustHID] == N: continue
		if not exists(keys, key, record, next_state, True, pretend_update, cmp_msk):
			return False
	return True

def rule1(keys, key, record, next_state, pretend_update):
	mask_triv = [O,N,N,N]
	mask_dsin = [R,N,N,N]
	
	return exists(keys, key, record, next_state, False, pretend_update, mask_triv) or \
		   exists(keys, key, record, next_state, False, pretend_update, mask_dsin)

def rule2(keys, key, record, next_state, pretend_update):
	mask_unsg =  [H, O, O, N]
	mask_triv =  [O, O, O, N]
	mask_ds_i =  [R, O, O, N]
	mask_ds_o =  [U, O, O, N]
	mask_k_i1 =  [O, R, R, N]
	mask_k_i2 =  [O, O, R, N]
	mask_k_o1 =  [O, U, U, N]
	mask_k_o2 =  [O, U, O, N]

	return \
		exists(keys, key, record, next_state, True, pretend_update, mask_triv) or \
		\
		exists(keys, key, record, next_state, True, pretend_update, mask_ds_i) and \
		exists(keys, key, record, next_state, True, pretend_update, mask_ds_o) or \
		\
		(exists(keys, key, record, next_state, True, pretend_update, mask_k_i1) or \
		 exists(keys, key, record, next_state, True, pretend_update, mask_k_i2) )and \
		(exists(keys, key, record, next_state, True, pretend_update, mask_k_o1) or \
		 exists(keys, key, record, next_state, True, pretend_update, mask_k_o2) ) or \
		\
		unsignedOk(keys, key, record, next_state, pretend_update, mask_unsg, DS)

def rule3(keys, key, record, next_state, pretend_update):

	mask_triv = [N, O, N, O]
	mask_keyi = [N, R, N, O]
	mask_keyo = [N, U, N, O]
	mask_sigi = [N, O, N, R]
	mask_sigo = [N, O, N, U]
	mask_unsg = [N, H, N, O]

	return \
		exists(keys, key, record, next_state, True, pretend_update, mask_triv) or \
		\
		exists(keys, key, record, next_state, True, pretend_update, mask_keyi) and \
		exists(keys, key, record, next_state, True, pretend_update, mask_keyo) or \
		\
		exists(keys, key, record, next_state, True, pretend_update, mask_sigi) and \
		exists(keys, key, record, next_state, True, pretend_update, mask_sigo) or \
		\
		unsignedOk(keys, key, record, next_state, pretend_update, mask_unsg, DK)

def dnssecApproval(keys, key, record, next_state, allow_unsigned):
	return \
		(allow_unsigned or \
		 not rule1(keys, key, record, next_state, False) or \
		  rule1(keys, key, record, next_state, True ) ) and \
		(not rule2(keys, key, record, next_state, False) or \
		  rule2(keys, key, record, next_state, True ) ) and \
		(not rule3(keys, key, record, next_state, False) or \
		  rule3(keys, key, record, next_state, True ) )

def policyApproval(keys, key, record, next_state):
	if next_state != R: return True
	
	mask_sig = [N,O,N,O]
	mask_dnskey = [O,O,O,N]


	if record.type == DS:
		return not record.minimize or key.dk().state == O
	elif record.type == DK:
		if not record.minimize: return True
		if key.rs().state != O and key.rs().state != N: return False
		if key.ds().state == O: return True
		return not exists(keys, key, record, N, True, False, mask_dnskey)
	elif record.type == RD:
		return key.dk().state != H
	elif record.type == RS:
		if not record.minimize: return True;
		if key.dk().state == O: return True;
		return not exists(keys, key, record, N, True, False, mask_sig);
	else:
		crash()

def print_keys(keys):
	for key in keys:
		#~ print >>stderr, " * %s"%(str(key))
		print key.id, key.alg, type2str(key.keyrole()), state2str(key.goal), "".join(map(str, key.records))

def print_next(times):
	return
	#~ print " @ Time %s \"%s\""%min(times)
	
def log(msg):
	return
	#~ print >> stderr, " # %s"%str(msg)

keys = []
#~ keys.append( Key(1, 1024, O, -90, [
	#~ Record(O, 10, -10, False, DS), 
	#~ Record(O, 10, -10, False, DK), 
	#~ Record(O, 10, -10, False, RD), 
	#~ Record(N, 10, -10, False, RS)]) )
#~ keys.append( Key(1, 1024, O, -90, [
	#~ Record(N, 10, -10, False, DS), 
	#~ Record(O, 10, -10, False, DK), 
	#~ Record(N, 10, -10, False, RD), 
	#~ Record(O, 10, -10, False, RS)]) )
keys.append( Key(2, 1024, O, -90, [
	Record(O, 10, -10, False, DS), 
	Record(O, 10, -10, False, DK), 
	Record(O, 10, -10, False, RD), 
	Record(O, 10, -10, False, RS)]) )




## begin parse 
policy = Policy()
policy.keys.append( KeyConfig( 1, 1024, ZSK, 3600*24*7, None) )
policy.keys.append( KeyConfig( 1, 1024, KSK, 3600*24*20, None) )
#~ policy.keys.append( KeyConfig( 1, 1024, CSK, 200, None) )
## end parse

zone = Zone()
now = 0
times = [(now, "First Evaluation")]

print "Time:0\t-"
print_keys(keys)
print_next(times)
maxiters = 30

while times and maxiters:
	maxiters -= 1
	now, reason = min(times)
	times = []
	print "\nTime:%d\t%s"%(now, reason)
	
	# throw away keys without configuration
	for key in policy.unmatching(keys):
		if key.goal != H:
			log( "[KEY id:%d] no config found. Treat as expired."%key.id )
			key.goal = H
	
	allow_unsigned = not len(policy.keys)
	# loop over key configs, insert new keys if old are stale
	for config in policy.keys:
		keysforconfig = policy.config2keys(config, keys)
		if keysforconfig:
			# what is the youngest key? is it expired yet?
			mr_key = max(keysforconfig, key=lambda x: x.inception)
			expiredate = mr_key.inception + config.lifetime
			if expiredate > now:
				times.append((expiredate, "[KEY id:%d] will reach lifetime"%mr_key.id))
				continue
			log( "[KEY id:%d] expired."%mr_key.id )

		 # if we got to here, we need a new key for this configuration!
		k = Key(config.algorithm, config.size, O, now, [
			Record([N, H][bool(config.keyrole&KSK)], policy.parent_ttl, now, False, DS), 
			Record(H, policy.key_ttl, now, False, DK), 
			Record([N, H][bool(config.keyrole&KSK)], policy.key_ttl, now, False, RD), 
			Record([N, H][bool(config.keyrole&ZSK)], policy.zone_ttl, now, False, RS)])
		keys.append( k )
		times.append((k.inception + config.lifetime, "[KEY id:%d] will expire"%k.id))
		log( "[KEY id:%d] created"%k.id )
		## mark all other keys for this config as old
		for key in keysforconfig:
			if key.goal != H:
				log( "[KEY id:%d] marked for removal"%key.id )
				key.goal = H
		
	zone.update_ttls(now, policy)
	# now loop over the keys and fiddle with state
	changes = True
	while changes:
		changes = False
		for key in keys:
			log(key)
			#~ a = 0
			for record in key.records:
				#~ print key, a
				#~ a += 1
				des_state = desired_state(record.state, key.goal)
				if des_state == record.state: continue
				#~ log( "going for next state for "+str(des_state) )
				#skip ds_seen et al.
				if not policyApproval(keys, key, record, des_state): continue
				#~ log( "policy ok" )
				if not dnssecApproval(keys, key, record, des_state, allow_unsigned): continue
				#~ log( "dnssec ok" )
				
				returntime_key = minTransitionTime(policy, record, des_state);
				## TODO: smoothstuff
				if returntime_key > now: #not yet!
					times.append((returntime_key, "[KEY id:%d record:%s] allowed to transition to \"%s\""%(key.id, record2str(record.type), state2str(des_state))))
					continue
				
				changes = True
				record.state = des_state
				record.timestamp = now
			
	# Remove old keys
	for key in keys[:]:
		t = key.purge_time(policy)
		if t == -1:
			continue
		elif t <= now:
			keys.remove( key )
			log( "removing [KEY id:%d] "%key.id )
		else:
			times.append( (t, "[KEY id:%d] will be purgable"%key.id) )
	
	#~ print times

	print_keys(keys)
	print_next(times)
