
CFLAGS=/nologo /MT /Zi /W3 /D_UNICODE /DUNICODE /D_CRT_SECURE_NO_WARNINGS

LIBS=advapi32.lib \
     shell32.lib

OBJS=misc.obj \
     version.obj

all: cc.exe clwrapper-lib.exe dumpinfo.exe

clean:
   del *.obj *.pdb *.exe *.ilk *.manifest

cc.exe: base.obj cc.obj $(OBJS)
   cl /nologo /Fe$@ /Zi /Fdcc.pdb base.obj cc.obj $(OBJS) $(LIBS)

clwrapper-lib.exe: base.obj lib.obj $(OBJS)
   cl /nologo /Fe$@ /Zi /Fdcc.pdb base.obj lib.obj $(OBJS) $(LIBS)

dumpinfo.exe: dumpinfo.obj $(OBJS)
   cl /nologo /Fe$@ /Zi /Fddumpinfo.pdb dumpinfo.obj $(OBJS) $(LIBS)

base.obj: base.c clwrapper.h
cc.obj: cc.c clwrapper.h
dumpinfo.obj: dumpinfo.c clwrapper.h
misc.obj: misc.c clwrapper.h
version.obj: version.c clwrapper.h

