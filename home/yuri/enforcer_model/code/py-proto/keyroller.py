#!/usr/bin/env python
from random import randint, sample, choice

VERBOSE = 2

HIDDEN      = " "
RUMOURED    = "+"
OMNIPRESENT = "O"
SQUASHED    = "-"

#~ AllowSmooth = True
#~ AllowSmooth = False

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
        print "key %s: [%s %s %s] goal(k)=%s alg(k)=%s %s"%(str(k.name), 
            strds, strdnskey, 
            strrrsig, str(k.goal), str(k.alg), 
            str(",".join(list(roles(k)))))
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
def S(r):     return state(r) == SQUASHED

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
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if ds_record.time <= now:
                return OMNIPRESENT

    elif O(ds_record):
        if goal(k) == HIDDEN:
            if exists(kc, lambda l: 
                k!=l and
                O(ds(l)) and
                O(dnskey(l)) and
                exists(kc, lambda m:
                    alg(m) == alg(l) and
                    O(dnskey(m)) and
                    O(rrsig(m))) and
                (
                    alg(k) == alg(l) or
                    not exists(kc, lambda m:
                        m != k and
                        #~ not O(ds(m)) and
                        not H(ds(m))
                    )
                )):
                return SQUASHED

    elif S(ds_record):
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
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if dnskey_record.time <= now:
                return OMNIPRESENT
                
    elif O(dnskey_record):
        if goal(k) == HIDDEN:
            #~ if not exists(kc, lambda l:
                #~ alg(k) == alg(l) and
                #~ O(ds(l))
                #~ ):
                #~ return SQUASHED
            if (not "ksk" in roles(k) or
                H(ds(k)) or
                exists(kc, lambda l:
                    k != l and
                    alg(k) == alg(l) and
                    O(ds(l)) and
                    O(dnskey(l)))) \
                and \
                (not "zsk" in roles(k) or
                H(rrsig(k)) or
                exists(kc, lambda m:
                    k != m and
                    alg(k) == alg(m) and
                    O(dnskey(m)) and
                    O(rrsig(m))) or
                not exists(kc, lambda m:
                    k != m and
                    alg(k) == alg(m) and
                    not H(dnskey(m)))):
                return SQUASHED
                        
    elif S(dnskey_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        if dnskey_record.time <= now:
            return HIDDEN
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record, now):
    if H(rrsig_record):
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        
    elif R(rrsig_record):
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if rrsig_record.time <= now:
                return OMNIPRESENT

    elif O(rrsig_record):
        if goal(k) == HIDDEN:
            if H(dnskey(k)):
                debug(k, "rrsig P->C (28) DNSKEY is ceased")
                return SQUASHED
            if exists(kc, lambda l:
                k!=l and
                alg(k) == alg(l) and
                O(dnskey(l)) and
                O(rrsig(l))):
                debug(k, "rrsig P->C (28) For all roles there is another key ready")
                return SQUASHED
    
    elif S(rrsig_record):
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

#### zsk rollover
#~ kc.add(Key("KSK1", 2, set(["ksk"]), OMNIPRESENT, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### ksk rollover
#~ kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), OMNIPRESENT, OMNIPRESENT))
#~ enforce(kc)

#~ #### zsk+ksk rollover
#~ kc.add(Key("KSK1", 2, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk+ksk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk/ksk rollover
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk/ksk alg rollover
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### ksk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ enforce(kc)

#~ #### zsk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll nieuw alg
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), OMNIPRESENT, HIDDEN))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("KSK1", 1, set(["ksk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), HIDDEN, OMNIPRESENT))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), OMNIPRESENT, HIDDEN))
#~ enforce(kc)



kc = set()
kc.add(Key("init", 1, set(["ksk", "zsk"]), OMNIPRESENT, OMNIPRESENT))

print "\t\tBEGIN STATE"
printkc(kc)
print "\t\tSTART ENFORCER\n"

now = 0

while True:
    kc = set(filter(lambda k: not(
                            impl("ksk" in roles(k), H(ds(k)) and H(dnskey(k))) and
                            impl("zsk" in roles(k), H(dnskey(k)) and H(rrsig(k))) and
                            goal(k) == HIDDEN
                            ), kc))
    for k in kc: k.goal = choice([OMNIPRESENT, HIDDEN])
    for i in range(randint(0, 1)):
        kc.add(Key("r"+str(randint(100, 999)), randint(0, 1), 
            set(sample(["ksk", "zsk"], randint(1, 2))), OMNIPRESENT, HIDDEN))

    enforce_step(kc, now)
    if VERBOSE > 0: print "\t\tADVANCING TIME"
    now += 1
print "\t\tEND ENFORCER\n"
print "\t\tEND STATE"
printkc(kc)
