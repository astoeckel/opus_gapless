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

#include <algorithm>
#include <cassert>
#include <exception>
#include <iostream>
#include <limits>

#include <opus/opus.h>

#include "encoder.hpp"
#include "lpc.hpp"
#include "ogg_opus_muxer.hpp"

namespace eolian {
namespace stream {
/******************************************************************************
 * Class OpusEncoderError                                                     *
 ******************************************************************************/

/**
 * The OpusEncoderError class is used to signal error conditions in the Opus
 * encoder class.
 */
class OpusEncoderError : public std::runtime_error {
private:
	/**
	 * Translates the given Opus encoder error code to an error message.
	 *
	 * @param err is the Opus error code that should be translated to an error
	 * message.
	 * @return a textual description of the error code.
	 */
	static const char *msg(int err)
	{
		switch (err) {
			case OPUS_ALLOC_FAIL:
				return "OPUS_ALLOC_FAIL: Memory allocation has failed.";
			case OPUS_BAD_ARG:
				return "OPUS_BAD_ARG: One or more invalid/out of range "
				       "arguments.";
			case OPUS_BUFFER_TOO_SMALL:
				return "OPUS_BUFFER_TOO_SMALL: Not enough bytes allocated in "
				       "the buffer.";
			case OPUS_INTERNAL_ERROR:
				return "OPUS_INTERNAL_ERROR: An internal error was detected.";
			case OPUS_INVALID_PACKET:
				return "OPUS_INVALID_PACKET: The compressed data passed is "
				       "corrupted.";
			case OPUS_INVALID_STATE:
				return "OPUS_INVALID_STATE: An encoder or decoder structure is "
				       "invalid or already freed.";
			case OPUS_OK:
				return "OPUS_OK: No error.";
			case OPUS_UNIMPLEMENTED:
				return "OPUS_UNIMPLEMENTED: Invalid/unsupported request "
				       "number.";
			default:
				return "Unknown error.";
		}
	}

public:
	/**
	 * Creates a new OpusEncoderError instance representing the given Opus
	 * error code.
	 *
	 * @param err is the error returned by the OpusEncoderError class.
	 */
	OpusEncoderError(int err) : OpusEncoderError(msg(err)) {}

	/**
	 * Creates a new OpusEncoderError class representing an error that was not
	 * triggered by the Opus encoder itself.
	 *
	 * @param msg is the error message that should be displayed.
	 */
	OpusEncoderError(const char *msg) : std::runtime_error(msg) {}
};

/******************************************************************************
 * Class OpusEncoderContainer                                                 *
 ******************************************************************************/

/**
 * The OpusEncoderContainer class is a minimal object-oriented RAII wrapper
 * around the C opus_encoder API.
 */
class OpusEncoderContainer {
private:
	/**
	 * Private instance of the OpusEncoder.
	 */
	mutable OpusEncoder *m_enc;

public:
	/**
	 * Creates a new OpusEncoderContainer instance.
	 *
	 * @param rate is the sample rate. Must be either 8000, 12000, 16000, 24000,
	 * or 48000.
	 * @param channels is the number of channels. Must be one or two.
	 * @param application is the coding mode that should be used.
	 */
	OpusEncoderContainer(int32_t rate = 48000, int channels = 2,
	                     int application = OPUS_APPLICATION_AUDIO)
	    : m_enc(nullptr)
	{
		// Instantiate a new opus encoder instance.
		int err;
		m_enc = opus_encoder_create(rate, channels, application, &err);
		if (!m_enc || err) {
			throw OpusEncoderError(err);
		}
	}

	/**
	 * Frees the contained OpusEncoder instance.
	 */
	~OpusEncoderContainer()
	{
		if (m_enc) {
			opus_encoder_destroy(m_enc);
			m_enc = nullptr;
		}
	}

	/**
	 * Sets the desired bitrate.
	 */
	void bitrate(size_t bitrate)
	{
		int err = opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
		if (err) {
			throw OpusEncoderError(err);
		}
	}

	/**
	 * Encodes the given floating point buffer as a single opus frame. Throws an
	 * exception if an error happens during encoding, e.g. invalid frame size,
	 * or the output buffer being too small.
	 *
	 * @param pcm is a pointer at the memory region containing the float data
	 * that should be encoded.
	 * @param frame_size is the number of samples in the frame that should be
	 * encoded.
	 * @param data is the target buffer into which the encoded data should be
	 * written.
	 * @param max_data_bytes is the size of the target buffer in bytes.
	 */
	size_t encode(const float *pcm, int frame_size, unsigned char *data,
	              int32_t max_data_bytes)
	{
		const int res =
		    opus_encode_float(m_enc, pcm, frame_size, data, max_data_bytes);
		if (res < 0) {
			throw OpusEncoderError(res);
		}
		return res;
	}

