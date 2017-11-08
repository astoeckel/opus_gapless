/**
 * Gapless concatenation of Opus-Frames in WebM container experiment.
 *
 * (c) Andreas Stöckel, 2017, licensed under AGPLv3 or later,
 * see https://www.gnu.org/licenses/AGPLv3
 */

#include <algorithm>
#include <random>
#include <iostream>
#include <vector>

#include <mkvmuxer/mkvmuxer.h>
#include <mkvmuxer/mkvwriter.h>
#include <opus/opus.h>

using namespace mkvmuxer;

struct OpusMkvCodecPrivate {
	uint8_t head[8] = {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64};
	uint8_t version = 1;
	uint8_t channel_count;
	uint16_t pre_skip = 0;
	uint32_t sample_rate;
	uint16_t gain = 0;
	uint8_t mapping_family = 0;

	OpusMkvCodecPrivate(uint8_t channel_count, uint32_t sample_rate)
	    : channel_count(channel_count), sample_rate(sample_rate)
	{
	}
};

int main()
{
	constexpr size_t BUF_SIZE = (1 << 16);
	constexpr size_t SAMPLE_RATE = 48000;
	constexpr size_t CHANNELS = 2;
	constexpr size_t BITDEPTH = 16;
	constexpr size_t BYTES_PER_SAMPLE = CHANNELS * (BITDEPTH / 8);
	constexpr size_t FRAME_LEN = 20;
	constexpr size_t FRAME_SAMPLES = FRAME_LEN * SAMPLE_RATE / 1000;
	constexpr size_t FRAME_SIZE =
	    BYTES_PER_SAMPLE * FRAME_SAMPLES;
	constexpr size_t BLOCK_SIZE = 50 * FRAME_SIZE;

	char buf[BUF_SIZE];

	// Read raw audio data from stdin into continous memory, expects audio in
	// raw dat format, generate e.g. using ffmpeg:
	// ffmpeg -loglevel error -i <IN FILE> -ac 2 -ar 48000 -f s16le -
	std::cout << "Reading input data to memory..." << std::endl;
	size_t size;
	std::vector<char> raw_audio;
	do {
		std::cin.read(buf, BUF_SIZE);
		size = std::cin.gcount();
		raw_audio.insert(raw_audio.end(), buf, buf + size);
	} while (size == BUF_SIZE);

	// Encode 1s blocks of the audio data into individual lists of Opus frames
	std::cout << "Encoding Opus frames..." << std::endl;
	int err;
	OpusEncoder *enc = opus_encoder_create(SAMPLE_RATE, CHANNELS,
	                                       OPUS_APPLICATION_AUDIO, &err);
	opus_encoder_ctl(enc, OPUS_SET_BITRATE(192000));
	std::vector<std::vector<std::vector<char>>> blocks;
	auto cur = raw_audio.begin();
	while (cur != raw_audio.end()) {
		auto block_end = std::min(raw_audio.end(), cur + BLOCK_SIZE);

		//----------------------------------------------------------------------
		// Start a new block, reset the opus encoder to emulate opus frames
		// being encoded by a different thread or originating from a different
		// file.
		// Note: with this line, there are major disruptions in the generated
		// webm file. Without this line things seemingly work fine, but there
		// sometimes are gaps in the audio.
		opus_encoder_ctl(enc, OPUS_RESET_STATE);
		//----------------------------------------------------------------------

		std::vector<std::vector<char>> block;
		while (cur != block_end) {
			auto frame_end = std::min(block_end, cur + FRAME_SIZE);
			int size = opus_encode(
			    enc, reinterpret_cast<const int16_t *>(&(*cur)),
			    FRAME_SAMPLES, reinterpret_cast<uint8_t *>(buf), BUF_SIZE);
			if (size > 0) {
				block.emplace_back(buf, buf + size);
			}
			cur = frame_end;
		}
		blocks.emplace_back(std::move(block));
	}
	opus_encoder_destroy(enc);

	// Write the frames to a webm file
	std::cout << "Writing test.webm..." << std::endl;
	MkvWriter mkv_writer;
	mkv_writer.Open("test.webm");
	Segment mkv_segment;
	uint64_t mkv_track_id;
	AudioTrack *mkv_audio_track;

	// Initialize the mkv segment
	mkv_segment.Init(&mkv_writer);
	mkv_segment.set_mode(Segment::kLive);

	// Add a single audio track
	mkv_track_id = mkv_segment.AddAudioTrack(SAMPLE_RATE, CHANNELS, 0);
	mkv_audio_track =
	    static_cast<AudioTrack *>(mkv_segment.GetTrackByNumber(mkv_track_id));
	mkv_audio_track->set_codec_id(Tracks::kOpusCodecId);
	mkv_audio_track->set_bit_depth(16);

	// Write the Opus private data
	OpusMkvCodecPrivate private_data(CHANNELS, SAMPLE_RATE);
	mkv_audio_track->SetCodecPrivate((uint8_t *)&private_data,
	                                 sizeof(OpusMkvCodecPrivate));

	// E.g. shuffle the blocks here
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(blocks.begin(), blocks.end(), g);

	// Write the individual blocks to the mkv file
	size_t granule = 0;
	for (const auto &block : blocks) {
		for (const auto &frame : block) {
			uint64_t ts = (granule * 1000 * 1000 * 1000) / SAMPLE_RATE;
			mkv_segment.AddFrame(reinterpret_cast<const uint8_t *>(&frame[0]),
			                     frame.size(), mkv_track_id, ts, true);
			granule += FRAME_SIZE;
		}
	}
	mkv_segment.Finalize();
}
