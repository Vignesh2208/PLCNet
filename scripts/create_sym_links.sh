#!/bin/sh
rm -f experiment-data
rm -f /home/user/Desktop/awlsim-0.42/scripts/../s3fnet-lxc/dilation-code
ln -sf s3fnet-lxc/experiment-data experiment-data
ln -sf  /home/user/Desktop/TimeKeeper/dilation-code /home/user/Desktop/awlsim-0.42/scripts/../s3fnet-lxc/dilation-code
