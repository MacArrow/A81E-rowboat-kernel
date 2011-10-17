/*
 * omap3cirrus.c  --  SoC audio for OMAP3 / Cirrus platform
 *
 * Author: Georgi Vlaev, Nucleus Systems <office@nucleusys.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <plat/mcbsp.h>
#include <plat/mux.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/cs42l52.h"

/* CDB42L52 Y1 (12.288 MHz) )oscillator =  MCLK1 */
// #define CDB42L52_DEFAULT_MCLK		12288000
// #define CDB42L52_DEFAULT_MCLK		6144000
#define CDB42L52_DEFAULT_MCLK		        26000000
#define CS42L52_FMT_I2S 

static struct snd_soc_card snd_soc_omap3cs42l52;

struct omap3_sl_clk 
{
        int rate;
	int clk_id;
        int clk_freq;
	int div;
};

/* OMAP35x Master -> CS42L52 slave clk table / 2ch, S16 */
struct omap3_sl_clk omap35xcs42l52_sl_clk_16[] =
{
	/* 96 Mhz */
	{ 44100, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 68  },
        { 22050, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 136 },
	/* 83 Mhz PER_L4_ICLK -> McBSP_ICLK */
        { 48000, OMAP_MCBSP_SYSCLK_CLK, 83000000, 54  },
	{ 32000, OMAP_MCBSP_SYSCLK_CLK, 83000000, 81  },
        { 24000, OMAP_MCBSP_SYSCLK_CLK, 83000000, 108 },
	{ 16000, OMAP_MCBSP_SYSCLK_CLK, 83000000, 162 },
        { 12000, OMAP_MCBSP_SYSCLK_CLK, 83000000, 216 }
}; 

/* AM37x Master -> CS42L52 slave clk table / 2ch, S16 */
struct omap3_sl_clk omap37xcs42l52_sl_clk_16[] =
{
	/* 96 Mhz */
        { 44100, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 68  },
	{ 22050, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 136 },
	{ 48000, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 62  },
        { 24000, OMAP_MCBSP_SYSCLK_CLKS_FCLK, 96000000, 134 },
}; 

static int omap3cs42l52_set_sysclk_div(struct snd_soc_dai* cpu_dai,  
	    struct snd_pcm_hw_params *params)
{
/*
    Select OMAP_MCBSP_SYSCLK_CLK for McBSP clock source, 
    McBSPi_ICLK for the SRG divider source
*/
	int i, rate = 0, div = 0, ret = 0;
	rate = params_rate(params);

	for (i = 0; i < ARRAY_SIZE(omap35xcs42l52_sl_clk_16); i ++ ) {
    		if (omap35xcs42l52_sl_clk_16[i].rate == rate) {
			div = omap35xcs42l52_sl_clk_16[i].clk_freq / rate / 2 / 16;

			ret = snd_soc_dai_set_sysclk(cpu_dai, 
				omap35xcs42l52_sl_clk_16[i].clk_id, 
				omap35xcs42l52_sl_clk_16[i].clk_freq, 
				SND_SOC_CLOCK_IN);

    			if (ret < 0) {
    				pr_err("can't set cpu system clock\n");
    				return ret;
    			}

			ret = snd_soc_dai_set_clkdiv(cpu_dai, 
				    OMAP_MCBSP_CLKGDV, div);

    			if (ret < 0) {
    				pr_err("Cannot't set SRG clock divider\n");
        			return ret;
    			}
        
			return 0;
		}
	}

	pr_err("Unsupported rate %d\n", rate);
	return -EINVAL;
}


static int omap3cs42l52_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int fmt;
	int ret;

	/* OMAP3 McBSP Master <=> CS42L52 Slave */
	fmt =	SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
	    | SND_SOC_DAIFMT_CBS_CFS;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration\n");
		return ret;
	}

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0) {
		pr_err("can't set cpu DAI configuration\n");
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, CS42L52_SYSCLK, 
				CDB42L52_DEFAULT_MCLK, SND_SOC_CLOCK_IN);
        if (ret < 0) {
                pr_err("can't set codec clock\n");
                return ret;
        }

	/* Slave cpu_dai */
    	if (fmt & SND_SOC_DAIFMT_CBS_CFS) 
	    return omap3cs42l52_set_sysclk_div(cpu_dai, params);
                                
	return 0;
}

/* OMAP3 I2S */
static struct snd_soc_ops omap3cs42l52_ops = {
	.hw_params = omap3cs42l52_hw_params,
};

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link omap3cs42l52_dai = 
{
/* PCM Audio Omap3 <=> CS42L52 XSP */	
		.name = "CS42L52",
		.stream_name = "XSP-In/Out",
		.cpu_dai_name = "omap-mcbsp-dai.0",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "cs42l52-hifi",
		.codec_name = "cs42l52-codec.2-004a",
		.ops = &omap3cs42l52_ops,
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_omap3cs42l52 = {
	.name = "omap3cirrus",
	.dai_link = &omap3cs42l52_dai,
	.num_links = 1,
};

static struct platform_device *omap3cs42l52_snd_device;

static int __init omap3cs42l52_soc_init(void)
{
	int ret;

	pr_info("OMAP3 SoC init .. register DAIs\n");

       	omap3cs42l52_snd_device = platform_device_alloc("soc-audio", -1);
	if (!omap3cs42l52_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(omap3cs42l52_snd_device, &snd_soc_omap3cs42l52);
	
	ret = platform_device_add(omap3cs42l52_snd_device);
	if (ret)
		goto err1;

	pr_info("OMAP3 SoC init .. register DAIs done\n");
	return 0;

err1:
	pr_err("Unable to add platform device\n");
	platform_device_put(omap3cs42l52_snd_device);

	return ret;
}

static void __exit omap3cs42l52_soc_exit(void)
{
	platform_device_unregister(omap3cs42l52_snd_device);
}

module_init(omap3cs42l52_soc_init);
module_exit(omap3cs42l52_soc_exit);

MODULE_AUTHOR("Georgi Vlaev, Nucleus Systems <office@nucleusys.com>");
MODULE_DESCRIPTION("ALSA SoC OMAP3 / Cirrus");
MODULE_LICENSE("GPL");
