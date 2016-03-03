NUM_CORES := 4

CLEAN_EXPERIMENT = bash CLEAN_EXPERIMENT

all: 
	echo "inc | rebuild | debuginc | debug"

inc: lxcman
	./build.sh -i -n $(NUM_CORES)

rebuild: lxcman
	./build.sh -f -n  $(NUM_CORES)
	
fullbuild: lxcman
	cd tklxcmngr/socket_hooks/; make clean; make;
	cd tklxcmngr/serial_driver/; make clean; make;
	./build.sh -d -n $(NUM_CORES)

debuginc: lxcman
	./build.sh -i -f -d -n $(NUM_CORES)

debug: lxcman
	./build.sh -f -d -n $(NUM_CORES)

lxcman:
	cd tklxcmngr; make clean; make;




examplerun:
	$(CLEAN_EXPERIMENT)	
	cd s3fnet/test/lxc_tests/small_2_udp/; make clean; make; make test
	chmod -R 777 ../experiment-data

exampleserialrun:
	$(CLEAN_EXPERIMENT)	
	cd s3fnet/test/lxc_tests/small_2_serial/; make clean; make; make test
	chmod -R 777 ../experiment-data	

run:
	$(CLEAN_EXPERIMENT)	
	# cd s3fnet/test/lxc_tests/small_2_udp_tiny_delay/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/small_2_udp_2_emu_1_router/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/sandbox/; make clean; make; make test
	cd s3fnet/test/lxc_tests/small_2_udp/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/small_2_udp_large_pings/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/small_1_ping/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/large/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/large_emu_only_25/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/large_emu_only_2/; make clean; make; make test
	# cd s3fnet/test/lxc_tests/huge_with_traffic/; make clean; make; make test
	chmod -R 777 ../experiment-data
	
	
