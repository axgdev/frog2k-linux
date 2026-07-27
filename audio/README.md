# HC15xx audio source driver

`hc15xx_audio.c` is a freestanding output-path replacement for the relevant
parts of the vendor `libauddrv.a`. It has no Linux or RTOS dependencies:
adapters supply MMIO reads/writes, a microsecond delay, and a DMA visibility
barrier.

The implemented path covers APLL reset, PWM DAC setup, 32 kHz mono S16 I2S,
volume, cyclic DMA setup, producer publication, underrun restart, interrupt
acknowledgement, start, and stop. The register sequences were recovered from
the SF2000 vendor archive rather than guessed.

DMA cursors and ring length use 16-byte units, while the period register uses
PCM frames. This mixed-unit contract follows `i2so_platform_hw_params`
exactly.

One non-obvious hardware rule is essential: the two 16-byte DMA cursors have
no separate full flag, so equal cursors mean empty. The vendor driver reserves
64 bytes (128 in its mode 2). The source core preserves that guard and later
publishes the deferred tail once the consumer moves.

Linux owns policy (ALSA buffering, GPIO amplifier mute, locking and IRQ
routing). HCRTOS/FreeRTOS can use this core with small platform callbacks and
keep their existing ASoC-facing API while replacing the closed output path.
