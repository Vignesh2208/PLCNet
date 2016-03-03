/**
 * \file phold_node.cc
 * \brief phold model for given topological graph
 */

#include <stdio.h>
#include <stdlib.h>
#include <s3f.h>
#include "phold_node.h"

//#define DEBUG
#define TRANSFER_DELAY 5

PholdMessage::PholdMessage(int msg_id) { 
	__msg_id = msg_id;
}

PholdMessage* PholdMessage::clone() {
	PholdMessage* phm = new PholdMessage(__msg_id);
	return phm;
}

PholdMessage::~PholdMessage() {
	//
	// call the PholdMessage construct when it goes out of scope
	//
	// printf("called PholdMessage %d dtor\n", __msg_id);
}

pHold_Node::pHold_Node(Timeline* tl, string name, int id, int ports ) : 
			Entity(tl,name) {

	myid = id;
	myports = ports;
	int i;

	ic = new InChannel*[ports+1];
	for(i=0; i<ports; i++) {
		stringstream ss;
		ss << "IC-" << i;
		ic[i] = new InChannel( (Entity *)this, ss.str());
	}
	ic[ports] = 0;

	oc = new OutChannel*[ports];
	for(i=0; i<ports; i++) {
		oc[i] = new OutChannel(this,1);
	}

	next_out = id;

	recvd = 0;

	listen_proc =
			new Process( (Entity *)this, (void (s3f::Entity::*)(s3f::Activation))&pHold_Node::listen);

	talk_proc =
			new Process( (Entity *)this, (void (s3f::Entity::*)(s3f::Activation))&pHold_Node::talk);

	// make a lasting binding of the "listen" process to the in-channels
	for(i=0; i<ports; i++)
		ic[i]->bind(listen_proc);
}

pHold_Node** nodes;

/* Process code body
   This method is called when an Activation arrives on 
   an inchannel to which the process is bound.  

   A waiting time is sampled (in milliseconds) and
   is transformed to the native simulation clock scale
   and a waitFor is done, after which the "talk" process
   is executed to pass the Activation along
 */

void pHold_Node::listen(Activation ac) 
{
	recvd++;

	// sample exponential with mean 1/10, assume time-scale is milliseconds
	double millisecs = rng.Exponential(0.1);

	// schedule execution of process "talk" at a time in the future
	// equivalent to the exponential sample interpreted as milliseconds,
	// and pass the given activation along
	//
	ltime_t ticks = d2t(millisecs,3)+1;

	// create a shared pointer to PholdMessage from ac (which we know
	// is a shared pointer to a PholdMessage
	//    One can get at the pointer wrapped in ac by ac.get(),
	// see the printf statement below
	//
	// tr1::shared_ptr<PholdMessage> phm = tr1::dynamic_pointer_cast<PholdMessage>(ac);

	// PholdMessage *phm;
	// Activation ac1( phm=new PholdMessage(ticks) );
	// ac.reset( new PholdMessage(ticks) );
#ifdef DEBUG
	int id = ((PholdMessage*)ac)->msg_id();
	printf("PH At %ld (%ld) listen on %d receives activation %x, waiting until time %ld\n", now(), recvd, myid, id, now()+ticks);
#endif
	//HandleCode h = waitFor( talk_proc, ac, ticks, myid );
	waitFor( talk_proc, ac, ticks, myid );

	// uncomment to exercise cancellation logic
	//
	// HandlePtr hptr(new Handle(h));
	// hptr->cancel();
	// waitFor( talk_proc, ac, ticks, myid );
}

void pHold_Node::talk(Activation ac) 
{
	recvd++;
	int tgt = next_out%myports;

	// Activation ac1( phm = new PholdMessage(now()));
#ifdef DEBUG
	int id = ((PholdMessage *)ac)->msg_id();
	printf("PH At %ld (%ld) talk on %d receives activation %x, writing to %d with %x\n", now(), recvd, myid, id, tgt, id );
#endif
	if(myports> 0) {
		oc[tgt]->write(ac ,  (ltime_t) 0, myid);
		// oc[tgt]->write(ac , (ltime_t)(tgt+1));
		next_out++;
	}
}

void pHold_Node::init() {
	// establish the mappings from this node's outchannel's
	// to the inchannels of the others.  Show use of
	// name directories
	//
	int i;
	for(i=0; i<myports; i++) {
		// make the name of the entity we are targeting
		stringstream en;
		en << "Entity-" << i;

		// Fetch the address of that entity
		Entity* e = Entity::getEntity(en.str());

		assert(e != NULL);

		// construct the name of the InChannel we target.  It
		// contains this entity's identity, i.e. Entity j targets
		// inchannel j on each of the entities it communicates with
		//
		stringstream in;
		in << "IC-" << s3fid();

		// get the address of that inchannel on that entity
		//
		InChannel *inc = e->getInChannel(in.str());
		assert(inc != NULL);

		// now do the mapping
		oc[i]->mapto(inc, TRANSFER_DELAY);
	}
	next_out = (myid+1)%myports;
	Activation act( new PholdMessage((myid<<8)));
	for(i=0; i<myports; i++) {
		oc[i]->write( act, (ltime_t ) 0 , myid);
	}
}

// user needs to create own Interface class and instantiate BuildModel method
void MyInterface::BuildModel( int num_nodes, int num_threads, vector<string> args ) 
{
	int i;

	/* initialize the RNG class mutex. This needed to
     make this class thread safe---every new instantiation
     modifies a state that creates the state of the next
     instantiation
	 */
	pthread_mutex_init(&RNGS::RngStream::nextState_lock,NULL);

	/* make num_ent phold nodes,  Name each
     as Node-x, where "x" is its id
	 */
	nodes = new pHold_Node*[num_nodes];
	for(i=0; i<num_nodes; i++) {
		stringstream ss;
		ss << "Entity-" << i ;
		nodes[i] = new pHold_Node(get_Timeline(i%num_threads), ss.str(), i, num_nodes);
	}
}

// try this out...
int main(int argc, char**argv) {
	ltime_t clock;

	if(argc != 4) {
		printf("usage: phold num_nodes num_threads sim-time\n");
		exit(-1);
	}
	int num_nodes   = atoi(argv[1]);
	int num_threads = atoi(argv[2]);
	long sim_time   = atoi(argv[3]);

	vector<string> inputs;

	// create num_threads timelines, microsecond time scale
	MyInterface myi( num_threads, 6 );

	// make a clique of nodes
	myi.BuildModel( num_nodes, num_threads, inputs );

	// initialize the entities
	myi.InitModel();

	// Run it for 10000 units, in ten window increments
	for(int i=1; i<=10; i++) {
		cout << "enter window " << i << endl;
		clock = myi.advance(STOP_BEFORE_TIME, sim_time);
		cout << "completed window, advanced time to " << clock << endl;
	}
	myi.runtime_measurements();
}
