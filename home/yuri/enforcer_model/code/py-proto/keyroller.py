#!/usr/bin/env python
from random import randint, sample, choice

VERBOSE = 1

HIDDEN        = " "
RUMOURED      = "+"
OMNIPRESENT   = "O"
UNRETENTIVE   = "-"

COMMITTED     = "C"
POSTCOMMITTED = "P"

class Record:
    def __init__(self, state):
        self.state = state
        self.time = 0
    def __repr__(self):
        return str(self.state)

class Key:
    def __init__(self, name, alg, roles, goal, initstate, minds=False, mindnskey=False, minrrsig=False):
        self.name = name
        self.alg = alg
        self.roles = roles
        self.goal = goal
        self.records = {"dnskey": Record(initstate)}
        if "ksk" in roles:
            self.records["ds"] = Record(initstate)
        if "zsk" in roles:
            self.records["rrsig"] = Record(initstate)
        self.minds = minds
        self.mindnskey = mindnskey
        self.minrrsig = minrrsig
    def __repr__(self):
        return self.name

def printkc(kc):
    keys = list(kc)
    keys.sort(lambda k, l: (k.name > l.name)*2-1)
    for k in keys:
        strds     = "_"
        strdnskey = "_"
        strrrsig  = "_"
        if ds(k): strds = ds(k)
        if dnskey(k): strdnskey = dnskey(k)
        if rrsig(k): strrrsig = rrsig(k)
        
        pub = C(dnskey(k)) or R(dnskey(k)) or O(dnskey(k))
        act = C(rrsig(k)) or R(rrsig(k)) or O(rrsig(k))
         
        conf = []
        if pub: conf.append("PUB")
        if act: conf.append("ACT")
        print "key %s: [%s %s %s] goal(k)=%s alg(k)=%s %s %s"%(str(k.name), 
            strds, strdnskey, 
            strrrsig, str(k.goal), str(k.alg), 
            str(",".join(list(roles(k)))), str(",".join(conf)))
    print ""

def debug(k, s):
    if VERBOSE > 1:
        print "-", str(k), str(s)

def exists(some_set, condition):
    for elem in some_set:
        if condition(elem):
            return True
    return False

def forall(some_set, precondition, condition):
    for elem in some_set:
        if precondition(elem) and not condition(elem):
            #~ print elem
            return False
    return True

def impl(a, b):
    return not a or b

def record(k, r):
    if r in k.records:
        return k.records[r]
    else:
        return None
def ds(k):    return record(k, "ds")
def dnskey(k):return record(k, "dnskey")
def rrsig(k): return record(k, "rrsig")
def goal(k):  return k.goal
def alg(k):   return k.alg
def roles(k): return k.roles
def state(r): 
    if not r: return "~"
    else: return r.state
def minds(k):return k.minds
def mindnskey(k):return k.mindnskey
def minrrsig(k):return k.minrrsig    
def H(r):     return state(r) == HIDDEN
def R(r):     return state(r) == RUMOURED
def C(r):     return state(r) == COMMITTED
def O(r):     return state(r) == OMNIPRESENT
def U(r):     return state(r) == UNRETENTIVE
def P(r):     return state(r) == POSTCOMMITTED

def reliable(func, k, kc):
    return O(func(k)) or \
        C(func(k)) and \
        exists(kc, lambda l: P(func(l)) and alg(l)==alg(k))

def valid(kc):
    return forall(kc, lambda x: True, lambda k:
        (
            impl(not H(ds(k))     and "ksk" in roles(k), exists(kc, lambda l:
                alg(k) == alg(l) and
                reliable(ds, l, kc) and 
                reliable(dnskey, l, kc) and 
                "ksk" in roles(l)
            ))
            or reliable(dnskey, k, kc)
        )
        and
        (
            impl(not H(dnskey(k)), exists(kc, lambda m:
                alg(k) == alg(m) and
                reliable(dnskey, m, kc) and 
                reliable(rrsig, m, kc) and 
                "zsk" in roles(m)
            )) 
            or reliable(rrsig, k, kc)
        )
    ) and \
    exists(kc, lambda k: O(ds(k)) or C(ds(k)) and exists(kc, lambda l: P(ds(l))))

