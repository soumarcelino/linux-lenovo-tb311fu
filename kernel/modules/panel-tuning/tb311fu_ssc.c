// SPDX-License-Identifier: GPL-2.0
/* Exact MT6768 SSC enable bit, independently verified in vendor binary.
 * No panel command, voltage, PLL frequency or divider is changed.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kprobes.h>
static bool disable;
module_param(disable, bool, 0444);
static unsigned int before, after, param_before, param_after;
module_param(param_before, uint, 0444);
module_param(param_after, uint, 0444);
static unsigned long __nocfi lookup_param(void)
{
 struct kprobe kp={.symbol_name="kallsyms_lookup_name"};
 unsigned long (*lookup)(const char *);
 unsigned long p;
 if (register_kprobe(&kp)) return 0;
 lookup=(void *)kp.addr;
 unregister_kprobe(&kp);
 p=lookup("panel_hx83102e_boe_vdo_60hz:ext_params");
 return p;
}
module_param(before, uint, 0444);
module_param(after, uint, 0444);
static int __init ssc_init(void)
{
 struct device_node *np;
 struct resource r;
 void __iomem *base;
 int ret;
 u8 *params=(void *)lookup_param();
 if (!params || READ_ONCE(*(u32 *)(params+4))!=508 || READ_ONCE(*(u32 *)(params+0xa9c))!=5) return -EINVAL;
 param_before=READ_ONCE(*(u32 *)(params+0xa94));
 if (param_before>1) return -EINVAL;
 np=of_find_compatible_node(NULL,NULL,"mediatek,mt6768-mipi-tx");
 if (!np) return -ENODEV;
 ret=of_address_to_resource(np,0,&r);
 of_node_put(np);
 if (ret) return ret;
 if (r.start!=0x11c80000 || resource_size(&r)!=0x1000) return -ENODEV;
 base=ioremap(r.start,resource_size(&r));
 if (!base) return -ENOMEM;
 before=readl(base+0x34);
 if (disable) {
  WRITE_ONCE(*(u32 *)(params+0xa94),0);
  writel(before & ~BIT(1),base+0x34);
 }
 param_after=READ_ONCE(*(u32 *)(params+0xa94));
 after=readl(base+0x34);
 iounmap(base);
 pr_info("tb311fu_ssc: PLL_CON2 before=%08x after=%08x SSC=%u disable=%u\n",before,after,!!(after&BIT(1)),disable);
 return disable && (after&BIT(1)) ? -EIO : 0;
}
static void __exit ssc_exit(void) {}
module_init(ssc_init);
module_exit(ssc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB311FU MT6768 exact SSC bit inspection/disable");
