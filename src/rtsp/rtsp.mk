#RTSP_PATH should be defined in top level Makefile

RTSP_CSRC=$(RTSP_PATH)/rtsp_buffer.c 			\
		  $(RTSP_PATH)/rtsp.c		 			\
		  $(RTSP_PATH)/rtsp_header_parsing.c 	\
		  $(RTSP_PATH)/rtsp_process.c			\

RTSP_INC=$(RTSP_PATH)/.
