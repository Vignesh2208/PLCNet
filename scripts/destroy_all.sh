#!/bin/bash
a=1


for i in {1..63}
do
   echo "Destroying lxc"$i
   sudo lxc-destroy -n lxc$i-0
done