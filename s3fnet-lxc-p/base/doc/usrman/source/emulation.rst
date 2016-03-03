OpenVZ-based Emulation
-------------------------

System Setup and configuration 
=================================================================

1. Install CentOS 5 

 * Deselect “Virtualization” (deselect all subcategories)
 * Under “Development”, select “Development Libraries” and “Development Tools”

2. Configure during the first run

 * Firewall: disabled
 * SELinux: disabled
 
 You will need to reboot after this.

3. Compile and install vtime kernel

::

 tar xzf vtime-kernel.tar.gz  	/* vtime-kernel.tar.gz is located at S3F_directory/kernel */

 cd kernel/

 make oldconfig

 make -j2 # this may take a while

 make modules -j2

 make modules_install

 make install

4. Edit sysctl.conf

   Refer to http://openvz.org/Quick_installation for more details. We have provided an sysctl.conf at S3F_directory/kernel, and users can copy it to /etc/ directly.

5. Install OpenVZ utility

::

    wget -P /etc/yum.repos.d/ http://ftp.openvz.org/openvz.repo

    rpm --import http://ftp.openvz.org/RPM-GPG-Key-OpenVZ

    Edit /etc/yum.repos.d/openvz.repo file: disable RHEL6 & enable RHEL5.

    yum install vzctl.x86_64 vzquota.x86_64 bridge-utils

    Reboot into the new vtime kernel (you may want to edit /boot/grub/grub.conf to make it default)

6. Install S3F/S3FNet dependency

* pcap library

::

 yum install libpcap-devel.x86_64

* Python 2.7.5

::

 wget http://www.python.org/ftp/python/2.7.5/Python-2.7.5.tgz
 tar xfvz Python-2.7.5.tgz
 cd Python-2.7.5
 ./configure
 make
 make install

* FANN library

::

 unzip fann-2.1.0beta.zip  	/* in S3F_directory/simctrl */
 cd fann-2.1.0
 ./configure
 make
 make install
 cd ..
 cp fann-2.1.0.conf /etc/ld.so.conf.d/
 ldconfig

7. Compile S3F/S3FNet

:: 

 ./build.sh  		/* in S3F_directory */

Getting Started
=================================================================
TODO

Virtual Time
======================================
TODO

Global Scheduler
======================================
TODO

VE-controller 
======================================
TODO

Distributed Emulation 
======================================
TODO

.. _demu:
.. figure::  images/d-emu.png
   :width: 50 %
   :align:   center
   
Network Testbed Architecture with Distributed Emulation Support

More Examples
=================================================================
