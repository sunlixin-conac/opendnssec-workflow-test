#!/usr/bin/env python
from random import randint, sample, choice

VERBOSE = True
#~ VERBOSE = False

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

class Key:
    def __init__(self, name, alg, roles, goal, initstate):
        self.name = name
        self.alg = alg
        self.roles = roles
        self.goal = goal
        self.records = {"ds": Record(initstate),
                        "dnskey": Record(initstate),
                        "rrsig": Record(initstate)}
        self.time = 0
    
    def __repr__(self):
        return self.name

def printkc(kc):
    keys = list(kc)
    keys.sort(lambda k, l: (k.name > l.name)*2-1)
    for k in keys:
        print "key %s: [%s %s %s] goal(k)=%s alg(k)=%s %s"%(str(k.name), 
            str(k.records["ds"].state), str(k.records["dnskey"].state), 
            str(k.records["rrsig"].state), str(k.goal), str(k.alg), 
            str(",".join(list(roles(k)))))
    if not valid(kc):
        print "!!!!!!!!!!!!!!  [bogus]  !!!!!!!!!!!!!!"
        exit(1)
    print ""

def debug(k, s):
    if VERBOSE:
        print "-", str(k), str(s)

def exists(some_set, condition):
    for elem in some_set:
        if condition(elem):
            return True
    return False

def forall(some_set, precondition, condition):
    for elem in some_set:
        if precondition(elem) and not condition(elem):
            return False
    return True

def impl(a, b):
    if a and not b:
        return False
    return True

def ds(k):    return k.records["ds"]
def dnskey(k):return k.records["dnskey"]
def rrsig(k): return k.records["rrsig"]
def goal(k):  return k.goal
def alg(k):   return k.alg
def roles(k): return k.roles
def state(r): return r.state
def H(r):     return state(r) == HIDDEN
def R(r):     return state(r) == RUMOURED
def O(r):     return state(r) == OMNIPRESENT
def S(r):     return state(r) == SQUASHED

def valid(kc):
    ConsistentKeys = filter(lambda k: 
        impl(roles(k).issubset(set(["ksk"])), impl(not H(ds(k)), O(dnskey(k)))) and
        impl(not H(dnskey(k)), O(rrsig(k))) and
        (
            H(ds(k)) or
            impl(roles(k).issubset(set(["ksk"])), 
                exists(kc, lambda l:
                    "zsk" in roles(l) and
                    alg(l) == alg(k) and
                    O(dnskey(l)) and
                    O(rrsig(l))
                )
            )
        
        )        
    , kc)
    debug("ConsistentKeys", ConsistentKeys)
    
    SafeKeys = filter(lambda k: 
        k in ConsistentKeys or
        forall(roles(k), lambda x: True, lambda r:
            exists(kc, lambda l: 
                alg(k)==alg(l) and
                r in roles(l) and
                l in ConsistentKeys and
                impl(r=="ksk", impl(not H(ds(k)), O(ds(l)))) and
                impl(not H(dnskey(k)), O(dnskey(l)))
            )
        )
    , kc)
    debug("SafeKeys", SafeKeys)
    
    return forall(kc, lambda x: True, lambda k:
        k in SafeKeys and
        exists(kc, lambda k:
            "ksk" in roles(k) and
            O(ds(k)) and
            O(dnskey(k)) and
            O(rrsig(k)) and
            exists(kc, lambda l:
                "zsk" in roles(l) and
                O(dnskey(l)) and
                O(rrsig(l)) and
                alg(k)==alg(l)
            )
        )
    )

def proc_ds(kc, k, ds_record):
    if H(ds_record):
        if goal(k) == OMNIPRESENT:
            if not "ksk" in roles(k):
                return RUMOURED
            if O(k.records["dnskey"]):
                return RUMOURED
            if exists(kc, lambda l:
                    alg(l) == alg(k) and
                    "ksk" in roles(l) and
                    O(ds(l)) and
                    O(dnskey(l)) and
                    O(rrsig(l))):
                return RUMOURED

    elif R(ds_record):
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if not "ksk" in roles(k):
                return OMNIPRESENT
            if ds_record.time <= k.time:
                return OMNIPRESENT

    elif O(ds_record):
        if goal(k) == HIDDEN:
            #~ if not "ksk" in roles(k):
                #~ return SQUASHED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        O(ds(l)) and
                        O(dnskey(l)) and
                        O(rrsig(l)))):
                debug(k, "ds P->C (12) Another key with alg(k)=%s is ready."%str(alg(k)))
                return SQUASHED
            if "ksk" in roles(k) and \
                    forall(k.roles, lambda x: True, lambda r:
                        forall(kc, lambda l: k!=l and
                            alg(k) == alg(l) and
                            r in roles(l),
                            lambda l:
                                H(ds(l)))
                        and exists(kc, lambda l:
                            k!=l and
                            r in roles(l) and
                            O(ds(l)) and
                            O(dnskey(l)) and
                            O(rrsig(l)))):
                debug(k, "ds P->C (12) All ds with same alg,role are in C|G and there is another key for each role")
                return SQUASHED
            if not "ksk" in roles(k) and\
                    not exists(kc, lambda l: k != l and
                    "ksk" in roles(l) and
                    alg(k) == alg(l) and
                    #~ O(ds(l)) and
                    #~ O(dnskey(l)) and
                    not H(dnskey(l)) and
                    O(rrsig(l))):
                debug(k, "ds P->C (12) there not a ksk requiring me.")
                return SQUASHED

    elif S(ds_record):
        #~ if goal(k) == HIDDEN:
        if not "ksk" in roles(k):
            return HIDDEN
        if ds_record.time <= k.time:
            return HIDDEN
    return state(ds_record)

