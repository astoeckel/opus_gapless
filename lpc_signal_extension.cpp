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
#include <cmath>

#include "lpc_signal_extension.hpp"

namespace eolian {
namespace stream {

/*
 * This code is an adapted version of that found in libopusenc. See the
 * following license header for more information.
 */

/* Copyright (C)2002-2017 Jean-Marc Valin
   Copyright (C)2007-2013 Xiph.Org Foundation
   Copyright (C)2008-2013 Gregory Maxwell
   File: opusenc.c

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

static constexpr size_t LPC_PADDING = 120;
static constexpr int LPC_ORDER = 24;
static constexpr size_t LPC_INPUT = 480;
static constexpr double LPC_GOERTZEL_CONST = 2 * std::cos(M_PI / double(LPC_PADDING));

/**
 * Compile-time generation of a window function of length LPC_PADDING using a
 * resonating IIR aka Goertzel's algorithm.
 */
template<size_t N>
class GoertzelWindow {
private:
	float m_window[N];
public:
	constexpr GoertzelWindow() : m_window() {
		float m0 = 1.0f, m1 = 0.5f * LPC_GOERTZEL_CONST;
		float a1 = LPC_GOERTZEL_CONST;
		m_window[0] = 1;
		for (size_t i = 1; i < N; i++) {
			m_window[i] = a1 * m0 - m1;
			m1 = m0;
			m0 = m_window[i];
		}
		for (size_t i = 0; i < N; i++) {
			m_window[i] = 0.5 + 0.5 * m_window[i];
		}
	}

	constexpr float operator[](size_t i) const {
		return m_window[i];
	}
};
static constexpr GoertzelWindow<LPC_PADDING> LPC_WINDOW;

// Forward declarations
static void vorbis_lpc_from_data(float *data, float *lpci, int n, int stride);

void lpc_signal_extension(float *buf, size_t buf_len, size_t valid_data_length,
                          size_t channels)
{
	// Abort if there is no space left in the buffer
	if (valid_data_length >= buf_len) {
		return;
	}

	// Abort if there is not enough data in the buffer for the prediction.
	const size_t before = std::min(LPC_INPUT, valid_data_length);
	const size_t after = buf_len - valid_data_length;
	if (before < 4 * LPC_ORDER || buf_len - valid_data_length == 0) {
		std::fill_n(buf + channels * valid_data_length, 2 * after, 0.0f);
		return;
	}

	// Iterate over all channels, for each channel calculate the LPC prediction
	// coefficients, then use those coefficients to calculate the actual
	// prediciton. Apply a window function to fade out the audio.
	for (size_t c = 0; c < channels; c++) {
		float lpc[LPC_ORDER];
		const ptrdiff_t offs = (valid_data_length - before) * channels + c;
		vorbis_lpc_from_data(buf + offs, lpc, before, channels);
		for (size_t i = valid_data_length; i < buf_len; i++) {
			float sum = 0;
			for (size_t j = 0; j < LPC_ORDER; j++) {
				sum -= buf[(i - j - 1) * channels + c] * lpc[j];
			}
			buf[i * channels + c] = sum;
		}
		const size_t end = std::min<size_t>(after, LPC_ORDER);
		for (size_t i = 0; i < end; i++) {
			buf[valid_data_length + i * channels + c] *= LPC_WINDOW[i];
		}
	}
}

/* Some of these routines (autocorrelator, LPC coefficient estimator)
   are derived from code written by Jutta Degener and Carsten Bormann;
   thus we include their copyright below.  The entirety of this file
   is freely redistributable on the condition that both of these
   copyright notices are preserved without modification.  */

/* Preserved Copyright: *********************************************/

/* Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
 * Technische Universita"t Berlin
 *
 * Any use of this software is permitted provided that this notice is not
 * removed and that neither the authors nor the Technische Universita"t
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

static void vorbis_lpc_from_data(float *data, float *lpci, int n, int stride)
{
	double aut[LPC_ORDER + 1];
	double lpc[LPC_ORDER];
	double error;
	double epsilon;
	int i, j;

	/* FIXME: Apply a window to the input. */
	/* autocorrelation, p+1 lag coefficients */
	j = LPC_ORDER + 1;
	while (j--) {
		double d = 0; /* double needed for accumulator depth */
		for (i = j; i < n; i++) {
			d += (double)data[i * stride] * data[(i - j) * stride];
		}
		aut[j] = d;
	}

	/* Apply lag windowing (better than bandwidth expansion) */
	if (LPC_ORDER <= 64) {
		for (i = 1; i <= LPC_ORDER; i++) {
			/* Approximate this gaussian for low enough order. */
			/* aut[i] *= exp(-.5*(2*M_PI*.002*i)*(2*M_PI*.002*i));*/
			aut[i] -= aut[i] * (0.008f * 0.008f) * i * i;
		}
	}
	/* Generate lpc coefficients from autocorr values */

	/* set our noise floor to about -100dB */
	error = aut[0] * (1. + 1e-7);
	epsilon = 1e-6 * aut[0] + 1e-7;

	for (i = 0; i < LPC_ORDER; i++) {
		double r = -aut[i + 1];

		if (error < epsilon) {
			std::fill_n(lpc + i, LPC_ORDER - i, 0.0);
			goto done;
		}

		/* Sum up this iteration's reflection coefficient; note that in
		   Vorbis we don't save it.  If anyone wants to recycle this code
		   and needs reflection coefficients, save the results of 'r' from
		   each iteration. */

		for (j = 0; j < i; j++) {
			r -= lpc[j] * aut[i - j];
		}
		r /= error;

		/* Update LPC coefficients and total error */

		lpc[i] = r;
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

	/* slightly damp the filter */
	{
		double g = .999;
		double damp = g;
		for (j = 0; j < LPC_ORDER; j++) {
			lpc[j] *= damp;
			damp *= g;
		}
	}

	for (j = 0; j < LPC_ORDER; j++) {
		lpci[j] = (float)lpc[j];
	}
}


}
}
