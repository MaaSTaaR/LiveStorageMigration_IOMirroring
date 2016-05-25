COMPILER = gcc
FUSE_FILES = filesystem.c
COPY_DEAMON_FILES = copy_deamon.c server.c

build: $(FUSE_FILES) $(COPY_DEAMON_FILES) $(WRITER_FILES)
	$(COMPILER) $(FUSE_FILES) -o bin/filesystem.o `pkg-config fuse --cflags --libs` -lm
	$(COMPILER) $(COPY_DEAMON_FILES) -o bin/copy_deamon.o -lpthread -g

clean:
	rm -f bin/*.o
