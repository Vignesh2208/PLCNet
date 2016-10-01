#!/bin/bash

scriptDir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
awlsimModulePath=$scriptDir/../awlsim
modbusModulePath=$scriptDir/../awlsim/modbus

if grep -q "awlsim" $HOME/.bashrc ; then
   echo "Already Found"
   source $HOME/.bashrc
else
	echo "export PYTHONPATH=\$PYTHONPATH:$awlsimModulePath" >> $HOME/.bashrc
	echo "export PYTHONPATH=\$PYTHONPATH:$modbusModulePath" >> $HOME/.bashrc
	source $HOME/.bashrc
	echo "Not found"
fi