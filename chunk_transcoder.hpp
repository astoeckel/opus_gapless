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
 * @file chunk_transcoder.hpp
 *
 * Declares the ChunkEncoder class which is used to split an audio file into
 * overlapping chunks of audio and encode these chunks as Ogg/Opus along with
 * the corresponding metadata.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>

namespace eolian {
namespace stream {
/**
 * The ChunkTranscoder class extracts overlaping chunks of audio data from a
 * RAW audio stream and encodes them as Ogg/Opus data with the correct metadata
 * describing the overlap and the overal position of the chunk within the
 * original audio stream.
 */
class ChunkTranscoder {
private:
	/**
	 * Actual implementation of the ChunkTranscoder class.
	 */
	struct Impl;
	std::unique_ptr<Impl> m_impl;

public:
	/**
	 * The Settings class conviniently stores the settings for the
	 * ChunkTranscoder and allows to compute important quantities such as the
	 * required second offset the decoder should seek to when encoding a chunk
	 * with a certain index.
	 */
	class Settings {
	private:
		size_t m_rate = 48000;
		size_t m_channels = 2;
		size_t m_bitrate = 256000;
		float m_overlap = 1.0e-3f;
		float m_length = 5.0f;

	public:
		/**
		 * Default constructor of the Settings class.
		 */
		Settings() {}

		/**
		 * Returns the sample rate used by the ChunkTranscoder in samples per
		 * second. Default value is 48000.
		 */
		size_t rate() const { return m_rate; }

		/**
		 * Sets the sample rate used by the ChunkTranscoder to the given value.
		 * Possible rate values are limited to those directly supported by the
		 * Opus codec, namely 8000, 12000, 16000, 24000, and 48000.
		 *
		 * @param rate is the new sample rate in samples per second.
		 * @return a reference at this Settings instance for function call
		 * chaining.
		 */
		Settings &rate(size_t rate)
		{
			assert(rate == 80000 || rate == 12000 || rate == 16000 ||
			       rate == 24000 || rate == 48000);
			m_rate = rate;
			return *this;
		}

		/**
		 * Returns the number of channels used by the ChunkTranscoder.
		 */
		size_t channels() const { return m_channels; }

		/**
		 * Sets the number of channels used by the ChunkTranscoder. Possible
		 * values are limited to those supported by the Opus codec -- the stream
		 * can either be mono or stereo. Default value is two.
		 *
		 * @param channels is the number of channels in the stream.
		 * @return a reference at this Settings instance for function call
		 * chaining.
		 */
		Settings &channels(size_t channels)
		{
			assert(channels >= 1 && channels <= 2);
			m_channels = channels;
			return *this;
		}

		/**
		 * Returns the bitrate in bits per second used in the encoded audio
		 * chunks. Default value is 256000 (256 kbit/s), which should guarantee
		 * a perceptually unchanged audio stream.
		 */
		size_t bitrate() const { return m_bitrate; }

		/**
		 * Sets the bitrate in bits per second that should be used in the
		 * encoded audio chunk. A per the libopus documentation the bitrate
		 * should be a value between 500 and 512000.
		 *
		 * @param bitrate is the target bitrate in bits per second.
		 * @return a reference at this Settings instance for function call
		 * chaining.
		 */
		Settings &bitrate(size_t bitrate)
		{
			assert(bitrate >= 500 && bitrate <= 512000);
			m_bitrate = bitrate;
			return *this;
		}

		/**
		 * Returns the overlap between chunks in seconds. Default value is one
		 * milliseconds (1e-3).
		 */
		float overlap() const { return m_overlap; }

		/**
		 * Returns the overlap between chunks in samples. The default values at
		 * a sampling rate of 48000 is 96.
		 */
		size_t overlap_samples() const { return m_overlap * m_rate; }

		/**
		 * Allows to set the overlap in seconds between two chunks.
		 *
		 * @param overlap is the desired overlap value in seconds. Must be a
		 * positive number.
		 * @return a reference at this Settings instance for function call
		 * chaining.
		 */
		Settings &overlap(float overlap)
		{
			assert(overlap > 0.0f);
			m_overlap = overlap;
			return *this;
		}

		/**
		 * Returns the length of one chunk in seconds. The default value is 5.0
		 * seconds.
		 */
		float length() const { return m_length; }

		/**
		 * Returns the lnegth of one chunk in samples. The default value at a
		 * sampling rate of 48000 is 240000.
		 */
		size_t length_samples() const { return m_length * m_rate; }

