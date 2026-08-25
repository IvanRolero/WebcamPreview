# Compiler and toolchain
CXX = g++
RC = windres
CXXFLAGS = -std=c++17 -O2 -s -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -mwindows
LIBS = -lgdi32 -lole32 -loleaut32 -lstrmiids -luuid -lcomdlg32 -lcomctl32

# Output Target executable
TARGET = webcam_app.exe

# Object files
OBJS = main.o qcam.o

# Build targets
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)

main.o: main.cpp resource.h stb_image_write.h
	$(CXX) -c main.cpp -o main.o $(CXXFLAGS)

qcam.o: qcam.rc resource.h
	$(RC) -i qcam.rc -o qcam.o

clean:
	rm -f *.o $(TARGET)

.PHONY: all clean