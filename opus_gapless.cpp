/**
 * Gapless concatenation of Opus-Frames in OGG container experiment.
 *
 * (c) Andreas Stöckel, 2017, licensed under AGPLv3 or later,
 * see https://www.gnu.org/licenses/AGPLv3
 */

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <opus/opus.h>
#include "ogg_opus_muxer.hpp"

using namespace eolian::stream;

int main()
{
	constexpr size_t BUF_SIZE = (1 << 16);
	constexpr size_t SAMPLE_RATE = 48000;
	constexpr size_t CHANNELS = 2;
	constexpr size_t BITDEPTH = 16;
	constexpr size_t BYTES_PER_SAMPLE = CHANNELS * (BITDEPTH / 8);
	constexpr size_t FRAME_LEN = 20;
	constexpr size_t FRAME_SAMPLES = FRAME_LEN * SAMPLE_RATE / 1000;
	constexpr size_t FRAME_SIZE = BYTES_PER_SAMPLE * FRAME_SAMPLES;
	constexpr size_t FRAMES_PER_BLOCK = 250;
	constexpr size_t BLOCK_SIZE = FRAMES_PER_BLOCK * FRAME_SIZE;

	char buf[BUF_SIZE];
	char zeros[BUF_SIZE];
	std::fill(zeros, zeros + BUF_SIZE, 0);

	// Read raw audio data from stdin into continous memory, expects audio in
	// raw dat format, generate e.g. using ffmpeg:
	// ffmpeg -loglevel error -i <IN FILE> -ac 2 -ar 48000 -f s16le -
	std::cerr << "Reading input data to memory..." << std::endl;
	size_t size;
	std::vector<char> raw_audio;
	do {
		std::cin.read(buf, BUF_SIZE);
		size = std::cin.gcount();
		raw_audio.insert(raw_audio.end(), buf, buf + size);
	} while (size == BUF_SIZE);

	// Pad the data with zeros
	if (raw_audio.size() % FRAME_SIZE > 0) {
		raw_audio.insert(raw_audio.end(), zeros,
		                 zeros + FRAME_SIZE - raw_audio.size() % FRAME_SIZE);
	}

	// Encode blocks of the audio data into individual vectors of Opus frames
	std::cerr << "Encoding Opus frames..." << std::endl;
	int err;
	OpusEncoder *enc = opus_encoder_create(SAMPLE_RATE, CHANNELS,
	                                       OPUS_APPLICATION_AUDIO, &err);
	opus_encoder_ctl(enc, OPUS_SET_BITRATE(128000));
	opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));

	std::vector<std::vector<std::vector<char>>> blocks;
	auto cur = raw_audio.begin();
	auto end = raw_audio.end();
	while (cur != end) {
		auto block_end = std::min(end, cur + BLOCK_SIZE);

		// Start a new block, reset the opus encoder to emulate opus frames
		// being encoded by a different thread or originating from a different
		// file.
		opus_encoder_ctl(enc, OPUS_SET_PREDICTION_DISABLED(0));
		std::vector<std::vector<char>> block;

		// Write the actual data
		while (cur != block_end) {
			auto frame_end = std::min(block_end, cur + FRAME_SIZE);
			if (frame_end == block_end) {
				opus_encoder_ctl(enc, OPUS_SET_PREDICTION_DISABLED(1));
			}

			int16_t *src = reinterpret_cast<int16_t *>(&(*cur));
			int size = opus_encode(enc, src, FRAME_SAMPLES,
			                       reinterpret_cast<uint8_t *>(buf), BUF_SIZE);
			if (size > 0) {
				block.emplace_back(buf, buf + size);
			}
			cur = frame_end;
		}

		blocks.emplace_back(std::move(block));
	}
	opus_encoder_destroy(enc);

	// Write the frames to ogg files
	size_t idx = 0;
	for (const auto &block : blocks) {
		int64_t granule = 0;

		std::stringstream ss;
		ss << "blocks/block_" << std::setfill('0') << std::setw(5) << idx
		   << ".ogg";
		std::cerr << "Writing " << ss.str() << std::endl;

		// Write the individual blocks to the Ogg file, set pre- and post-skip
		// correctly to cut the lead-in and lead-out frames away; take the 6.5ms
		// Opus pre-skip into account.
		std::ofstream os(ss.str());
		size_t pre_skip = (idx == 0) ? 312 : 960;
		OggOpusMuxer muxer(os, pre_skip, opus_get_version_string());
		for (size_t i = 0; i < block.size(); i++) {
			if (i == 0 && idx > 0) {
				const auto &frame = blocks[idx - 1].back();
				const uint8_t *src = reinterpret_cast<const uint8_t *>(&frame[0]);
				muxer.write_frame(false, granule, src, frame.size());
				granule += FRAME_SAMPLES;
			}
			const auto &frame = block[i];
			const bool last = i + 1 == block.size();
			granule += FRAME_SAMPLES;
			const uint8_t *src = reinterpret_cast<const uint8_t *>(&frame[0]);
			muxer.write_frame(last, granule, src, frame.size());
		}
		idx++;
	}
}

