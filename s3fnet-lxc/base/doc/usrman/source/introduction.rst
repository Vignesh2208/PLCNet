Introduction
------------

System Overview
**********************

`SSF (Scalable Simulation Framework) <http://www.ssfnet.org/>`_ defines an API that supports modular construction of simulation models, with automated exploitation of parallelism. Building on ten years of experience, we have developed a second generation API, named "S3F", to better reflect use and support maintainability.

We have also developed a network simulation/emulation testbed, named "S3FNet", on top of S3F, for large-scale system analysis and evaluation. Figure :ref:`arch` shows the overall system architecture with following three layers:

* S3F: the kernel of the system
* S3FNet: a parallel network simulator
* OpenVZ virtual machine manager for network emulation

.. _arch:

.. figure::  images/arch.png
   :width: 30 %
   :align:   center
   
   System Architecture

**S3F** defines an API for parallel discrete-event simulation, and has the following major improvements:

* S3F uses only standard language and library features found in the GNU C++ compiler, g++.
* Time advancement is broken up into epochs, between which control is released from the simulation threads to allow other activity (e.g., recalculation of received radio strength maps and location of mobile devices).
* S3F gives a modeler extreme control over the ordering of process body executions.
* S3F supports the OpenVZ-based network emulation based on the notion of virtual clock.

**S3FNet** is a network simulator built on top of the S3F kernel. It is capable for creating communication network models with network devices (e.g., host, switch, and router) and layered protocols (e.g., TCP/IP, OpenFlow).  We also expand its capacity by integrating the network simulator with the OpenVZ-based network emulation. Users can use emulation to represent the execution of critical software, and simulation to model an extensive ensemble of background computation and communication. The main features of S3FNet are summarized as follows:

* Parallel discrete-event simulation
* Virtual-machine-based (OpenVZ) emulation (embedded in **virtual time** instead of wall-clock time)
* **Distributed emulation** over TCP/IP networking
* Application lookahead (neural-network-based model)
* Simulation and emulation of **OpenFlow-based software-defined networks (SDN)**

`OpenVZ <http://openvz.org/Main_Page>`_ is an OS-level virtualization technology, which creates and manages multiple isolated Linux containers, also called Virtual Environments (VEs), on a single physical server. Each VE performs exactly like a stand-alone host with its own root access, users and groups, IP and MAC address, memory, process tree, file system, system libraries and configuration files, but shares a single instance of the Linux OS kernel for services such as TCP/IP. Compared with other virtualization technologies such as Xen (para-virtualization) and QEMU (full-virtualization), OpenVZ is light-weight (e.g., we are able to run 300+ VEs on a commodity server with eight 2GHz-cores and 16 GB memory), at the cost of diversity in the underlying operating system.

OpenVZ emulation allows users to execute real software in the VEs to achieve high functional fidelity. It also frees a modeler from developing complicated simulation models for network applications and the corresponding protocol stacks. Besides functional fidelity, our version of OpenVZ also provides **temporal fidelity** through a virtual clock implementation, allowing an integration with the simulation system.

File Organization
*******************

The S3F project structure is summarized as follows::

 ├── api				# S3F core API: Entity, InChannel, OutChannel, Timeline, Event ...
 ├── app				# S3F application: phold_node
 ├── aux				# synchronization barriers
 ├── build.sh				# script for building the S3F/S3FNet project
 ├── dml				# dml library
 ├── doc				# documentation: user manual, doxygen documents ...
 ├── kernel				# virtual time kernel based on linux 2.6.18 for OpenVZ-based emulation
 ├── metis				# metis library
 ├── openflowlib			# OpenFlow library
 ├── openvzemu_include		 	# linux header needed for OpenVZ-based emulation	
 ├── openvzemu_include.tar.gz		# tarball of the above headers
 ├── rng				# random number generator
 ├── s3f.h				# Header file of the S3F project
 ├── s3fnet				# S3FNet network simulator
 ├── simctrl				# emulation management module
 └── time				# event list

