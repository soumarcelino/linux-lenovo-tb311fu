// SPDX-License-Identifier: GPL-2.0
/* Exact vendor 6.6 diagnostics and explicit cleanup before controlled unload.
 * vcu_func is a 128-byte exported table in mtk-vcodec-common. Read only.
 * Decoder notifier offset 0x2c68 is verified in the immutable probe binary.
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/sched/task.h>

extern void *vcu_func[16];
extern void vcu_get_task(struct task_struct **task, int reset);
extern void vcu_put_task(void);
static unsigned long decoder_notifier;
module_param(decoder_notifier, ulong, 0400);
MODULE_PARM_DESC(decoder_notifier, "Expected decoder callback address; zero only inspects");

static int __init vcodec_runtime_init(void)
{
 struct device *dev;
 struct notifier_block *nb;
 void *ctx;
 int i, ret = 0;
 struct task_struct *task;
 vcu_get_task(&task, 0);
 pr_info("tb311fu-vcodec: VCU userspace task=%s pid=%d\n",
  task ? task->comm : "absent", task ? task_pid_nr(task) : 0);
 vcu_put_task();
 for (i = 0; i < ARRAY_SIZE(vcu_func); i++)
  pr_info("tb311fu-vcodec: vcu_func[%d]=%ps\n", i, READ_ONCE(vcu_func[i]));
 dev = bus_find_device_by_name(&platform_bus_type, NULL, "16000000.vcodec_dec");
 if (!dev)
  return -ENODEV;
 device_lock(dev);
 ctx = dev_get_drvdata(dev);
 if (!dev->driver || strcmp(dev->driver->name, "mtk-vcodec-dec") || !ctx) {
  ret = -ENODEV;
  goto out;
 }
 pr_info("tb311fu-vcodec: decoder platform=%px vcu=%px context=%px self=%px backend=%ps handle=%px\n",
  *(void **)(ctx + 0xd0), *(void **)(ctx + 0xd8), ctx + 0x110,
  *(void **)(ctx + 0x120), *(void **)(ctx + 0x5a0), *(void **)(ctx + 0x5b0));
 if (!decoder_notifier)
  goto out;
 nb = ctx + 0x2c68;
 if ((unsigned long)READ_ONCE(nb->notifier_call) != decoder_notifier) {
  ret = -EINVAL;
  goto out;
 }
 ret = unregister_pm_notifier(nb);
 pr_info("tb311fu-vcodec: decoder notifier cleanup returned %d\n", ret);
out:
 device_unlock(dev);
 put_device(dev);
 return ret;
}
static void __exit vcodec_runtime_exit(void) {}
module_init(vcodec_runtime_init);
module_exit(vcodec_runtime_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB311FU exact vendor codec inspection and controlled unload cleanup");
