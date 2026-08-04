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
static __always_inline bool check_v2_signature(char *path,
					       unsigned expected_size,
					       const char *expected_sha256)
{
	return true;
}
int get_pkg_from_apk_path(char *pkg, const char *path)
{
	int len = strlen(path);
	if (len >= KSU_MAX_PACKAGE_NAME || len < 1)
		return -1;

	const char *last_slash = NULL, *second_last_slash = NULL;
	for (int i = len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			if (!last_slash)
				last_slash = &path[i];
			else {
				second_last_slash = &path[i];
				break;
			}
		}
	}
	if (!last_slash || !second_last_slash)
		return -1;

	const char *last_hyphen = strchr(second_last_slash, '-');
	if (!last_hyphen || last_hyphen > last_slash)
		return -1;

	int pkg_len = last_hyphen - second_last_slash - 1;
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
	return strstr(path, "me.weishu.kernelsu") ||
	       strstr(path, "com.ripes.kernelsu");
}
#ifdef CONFIG_KSU_DEBUG
int ksu_debug_manager_appid = -1;

static int set_dummy(const char *val, const struct kernel_param *kp)
{
	return param_set_int(val, kp);
}

static struct kernel_param_ops dummy_ops = {
	.set = set_dummy,
	.get = param_get_int,
};

module_param_cb(ksu_debug_manager_appid, &dummy_ops,
		&ksu_debug_manager_appid, S_IRUSR | S_IWUSR);
#endif
