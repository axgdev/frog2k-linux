/*
 * mktestwav - Generate a 1-second 440 Hz stereo WAV file for player testing.
 * Output: 44100 Hz, 2 channels, S16_LE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 44100
#define DURATION_S  1
#define FREQUENCY   440.0
#define AMPLITUDE   16000

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = v & 0xff;
	p[1] = v >> 8;
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = v >> 24;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "test-tone.wav";
	uint32_t num_samples = SAMPLE_RATE * DURATION_S;
	uint32_t data_size = num_samples * 2 * 2; /* stereo S16 */
	uint32_t file_size = 36 + data_size;
	uint8_t header[44];
	FILE *f;
	uint32_t i;

	f = fopen(path, "wb");
	if (!f) {
		perror(path);
		return 1;
	}

	memset(header, 0, sizeof(header));
	memcpy(header, "RIFF", 4);
	put32(header + 4, file_size);
	memcpy(header + 8, "WAVE", 4);
	memcpy(header + 12, "fmt ", 4);
	put32(header + 16, 16);
	put16(header + 20, 1); /* PCM */
	put16(header + 22, 2); /* stereo */
	put32(header + 24, SAMPLE_RATE);
	put32(header + 28, SAMPLE_RATE * 2 * 2); /* byte rate */
	put16(header + 32, 4); /* block align */
	put16(header + 34, 16); /* bits per sample */
	memcpy(header + 36, "data", 4);
	put32(header + 40, data_size);
	fwrite(header, 1, sizeof(header), f);

	for (i = 0; i < num_samples; i++) {
		double t = (double)i / SAMPLE_RATE;
		int16_t sample = (int16_t)(AMPLITUDE * sin(2.0 * M_PI * FREQUENCY * t));
		uint8_t frame[4];

		put16(frame, (uint16_t)sample);
		put16(frame + 2, (uint16_t)sample);
		fwrite(frame, 1, 4, f);
	}

	fclose(f);
	printf("wrote %s (%u samples, %u bytes)\n", path, num_samples, data_size + 44);
	return 0;
}
