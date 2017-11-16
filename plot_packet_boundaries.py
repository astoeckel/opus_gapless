#!/usr/bin/env python3

import sys
import numpy as np
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2)
pre_skip = 0#960#312# + 1 * 960
post_skip = 0#960 - 312

offs = 0
for fn in sys.argv[1:]:
	print(fn)
	data = np.fromfile(fn, dtype=np.int16, sep='')
	print(data.shape)
	data = data.reshape(-1, 2)

	ts = np.arange(offs - pre_skip, offs - pre_skip + data.shape[0]) / 48000

	t0 = offs / 48000

	axs[0].plot([t0, t0], [-30000, 30000], 'k-', linewidth=1, zorder=-1)
	axs[1].plot([t0, t0], [-30000, 30000], 'k-', linewidth=1, zorder=-1)
	axs[0].plot(ts, data[:, 0])
	axs[1].plot(ts, data[:, 1])
	offs += data.shape[0] - post_skip - pre_skip

for t in np.arange(0, offs, 960) / 48000:
	axs[0].plot([t, t], [-15000, 15000], 'k--', linewidth=0.5, zorder=-2)
	axs[1].plot([t, t], [-15000, 15000], 'k--', linewidth=0.5, zorder=-2)

plt.show()
