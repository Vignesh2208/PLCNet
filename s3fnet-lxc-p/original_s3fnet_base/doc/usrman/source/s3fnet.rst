S3FNet
--------------------------------

We implement S3F (Simpler Scalable Simulation Framework) in C++, and S3FNet is a network simulator built on top of S3F. This tutorial is for users who are going to develop their own protocol models in S3FNet. We also recommend users to read the tutorial at `SSFNet.org <http://www.ssfnet.org>`_. That tutorial is for a Java SSF-based network simulator, but S3FNet has the same flavor of network configuration and model construction scheme.

Core Components 
=================================
S3FNet tries to only leak minimum of simulation details and present users basic units to build a network. The basic S3FNet components are illustrated in the Figure :ref:`s3fnetcomp`, and they should be conceptually familiar to most network researchers.  

* Net: This class represents a network. It is composed of some smaller nets (subnets), hosts/routers, and links.
* Host: Both hosts and routers are represented by this class. Each host is a ProtocolGraph with one or more NetworkInterfaces.
* ProtocolGraph: Each protocol graph is composed of several ProtocolSessions such as IP, UDP, TCP, etc.
* NetworkInterface: This is the representation of a network interface card. It contains a MAC layer and a physical layer protocol sessions.
* ProtocolSession: Each protocol session is a model of one particular protocol, e.g. TCP, IP etc.
* Link: This represents a physical link that connects multiple network interface cards.
* ProtocolMessage: Each protocol implements its own protocol header. It should be an extension of this base class.
* Packet: This is the smallest data unit that is sent over a link. Usually it is composed by several ProtocolMessages, just the same as in the real life.

.. _s3fnetcomp:

.. figure::  images/s3fnet-components.png
   :width: 40 %
   :align:   center

   S3FNet Basic Components

.. _s3fnet-dev:
   
Develop your own protocol/application
=======================================

The best way to learn to develop protocol models in S3FNet is to read our development manual (doxygen documentation) for existing protocols (in s3fnet/src/os). However, there are principles and rules that can speed up this process. To facilitate the explanation, we use DUMMY protocol as an example to show how to develop a new protocol model. The DUMMY protocol periodically sends hello message to a specified destination.

Usually implementing a protocol session involves (but is not limited to) extending **ProtocolSession** and **ProtocolMessage** classes. Also, users may need to change the makefile.common for new protocols and turn on/off debug messages.

ProtocolSession APIs 
****************************

Each protocol/application model in S3FNet should be a public extension of the class ProtocolSession. There are a set of APIs that a protocol session has to implement, and below we will briefly describe the functionality of each API. 

1. Before APIs

A protocol needs to define its protocol type number, which is defined in s3fnet/src/s3fnet.h. There is an enumeration S3FNetProtocolType defined in that file. Add an entry for the new protocol. For DUMMY, S3FNET_PROTOCOL_TYPE_DUMMY = 254. Please also note that many other global macros and enumerations are also defined in this file to facilitate management of the code.

2. config

This is the function that the model reads in the parameters from the configuration of this protocol. The default attributes are
	+ *name*  The name of the protocol session.
	+ *use*   The class to use. It should be the second parameter of REGISTER_PROTOCOL macro.

The user can add any other attributes. For the DUMMY protocol, we add some attributes including message content, sending interval, destination host address, and jitter. A portion of the config() code is shown below::
 
 void DummySession::config(s3f::dml::Configuration* cfg)
 {
  // IMPORTANT, MUST BE CALLED FIRST!
  ProtocolSession::config(cfg);

  // parameterize your protocol session from DML configuration
  char* str = (char*)cfg->findSingle("hello_message");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: DummySession::config(), invalid HELLO_MESSAGE attribute.");
    else hello_message = str;
  }
  else hello_message = "hello world";

  double hello_interval;
  str = (char*)cfg->findSingle("hello_interval");
  if(!str) hello_interval = 0;
  else if(s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: DummySession::config(), invalid HELLO_INTERVAL attribute.");
  else 
    hello_interval = atof(str);
  if(hello_interval < 0)
    error_quit("ERROR: DummySession::config(), HELLO_INTERVAL needs to be non-negative.");

  printf("A dummy session is configured in host of NHI=\"%s\":\n", inHost()->nhi.toString());
  printf("=> hello message: \"%s\".\n", hello_message.c_str());
  printf("=> hello interval: %f.\n", hello_interval);

  ...
 }

