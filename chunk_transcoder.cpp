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

#include <iostream>
#include <string>
#include <vector>

#include "chunk_transcoder.hpp"
#include "encoder.hpp"

namespace eolian {
namespace stream {
/******************************************************************************
 * Class ChunkTranscoder::Impl                                                *
 ******************************************************************************/

/**
 * Actual implementation of the ChunkTranscoder class.
 */
struct ChunkTranscoder::Impl {
	/**
	 * Callback function used for reading RAW audio data from the decoder.
	 */
	DecoderCallback decoder;

	/**
	 * Current location within the stream. Offset after all the samples that
	 * are currently in the sample buffer.
	 */
	size_t offs;

	/**
	 * Parameters describing the ChunkTranscoder.
	 */
	Settings settings;

	/**
	 * Buffer holding enough samples for exactly one chunk, including the
	 * overlaps. Such a large buffer is necessary since we don't know ahead of
	 * time whether we're actually able to read the end overlap. Hence, the
	 * entire buffer is sent to the encoder in a single pass, along with the
	 * correct metadata.
	 */
	std::vector<float> buf;

	/**
	 * Number of samples currently in the sample buffer.
	 */
	size_t buf_ptr = 0;

	/**
	 * Flag indicating whether the stream is currently at its end.
	 */
	bool at_end = false;

	Impl(DecoderCallback decoder, size_t decoder_offset,
	     const Settings &settings)
	    : decoder(decoder),
	      offs(decoder_offset),
	      settings(settings),
	      buf(settings.total_length_samples() * settings.channels(), 0.0)
	{
	}

	Impl(std::istream &is, size_t decoder_offset, const Settings &settings)
	    : Impl(DecoderCallback(), decoder_offset, settings)
	{
		std::istream *is_ptr = &is;
		this->decoder = [is_ptr, this](float *buf,
		                               size_t buf_size) mutable -> size_t {
			const size_t bytes_per_sample =
			    sizeof(float) * this->settings.channels();
			is_ptr->read(reinterpret_cast<char *>(buf),
			             buf_size * bytes_per_sample);
			return is_ptr->gcount() / bytes_per_sample;
		};
	}

	/**
	 * Calculates the current read offset including the data that is currently
	 * in the input buffer.
	 */
	size_t read_offs() const
	{
		return std::max(0L, int64_t(offs) - int64_t(buf_ptr));
	}

	bool transcode(std::ostream &os)
	{
		// If we've already reached the end, abort
		if (at_end) {
			return false;
		}

		// If the decoder is currently at an offset that is smaller than the
		// start offset of the next block, advance to the actual start offset.
		const size_t next_idx = idx();
		const size_t next_idx_offs =
		    settings.offs_for_block_idx_samples(next_idx);
		while (offs < next_idx_offs) {
			const size_t n_read = std::min(next_idx_offs - offs,
			                               buf.size() / settings.channels());
			const size_t read = decoder(buf.data(), n_read);
			offs += read;
			if (read < n_read) {
				at_end = true;
				return false;
			}

			// Discard all buffered data
			buf_ptr = 0;
		}

		// We should now be at the exact location we need to be at
		assert(read_offs() == next_idx_offs);

		// Read all remaining data, assemble the crossfade metadata
		size_t crossfade_in =
		    (next_idx_offs == 0) ? 0 : settings.overlap_samples();
		size_t crossfade_out = settings.overlap_samples();
		const size_t offs_end =
		    settings.offs_end_for_block_idx_samples(next_idx);
		const size_t n_read = offs_end - offs;
		const size_t read =
		    decoder(buf.data() + buf_ptr * settings.channels(), n_read);
		if (read < n_read) {
			crossfade_out = 0;
			at_end = true;
		}
		offs += read;

		// Encode the data as Ogg/Opus and write it to the output stream
		const size_t chunk_size_total = buf_ptr + read;
		if (chunk_size_total == 0) {
			return false;
		}
		{
			Encoder enc(os,
			            {{"CF_IN", std::to_string(crossfade_in)},
			             {"CF_OUT", std::to_string(crossfade_out)}},
			            0, settings.channels(), settings.rate());

			enc.encode(buf.data(), chunk_size_total, settings.bitrate());
		}

		// Keep the last crossfade_out samples in the buffer, adjust the buf_ptr
		// accordingly
		float *crossfade_end =
		    buf.data() + chunk_size_total * settings.channels();
		float *crossfade_start =
		    crossfade_end - crossfade_out * settings.channels();
		std::copy(crossfade_start, crossfade_end, buf.data());
		buf_ptr = crossfade_out;

		return true;
	}

	size_t idx() const
	{
		// Calculate the index based on the length of one chunk in samples.
		size_t idx = (read_offs() + settings.overlap_samples()) /
		             (settings.length_samples() + settings.overlap_samples());

		// Calculate the offset for this index in samples
		const size_t idx_offs = settings.offs_for_block_idx_samples(idx);

		// If the read offset is beyond the index at which the actual segment
		// would start, this segment cannot be produced. Increment the index by
		// one, since we can only output the next segment.
		if (read_offs() > idx_offs) {
			idx++;
		}

		return idx;
	}
};

/******************************************************************************
 * Class ChunkTranscoder                                                      *
 ******************************************************************************/

ChunkTranscoder::ChunkTranscoder(DecoderCallback decoder, size_t decoder_offset,
                                 const Settings &settings)
    : m_impl(std::make_unique<ChunkTranscoder::Impl>(decoder, decoder_offset,
                                                     settings))
{
}

ChunkTranscoder::ChunkTranscoder(std::istream &is, size_t decoder_offset,
                                 const Settings &settings)
    : m_impl(
          std::make_unique<ChunkTranscoder::Impl>(is, decoder_offset, settings))
{
}

ChunkTranscoder::~ChunkTranscoder()
{
	// Implicitly destroy the unique_ptr
}

bool ChunkTranscoder::transcode(std::ostream &os)
{
	return m_impl->transcode(os);
}

size_t ChunkTranscoder::idx() const { return m_impl->idx(); }

bool ChunkTranscoder::has_next() const { return !m_impl->at_end; }

ChunkTranscoder::Settings ChunkTranscoder::settings() const
{
	return m_impl->settings;
}
}
}
