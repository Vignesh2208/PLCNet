# -*- coding: utf-8 -*-
#
# AWL simulator - instructions
#
# Copyright 2012-2014 Michael Buesch <m@bues.ch>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

from __future__ import division, absolute_import, print_function, unicode_literals
from awlsim.common.compat import *

from awlsim.core.instructions.main import * #@nocy
from awlsim.core.operators import *
#from awlsim.core.instructions.main cimport * #@cy

import time
import ctypes
libc = ctypes.CDLL('libc.so.6')

class Timespec(ctypes.Structure):
	""" timespec struct for nanosleep, see:
      	http://linux.die.net/man/2/nanosleep """
	_fields_ = [('tv_sec', ctypes.c_long),
	('tv_nsec', ctypes.c_long)]

libc.nanosleep.argtypes = [ctypes.POINTER(Timespec),
                           ctypes.POINTER(Timespec)]
nanosleep_req = Timespec()
nanosleep_rem = Timespec()

def nsleep(us):
	#print('nsleep: {0:.9f}'.format(us)) 
	""" Delay microseconds with libc nanosleep() using ctypes. """
	if (us >= 1000000):
		sec = us/1000000
		us %= 1000000
	else: sec = 0
	nanosleep_req.tv_sec = sec
	nanosleep_req.tv_nsec = int(us * 1000)

	libc.nanosleep(nanosleep_req, nanosleep_rem)



class AwlInsn_SLEEP(AwlInsn): #+cdef

	__slots__ = ()

	def __init__(self, cpu, rawInsn):
		AwlInsn.__init__(self, cpu, AwlInsn.TYPE_SLEEP, rawInsn)
		self.assertOpCount(1)

	def run(self):
#@cy		cdef S7StatusWord s

		sleepMsecs = self.cpu.fetch(self.ops[0])
		sleepSecs = sleepMsecs / 1000.0

		if sleepSecs >= self.cpu.cycleTimeLimit:
			raise AwlSimError("__SLEEP time exceed cycle time limit")

		self.cpu.updateTimestamp()
		start = self.cpu.now
		while 1:
			self.cpu.updateTimestamp()
			slept = self.cpu.now - start
			remaining = sleepSecs - slept
			if remaining <= 0.0:
				break
			if remaining >= 0.1:
				self.cpu.requestScreenUpdate()
				# **time.sleep(0.05)
				nsleep(50000)