def proc_dnskey(kc, k, dnskey_record):
    if H(dnskey_record):
        if goal(k) == OMNIPRESENT:
            if O(rrsig(k)) and "zsk" in roles(k):
                return RUMOURED
            if O(rrsig(k)) and  \
                    exists(kc, lambda l:
                        alg(k) == alg(l) and
                        "zsk" in roles(l) and
                        O(ds(l)) and
                        O(dnskey(l)) and
                        O(rrsig(l))):
                return RUMOURED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        O(ds(l)) and
                        O(dnskey(l)) and
                        O(rrsig(l)))):
                return RUMOURED
    
    elif R(dnskey_record):
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if dnskey_record.time <= k.time:
                return OMNIPRESENT
                
    elif O(dnskey_record):
        if goal(k) == HIDDEN:
            if H(ds(k)) and H(rrsig(k)):
                debug(k, "dnskey P->C (20) DS and RRSIG already ceased")
                return SQUASHED
            #~ if not AllowSmooth:
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                    k!=l and
                    alg(k) == alg(l) and
                    r in roles(l) and
                    O(ds(l)) and
                    O(dnskey(l)) and
                    O(rrsig(l)))):
                debug(k, "dnskey P->C (20) For each role another key is ready")
                return SQUASHED
            if H(ds(k)) and \
                    forall(roles(k), lambda x: True, lambda r:
                    forall(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l), lambda l:
                        H(dnskey(l)))
                    and exists(kc, lambda l:
                        k!=l and
                        r in roles(l) and
                        O(ds(l)) and
                        O(dnskey(l)) and
                        O(rrsig(l)))):
                debug(k, "dnskey P->C (20) DS is ceased and for role there is no DNSKEY with the same alg rumoured")
                return SQUASHED
                        
    elif S(dnskey_record):
        #~ if goal(k) == HIDDEN:
        #~ if not "ksk" in roles(k) and not AllowSmooth:
        if not "ksk" in roles(k):
            return HIDDEN
        if dnskey_record.time <= k.time:
            return HIDDEN
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record):
    if H(rrsig_record):
        if goal(k) == OMNIPRESENT:
            #~ if not AllowSmooth:
                #~ return RUMOURED
            #~ if O(dnskey(k)):
                #~ return RUMOURED
            return RUMOURED
        
    elif R(rrsig_record):
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if rrsig_record.time <= k.time:
                return OMNIPRESENT
            #~ if AllowSmooth and O(dnskey(k)) and \
                    #~ forall(roles(k), lambda x: True,
                    #~ lambda r: exists(kc, lambda l: alg(k)==alg(l) and
                    #~ r in roles(l) and
                    #~ O(dnskey(l)) and
                    #~ O(rrsig(l)))):
                #~ return OMNIPRESENT

    elif O(rrsig_record):
        if goal(k) == HIDDEN:
            if H(dnskey(k)):
                debug(k, "rrsig P->C (28) DNSKEY is ceased")
                return SQUASHED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        O(ds(l)) and
                        O(dnskey(l)) and
                        O(rrsig(l)))):
                debug(k, "rrsig P->C (28) For all roles there is another key ready")
                return SQUASHED
    
    elif S(rrsig_record):
        #~ if goal(k) == HIDDEN:
        if H(dnskey(k)):
            return HIDDEN
        if rrsig_record.time <= k.time:
            return HIDDEN 
            
    return state(rrsig_record)

def proc_key(kc, k):
    changed = False
    for rrtype, record in k.records.items():
        newstate = state(record)
        if rrtype == "ds":
            newstate = proc_ds(kc, k, record)
        elif rrtype == "dnskey":
            newstate = proc_dnskey(kc, k, record)
        elif rrtype == "rrsig":
            newstate = proc_rrsig(kc, k, record)

        if newstate != state(record):
            record.state = newstate
            record.time = k.time + 1
            changed |= True
    return changed

def enforce_step(kc):
    upd = False
    changed = True
    while changed:
        changed = False
        for k in kc:
            changed |= proc_key(kc, k)
        upd |= changed
    return upd
    
def enforce(kc):
    print "\t\tBEGIN STATE"
    printkc(kc)
    print "\t\tSTART ENFORCER\n"
    upd = True
    while upd:
        upd = enforce_step(kc)
        printkc(kc)
        for k in kc: k.time += 1
        print "\t\tADVANCING TIME"
    print "\t\tEND ENFORCER"


kc = set()

##          Key(name alg, roles, goal, state)

#~ #### zsk rollover
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

while True:
    kc = set(filter(lambda k: not(H(ds(k)) and 
                            H(dnskey(k)) and 
                            H(rrsig(k)) and 
                            goal(k) == HIDDEN), kc))
    for k in kc: k.goal = choice([OMNIPRESENT, HIDDEN])
    if randint(0, 1):
        kc.add(Key("r"+str(randint(100, 999)), randint(0, 2), 
            set(sample(["ksk", "zsk"], randint(1, 2))), OMNIPRESENT, HIDDEN))

    enforce_step(kc)
    printkc(kc)
    for k in kc: k.time += 1
    print "\t\tADVANCING TIME"

print "\t\tEND ENFORCER"
