
PLAT=linux
NINJA=./build/${PLAT}/ninja

all: 

emscripten:
	./configure.py --platform=emscripten 
	${NINJA}

mingw: 
	./configure.py --platform=win
	${NINJA}

linux: 
	./configure.py --platform=linux
	${NINJA}

macosx:
	./configure.py --platform=macosx
	${NINJA}

clean: 
	${NINJA} -t clean
