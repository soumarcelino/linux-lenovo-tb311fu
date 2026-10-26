// SPDX-License-Identifier: GPL-2.0
/* Exact vendor 39df257c... MT6768 clock mode; unload restores original bits. */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
static bool continuous;
module_param(continuous,bool,0444);
static unsigned int before, after;
module_param(before,uint,0444);
module_param(after,uint,0444);
static unsigned long mode_before, mode_after;
module_param(mode_before,ulong,0444);
module_param(mode_after,ulong,0444);
static void __iomem *base;
static struct platform_device *pdev;
static struct module *owner;
static unsigned long *mode;
static int __init cc_init(void)
{
 struct device_node *np;
 struct resource r;
 u8 *dsi;
 int ret;
 np=of_find_compatible_node(NULL,NULL,"mediatek,mt6768-dsi");
 if (!np) return -ENODEV;
 ret=of_address_to_resource(np,0,&r);
 pdev=of_find_device_by_node(np);
 of_node_put(np);
 if (ret || !pdev) return ret ? ret : -ENODEV;
 ret=-ENODEV;
 if (r.start!=0x14014000 || resource_size(&r)!=0x1000 || !pdev->dev.driver) goto put;
 owner=pdev->dev.driver->owner;
 if (!owner || !try_module_get(owner)) goto put;
 dsi=platform_get_drvdata(pdev);
 if (!dsi || READ_ONCE(*(u32 *)(dsi+0xd3c))!=4) goto release;
 mode=(void *)(dsi+0xd30);
 mode_before=READ_ONCE(*mode);
 if ((mode_before&~BIT(10))!=0xa01) goto release;
 base=ioremap(r.start,resource_size(&r));
 if (!base) {ret=-ENOMEM;goto release;}
 before=readl(base+0x18);
 if ((before&0x3c)!=0x3c) {ret=-EBUSY;goto unmap;}
 if (continuous) {
  WRITE_ONCE(*mode,mode_before & ~BIT(10));
  writel(before & ~BIT(16),base+0x18);
 }
 after=readl(base+0x18);
 mode_after=READ_ONCE(*mode);
 pr_info("tb311fu_cc: TXRX_CTRL %08x -> %08x; mode_flags=%lx -> %lx\n",before,after,mode_before,mode_after);
 return 0;
unmap:
 iounmap(base);base=NULL;
release:
 module_put(owner);owner=NULL;
put:
 put_device(&pdev->dev);pdev=NULL;
 return ret;
}
static void __exit cc_exit(void)
{
 unsigned long m;
 u32 v;
 if (continuous) {
  m=READ_ONCE(*mode);WRITE_ONCE(*mode,(m&~BIT(10))|(mode_before&BIT(10)));
  v=readl(base+0x18);writel((v&~BIT(16))|(before&BIT(16)),base+0x18);
 }
 iounmap(base);module_put(owner);put_device(&pdev->dev);
}
module_init(cc_init);
module_exit(cc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB311FU persistent host and physical continuous DSI clock");
