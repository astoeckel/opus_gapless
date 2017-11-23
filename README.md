# Gapless stream from pre-encoded Opus frames

This repository contains a C++ program which generates independently encoded Ogg/Opus audio segments. These audio segments can be seamlessly played back in a webbrowser using WebAudio. *Indepdentently encoded* means that the individual segments could have been encoded in parallel or in a random order. Since each segment can be deterministically generated, segments can be seved using a RESTful API or stored in Content-Addressed filesystems such as IPFS.

An online demo can be found at

https://somweyr.de/opus/demo.html

Gapless playback is achieved by two independent measures:
* **Lead-in and lead-out frames:** The encoder adds an additional frame to the beginning/end of each chunk. This frame contains artificial audio data predicted using Linear Predictive Coding. This suppresses ringing-artifacts caused by the encoder trying to encode the discontinuities at the beginning/end of a chunk.
* **Overlap:** All chunks slightly overlap each other and are cross-faded by the browser. The exact number of audio samples that should be cross-faded is stored in the proprietary `CF_IN`, `CF_OUT` metadata fields. These metadata fields are parsed on the JavaScript client. The demo uses extreme overlap values (250ms) for visualization purposes, but 1ms will work just as well.

This code is part of a larger project I'm currently working on and will not receive any further updates in this repository (unless I find severe bugs).

## Building

The supplied Makefile searches for an installed `libopus` using `pkgconfig`.

To build:
```sh
git clone https://github.com/astoeckel/opus_gapless
cd opus_gapless
make
```

## Running

To run feed raw stereo floating point audio data at 48000 samples/s into the `opus_gapless` program. This will create a number of files in the `blocks` subdirectory.
```sh
mkdir -p blocks && rm -f blocks/* && ffmpeg -loglevel error -i <AUDIO FILE> -ac 2 -ar 48000 -f f32le - | ./opus_gapless
```

Then serve this directory via HTTP, e.g. by running
```sh
python3 -m http.server
```
and go to http://localhost:8000/demo.html


## License

```
Opus Gapless
Copyright (C) 2017  Andreas Stöckel

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
```