	/**
	 * Returns the name of the libopus version.
	 */
	static const char *version_string() { return opus_get_version_string(); }

	/**
	 * Returns the number of samples that should be skipped by the decoder at
	 * the beginning of the decoded stream and that must (at least) be appended
	 * to the end of the stream for the decoder to be able to recover all
	 * encoded audio data.
	 */
	size_t pre_skip() const
	{
		opus_int32 lookahead = 0;
		opus_encoder_ctl(m_enc, OPUS_GET_LOOKAHEAD(&lookahead));
		return lookahead;
	}
};

/******************************************************************************
 * Struct Enocder::Impl                                                       *
 ******************************************************************************/

/**
 * Actual implementation of the Encoder class.
 */
struct Encoder::Impl {
	/**
	 * Scale factor for the conversion of int16_t audio samples to float.
	 */
	static constexpr float INT16_SCALE = 1.0f / float(1 << 15);

	/**
	 * Number of coefficients used by the linear predictive coder.
	 */
	static constexpr size_t LPC_ORDER = 24;

	/**
	 * Number of bytes in the buffer used for the Opus encoder. Size is chosen
	 * such that 20ms of raw stero audio frames could be held in the buffer at
	 * 48000. The Opus encoded audio should be smaller than that.
	 */
	static constexpr size_t ENC_BUF_SIZE = 4096;

	/**
	 * Number of floats in the temporary sample buffer. Corresponds to the
	 * number
	 * of floats in a 20ms frame at 48000 Hz rounded to the next power of two.
	 */
	static constexpr size_t RAW_BUF_SIZE = 2048;

	/**
	 * Number of floats in the LPC/lead-in/out buffer. The number of float is
	 * chosen as twice the maximum window size rounded to the next power of two.
	 */
	static constexpr size_t LPC_BUF_SIZE = 4096;

	/**
	 * Buffer used for storing encoded Opus frame data.
	 */
	alignas(16) uint8_t enc_buf[ENC_BUF_SIZE];

	/**
	 * Buffer holding RAW sample data that was not used to fill an entire frame.
	 */
	alignas(16) float buf[RAW_BUF_SIZE];

	/**
	 * Buffer used for storing lpc data, as well as lead in/lead out buffers.
	 */
	alignas(16) float lpc_buf[LPC_BUF_SIZE];

	/**
	 * Multiplier used to translate between granule/pre_skip values in samples
	 * to the values used in the Ogg stream, which are always w.r.t. a 48000
	 * samples/s time basis.
	 */
	int granule_mul;

	/**
	 * LPC instances used for predictive coding.
	 */
	LinearPredictiveCoder lpc;

	/**
	 * Instance of the opus encoder.
	 */
	OpusEncoderContainer enc;

	/**
	 * Instance of the OggOpusMuxer class used to encapsulate the Opus
	 * frame in an Ogg/Opus transport stream.
	 */
	OggOpusMuxer muxer;

	/**
	 * Current element in any of the internal buffers.
	 */
	size_t buf_ptr = 0, lpc_buf_ptr = 0;

	/**
	 * Total sample count including the samples in the sample buffer.
	 */
	int64_t granule = 0;

	/**
	 * Number of samples that must be added to the end of the stream to account
	 * for the latency (pre_skip in Ogg) of the Opus encoder.
	 */
	size_t final_padding = 0;

	/**
	 * Number of channels being encoded.
	 */
	size_t channels = 0;

	/**
	 * Sample rate.
	 */
	size_t rate = 0;

	/**
	 * Current bitrate being used.
	 */
	size_t current_bitrate = 0;

	/**
	 * Flag indicating whether the first frame has been issued.
	 */
	bool first = true;

	Impl(std::ostream &os, const Tags &tags, int64_t granule_offset,
	     size_t channels, size_t rate)
	    : granule_mul(48000 / rate),
	      enc(rate, channels),
	      muxer(os, granule_mul * (frame_size(rate) + enc.pre_skip()),
	            enc.version_string(), tags, channels, rate),
	      granule(granule_offset),
	      final_padding(enc.pre_skip()),
	      channels(channels),
	      rate(rate)
	{
		// Make sure there are at most two channels
		if (channels > 2) {
			throw OpusEncoderError(
			    "Encoder does not support more than two channels");
		}

		// Make sure all internal buffers are of sufficient size
		assert(ENC_BUF_SIZE >= (frame_size() * channels * 2));
		assert(RAW_BUF_SIZE >= (frame_size() * channels));
		assert(LPC_BUF_SIZE >= (frame_size() * channels * 2));
	}

