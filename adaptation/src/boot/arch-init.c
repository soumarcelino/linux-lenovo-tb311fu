#define _GNU_SOURCE
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static void fail(const char *s) { perror(s); sleep(10); execl("/bin/busybox","busybox","sh",NULL); for (;;) sleep(60); }
static void bind(const char *a,const char *b) { if(mount(a,b,NULL,MS_BIND,NULL))fail(b); }
int main(int argc,char **argv) {
 if(getpid()!=1) { fprintf(stderr,"PID 1 only\n"); return 1; }
 if(access("/arch/usr/lib/systemd/systemd",X_OK))fail("Arch systemd missing");
 if(mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL))fail("private root");
 bind("/arch","/arch");
 mkdir("/arch/.device-root",0700);
 if(chdir("/arch"))fail("chdir");
 if(syscall(SYS_pivot_root,".",".device-root"))fail("pivot_root");
 if(chdir("/"))fail("chdir root");
 const char *dirs[]={"dev","proc","sys","run",NULL};
 for(int i=0;dirs[i];i++) { char a[128],b[128];snprintf(a,sizeof(a),"/.device-root/%s",dirs[i]);snprintf(b,sizeof(b),"/%s",dirs[i]);if(mount(a,b,NULL,MS_MOVE,NULL))fail(b); }
 bind("/.device-root/home","/home");
 bind("/.device-root/lib/modules","/usr/lib/modules");
 bind("/.device-root/lib/firmware/postmarketos","/usr/lib/firmware/postmarketos");
 bind("/.device-root/vendor","/vendor");
 bind("/.device-root/boot","/boot");
 bind("/.device-root/var/lib/waydroid","/var/lib/waydroid");
 bind("/.device-root/etc/waydroid-extra","/etc/waydroid-extra");
 setenv("PATH","/usr/local/sbin:/usr/local/bin:/usr/bin",1);
 execl("/usr/lib/systemd/systemd","/sbin/init",NULL);fail("systemd");
}
