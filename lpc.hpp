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
 * @file lpc.hpp
 *
 * Implements Linear Predictive Coding (LPC). This is used to naturally extend
 * audio signals without introducing new frequency content. The code used here
 * is based on an implementation by Jutta Degener and Carsten Bormann as found
 * in libopusenc.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace eolian {
namespace stream {
/**
 * The LinearPredictiveCoding class represents the convolution coefficients of a
 * linear predictive coder, as well as functions to extract these coefficients
 * from a chunk of audio data and to predict audio given these coefficients.
 */
class LinearPredictiveCoder {
private:
	/**
	 * Number of coefficients to use in the filter.
	 */
	static constexpr size_t LPC_ORDER = 24;

	/**
	 * Vector used to store the LPC coefficients.
	 */
	alignas(16) float m_coeffs[LPC_ORDER];

	template <typename T>
	void predict_impl(const T *src_samples, size_t n_src_samples,
	                  T *tar_samples, size_t n_tar_samples,
	                  size_t stride) const;

public:
	/**
	 * Returns a pointer at the extracted LPC coefficient buffer. The number of
	 * elements is equal to the order of the predictor.
	 */
	const float *coeffs() const { return m_coeffs; }

	/**
	 * Returns the order of the LPC, which corresponds to the number of
	 * coefficients used for the linear predictive coding.
	 */
	static constexpr size_t order() { return LPC_ORDER; }

	/**
	 * Extracts the LPC coefficients from a chunk of audio data represented as
	 * floating point numbers.
	 *
	 * @param samples is a pointer at the first audio sample in the buffer from
	 * which the predictor coefficient should be extracted.
	 * @param n_samples is the number of samples that should be extracted.
	 * @param stride is the number of interleaved samples. Usually this is the
	 * number of audio channels in a certain signal.
	 */
	void extract_coefficients(const float *samples, size_t n_samples,
	                          size_t stride = 1);

	/**
	 * Extracts the LPC coefficients from a chunk of audio data represented as
	 * signed 16-bit integers.
	 *
	 * @param samples is a pointer at the first audio sample in the buffer from
	 * which the predictor coefficient should be extracted.
	 * @param n_samples is the number of samples that should be extracted.
	 * @param stride is the number of interleaved samples. Usually this is the
	 * number of audio channels in a certain signal.
	 */
	void extract_coefficients(const int16_t *samples, size_t n_samples,
	                          size_t stride = 1);

	/**
	 * Predicts an audio signal from the given floating point source signal
	 * segment and writes the predicted signal to the given buffer.
	 *
	 * @param src_samples is a pointer at the first source sample that should be
	 * the basis for the prediction.
	 * @param n_src_samples is the number of samples in the source buffer.
	 * @param tar_samples is a pointer at the last sample in the source buffer.
	 * @param stride is the number of interleaved samples in both the input and
	 * output buffer. Usually this is the number of audio channels in a certain
	 * signal.
	 */
	void predict(const float *src_samples, size_t n_src_samples,
	             float *tar_samples, size_t n_tar_samples,
	             size_t stride = 1) const;

	/**
	 * Predicts an audio signal from the given integer point source signal
	 * segment and writes the predicted signal to the given buffer.
	 *
	 * @param src_samples is a pointer at the first source sample that should be
	 * the basis for the prediction.
	 * @param n_src_samples is the number of samples in the source buffer.
	 * @param tar_samples is a pointer at the last sample in the source buffer.
	 * @param stride is the number of interleaved samples in both the input and
	 * output buffer. Usually this is the number of audio channels in a certain
	 * signal.
	 */
	void predict(const int16_t *src_samples, size_t n_src_samples,
	             int16_t *tar_samples, size_t n_tar_samples,
	             size_t stride = 1) const;
};
}
}
