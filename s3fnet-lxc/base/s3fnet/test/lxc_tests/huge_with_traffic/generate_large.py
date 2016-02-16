#!/usr/bin/python

# This script generates a DML file containing N LXCs aligned on M timeilnes. Each timeline is aligned on a single net
# the timelines are connected via a ring of routers. It is possible to tweak how many hosts are in a network
# as well as the simulation time, if there is traffic or not. Note, the command generating traffic can be modified
# see code below
# Note - currently works for an even number of timelines and an even number of hosts per timeline


# Import the os module, for the os.walk function
import os
import re
import sys

numHostsInNet  = 4
numTimelines   = 4
simulationTime = 0.1
logDIR         = "Sample_Out"
traffic        = 1
TDF            = 300

dmlFile      = open("test.dml", 'w')	
netWorksFile = open("custom_net.dml", 'w')

netWorksFile.write( "custom_net \n[\n")
netWorksFile.write( "\temuNet_%s\n" % numHostsInNet)
netWorksFile.write( "\t[\n")

# First Router
netWorksFile.write( "\t\tnet\n" )
netWorksFile.write( "\t\t[\n" )
netWorksFile.write( "\t\t\trouter\n" )
netWorksFile.write( "\t\t\t[\n" )
netWorksFile.write( "\t\t\t\tid %s\n" % (numHostsInNet + 1) )  
netWorksFile.write( "\t\t\t\t_find .dict.routerGraph.graph\n" )      
netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ] # Not Attached\n" % 0  )   

for x in range(1, numHostsInNet/2 + 1):
    netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ]\n" % x )  

netWorksFile.write( "\t\t\t]\n" )

# Second Router
netWorksFile.write( "\t\t\trouter\n" )
netWorksFile.write( "\t\t\t[\n" )
netWorksFile.write( "\t\t\t\tid %s\n" % (numHostsInNet + 2) )  
netWorksFile.write( "\t\t\t\t_find .dict.routerGraph.graph\n" )      
netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ] # Not Attached\n" % 0  )   

for x in range(numHostsInNet/2+1, numHostsInNet + 1):
    netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ]\n" % x )  

# Routers Finished       
        
netWorksFile.write( "\t\t\t]\n" )

netWorksFile.write( "\t\t\thost\n" )
netWorksFile.write( "\t\t\t[\n" )
netWorksFile.write( "\t\t\t\tidrange [from 1 to %s ]\n" % numHostsInNet )     
netWorksFile.write( "\t\t\t\t_extends .dict.emuHost\n" )
netWorksFile.write( "\t\t\t]\n" )

for x in range(1, numHostsInNet/2 + 1):
	netWorksFile.write( "\t\t\tlink [ attach %s(%s)  attach %s(0)  _extends .dict.link_delay_50us  ]\n " % (numHostsInNet + 1,x, x)  )

for x in range(numHostsInNet/2+1, numHostsInNet + 1):
	netWorksFile.write( "\t\t\tlink [ attach %s(%s)  attach %s(0)  _extends .dict.link_delay_50us  ]\n " % (numHostsInNet + 2,x, x)  )


netWorksFile.write( "\t\t]\n")
netWorksFile.write( "\t]\n")
netWorksFile.write( "]\n" )

# =========================================================================================================
# =========================================================================================================
# =========================================================================================================
# =========================================================================================================

dmlFile.write( "total_timeline %s\n"  % (numTimelines + 1))
dmlFile.write( "tick_per_second 6\n" )
dmlFile.write( "run_time %s\n" % simulationTime )
dmlFile.write( "seed 1\n" )
dmlFile.write( "log_dir %s\n\n" % logDIR )
dmlFile.write( "Net\n" )
dmlFile.write( "[\n" )

# LXC settings

if (traffic == 1):
	dmlFile.write( "\tlxcConfig\n\t[\n" )
	for tl in range(0, numTimelines,2):
		for host in range(1, numHostsInNet + 1):
			destinationTL = (tl + 1) % numTimelines 
			dmlFile.write( "\t\t settings [ lxcNHI %s:%s TDF %s cmd \"ls\" ] \n" % (tl , host, TDF));
			dmlFile.write( "\t\t settings [ lxcNHI %s:%s TDF %s cmd \"ping %s:%s -c 2\" ] \n" % (tl+1 , host, TDF, tl, host));
			# dmlFile.write( "\t\t settings [ lxcNHI %s:%s TDF %s cmd \"~/csudp/client %s:%s 25000 2\" ] \n" % (tl+1 , host, TDF, tl, host));

	dmlFile.write( "\t]\n")
else:
	dmlFile.write( "\tlxcConfig\n\t[\n" )
	for tl in range(0, numTimelines,2):
		for host in range(1, numHostsInNet + 1):
			destinationTL = (tl + 1) % numTimelines 
			dmlFile.write( "\t\t settings [ lxcNHI %s:%s TDF %s cmd \"ls\" ] \n" % (tl , host, TDF));
			dmlFile.write( "\t\t settings [ lxcNHI %s:%s TDF %s cmd \"ls\" ] \n" % (tl+1 , host, TDF));
	dmlFile.write( "\t]\n")


for x in range(0, numTimelines):
	dmlFile.write( "\tNet [id %s alignment %s _extends .custom_net.emuNet_%s.Net]\n" % (x,x,numHostsInNet) )
	
netContainingRouterRing = 60	

dmlFile.write( "\n\tNet\n" )
dmlFile.write( "\t[\n" )
dmlFile.write( "\t\tid %s alignment %s\n" % (netContainingRouterRing, numTimelines))

for x in range(0, numTimelines):
	dmlFile.write( "\t\trouter\n" )
	dmlFile.write( "\t\t[\n" )
	dmlFile.write( "\t\t\tid %s\n" % (x + 40) )
	dmlFile.write( "\t\t\t_find .dict.routerGraph.graph\n" )  

	dmlFile.write( "\t\t\tinterface [ id 0 _extends .dict.1Gb ]\n") 
	dmlFile.write( "\t\t\tinterface [ id 1 _extends .dict.1Gb ]\n")  
	dmlFile.write( "\t\t\tinterface [ id 2 _extends .dict.1Gb ]\n")  
	dmlFile.write( "\t\t\tinterface [ id 3 _extends .dict.1Gb ]\n")   
	dmlFile.write( "\t\t]\n" )

dmlFile.write( "\t]\n"  )

counter = 0;

routerIDStart = 40

for x in range(0 + routerIDStart, numTimelines + routerIDStart):
	
	startID = (x - routerIDStart)       % numTimelines + routerIDStart;
	endID   = (x - routerIDStart + 1) % numTimelines + routerIDStart; 
	
	# print(str(startID) + "-" + str(endID)) 
	
	start = counter % 2
	end = (counter + 1) % 2
	# print x
	
	dmlFile.write( "\tlink [ attach 60:%s(%s) attach 60:%s(%s)  _extends .dict.link_delay_50us  ]\n" % (startID,start, endID, end)  )

dmlFile.write( "\n" )

	
for x in range(0, numTimelines):
	routerID = x + 40
	dmlFile.write( "\tlink [ attach %s:%s(0) attach 60:%s(2)  _extends .dict.link_delay_50us  ]\n" % (x, numHostsInNet + 1, routerID)  )
	dmlFile.write( "\tlink [ attach %s:%s(0) attach 60:%s(3)  min_delay 1e-6 prop_delay 0.001  ]\n"  % (x, numHostsInNet + 2, routerID)  )
	
dmlFile.write( "]\n" )



























	
