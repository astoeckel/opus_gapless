/*
 *  EOLIAN Web Music Player
 *  Copyright (C) 2017  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file ogg_opus_muxer.hpp
 *
 * Implements a minimal multiplexer which encapsulates Opus frames in an Ogg
 * stream.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>

namespace eolian {
namespace stream {

/**
 * The OggOpusMuxer class packs a set of Opus frames into an Ogg stream along
 * with meta information required by the Opus decoder.
 */
class OggOpusMuxer {
private:
	/**
	 * Actual implementation of the OggOpusMuxer.
	 */
	class Impl;
	std::unique_ptr<Impl> m_impl;

public:
	/**
	 * Starts writing the Ogg bitstream by writing the header information for
	 * a single contained Opus audio stream with the given data.
	 *
	 * @param pre_skip is the number of samples that should be discarded at the
	 * beginning of the stream.
	 * @param channel_count is the number of channels contained in the Opus
	 * audio stream.
	 * @param sample_rate is the sample rate of the Opus audio stream.
	 * @param gain is a gain factor that should be applied to the audio data.
	 */
	OggOpusMuxer(std::ostream &os, uint16_t pre_skip, uint8_t channel_count = 2,
	         uint32_t sample_rate = 48000);

	/**
	 * Writes an Opus frame into the Ogg bitstream.
	 *
	 * @param last set to true if this is the last frame being written.
	 * @param granule number of samples in the stream after the end of this
	 * frame.
	 * @param buf is a pointer at the byte-buffer containing the Opus frame that
	 * should be written into the Ogg bitstream.
	 * @param len is the length of the byte buffer that should be written.
	 */
	void write_frame(bool last, int64_t granule, const uint8_t *buf,
	                 size_t len);

	/**
	 * Finalises the Ogg bitstream.
	 */
	~OggOpusMuxer();
};
}
}
