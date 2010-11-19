#!/usr/bin/env python

#~ VERBOSE = True
VERBOSE = False

HIDDEN      = "H"
RUMOURED    = "R"
OMNIPRESENT = "O"
SQUASHED    = "S"

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

def printkc(kc):
    keys = list(kc)
    keys.sort(lambda k, l: (k.name > l.name)*2-1)
    for k in keys:
        print "key %s: [%s %s %s] goal(k)=%s alg(k)=%s %s"%(str(k.name), 
            str(k.records["ds"].state), str(k.records["dnskey"].state), 
            str(k.records["rrsig"].state), str(k.goal), str(k.alg), 
            str(",".join(list(roles(k)))))
    print ""

def debug(k, s):
    if VERBOSE:
        print "-", k.name, s

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

def ds(k):    return k.records["ds"]
def dnskey(k):return k.records["dnskey"]
def rrsig(k): return k.records["rrsig"]
def goal(k):  return k.goal
def alg(k):   return k.alg
def roles(k): return k.roles
def state(r): return r.state

def proc_ds(kc, k, ds_record):
    if state(ds_record) == HIDDEN:
        if goal(k) == OMNIPRESENT:
            if not "ksk" in roles(k):
                return RUMOURED
            if state(k.records["dnskey"]) == OMNIPRESENT:
                return RUMOURED
            if exists(kc, lambda l:
                    alg(l) == alg(k) and
                    "ksk" in roles(l) and
                    state(ds(l)) == OMNIPRESENT and
                    state(dnskey(l)) == OMNIPRESENT and
                    state(rrsig(l)) == OMNIPRESENT):
                return RUMOURED

    elif state(ds_record) == RUMOURED:
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) ==OMNIPRESENT:
            if not "ksk" in roles(k):
                return OMNIPRESENT
            if ds_record.time <= k.time:
                return OMNIPRESENT

    elif state(ds_record) == OMNIPRESENT:
        if goal(k) == HIDDEN:
            #~ if not "ksk" in roles(k):
                #~ return SQUASHED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == OMNIPRESENT and
                        state(dnskey(l)) == OMNIPRESENT and
                        state(rrsig(l)) == OMNIPRESENT)):
                debug(k, "ds P->C (12) Another key with alg(k)=%s is ready."%str(alg(k)))
                return SQUASHED
            if "ksk" in roles(k) and \
                    forall(k.roles, lambda x: True, lambda r:
                        forall(kc, lambda l: k!=l and
                            alg(k) == alg(l) and
                            r in roles(l),
                            lambda l:
                                state(ds(l)) == HIDDEN)
                        and exists(kc, lambda l:
                            k!=l and
                            r in roles(l) and
                            state(ds(l)) == OMNIPRESENT and
                            state(dnskey(l)) == OMNIPRESENT and
                            state(rrsig(l)) == OMNIPRESENT)):
                debug(k, "ds P->C (12) All ds with same alg,role are in C|G and there is another key for each role")
                return SQUASHED
            if not "ksk" in roles(k) and\
                    not exists(kc, lambda l: k != l and
                    "ksk" in roles(l) and
                    alg(k) == alg(l) and
                    #~ state(ds(l)) == OMNIPRESENT and
                    state(dnskey(l)) == OMNIPRESENT and
                    state(rrsig(l)) == OMNIPRESENT):
                debug(k, "ds P->C (12) there not a ksk requiring me.")
                return SQUASHED

    elif state(ds_record) == SQUASHED:
        if goal(k) == HIDDEN:
            if not "ksk" in roles(k):
                return HIDDEN
            if ds_record.time <= k.time:
                return HIDDEN
    return state(ds_record)

