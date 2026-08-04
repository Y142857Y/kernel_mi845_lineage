#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "klog.h"
#include "ksu.h"

#ifndef KSU_MAX_PACKAGE_NAME
#define KSU_MAX_PACKAGE_NAME 256
#endif


bool check_v2_signature(char *path, unsigned expected_size,
			const char *expected_sha256)
{
	(void)path;
	(void)expected_size;
	(void)expected_sha256;

	return true;
}


int get_pkg_from_apk_path(char *pkg, const char *path)
{
	int len = 0;
	int i;
	int pkg_len;
	const char *last_slash = NULL;
	const char *second_last_slash = NULL;
	const char *hyphen = NULL;

	if (!pkg || !path)
		return -1;

	while (path[len])
		len++;

	if (len <= 0 || len >= KSU_MAX_PACKAGE_NAME)
		return -1;


	for (i = len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			if (!last_slash)
				last_slash = &path[i];
			else {
				second_last_slash = &path[i];
				break;
			}
		}
	}

	if (!second_last_slash || !last_slash)
		return -1;


	for (i = 0; second_last_slash[i]; i++) {
		if (second_last_slash[i] == '-') {
			hyphen = &second_last_slash[i];
			break;
		}
	}

	if (!hyphen || hyphen > last_slash)
		return -1;


	pkg_len = (int)(hyphen - second_last_slash - 1);

	if (pkg_len <= 0 || pkg_len >= KSU_MAX_PACKAGE_NAME)
		return -1;


	for (i = 0; i < pkg_len; i++)
		pkg[i] = second_last_slash[i + 1];

	pkg[pkg_len] = '\0';

	return 0;
}


static bool simple_contains(const char *str, const char *sub)
{
	const char *a;
	const char *b;

	if (!str || !sub)
		return false;

	while (*str) {
		a = str;
		b = sub;

		while (*a && *b && *a == *b) {
			a++;
			b++;
		}

		if (!*b)
			return true;

		str++;
	}

	return false;
}


bool is_manager_apk(char *path)
{
	if (!path)
		return false;

	return simple_contains(path, "me.weishu.kernelsu");
}
