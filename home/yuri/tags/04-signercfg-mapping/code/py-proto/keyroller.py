#!/usr/bin/env python
from random import randint, sample, choice

VERBOSE = 1

HIDDEN      = " "
RUMOURED    = "+"
OMNIPRESENT = "O"
UNRETENTIVE = "-"

# Double sig
#~ MinFlux = False
#~ MinSig  = False

#~ # Pre pub
MinFlux = False
MinSig  = True

#~ # Pre pub reuse
#~ MinFlux = True
#~ MinSig  = True

# MinFlux -> MinSig. Always.
MinSig |= MinFlux

class Record:
    def __init__(self, state):
        self.state = state
        self.time = 0
    def __repr__(self):
        return str(self.state)

class Key:
    def __init__(self, name, alg, roles, goal, initstate):
        self.name = name
        self.alg = alg
        self.roles = roles
        self.goal = goal
        self.records = {"dnskey": Record(initstate)}
        if "ksk" in roles:
            self.records["ds"] = Record(initstate)
        if "zsk" in roles:
            self.records["rrsig"] = Record(initstate)
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
        
        pub = R(dnskey(k)) or O(dnskey(k)) and \
                (not "zsk" in roles(k) or \
                (MinFlux) or \
                (goal(k) == OMNIPRESENT  or not exists(kc, lambda l:
                #~ l != k and
                O(dnskey(l)) and
                (O(rrsig(l)) or R(rrsig(l))) and
                goal(l) == OMNIPRESENT and
                alg(k) == alg(l)
            )))
        act = (R(rrsig(k)) or O(rrsig(k))) and \
                "zsk" in roles(k) and \
                (goal(k) == OMNIPRESENT  or not exists(kc, lambda l:
                #~ l != k and
                O(dnskey(l)) and
                (O(rrsig(l)) or R(rrsig(l))) and
                goal(l) == OMNIPRESENT and
                alg(k) == alg(l)
            ))
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
def H(r):     return state(r) == HIDDEN
def R(r):     return state(r) == RUMOURED
def O(r):     return state(r) == OMNIPRESENT
def U(r):     return state(r) == UNRETENTIVE

def valid(kc):
    return forall(kc, lambda x: True, lambda k:
        (
            impl(not H(ds(k))     and "ksk" in roles(k), exists(kc, lambda l:
                alg(k) == alg(l) and
                O(ds(l)) and 
                O(dnskey(l)) and 
                "ksk" in roles(l)
            ))
            or O(dnskey(k))
        )
        and
        (
            impl(not H(dnskey(k)), exists(kc, lambda m:
                alg(k) == alg(m) and
                O(dnskey(m)) and 
                O(rrsig(m)) and 
                "zsk" in roles(m)
            )) 
            or O(rrsig(k))
        )
    ) and \
    exists(kc, lambda n: O(ds(n)) and "ksk" in roles(n))

def proc_ds(kc, k, ds_record, now):
    if H(ds_record):
        if goal(k) == OMNIPRESENT:
            if O(dnskey(k)):
                return RUMOURED
            if exists(kc, lambda l:
                    alg(l) == alg(k) and
                    O(ds(l)) and
                    O(dnskey(l))):
                return RUMOURED

    elif R(ds_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if ds_record.time <= now:
                return OMNIPRESENT

    elif O(ds_record):
        if goal(k) == HIDDEN:
            if  exists(kc, lambda l: 
                    k!=l and
                    O(ds(l))) and \
                forall(kc, lambda x: True, lambda l:
                    alg(k) != alg(l) or
                    H(ds(l)) or
                    O(dnskey(l)) or
                    exists(kc, lambda m:
                        alg(m) == alg(l) and
                        O(ds(m)) and
                        O(dnskey(m)) and
                        m != k
                    )
                ):
                return UNRETENTIVE

    elif U(ds_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if ds_record.time <= now:
            return HIDDEN
    return state(ds_record)

def proc_dnskey(kc, k, dnskey_record, now):
    if H(dnskey_record):
        if goal(k) == OMNIPRESENT:
            #~ if not "zsk" in roles(k):
                #~ return RUMOURED
            if O(rrsig(k)):
                return RUMOURED
            if exists(kc, lambda l:
                alg(k) == alg(l) and
                O(dnskey(l)) and
                O(rrsig(l))):
                return RUMOURED
    
    elif R(dnskey_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if dnskey_record.time <= now:
                return OMNIPRESENT
                
    elif O(dnskey_record):
        if goal(k) == HIDDEN:
            if ("zsk" not in roles(k) or not MinSig or H(rrsig(k)) or not exists(kc, lambda l: H(rrsig(l)) and alg(k) == alg(l))):
                if  forall(kc, lambda x: True, lambda l:
                        ("ksk" not in roles(l) or 
                        H(ds(l)) or 
                        (O(dnskey(l)) and k != l) or 
                        exists(kc, lambda m:
                            alg(m)==alg(l) and 
                            O(ds(m)) and 
                            O(dnskey(m)) and 
                            k != m))
                    and
                        (H(dnskey(l)) or
                        O(rrsig(l)) or 
                        exists(kc, lambda m:
                            alg(m)==alg(l) and 
                            k != m and 
                            O(dnskey(m)) and 
                            O(rrsig(m))))):            
                    return UNRETENTIVE

    elif U(dnskey_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if dnskey_record.time <= now:
            return HIDDEN
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record, now):
    if H(rrsig_record):
        if goal(k) == OMNIPRESENT:
            if (not MinSig or O(dnskey(k)) or not exists(kc, lambda l: O(dnskey(l)) and alg(k) == alg(l))):
                return RUMOURED
        
    elif R(rrsig_record):
        if goal(k) == HIDDEN:
            return UNRETENTIVE
        if goal(k) == OMNIPRESENT:
            if rrsig_record.time <= now:
                return OMNIPRESENT

    elif O(rrsig_record):
        if goal(k) == HIDDEN:
            if H(dnskey(k)):
                debug(k, "rrsig P->C (28) DNSKEY is ceased")
                return UNRETENTIVE
            if exists(kc, lambda l:
                k!=l and
                alg(k) == alg(l) and
                O(dnskey(l)) and
                O(rrsig(l))):
                debug(k, "rrsig P->C (28) For all roles there is another key ready")
                return UNRETENTIVE
    
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
#kc = set()
#kc.add(Key("KSK1", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#kc.add(Key("KSK2", 3, set(["ksk"]), HIDDEN, OMNIPRESENT))
#kc.add(Key("ZSK1", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#kc.add(Key("ZSK2", 3, set(["zsk"]), HIDDEN, OMNIPRESENT))
#enforce(kc)


#~ #### zsk rollover
kc = set()
kc.add(Key("KSK1", 2, set(["ksk"]), OMNIPRESENT, OMNIPRESENT))
kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)

#### ksk rollover
kc = set()
kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 2, set(["zsk"]), OMNIPRESENT, OMNIPRESENT))
enforce(kc)
#~ 
#### zsk+ksk rollover
kc = set()
kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk+ksk new alg rollover
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk/ksk rollover
kc = set()
kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk/ksk alg rollover
kc = set()
kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### ksk new alg rollover
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
enforce(kc)
#~ 
#### zsk new alg rollover
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk,ksk  to csk roll
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk,ksk  to csk roll
kc = set()
kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk,ksk  to csk roll nieuw alg
kc = set()
kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk,ksk  to csk roll
kc = set()
kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
#~ 
#### zsk,ksk  to csk roll
kc = set()
kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK3", 3, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)
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
