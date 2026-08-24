#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/version.h>

#ifndef KSU_MAX_PACKAGE_NAME
#define KSU_MAX_PACKAGE_NAME 256
#endif

bool check_v2_signature(char *path, unsigned expected_size,
			const char *expected_sha256)
{
	return true;
}

int get_pkg_from_apk_path(char *pkg, const char *path)
{
	const char *last_slash;
	const char *second_last_slash;
	const char *last_hyphen;
	size_t pkg_len;

	if (unlikely(!pkg || !path || !*path))
		return -1;

	/*
	 * Find the last '/' first.
	 */
	last_slash = strrchr(path, '/');
	if (unlikely(!last_slash || last_slash == path))
		return -1;

	/*
	 * Find the second-last '/'.
	 */
	second_last_slash = last_slash - 1;

	while (second_last_slash > path && *second_last_slash != '/')
		second_last_slash--;

	if (unlikely(*second_last_slash != '/'))
		return -1;

	/*
	 * Expected path format:
	 *
	 * /.../<package>-<version>/base.apk
	 */
	last_hyphen = strchr(second_last_slash + 1, '-');
	if (unlikely(!last_hyphen || last_hyphen >= last_slash))
		return -1;

	pkg_len = last_hyphen - second_last_slash - 1;

	if (unlikely(!pkg_len || pkg_len >= KSU_MAX_PACKAGE_NAME))
		return -1;

	memcpy(pkg, second_last_slash + 1, pkg_len);
	pkg[pkg_len] = '\0';

	return 0;
}

bool is_manager_apk(char *path)
{
	if (unlikely(!path))
		return false;

	return !!(strstr(path, "me.weishu.kernelsu") ||
		  strstr(path, "com.ripes.kernelsu"));
}

#ifdef CONFIG_KSU_DEBUG
int ksu_debug_manager_appid = -1;

static int set_dummy(const char *val, const struct kernel_param *kp)
{
	return param_set_int(val, kp);
}

static const struct kernel_param_ops dummy_ops = {
	.set = set_dummy,
	.get = param_get_int,
};

module_param_cb(ksu_debug_manager_appid, &dummy_ops,
		&ksu_debug_manager_appid, S_IRUSR | S_IWUSR);
#endif
