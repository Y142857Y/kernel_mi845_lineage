#ifndef __KSU_H_KERNEL_UMOUNT
#define __KSU_H_KERNEL_UMOUNT

#include <linux/types.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/cred.h>

void ksu_kernel_umount_init(void);
void ksu_kernel_umount_exit(void);
extern bool ksu_webview_zygote_umount_enabled;

// Handler function to be called from setresuid hook
int ksu_handle_umount(struct cred *new, const struct cred *old);

// for the umount list
struct mount_entry {
	char *umountable;
	unsigned int flags;
	struct list_head list;
};
extern struct list_head mount_list;
extern struct rw_semaphore mount_list_lock;

#endif
