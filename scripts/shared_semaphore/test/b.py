import shared_sem
import time
from shared_sem import *

#time.sleep(5)
print("Started")
init_shared_sem(1,1,0)
res = 0
while res == 0 :
	print("acquiring shared sem")
	res  = acquire_shared_sem(1)
	if res == 1 :
		print("acquired shared sem")
		time.sleep(5)
		break

print("releasing shared sem")
release_shared_sem(1)
