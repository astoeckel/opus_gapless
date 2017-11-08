all: opus_gapless_webm

opus_gapless_webm: opus_gapless_webm.cpp libwebm/mkvmuxer/*
	c++ -o opus_gapless_webm -std=c++14 -Wall \
		opus_gapless_webm.cpp \
		libwebm/mkvmuxer/mkvmuxer.cc \
		libwebm/mkvmuxer/mkvmuxerutil.cc \
		libwebm/mkvmuxer/mkvwriter.cc \
		-I libwebm/ \
		`pkg-config --libs --cflags opus`

clean:
	rm -f opus_gapless
