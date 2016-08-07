#!/bin/bash

baseDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
USAGE="Usage: `basename $0` [-h] [-n arg] [-m] [-l] [-r]" 
PARAMETER="-n\t Project Name\n-l\t load project \n-r\t run project \n-m\t make new (create) project \n-h\t display the usage "
OVERVIEW="Script for building and running an Awlsim-TimeKeeper project"
EXAMPLE="e.g., `basename $0` -n 4 -f \n Use 4 cores to clean build the S3F project excluding those libraries that only need to be built once"

#default
projectSpecified=0
compile=0
run=0
create=0

# Parse command line options
while getopts hn:mlr OPT; do
    case "$OPT" in 
        h)
            echo $OVERVIEW
            echo $USAGE
            echo $PARAMETER
            echo $EXAMPLE
            exit 0
            ;;
        n)
			projectSpecified=1
            name=$OPTARG
            ;;
        l)
            compile=1
            ;;
        r)
            run=1
            ;;
        m)
			create=1
        	;;

        \?)
            # getopts issues an error message
            echo $OVERVIEW
            echo $USAGE >&2
            echo $PARAMETER
            echo $EXAMPLE
            exit 1
            ;;
        esac
done

# Remove the switches we parsed above
shift `expr $OPTIND - 1`

if [ $projectSpecified -eq 0 ]; then
	echo "Project Name not specified. Error !"
	exit -1
fi

if [ $create -eq 1 ]; then

	cd $baseDir/scripts;./create_new_project.sh $name ;
	echo "-----------------------------"
	echo "Created new project " $name
	echo "-----------------------------"
fi


if [ $compile -eq 1 ]; then
	echo "-----------------------------"
	echo "Loading project ..."
	echo "-----------------------------"
	cp -f $baseDir/Projects/$name/conf/cApp_inject_attack.cc s3fnet-lxc/base/s3fnet/src/os/cApp/cApp_inject_attack.cc
	cp -f $baseDir/Projects/$name/conf/cApp_session.h s3fnet-lxc/base/s3fnet/src/os/cApp/cApp_session.h
	make initialize
	make build_s3f_simulator
	echo $name > $baseDir/scripts/activeProjectID
	echo "-----------------------------"
	echo "Load successfull ..."
	echo "-----------------------------"

fi

activeProjectid=$(cat "$baseDir/scripts/activeProjectID")
echo "Current Active Project = " $activeProjectid
if [ $run -eq 1 ]; then
	if [ "$activeProjectid" == "$name" ]; then

		echo "-----------------------------"
		echo "Running Project ..."
		echo "-----------------------------"
		cd $baseDir/s3fnet-lxc/base
		bash CLEAN_EXPERIMENT
		cd $baseDir/awlsim
		python generator.py $name
		python config_parser.py
		cd $baseDir/Projects/$name
		cd conf/PLC_Config/; make clean; make; sudo make test
		chmod -R 777 $baseDir/s3fnet-lxc/experiment-data
		echo "-----------------------------"
		echo "Project Run Finished ..."
		echo "-----------------------------"
	else
		echo "Cannot run this project. Current Active project is " $activeProjectid
		echo "To run this projectm it must be loaded first."
	fi
fi
cd $baseDir
