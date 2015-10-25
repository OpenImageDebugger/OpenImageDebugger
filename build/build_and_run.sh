CFLAGS="-fpic -I /usr/include/python3.4 -std=c++11 `pkg-config --cflags freetype2`"
LDFLAGS="-rdynamic -lglfw3 -lGLU -lGL -lXrandr -lXext -lX11 -lGLEW -lrt -ldl -lpthread -lXi -lXxf86vm -lm -lXcursor -lXinerama `pkg-config --libs freetype2` -shared -Wl,-soname,libwatch.so"

g++ -c ../src/buffer.cpp -o buffer.o ${CFLAGS} && g++ -c ../src/buffer_values.cpp -o buffer_values.o ${CFLAGS} && g++ -c ../src/camera.cpp -o camera.o ${CFLAGS} && g++ -c ../src/main.cpp -o main.o ${CFLAGS} && g++ ../src/shaders/*.cpp ${CFLAGS} buffer.o buffer_values.o camera.o main.o ${LDFLAGS} -o libwatch.so && python3 gdb-imagewatch.py
