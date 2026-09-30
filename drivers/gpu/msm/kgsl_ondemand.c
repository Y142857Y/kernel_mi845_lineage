// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 awkox
 *
 * KGSL on-demand devfreq governor.
 *
 * The stock msm-adreno-tz governor hands the frequency decision to secure
 * world (TZ DCVS) and only passes along the busy statistics that KGSL already
 * keeps.  When nothing feeds TZ a workload it has nothing to work with and
 * answers with the level the GPU was parked at, so the clock never moves:
 * on this device the GPU sits at 257MHz out of a 710MHz top level no matter
 * how busy it gets, because min_pwrlevel defaults to the lowest level.
 *
 * This governor derives the level from the busy counter KGSL samples in
 * kgsl_devfreq_get_dev_status() instead of asking secure world, so it works
 * with nothing but the KGSL power infrastructure.  Thermal, min/max and
 * per-context constraints are still honoured, because the requested level
 * goes through kgsl_pwrctrl_adjust_pwrlevel() on the way down.
 */

#include <linux/devfreq.h>
#include <linux/math64.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"

/*
 * Utilization thresholds in percent of the devfreq polling window.  The gap
 * between them is the hysteresis that keeps the clock from oscillating
 * between two neighbouring levels.
 */
#define KGSL_ONDEMAND_UP_THRESHOLD 60
#define KGSL_ONDEMAND_DOWN_THRESHOLD 20

/* Sampling windows shorter than this (us) carry too little data to trust */
#define KGSL_ONDEMAND_MIN_WINDOW 1000

/**
 * kgsl_ondemand_get_target_freq - pick a level from the measured GPU busy time
 * @devfreq: the devfreq instance
 * @freq: where to store the requested frequency
 *
 * Climb one level when the GPU was busier than the up threshold, drop one
 * level when it was idle enough, and otherwise stay where we are.
 */
static int kgsl_ondemand_get_target_freq(struct devfreq *devfreq,
		unsigned long *freq)
{
	struct kgsl_device *device = dev_get_drvdata(devfreq->dev.parent);
	struct devfreq_dev_status stats;
	unsigned int busy_pct;
	int max_state, level, i;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;

	max_state = devfreq->profile->max_state;
	if (max_state <= 1 || devfreq->profile->freq_table == NULL)
		return -EINVAL;

	if (kgsl_devfreq_get_dev_status(devfreq->dev.parent, &stats))
		return -EINVAL;

	/* Stay where we are unless we find a reason to move */
	*freq = stats.current_frequency;

	/*
	 * The busy counter is only refreshed while the GPU is ACTIVE, so a
	 * window without any sample means there is nothing to react to.
	 */
	if (stats.total_time < KGSL_ONDEMAND_MIN_WINDOW)
		return 0;

	/*
	 * freq_table runs from the fastest level to the slowest one, so the
	 * first entry that is not faster than the current clock is the level
	 * the GPU is sitting at.
	 */
	level = max_state - 1;
	for (i = 0; i < max_state; i++) {
		if (devfreq->profile->freq_table[i] <= stats.current_frequency) {
			level = i;
			break;
		}
	}

	busy_pct = div_u64((u64)stats.busy_time * 100, stats.total_time);

	/* A smaller index is a faster level */
	if (busy_pct >= KGSL_ONDEMAND_UP_THRESHOLD)
		level = max(0, level - 1);
	else if (busy_pct <= KGSL_ONDEMAND_DOWN_THRESHOLD)
		level = min(max_state - 1, level + 1);

	*freq = devfreq->profile->freq_table[level];

	return 0;
}

/**
 * kgsl_ondemand_event_handler - devfreq governor event handler
 *
 * The devfreq core calls this unconditionally on start, stop, suspend,
 * resume and interval changes, so it has to exist even though this governor
 * keeps no state and has nothing to do on any of them.
 */
static int kgsl_ondemand_event_handler(struct devfreq *devfreq,
		unsigned int event, void *data)
{
	return 0;
}

static struct devfreq_governor kgsl_ondemand_governor = {
	.name = KGSL_GOVERNOR_ONDEMAND,
	.immutable = 0,
	.get_target_freq = kgsl_ondemand_get_target_freq,
	.event_handler = kgsl_ondemand_event_handler,
};

/**
 * kgsl_ondemand_governor_register - publish the governor to the devfreq core
 *
 * devfreq_add_device() rejects the device outright when the requested
 * governor is unknown, so this has to happen before the GPU is registered.
 */
int kgsl_ondemand_governor_register(void)
{
	return devfreq_add_governor(&kgsl_ondemand_governor);
}
EXPORT_SYMBOL(kgsl_ondemand_governor_register);