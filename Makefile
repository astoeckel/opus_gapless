all: opus_gapless

opus_gapless: opus_gapless.cpp ogg_opus_muxer.* lpc.* encoder.* chunk_transcoder.*
	c++ -o opus_gapless -g -O0 -std=c++14 -Wall \
		opus_gapless.cpp \
		chunk_transcoder.cpp \
		encoder.cpp \
		lpc.cpp \
		ogg_opus_muxer.cpp \
		-O3 \
		`pkg-config --libs --cflags opus`

clean:
	rm -f opus_gapless