	~Impl()
	{
		// Encode any data that is still in the input buffer, append an extra
		// frame if there was not enough space to allow the decoder to
		// compensate for the pre_skip
		const bool needs_extra_frame =
		    (frame_size() - buf_ptr) < enc.pre_skip();
		encode_frame(buf, buf_ptr, needs_extra_frame, !needs_extra_frame);
		if (needs_extra_frame) {
			encode_frame(nullptr, 0, false, true);
		}
	}

	/**
	 * Returns the number of samples in a frame given the specified rate. The
	 * frame size is fixed to 20ms.
	 */
	static constexpr size_t frame_size(size_t rate)
	{
		return (20 * rate) / 1000;
	}

	/**
	 * Returns the number of samples in a frame.
	 */
	size_t frame_size() const { return frame_size(rate); }

	/**
	 * Instructs the Opus encoder to use the given bitrate.
	 *
	 * @param bitrate is the target bitrate in kbit/s.
	 */
	void bitrate(size_t bitrate)
	{
		if (bitrate != current_bitrate) {
			enc.bitrate(bitrate);
			current_bitrate = bitrate;
		}
	}

	/**
	 * Encodes a single Opus frame. Inserts either a lead-in or lead-out frame
	 * depending on whether
	 *
	 * @param src is a pointer at the buffer containing the data that should be
	 * encoded/appended to the buffer.
	 * @param n_src is the number of samples available in the buffer. If
	 * this value is smaller than frame_size() this indicates that this is the
	 * last frame in the stream. The encoder will automatically fill the frame
	 * with data predicted from the linear predictive coder.
	 * @param last_in_seq if true, this function will copy the given frame data
	 * to a temporary buffer such that the LPC coefficients can be extracted
	 * should they be needed.
	 * @param flush if set to true, the output stream will be marked as closed
	 * after this frame.
	 */
	void encode_frame(const float *src, size_t n_src, bool last_in_seq,
	                  bool flush = false)
	{
		// Some shorthands
		const size_t fs = frame_size();
		const size_t lpc_fs = fs / 2;

		// Make sure the input size is at most the framesize
		assert(n_src <= fs);

		// If this is the first frame that is ever being encoded, produce a
		// lead-in frame. This frame leads up to the actual sample data and must
		// be discarded by the decoder (along with the encoder lookahead).
		if (first) {
			first = false;

			// Copy the input data to a temporary buffer, reverse the buffer
			// such that a reverse LPC can be performed (predicting the unkown
			// past)
			size_t n_lpc_src = lpc_fs;
			size_t n_lpc_tar = fs;
			float *lpc_src = lpc_buf + lpc_fs * channels;
			float *lpc_tar = lpc_buf + fs * channels;
			float *lpc_buf_end = lpc_buf + 2 * fs * channels;

			std::fill(lpc_buf, lpc_buf_end, 0.0f);
			std::copy(src, src + n_src * channels, lpc_buf);
			std::reverse(lpc_buf, lpc_buf + fs * channels);

			// Extract the LPC coefficients for the reversed buffer and create
			// a prediction of the unkown past.
			for (size_t i = 0; i < channels; i++) {
				lpc.extract_coefficients(lpc_src + i, n_lpc_src, channels);
				lpc.predict(lpc_src + i, n_lpc_src, lpc_tar + i, n_lpc_tar,
				            channels);
			}

			// Reverse the prediction and encode it as frame
			std::reverse(lpc_tar, lpc_buf_end);
			encode_frame(lpc_tar, fs, false);
		}

		// Advance the granule position before potentially padding the frame
		granule += n_src;

		// If the number of samples is smaller than the frame size, fill the
		// remaining data with a linear prediction, add the missing pre_skip
		// to the granule.
		if (n_src < fs) {
			// Append the given input data to the current LPC buffer
			float *lpc_new_data_src = lpc_buf + lpc_buf_ptr * channels;
			std::copy(src, src + n_src * channels, lpc_new_data_src);
			lpc_buf_ptr += n_src;

			// Predict how the data will continue using LPC
			size_t n_lpc_src = std::min(lpc_fs, lpc_buf_ptr);
			size_t n_lpc_tar = fs - n_src;
			float *lpc_src = lpc_buf + (lpc_buf_ptr - n_lpc_src) * channels;
			float *lpc_tar = lpc_src + n_lpc_src * channels;
			for (size_t i = 0; i < channels; i++) {
				lpc.extract_coefficients(lpc_src + i, n_lpc_src, channels);
				lpc.predict(lpc_src + i, n_lpc_src, lpc_tar + i, n_lpc_tar,
				            channels);
			}

			// Make sure that pre_skip samples of the padding are actually
			// decoded. How many of these samples have already been accounted
			// for is stored in final_padding.
			size_t add_granule = std::min(final_padding, fs - n_src);
			granule += add_granule;
			final_padding -= add_granule;

			// Pretend this was just a normal frame. Note that the std::copy
			// below will still work, since lpc_buf < lpc_tar
			src = lpc_new_data_src;
			n_src = fs;
		}

		if (last_in_seq) {
			// LPC buffer to be able to extract the LPC coefficients in the case
			// the stream ends with the next frame and padding information is
			// required
			std::copy(src, src + n_src * channels, lpc_buf);
			lpc_buf_ptr = n_src;
		}

		// Encode the frame and multiplex it into the output stream
		size_t size = enc.encode(src, fs, enc_buf, ENC_BUF_SIZE);
		muxer.write_frame(flush, granule * granule_mul, enc_buf, size);
	}

