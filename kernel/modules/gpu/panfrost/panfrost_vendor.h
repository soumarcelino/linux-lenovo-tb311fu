/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PANFROST_VENDOR_H__
#define __PANFROST_VENDOR_H__

#include <linux/of.h>
#include "panfrost_device.h"

/* gpufreq_v2 uses kHz and a descending, platform-selected OPP table. */
#define PANFROST_TARGET_GPU 1
extern int gpufreq_power_control(int power);
extern int gpufreq_commit(int target, int oppidx);
extern unsigned int gpufreq_get_cur_freq(int target);
extern int gpufreq_get_opp_num(int target);
extern unsigned int gpufreq_get_freq_by_idx(int target, int oppidx);

static inline bool panfrost_vendor_pm(struct panfrost_device *pfdev)
{
	return of_device_is_compatible(pfdev->dev->of_node, "mediatek,mali");
}

#endif
