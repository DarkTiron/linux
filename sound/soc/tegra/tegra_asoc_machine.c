// SPDX-License-Identifier: GPL-2.0-only
/*
 * tegra_asoc_machine.c - Universal ASoC machine driver for NVIDIA Tegra boards.
 */

#include <linux/export.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_asoc_machine.h"

/* Headphones Jack */

static struct snd_soc_jack tegra_machine_hp_jack;

static struct snd_soc_jack_pin tegra_machine_hp_jack_pins[] = {
	{ .pin = "Headphone",  .mask = SND_JACK_HEADPHONE },
	{ .pin = "Headphones", .mask = SND_JACK_HEADPHONE },
};

static struct snd_soc_jack_gpio tegra_machine_hp_jack_gpio = {
	.name = "Headphones detection",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 150,
};

/* Headset Jack */

static struct snd_soc_jack tegra_machine_headset_jack;

static struct snd_soc_jack_pin tegra_machine_headset_jack_pins[] = {
	{ .pin = "Headset Mic", .mask = SND_JACK_MICROPHONE },
	{ .pin = "Headset Stereophone", .mask = SND_JACK_HEADPHONE },
};

static struct snd_soc_jack_gpio tegra_machine_headset_jack_gpio = {
	.name = "Headset detection",
	.report = SND_JACK_HEADSET,
	.debounce_time = 150,
};

/* Mic Jack */

static struct snd_soc_jack tegra_machine_mic_jack;

static struct snd_soc_jack_pin tegra_machine_mic_jack_pins[] = {
	{ .pin = "Mic Jack",    .mask = SND_JACK_MICROPHONE },
	{ .pin = "Headset Mic", .mask = SND_JACK_MICROPHONE },
};

static struct snd_soc_jack_gpio tegra_machine_mic_jack_gpio = {
	.name = "Mic detection",
	.report = SND_JACK_MICROPHONE,
	.debounce_time = 150,
};

