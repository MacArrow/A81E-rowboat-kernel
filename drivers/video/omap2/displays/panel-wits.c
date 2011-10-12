/*
 * WITS LCD panel driver
 * Copyright (C) 2011 Vitaly Makarov <dev.macarrow@gmal.com> 
 *   
 * Based on LCD panel driver for Sharp LS037V7DW01
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <plat/display.h>

static struct omap_video_timings wits_lcd_timings = {
        .x_res = 800,               //800
        .y_res = 480,               //480

        .pixel_clock    = 27000,

        .hsw            = 56,       //56
        .hfp            = 100,      //100
        .hbp            = 100,      //100

        .vsw            = 15,       //15
        .vfp            = 15,       //15
        .vbp            = 15,       //15
};

static int wits_lcd_panel_probe(struct omap_dss_device *dssdev)
{
	dssdev->panel.config = 0
                               | OMAP_DSS_LCD_TFT 
                               | OMAP_DSS_LCD_IPC
                                ;
	dssdev->panel.acb = 0x0;
	dssdev->panel.timings = wits_lcd_timings;
	
        return 0;
}

static void wits_lcd_panel_remove(struct omap_dss_device *dssdev)
{
}

static int wits_lcd_panel_enable(struct omap_dss_device *dssdev)
{
	int r = 0;
        
        if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE)
		return 0;

	r = omapdss_dpi_display_enable(dssdev);
        
	if(r)
		return r;

	/* wait couple of vsyncs until enabling the LCD */
	msleep(50);

	if (dssdev->platform_enable) {
		r = dssdev->platform_enable(dssdev);
                if(r) {
                    omapdss_dpi_display_disable(dssdev);
                    return r;
                }
                    
	}

	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;

	return r;
}

static void wits_lcd_panel_disable(struct omap_dss_device *dssdev)
{
	if (dssdev->state != OMAP_DSS_DISPLAY_ACTIVE)
		return;

	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	/* wait at least 5 vsyncs after disabling the LCD */

	msleep(100);
	
	omapdss_dpi_display_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_DISABLED;
}

static int wits_lcd_panel_suspend(struct omap_dss_device *dssdev)
{
	wits_lcd_panel_disable(dssdev);

	dssdev->state = OMAP_DSS_DISPLAY_SUSPENDED;

	return 0;
}

static int wits_lcd_panel_resume(struct omap_dss_device *dssdev)
{
	return wits_lcd_panel_enable(dssdev);
}

static void wits_lcd_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	wits_lcd_panel_suspend(dssdev);
	dssdev->panel.timings = *timings;
	wits_lcd_panel_resume(dssdev);
}

static void wits_lcd_panel_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int wits_lcd_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return 0;
}

static struct omap_dss_driver wits_lcd_driver = {
	.probe		= wits_lcd_panel_probe,
	.remove		= wits_lcd_panel_remove,

	.enable		= wits_lcd_panel_enable,
	.disable	= wits_lcd_panel_disable,
	.suspend	= wits_lcd_panel_suspend,
	.resume		= wits_lcd_panel_resume,

	.set_timings	= wits_lcd_panel_set_timings,
	.get_timings	= wits_lcd_panel_get_timings,
	.check_timings	= wits_lcd_panel_check_timings,

	.driver         = {
		.name   = "wits_lcd_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init wits_lcd_panel_drv_init(void)
{
	return omap_dss_register_driver(&wits_lcd_driver);
}

static void __exit wits_lcd_panel_drv_exit(void)
{
	omap_dss_unregister_driver(&wits_lcd_driver);
}

module_init(wits_lcd_panel_drv_init);
module_exit(wits_lcd_panel_drv_exit);
MODULE_LICENSE("GPL");
