// SPDX-License-Identifier: GPL-2.0
/* TB311FU, vendor mediatek-drm 39df257c... on configured 6.6.142 only.
 * Use normal DRM plane helper callbacks; no code patching or IRQ hooks.
 * Install/remove while the compositor is stopped. Restore original ownership
 * of format arrays/helpers before releasing the target DRM device.
 */
#include <linux/dma-resv.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_plane.h>
#include <drm/drm_property.h>

static bool apply;
module_param(apply, bool, 0444);
MODULE_PARM_DESC(apply, "Apply validated TB311FU plane quirk; default only inspects");
/* Runtime fallback for comparing the original conservative path. */
static bool legacy_waits;
module_param(legacy_waits, bool, 0644);
MODULE_PARM_DESC(legacy_waits, "Restore duplicate WRITE waits and unconditional explicit wait calls");
static atomic_long_t implicit_waits, explicit_waits, errors;
static int get_counter(char *buf, const struct kernel_param *kp)
{
 return scnprintf(buf, PAGE_SIZE, "%ld\n", atomic_long_read(kp->arg));
}
static const struct kernel_param_ops counter_ops = { .get = get_counter };
module_param_cb(implicit_waits, &counter_ops, &implicit_waits, 0444);
module_param_cb(explicit_waits, &counter_ops, &explicit_waits, 0444);
module_param_cb(errors, &counter_ops, &errors, 0444);


/* Opt-in aggregate timing: no per-frame logging and no clocks on the default
 * path. Counters are read under one lock to produce a consistent snapshot. */
static bool profile;
module_param(profile, bool, 0644);
MODULE_PARM_DESC(profile, "Measure prepare/explicit/reservation waits in nanoseconds");
struct wait_stats {
 u64 count, total_ns, max_ns, buckets[8];
};
static struct wait_stats timing[3];
static DEFINE_SPINLOCK(timing_lock);
static void record_time(unsigned int index, u64 start)
{
 static const u64 limits[] = {10000, 100000, 1000000, 4000000, 8000000, 16000000, 32000000};
 struct wait_stats *s = &timing[index];
 unsigned long flags;
 unsigned int bucket = 0;
 u64 ns = ktime_get_ns() - start;
 while (bucket < ARRAY_SIZE(limits) && ns > limits[bucket])
  bucket++;
 spin_lock_irqsave(&timing_lock, flags);
 s->count++;
 s->total_ns += ns;
 s->max_ns = max(s->max_ns, ns);
 s->buckets[bucket]++;
 spin_unlock_irqrestore(&timing_lock, flags);
}
static int get_timing(char *buf, const struct kernel_param *kp)
{
 struct wait_stats snapshot[3];
 static const char * const names[] = {"prepare", "explicit", "reservation"};
 unsigned long flags;
 unsigned int i, j;
 int n = 0;
 spin_lock_irqsave(&timing_lock, flags);
 memcpy(snapshot, timing, sizeof(snapshot));
 spin_unlock_irqrestore(&timing_lock, flags);
 for (i = 0; i < ARRAY_SIZE(snapshot); i++) {
  n += scnprintf(buf + n, PAGE_SIZE - n, "%s count=%llu total_ns=%llu max_ns=%llu buckets=",
                 names[i], snapshot[i].count, snapshot[i].total_ns, snapshot[i].max_ns);
  for (j = 0; j < ARRAY_SIZE(snapshot[i].buckets); j++)
   n += scnprintf(buf + n, PAGE_SIZE - n, "%s%llu", j ? "," : "", snapshot[i].buckets[j]);
  n += scnprintf(buf + n, PAGE_SIZE - n, "\n");
 }
 return n;
}
static const struct kernel_param_ops timing_ops = { .get = get_timing };
module_param_cb(timing, &timing_ops, NULL, 0444);

struct plane_quirk {
 struct drm_plane *plane;
 const struct drm_plane_helper_funcs *original_helper;
 struct drm_plane_helper_funcs helper;
 u32 *original_formats, *formats;
 unsigned int original_count;
 struct drm_property_blob *original_blob, *blob;
 bool installed;
};
static struct device *target;
static struct drm_device *drm;
static struct module *owner;
static struct plane_quirk *quirks;
static unsigned int count;