one segment of DML that contains DUMMY may look like::

	graph [
		ProtocolSession [ name dummy use "s3f.os.dummy"
				  hello_interval 10 hello_peer_nhi 1:0(0)
				  hello_message "hello from host 0:0" jitter 0 ]
		ProtocolSession [ name ip use "s3f.os.ip" ]
	]

3. init

The init() is the initialization part of a protocol session. The init() is executed after config() (i.e. after obtaining all inputs from DML files). You can refer to :ref:`sequence` for more information on the sequence of function calls.  init() is the first location in a protocol session that can interact with other protocol sessions or network interfaces. In another word, it is NOT assured until the init phase that the other protocol sessions or interfaces have been properly configured. For example, our DUMMY  model needs to have a handle of IP. If one tries to ask for such a handle in the config phase, it's possible that it will get a NULL pointer back. However, in the init phase, it is assured that it can get such a handle correctly given that it is configured correctly in the DML. A portion of the init() code is listed below.::
 
 void DummySession::init()
 {
  // IMPORTANT, MUST BE CALLED FIRST!
  ProtocolSession::init();

  // we couldn't resolve the IP layer until now
  ip_session = (IPSession*)inHost()->getNetworkLayerProtocol();
  if(!ip_session) error_quit("ERROR: can't find the IP layer; impossible!");
   
  // initialize the session-related variables here
  pkt_seq_num = 0;
  ltime_t wait_time = (ltime_t)getRandom()->Uniform(hello_interval-jitter, hello_interval+jitter);

  ...
 } 


4. getProtocolNumber

Each protocol session should has its own unique protocol number, this function returns that protocol number. All protocol numbers are defined in "s3fnet/src/s3fnet.h".::
 
 int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_DUMMY; }
 
5. push

The push() function is called when an upper layer in the protocol stack pushes down a message. The ownership of that message is also passed along. For example, when DUMMY needs to push a message to IP, it will call the push function of IP, and it is up to IP to decide whether to delete this message or pass it along. In the DUMMY protocol, it does not have any upper layer that push message to it, so it's not necessary to overload it. We only put it here to make sure that it won't be called accidentally.::
 
 int DummySession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
 {
   error_quit("ERROR: a message is pushed down to the dummy session from protocol layer above; it's impossible.\n");
   return 0;
 } 

6. pop
  
As a counterpart for push(), pop() is called when the lower protocol session has some messages for this protocol. For example, when IP receives a hello message, it will call the DUMMY::pop(). Similar to push(), the ownership of that message is also passed to DUMMY. If DUMMY has some other upper layer to pop this packet to, it will pass along the ownership of the whole message when it calls the pop of the upper layer. In this particular case, DUMMY will process the hello message itself. After that, DUMMY calls the "erase_all()" to delete the message.:: 

 int DummySession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
 {
   printf("A message is popped up to the dummy session from the IP layer.\n");
  
   char* pkt_type = "dummy";

   //check if it is a dummy packet 
   ProtocolMessage* message = (ProtocolMessage*)msg;
   if(message->type() != S3FNET_PROTOCOL_TYPE_DUMMY)
   {
      error_quit("ERROR: the message popup to dummy session is not S3FNET_PROTOCOL_TYPE_DUMMY.\n");
   }

   DummyMessage* dmsg = (DummyMessage*)msg;
   IPOptionToAbove* ipopt = (IPOptionToAbove*)extinfo;

   char buf1[32]; char buf2[32];
   printf("Dummy session receives hello: \"%s\" (ip_src=%s, ip_dest=%s)\n",
          dmsg->hello_message.c_str(), IPPrefix::ip2txt(ipopt->src_ip, buf1), 
          IPPrefix::ip2txt(ipopt->dst_ip, buf2));

   dmsg->erase_all();

   return 0;
 }

