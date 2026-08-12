CC = gcc
WINDRES = windres
CFLAGS = -mwindows -Wall -O2
LIBS = -lpsapi -lshlwapi -ldwmapi -lcomctl32
OBJ = main.o optimizer.o resource.o
TARGET = MemoryOptimizer.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(CFLAGS) $(LIBS)

main.o: main.c optimizer.h
	$(CC) $(CFLAGS) -c main.c -o main.o

optimizer.o: optimizer.c optimizer.h
	$(CC) $(CFLAGS) -c optimizer.c -o optimizer.o

resource.o: resource.rc
	$(WINDRES) resource.rc -O coff -o resource.o

clean:
	del /Q $(OBJ) $(TARGET) 2>NUL || true
