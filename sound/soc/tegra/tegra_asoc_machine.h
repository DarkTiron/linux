// SPDX-License-Identifier: GPL-2.0-only

#ifndef __TEGRA_ASOC_MACHINE_H__
#define __TEGRA_ASOC_MACHINE_H__

#include "tegra_asoc_utils.h"

struct gpio_desc;
struct snd_kcontrol;
struct snd_soc_card;
struct snd_soc_jack;
struct platform_device;
struct snd_soc_jack_gpio;
struct snd_soc_pcm_runtime;
struct snd_soc_dapm_widget;

struct tegra_asoc_data {
	int (*dapm_event)(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *k, int event);
	unsigned int (*mclk_rate)(unsigned int srate);
	struct snd_soc_card *card;
	bool add_common_dapm_widgets;
	bool add_common_controls;
	bool add_common_soc_ops;
	bool add_headset_jack;
	bool add_mic_jack;
	bool add_hp_jack;
};

struct tegra_machine {
	struct tegra_asoc_utils_data util_data;
	const struct tegra_asoc_data *asoc;
	struct gpio_desc *gpiod_ext_mic_en;
	struct gpio_desc *gpiod_int_mic_en;
	struct gpio_desc *gpiod_spkr_en;
	struct gpio_desc *gpiod_mic_det;
	struct gpio_desc *gpiod_ear_sel;
	struct gpio_desc *gpiod_hp_mute;
	struct gpio_desc *gpiod_hp_det;
	struct snd_soc_jack *mic_jack;
	struct snd_soc_jack_gpio *hp_jack_gpio;
};

int tegra_asoc_machine_probe(struct platform_device *pdev);
int tegra_asoc_machine_init(struct snd_soc_pcm_runtime *rtd);
unsigned int tegra_asoc_machine_mclk_rate(unsigned int srate);

#endif