7. control

This function is used for different purposes by different protocol sessions. It is mainly used by protocols to exchange information with other protocol sessions. It can also be used to adjust parameter settings of this protocol. For example, inserting a new route in IP protocol, getting bitrate and buffer size in simple physical protocol. The control types are defined in enum ProtocolSessionCtrlTypes in s3fnet/src/s3fnet.h. For the DUMMY, there are two dummy control type implemented::

 int DummySession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
 {
   switch(ctrltyp)
   {
  	case DUMMY_CTRL_COMMAND1:
  	case DUMMY_CTRL_COMMAND2:
  	  return 0; // dummy control commands, we do nothing here
  	default:
  	  return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
   } 
 }

8. callback

callback() function defines what to do when an event scheduled by waitFor() fires, and the callback_body() defines the actual work.  A timer used in a protocol can be implemented in this way.:: 

  //create a callback process
  Process* callback_proc = new Process( (Entity *)this_host, (void (s3f::Entity::*)(s3f::Activation))&DummySession::callback);

  //schedule a hello message
  HandleCode h = this_host->waitFor( callback_proc, ac, wait_time, this_host->tie_breaking_seed);

  //when an event fires
  void DummySession::callback(Activation ac)
  {
    DummySession* ds = (DummySession*)((ProtocolCallbackActivation*)ac)->session;
    ds->callback_body(ac);
  }
  
  //actual work: create a hello message and push down to the lower layer (IP)
  void DummySession::callback_body(Activation ac)
  {
    DummyMessage* dmsg = new DummyMessage(hello_message); //create hello message
    Activation dmsg_ac (dmsg);

    IPPushOption ipopt; // preparing for calling IP's push()
    ipopt.dst_ip = hello_peer;
    ipopt.src_ip = IPADDR_INADDR_ANY;
    ipopt.prot_id = S3FNET_PROTOCOL_TYPE_DUMMY;
    ipopt.ttl = DEFAULT_IP_TIMETOLIVE;

    ip_session->pushdown(dmsg_ac, this, (void*)&ipopt, sizeof(IPPushOption)); // push the hello message to IP layer

    // schedule for the next hello message
    ltime_t wait_time = (ltime_t)getRandom()->Uniform(hello_interval - jitter, hello_interval + jitter);
    HandleCode h = inHost()->waitFor( callback_proc, ac, wait_time, inHost()->tie_breaking_seed );
  }

9. REGISTER_PROTOCOL

It is required for each protocol session to use this macro. Each protocol session needs a default constructor. It is needed by this macro.  Below is how we register the DUMMY protocol::

 in dummy_session.h
 #define DUMMY_PROTOCOL_CLASSNAME "S3F.OS.DUMMY"

 in dummy_session.cc
 S3FNET_REGISTER_PROTOCOL(DummySession, DUMMY_PROTOCOL_CLASSNAME);

ProtocolMessage APIs
***************************

Each protocol session will use its own message header. The class for the header should be an extended class of ProtocolMessage. Here we also use ICMP header as an example to explain how to develop a new protocol message header.

1. Default Constructor

Default constructor is required. It is used by the REGISTER_MASSAGE macro. 

2. Copy Constructor

Copy constructor is required for each extended class of ProtocolMessage. It is also required to call the copy constructor of ProtocolMessage in this copy constructor.::
  
  DummyMessage::DummyMessage(const DummyMessage& msg) :
  ProtocolMessage(msg), // the base class's copy constructor must be called
  hello_message(msg.hello_message) {} 

3. clone

This function is required for any extended class of ProtocolMessage, but usually they all follow the same pattern. The clone of DUMMY simply looks like::

  ProtocolMessage* clone() 
  { 
     printf("DummyMessage cloned\n"); return new DummyMessage(*this); 
  }

