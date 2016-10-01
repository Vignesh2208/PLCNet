Getting Started
-----------------
There are two versions of the S3F/S3FNet project

* Base version (S3F simulation framework + S3FNet network simulator)

* Full version (More features including OpenVZ-based emulation, and OpenFlow-based software-defined network support)


Supported Platforms
*******************
S3F simulation framework and S3FNet network simulator (base version) are able to run on most UNIX-based machines.

The OpenVZ-based network emulation feature (in full version) requires Linux kernel 2.6.18. Examples of supporting platform are CentOS 5 and RedHat 5.
 
The OpenFlow simulation feature (in full version) is able to run on most Linux platforms. Apple systems are not supported yet.


Download
**********************************
There are two ways to obtain the source code. You can get it as a zipped tarball from the web site: https://s3f.iti.illinois.edu/. Once downloaded, it can be decompressed using::

   % tar xvzf s3f-<version>.tar.gz

Alternatively, You can use svn to check out the source code from the repository (using ``guest`` both as the username and password).

Base version::

	% svn co https://xxx
   
Full version::

	% svn co https://xxx
   

Software Requirement
***********************************
Software packages required for building different features of S3F/S3FNet are summarized below. The apt-get commands for Debian-based platforms are just used as examples.

* S3F/S3FNet base version
	No special packages required.

* OpenFlow Simulation
	Required package: libxml, boost, Python 2.7.5

	To install libxml::

	% sudo apt-get install libxml2-dev

	To install boost library::

	% sudo apt-get install libboost-all-dev

	To install Python-2.7.5::

	% wget http://www.python.org/ftp/python/2.7.5/Python-2.7.5.tgz
	% tar xfvz Python-2.7.5.tgz
	% cd Python-2.7.5
	% ./configure
	% make
	% make install

* OpenVZ-based Emulation
	Required package: pcap, Fast Artificial Neural Network Library (FANN)

	To install pcap library::
	
	% sudo apt-get install libpcap-dev

	To install FANN (the zipped FANN library is located under S3F_project_root/simctrl)::

	% cd fann-2.1.0
	% ./configure
	% make
	% make install
	% cd ..
	% cp fann-2.1.0.conf /etc/ld.so.conf.d/
	% ldconfig


Build
******************************

The build.sh script in S3F's root directory is used for building the S3F/S3FNet project::
 
 % ./build.sh

By default, it clean builds the network simulator for the base version; and clean builds the system with OpenVZ-based emulation (no application lookahead) and OpenFlow simulation for the full version.

Usage: build.sh [-h] [-n arg] [-f] [-i] [-s] [-l] [-d]

-n	 number of cores used to run make

-f	 skip building those libraries that only need to be built once (DML, metis, openflowlib)

-i	 incremental build (update) instead of clean build

-d	 display S3FNet debug messages

-h	 display the usage

-s	 simulation only, not building emulation [Full Version Only]

-l	 enable emulation lookahead [Full Version Only]

*Examples:*

Use 4 cores to clean build the S3F project for the first time::

% build.sh -n 4

Use 4 cores to clean build the S3F project excluding those libraries that only need to be built once::

% build.sh -n 4 -f 

Clean build the S3F project with application lookahead for OpenVZ-based emulation [Full Version Only]::

% build.sh -l

Clean build the S3F project with simulation only including OpenFlow support (OpenVZ-based emulation disabled) with debug message turned on. [Fully Version Only]::

% build.sh -s -d

Running S3F/S3FNet Experiments
*********************************

Once the project has been built successfully, it is time to run some simulation experiments. Here, we are going to run two experiments:

1. PHOLD model (a simple S3F-based application)
2. A server-client network model downloading files through UDP protocol (a test case in S3FNet)

How to develop your own network protocol and application is described in :ref:`s3fnet-dev`, and the logical organization of S3F simulation program is presented in :ref:`s3f-logic-org`. 

To run the PHOLD model::

 % cd app
 % ./phold_node 4 2 10

The results may look like::

 enter window 1
 completed window, advanced time to 10
 enter window 2
 completed window, advanced time to 20
 enter window 3
 completed window, advanced time to 30
 enter window 4
 completed window, advanced time to 40
 enter window 5
 completed window, advanced time to 50
 enter window 6
 completed window, advanced time to 60
 enter window 7
 completed window, advanced time to 70
 enter window 8
 completed window, advanced time to 80
 enter window 9
 completed window, advanced time to 90
 enter window 10
 completed window, advanced time to 100
 -------------- runtime measurements ----------------
 Simulation run of 0.0001 sim seconds, with 2 timelines
 	configured without smart pointer events
 	configured without smart pointer activations
 total run time is 0.004684 seconds
 simulation run time is 0.004392 seconds
 accumulated usr time is 0.004 seconds
 accumulated sys time is 0 seconds
 total evts 59, work evts 34, sync evts 0
 total exec evt rate 13433.5, work evt rate 7741.35
 ----------------------------------------------------


To run the UDP server-client model::
 
 % cd s3fnet/test/udp_oneflow_2timeline
 % make 
   ../../dmlenv -b 10.10.0.0 oneflow.dml > oneflow-env.dml
   ../../dmlenv -r all oneflow.dml > oneflow-rt.dml
 % make test

The results may look like::
   
   ../../s3fnet *.dml 
   Input DML files:
   oneflow-env.dml
   oneflow-rt.dml
   oneflow.dml
   
   enter epoch window 1
   2,639,721: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   14,279,442: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   25,919,163: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   37,558,884: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   49,198,605: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   60,838,326: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   72,478,047: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   84,117,768: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   95,757,489: UDP client "10.10.0.2" downloaded 100000 bytes from server "10.10.0.18", throughput 487.887878 Kb/s.
   completed epoch window, advanced time to 100000000
   Finished
   -------------- runtime measurements ----------------
   Simulation run of 100 sim seconds, with 2 timelines
 	configured without smart pointer events
 	configured without smart pointer activations
   total run time is 0.319977 seconds
   simulation run time is 0.318845 seconds
   accumulated usr time is 0.0260015 seconds
   accumulated sys time is 0.356022 seconds
   total evts 6364, work evts 3637, sync evts 0
   total exec evt rate 19959.5, work evt rate 11406.8
   ----------------------------------------------------

