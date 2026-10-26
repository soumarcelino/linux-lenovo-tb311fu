/* TB311FU vendor workaround: use the external gauge's actual charge state.
 * Only loaded into upowerd. All other attributes retain the stock behavior.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

typedef struct _GUdevDevice GUdevDevice;
extern const char *g_udev_device_get_sysfs_path(GUdevDevice *device);

const char *g_udev_device_get_sysfs_attr_uncached(GUdevDevice *device,
                                                const char *name)
{
    typedef const char *(*Getter)(GUdevDevice *, const char *);
    Getter original = (Getter)dlsym(RTLD_NEXT,
                                    "g_udev_device_get_sysfs_attr_uncached");
    const char *path = g_udev_device_get_sysfs_path(device);
    static _Thread_local char status[32];

    if (path && !strcmp(path,
            "/sys/devices/platform/battery-manager/power_supply/battery") &&
            !strcmp(name, "status")) {
        FILE *file = fopen("/sys/class/power_supply/mm8013c10-0/status", "r");
        if (file) {
            char *value = fgets(status, sizeof(status), file);
            fclose(file);
            if (value) {
                status[strcspn(status, "\r\n")] = '\0';
                if (!strcmp(status, "Charging") ||
                    !strcmp(status, "Discharging") ||
                    !strcmp(status, "Full") ||
                    !strcmp(status, "Not charging"))
                    return status;
            }
        }
    }
    return original ? original(device, name) : NULL;
}
