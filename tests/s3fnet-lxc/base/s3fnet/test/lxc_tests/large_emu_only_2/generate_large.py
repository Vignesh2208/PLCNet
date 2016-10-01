#!/usr/bin/python

# Import the os module, for the os.walk function
import os
import re
import sys

numHostsInNet  = 2
numTimelines   = 4
simulationTime = 0.1
logDIR         = "large_2000"

dmlFile      = open("test.dml", 'w')	
netWorksFile = open("custom_net.dml", 'w')

netWorksFile.write( "custom_net \n[\n")
netWorksFile.write( "\temuNet_%s\n" % numHostsInNet)
netWorksFile.write( "\t[\n")

netWorksFile.write( "\t\tnet\n" )
netWorksFile.write( "\t\t[\n" )
netWorksFile.write( "\t\t\trouter\n" )
netWorksFile.write( "\t\t\t[\n" )
netWorksFile.write( "\t\t\t\tid 0\n" )  
netWorksFile.write( "\t\t\t\t_find .dict.routerGraph.graph\n" )      
netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ]\n" % 0  )   
   
for x in range(1, numHostsInNet + 1):
    netWorksFile.write( "\t\t\t\tinterface [ id %-3s _extends .dict.1Gb ]\n" % x )        
        
netWorksFile.write( "\t\t\t]\n" )

netWorksFile.write( "\t\t\thost\n" )
netWorksFile.write( "\t\t\t[\n" )
netWorksFile.write( "\t\t\t\tidrange [from 1 to %s ]\n" % numHostsInNet )     
netWorksFile.write( "\t\t\t\t_extends .dict.emuHost\n" )



netWorksFile.write( "\t\t\t]\n" )

for x in range(1, numHostsInNet + 1):
	netWorksFile.write( "\t\t\tlink [ attach 0(%s)  attach %s(0)  _extends .dict.link_delay_50us  ]\n " % (x, x)  )


netWorksFile.write( "\t\t]\n")

netWorksFile.write( "\t]\n")

netWorksFile.write( "]\n" )

# =========================================================================================================

dmlFile.write( "total_timeline %s\n"  % numTimelines)
dmlFile.write( "tick_per_second 6\n" )
dmlFile.write( "run_time %s\n" % simulationTime )
dmlFile.write( "seed 1\n" )
dmlFile.write( "log_dir %s\n\n" % logDIR )

dmlFile.write( "dilation [ TDF 30.0 ]\n\n" )
dmlFile.write( "Net\n" )
dmlFile.write( "[\n" )

for x in range(0, numTimelines):
	dmlFile.write( "\tNet [id %s alignment %s _extends .custom_net.emuNet_%s.Net]\n" % (x,x,numHostsInNet) )
	

dmlFile.write( "\n\tNet\n" )
dmlFile.write( "\t[\n" )
dmlFile.write( "\t\tid 20 alignment 0\n" )
dmlFile.write( "\t\trouter\n" )
dmlFile.write( "\t\t[\n" )
dmlFile.write( "\t\t\tid 0\n" )
dmlFile.write( "\t\t\t_find .dict.routerGraph.graph\n" )  

for x in range(0, numTimelines):
	dmlFile.write( "\t\t\tinterface [ id %-3s _extends .dict.1Gb ]\n" % x )  
	
dmlFile.write( "\t\t]\n" )
dmlFile.write( "\t]\n"  )

for x in range(0, numTimelines):
	dmlFile.write( "\tlink [ attach 20:0(%s) attach %s:0(0)  _extends .dict.link_delay_1ms  ]\n" % (x,x)  )
	
dmlFile.write( "]\n" )	
