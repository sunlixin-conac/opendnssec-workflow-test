#!/usr/bin/env python

#~ VERBOSE = True
VERBOSE = False

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
    if state(ds_record) == "G":
        if goal(k) =="C":
            return "C"
        if goal(k) =="P":
            if not "ksk" in roles(k):
                return "S"
            if state(k.records["dnskey"]) == "P":
                return "S"
            if exists(kc, lambda l:
                    alg(l) == alg(k) and
                    "ksk" in roles(l) and
                    state(ds(l)) == "P" and
                    state(dnskey(l)) == "P" and
                    state(rrsig(l)) == "P"):
                return "S"

    elif state(ds_record) == "S":
        if goal(k) =="C":
            return "W"
        if goal(k) =="P":
            if not "ksk" in roles(k):
                return "P"
            if ds_record.time <= k.time:
                return "P"

    elif state(ds_record) == "P":
        if goal(k) =="C":
            #~ if not "ksk" in roles(k):
                #~ return "W"
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == "P" and
                        state(dnskey(l)) == "P" and
                        state(rrsig(l)) == "P")):
                debug(k, "ds P->C (12) Another key with alg(k)=%s is ready."%str(alg(k)))
                return "W"
            #~ if forall(k.roles, lambda x: True, lambda r:
                    #~ forall(kc, lambda l: k!=l and
                        #~ alg(k) == alg(l) and
                        #~ r in roles(l),
                        #~ lambda l:
                            #~ state(ds(l)) in set(["G", "C"]))
                    #~ and exists(kc, lambda l:
                        #~ k!=l and
                        #~ r in roles(l) and
                        #~ state(ds(l)) == "P" and
                        #~ state(dnskey(l)) == "P" and
                        #~ state(rrsig(l)) == "P")):
                #~ debug(k, "ds P->C (12) All ds with same alg,role are in C|G and there is another key for each role")
                #~ return "W"
            if "ksk" in roles(k) and \
                    forall(k.roles, lambda x: True, lambda r:
                        forall(kc, lambda l: k!=l and
                            alg(k) == alg(l) and
                            r in roles(l),
                            lambda l:
                                state(ds(l)) in set(["G", "C"]))
                        and exists(kc, lambda l:
                            k!=l and
                            r in roles(l) and
                            state(ds(l)) == "P" and
                            state(dnskey(l)) == "P" and
                            state(rrsig(l)) == "P")):
                debug(k, "ds P->C (12) All ds with same alg,role are in C|G and there is another key for each role")
                return "W"
            if not "ksk" in roles(k) and\
                    not exists(kc, lambda l: k != l and
                    "ksk" in roles(l) and
                    alg(k) == alg(l) and
                    #~ state(ds(l)) == "P" and
                    state(dnskey(l)) == "P" and
                    state(rrsig(l)) == "P"):
                debug(k, "ds P->C (12) there not a ksk requiring me.")
                return "W"

    elif state(ds_record) == "W":
        if goal(k) =="C":
            if not "ksk" in roles(k):
                return "C"
            if ds_record.time <= k.time:
                return "C"
    return state(ds_record)

