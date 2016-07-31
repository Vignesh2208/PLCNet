#!/bin/sh
rm -f experiment-data
rm -f /home/vignesh/Desktop/PLCs/awlsim-0.42/scripts/../s3fnet-lxc/dilation-code
ln -sf s3fnet-lxc/experiment-data experiment-data
ln -sf  /home/vignesh/Desktop/Timekeeper/dilation-code /home/vignesh/Desktop/PLCs/awlsim-0.42/scripts/../s3fnet-lxc/dilation-code
