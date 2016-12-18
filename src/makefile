#
# Makefile for SMTP daemon
#
# Bob Eager   August 2003
#
# Product name
#
PRODUCT		= smtpd
#
# Target directory
#
TARGET		= f:\binp
#
# Compiler setup
#
CC		= icc
DEFS		= -DTCPV40HDRS
!IFDEF DEBUG
DBUG		= -DDEBUG
!ELSE
DBUG		=
!ENDIF
!IFDEF	NOLOG
SLOG		= -DNOLOG
!ENDIF
!IFDEF	PROD
CFLAGS		= $(DEFS) $(DBUG) $(SLOG) -G4 -Gm -Gn -O -Q -Se -Tl30
CLIB		= cppom30o.lib
!ELSE
CFLAGS		= $(DEFS) $(DBUG) $(SLOG) -G4 -Gm -Gn -Q -Se -Ti -Tl30 -Tm -Tx
CLIB		= cppom30.lib
!ENDIF
#
# Names of library files
#
NETLIB		= ..\netlib\netlib.lib
LIBS		= so32dll.lib tcp32dll.lib $(CLIB) \
		  $(NETLIB) os2386.lib
#
# Names of object files
#
OBJ		= smtpd.obj config.obj server.obj netio.obj mailstor.obj log.obj
#
# Other files
#
DEF		= $(PRODUCT).def
LNK		= $(PRODUCT).lnk
README		= readme.txt
MISC		= sample.cnf
OTHER		= todo.txt
#
# Final executable file
#
EXE		= $(PRODUCT).exe
#
# Distribution
#
DIST		= dist.zip
NETLIBDLL	= ..\netlib\netlib.dll
#
# Archive
#
ARCH		= arch.zip
#
#-----------------------------------------------------------------------------
#
$(EXE):		$(OBJ) $(LNK) $(DEF)
!IFDEF	PROD
		ilink /nodefaultlibrarysearch /nologo /exepack:2 @$(LNK)
		lxlite $(EXE)
		eautil $(EXE) nul /s
!ELSE
		ilink /nodefaultlibrarysearch /debug /nobrowse /nologo @$(LNK)
!ENDIF
#
# Object files
#
smtpd.obj:	smtpd.c smtpd.h mailstor.h log.h
#
config.obj:	config.c smtpd.h confcmds.h log.h
#
server.obj:	server.c smtpd.h cmds.h mailstor.h netio.h log.h
#
netio.obj:	netio.c netio.h
#
mailstor.obj:	mailstor.c mailstor.h smtpd.h log.h
#
log.obj:	log.c log.h
#
# Linker response file. Rebuild if makefile changes
#
$(LNK):		makefile
		@if exist $(LNK) erase $(LNK)
		@echo /map:$(PRODUCT) >> $(LNK)
		@echo /out:$(PRODUCT) >> $(LNK)
		@echo $(OBJ) >> $(LNK)
		@echo $(LIBS) >> $(LNK)
		@echo $(DEF) >> $(LNK)
#
clean:		
		-erase $(OBJ) $(LNK) $(PRODUCT).map csetc.pch
#
install:	$(EXE)
		@copy $(EXE) $(TARGET) > nul
#
dist:		$(EXE) $(NETLIBDLL) $(README) $(MISC)
		zip -9 -j $(DIST) $**
#
arch:		$(EXE) $(README) $(DEF) $(MISC) $(OTHER) *.c *.h makefile
		zip -9 -j $(ARCH) $**
#
# End of makefile for SMTP daemon
#
