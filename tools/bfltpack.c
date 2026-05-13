/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BFLT_HDR_SIZE 64u
#define BFLT_VERSION 4u
#define BFLT_FLAG_RAM 1u
#define BFLT_FLAG_KTRACE 0x10u

static void put_be32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static int copy_file(FILE *out, FILE *in)
{
	unsigned char buf[4096];

	for (;;) {
		size_t n = fread(buf, 1, sizeof(buf), in);

		if (n && fwrite(buf, 1, n, out) != n)
			return -1;
		if (n < sizeof(buf)) {
			if (ferror(in))
				return -1;
			return 0;
		}
	}
}

int main(int argc, char **argv)
{
	const char *in_path;
	const char *out_path;
	unsigned char hdr[BFLT_HDR_SIZE];
	FILE *in;
	FILE *out;
	long raw_size;
	uint32_t data_start;

	if (argc != 3) {
		fprintf(stderr, "usage: %s raw.bin init.bflt\n", argv[0]);
		return 2;
	}

	in_path = argv[1];
	out_path = argv[2];
	in = fopen(in_path, "rb");
	if (!in) {
		fprintf(stderr, "%s: %s\n", in_path, strerror(errno));
		return 1;
	}
	if (fseek(in, 0, SEEK_END) || (raw_size = ftell(in)) < 0 ||
	    fseek(in, 0, SEEK_SET)) {
		fprintf(stderr, "%s: cannot determine size\n", in_path);
		fclose(in);
		return 1;
	}
	if (raw_size > 0x00ffff00L) {
		fprintf(stderr, "%s: raw image too large\n", in_path);
		fclose(in);
		return 1;
	}

	out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "%s: %s\n", out_path, strerror(errno));
		fclose(in);
		return 1;
	}

	memset(hdr, 0, sizeof(hdr));
	memcpy(hdr, "bFLT", 4);
	data_start = BFLT_HDR_SIZE + (uint32_t)raw_size;
	put_be32(hdr + 4, BFLT_VERSION);
	put_be32(hdr + 8, BFLT_HDR_SIZE);
	put_be32(hdr + 12, data_start);
	put_be32(hdr + 16, data_start);
	put_be32(hdr + 20, data_start);
	put_be32(hdr + 24, 4096);
	put_be32(hdr + 28, data_start);
	put_be32(hdr + 32, 0);
	put_be32(hdr + 36, BFLT_FLAG_RAM | BFLT_FLAG_KTRACE);

	if (fwrite(hdr, 1, sizeof(hdr), out) != sizeof(hdr) ||
	    copy_file(out, in)) {
		fprintf(stderr, "%s: write failed\n", out_path);
		fclose(out);
		fclose(in);
		return 1;
	}

	fclose(out);
	fclose(in);
	return 0;
}

