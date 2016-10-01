#!/bin/bash

projectName=$1
cp -r -n ../awlsim/template/ProjectTemplates/UntitledProject ../Projects/$projectName
sed -i "s/@ExperimentName@/$projectName/g" ../Projects/$projectName/conf/exp_config
cp -f ../Projects/$projectName/conf/cApp_inject_attack.cc ../s3fnet-lxc/base/s3fnet/src/os/cApp/cApp_inject_attack.cc
cp -f ../Projects/$projectName/conf/cApp_session.h ../s3fnet-lxc/base/s3fnet/src/os/cApp/cApp_session.h
