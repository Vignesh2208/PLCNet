
##Experiment Configuration. 
```
Do not remove the markers: START_OF_EXPERIMENT_CONFIGURATION and  ND_OF_EXPERIMENT_CONFIGURATION


    Description:
    ------------
    

    Experiment_Name     (Type: String)  #Name of the experiment. A folder with this name gets created with
                                        #all the log files inside experiment-data directory

    Run_Time            (Type: Int)     #Run time of experiment in seconds

    Dilation_Factor     (Type: Int)     #Dilation factor (TDF) of all PLCs in the experiment. Must be > 0

    Number_Of_Nodes     (Type: Int)     #Number of nodes in the experiment

    ModBus_Network_Type (Type: String)  #Denotes the type of network used. Can be IP or SERIAL. (MODBUS 
                                        #over IP or RS-232 respectively)
	
```


##General Configuration. 
```
Do not remove the markers: START_OF_GENERAL_CONFIGURATION and  ND_OF_GENERAL_CONFIGURATION


    Description:
    -----------

	NODE_ID			    (Type: Int)	 #ID of emulated node. 

	CONNECTION_ID 		(Type: Int)	 #Connection ID which defines a specific connection. A node can have 
                                     #any connections simultaneously

	REMOTE_PORT 		(Type: Int)	 #Port to connect to on the remote host. Valid for only for a 
                                     #connection 	where the node behaves as a client

	LOCAL_PORT	  	    (Type: Int)	 #Port to bind to on the host and listen for connections. Valid for 
                                     #only a connection where the node behaves as a server

	REMOTE_PARTNER_ID 	(Type: Int)	 #NODE_ID of partner to connect to. Valid only for a connection where 
                                     #the node behaves as a client

	IS_SERVER		    (Type: Bool) #Indicates if the node should behave as a server for the given 
                                     #connection ID

```

##Awl Script configuration.
```
Do not remove the markers: START_OF_AWL_SCRIPT_CONFIGURATION and END_OF_AWL_SCRIPT_CONFIGURATION


    Description:
    ------------

	NODE_ID                (Type: Int)	    #ID of Emulated Node. Must be between 1 and Number_Of_Nodes.

	AWL_SCRIPT_PATH        (Type: String)	#Path of Awl script to run on the emulated node


```
	Data_Area configuration. 
    Do not remove the markers: START_OF_DATA_AREA_CONFIGURATION and END_OF_DATA_AREA_CONFIGURATION
----------------------------------------------------------------------------------------------------------

    Description:
    ------------

	NODE_ID                (Type: Int)	    ID of Emulated Node.

	DATA_AREA_i(DAi)       (Type: Tuple)	(Data Type:Data Block number:Start Address:End Address).
                                            see SIMATIC S-7 OpenModbus/TCP communication Manual for more information.
						
    eg : (1:2:5:100) => Data_Type = 1 (Coil), Storage DataBlock Number = 2, Start Coil Address = 5, End Coil Address = 100

----------------------------------------------------------------------------------------------------------