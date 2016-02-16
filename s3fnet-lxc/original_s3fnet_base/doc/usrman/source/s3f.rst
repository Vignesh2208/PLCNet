S3F
--------------------------------
The Simpler Scalable Simulation Framework is an API developed to support modular construction of simulation models, in such a way that potential parallelism can be easily identiﬁed and exploited. More details are presented in the paper "`S3F: the Scalable Simulation Framework revisited <http://dl.acm.org/citation.cfm?id=2431908>`_."
 

Core Components
=================================

 In S3F, a simulation is composed of interactions among a number of **Entity** objects. Entities interact by passing **Events** through channel endpoints they own. Channel endpoints are described by **InChannels** and **OutChannels** depending on the message direction. The Entity class is "owner" for simulation state and instances of other S3F objects (OutChannel, InChannel, and Process). Each entity is aligned to a **Timeline**, which hosts an **Event List** and is responsible for advancing all entities aligned to it. Interactions between co-aligned entities need no synchronization other than this event-list. Multiple timelines may run simultaneously to exploit parallelism, but they have to be carefully synchronized to guarantee global causality. The synchronization mechanism is built around explicitly expressed delays across channels whose end-points reside on entities that are not aligned. We call these **cross-timeline channels**. The synchronization algorithm creates synchronization windows, within which all timelines are safe to advance without being affected by other timelines. Figure :ref:`s3f-basic` depicts the basic elements described above.

.. _s3f-basic:
.. figure::  images/s3f-basic.png
   :width: 30 %
   :align:   center
   
   S3F Basic Elements

.. _s3f-logic-org:

Logical Organization of S3F Simulation Program
===============================================
Figure :ref:`s3flogic` illustrates how an S3F-based application interfacing with the S3F kernel to drive simulation experiments. A thumbnail sketch of the approach is as follow:

1. The control thread creates an instance of a class derived from Interface. 
2. The derived class constructor passes its parameters to the base Interface class constructor. This creates a number of pthreads whose code bodies are the Timeline::thread function. These all immediately synchronize on a barrier that includes them all, and the control function. 
3. The control thread continues, and calls Interface::BuildModel. This actually runs code written by a modeler or simulation framework builder, and does whatever it likes to create the structure of a simulation model by calling constructors of various S3F objects. 
4. After Interface::BuildModel returns, the control thread calls Interface::Init. This method just calls the init method on every S3F “Entity” class instance, methods written by the modeler. 
5. The control now calls Interface::advance with arguments specifying how/when the newly initiated simulation epoch should end. The control thread suspends, the Timeline threads are released to advance simulation time until the stopping criterion is met, and when they are done the control thread is released while the Timeline threads are suspended.


.. _s3flogic:
.. figure::  images/s3f-interface.png
   :width: 30 %
   :align:   center
   
   Logical Organization of S3F Simulation Program

S3FNet is a network simulator application built on top of S3F. Its main() is displayed below as an illustrative example::

 int main(int argc, char** argv)
 {
  ...

  // create total timelines and timescale
  Interface sim_inf( total_timeline, tick_per_second );

  // build and configure the simulation model
  sim_inf.BuildModel( dml_cfg );

  // initialize the entities (hosts)
  sim_inf.InitModel();

   // run it some window increments
  for(int i=1; i<=num_epoch; i++)
  {
    cout << "enter epoch window " << i << endl;
    clock = sim_inf.advance(STOP_BEFORE_TIME, sim_single_run_time);
    cout << "completed epoch window, advanced time to " << clock << endl;
  }
  cout << "Finished" << endl;

  // simulation runtime speed measurement
  sim_inf.runtime_measurements();

  return 0;
 }

Synchronization
=================================

S3F supports parallel execution, which requires synchronization among the timelines. Multiple timelines may run simultaneously to exploit parallelism, but they have to be carefully synchronized to guarantee global causality. Much of the motivation and design of S3F is to support synchronization more-or-less transparently, yet provide hooks to the sophisticated modeler to transfer modeling information to the synchronization engine that is used to improve performance.

S3F synchronizes its timelines at two levels. At a coarse level, timelines are left to run during an **epoch**, which terminates either after a specified length of simulation time, or when the global state meets some specified condition. Between epochs S3F allows a modeler to do computations that affect the global simulation state, without concern for interference by timelines. Good examples of use include periodically recalculating of path loss delays in a wireless simulator, or periodic updating of forwarding tables within routers. States created by these computations are otherwise taken to be constant
when the simulation is running. Within an epoch timelines synchronize with each other using **barrier synchronization**, each of which establishes the length of the next synchronization window during which timelines may execute concurrently. More details are presented in the paper "`S3F: the Scalable Simulation Framework revisited <http://dl.acm.org/citation.cfm?id=2431908>`_."

S3F also supports network emulation. Synchronization between emulation and simulation is managed by the global scheduler in S3F at the end of a synchronization window, when all timelines are blocked, and events and control information are passed between emulation and simulation. The detailed algorithm of global scheduler is presented in the paper "`Virtual time integration of emulation and parallel simulation <http://dl.acm.org/citation.cfm?id=2372597>`_."

More details on synchronization in S3F (synchronous, asynchronous, and composite synchronization) are discussed in Chapter :ref:`s3f-synchronization`.

