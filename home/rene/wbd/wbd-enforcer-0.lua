--[[
 *  iteration0.lua
 *  lua
 *
 *  Created by René Post on 12/9/10.
 *
--]]

--[[ --------------------------------
TARGETS
--]] --------------------------------

target{
	name="ods-enforcer",
	kind="bin",
	language="C",
	origin="signer",
	description[[
		Client that connects to the ods-enforcerd daemon and allows you to submit 
		commands to it, responses from the daemon are printed back to commandline.
	]],
	responsibility{
		name="Configuration", 
		"Read configuration file.",
		"Set flags based on configuration.",
	},
	responsibility{
		name="Connect to daemon",
		"Establish a bi-directional channel to the daemon.",
		"Report failure to connect back to the user via stderr.",
		"Report successfull connect back to the user via stdout.",
	},
	responsibility{
		name="Send commands",
		"Pass the commandline arguments to the daemon without processing.",
	},
	responsibility{
		name="Show command responses",
		"Monitor availability of responses from the daemon.",
		"Pass output responses received from the daemon to stdout without processing.",
		"Pass error responses received from the daemon to stderr without processing.",
		"Terminate the program when an 'end of response' message is received.",
	},
	requires{
		"log",
	},
	work{
		task{ hours=4, confidence=0.9, "Copy whole program from ods-signer." },
		task{ hours=8, confidence=0.7, "Refactor code to remove signer specific stuff leaving generic connection code." },
		task{ hours=8, confidence=0.7, "Add enforcer specific code in a separate module that uses the generic connection code." },
	},
}

target{
	name="ods-enforcerd",
	kind="bin",
	language="C",
	origin="new",
	description[[
		Key and signing policy enforcer deals with key rollover and key generation.
		It is a daemon that reads the policy associated with a zone and then performs 
		the key management according to that policy. The enforcer generates configuration files 
		for the ods-signer to perform the actual signing of RR sets. The ods-enforcer client can
		connect to this daemon to initiate enforcer commands.
	]],
	graphic{
		filepath="enforcer.pdf",
		refid="fig:odf-enforcerd",
		caption="Enforcer Dependencies"
	},
	requires{
		"daemon",
		"cmdhandler",

		"workflowhandler",
		
		"config",
		"configreader",
		"hsmpersistence",
		"confighandler",
		
		"policy",
		"policyreader",
		"policypersistence",
		"policyhandler",
		
		"zone",
		"zonereader",
		"zonepersistence",
		"zonehandler",
		
		"keystate",
		"keystatepersistence",
		"keystateenforcer",
		"keygenerator",
		"singerconfig",
		"signerconfigwriter",
		
		"audittrail",
		"audittrailpersistence",
		"audittraillogger",

		"datapersistence",
		"datadefinition",
		
		"privdrop",
		"log"
	},
	work{
		task{ hours=8, confidence=0.9, "Setup skeleton and hookup with the targets that actually implement the functionality." },
	},
}

target{
	name="daemon",
	kind="lib",
	language="C",
	origin="signer",
	description[[
		Code that makes the ods-enforcerd actually behave as a proper daemon process.
	]],
	responsibility{
		name="Daemon commandline options",
		"Setup the commandline options supported by the daemon.",
		"Proces the commandline during startup and retrieve the options that were set on the commandline.",
	},
	responsibility{
		name="Daemon",
		"Setup signal handlers appropriate for the daemon operation like SIGHUP, SIGTERM to reload config and terminate respectively.",
		"Properly handle daemon mode by detaching from the console.",
		"Support being started in interactive mode, i.e. not going into daemon mode.",
	},
	requires{
		"getopt",
		"log",
	},
	work{
		task{ hours=4, confidence=0.7, "Daemon commandline options, remove signer specific code." },
		task{ hours=8, confidence=0.6, "Daemon, remove signer specific code from the daemon code." },
	},
}

