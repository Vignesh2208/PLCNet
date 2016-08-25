from multiprocessing import Process, Condition, Lock  
from multiprocessing.managers import BaseManager  
import time, os  

lock = Lock()  
waitC = Condition(lock)  
waitP = Condition(lock)  

class numeri(object):  
	def __init__(self):  
		self.nl = []  

	def getLen(self):  
		return len(self.nl)  

	def stampa(self):  
		print self.nl  

	def appendi(self, x):  
		self.nl.append(x)  

	def svuota(self):  
		for i in range(len(self.nl)):  
			del self.nl[0]  

class numManager(BaseManager):  
	pass  

numManager.register('numeri', numeri, exposed = ['getLen', 'appendi', 'svuota', 'stampa'])  

def consume(waitC, waitP, listaNumeri):  
	lock.acquire()  
	if (listaNumeri.getLen() == 0):  
		waitC.wait()  
	listaNumeri.stampa()  
	listaNumeri.svuota()  
	waitP.notify()  
	lock.release()  

def produce(waitC, waitP, listaNumeri):  
	lock.acquire()  
	if (listaNumeri.getLen() > 0):  
		waitP.wait()  
	for i in range(10):  
		listaNumeri.appendi(i)  
	waitC.notify()  
	lock.release()  


def main():  
	mymanager = numManager()  
	mymanager.start()  
	listaNumeri = mymanager.numeri()  
	producer = Process(target = produce, args =(waitC, waitP, listaNumeri,))  
	producer.start()  
	time.sleep(2)  
	consumer = Process(target = consume, args =(waitC, waitP, listaNumeri,))  
	consumer.start()
	consumer.join()
	producer.join()  

main() 