#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <time_functions.h>

const char *FILENAME = "/proc/dilation/status";

/*
Checks if it is being ran as root or not. All of my code requires root.
*/
int is_root() {
	if (geteuid() == 0)
		return 1;
	printf("Needs to be ran as root\n");
	return 0;

}

int isModuleLoaded() {
	if( access( FILENAME, F_OK ) != -1 ) {
	        // module loaded
		return 1;
	} else { //module not loaded
		printf("TimeKeeper kernel module is not loaded\n");
		return 0;
	}
}

int fixDilation(double dilation) {
	int dil;
	if (dilation < 0) {
		printf("Negative dilation does not make sense\n");
		return -1;
	}
	if (dilation < 1.0 && dilation > 0.0) {
		dil = (int)((1/dilation)*1000.0);
		dil = dil*-1;
	}
	else if (dilation == 1.0 || dilation == -1.0) {
		dil = 0;
	}
	else {
		dil = (int)(dilation*1000.0);
	}
	return dil;
}

/*
Takes an integer (the time_dilation factor) and returns the time dilated process.
    ie: time_dilation = 2 means that when 4 seconds happens in real life,
		the process only thinks 2 seconds has happened
	time_dilation = -2 means that when 2 seconds happens in real life,
		 the process thinks 4 seconds actually happened
*/
int clone_time(unsigned long flags, double dilation, int should_freeze) {
	int dil;
	if ( (dil = fixDilation(dilation)) == -1) {
		return -1;
	}
	printf("Trying to create dilation %d from %f\n",dil, dilation);
        return syscall(351, flags | 0x02000000, should_freeze, NULL, dil, NULL);
}

int synchronize_pids(int count, ...) {
   va_list list;
   int j = 0;
   char final[100];
   char intermediate[100];
   char begin[100];

   sprintf(intermediate, "echo \"E,%d", count);

   va_start(list, count);
   for(j=0; j<count; j++)
   {
     sprintf(intermediate,"%s,%d",intermediate, va_arg(list, int));
   }
   sprintf(intermediate,"%s\" > %s",intermediate, FILENAME);
   va_end(list);
   system((char *)intermediate);
   //printf("%s\n",intermediate);
   return count;
}

int addToExp(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"H,%d\" > %s", pid, FILENAME);
                system((char *)command);
		return 0;
        }
        return 1;
}

int addToExpSim(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"L,%d\" > %s", pid, FILENAME);
                system((char *)command);
                return 0;
        }
        return 1;
}

int startExp() {
	if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"C\" > %s", FILENAME);
                system((char *)command);
                return 0;
        }
        return 1;
}

int stopExp() {
	if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"W\" > %s", FILENAME);
                system((char *)command);
                return 0;
        }
        return 1;
}


int dilate(int pid, double dilation) {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		int dil;
		if ( (dil = fixDilation(dilation)) == -1) {
			return -1;
		}
		printf("Trying to create dilation %d from %f\n",dil, dilation);
		if (dil < 0) {
		sprintf(command, "echo \"A,%d,1,%d\" > %s", pid, dil*-1, FILENAME);
                system((char *)command);
		}
		else {
		sprintf(command, "echo \"A,%d,%d\" > %s", pid, dil, FILENAME);
                system((char *)command);
		}
	}
	return 0;
}

int dilate_all(int pid, double dilation) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
		int dil;
		if ( (dil = fixDilation(dilation)) == -1) {
			return -1;
		}
		printf("Trying to create dilation %d from %f\n",dil, dilation);
                if (dil < 0) {
                sprintf(command, "echo \"B,%d,1,%d\" > %s", pid, dil*-1, FILENAME);
                system((char *)command);
                }
                else {
                sprintf(command, "echo \"B,%d,%d\" > %s", pid, dil, FILENAME);
                system((char *)command);
                }
        }
        return 0;
}

/*
Takes an integer (pid of the process). This function will essentially 'freeze' the
time of the process. It does this by sending a sigstop signal to the process.
*/
int freeze(int pid) {
	if (is_root() && isModuleLoaded()) {
		char command[100];
		sprintf(command, "echo \"Y,%d,%d\" > %s", pid, SIGSTOP, FILENAME);
        	system((char *)command);
		return 1;
	}
	return 0;
}

/*
Takes an integer (pid of the process). This function will unfreeze the process.
When the process is unfrozen, it will think that no time has passed, and will
continue doing whatever it was doing before it was frozen.
*/
int unfreeze(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"Y,%d,%d\" > %s", pid, SIGCONT, FILENAME);
                system((char *)command);
                return 1;
        }
        return 0;
}

/*
Same as freeze, except that it will freeze the process as well as all of its children.
*/
int freeze_all(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"Z,%d,%d\" > %s", pid, SIGSTOP, FILENAME);
                system((char *)command);
                return 1;
        }
        return 0;
}

/*
Same as unfreeze, except that it will unfreeze the process as well as all of its children.
*/
int unfreeze_all(int pid) {
        if (is_root() && isModuleLoaded()) {
                char command[100];
                sprintf(command, "echo \"Z,%d,%d\" > %s", pid, SIGCONT, FILENAME);
                system((char *)command);
                return 1;
        }
        return 0;
}

/*
Takes an integer (pid of the process). This function will actually make a process
	take longer to do something.
    ie: if the time_dilation of the process is 2, when a process does something in
		4 seconds in real life, it thinks 2 seconds has happened. Now, if
		you use slow_computation on this process, it will take 8 seconds
		in real life time to accomplish the same task, and the process will
		think 4 seconds has happened.
   The way I got this to work is a hack at the moment. Basically, I use signals to
	stop and continue the process based on its time dilation factor.
	ie: For every 300 ms time chunk and a time dilation factor of 2, I allow the
	 process to run for 150 ms, then I send a stop signal for 150 ms. Then I repeat.
*/
int slow_computation(int pid) {
        if (is_root()) {
                char command[100];
                sprintf(command, "echo \"R,%d\" > %s", pid, FILENAME);
                system((char *)command);
                return 1;
        }
        return 0;
}

/*
Takes an integer (pid of the process). This is the inverse of slow_computation.
	Will no longer send stop/continue signals to dictate how much time a
	process gets to run. However, the processes perspective of time will still be dilated.
*/
int regular_computation(int pid) {
        if (is_root()) {
                char command[100];
                sprintf(command, "echo \"D,%d\" > %s", pid, FILENAME);
                system((char *)command);
                return 1;
        }
        return 0;
}
