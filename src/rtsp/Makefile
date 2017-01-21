CC=gcc

#CFLAGS+=-Wall -Wextra
#CFLAGS+=-std=c99 -Wpedantic

CFLAGS+=-ggdb3 $(CLI_CFLAGS) -pthread

RTSP_CSRC := rtsp.c \
	         rtsp_process.c \
			 rtsp_header_parsing.c \
			 rtsp_buffer.c

all: CSRC := $(RTSP_CSRC) main_socket_test.c
all: compile

test: CFLAGS += -D RTSP_TESTING 
test: CSRC   := $(RTSP_CSRC) main_api_test.c
test: compile

compile: $(CSRC) 
	cscope -Rb
	$(CC) $(CFLAGS) $(CSRC) -o rtsp.exe
	@echo "Done."