target{
	name="cmdhandler",
	kind="lib",
	language="C",
	origin="signer",
	description[[
		Command handler listening for commands coming in from an endpoint it has setup.
		A client program can connect to the endpoint and send commands to the command handler.
	]],
	responsibility{
		name="Command connection endpoint",
		"Setup an endpoint for a client to connect to.",
		"Listen on the endpoint for incoming connections.",
		"Cleanup endpoint on termination.",
		"Accept connections on the endpoint.",
		"Kill other active connections when a new connection is established.",
	},
	responsibility{
		name="Command processing",
		"Wait for a command to be send and wait for the 'end of command' indicator before actually starting processing",
		"If another command is currently busy, respond with 'command already active error' but keep the connection open to allow the active command to output results",
		"Whenever a running command reports a (partial) respons, send it straight back to the client via the endpoint",
	},
	responsibility{
		name="Command handling",
		"Process a command until it is complete",
		"When a command is generating a stream of output data, stream that directly back to the user.",
		"After a command is finished send the 'end of response' indicator."
	},
	responsibility{
		name="Enforcer specific command handling",
		"Setup usage text and command options for the cmdhandler to return to the client",
		"Handle actual commands send to the enforcer",
		"Command handling only can control the workflow by modifying the persistent state of zones, keystates, policies etc."
	},
	requires{
		"log",
	},
	work{
		task{ hours=8, confidence=0.8, "Remove signer specific code and create a generic cmdhandler." },
		task{ hours=2, confidence=0.7, "Command connection endpoint, adapt to enforcer requirements." },
		task{ hours=8, confidence=0.7, "Command processing." },
		task{ hours=8, confidence=0.8, "Command handling, allow the command handler to be hookable with callbacks for hooking up actual commands." },
		task{ hours=8, confidence=0.5, "Enforcer specific command handling." },
	},
}

target{
	name="workflowhandler",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		During normal operation the workflowhandler will actually be monitoring workflows that 
		are currently running. The idea is to have no persistent state for the workflow itself,
		but that it will look at policies, zones, keystates and goals and decides what to do based 
		on that information. The cmdhandler can only influence the workflowhandler by changing 
		persistent data and triggering the workflowhandler.
	]],
	responsibility{
		name="Handle cmdhandler trigger",
		"Wakeup normally and decide task to perform.",
	},
	responsibility{
		name="Decide task to perform",
		"Based on the persisten information at hand decide the task to perform.",
		"Select the most pressing task first when there are more that 1 task to choose from.",
	},
	responsibility{
		name="Perform tasks",
		"Perform a task deduced from the current persistent state of the system.",
		"This can be something like generating a key, introducing new keys for a zone etc.",
	},
	responsibility{
		name="Deduce next wakeup",
		"Based on the analysis of all current workflows, decide when to wakeup to perform another task.",
		"Don't persiste the wakeup, on a restart of the enforcer the deduction will generate a new wakeup time."
	},
	requires{
		"hsmpersistence",
		"policypersistence",
		"zonepersistence",
		"keystatepersistence",
		"audittrailpersistence",
		"keystateenforcer",
		"log",
	},
	work{
		task{ hours=4, confidence=0.7, "Handle cmdhandler trigger" },
		task{ hours=16, confidence=0.4, "Decide task to perform" },
		task{ hours=16, confidence=0.4, "Perform tasks" },
		task{ hours=8, confidence=0.7, "Deduce next wakeup" },
	},
}

target{
	name="config",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Contains class declarations that represent the contents of the conf.xml global configuration file.
	]],
	responsibility{
		name="Data Declaration",
		"For every distinguishable separate element of data from the global configuration file, introduce a class declaration.",
	},
	requires{
		"log"
	},
	work{
		task{ hours=8, confidence=0.9, "Create data declarations." }
	}
}

target{
	name="configreader",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Reads the conf.xml file into config classes.
	]],
	responsibility{
		name="Read configuration",
		"Use libxml2 to read the conf.xml file."
	},
	requires{
		"config",
		"log",
		"libxml2",
	},
	work{
		task{ hours=12, confidence=0.7, "Read the configuration from the conf.xml file into the config classes." }
	},
}

target{
	name="hsmpersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Update the database with a HSM entry read from the config file.
	]],
	responsibility{
		name="Store HSM entry",
		"Store a HSM entry in the database.",
	},
	responsibility{
		name="Delete HSM entry",
		"Delete a HSM entry from the database.",
	},
	requires{
		"config",
		"datapersistence",
		"log",
	},
	work{
		task{ hours=12, confidence=0.7, "Write a HSM entry into the database via datapersistence." }
	},
}

