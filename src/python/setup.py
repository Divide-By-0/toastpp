from __future__ import print_function
import setuptools  # To enable bdist_wheel
from distutils.core import setup, Extension
import os
import sys
from distutils.sysconfig import get_python_inc
import numpy.distutils as npd


toastdir = os.getenv('TOASTDIR')
if toastdir == None or toastdir == "":
	if "nt" in os.name or "posix" in os.name: # If Windows
		toastdir = os.getcwd() + '/../..'
	else:
		print('Expected environment variable TOASTDIR is not defined!')
		print('Please enter the path to the TOAST root directory:')
		toastdir = raw_input()
		#sys.exit(1)
print("Toast dir: ", toastdir, os.getcwd())

major = "%d" % sys.version_info[0]
minor = "%d" % sys.version_info[1]
pyinc = get_python_inc(plat_specific=1)
npinc = npd.misc_util.get_numpy_include_dirs()

module1 = Extension('toast.toastmod',
                    include_dirs = [pyinc,
                                    npinc[0],
                                    toastdir,
                                    toastdir+'/include',
                                    toastdir+'/src/libmath',
                                    toastdir+'/src/libfe',
                                    toastdir+'/src/libstoast'],
                    libraries = ['libmath','libfe','libstoast'] if "nt" in os.name else ['math','fe','stoast'],
                    library_dirs = [toastdir+'/win/x64/Release/lib'] if "nt" in os.name else [toastdir+'/lib'],
                    runtime_library_dirs = None if "nt" in os.name else [toastdir+'/lib'],
                    sources = ['toastmodule.cc'])

setup(
    name = 'PyToast',
    version = '120529',
    description = 'Python TOAST extension',
    author = 'Martin Schweiger',
    url = 'http://www.toastplusplus.org',
    ext_modules = [module1],
    packages = ['toast'],
    data_files = [('', [toastdir+'/win/x64/Release/bin/pthreadVC2.dll',
                        toastdir+'/win/x64/Release/bin/libfe.dll',
                        toastdir+'/win/x64/Release/bin/libmath.dll',
                        toastdir+'/win/x64/Release/bin/libstoast.dll'])] if "nt" in os.name else []
)
