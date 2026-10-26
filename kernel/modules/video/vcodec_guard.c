// SPDX-License-Identifier: GPL-2.0
/* Never dereference a vendor context from a pending/failed platform probe. */
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <media/v4l2-dev.h>

extern int tb311fu_vcodec_call_open(struct file *file);

int tb311fu_vcodec_open_guard(struct file *file)
{
#ifndef TB311FU_VCODEC_ALLOW_OPEN
	/* Probe-only artifact: automatic scanners must not enter vendor open. */
	return -EAGAIN;
#else
	struct video_device *vfd = video_devdata(file);
	struct device *parent = vfd->dev.parent;
	int ret = -ENODEV;

	/* The registered video device holds a reference to its parent. */
	if (!parent)
		return ret;
	/* really_probe/remove hold this mutex. Wait until probe has finished,
	 * then reject orphan video nodes without touching freed vendor data.
	 */
	device_lock(parent);
	if (parent->driver && dev_get_drvdata(parent) &&
	    dev_get_drvdata(parent) == video_get_drvdata(vfd))
		ret = tb311fu_vcodec_call_open(file);
	device_unlock(parent);
	return ret;
#endif
}

/* Both immutable vendor probes register dev+0x2c68; neither remove path
 * unregisters it. Do this before the vendor context can be freed.
 * The platform core already holds the device mutex during remove.
 */
extern int tb311fu_vcodec_call_remove(struct platform_device *pdev);
int tb311fu_vcodec_remove_guard(struct platform_device *pdev)
{
 void *ctx = platform_get_drvdata(pdev);
 if (ctx)
  unregister_pm_notifier(ctx + 0x2c68);
 return tb311fu_vcodec_call_remove(pdev);
}
