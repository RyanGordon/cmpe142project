#-------------------------------------------------------------------------------
# File:
#	makefile for CMPE-142 project
# Author:
#	Charles MacDonald
# Notes:
# 	None
#-------------------------------------------------------------------------------

CC	=	g++
AS	=	as
LD	=	g++
CCFLAGS	=	-fpermissive -Wno-int-to-pointer-cast -Wno-pointer-arith \
		-Wno-write-strings
ASFLAGS	=	
LDFLAGS	=

# Output binary
EXE	=	main.exe

# Object list
OBJ	=	obj/main.o	\
		obj/server.o	\
		obj/client.o	\
		obj/util.o

# Dependencies
$(EXE)	:	$(OBJ)
		$(LD) $(OBJ) $(LDFLAGS) -o $(EXE)

all	:	$(EXE)

obj/%.o	:	%.cpp
		$(CC) -c $< -o $@ $(CCFLAGS)
		
# Clean project
.PHONY	:	clean
clean	:
		rm *.o
		rm $(EXE)
		
# Run project
.PHONY	:	a
a	:
		./$(EXE)		

# Clear backup files
.PHONY	:	freshen
freshen	:	
		rm *~
		
#-------------------------------------------------------------------------------
# End
#-------------------------------------------------------------------------------