4. type

Each protocol message should also return a type number. This number should be the same as the number of the protocol that uses this protocol message.::

  int type() { return S3FNET_PROTOCOL_TYPE_DUMMY; }

5. packingSize

Returns the byte count (in simulation) only for this protocol header. In simulation, when the packet is sent cross timelines, it will be packed into a byte array using serialize function. The return value of packingSize() should return the size in bytes this header will be packed into. It is very important to ensure that the method in the base class be called.::

 int DummyMessage::packingSize()
 {
  // must add the parent class packing size
  int size = ProtocolMessage::packingSize();

  size += hello_message.length();
  return size;
 }

6. realByteCount

This function returns the size of the message in real networks. For example, a UDP header (without data) takes 8 bytes in real; in the simulation, it may take 20 bytes to implement the UDP header because of some meta data. This function should then return 8 bytes.

7. REGISTER_MESSAGE

Each extended class of the ProtocolMessage should use REGISTER_MESSAGE macro in the .cc file. In DUMMY, it looks like::

 S3FNET_REGISTER_MESSAGE(DummyMessage, S3FNET_PROTOCOL_TYPE_DUMMY);

Makefile Change and Debug Message
**********************************

Users will need to change the makefile.common located at the s3fnet directory for a new protocol. Let us take the DUMMY protocol as an example::

 # os/dummy: dummy protocol

 DUMMY_HDRFILES = \
 	$(SRCDIR)/os/dummy/dummy_session.h \
 	$(SRCDIR)/os/dummy/dummy_message.h
 DUMMY_SRCFILES = \
 	$(SRCDIR)/os/dummy/dummy_session.cc \
 	$(SRCDIR)/os/dummy/dummy_message.cc
 DUMMY_NONXFORM = $(filter %.cc,$(DUMMY_SRCFILES))
 DUMMY_XFORM = $(filter %.cxx,$(DUMMY_SRCFILES))
 DUMMY_OBJECTS = $(DUMMY_XFORM:.cxx=.xform.o) $(DUMMY_NONXFORM:.cc=.o)

 S3FNET_HDRFILES = \ ...
 	$(DUMMY_HDRFILES) \
 	...

 S3FNET_SRCFILES = \ ...
        $(DUMMY_SRCFILES) \
        ...
 
 S3FNET_OBJECTS = \ ...
        $(DUMMY_OBJECTS) \
        ...

Users may also want to implement the protocol debug messages in the following way::
 
 in dummy_session.cc
 #ifdef DUMMY_DEBUG
 #define DUMMY_DUMP(x) printf("DUMMY: "); x
 #else
 #define DUMMY_DUMP(x)
 #endif

 e.g., DUMMY_DUMP(printf("A dummy protocol session is created.\n"));

 in makefile.common
 ifeq ($(ENABLE_S3FNET_DEBUG), yes)
 S3FNET_DBGCFG = \...
 	-DDUMMY_DEBUG \
 	...
 else
 S3FNET_DBGCFG =
 endif

		
DML 
=================================
DML stands for Domain Modeling Language. Below are some useful DML tutorials:

* `Domain Modeling Language (DML) Reference Manual <http://www.ssfnet.org/SSFdocs/dmlReference.html>`_
* A brief DML tutorial from Jason Liu at Florida International University
	+ `What is DML? <http://users.cis.fiu.edu/~liux/research/projects/dassfnet/dmlintro/dml-explained.html>`_
	+ `Introduction to SSFNet DML <http://users.cis.fiu.edu/~liux/research/projects/dassfnet/dmlintro/ssfnet-dml-intro.html>`_
* `A detailed tutorial on how to use DML to configure network models <http://www.ssfnet.org/InternetDocs/ssfnetTutorial-1.html>`_