static int tegra_machine_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(dapm->card);

	if (machine->asoc->dapm_event) {
		int ret = machine->asoc->dapm_event(w, k, event);
		if (ret <= 0)
			return ret;
	}

	if (!strcmp(w->name, "Int Spk") || !strcmp(w->name, "Speakers"))
		gpiod_set_value_cansleep(machine->gpiod_spkr_en,
					 SND_SOC_DAPM_EVENT_ON(event));

	if (!strcmp(w->name, "Mic Jack"))
		gpiod_set_value_cansleep(machine->gpiod_ext_mic_en,
					 SND_SOC_DAPM_EVENT_ON(event));

	if (!strcmp(w->name, "Int Mic"))
		gpiod_set_value_cansleep(machine->gpiod_int_mic_en,
					 SND_SOC_DAPM_EVENT_ON(event));

	if (!strcmp(w->name, "Headphone") || !strcmp(w->name, "Headphone Jack"))
		gpiod_set_value_cansleep(machine->gpiod_hp_mute,
					 !SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static const struct snd_soc_dapm_widget tegra_machine_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", tegra_machine_event),
	SND_SOC_DAPM_HP("Headphone", tegra_machine_event),
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_SPK("Speakers", tegra_machine_event),
	SND_SOC_DAPM_SPK("Int Spk", tegra_machine_event),
	SND_SOC_DAPM_MIC("Int Mic", tegra_machine_event),
	SND_SOC_DAPM_MIC("Mic Jack", tegra_machine_event),
	SND_SOC_DAPM_MIC("Internal Mic 1", NULL),
	SND_SOC_DAPM_MIC("Internal Mic 2", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic", NULL),
	SND_SOC_DAPM_MIC("Mic", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
};

static const struct snd_kcontrol_new tegra_machine_controls[] = {
	SOC_DAPM_PIN_SWITCH("Speakers"),
	SOC_DAPM_PIN_SWITCH("Int Spk"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic 1"),
	SOC_DAPM_PIN_SWITCH("Internal Mic 2"),
};

int tegra_asoc_machine_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	int err;

	if (machine->gpiod_hp_det && machine->asoc->add_hp_jack) {
		err = snd_soc_card_jack_new(card, "Headphones Jack",
					    SND_JACK_HEADPHONE,
					    &tegra_machine_hp_jack,
					    tegra_machine_hp_jack_pins,
					    ARRAY_SIZE(tegra_machine_hp_jack_pins));
		if (err) {
			dev_err(rtd->dev,
				"Headphones Jack creation failed: %d\n", err);
			return err;
		}

		tegra_machine_hp_jack_gpio.desc = machine->gpiod_hp_det;

		err = snd_soc_jack_add_gpios(&tegra_machine_hp_jack, 1,
					     &tegra_machine_hp_jack_gpio);
		if (err)
			dev_err(rtd->dev, "HP GPIOs not added: %d\n", err);
	}

	if (machine->gpiod_hp_det && machine->asoc->add_headset_jack) {
		err = snd_soc_card_jack_new(card, "Headset Jack",
					    SND_JACK_HEADSET,
					    &tegra_machine_headset_jack,
					    tegra_machine_headset_jack_pins,
					    ARRAY_SIZE(tegra_machine_headset_jack_pins));
		if (err) {
			dev_err(rtd->dev,
				"Headset Jack creation failed: %d\n", err);
			return err;
		}

		tegra_machine_headset_jack_gpio.desc = machine->gpiod_hp_det;

		err = snd_soc_jack_add_gpios(&tegra_machine_headset_jack, 1,
					     &tegra_machine_headset_jack_gpio);
		if (err)
			dev_err(rtd->dev, "Headset GPIOs not added: %d\n", err);
	}

	if (machine->gpiod_mic_det && machine->asoc->add_mic_jack) {
		err = snd_soc_card_jack_new(rtd->card, "Mic Jack",
					    SND_JACK_MICROPHONE,
					    &tegra_machine_mic_jack,
					    tegra_machine_mic_jack_pins,
					    ARRAY_SIZE(tegra_machine_mic_jack_pins));
		if (err) {
			dev_err(rtd->dev, "Mic Jack creation failed: %d\n", err);
			return err;
		}

		tegra_machine_mic_jack_gpio.desc = machine->gpiod_mic_det;

		err = snd_soc_jack_add_gpios(&tegra_machine_mic_jack, 1,
					     &tegra_machine_mic_jack_gpio);
		if (err)
			dev_err(rtd->dev, "Mic GPIOs not added: %d\n", err);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_machine_init);

static unsigned int tegra_machine_mclk_rate_128(unsigned int srate)
{
	return 128 * srate;
}

static unsigned int tegra_machine_mclk_rate_256(unsigned int srate)
{
	return 256 * srate;
}

static unsigned int tegra_machine_mclk_rate_512(unsigned int srate)
{
	return 512 * srate;
}

static unsigned int tegra_max98090_mclk_rate(unsigned int srate)
{
	unsigned int mclk;

	switch (srate) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 12288000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 11289600;
		break;
	default:
		mclk = 12000000;
		break;
	}

	return mclk;
}

unsigned int tegra_asoc_machine_mclk_rate(unsigned int srate)
{
	unsigned int mclk;

	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		mclk = 128 * srate;
		break;
	default:
		mclk = 256 * srate;
		break;
	}
	/* FIXME: Codec only requires >= 3MHz if OSR==0 */
	while (mclk < 6000000)
		mclk *= 2;

	return mclk;
}
EXPORT_SYMBOL_GPL(tegra_asoc_machine_mclk_rate);

static int tegra_machine_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_card *card = rtd->card;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	unsigned int srate = params_rate(params);
	unsigned int mclk = machine->asoc->mclk_rate(srate);
	const unsigned int clk_id = 0;
	int err;

	err = tegra_asoc_utils_set_rate(&machine->util_data, srate, mclk);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks: %d\n", err);
		return err;
	}

	err = snd_soc_dai_set_sysclk(codec_dai, clk_id, mclk, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "codec_dai clock not set: %d\n", err);
		return err;
	}

	return 0;
}

static struct snd_soc_ops tegra_machine_ops = {
	.hw_params = tegra_machine_hw_params,
};

static void tegra_machine_node_release(void *of_node)
{
	of_node_put(of_node);
}

static struct device_node *
tegra_machine_parse_phandle(struct device *dev, const char *name)
{
	struct device_node *np;
	int err;

	np = of_parse_phandle(dev->of_node, name, 0);
	if (!np) {
		dev_err(dev, "Property '%s' missing or invalid\n", name);
		return ERR_PTR(-EINVAL);
	}

	err = devm_add_action_or_reset(dev, tegra_machine_node_release, np);
	if (err)
		return ERR_PTR(err);

	return np;
}

