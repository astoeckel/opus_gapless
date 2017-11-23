/**
 * Gapless concatenation of Opus-Frames in OGG container experiment.
 *
 * (c) Andreas Stöckel, 2017, licensed under AGPLv3 or later,
 * see https://www.gnu.org/licenses/AGPLv3
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include "chunk_transcoder.hpp"

using namespace eolian::stream;

int main()
{
	// Read raw audio data from stdin into continous memory, expects audio in
	// raw float format, generate e.g. using ffmpeg:
	// ffmpeg -loglevel error -i <IN FILE> -ac 2 -ar 48000 -f f32le -

	// Encode blocks of the audio data into individual vectors of Opus frames
	ChunkTranscoder trans(
	    std::cin, 0,
	    ChunkTranscoder::Settings().overlap(0.25).bitrate(96000).length(1.0));
	size_t idx = 0;
	std::stringstream ss;
	while (true) {
		ss.str("");
		ss << "blocks/block_" << std::setfill('0') << std::setw(5) << (idx++)
		   << ".ogg";
		std::cerr << "Writing " << ss.str() << std::endl;
		std::ofstream os(ss.str());
		if (!trans.transcode(os)) {
			break;
		}
	}

	// Delete the last block
	{
		std::string fn = ss.str();
		unlink(fn.c_str());
	}
}
