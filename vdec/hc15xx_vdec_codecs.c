// SPDX-License-Identifier: MIT
/*
 * Codec backend stubs for HC15xx VDEC.
 *
 * The vendor codec archives (libviddrv_h264dec.a, etc.) contain full
 * software decoders: bitstream parsing, entropy decoding, motion
 * compensation, and reconstruction are all done in MIPS software.
 * The VDEC hardware provides DMA data movement and display output.
 *
 * Each codec's attach function (recovered from disassembly) performs:
 * 1. dev_alloc() with a codec-specific ops table
 * 2. malloc() for the codec context (H.264: 136KB, VP8: ~32KB, etc.)
 * 3. hc_sys_get_chip_id() check for HC1512/HC1513
 * 4. Hardware register bank setup at 0x18802000-0x18802400
 * 5. Function pointer table fill (init/decode/flush/close/cfg_output)
 * 6. dev_register() to register with the VDEC device framework
 *
 * These stubs implement the hardware register setup and interface.
 * For actual decoding, integrate an open-source codec library.
 */
#include <string.h>
#include "hc15xx_vdec_codec.h"

struct codec_ctx {
	struct hc15xx_vdec_config config;
	uintptr_t reg_base[4];
	int initialized;
	uint32_t frames_decoded;
};

static struct codec_ctx h264_ctx;
static struct codec_ctx mpeg2_ctx;
static struct codec_ctx mpeg4_ctx;
static struct codec_ctx vc1_ctx;
static struct codec_ctx vp8_ctx;
static struct codec_ctx jpeg_ctx;

static int codec_init(void *ctx, const struct hc15xx_vdec_config *config)
{
	struct codec_ctx *c = ctx;

	memset(c, 0, sizeof(*c));
	c->config = *config;
	c->reg_base[0] = HC15XX_VDEC_CODEC_REG_BASE0;
	c->reg_base[1] = HC15XX_VDEC_CODEC_REG_BASE1;
	c->reg_base[2] = HC15XX_VDEC_CODEC_REG_BASE2;
	c->reg_base[3] = HC15XX_VDEC_CODEC_REG_BASE3;
	c->initialized = 1;
	return 0;
}

static int codec_decode(void *ctx, const uint8_t *data, uint32_t size)
{
	struct codec_ctx *c = ctx;

	(void)data;
	(void)size;

	if (!c->initialized)
		return -1;

	c->frames_decoded++;
	return 0;
}

static int codec_flush(void *ctx)
{
	struct codec_ctx *c = ctx;

	if (!c->initialized)
		return -1;
	return 0;
}

static void codec_close(void *ctx)
{
	struct codec_ctx *c = ctx;

	c->initialized = 0;
}

static int codec_cfg_output(void *ctx, uintptr_t fb_addr, uint32_t stride)
{
	struct codec_ctx *c = ctx;

	(void)fb_addr;
	(void)stride;

	if (!c->initialized)
		return -1;
	return 0;
}

static const struct hc15xx_vdec_codec_ops codec_ops = {
	.init = codec_init,
	.decode = codec_decode,
	.flush = codec_flush,
	.close = codec_close,
	.cfg_output = codec_cfg_output,
};

static struct hc15xx_vdec_codec h264_codec = {
	.id = HC15XX_VDEC_CODEC_H264,
	.name = "h264",
	.ops = &codec_ops,
	.ctx = &h264_ctx,
	.ctx_size = 0x21538,
};

static struct hc15xx_vdec_codec mpeg2_codec = {
	.id = HC15XX_VDEC_CODEC_MPEG2,
	.name = "mpeg2",
	.ops = &codec_ops,
	.ctx = &mpeg2_ctx,
	.ctx_size = 0x10000,
};

static struct hc15xx_vdec_codec mpeg4_codec = {
	.id = HC15XX_VDEC_CODEC_MPEG4,
	.name = "mpeg4",
	.ops = &codec_ops,
	.ctx = &mpeg4_ctx,
	.ctx_size = 0xc000,
};

static struct hc15xx_vdec_codec vc1_codec = {
	.id = HC15XX_VDEC_CODEC_VC1,
	.name = "vc1",
	.ops = &codec_ops,
	.ctx = &vc1_ctx,
	.ctx_size = 0x14000,
};

static struct hc15xx_vdec_codec vp8_codec = {
	.id = HC15XX_VDEC_CODEC_VP8,
	.name = "vp8",
	.ops = &codec_ops,
	.ctx = &vp8_ctx,
	.ctx_size = 0x8000,
};

static struct hc15xx_vdec_codec jpeg_codec = {
	.id = HC15XX_VDEC_CODEC_JPEG,
	.name = "jpeg",
	.ops = &codec_ops,
	.ctx = &jpeg_ctx,
	.ctx_size = 0x4000,
};

struct hc15xx_vdec_codec *hc15xx_vdec_codec_h264(void)
{
	return &h264_codec;
}

struct hc15xx_vdec_codec *hc15xx_vdec_codec_mpeg2(void)
{
	return &mpeg2_codec;
}

struct hc15xx_vdec_codec *hc15xx_vdec_codec_mpeg4(void)
{
	return &mpeg4_codec;
}

struct hc15xx_vdec_codec *hc15xx_vdec_codec_vc1(void)
{
	return &vc1_codec;
}

struct hc15xx_vdec_codec *hc15xx_vdec_codec_vp8(void)
{
	return &vp8_codec;
}

struct hc15xx_vdec_codec *hc15xx_vdec_codec_jpeg(void)
{
	return &jpeg_codec;
}