def proc_ds(kc, k, ds_record, now):
    if H(ds_record):
        if goal(k) == OMNIPRESENT:
            if not minds(k):
                if O(dnskey(k)) or exists(kc, lambda l:
                        alg(l) == alg(k) and
                        reliable(ds, l, kc) and
                        reliable(dnskey, l, kc)):
                    return RUMOURED
            if minds(k) and O(dnskey(k)):
                    return COMMITTED
               

    elif R(ds_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if ds_record.time <= now:
                return OMNIPRESENT

    elif C(ds_record):
        if ds_record.time <= now:
            return OMNIPRESENT

    elif O(ds_record):
        if goal(k) == HIDDEN:
            if  O(dnskey(k)) and exists(kc, lambda l: C(ds(l))):
                return POSTCOMMITTED
            if not P(dnskey(k)) and \
                exists(kc, lambda l: 
                    k!=l and
                    O(ds(l))) and \
                forall(kc, lambda x: True, lambda l:
                    alg(k) != alg(l) or
                    H(ds(l)) or
                    reliable(dnskey, l, kc) or
                    exists(kc, lambda m:
                        alg(m) == alg(l) and
                        reliable(ds, m, kc) and
                        reliable(dnskey, m, kc) and
                        m != k
                    )
                ):
                return UNRETENTIVE

    elif P(ds_record):
        if ds_record.time <= now:
            return HIDDEN
            
    elif U(ds_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if ds_record.time <= now:
            return HIDDEN
    return state(ds_record)

def proc_dnskey(kc, k, dnskey_record, now):
    if H(dnskey_record):
        if goal(k) == OMNIPRESENT:
            if mindnskey(k):
                if (not "ksk" in roles(k) or O(ds(k))) and \
                (not "zsk" in roles(k) or O(rrsig(k))):
                    return COMMITTED
            if not mindnskey(k) or not exists(kc, lambda l: alg(l)==alg(k) and reliable(dnskey, l, kc) and "ksk" in roles(l)):
                if O(rrsig(k)):
                    return RUMOURED
                if exists(kc, lambda l:
                    alg(k) == alg(l) and
                    reliable(dnskey, l, kc) and
                    reliable(rrsig, l, kc)):
                    return RUMOURED
    
    elif R(dnskey_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if dnskey_record.time <= now:
                return OMNIPRESENT
    
    elif C(dnskey_record):
        if dnskey_record.time <= now:
            return OMNIPRESENT
                
    elif O(dnskey_record):
        if goal(k) == HIDDEN:
            if ("ksk" not in roles(k) or O(ds(k))) and ("zsk" not in roles(k) or O(rrsig(k))) and exists(kc, lambda l: C(dnskey(l)) and alg(k) == alg(l) and roles(k) == roles(l)):
                return POSTCOMMITTED
            #~ if ("zsk" not in roles(k) or not MinSig or H(rrsig(k)) or not exists(kc, lambda l: H(rrsig(l)) and alg(k) == alg(l))):
            if not P(ds(k)) and not P(rrsig(k)) and \
                forall(kc, lambda x: True, lambda l:
                    ("ksk" not in roles(l) or 
                    H(ds(l)) or 
                    (reliable(dnskey, l, kc) and k != l) or 
                    exists(kc, lambda m:
                        alg(m)==alg(l) and 
                        reliable(ds, m, kc) and 
                        reliable(dnskey, m, kc) and 
                        k != m))
                and
                    (H(dnskey(l)) or
                    reliable(rrsig, l, kc) or 
                    exists(kc, lambda m:
                        alg(m)==alg(l) and 
                        k != m and 
                        reliable(dnskey, m, kc) and 
                        reliable(rrsig, m, kc)))):            
                return UNRETENTIVE

    elif P(dnskey_record):
        if dnskey_record.time <= now:
            return HIDDEN
            
    elif U(dnskey_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if dnskey_record.time <= now:
            return HIDDEN
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record, now):
    if H(rrsig_record):
        if goal(k) == OMNIPRESENT:
            if minrrsig(k):
                if O(dnskey(k)):
                    return COMMITTED
            if not minrrsig(k) or not exists(kc, lambda l: reliable(rrsig, l, kc) and alg(k)==alg(l)):
                return RUMOURED
        
    elif C(rrsig_record):
        if rrsig_record.time <= now:
            return OMNIPRESENT

    elif R(rrsig_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if rrsig_record.time <= now:
                return OMNIPRESENT

    elif O(rrsig_record):
        if goal(k) == HIDDEN:
            if O(dnskey(k)) and exists(kc, lambda l: C(rrsig(l)) and alg(k)==alg(l)):
                return POSTCOMMITTED
            if (not P(dnskey(k))):
                if H(dnskey(k)):
                    debug(k, "rrsig P->C (28) DNSKEY is ceased")
                    return UNRETENTIVE
                if exists(kc, lambda l:
                    k!=l and
                    alg(k) == alg(l) and
                    reliable(dnskey, l, kc) and
                    reliable(rrsig, l, kc)):
                    debug(k, "rrsig P->C (28) For all roles there is another key ready")
                    return UNRETENTIVE
    
    elif P(rrsig_record):
        if rrsig_record.time <= now:
            return HIDDEN 

    elif U(rrsig_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if rrsig_record.time <= now:
            return HIDDEN 
            
    return state(rrsig_record)

def proc_key(kc, k, now):
    changed = False
    for rrtype, record in k.records.items():
        newstate = state(record)
        if rrtype == "ds":
            newstate = proc_ds(kc, k, record, now)
        elif rrtype == "dnskey":
            newstate = proc_dnskey(kc, k, record, now)
        elif rrtype == "rrsig":
            newstate = proc_rrsig(kc, k, record, now)

        if newstate != state(record):
            record.state = newstate
            record.time = now + 1
            changed |= True
    return changed

def enforce_step(kc, now):
    # remove old keys
    dead = set(filter(lambda k:
                            impl("ksk" in roles(k), H(ds(k)) and H(dnskey(k))) and
                            impl("zsk" in roles(k), H(dnskey(k)) and H(rrsig(k))) and
                            goal(k) == HIDDEN
                            , kc))
    kc.difference_update(dead)
    upd = False
    changed = True
    while changed:
        changed = False
        for k in kc:
            changed |= proc_key(kc, k, now)
        upd |= changed
    if VERBOSE > 0: printkc(kc)
    if not valid(kc):
        printkc(kc)
        print "!!!!!!!!!!!!!!  [bogus]  !!!!!!!!!!!!!!"
        exit(1)
    return upd
    
def enforce(kc):
    now = 0
    print "\t\tBEGIN STATE"
    printkc(kc)
    print "\t\tSTART ENFORCER\n"
    upd = True
    while upd:
        upd = enforce_step(kc, now)
        now += 1
        if VERBOSE > 0: print "\t\tADVANCING TIME"
    print "\t\tEND ENFORCER"


kc = set()

##          Key(name alg, roles, goal, state)

##### test
#~ kc = set()
#~ kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSKA", 2, set(["zsk"]), OMNIPRESENT, HIDDEN, True, True, False))
#~ kc.add(Key("ZSKB", 2, set(["zsk"]), OMNIPRESENT, HIDDEN, False, False, True))
#~ enforce(kc)


#~ #### zsk rollover
#~ kc = set()
#~ kc.add(Key("KSK1", 2, set(["ksk"]), OMNIPRESENT, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT, False, True, False))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN, False, False, True))
#~ enforce(kc)

#~ #### ksk rollover
#~ kc = set()
#~ kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), OMNIPRESENT, OMNIPRESENT))
#~ enforce(kc)

#~ #### zsk+ksk rollover
#~ kc = set()
#~ kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT, False, True, False))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN, False, False, True))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT, False, False, False))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN, False, False, True))
#~ enforce(kc)

#~ #### zsk+ksk new alg rollover
#~ kc = set()
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk/ksk rollover
#~ kc = set()
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN, False, False, False))
#~ enforce(kc)

#~ #### zsk/ksk alg rollover
#~ kc = set()
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### ksk new alg rollover
#~ kc = set()
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ enforce(kc)

#~ #### zsk new alg rollover
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN, False, True, False))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK1", 2, set(["zsk"]), OMNIPRESENT, OMNIPRESENT))
enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc = set()
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc = set()
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll nieuw alg
#~ kc = set()
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc = set()
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc = set()
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK3", 3, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)
#~ 
#~ 
#~ 
#~ kc = set()
#~ kc.add(Key("init", 1, set(["ksk", "zsk"]), OMNIPRESENT, OMNIPRESENT))
#~ 
#~ print "\t\tBEGIN STATE"
#~ printkc(kc)
#~ print "\t\tSTART ENFORCER\n"
#~ 
#~ now = 0
#~ 
#~ while True:
    #~ for k in kc: k.goal = choice([OMNIPRESENT, HIDDEN])
    #~ for i in range(randint(0, 1)):
        #~ kc.add(Key("r"+str(randint(100, 999)), randint(0, 1), 
            #~ set(sample(["ksk", "zsk"], randint(1, 2))), OMNIPRESENT, HIDDEN))
#~ 
    #~ enforce_step(kc, now)
    #~ if VERBOSE > 0: print "\t\tADVANCING TIME"
    #~ now += 1
#~ print "\t\tEND ENFORCER\n"
#~ print "\t\tEND STATE"
#~ printkc(kc)
