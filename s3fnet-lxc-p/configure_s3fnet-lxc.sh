#!/bin/bash


# Compile the CS Udp Client & Server
cd csudp
make
cd ..

# Go to dilation code and compile the TimeKeeper kernel module, as well as scripts
cd dilation-code
make
cd scripts
make 
cd ..
cd ..

# Go to Lxc-command and compile the "reader"
cd lxc-command
make
cd ..
