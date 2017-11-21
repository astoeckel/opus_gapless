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

#include <cmath>
#include <limits>

#include "lpc.hpp"

namespace eolian {
namespace stream {
/******************************************************************************
 * Helper class used to extract scaling factors from audio data types         *
 ******************************************************************************/

namespace {
template <typename T>
struct Scale {
	static constexpr bool T_is_int = std::numeric_limits<T>::is_integer;
	static constexpr size_t T_bits = sizeof(T) * 8;
	static constexpr double s = T_is_int ? (1 << (T_bits - 1)) : 1.0;
	static constexpr double is = 1.0 / s;
	static constexpr double iss = 1.0 / (s * s);
};
}

/******************************************************************************
 * External Code vorbis_lpc_from_data                                         *
 ******************************************************************************/

/*
 * The code is an adapted version of code found in libopusenc, which in turn
 * was derived from code with the following copyright notice. When copying this
 * file, please preserve this notice.
 */

/* Preserved Copyright: ***********************************************
 *
 * Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
 * Technische Universität Berlin
 *
 * Any use of this software is permitted provided that this notice is not
 * removed and that neither the authors nor the Technische Universität
 * Berlin are deemed to have made any representations as to the
 * suitability of this software for any purpose nor are held responsible
 * for any defects of this software. THERE IS ABSOLUTELY NO WARRANTY FOR
 * THIS SOFTWARE.
 *
 * As a matter of courtesy, the authors request to be informed about uses
 * this software has found, about bugs in this software, and about any
 * improvements that may be of general interest.
 * Berlin, 28.11.1994
 * Jutta Degener
 * Carsten Bormann
 *
 *********************************************************************/

template <typename T>
static void lpc_coefficients_from_data(float *lpcf, const T *src, size_t n_src,
                                       size_t stride)
{
	constexpr size_t order = LinearPredictiveCoder::order();
	alignas(16) double lpc[order];
	alignas(16) double aut[order + 1];
	double error;
	double epsilon;

	// FIXME: Apply a window to the input.
	// autocorrelation, p+1 lag coefficients
	{
		int j = order + 1;
		while (j--) {
			double d = 0; /* double needed for accumulator depth */
			for (size_t i = j; i < n_src; i++) {
				d += (double)src[i * stride] * (double)src[(i - j) * stride] *
				     Scale<T>::iss;
			}
			aut[j] = d;
		}
	}

	// Apply lag windowing (better than bandwidth expansion)
	if (order <= 64) {
		for (size_t i = 1; i <= order; i++) {
			// Approximate this gaussian for low enough order.
			// aut[i] *= exp(-.5*(2*M_PI*.002*i)*(2*M_PI*.002*i));
			aut[i] -= aut[i] * (0.008f * 0.008f) * i * i;
		}
	}

	// Generate lpc coefficients from autocorr values

	// Set our noise floor to about -100dB
	error = aut[0] * (1. + 1e-7);
	epsilon = 1e-6 * aut[0] + 1e-7;

	for (size_t i = 0; i < order; i++) {
		double r = -aut[i + 1];

		if (error < epsilon) {
			for (size_t j = i; j < order; j++) {
				lpc[j] = 0.0;
			}
			goto done;
		}

		// Sum up this iteration's reflection coefficient; note that in
		// Vorbis we don't save it.  If anyone wants to recycle this code
		// and needs reflection coefficients, save the results of 'r' from
		// each iteration.
		for (size_t j = 0; j < i; j++) {
			r -= lpc[j] * aut[i - j];
		}
		r /= error;

		// Update LPC coefficients and total error.
		lpc[i] = r;
		size_t j;
		for (j = 0; j < i / 2; j++) {
			double tmp = lpc[j];

			lpc[j] += r * lpc[i - 1 - j];
			lpc[i - 1 - j] += r * tmp;
		}
		if (i & 1) {
			lpc[j] += lpc[j] * r;
		}

		error *= 1. - r * r;
	}

done:

	// Slightly dampen the filter
	{
		double g = .999;
		double damp = g;
		for (size_t j = 0; j < order; j++) {
			lpcf[j] = lpc[j] * damp;
			damp *= g;
		}
	}
}

/******************************************************************************
 * Class LinearPredictiveCoder                                                *
 ******************************************************************************/

void LinearPredictiveCoder::extract_coefficients(const float *samples,
                                                 size_t n_samples,
                                                 size_t stride)
{
	lpc_coefficients_from_data(m_coeffs, samples, n_samples, stride);
}

void LinearPredictiveCoder::extract_coefficients(const int16_t *samples,
                                                 size_t n_samples,
                                                 size_t stride)
{
	lpc_coefficients_from_data(m_coeffs, samples, n_samples, stride);
}

template <typename T>
void LinearPredictiveCoder::predict_impl(const T *src_samples,
                                         size_t n_src_samples, T *tar_samples,
                                         size_t n_tar_samples,
                                         size_t stride) const
{
	// Fill the target memory with zeros
	for (size_t i = 0; i < n_tar_samples; i++) {
		tar_samples[i * stride] = 0;
	}

	// Function used for uniform access to the n-th source sample in the
	// convolution
	auto read = [&](size_t i, size_t j) -> double {
		ptrdiff_t idx = i - j - 1;
		if (idx >= 0) {
			return double(tar_samples[idx * stride]) * Scale<T>::is;
		}
		else {
			idx += n_src_samples;
			if (idx >= 0) {
				return double(src_samples[idx * stride]) * Scale<T>::is;
			}
		}
		return 0.0;
	};

	// Iterate over all target samples, convolve with the previous target/source
	// samples
	for (size_t i = 0; i < n_tar_samples; i++) {
		double sum = 0.0;
		for (size_t j = 0; j < order(); j++) {
			sum -= read(i, j) * m_coeffs[j];
		}
		tar_samples[i * stride] = sum * Scale<T>::s;
	}
}

void LinearPredictiveCoder::predict(const float *src_samples,
                                    size_t n_src_samples, float *tar_samples,
                                    size_t n_tar_samples, size_t stride) const
{
	predict_impl(src_samples, n_src_samples, tar_samples, n_tar_samples,
	             stride);
}

void LinearPredictiveCoder::predict(const int16_t *src_samples,
                                    size_t n_src_samples, int16_t *tar_samples,
                                    size_t n_tar_samples, size_t stride) const
{
	predict_impl(src_samples, n_src_samples, tar_samples, n_tar_samples,
	             stride);
}
}
}