static int prepare_fb(struct drm_plane *plane, struct drm_plane_state *state)
{
 struct drm_gem_object *gem;
 long ret;
 bool measure = READ_ONCE(profile), legacy = READ_ONCE(legacy_waits);
 enum dma_resv_usage usage = DMA_RESV_USAGE_WRITE;
 u64 start = measure ? ktime_get_ns() : 0, wait_start;
 if (!state->fb)
  return 0;
 /* mtk_fb_get_gem_obj is ldr x0,[x0,#0xc8]; generic fb->obj[] is empty.
  * Its object embeds drm_gem_object first. The normal PRIME importer sets
  * gem->resv to the shared dma-buf reservation, preserving Panfrost fences.
  */
 BUILD_BUG_ON(sizeof(struct drm_framebuffer) != 0xc8);
 gem = *(struct drm_gem_object **)((char *)state->fb + 0xc8);
 if (!gem || gem->dev != drm || !gem->resv) {
  atomic_long_inc(&errors);
  return -EINVAL;
 }
 if (state->fence) {
  atomic_long_inc(&explicit_waits);
  wait_start = measure ? ktime_get_ns() : 0;
  /* Fence signaling is final. Match dma_fence_wait()'s fast path without
   * enabling software signaling on fences that have already completed. */
  ret = !legacy && dma_fence_is_signaled(state->fence) ? 1 :
        dma_fence_wait_timeout(state->fence, true, 5 * HZ);
  if (measure)
   record_time(1, wait_start);
  if (ret <= 0)
   goto failed;
  /* Follow drm_gem_plane_helper_prepare_fb(): an explicit fence replaces
   * implicit render fences, but never the mandatory memory-management
   * fences. Keep waiting synchronously: vendor mtk_fb_wait is a stub. */
  if (!legacy)
   usage = DMA_RESV_USAGE_KERNEL;
 }
 atomic_long_inc(&implicit_waits);
 wait_start = measure ? ktime_get_ns() : 0;
 ret = dma_resv_wait_timeout(gem->resv, usage, true, 5 * HZ);
 if (measure) {
  record_time(2, wait_start);
  record_time(0, start);
 }
 if (ret > 0)
  return 0;
failed:
 atomic_long_inc(&errors);
 pr_err_ratelimited("tb311fu-scanout: plane %u buffer wait failed: %ld\n", plane->base.id, ret);
 return ret < 0 ? ret : -ETIMEDOUT;
}

/* Rebuild IN_FORMATS with the same modifier capabilities, selecting the two
 * known-working scanout formats. Reindex every modifier bitmap consistently.
 */
static int make_blob(struct plane_quirk *q)
{
 struct drm_property *prop = drm->mode_config.modifiers_property;
 struct drm_format_modifier_blob *old, *new;
 struct drm_format_modifier *mods, *newmods;
 u32 *formats;
 u64 id;
 size_t size;
 unsigned int i, j, k;
 if (!prop)
  return 0;
 /* IN_FORMATS is immutable. Its stored value is protected by the modeset
  * locks; this kernel does not export its property read helper to modules. */
 id = 0;
 for (i = 0; i < q->plane->properties.count; i++)
  if (q->plane->properties.properties[i] == prop) {
   id = q->plane->properties.values[i];
   break;
  }
 if (!id)
  return 0;
 q->original_blob = drm_property_lookup_blob(drm, id);
 if (!q->original_blob)
  return -EINVAL;
 old = q->original_blob->data;
 if (q->original_blob->length < sizeof(*old) || old->version != 1 || old->flags ||
     old->count_formats != q->original_count || old->count_formats > 64 ||
     old->formats_offset > q->original_blob->length ||
     old->count_formats * sizeof(u32) > q->original_blob->length - old->formats_offset ||
     old->modifiers_offset > q->original_blob->length ||
     old->count_modifiers > (q->original_blob->length - old->modifiers_offset) / sizeof(*mods))
  return -EINVAL;
 size = ALIGN(sizeof(*new) + 2 * sizeof(u32), 8) + old->count_modifiers * sizeof(*mods);
 new = kzalloc(size, GFP_KERNEL);
 if (!new)
  return -ENOMEM;
 new->version = 1;
 new->count_formats = 2;
 new->formats_offset = sizeof(*new);
 new->count_modifiers = old->count_modifiers;
 new->modifiers_offset = ALIGN(sizeof(*new) + 2 * sizeof(u32), 8);
 memcpy((char *)new + new->formats_offset, q->formats, 2 * sizeof(u32));
 formats = (void *)old + old->formats_offset;
 mods = (void *)old + old->modifiers_offset;
 newmods = (void *)new + new->modifiers_offset;
 for (i = 0; i < old->count_modifiers; i++) {
  newmods[i].modifier = mods[i].modifier;
  for (j = 0; j < 2; j++)
   for (k = 0; k < old->count_formats; k++)
    if (formats[k] == q->formats[j] && k >= mods[i].offset &&
        k - mods[i].offset < 64 && (mods[i].formats & BIT_ULL(k - mods[i].offset)))
     newmods[i].formats |= BIT_ULL(j);
 }
 q->blob = drm_property_create_blob(drm, size, new);
 kfree(new);
 if (IS_ERR(q->blob)) {
  int ret = PTR_ERR(q->blob);
  q->blob = NULL;
  return ret;
 }
 return 0;
}

