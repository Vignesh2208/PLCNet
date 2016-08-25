NUM_CORES := 4
CLEAN_EXPERIMENT = cd s3fnet-lxc/base/; bash CLEAN_EXPERIMENT

all: fullbuild 
	
rebuild: build_s3f_extensions build_python_modules initialize

	@echo "-----------------------------------------------"
	@echo "Re-Building s3f socket hook modules ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/tklxcmngr/socket_hooks/; make clean; make;

	@echo "-----------------------------------------------"
	@echo "Re-Building s3f simulator files ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/; ./build.sh -f -n  $(NUM_CORES)

build_python_modules:

	@echo "-----------------------------------------------"
	@echo "Building set_connection python module ..."
	@echo "-----------------------------------------------"
	@cd scripts/set_connection; python setup.py build 2>/dev/null; python setup.py install 2>/dev/null;
	

	@echo "-----------------------------------------------"
	@echo "Building shared_semaphore python module ..."
	@echo "-----------------------------------------------"
	@cd scripts/shared_semaphore; python setup.py build 2>/dev/null; python setup.py install 2>/dev/null;

	@echo "-----------------------------------------------"
	@echo "Setting environment variables and permissions ..."
	@echo "-----------------------------------------------"
	@chmod a+x scripts/set_env_variables.sh
	@chmod a+x scripts/create_new_project.sh
	@chmod a+x scripts/create_sym_links.sh
	@chmod a+x scripts/destroy_all.sh
	@cd s3fnet-lxc/base/; chmod a+x build.sh
	@cd scripts; ./set_env_variables.sh
	
build_s3f_extensions:

	@echo "-----------------------------------------------"
	@echo "Building lxc manager ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/tklxcmngr; make clean; make;

	@echo "-----------------------------------------------"
	@echo "Building s3f socket hook module ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/tklxcmngr/socket_hooks/; make clean; make;

	@echo "-----------------------------------------------"
	@echo "Building s3f serial driver module ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/tklxcmngr/serial_driver/; make clean; make;	

	@echo "-----------------------------------------------"
	@echo "Building miscellaneous executables ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/lxc-command/; make clean; make;
	@cd s3fnet-lxc/csudp/; make clean; make;

build_s3f_simulator:
	@echo "-----------------------------------------------"
	@echo "Building s3f simulator files ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/; ./build.sh -n $(NUM_CORES)


initialize:

	@echo "-----------------------------------------------"
	@echo "Reading CONFIG.txt and initializing ..."
	@echo "-----------------------------------------------"
	@mkdir -p s3fnet-lxc/data
	@chmod -R 777 s3fnet-lxc/lxc-scripts
	@chmod -R 777 s3fnet-lxc/lxc-command
	python scripts/initialize.py
	./scripts/create_sym_links.sh

install:
	@touch README.txt; sudo python setup.py build install; rm -rf README.txt
	
fullbuild: build_python_modules build_s3f_extensions initialize install
	
	@echo "-----------------------------------------------"
	@echo "Building s3f simulator files ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/; ./build.sh -n $(NUM_CORES)

debug: build_s3f_extensions build_python_modules initialize

	@echo "-----------------------------------------------"
	@echo "Building s3f simulator with debug enabled ..."
	@echo "-----------------------------------------------"
	@cd s3fnet-lxc/base/; ./build.sh -f -d -n $(NUM_CORES)


clean_exp:
	$(CLEAN_EXPERIMENT)


	

