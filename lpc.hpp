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
#include <vector>

namespace eolian {
namespace stream {

/**
 * The LinearPredictiveCoding class represents the convolution coefficients of a
 * linear predictive coder, as well as functions to extract these coefficients
 * from a chunk of audio data and to predict audio given these coefficients.
 */
class LinearPredictiveCoding {
private:
	/**
	 * Number of coefficients used in this particular linear predictive coder.
	 */
	size_t m_order;

	/**
	 * Vector used to store the LPC coefficients.
	 */
	std::vector<double> m_coeffs;

public:
	/**
	 * Creates a new LinearPredictiveCoding instance of the given order.
	 *
	 * @param order is the number of coefficients used in the linear predictive
	 * coder.
	 */
	LinearPredictiveCoding(size_t order = 24);

	/**
	 * Returns the extracted LPC coefficients.
	 */
	const std::vector<double> &coeffs() const {return m_coeffs;}

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
	void extract_coefficients_float(const float *samples, size_t n_samples,
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
	void predict_float(const float *src_samples, size_t n_src_samples,
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
	             int16_t *tar_samples, size_t n_tar_samples, size_t stride = 1) const;
};
}
}
