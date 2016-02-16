import threading
from threading import Thread
import time

somevar = 'someval'
class MyClass:

	def __init__(self) :
		self.init_value = 0

	def _func_to_be_threaded(self):
		# main body

		print "Inside thread : ", self.init_value
		

	def func_to_be_threaded(self):
		self.init_value = 2
		print "somevar value = ", somevar
		threading.Thread(target=self._func_to_be_threaded).start()


#obj = MyClass()
#obj.func_to_be_threaded()

while 1 :
	print "hello"
	time.sleep(1)
