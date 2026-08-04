#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "klog.h" // NOLINT
#include "ksu.h"

#ifndef KSU_MAX_PACKAGE_NAME
#define KSU_MAX_PACKAGE_NAME 256
#endif

#ifdef CONFIG_KSU_DEBUG

int ksu_debug_manager_appid = -1;

static int set_expected_size(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	ksu_set_manager_appid(ksu_debug_manager_appid);
	pr_info("ksu_manager_appid set to %d\n", ksu_debug_manager_appid);
	return rv;
}

static struct kernel_param_ops expected_size_ops = {
	.set = set_expected_size,
	.get = param_get_uint,
};

module_param_cb(ksu_debug_manager_appid, &expected_size_ops,
	&ksu_debug_manager_appid, S_IRUSR | S_IWUSR);

#endif

// 导出给 KernelSU 其他模块调用的签名校验函数 Stub
// 直接返回 true，彻底跳过实际的计算与文件读取
bool check_v2_signature(char *path, unsigned expected_size, const char *expected_sha256)
{
	(void)path;
	(void)expected_size;
	(void)expected_sha256;
	return true;
}

int get_pkg_from_apk_path(char *pkg, const char *path)
{
	int len;
	const char *last_slash = NULL;
	const char *second_last_slash = NULL;
	const char *last_hyphen = NULL;
	int pkg_len;
	int i;

	if (!pkg || !path)
		return -1;

	len = strlen(path);
	if (len >= KSU_MAX_PACKAGE_NAME || len < 1)
		return -1;

	for (i = len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			if (!last_slash) {
				last_slash = &path[i];
			} else {
				second_last_slash = &path[i];
				break;
			}
		}
	}

	if (!last_slash || !second_last_slash)
		return -1;

	last_hyphen = strchr(second_last_slash, '-');
	if (!last_hyphen || last_hyphen > last_slash)
		return -1;

	pkg_len = last_hyphen - second_last_slash - 1;
	if (pkg_len >= KSU_MAX_PACKAGE_NAME || pkg_len <= 0)
		return -1;

	memcpy(pkg, second_last_slash + 1, pkg_len);
	pkg[pkg_len] = '\0';

	return 0;
}

bool is_manager_apk(char *path)
{
	if (!path)
		return false;
	if (strstr(path, "me.weishu.kernelsu") || strstr(path, "com.ripes.kernelsu")) {
		return true;
	}
	return false;
}
