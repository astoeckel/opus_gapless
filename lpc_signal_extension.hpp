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
 * @file lpc_signal_extension.hpp
 *
 * Implements the generation of lead-out frames using linear predictive coding
 * (LPC). The code is based on code found in libopusenc. See .cpp file for
 * licensing information.
 *
 * @author Andreas Stöckel
 */

#pragma once

#include <cstddef>

namespace eolian {
namespace stream {
/**
 * Predicts how data in the given buffer might continue and fills empty space
 * in the buffer with a corresponding prediction. This function is used to
 * generate lead-out frames without introducing any transients/high-frequency
 * content in the encoded data.
 *
 * @param buf is a pointer at first frame of the the interleaved audio data.
 * @param buf_len is the total length of the buffer in samples (where one
 * sample includes all channels, e.g. buf_len = 1 and channels = 2 corresponds
 * to 8 bytes of data).
 * @param valid_data_length is the number of samples in the input buffer that
 * are valid, e.g. that should not be extended and that should be used to
 * perform the prediction.
 * @param channels is the number of channels.
 */
void lpc_signal_extension(float *buf, size_t buf_len, size_t valid_data_length,
                          size_t channels);
}
}
