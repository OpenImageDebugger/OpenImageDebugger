# Font path: The full path to the font file
FONT_PATH=`pwd`/serif.ttf
# Compilation flags
CFLAGS="-fpic -g -I /usr/include/python3.4 -std=c++11 `pkg-config --cflags freetype2` -DFONT_PATH=\"${FONT_PATH}\""
# Link flags
LDFLAGS="-rdynamic -g -lglfw3 -lGLU -lGL -lXrandr -lXext -lX11 -lGLEW -lrt -ldl -lpthread -lXi -lXxf86vm -lm -lXcursor -lXinerama `pkg-config --libs freetype2` -shared -Wl,-soname,libwatch.so"

# I haven't created a makefile because in the near future I'll probably switch to qmake anyways.
g++ -c ../src/buffer.cpp -o buffer.o ${CFLAGS} && g++ -c ../src/buffer_values.cpp -o buffer_values.o ${CFLAGS} && g++ -c ../src/camera.cpp -o camera.o ${CFLAGS} && g++ -c ../src/main.cpp -o main.o ${CFLAGS} && g++ ../src/shaders/*.cpp ${CFLAGS} buffer.o buffer_values.o camera.o main.o ${LDFLAGS} -o libwatch.so
