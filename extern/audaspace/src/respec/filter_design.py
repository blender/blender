#!/usr/bin/python

################################################################################
# Copyright 2009-2023 Jörg Müller
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

# high quality:   sinc filter coefficients, Nz = 136, L = 2304, freq = 0.963904, Kaiser Window B = 16
# medium quality: sinc filter coefficients, Nz = 42, L = 500, freq = 0.916636, Kaiser Window B = 12
# low quality:    sinc filter coefficients, Nz = 16, L = 128, freq = 0.834068, Kaiser Window B = 10

import numpy as np
import scipy

L = 2304
Nz = 136
B = 16
freq = Nz / (Nz + B / np.pi)

print(f'// sinc filter coefficients, Nz = {Nz}, L = {L}, freq = {freq:.6f}, Kaiser Window B = {B}')

Nz = Nz / freq

a = freq * np.sinc(freq * np.arange(0, Nz, 1/L))

M = len(a)*2-1

b = scipy.signal.windows.kaiser(M, B)

b = b[len(a)-1:]

y = a * b

# print filter coefficients from y
if False:
	print(f'AUD_NAMESPACE_BEGIN')
	print(f'const int JOSResampleReader::m_len_PRESET = {int(L*Nz)};')
	print(f'const int JOSResampleReader::m_L_PRESET = {L};')
	print(f'const float JOSResampleReader::m_coeff_PRESET[m_len_PRESET + 1] = {{')
	for idx, val in enumerate(y):
		print(f'{val:.9e}f', end=', ')
		if (idx + 1) % 10 == 0:
			print("\n", end='')
	print(f'}};')
	print(f'AUD_NAMESPACE_END')

# visualize filter

import matplotlib.pyplot as plt

mid = len(y)
res = np.concatenate([y[:0:-1], y])

f1 = L
f2 = 1
Fs1 = L
Fs2 = 2

area = mid - 1
t = (np.arange(1, area*2+1) - area) / (Fs1 * f2)

plt.figure()
plt.plot(t, res[mid - area:mid + area])
plt.xlim([t[0], t[-1]])
plt.ylim(np.array([np.min(res), np.max(res)]) * 1.05)
plt.xlabel('Time [s]')
plt.ylabel('Amplitude')
plt.title('Response')

fftres = np.fft.fft(res / f1)

f = np.arange(len(fftres)) * Fs2 * f1 / len(fftres)

plt.figure()
plt.plot(f, np.log10(np.abs(fftres))*20)
plt.xlim([0, Fs2])
plt.ylim([-200, 0])
plt.xlabel('Frequency [Hz]')
plt.ylabel('Magnitude [dB]')
plt.title('Magnitude')

plt.figure()
plt.plot(f, np.log10(np.abs(fftres)/np.abs(fftres[0]))*20)
plt.xlim(np.array([0, Fs2/2])*1.1)
plt.ylim([-3, 1.5])
plt.xlabel('Frequency [Hz]')
plt.ylabel('Magnitude [dB]')
plt.title('Passband')

plt.figure()
plt.plot(f, np.log10(np.abs(fftres)/np.abs(fftres[0]))*20)
plt.xlim(np.array([0.8, 1.1])*Fs2/2)
plt.ylim([-100, 6])
plt.xlabel('Frequency [Hz]')
plt.ylabel('Magnitude [dB]')
plt.title('Transition')

phi = np.angle(fftres);
phi -= (phi > np.pi / 2) * np.pi;
phi += (phi < -np.pi / 2) * np.pi;

plt.figure()
plt.plot(f, phi * 180 / np.pi)
plt.xlim([0, Fs2/2])
plt.ylim([-180, 180])
plt.xlabel('Frequency [Hz]')
plt.ylabel('Phase [deg]')
plt.title('Phase')

plt.show()
