#
# FW tools makefile
#
soptrw :
	$(CC) -o soptrw soptrw.c

stnrw :
	$(CC) -o stnrw stnrw.c

#
# The emulator FW is just the normal OSPi FW with an extra define that disables
# the OSPi built-in gpio pins (sets them to -1) and can be compiled on any desktop
# It's compiled for debugging (-g) and so you can use gdb on it to help debug issues.
#
FW_SRC = ../main.cpp ../OpenSprinkler.cpp ../program.cpp ../server.cpp \
		 ../utils.cpp ../weather.cpp ../gpio.cpp ../etherport.cpp

emulfw : $(FW_SRC)
	$(CXX) -o $@ -g -O1 -DENABLE_DEBUG -DEMUL -DOSPI -I .. $^ -lpthread

clean :
	$(RM) -rf soptrw stnrw emulfw *.dat *.output