	/**
	 * Chunks the given sample buffer into individual frames and stores data
	 * that has not been encoded in a temporary buffer.
	 *
	 * @param src is a pointer at the buffer containing the data that should be
	 * encoded/appended to the buffer.
	 * @param n_src is the number of samples available in the buffer.
	 */
	void encode(const float *src, size_t n_src)
	{
		const size_t fs = frame_size();
		while (n_src > 0) {
			// Extract one frame of input data, either by directly encoding from
			// the given source buffer or by filling the sample buffer.
			const size_t n_read = std::min(fs - buf_ptr, n_src);
			if (n_read == fs) {
				encode_frame(src, fs, n_src < fs);
			}
			else {
				std::copy(src, src + n_src * channels,
				          buf + buf_ptr * channels);
				buf_ptr += n_read;
				if (buf_ptr == fs) {
					encode_frame(buf, fs, n_src < fs);
					buf_ptr = 0;
				}
			}

			// Advance the source pointers
			src += channels * n_read;
			n_src -= n_read;
		}
	}

	/**
	 * Chunks the given sample buffer into individual frames and stores data
	 * that has not been encoded in a temporary buffer.
	 *
	 * @param src is a pointer at the buffer containing the data that should be
	 * encoded/appended to the buffer.
	 * @param n_src is the number of samples available in the buffer.
	 */
	void encode(const int16_t *src, size_t n_src)
	{
		const size_t fs = frame_size();
		while (n_src > 0) {
			// Fill the internal input buffer with one frame of input data
			const size_t n_read = std::min(fs - buf_ptr, n_src);
			for (size_t i = 0; i < n_read; i++, buf_ptr++) {
				for (size_t j = 0; j < channels; j++) {
					buf[buf_ptr * channels + j] =
					    float(src[i * channels + j]) * INT16_SCALE;
				}
			}

			// Advance the source pointers
			src += channels * n_read;
			n_src -= n_read;

			// If the internal buffer is full, encode the buffer as Opus frame
			if (buf_ptr == fs) {
				encode_frame(buf, fs, n_src < fs);
				buf_ptr = 0;
			}
		}
	}
};

/******************************************************************************
 * Class Encoder                                                              *
 ******************************************************************************/

Encoder::Encoder(std::ostream &os, const Tags &tags, int64_t granule_offset,
                 size_t channels, size_t rate)
    : m_impl(std::make_unique<Impl>(os, tags, granule_offset, channels, rate))
{
}

Encoder::~Encoder()
{
	// Implicitly destroy m_impl
}

size_t Encoder::frame_size() const { return m_impl->frame_size(); }

size_t Encoder::pre_skip() const { return m_impl->enc.pre_skip(); }

size_t Encoder::rate() const { return m_impl->rate; }

void Encoder::encode(const float *src, size_t n_src, size_t bitrate)
{
	m_impl->bitrate(bitrate);
	m_impl->encode(src, n_src);
}

void Encoder::encode(const int16_t *src, size_t n_src, size_t bitrate)
{
	m_impl->bitrate(bitrate);
	m_impl->encode(src, n_src);
}
}
}
