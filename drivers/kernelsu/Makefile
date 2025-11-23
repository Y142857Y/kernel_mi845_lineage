ksu_obj-y := ksu.o
ksu_obj-y += allowlist.o
ksu_obj-y += app_profile.o
ksu_obj-y += apk_sign.o
ksu_obj-y += sucompat.o
ksu_obj-y += throne_tracker.o
ksu_obj-y += setuid_hook.o
ksu_obj-$(CONFIG_KSU_MANUAL_HOOK) += lsm_hook.o
ksu_obj-$(CONFIG_KSU_SYSCALL_HOOK) += kp_hook.o syscall_handler.o pkg_observer.o
ksu_obj-y += kernel_compat.o
ksu_obj-y += kernel_umount.o
ksu_obj-y += supercalls.o
ksu_obj-y += feature.o
ksu_obj-y += ksud.o
ksu_obj-y += seccomp_cache.o
ksu_obj-y += file_wrapper.o
ksu_obj-y += selinux/selinux.o
ksu_obj-y += selinux/sepolicy.o
ksu_obj-y += selinux/rules.o

kernelsu-objs := $(ksu_obj-y)

ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux -include $(srctree)/include/uapi/asm-generic/errno.h

obj-$(CONFIG_KSU) += kernelsu.o

KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# Stop printing unless ddk environment
ifneq ($(KDIR),)
# DDK environment workaround.
ccflags-y += -DDDK_ENV
endif

ccflags-y += -DKSU_VERSION=22213

# SELinux drivers check
ifeq ($(shell grep -q "current_sid(void)" $(srctree)/security/selinux/include/objsec.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_COMPAT_HAS_CURRENT_SID
endif

ifeq ($(shell grep -q "struct selinux_state " $(srctree)/security/selinux/include/security.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_COMPAT_HAS_SELINUX_STATE
endif

# Handle optional backports
ifeq ($(shell grep -q "strncpy_from_user_nofault" $(srctree)/include/linux/uaccess.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_STRNCPY
endif

ifeq ($(shell grep -q "ssize_t kernel_read" $(srctree)/fs/read_write.c 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_KERNEL_READ
endif

ifeq ($(shell grep "ssize_t kernel_write" $(srctree)/fs/read_write.c | grep -q "const void" 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_KERNEL_WRITE
endif

ifeq ($(shell grep -q "int\s\+path_umount" $(srctree)/fs/namespace.c 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_HAS_PATH_UMOUNT
endif

ifeq ($(shell grep -q "inode_security_struct\s\+\*selinux_inode" $(srctree)/security/selinux/include/objsec.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_SELINUX_INODE
endif

ifeq ($(shell grep -q "task_security_struct\s\+\*selinux_cred" $(srctree)/security/selinux/include/objsec.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_SELINUX_CRED
endif

# seccomp_types.h were added on 6.7
ifeq ($(shell grep -q "atomic_t\s\+filter_count" $(srctree)/include/linux/seccomp.h $(srctree)/include/linux/seccomp_types.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_SECCOMP_FILTER_CNT
endif

# some old kernel backport this, let's check if put_seccomp_filter still exist
ifneq ($(shell grep -wq "put_seccomp_filter" $(srctree)/kernel/seccomp.c $(srctree)/include/linux/seccomp.h 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_OPTIONAL_SECCOMP_FILTER_RELEASE
endif

ifeq ($(shell grep -q "anon_inode_getfd_secure" $(srctree)/fs/anon_inodes.c 2>/dev/null; echo $$?),0)
ccflags-y += -DKSU_HAS_GETFD_SECURE
endif

ccflags-y += -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function -Wno-missing-prototypes

