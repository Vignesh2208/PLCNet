from distutils.core import setup, Extension
setup(name='shared_sem', version='1.0',  \
      ext_modules=[Extension('shared_sem', include_dirs=['/usr/local/include','/usr/local/include/python3.2'], libraries=['rt','pthread'], library_dirs = ['/usr/local/lib','/usr/lib/i386-linux-gnu'], sources = ['shared_semaphore.c'])])
