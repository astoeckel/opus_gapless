all: opus_gapless

opus_gapless: opus_gapless.cpp ogg_opus_muxer.*
	c++ -o opus_gapless -g -O0 -std=c++14 -Wall \
		opus_gapless.cpp \
		ogg_opus_muxer.cpp \
		-O3 -g \
		`pkg-config --libs --cflags opus`

clean:
	rm -f opus_gapless