target{
	name="confighandler",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Uses the config reader to read in the configuration and then uses 
		hsm persistence to update the list of active hsms in the database.
	]],
	responsibility{
		name="Config Processing",
		"Load configuration at startup.",
		"Reload configuration when triggered by a command.",
		"Reload config when triggered by a SIGHUP signal.",
		"Read configuration file using configreader.",
		"Interpret the configuration file contents and update the database accordingly.",
		"Verify sanity of configuration before actually applying the results.",
	},
	responsibility{
		name="HSM List Update",
		"Update the list of hsm in the database with the contents of the configuration.",
		"Removes hsm records from the database that are no longer present in the configuration.",
	},
	requires{
		"configreader",
		"hsmpersistence",
		"config",
		"log",
	},
	work{
		task{ hours=16, confidence=0.7, "Implement config processing." },
		task{ hours=8, confidence=0.6, "Implement HSM list update." }
	},
}

target{
	name="policy",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Classes for the policy information used by the enforcer.
	]],
	responsibility{
		name="Declare Policy Classes",
		"Declare the classes to hold policy information.",
	},
	requires{
		"policyreader",
		"policypersistence",
		"log",
	},
	work{
		task{ hours=8, confidence=0.9, "Create data declarations." }
	},
}

target{
	name="policyreader",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Allows the policy to be read from the kasp.xml file.
	]],
	responsibility{
		name="Policy Loading",
		"Read the policy from the kasp.xml file into the policy classes.",
	},
	requires{
		"log",
		"libxml2",
	},
	work{
		task{ hours=12, confidence=0.7, "Read policy objects from the kasp.xml file." }
	},
}

target{
	name="policypersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Allows the policy information to be persisted to the database.
		Takes care of turning policy objects into persistent data and vice versa.
	]],
	responsibility{
		name="Persist Policies",
		"Maps an object to (1 or more) records in the database.",
		"Handles reading and writing of the policy.",
	},
	requires{
		"datapersistence",
		"log",
	},
	work{
		task{ hours=12, confidence=0.7, "Implement code to write a policy into the database." }
	},
}

target{
	name="policyhandler",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Implements commands to import policies from the kasp.xml configuration file into the system.
	]],
	responsibility{
		name="Kasp Import",
		"Use the policyreader to read policies from kasp.xml and then persist them into the database via policypersistence.",
		"Verify that the imported policy is sane.",
	},
	requires{
		"policy",
		"policyreader",
		"policypersistence",
		"log"
	},
	work{
		task{ hours=16, confidence=0.8, "Perform a key and signing policy import" }
	}
}

target{
	name="zone",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Contains class declarations that represents the zone data that is used by the enforcer.
	]],
	responsibility{
		name="Data Declaration",
		"For every distinguishable separate element of data from the global configuration file, introduce a class declaration.",
	},
	requires{
		"log"
	},
	work{
		task{ hours=8, confidence=0.9, "Create data declarations." }
	}
}

target{
	name="zonereader",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Reads the zonelist.xml.
	]],
	responsibility{
		name="Zonelist Loading",
		"Loads zone objects from the zonelist.xml file.",
	},
	requires{
		"log",
		"libmxml2",
	},
	work{
		task{ hours=12, confidence=0.7, "Read zone objects from the zonelist.xml file." }
	},
}

target{
	name="zonepersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Allows the zone information to be persisted to the database.
		Takes care of turning zone objects into persistent data and vice versa.
	]],
	responsibility{
		name="Persist Zone",
		"Maps an object to (1 or more) records in the database.",
	},
	requires{
		"datapersistence",
		"log",
	},
	work{
		task{ hours=12, confidence=0.7, "Implement code to write a zone into the database." }
	},
}

target{
	name="zonehandler",
	kind="lib",
	language="C++",
	origin="new",
	description=[[
		De zonehandler will perform any commands related to modifying the zonelist and persisten zone data.
	]],
	responsibility{
		name="Zone Import",
		"Read zone from zonelist.xml file using zonereader.",
		"Persist zones in the database using zonepersistence.",
	},
	responsibility{
		name="Zone Export",
		"Write all zones to a zonelist.xml file"
	},
	requires{
		"zonereader",
		"zonepersistence",
		"zone",
		"log"
	},
	work{
		task{ hours=8, confidence=0.6, "Implement zone import" },
		task{ hours=8, confidence=0.6, "Implement zone export" },
	}
}

