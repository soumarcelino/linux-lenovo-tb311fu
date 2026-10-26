// SPDX-License-Identifier: GPL-2.0
/* One-shot coordinator for the exact BOE/Himax vendor modules. No voltage changes. */
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/notifier.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
static unsigned long (*lookup)(const char *);
static int (*unreg)(struct notifier_block *);
static int (*regclient)(const char *,struct notifier_block *);
static int (*notify)(struct notifier_block *,unsigned long,void *);
static int (*resume_ts)(void *);
static struct device *dev;
static struct module *owner;
static void *ts;
static struct notifier_block *nb;
static DEFINE_MUTEX(lock);
static bool held;
static atomic_t prepared=ATOMIC_INIT(0);
static unsigned int baseline;
static struct kretprobe rp;
static bool probe_live;
static unsigned int cycles;
module_param(cycles,uint,0444);
static int prepare_return(struct kretprobe_instance *ri,struct pt_regs *regs)
{
 if ((long)regs_return_value(regs)==0) atomic_inc(&prepared);
 return 0;
}
static int __nocfi start_hold(void)
{
 int down=1,ret;
 u32 *dbl=(void *)lookup("panel_hx83102e_boe_vdo_60hz:double_click_flag");
 if (!dbl || READ_ONCE(*dbl)!=0 || READ_ONCE(*((u8 *)ts+2))) return -EBUSY;
 ret=unreg(nb);
 if (ret) return ret;
 held=true;
 /* Native callback cancels/flushes pending resume work, then suspends IRQ/reset. */
 ret=notify(nb,0,&down);
 if (ret || !READ_ONCE(*((u8 *)ts+2))) return -EIO;
 baseline=atomic_read(&prepared);
 pr_info("tb311fu_reinit: touch suspended, notifier detached; baseline=%u\n",baseline);
 return 0;
}
static int __nocfi finish_hold(bool force)
{
 int ret;
 if (!held) return 0;
 if (!force && atomic_read(&prepared)<=baseline) return -EAGAIN;
 pr_info("tb311fu_reinit: panel prepare complete; starting serialized Himax resume/FW reload\n");
 ret=resume_ts(ts);
 if (ret) return ret;
 ret=regclient("HIMAX Touch",nb);
 if (ret) return ret;
 held=false;cycles++;
 pr_info("tb311fu_reinit: firmware resume returned, notifier restored; suspended=%u\n",READ_ONCE(*((u8 *)ts+2)));
 return READ_ONCE(*((u8 *)ts+2)) ? -EIO : 0;
}
static int set_action(const char *val,const struct kernel_param *kp)
{
 int ret;
 mutex_lock(&lock);
 if (sysfs_streq(val,"hold")) ret=held ? -EBUSY : start_hold();
 else if (sysfs_streq(val,"finish")) ret=finish_hold(false);
 else if (sysfs_streq(val,"recover")) ret=finish_hold(true);
 else ret=-EINVAL;
 mutex_unlock(&lock);
 return ret;
}
static int get_action(char *buf,const struct kernel_param *kp)
{
 return scnprintf(buf,PAGE_SIZE,"held=%u suspended=%u prepared=%d baseline=%u cycles=%u\n",held,ts ? READ_ONCE(*((u8 *)ts+2)):255,atomic_read(&prepared),baseline,cycles);
}
static const struct kernel_param_ops ops={.set=set_action,.get=get_action};
module_param_cb(action,&ops,NULL,0600);
static int __init reinit_init(void)
{
 struct kprobe kp={.symbol_name="kallsyms_lookup_name"};
 int ret=register_kprobe(&kp);
 if (ret) return ret;
 lookup=(void *)kp.addr;unregister_kprobe(&kp);
 /* Native pointer calls are isolated in nocfi helpers, matching existing vendor ABI. */
 return 0;
}
static int __nocfi resolve_native(void)
{
 void **tsp=(void *)lookup("hx83102e:hx_s_ts");
 int ret;
 if (!tsp || !(ts=READ_ONCE(*tsp))) return -ENODEV;
 dev=bus_find_device_by_name(&spi_bus_type,NULL,"spi2.1");
 if (!dev) return -ENODEV;
 if (dev_get_drvdata(dev)!=ts || !dev->driver) {ret=-ENODEV;goto put;}
 owner=dev->driver->owner;
 if (!owner || !try_module_get(owner)) {ret=-ENODEV;goto put;}
 nb=(void *)((u8 *)ts+0x250);
 notify=(void *)lookup("hx83102e:himax_disp_notifier_callback");
 resume_ts=(void *)lookup("hx83102e:himax_chip_common_resume");
 unreg=(void *)lookup("mtk_disp_notifier_unregister");
 regclient=(void *)lookup("mtk_disp_notifier_register");
 if (!notify || !resume_ts || !unreg || !regclient || (void *)READ_ONCE(nb->notifier_call)!=(void *)notify) {ret=-EINVAL;goto release;}
 rp.kp.addr=(void *)lookup("panel_hx83102e_boe_vdo_60hz:lcm_prepare");
 if (!rp.kp.addr) {ret=-EINVAL;goto release;}
 rp.handler=prepare_return;rp.maxactive=8;
 ret=register_kretprobe(&rp);
 if (ret) goto release;
 probe_live=true;return 0;
release:
 module_put(owner);owner=NULL;
put:
 put_device(dev);dev=NULL;return ret;
}
static int __init init_all(void)
{
 int ret=reinit_init();
 if (ret) return ret;
 ret=resolve_native();
 if (!ret) pr_info("tb311fu_reinit: ready, no hardware changed\n");
 return ret;
}
static void __exit reinit_exit(void)
{
 mutex_lock(&lock);if (held) finish_hold(true);mutex_unlock(&lock);
 if (probe_live) unregister_kretprobe(&rp);
 if (owner) module_put(owner);
 if (dev) put_device(dev);
}
module_init(init_all);module_exit(reinit_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB311FU serialized Himax/display reinitialization coordinator");
