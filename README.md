# Gapless stream from pre-encoded Opus frames

This program demos streaming independent chunks of Ogg/Opus audio and playing them back in the browser.

To build:
```sh
git clone https://github.com/astoeckel/opus_gapless
cd opus_gapless
make
```

To run:
```sh
mkdir -p blocks && rm -f blocks/* && ffmpeg -loglevel error -i <AUDIO FILE> -ac 2 -ar 48000 -f s16le - | ./opus_gapless
```

Then serve this directory via HTTP, e.g. by running
```sh
python3 -m http.server
```
and go to http://localhost:8000/demo.html

Or alternatively play back using
```sh
( for i in `find blocks -name '*.ogg' | sort -n`; do opusdec $i -; done ) | aplay -f dat
```
