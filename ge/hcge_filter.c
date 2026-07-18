/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "ge_api.h"

#include <math.h>

short extract_coef(const short *coefficients, int phases, int taps,
		   int phase, int tap)
{
	(void)taps;
	return coefficients[(taps - 1 - tap) * phases + phase];
}

void extract_phase(short *output, const short *coefficients, int phases,
		   int taps, int phase)
{
	int tap;

	for (tap = 0; tap < taps; ++tap)
		output[tap] = coefficients[(taps - 1 - tap) * phases + phase];
}

static double hcge_sinc(double value)
{
	const double pi = 3.14159265358979323846;
	double angle;

	if (value == 0.0)
		return 1.0;
	angle = pi * value;
	return sin(angle) / angle;
}

static long hcge_round_even(double value)
{
	long integer = (long)value;
	double fraction = value - integer;

	if (fraction > 0.5 || (fraction == 0.5 && (integer & 1)))
		++integer;
	else if (fraction < -0.5 || (fraction == -0.5 && (integer & 1)))
		--integer;
	return integer;
}

static void hcge_firls(double *coefficients, double cutoff, int length)
{
	double amplitude[4] = { 1.0, 1.0, 0.0, 0.0 };
	double bound[4] = { 0.0, cutoff * 0.5, cutoff * 0.5, 0.5 };
	double center = 0.0;
	int half = length / 2, odd = length & 1, band, i;

	for (i = 0; i < length; ++i)
		coefficients[i] = 0.0;
	for (band = 0; band < 4; band += 2) {
		double slope = (amplitude[band + 1] - amplitude[band]) /
			(bound[band + 1] - bound[band]);
		double intercept = amplitude[band] - slope * bound[band];
		if (odd)
			center += intercept * (bound[band + 1] - bound[band]) +
				slope * 0.5 * (bound[band + 1] * bound[band + 1] -
				bound[band] * bound[band]);
		for (i = 0; i < half; ++i) {
			double k = i + (odd ? 1.0 : 0.5);
			double hi = bound[band + 1], lo = bound[band];
			double value = slope * (cos(2.0 * 3.14159265358979323846 * k * hi) -
				cos(2.0 * 3.14159265358979323846 * k * lo)) /
				(4.0 * 3.14159265358979323846 * 3.14159265358979323846 * k * k);
			value += hi * (slope * hi + intercept) * hcge_sinc(2.0 * k * hi);
			value -= lo * (slope * lo + intercept) * hcge_sinc(2.0 * k * lo);
			coefficients[length / 2 + (odd ? 1 : 0) + i] += value;
		}
	}
	if (odd) {
		coefficients[length / 2] = center;
		for (i = 0; i < half; ++i)
			coefficients[length / 2 - 1 - i] =
				coefficients[length / 2 + 1 + i];
	} else {
		for (i = 0; i < half; ++i)
			coefficients[(length - 1) / 2 - i] = coefficients[length / 2 + i];
	}
	for (i = 0; i < length; ++i)
		coefficients[i] *= 2.0;
}

void designfilter(int srcsampling, int dstsampling, int nphase, int ntap,
		  int centered, double cutoffscale, int nfracbits,
		  double *preal, short *pquant)
{
	int phase, tap, length, scale;
	double cutoff;

	if (!preal || !pquant || srcsampling <= 0 || dstsampling <= 0 ||
	    nphase <= 0 || ntap <= 0 || nfracbits < 0 || nfracbits > 14)
		return;
	length = nphase * ntap + !!centered;
	cutoff = 1.0 / nphase;
	if (dstsampling < srcsampling)
		cutoff *= (double)dstsampling / srcsampling;
	cutoff *= cutoffscale;
	hcge_firls(preal, cutoff, length);
	for (tap = 0; tap < length; ++tap)
		preal[tap] *= hcge_sinc(2.0 * tap / (length - 1) - 1.0);
	scale = 1 << nfracbits;
	for (phase = 0; phase < nphase; ++phase) {
		double sum = 0.0;
		int qsum = 0, best = phase;
		for (tap = 0; tap < ntap; ++tap)
			sum += preal[tap * nphase + phase];
		if (sum == 0.0)
			sum = 1.0;
		for (tap = 0; tap < ntap; ++tap) {
			int index = tap * nphase + phase;
			preal[index] /= sum;
			pquant[index] = (short)hcge_round_even(preal[index] * scale);
			qsum += pquant[index];
			if (preal[index] > preal[best])
				best = index;
		}
		pquant[best] += scale - qsum;
	}
}

void designfilterff(int srcsampling, int dstsampling, int nphase, int ntap,
		    int centered, int cutoffscale, int nfracbits,
		    int *preal, short *pquant)
{
	double real[256];
	int length, i;

	if (!preal || nphase <= 0 || ntap <= 0 ||
	    nphase * ntap + !!centered > (int)(sizeof(real) / sizeof(real[0])))
		return;
	length = nphase * ntap + !!centered;
	designfilter(srcsampling, dstsampling, nphase, ntap, centered,
		     (double)cutoffscale / 65536.0, nfracbits, real, pquant);
	for (i = 0; i < length; ++i)
		preal[i] = (int)hcge_round_even(real[i] * 65536.0);
}