target{
	name="keystate",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Classes for the keystate information used by the keystateenforcer.
	]],
	responsibility{
		name="Declare keystate classes",
		"Declare the classes for holding keystate information.",
	},
	requires{
		"keystatepersistence",
		"log",
	},
	work{
		task{ hours=8, confidence=0.9, "Create data declarations." }
	},
}

target{
	name="keystatepersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Allows the key state in the key state enforcer to be persisted to the database.
		Takes care of turning key state objects into persistent data and vice versa.
	]],
	responsibility{
		name="persists keystate",
		"maps an object to (1 or more) records in the database",
	},
	requires{
		"log",
	},
	work{
		task{ hours=12, confidence=0.7, "Implement code to write a keystate into the database." }
	},
}

target{
	name="keystateenforcer",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		State machine that is configured with keys for zones and the goals for the keys based
		on the policies configured for a specific zone. The machine is then provided with the current 
		time and events that may have occured and then asked to determine the transitions that are allowed
		and the actions that need to be taken by the enforcer.
	]],
	responsibility{
		name="Transition Rules",
		"A mathematical formalization of the states the DNSKEY, RRSIG, and DS records associated with a key can go through.",
		"The key state enforcer can tell the higher level layers which state transitions are currently allowed for the DNSKEY,RRSIG and DS records associated with a key.",
	},
	requires{
		"keystate",
		"log",
	},
	work{
		task{ hours=16, confidence=0.7, "Update to current state of the key state document." },
		task{ hours=8, confidence=0.6, "Add functionality to allow a key state to persist itself." } ,
	},
}

target{
	name="keygenerator",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Generate keypairs using the HSM.
	]],
	responsibility{
		name="Generate Keys",
		"Generate a keypair."
	},
	requires{
		"keystate",
		"log",
		"libhsm",
	},
	work{
		task{ hours=12, confidence=0.81, "Generate a key in the HSM and create keystate information based on it." }
	},
}

target{
	name="signerconfig",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Specification of data needed in order to be able to write a singer configuration to an XML file.
	]],
	responsibility{
		name="Signer Configuration Declaration",
		"Declare classes associated with signer configuration."
	},
	requires{
		"log",
	},
	work{
		task{ hours=8, confidence=0.7, "Signer configuration declaration." },
	},
}

target{
	name="signerconfigwriter",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		"Writes signer configuration (one per zone) to the signerconf directory."
	]],
	responsibility{
		name="gather signer information",
		"access the data objects to collect information for the signer configuration",
	},
	responsibility{
		name="Write configuration",
		"Write the configuration to a file in the singerconf directory",
		"Use a name that is deterministically derived from the zone name",
	},
	requires{
		"signerconfig",
		"log",
	},
	work{
		task{ hours=16, confidence=0.7, "Write a signer configuration to the database." }
	},
}

target{
	name="audittrail",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Specification of data needed in order to be able to reconstruct the actions the enforcer has performed.
	]],
	responsibility{
		name="Audit Trail Declaration",
		"Declare classes associated with audit trail information"
	},
	requires{
		"log",
	},
	work{
		task{ hours=16, confidence=0.7, "Audit trail declaration." },
	},
}

target{
	name="audittrailpersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Store an audit trail entry into the database.
	]],
	responsibility{
		name="Persist Audit Trail",
		"Write audit trail entries into the database.",
		"Audit trails always include de-normalized data so they can be evaluated separately from the database.",
		"Don't rely on the number of record in the database, data archiving can empty the audit trail tables.",
	},
	requires{
		"datapersistence",
		"log",
	},
	work{
		task{ hours=8, confidence=0.6, "Persist audit trail."},
	},
}

target{
	name="audittraillogger",
	kind="lib",
	language="C++",
	origin="new",
	description=[[
		Collect information about events and operations that need to be logged in the audit trail.
	]],
	responsibility{
		name="Persist Audit Trail Entries",
		"Based on the information passed to this lib, contruct appropriate audit trail entries and use audittrailpersistence to log them in a database."
	},
	requires{
		"audittrailpersistence",
		"audittrail",
		"log"
	},
	work{
		task{ hours=24, confidence=0.6, "Implement functions for logging specific audit trail entries" },
	}
}