		/**
		 * Sets the chunk length to the given length in seconds.
		 *
		 * @param length is the length of one audio chunk in seconds. Must be
		 * a positive number.
		 * @return a reference at this Settings instance for function call
		 * chaining.
		 */
		Settings &length(float length)
		{
			assert(length > 0.0f);
			m_length = length;
			return *this;
		}

		/**
		 * Returns the offset in seconds an audio block with the given index
		 * would start at (including overlap).
		 */
		float offs_for_block_idx(size_t idx) const
		{
			return offs_for_block_idx_samples(idx) / float(m_rate);
		}

		/**
		 * Returns the offset in samples an audio block with the given index
		 * would start at (including overlap).
		 */
		size_t offs_for_block_idx_samples(size_t idx) const
		{
			return std::max<int64_t>(
			    (int64_t(m_length * m_rate) + int64_t(m_overlap * m_rate)) *
			            idx -
			        int64_t(m_overlap * m_rate),
			    0);
		}

		/**
		 * Returns the offset in seconds an audio block with the given index
		 * would end at (including overlap).
		 */
		float offs_end_for_block_idx(size_t idx) const
		{
			return offs_end_for_block_idx_samples(idx) / float(m_rate);
		}

		/**
		 * Returns the offset in samples an audio block with the given index
		 * would end at (including overlap).
		 */
		size_t offs_end_for_block_idx_samples(size_t idx) const
		{
			return std::max<int64_t>(
			    (int64_t(m_length * m_rate) + int64_t(m_overlap * m_rate)) *
			        (idx + 1),
			    0);
		}

		/**
		 * Returns the maximum total length (i.e. including the overlap at the
		 * beginning and the end) of a single chunk in seconds.
		 */
		float total_length() const { return m_length + 2 * m_overlap; }

		/**
		 * Returns the maximum total length (i.e. including the overlap at the
		 * beginning and the end) of a single chunk in samples.
		 */
		size_t total_length_samples() const
		{
			return length_samples() + 2 * overlap_samples();
		}
	};

	/**
	 * Callback called whenever data must be read from the decoder.
	 *
	 * @param buf is the buffer into which the decoder should read the data.
	 * @param buf_size is the number of (stereo) samples that should be read.
	 * @return the number of samples that have actually been read from the
	 * decoder. If the returned value is smaller than the requested number of
	 * samples, the ChunkTranscoder class assumes that the underlying stream
	 * has come to an end.
	 */
	using DecoderCallback = std::function<size_t(float *buf, size_t buf_size)>;

	/**
	 * Instantiates the ChunkTranscoder class reading RAW data from a callback
	 * function.
	 *
	 * @param decoder is a callback function that provides RAW floating point
	 * sample data.
	 * @param decoder_offset is the offset of the decoder within the underlying
	 * stream in samples. I.e. the first sample returned by the decoder read
	 * callback has the sample number specified by decoder_offset.
	 * @param settings contains all other parameters of the ChunkTranscoder,
	 * including number of channels, sampling rate, bitrate, overlap and
	 * chunk length.
	 */
	ChunkTranscoder(DecoderCallback decoder, size_t decoder_offset = 0,
	                const Settings &settings = Settings());

	/**
	 * Instantiates the ChunkTranscoder class reading RAW data from an input
	 * stream.
	 *
	 * @param decoder is a callback function that provides RAW floating point
	 * sample data.
	 * @param decoder_offset is the offset of the decoder within the underlying
	 * stream in samples. I.e. the first sample returned by the decoder read
	 * callback has the sample number specified by decoder_offset.
	 * @param settings contains all other parameters of the ChunkTranscoder,
	 * including number of channels, sampling rate, bitrate, overlap and
	 * chunk length.
	 */
	ChunkTranscoder(std::istream &is, size_t decoder_offset = 0,
	                const Settings &settings = Settings());

	/**
	 * Destructor of the ChunkTranscoder class, releases all memory held by the
	 * internal representation.
	 */
	~ChunkTranscoder();

	/**
	 * Reads the next chunk from the input stream, encodes it and writes it to
	 * the given output stream.
	 *
	 * @param os is the output stream the transcoded chunk should be written to.
	 * @return true if a valid chunk was written to the output stream, false
	 * if the decoder is already at its end.
	 */
	bool transcode(std::ostream &os);

	/**
	 * Returns the current chunk index of the transcoder. This corresponds to
	 * the index of the next chunk that is going to be returned with a call to
	 * transcode.
	 */
	size_t idx() const;

	/**
	 * Returns true if a next call to transcode is likely going to be
	 * successful. If the function returns false, calles to transcode() are
	 * guaranteed to return false.
	 */
	bool has_next() const;

	/**
	 * Returns the settings used for this ChunkTranscoder.
	 */
	Settings settings() const;
};
}
}
