/* SPDX-License-Identifier: MIT */
#ifndef HC15XX_VDEC_CODEC_H
#define HC15XX_VDEC_CODEC_H

#include "hc15xx_vdec.h"

/*
 * Codec plugin interface recovered from viddec_h264_attach in
 * libviddrv_h264dec.a.  Each codec backend allocates a context,
 * fills in this vtable, and registers with the VDEC core.
 *
 * The vendor codecs are software decoders that use the VDEC hardware
 * for DMA data movement and display output.  Context sizes recovered
 * from the malloc calls in each attach function:
 *   H.264:  0x21538 (136504 bytes)
 *   MPEG-2: ~64K (estimated from archive size ratio)
 *   MPEG-4: ~48K
 *   VC-1:   ~80K
 *   VP8:    ~32K
 *   JPEG:   ~16K
 */

struct hc15xx_vdec_codec_ops {
	int (*init)(void *ctx, const struct hc15xx_vdec_config *config);
	int (*decode)(void *ctx, const uint8_t *data, uint32_t size);
	int (*flush)(void *ctx);
	void (*close)(void *ctx);
	int (*cfg_output)(void *ctx, uintptr_t fb_addr, uint32_t stride);
};

struct hc15xx_vdec_codec {
	enum hc15xx_vdec_codec_id id;
	const char *name;
	const struct hc15xx_vdec_codec_ops *ops;
	void *ctx;
	uint32_t ctx_size;
};

/* Hardware register banks used by codec backends (HC1512/HC1513) */
#define HC15XX_VDEC_CODEC_REG_BASE0	0x18802000
#define HC15XX_VDEC_CODEC_REG_BASE1	0x18802200
#define HC15XX_VDEC_CODEC_REG_BASE2	0x18802300
#define HC15XX_VDEC_CODEC_REG_BASE3	0x18802400

int hc15xx_vdec_codec_attach(struct hc15xx_vdec *vdec,
			     struct hc15xx_vdec_codec *codec,
			     const struct hc15xx_vdec_config *config);
void hc15xx_vdec_codec_detach(struct hc15xx_vdec *vdec,
			      struct hc15xx_vdec_codec *codec);
int hc15xx_vdec_codec_decode(struct hc15xx_vdec *vdec,
			     struct hc15xx_vdec_codec *codec,
			     const uint8_t *data, uint32_t size);

/* Codec constructors - each returns a statically allocated descriptor */
struct hc15xx_vdec_codec *hc15xx_vdec_codec_h264(void);
struct hc15xx_vdec_codec *hc15xx_vdec_codec_mpeg2(void);
struct hc15xx_vdec_codec *hc15xx_vdec_codec_mpeg4(void);
struct hc15xx_vdec_codec *hc15xx_vdec_codec_vc1(void);
struct hc15xx_vdec_codec *hc15xx_vdec_codec_vp8(void);
struct hc15xx_vdec_codec *hc15xx_vdec_codec_jpeg(void);

#endif /* HC15XX_VDEC_CODEC_H */