def proc_dnskey(kc, k, dnskey_record):
    if state(dnskey_record) == "G":
        if goal(k) == "C":
            return "C"
        if goal(k) == "P":
            if state(rrsig(k)) == "P" and "zsk" in roles(k):
                return "S"
            if state(rrsig(k)) == "P" and  \
                    exists(kc, lambda l:
                        alg(k) == alg(l) and
                        "zsk" in roles(l) and
                        state(ds(l)) == "P" and
                        state(dnskey(l)) == "P" and
                        state(rrsig(l)) == "P"):
                return "S"
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == "P" and
                        state(dnskey(l)) == "P" and
                        state(rrsig(l)) == "P")):
                return "S"
    
    elif state(dnskey_record) == "S":
        if goal(k) == "C":
            return "W"
        if goal(k) == "P":
            if dnskey_record.time <= k.time:
                return "P"
                
    elif state(dnskey_record) == "P":
        if goal(k) == "C":
            if state(ds(k)) == "C" and state(rrsig(k)) == "C":
                debug(k, "dnskey P->C (20) DS and RRSIG already ceased")
                return "W"
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l:
                    k!=l and
                    alg(k) == alg(l) and
                    r in roles(l) and
                    state(ds(l)) == "P" and
                    state(dnskey(l)) == "P" and
                    state(rrsig(l)) == "P")):
                debug(k, "dnskey P->C (20) For each role another key is ready")
                return "W"
            if state(ds(k)) == "C" and \
                    forall(roles(k), lambda x: True, lambda r:
                    forall(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l), lambda l:
                        state(dnskey(l)) in set(["G", "C"]))
                    and exists(kc, lambda l:
                        k!=l and
                        r in roles(l) and
                        state(ds(l)) == "P" and
                        state(dnskey(l)) == "P" and
                        state(rrsig(l)) == "P")):
                debug(k, "dnskey P->C (20) DS is ceased and for role there is no DNSKEY with the same alg rumoured")
                return "W"
                        
    elif state(dnskey_record) == "W":
        if goal(k) == "C":
            if not "ksk" in roles(k):
                return "C"
            if dnskey_record.time <= k.time:
                return "C"
                
    return state(dnskey_record)

def proc_rrsig(kc, k, rrsig_record):
    if state(rrsig_record) == "G":
        if goal(k) == "C":
            return "C"
        if goal(k) == "P":
            return "S"
        
    elif state(rrsig_record) == "S":
        if goal(k) == "C":
            return "W"
        if goal(k) == "P":
            if rrsig_record.time <= k.time:
                return "P"

    elif state(rrsig_record) == "P":
        if goal(k) == "C":
            if state(dnskey(k)) == "C":
                debug(k, "rrsig P->C (28) DNSKEY is ceased")
                return "W"
            if forall(roles(k), lambda x: True, lambda r:
                    exists(kc, lambda l: 
                        k!=l and
                        alg(k) == alg(l) and
                        r in roles(l) and
                        state(ds(l)) == "P" and
                        state(dnskey(l)) == "P" and
                        state(rrsig(l)) == "P")):
                debug(k, "rrsig P->C (28) For all roles there is another key ready")
                return "W"
    
    elif state(rrsig_record) == "W":
        if goal(k) == "C":
            if state(dnskey(k)) == "C":
                return "C"
            if rrsig_record.time <= k.time:
                return "C" 
            
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

##          name alg, roles, goal, state

#### zsk rollover
kc.add(Key("KSK1", 2, set(["ksk"]), "P", "P"))
kc.add(Key("ZSK1", 2, set(["zsk"]), "C", "P"))
kc.add(Key("ZSK2", 2, set(["zsk"]), "P", "G"))
enforce(kc)

#~ #### ksk rollover
#~ kc.add(Key("KSK1", 2, set(["ksk"]), "C", "P"))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), "P", "P"))
#~ enforce(kc)

#~ #### zsk+ksk rollover
#~ kc.add(Key("KSK1", 2, set(["ksk"]), "C", "P"))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 2, set(["zsk"]), "C", "P"))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk+ksk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "C", "P"))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "C", "P"))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk/ksk rollover
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), "C", "P"))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk/ksk alg rollover
#~ kc.add(Key("CSK1", 1, set(["ksk", "zsk"]), "C", "P"))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### ksk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "C", "P"))
#~ kc.add(Key("KSK2", 2, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "C", "P"))
#~ enforce(kc)

#~ #### zsk new alg rollover
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "C", "P"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "C", "P"))
#~ kc.add(Key("ZSK2", 2, set(["zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "C", "P"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "C", "P"))
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("CSK2", 1, set(["ksk", "zsk"]), "C", "P"))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll nieuw alg
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), "C", "P"))
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "P", "G"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "P", "G"))
#~ enforce(kc)

#~ #### zsk,ksk  to csk roll
#~ kc.add(Key("KSK1", 1, set(["ksk"]), "C", "P"))
#~ kc.add(Key("ZSK1", 1, set(["zsk"]), "C", "P"))
#~ kc.add(Key("CSK2", 2, set(["ksk", "zsk"]), "P", "G"))
#~ enforce(kc)
