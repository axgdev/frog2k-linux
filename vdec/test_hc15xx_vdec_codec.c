// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hc15xx_vdec_codec.h"

#define REG_COUNT 1024
#define SYS_REG_COUNT 256

struct fake {
	uint32_t regs[REG_COUNT];
	uint32_t sys_regs[SYS_REG_COUNT];
	unsigned int delay_count;
	unsigned int flush_count;
	unsigned int invalidate_count;
	struct hc15xx_vdec_io io;
};

static uint32_t fake_read32(void *cookie, uintptr_t address)
{
	struct fake *f = cookie;

	if (address >= 0x18810000 && address < 0x18810000 + REG_COUNT * 4)
		return f->regs[(address - 0x18810000) / 4];
	if (address >= 0x18800000 && address < 0x18800000 + SYS_REG_COUNT * 4)
		return f->sys_regs[(address - 0x18800000) / 4];
	return 0;
}

static void fake_write32(void *cookie, uintptr_t address, uint32_t value)
{
	struct fake *f = cookie;

	if (address >= 0x18810000 && address < 0x18810000 + REG_COUNT * 4)
		f->regs[(address - 0x18810000) / 4] = value;
	else if (address >= 0x18800000 && address < 0x18800000 + SYS_REG_COUNT * 4)
		f->sys_regs[(address - 0x18800000) / 4] = value;
}

static void fake_delay_us(void *cookie, unsigned int usec)
{
	struct fake *f = cookie;

	(void)usec;
	f->delay_count++;
}

static void fake_cache_flush(void *cookie, uintptr_t address, size_t len)
{
	struct fake *f = cookie;

	(void)address;
	(void)len;
	f->flush_count++;
}

static void fake_cache_invalidate(void *cookie, uintptr_t address, size_t len)
{
	struct fake *f = cookie;

	(void)address;
	(void)len;
	f->invalidate_count++;
}

static void setup(struct hc15xx_vdec *vdec, struct fake *f)
{
	memset(f, 0, sizeof(*f));
	f->io.cookie = f;
	f->io.read32 = fake_read32;
	f->io.write32 = fake_write32;
	f->io.delay_us = fake_delay_us;
	f->io.cache_flush = fake_cache_flush;
	f->io.cache_invalidate = fake_cache_invalidate;
	f->sys_regs[HC15XX_SYS_REG_CHIP_ID / 4] = 0x1512a501;
	hc15xx_vdec_init(vdec, &f->io, 0x18810000, 0x18800000);
}

static void test_codec_attach_h264(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_codec *codec;
	struct hc15xx_vdec_config config;
	int ret;

	setup(&vdec, &f);
	codec = hc15xx_vdec_codec_h264();
	assert(codec != NULL);
	assert(codec->id == HC15XX_VDEC_CODEC_H264);
	assert(strcmp(codec->name, "h264") == 0);
	assert(codec->ctx_size == 0x21538);

	memset(&config, 0, sizeof(config));
	config.codec = HC15XX_VDEC_CODEC_H264;
	config.width = 1920;
	config.height = 1080;
	config.frame_rate = 30;

	ret = hc15xx_vdec_codec_attach(&vdec, codec, &config);
	assert(ret == 0);

	hc15xx_vdec_codec_detach(&vdec, codec);
	printf("test_codec_attach_h264: PASS\n");
}

static void test_codec_attach_all(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_codec *codecs[6];
	struct hc15xx_vdec_config config;
	int ret;
	unsigned int i;

	setup(&vdec, &f);
	codecs[0] = hc15xx_vdec_codec_h264();
	codecs[1] = hc15xx_vdec_codec_mpeg2();
	codecs[2] = hc15xx_vdec_codec_mpeg4();
	codecs[3] = hc15xx_vdec_codec_vc1();
	codecs[4] = hc15xx_vdec_codec_vp8();
	codecs[5] = hc15xx_vdec_codec_jpeg();

	for (i = 0; i < 6; i++) {
		assert(codecs[i] != NULL);
		memset(&config, 0, sizeof(config));
		config.codec = codecs[i]->id;
		config.width = 320;
		config.height = 240;
		config.frame_rate = 25;

		ret = hc15xx_vdec_codec_attach(&vdec, codecs[i], &config);
		assert(ret == 0);
		hc15xx_vdec_codec_detach(&vdec, codecs[i]);
	}

	printf("test_codec_attach_all: PASS\n");
}

static void test_codec_decode(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_codec *codec;
	struct hc15xx_vdec_config config;
	uint8_t frame[256];
	int ret;

	setup(&vdec, &f);
	codec = hc15xx_vdec_codec_jpeg();

	memset(&config, 0, sizeof(config));
	config.codec = HC15XX_VDEC_CODEC_JPEG;
	config.width = 320;
	config.height = 240;
	config.output_buf = 0x02000000;
	config.output_buf_size = 320 * 240 * 2;

	ret = hc15xx_vdec_codec_attach(&vdec, codec, &config);
	assert(ret == 0);

	/* Simulate hardware completing decode */
	f.regs[HC15XX_VDEC_REG_STATUS2 / 4] = HC15XX_VDEC_STATUS_DONE;

	memset(frame, 0xAA, sizeof(frame));
	ret = hc15xx_vdec_codec_decode(&vdec, codec, frame, sizeof(frame));
	assert(ret == 0);
	assert(vdec.frames_decoded == 1);

	hc15xx_vdec_codec_detach(&vdec, codec);
	printf("test_codec_decode: PASS\n");
}

static void test_codec_decode_not_attached(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_codec *codec;
	uint8_t frame[64];
	int ret;

	setup(&vdec, &f);
	codec = hc15xx_vdec_codec_h264();

	/* Don't attach - decode should fail */
	memset(frame, 0, sizeof(frame));
	ret = hc15xx_vdec_codec_decode(&vdec, codec, frame, sizeof(frame));
	assert(ret == -1);

	printf("test_codec_decode_not_attached: PASS\n");
}

static void test_codec_null_ops(void)
{
	struct fake f;
	struct hc15xx_vdec vdec;
	struct hc15xx_vdec_codec codec;
	struct hc15xx_vdec_config config;
	int ret;

	setup(&vdec, &f);
	memset(&codec, 0, sizeof(codec));
	memset(&config, 0, sizeof(config));

	ret = hc15xx_vdec_codec_attach(&vdec, &codec, &config);
	assert(ret == -1);

	printf("test_codec_null_ops: PASS\n");
}

int main(void)
{
	test_codec_attach_h264();
	test_codec_attach_all();
	test_codec_decode();
	test_codec_decode_not_attached();
	test_codec_null_ops();

	printf("vdec-codec: all tests passed\n");
	return 0;
}
