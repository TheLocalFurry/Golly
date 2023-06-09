# makefile-win includes local-win.mk, so create a copy of this file
# and call it local-win.mk, then make any desired changes.

# Change the next 2 lines to specify where you installed wxWidgets:
!include <C:/wxWidgets-3.1.5/build/msw/config.vc>
WX_DIR = C:\wxWidgets-3.1.5

# Change the next line to match your wxWidgets version (first two digits):
WX_RELEASE = 31

# Change the next line depending on where you installed Python:
PYTHON_INCLUDE = -I"C:\Python39-64\include"

# Uncomment the next 4 lines to allow Golly to run Perl scripts:
# PERL_INCLUDE = \
# -DENABLE_PERL \
# -DHAVE_DES_FCRYPT -DNO_HASH_SEED -DUSE_SITECUSTOMIZE -DPERL_IMPLICIT_CONTEXT \
# -DPERL_IMPLICIT_SYS -DUSE_PERLIO -DPERL_MSVCRT_READFIX -I"C:\Perl514-64\lib\CORE"

# Comment out the next line to disable sound play:
ENABLE_SOUND = 1

# Add any extra CXX flags here
EXTRACXXFLAGS =
