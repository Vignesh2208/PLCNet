# makefile.common lists all header, source, and object files.
include ./makefile.common

PROGRAMS = s3fnet dmlenv dmlpart
S3FLIB  = ../api/s3f.a
RNDLIB  = ../rng/rng.a
AUXLIB  = ../aux/aux.a

all: $(PROGRAMS)

# This is how to link the object files to produce the executable.
s3fnet:	$(S3FNET_OBJECTS) $(S3FLIB) $(RNDLIB) $(AUXLIB)
	$(CXX) $(S3FNET_OBJECTS) -o $@ $(S3FNET_LDFLAGS) $(S3FNET_LDADD)
	
dmlenv: $(DMLENV_OBJECTS) $(EXTRA_OBJECTS)
	$(CXX) $(DMLENV_OBJECTS) -o $@ $(DMLENV_LDFLAGS) ${DMLENV_LDADD}

dmlpart: $(DMLPART_OBJECTS) $(EXTRA_OBJECTS)
	$(CXX) $(DMLPART_OBJECTS) -o $@ $(DMLPART_LDFLAGS) $(DMLPART_LDADD)


# This is how to compile the source code that does not require instrumentation.
.cc.o:
	$(CXX) $(S3FNET_INCLUDES) $(S3FNET_CFLAGS) -o $@ -c $<

.c.o:
	$(CC) $(S3FNET_INCLUDES) $(S3FNET_CFLAGS) -o $@ -c $<

# This is how to compile the source files for dmlenv utility.
%.env.o:	%.cc $(DMLENV_HDRFILES) $(DMLENV_SRCFILES)
	$(CXX) $(DMLENV_INCLUDES) $(DMLENV_CFLAGS) $(S3FNET_INCLUDES) $(S3FNET_CFLAGS) -o $@ -c $<

# This is how to compile the source files for dmlpart utility.
%.part.o:	%.cc $(DMLPART_HDRFILES) $(DMLPART_SRCFILES)
	$(CXX) $(DMLPART_INCLUDES) $(DMLPART_CFLAGS) $(S3FNET_INCLUDES) $(S3FNET_CFLAGS) -o $@ -c $<

distclean: clean

clean: dependclean
	rm -f $(PROGRAMS) $(S3FNET_OBJECTS) $(DMLENV_OBJECTS) $(DMLPART_OBJECTS)
	rm -f makefile.depend makefile.bak _tmp?.cc .tmpdat* *.log *~

depend: $(S3FNET_HDRFILES) $(S3FNET_SRCFILES)
	@ echo '# Compilation Dependencies' > makefile.depend
	$(CXX) -M $(S3FNET_INCLUDES) $(S3FNET_CFLAGS) $(S3FNET_SRCFILES) >> makefile.depend
	@ mv makefile makefile.bak
	@ sed -e '/^\# Compilation Dependencies/,$$d' < makefile.bak > makefile
	@ cat makefile.depend >> makefile
	@ rm -f makefile.bak
	@ rm -f makefile.depend

dependclean :
	@ mv makefile makefile.bak
	@ sed -e '/^\# Compilation Dependencies/,$$d' < makefile.bak > makefile
	@ echo '# Compilation Dependencies' >> makefile
	@ rm -f makefile.bak

tags:
	@ etags $(S3FNET_HDRFILES) $(S3FNET_SRCFILES)

print:
	@ echo $(S3FNET_OBJECTS)
	#@ echo $(DMLENV_OBJECTS)
	#@ echo $(DMLENV_LDFLAGS)
	#@ echo ${DMLENV_LDADD}
	@ echo $(UDP_SRCFILES)
	@ echo $(UDP_HDRFILES)
	@ echo $(TCP_SRCFILES)
	@ echo $(TCP_HDRFILES)
	@ echo $(TCP_OBJECTS)
# Compilation Dependencies