int tegra_asoc_machine_probe(struct platform_device *pdev)
{
	struct device_node *np_codec, *np_i2s;
	const struct tegra_asoc_data *asoc;
	struct device *dev = &pdev->dev;
	struct tegra_machine *machine;
	struct snd_soc_card *card;
	struct gpio_desc *gpiod;
	int err;

	machine = devm_kzalloc(dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	asoc = of_device_get_match_data(dev);
	card = asoc->card;
	card->dev = dev;

	machine->asoc = asoc;
	machine->mic_jack = &tegra_machine_mic_jack;
	machine->hp_jack_gpio = &tegra_machine_hp_jack_gpio;
	snd_soc_card_set_drvdata(card, machine);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,hp-mute", GPIOD_OUT_HIGH);
	machine->gpiod_hp_mute = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,hp-det", GPIOD_IN);
	machine->gpiod_hp_det = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,mic-det", GPIOD_IN);
	machine->gpiod_mic_det = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,spkr-en", GPIOD_OUT_LOW);
	machine->gpiod_spkr_en = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,int-mic-en", GPIOD_OUT_LOW);
	machine->gpiod_int_mic_en = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,ext-mic-en", GPIOD_OUT_LOW);
	machine->gpiod_ext_mic_en = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	gpiod = devm_gpiod_get_optional(dev, "nvidia,ear-sel-en", GPIOD_OUT_LOW);
	machine->gpiod_ear_sel = gpiod;
	if (IS_ERR(gpiod))
		return PTR_ERR(gpiod);

	err = snd_soc_of_parse_card_name(card, "nvidia,model");
	if (err)
		return err;

	if (!card->dapm_routes) {
		err = snd_soc_of_parse_audio_routing(card, "nvidia,audio-routing");
		if (err)
			return err;
	}

	np_codec = tegra_machine_parse_phandle(dev, "nvidia,audio-codec");
	if (IS_ERR(np_codec))
		return PTR_ERR(np_codec);

	np_i2s = tegra_machine_parse_phandle(dev, "nvidia,i2s-controller");
	if (!np_i2s)
		return PTR_ERR(np_i2s);

	card->dai_link->cpus->of_node = np_i2s;
	card->dai_link->codecs->of_node = np_codec;
	card->dai_link->platforms->of_node = np_i2s;

	if (asoc->add_common_controls) {
		card->controls = tegra_machine_controls,
		card->num_controls = ARRAY_SIZE(tegra_machine_controls);
	}

	if (asoc->add_common_dapm_widgets) {
		card->dapm_widgets = tegra_machine_dapm_widgets,
		card->num_dapm_widgets = ARRAY_SIZE(tegra_machine_dapm_widgets);
	}

	if (asoc->add_common_soc_ops)
		card->dai_link->ops = &tegra_machine_ops;

	err = tegra_asoc_utils_init(&machine->util_data, &pdev->dev);
	if (err)
		return err;

	err = devm_snd_soc_register_card(dev, card);
	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_machine_probe);

/* WM8753 machine */

SND_SOC_DAILINK_DEFS(wm8753_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "wm8753-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_wm8753_dai = {
	.name = "WM8753",
	.stream_name = "WM8753 PCM",
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(wm8753_hifi),
};

static struct snd_soc_card snd_soc_tegra_wm8753 = {
	.name = "tegra-wm8753",
	.owner = THIS_MODULE,
	.dai_link = &tegra_wm8753_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_wm8753_data = {
	.mclk_rate = tegra_max98090_mclk_rate,
	.card = &snd_soc_tegra_wm8753,
	.add_common_dapm_widgets = true,
	.add_common_soc_ops = true,
};

/* WM9712 machine */

static int tegra_wm9712_init(struct snd_soc_pcm_runtime *rtd)
{
	return snd_soc_dapm_force_enable_pin(&rtd->card->dapm, "Mic Bias");
}

SND_SOC_DAILINK_DEFS(wm9712_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC("wm9712-codec", "wm9712-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_wm9712_dai = {
	.name = "AC97 HiFi",
	.stream_name = "AC97 HiFi",
	.init = tegra_wm9712_init,
	SND_SOC_DAILINK_REG(wm9712_hifi),
};

static struct snd_soc_card snd_soc_tegra_wm9712 = {
	.name = "tegra-wm9712",
	.owner = THIS_MODULE,
	.dai_link = &tegra_wm9712_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_wm9712_data = {
	.card = &snd_soc_tegra_wm9712,
	.add_common_dapm_widgets = true,
};

/* MAX98090 machine */

SND_SOC_DAILINK_DEFS(max98090_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "HiFi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_max98090_dai = {
	.name = "max98090",
	.stream_name = "max98090 PCM",
	.init = tegra_asoc_machine_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(max98090_hifi),
};

static struct snd_soc_card snd_soc_tegra_max98090 = {
	.name = "tegra-max98090",
	.owner = THIS_MODULE,
	.dai_link = &tegra_max98090_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_max98090_data = {
	.mclk_rate = tegra_max98090_mclk_rate,
	.card = &snd_soc_tegra_max98090,
	.add_common_dapm_widgets = true,
	.add_common_controls = true,
	.add_common_soc_ops = true,
	.add_mic_jack = true,
	.add_hp_jack = true,
};

/* SGTL5000 machine */

SND_SOC_DAILINK_DEFS(sgtl5000_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "sgtl5000")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_sgtl5000_dai = {
	.name = "sgtl5000",
	.stream_name = "HiFi",
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(sgtl5000_hifi),
};

static struct snd_soc_card snd_soc_tegra_sgtl5000 = {
	.name = "tegra-sgtl5000",
	.owner = THIS_MODULE,
	.dai_link = &tegra_sgtl5000_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_sgtl5000_data = {
	.mclk_rate = tegra_max98090_mclk_rate,
	.card = &snd_soc_tegra_sgtl5000,
	.add_common_dapm_widgets = true,
	.add_common_soc_ops = true,
};

/* TLV320AIC23 machine */

static const struct snd_soc_dapm_widget trimslice_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Line Out", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
};

static const struct snd_soc_dapm_route trimslice_audio_map[] = {
	{"Line Out", NULL, "LOUT"},
	{"Line Out", NULL, "ROUT"},

	{"LLINEIN", NULL, "Line In"},
	{"RLINEIN", NULL, "Line In"},
};

SND_SOC_DAILINK_DEFS(tlv320aic23_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "tlv320aic23-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_tlv320aic23_dai = {
	.name = "TLV320AIC23",
	.stream_name = "AIC23",
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(tlv320aic23_hifi),
};

static struct snd_soc_card snd_soc_tegra_trimslice = {
	.name = "tegra-trimslice",
	.owner = THIS_MODULE,
	.dai_link = &tegra_tlv320aic23_dai,
	.num_links = 1,
	.dapm_widgets = trimslice_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(trimslice_dapm_widgets),
	.dapm_routes = trimslice_audio_map,
	.num_dapm_routes = ARRAY_SIZE(trimslice_audio_map),
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_trimslice_data = {
	.mclk_rate = tegra_machine_mclk_rate_128,
	.card = &snd_soc_tegra_trimslice,
	.add_common_soc_ops = true,
};

/* RT5677 machine */

static int tegra_rt5677_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	int err;

	err = tegra_asoc_machine_init(rtd);
	if (err)
		return err;

	snd_soc_dapm_force_enable_pin(&card->dapm, "MICBIAS1");

	return 0;
}

SND_SOC_DAILINK_DEFS(rt5677_aif1,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "rt5677-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_rt5677_dai = {
	.name = "RT5677",
	.stream_name = "RT5677 PCM",
	.init = tegra_rt5677_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(rt5677_aif1),
};

static struct snd_soc_card snd_soc_tegra_rt5677 = {
	.name = "tegra-rt5677",
	.owner = THIS_MODULE,
	.dai_link = &tegra_rt5677_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_rt5677_data = {
	.mclk_rate = tegra_machine_mclk_rate_256,
	.card = &snd_soc_tegra_rt5677,
	.add_common_dapm_widgets = true,
	.add_common_controls = true,
	.add_common_soc_ops = true,
	.add_mic_jack = true,
	.add_hp_jack = true,
};

/* RT5640 machine */

SND_SOC_DAILINK_DEFS(rt5640_aif1,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "rt5640-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_rt5640_dai = {
	.name = "RT5640",
	.stream_name = "RT5640 PCM",
	.init = tegra_asoc_machine_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(rt5640_aif1),
};

static struct snd_soc_card snd_soc_tegra_rt5640 = {
	.name = "tegra-rt5640",
	.owner = THIS_MODULE,
	.dai_link = &tegra_rt5640_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_rt5640_data = {
	.mclk_rate = tegra_machine_mclk_rate_256,
	.card = &snd_soc_tegra_rt5640,
	.add_common_dapm_widgets = true,
	.add_common_controls = true,
	.add_common_soc_ops = true,
	.add_hp_jack = true,
};

/* RT5632 machine */

SND_SOC_DAILINK_DEFS(rt5632_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "alc5632-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_rt5632_dai = {
	.name = "ALC5632",
	.stream_name = "ALC5632 PCM",
	.init = tegra_rt5677_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(rt5632_hifi),
};

static struct snd_soc_card snd_soc_tegra_rt5632 = {
	.name = "tegra-alc5632",
	.owner = THIS_MODULE,
	.dai_link = &tegra_rt5632_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_rt5632_data = {
	.mclk_rate = tegra_machine_mclk_rate_512,
	.card = &snd_soc_tegra_rt5632,
	.add_common_dapm_widgets = true,
	.add_common_controls = true,
	.add_common_soc_ops = true,
	.add_headset_jack = true,
};

/* RT5631 machine */

SND_SOC_DAILINK_DEFS(rt5631_hifi,
	DAILINK_COMP_ARRAY(COMP_EMPTY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(NULL, "rt5631-hifi")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link tegra_rt5631_dai = {
	.name = "RT5631",
	.stream_name = "RT5631 PCM",
	.init = tegra_asoc_machine_init,
	.dai_fmt = SND_SOC_DAIFMT_I2S |
		   SND_SOC_DAIFMT_NB_NF |
		   SND_SOC_DAIFMT_CBS_CFS,
	SND_SOC_DAILINK_REG(rt5631_hifi),
};

static struct snd_soc_card snd_soc_tegra_rt5631 = {
	.name = "tegra-rt5631",
	.owner = THIS_MODULE,
	.dai_link = &tegra_rt5631_dai,
	.num_links = 1,
	.fully_routed = true,
};

static const struct tegra_asoc_data tegra_rt5631_data = {
	.mclk_rate = tegra_asoc_machine_mclk_rate,
	.card = &snd_soc_tegra_rt5631,
	.add_common_dapm_widgets = true,
	.add_common_controls = true,
	.add_common_soc_ops = true,
	.add_hp_jack = true,
};

static const struct of_device_id tegra_machine_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-trimslice", .data = &tegra_trimslice_data },
	{ .compatible = "nvidia,tegra-audio-max98090", .data = &tegra_max98090_data },
	{ .compatible = "nvidia,tegra-audio-sgtl5000", .data = &tegra_sgtl5000_data },
	{ .compatible = "nvidia,tegra-audio-wm9712", .data = &tegra_wm9712_data },
	{ .compatible = "nvidia,tegra-audio-wm8753", .data = &tegra_wm8753_data },
	{ .compatible = "nvidia,tegra-audio-rt5677", .data = &tegra_rt5677_data },
	{ .compatible = "nvidia,tegra-audio-rt5640", .data = &tegra_rt5640_data },
	{ .compatible = "nvidia,tegra-audio-alc5632", .data = &tegra_rt5632_data },
	{ .compatible = "nvidia,tegra-audio-rt5631", .data = &tegra_rt5631_data },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_machine_of_match);

static struct platform_driver tegra_asoc_machine_driver = {
	.driver = {
		.name = "tegra-audio",
		.of_match_table = tegra_machine_of_match,
		.pm = &snd_soc_pm_ops,
	},
	.probe = tegra_asoc_machine_probe,
};
module_platform_driver(tegra_asoc_machine_driver);

MODULE_AUTHOR("Anatol Pomozov <anatol@google.com>");
MODULE_AUTHOR("Andrey Danin <danindrey@mail.ru>");
MODULE_AUTHOR("Dmitry Osipenko <digetx@gmail.com>");
MODULE_AUTHOR("Ion Agorria <ion@agorria.com>");
MODULE_AUTHOR("Leon Romanovsky <leon@leon.nu>");
MODULE_AUTHOR("Lucas Stach <dev@lynxeye.de>");
MODULE_AUTHOR("Marc Dietrich <marvin24@gmx.de>");
MODULE_AUTHOR("Marcel Ziswiler <marcel@ziswiler.com>");
MODULE_AUTHOR("Mike Rapoport <mike@compulab.co.il>");
MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_AUTHOR("Svyatoslav Ryhel <clamor95@gmail.com>");
MODULE_DESCRIPTION("Tegra machine ASoC driver");
MODULE_LICENSE("GPL");