The DML in S3FNet is almost the	same as the ones used in SSFNet except a few new attributes/features. We will explain the difference through a simple example in which 2 hosts are directly connected through a wired link, and each host periodically sends hello message to the other.::
 
 total_timeline 2
 tick_per_second 6
 run_time 100
 seed 0

 Net [
        Net [ id 0 alignment 0  # every host/router in Net 0 is assigned to timeline 0 (i.e. logical process 0)
                host [ id 0
                        graph [
        	                ProtocolSession [ name dummy use "s3f.os.dummy"
                        			  hello_interval 10 hello_peer_nhi 1:0(0) hello_message "hello from host 0:0"
                	        ]
 	                        ProtocolSession [ name ip use "s3f.os.ip" ]
                        ]

                        interface [ id 0 _extends .dict.iface ]
                ]

        ]

        Net [ id 1 alignment 1 #  every host/router in Net 1 is assigned to timeline 1 (i.e. logical process 1)
                host [ id 0
                        graph [
                        	ProtocolSession [ name dummy use "s3f.os.dummy"
                        			  hello_interval 10 hello_peer_nhi 0:0(0) hello_message "hello from host 1:0"
                        ]
 	                        ProtocolSession [ name ip use "s3f.os.ip" ]
                        ]

                        interface [ id 0 _extends .dict.iface ]
                ]

        ]

        link [ attach 0:0(0) attach 1:0(0) prop_delay 1e-3 min_delay 0.2e-3 ]
        # min_delay is the minimum packet transfer delay, e.g., min packet size / bandwidth
        # prop_delay is the propagation delay
 	# note: prop_delay and min_delay contributes to the S3F::outchannel's mapping delay)
 ] 

 dict [ iface [  bitrate 1e6 latency 0 buffer 150000 ] ]
 # bitrate is the speed of the network interface card (NIC)
 # latency is the delay of the NIC (note: S3F::outChannel's per-write-delay)
 # buffer is the sending buffer of the NIC


Configureation parameters at the top of DML files:

* total_timeline
	total timeline (i.e. logical process) in the model.
* tick_per_second 
	tick is the simulation time unit. E.g., 6 means one second has 10^6 ticks (i.e. the time unit is microsecond). 0 means the time unit is second.
* run_time
	simulation run-time in second. Actually it is the run-time of one epoch, and the system run one epoch in default.
* seed
	seed for random number generator (a non-negative integer number). Simulation results are repeatable for the same seed.

More options are available in the S3F/S3FNet full version. Please refer to the test cases for details, for example::

	full-version/s3fnet/test/openflow/openflow_1switch_2TCPhost_2UDPhost_1timeline/test.dml

More complicated DML examples can be found at::

	base-version or full version/s3fnet/test

.. _sequence:

Running Procedure of S3FNet Experiments
===============================================