static void restore(void)
{
 unsigned int i;
 drm_modeset_lock_all(drm);
 for (i = 0; i < count; i++) {
  struct plane_quirk *q = &quirks[i];
  if (!q->installed)
   continue;
  q->plane->helper_private = q->original_helper;
  q->plane->format_types = q->original_formats;
  q->plane->format_count = q->original_count;
  if (q->blob)
   drm_object_property_set_value(&q->plane->base, drm->mode_config.modifiers_property,
                                q->original_blob->base.id);
  q->installed = false;
 }
 drm_modeset_unlock_all(drm);
}

static void release(void)
{
 unsigned int i;
 for (i = 0; i < count; i++) {
  drm_property_blob_put(quirks[i].blob);
  drm_property_blob_put(quirks[i].original_blob);
  kfree(quirks[i].formats);
 }
 kfree(quirks);
 drm_dev_put(drm);
 module_put(owner);
 put_device(target);
}

static int __init scanout_init(void)
{
 struct drm_plane *plane;
 unsigned int i = 0, j;
 int ret = -EINVAL;
 const u32 stub[] = { 0x2a1f03e0, 0xd65f03c0 };
 BUILD_BUG_ON(sizeof(struct drm_framebuffer) != 200);
 target = bus_find_device_by_name(&platform_bus_type, NULL, "14000000.dispsys_config");
 if (!target)
  return -ENODEV;
 device_lock(target);
 if (!target->driver || strcmp(target->driver->name, "mediatek-drm") || !dev_get_drvdata(target))
  goto bad_target;
 /* mtk_drm_remove: private = pdev->dev.driver_data; drm = private[0]. */
 drm = *(struct drm_device **)dev_get_drvdata(target);
 if (!drm || drm->dev != target || !drm->driver || strcmp(drm->driver->name, "mediatek"))
  goto bad_target;
 owner = target->driver->owner;
 if (!try_module_get(owner))
  goto bad_target;
 drm_dev_get(drm);
 device_unlock(target);
 count = drm->mode_config.num_total_plane;
 if (!count || count > 64) {
  count = 0;
  goto failed;
 }
 quirks = kcalloc(count, sizeof(*quirks), GFP_KERNEL);
 if (!quirks) {
  count = 0;
  ret = -ENOMEM;
  goto failed;
 }
 drm_modeset_lock_all(drm);
 drm_for_each_plane(plane, drm) {
  struct plane_quirk *q = &quirks[i++];
  q->plane = plane;
  q->original_helper = plane->helper_private;
  q->original_formats = plane->format_types;
  q->original_count = plane->format_count;
  pr_info("tb311fu-scanout: plane %u formats=%u prepare=%ps modifiers=%u apply=%d\n",
          plane->base.id, plane->format_count,
          q->original_helper ? q->original_helper->prepare_fb : NULL, plane->modifier_count, apply);
  if (!apply)
   continue;
  if (!q->original_helper || !q->original_helper->prepare_fb ||
      memcmp((const void *)q->original_helper->prepare_fb, stub, sizeof(stub))) {
   ret = -EINVAL;
   goto unlock_failed;
  }
  q->helper = *q->original_helper;
  q->helper.prepare_fb = prepare_fb;
  q->formats = kmalloc_array(2, sizeof(u32), GFP_KERNEL);
  if (!q->formats) {
   ret = -ENOMEM;
   goto unlock_failed;
  }
  q->formats[0] = DRM_FORMAT_XRGB8888;
  q->formats[1] = DRM_FORMAT_ARGB8888;
  for (j = 0; j < 2; j++) {
   unsigned int k;
   for (k = 0; k < plane->format_count; k++)
    if (q->formats[j] == plane->format_types[k])
     break;
   if (k == plane->format_count) {
    ret = -EINVAL;
    goto unlock_failed;
   }
  }
  ret = make_blob(q);
  if (ret)
   goto unlock_failed;
 }
 /* Prepare every allocation and blob before changing any live plane. */
 if (apply)
  for (i = 0; i < count; i++) {
   struct plane_quirk *q = &quirks[i];
   q->plane->helper_private = &q->helper;
   q->plane->format_types = q->formats;
   q->plane->format_count = 2;
   q->installed = true;
   if (q->blob) {
    ret = drm_object_property_set_value(&q->plane->base, drm->mode_config.modifiers_property, q->blob->base.id);
    if (ret)
     goto unlock_failed;
   }
  }
 drm_modeset_unlock_all(drm);
 return 0;
unlock_failed:
 drm_modeset_unlock_all(drm);
 restore();
failed:
 release();
 return ret;
bad_target:
 device_unlock(target);
 put_device(target);
 return -ENODEV;
}

static void __exit scanout_exit(void)
{
 /* Stop compositor clients and let their atomic commits finish first. */
 restore();
 release();
}
module_init(scanout_init);
module_exit(scanout_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TB311FU MediaTek plane formats and dma-buf presentation synchronization");
MODULE_SOFTDEP("pre: mediatek_drm");
