// SPDX-License-Identifier: MIT
#include "hc15xx_vdec_codec.h"

int hc15xx_vdec_codec_attach(struct hc15xx_vdec *vdec,
			     struct hc15xx_vdec_codec *codec,
			     const struct hc15xx_vdec_config *config)
{
	int ret;

	if (!codec || !codec->ops || !codec->ops->init)
		return -1;

	ret = codec->ops->init(codec->ctx, config);
	if (ret != 0)
		return ret;

	/* Configure the VDEC hardware for this codec */
	ret = hc15xx_vdec_configure(vdec, config);
	if (ret != 0)
		return ret;

	return 0;
}

void hc15xx_vdec_codec_detach(struct hc15xx_vdec *vdec,
			      struct hc15xx_vdec_codec *codec)
{
	(void)vdec;

	if (codec && codec->ops && codec->ops->close)
		codec->ops->close(codec->ctx);
}

int hc15xx_vdec_codec_decode(struct hc15xx_vdec *vdec,
			     struct hc15xx_vdec_codec *codec,
			     const uint8_t *data, uint32_t size)
{
	int ret;

	if (!codec || !codec->ops || !codec->ops->decode)
		return -1;

	/* Software decode: parse bitstream and reconstruct frame */
	ret = codec->ops->decode(codec->ctx, data, size);
	if (ret != 0)
		return ret;

	/* Hardware DMA: move decoded frame to display buffer */
	ret = hc15xx_vdec_decode_frame(vdec, (uintptr_t)data, size);
	return ret;
}
