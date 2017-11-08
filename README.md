# Gapless WebM stream from pre-encoded Opus frames

To build:
```sh
git clone https://github.com/astoeckel/opus_gapless_webm --recursive
cd opus_gapless_webm
make
```

To run:
```sh
ffmpeg -loglevel error -i <AUDIO FILE> -ac 2 -ar 48000 -f s16le - | ./opus_gapless_webm
```

The program produces a file `test.webm` which contains a transcoded version of the input file where the audio was divided into 1s blocks and then shuffled.