We use a simple test case, show in Figure :ref:`2dummy`,  to illustrate the order of procedures (configuration -> initialization -> experiment running -> output) for running a network experiment in S3FNet. It provides users a general idea on when and how different components (such as host, link, protocol, network interface) are configured and initialized. ::

 MAIN: parsing dml files.
 Input DML files:
  test-env.dml
  test-rt.dml
  test.dml

 /****************************************
    Configuration 
 *****************************************/
 NET: new topnet.
 NET: config_top_net().
 NAMESVC: new global name service
 NAMESVC: config().
 NET: config_host(), created host 0 on timeline 0
 HOST: [id=0] new host.
 HOST: [nhi="0"] config().
 HOST: [nhi="0"] register host.
 DUMMY: A dummy protocol session is created.
 IP: [host="0"] new ip session.
 IP: [host="0"] config().
 NW_IFACE: [id=0] new network interface.
 NET: register_interface(): ip="10.10.0.1".
 NW_IFACE: NIC = 0(0), IP = 10.10.0.1, MAC = 00:00:00:00:00:01
 SMAC: [nic="0(0)"] new simple_mac session.
 SMAC: [nic="0(0)"] config().
 SPHY: [nic="0(0)"] new simple_phy session.
 SPHY: [nic="0(0)"] config().
 NET: config_host(), created host 1 on timeline 0
 HOST: [id=1] new host.
 HOST: [nhi="1"] config().
 HOST: [nhi="1"] register host.
 DUMMY: A dummy protocol session is created.
 IP: [host="1"] new ip session.
 IP: [host="1"] config().
 NW_IFACE: [id=0] new network interface.
 NET: register_interface(): ip="10.10.0.2".
 NW_IFACE: NIC = 1(0), IP = 10.10.0.2, MAC = 00:00:00:00:00:02
 SMAC: [nic="1(0)"] new simple_mac session.
 SMAC: [nic="1(0)"] config().
 SPHY: [nic="1(0)"] new simple_phy session.
 SPHY: [nic="1(0)"] config().
 NET: config_link().
 LINK: new link.
 LINK: config().
 NET: finish_config_top_net().
 LINK: connect().
 HOST: [nhi="0"] load forwarding table.
 HOST: [nhi="1"] load forwarding table.
 
 /****************************************
    Initialization 
 *****************************************/
 MAIN: initializing the top net.
 NET: net "" init.
 LINK: init().
 HOST: [nhi="0"] init().
 IP: [host="0"] init().
 DUMMY: Dummy session is initialized.
 NW_IFACE: [nhi="0(0)", ip="10.10.0.1"] init().
 SPHY: [nic="0(0)", ip="10.10.0.1"] init().
 SMAC: [nic="0(0)", ip="10.10.0.1"] init().
 
 HOST: [nhi="1"] init().
 IP: [host="1"] init().
 DUMMY: Dummy session is initialized.
 NW_IFACE: [nhi="1(0)", ip="10.10.0.2"] init().
 SPHY: [nic="1(0)", ip="10.10.0.2"] init().
 SMAC: [nic="1(0)", ip="10.10.0.2"] init().


 /****************************************
    Running Experiments
 *****************************************/
 enter epoch window 1
  .. actual work here...
 completed epoch window, advanced time to 100000000
 Finished

 /****************************************
    Experiment Output
 *****************************************/
 -------------- runtime measurements ----------------
 Simulation run of 100 sim seconds, with 1 timelines
 	configured without smart pointer events
 	configured without smart pointer activations
 total run time is 0.00181 seconds
 simulation run time is 0.000762 seconds
 accumulated usr time is 0 seconds
 accumulated sys time is 0 seconds
 total evts 54, work evts 36, sync evts 0
 total exec evt rate 70866.1, work evt rate 47244.1
 ----------------------------------------------------

.. _2dummy:

.. figure::  images/2dummy.png
   :width: 30 %
   :align:   center
   
   A Network Scenario with 2 Dummy Nodes


S3FNet File Organization
=================================

The S3FNet file structure displayed and explained below::
 
 ├── makefile
 ├── makefile.common 		# auxiliary file for the makefile
 ├── src 			# source code
 │   ├── dmlprep		# DML parse/partition tools
 │   ├── env			# global naming storage and conversion (e.g., IP, NHI, MAC, emulation VE_id)
 │   ├── extra			# some C utility functions
 │   ├── net			# S3F components: Net, Host, Link, NetworkInterface, traffic, forwarding table ...
 │   ├── os			# protocol 
 │   │   ├── base		# base class (ProtocolGraph, ProtocolSession, protocolMessage ...)
 │   │   ├── dummy		# dummy application (periodic hello message)
 │   │   ├── ipv4		# IPV4 protocol
 │   │   ├── simple_mac		# a simple MAC layer
 │   │   ├── simple_phy		# a simple Physical layer
 │   │   ├── socket		# socket (blocking, non-blocking)
 │   │   ├── tcp		# TCP protocol with some applications
 │   │   ├── udp		# UDP protocol with some applications
 │   │   ├── openVZEmu		# Handle Interaction with OpenVZ-based Emulation Node
 │   │   ├── openflow_switch	# OpenFlow switch, controller and interface
 │   ├── s3fnet.cc		# where main() resides
 │   ├── s3fnet.h		# header file of S3FNet
 │   └── util			# utility functions
 └── test			# S3FNet test cases
	