target{
	name="datapersistence",
	kind="lib",
	language="C++",
	origin="new",
	description[[
		Data access library for the enforcer persistent data.
		Classes representing the data access that is needed for storing, querying and retrieving the enforcer data from the database.
	]],
	responsibility{
		name="Data Abstraction",
		"All SQL queries are done inside the datamodel classes",
		"make sure universally unique identification of objects is handled correctly",
	},
	responsibility{
		name="dynamic loading",
		"Refactor data persistence into an interface with 2 implementations for MySQL and SQLite",
		"Turn the separate implementations into dynamically loaded libaries.",
		"Dynamically load either MySQL or SQLite driver depending on the database configured.",
	},
	requires{
		"MySQL",
		"SQLite",
		"log",
	},
	work{
		task{ hours=60, confidence=0.6, "Implement data abstraction" },
		task{ hours=20, confidence=0.8, "implement dynamic loading" },
	},
}

target{
	name="datadefinition",
	kind="datadef",
	language="SQL",
	origin="enforcer",
	description[[
		Definition of all the tables and fields in the enforcer database.
	]],
	responsibility{
		name="table creation",
		"create the tables for the enforcer and fill it with initial data"
	},
	requires{
	},
	work{
		task{ hours=80, confidence=0.5, "Update table creation definitions to reflect the changes needed for storing new data associated with keystates" },
		task{ hours=20, confidence=0.7, "update table creation definitions to remove the meta-data tables from the database" },
	},
}

target{
	name="privdrop",
	kind="lib",
	language="C",
	origin="signer",
	description[[
		Allows a program to drop root privileges and run as a less privileged user or group.
	]],
	responsibility{
		name="Drop Privileges",
		"Drop privileges to a less privileged user or group"
	},
	requires{
		"log"
	},
	work{
	},
}	

target{
	name="log",
	kind="lib",
	language="C",
	origin="signer",
	description[[
		Wrapper around syslog that allows differentiated logging of errors, warnings and information.
	]],
	responsibility{
		name="Wrap syslog",
		"Just wrap syslog with a simple library of reusable logging code."
	},
	requires{
		"syslog",
	},
	work{
	},
}

--[[ --------------------------------
DEPENDENCIES
--]] --------------------------------

dependency{
	name="libhsm",
	kind="lib",
	language="C",
	description[[
		Wrapper around multiple PKCS#11 modules to manage keys and 
		to perform cryptographic operations with the keys.
	]],
	requires{
		"ldns",
		"pkcs11",
		"libxml2",
	}
}

dependency{
	name="ldns",
	kind="lib",
	description[[
		Library for working with DNS resource records and for talking to servers.
	]],
	requires{
		"OpenSSL",
	},
}

dependency{
	name="pkcs11",
	kind="lib",
	description[[
		Cryptographic token interface library.
		Most vendors implement a dynamic library that conform to this interface to talk to the HSM.
	]],
}

dependency{
	name="MySQL",
	kind="lib",
	description[[
		Database access library for MySQL database.
	]],	
}

dependency{
	name="SQLite",
	kind="lib",
	description[[
		Database access library for SQLite database.
	]],	
}

dependency{
	name="OpenSSL",
	kind="lib",
	description[[
		Library of cryptographic functions and mechanisms.
	]],
}

dependency{
	name="libxml2",
	kind="lib",
	description[[
		The XML C parser and toolkit developed for the Gnome project.
	]],
}

dependency{
	name="getopt",
	kind="lib",
	language="C",
	description[[
		A library that allows a program to specify the commandline options it supports and 
		a way to analyze the commandline for options specified.
	]],
}

dependency{
	name="syslog",
	kind="lib",
	language="C",
	description[[
		"System logging facility"
	]],
}

--[[ --------------------------------
MISCELLANEOUS
--]] --------------------------------
--[[
"coding features",
"coding tests",
"revision control",
"testing",
"bug fixing",
"meetings",
"design security permissions on files",
"keep design document up to date,flesh out the design with text and diagrams.",
--]]
