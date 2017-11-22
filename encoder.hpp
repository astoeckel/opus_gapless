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
 * @file encoder.hpp
 *
 * Provides an encoder which produces standard-compliant Ogg/Opus streams from
 * RAW audio data. Input data may be  represented as float or int16 samples.
 * The encoder automatically adds lead in and lead out frames to the data using
 * linear predictive coding, which minimises distortions while crossing stream
 * boundaries.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

namespace eolian {
namespace stream {

/**
 * The Encoder encodes OGG/Opus media streams from RAW data. It is responsible
 * for encoding individual Opus frames and multiplexing them into an OGG stream.
 * Furthermore, the encoder automatically generates lead in and lead out frames
 * which allow (almost) gapless (and distortion-less) concatenation of
 * individual audio streams. Clicking noise between streams may be heard if
 * there is low-frequency content in the stream at the moment of the transition.
 * For completely distortion-less playback there should be
 */
class Encoder {
private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

public:
	/**
	 * Data structure representing key-value tag pairs that should be written
	 * to the OGG metadata header.
	 */
	using Tags = std::vector<std::tuple<std::string, std::string>>;

	/**
	 * Creates a new encoder instance and writes the stream header to the given
	 * output stream. Throws an exception if for some reason the encoder cannot
	 * be initialised (such as bad values for channels and/or rate).
	 *
	 * @param os is the output stream to which the OGG/Opus stream should be
	 * written.
	 * @param comments is a list of key/value pairs that should be written to
	 * the opus stream head.
	 * @param granule_offset is the offset of the first sample in the stream
	 * within a chain of streams.
	 * @param channels is the number of channels that are interleaved in the
	 * input data. This must either be one or two. More than two channels are
	 * currently not supported.
	 * @param rate is the sample rate, valid values are 8000, 12000, 16000,
	 * 24000 or 48000, where the latter should always be used for music
	 * encoding.
	 */
	Encoder(std::ostream &os, const Tags &tags = Tags(),
	        int64_t granule_offset = 0, size_t channels = 2,
	        size_t rate = 48000);

	/**
	 * Finalises the OGG/Opus stream. Encodes all pending opus frames and
	 * generates the lead-out frame.
	 */
	~Encoder();

	/**
	 * Number of samples constituting a single Opus frame. The Encoder class
	 * is hardcoded to produced 20ms frames, just as the libpus documentation
	 * recommends.
	 */
	size_t frame_size() const;

	/**
	 * Number of samples of latency (pre_skip) of the Opus codec. This many
	 * samples must be discarded from the decoded stream.
	 */
	size_t pre_skip() const;

	/**
	 * Sampling rate passed to the constructor.
	 */
	size_t rate() const;

	/**
	 * Encodes a chunk of floating point audio data. The number of bytes that is
	 * being read from the input buffer is
	 *
	 *      sizeof(float) * n_samples * channels
	 *
	 * Throws an exception if an unexpected error happens during encoding, such
	 * as invalid bitrate values.
	 *
	 * @param src is a pointer at a floating point buffer containing the
	 * interleaved audio data.
	 * @param n_src is the number of multi-channel samples in the buffer.
	 * @param bitrate is the taget bitrate in bits per second. Valid values are
	 * in the range between 500 to 512000. Note that this parameter only affects
	 * the next full frame that is going to be encoded. The default value of
	 * 192000 is claimed to be perceptually transparent.
	 */
	void encode(const float *src, size_t n_src, size_t bitrate = 192000);

	/**
	 * Encodes a chunk of integer 16-bit audio data. The number of bytes that
	 * is being read from the input buffer is
	 *
	 *      sizeof(int16_t) * n_samples * channels
	 *
	 * Throws an exception if an unexpected error happens during encoding, such
	 * as invalid bitrate values.
	 *
	 * @param src is a pointer at an integer buffer containing the interleaved
	 * audio data.
	 * @param n_src is the number of multi-channel samples in the buffer.
	 * @param bitrate is the taget bitrate in bits per second. Valid values are
	 * in the range between 500 to 512000. Note that this parameter only affects
	 * the next full frame that is going to be encoded. The default value of
	 * 192000 is claimed to be perceptually transparent.
	 */
	void encode(const int16_t *src, size_t n_src, size_t bitrate = 192000);
};
}
}
