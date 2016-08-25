from distutils.core import setup, Extension
setup(name='set_connection', version='1.0',  \
      ext_modules=[Extension('set_connection', ['py_set_connection.c'])])