def proc_dnskey(kc, k, dnskey_record):
    if state(dnskey_record) == HIDDEN:
        if goal(k) == OMNIPRESENT:
            if state(rrsig(k)) == OMNIPRESENT and "zsk" in roles(k):
                return RUMOURED
            if state(rrsig(k)) == OMNIPRESENT and  \
                    exists(kc, lambda l:
                        alg(k) == alg(l) and
                        "zsk" in roles(l) and
                        state(ds(l)) == OMNIPRESENT and
                        state(dnskey(l)) == OMNIPRESENT and
                        state(rrsig(l)) == OMNIPRESENT):
                return RUMOURED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == OMNIPRESENT and
                        state(dnskey(l)) == OMNIPRESENT and
                        state(rrsig(l)) == OMNIPRESENT)):
                return RUMOURED
    
    elif state(dnskey_record) == RUMOURED:
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if dnskey_record.time <= k.time:
                return OMNIPRESENT
                
    elif state(dnskey_record) == OMNIPRESENT:
        if goal(k) == HIDDEN:
            if state(ds(k)) == HIDDEN and state(rrsig(k)) == HIDDEN:
                debug(k, "dnskey P->C (20) DS and RRSIG already ceased")
                return SQUASHED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                    k!=l and
                    alg(k) == alg(l) and
                    r in roles(l) and
                    state(ds(l)) == OMNIPRESENT and
                    state(dnskey(l)) == OMNIPRESENT and
                    state(rrsig(l)) == OMNIPRESENT)):
                debug(k, "dnskey P->C (20) For each role another key is ready")
                return SQUASHED
            if state(ds(k)) == HIDDEN and \
                    forall(roles(k), lambda x: True, lambda r:
                    forall(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l), lambda l:
                        state(dnskey(l)) == HIDDEN)
                    and exists(kc, lambda l:
                        k!=l and
                        r in roles(l) and
                        state(ds(l)) == OMNIPRESENT and
                        state(dnskey(l)) == OMNIPRESENT and
                        state(rrsig(l)) == OMNIPRESENT)):
                debug(k, "dnskey P->C (20) DS is ceased and for role there is no DNSKEY with the same alg rumoured")
                return SQUASHED
                        
    elif state(dnskey_record) == SQUASHED:
        if goal(k) == HIDDEN:
            if not "ksk" in roles(k):
                return HIDDEN
            if dnskey_record.time <= k.time:
                return HIDDEN
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record):
    if state(rrsig_record) == HIDDEN:
        if goal(k) == OMNIPRESENT:
            return RUMOURED
        
    elif state(rrsig_record) == RUMOURED:
        if goal(k) == HIDDEN:
            return SQUASHED
        if goal(k) == OMNIPRESENT:
            if rrsig_record.time <= k.time:
                return OMNIPRESENT

    elif state(rrsig_record) == OMNIPRESENT:
        if goal(k) == HIDDEN:
            if state(dnskey(k)) == HIDDEN:
                debug(k, "rrsig P->C (28) DNSKEY is ceased")
                return SQUASHED
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == OMNIPRESENT and
                        state(dnskey(l)) == OMNIPRESENT and
                        state(rrsig(l)) == OMNIPRESENT)):
                debug(k, "rrsig P->C (28) For all roles there is another key ready")
                return SQUASHED
    
    elif state(rrsig_record) == SQUASHED:
        if goal(k) == HIDDEN:
            if state(dnskey(k)) == HIDDEN:
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


def enforce(kc):
    print "# id: [ds dnskey rrsig] goal alg roles\n"
    print "\t\tBEGIN STATE"
    printkc(kc)
    print "\t\tSTART ENFORCER\n"
    wait = False
    changed = True
    while changed or wait:
        changed = False
        for k in kc:
            changed |= proc_key(kc, k)
        if changed:
            wait = False
        else:
            if not wait:
                printkc(kc)
                for k in kc: k.time += 1
                wait = True
                print "\t\tADVANCING TIME"
            else:
                break
    print "\t\tEND ENFORCER"


kc = set()

##          Key(name alg, roles, goal, state)

#### zsk rollover
kc.add(Key("KSK1", 2, set(["ksk"]), OMNIPRESENT, OMNIPRESENT))
kc.add(Key("ZSK1", 2, set(["zsk"]), HIDDEN, OMNIPRESENT))
kc.add(Key("ZSK2", 2, set(["zsk"]), OMNIPRESENT, HIDDEN))
enforce(kc)

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
