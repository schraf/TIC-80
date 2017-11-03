
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
	./configure.py --platform=linux --no_file_dialog
	${NINJA}

macosx:
	./configure.py --platform=macosx
	${NINJA}

clean: 
	${NINJA} -t clean
