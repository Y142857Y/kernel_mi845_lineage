static bool check_block(struct file *fp, u32 *size4, loff_t *pos, u32 *offset,
			unsigned expected_size, const char *expected_sha256)
{
	kernel_read(fp, size4, 0x4, pos); // signer-sequence length
	kernel_read(fp, size4, 0x4, pos); // signer length
	kernel_read(fp, size4, 0x4, pos); // signed data length

	*offset += 0x4 * 3;

	kernel_read(fp, size4, 0x4, pos); // digests-sequence length

	*pos += *size4;
	*offset += 0x4 + *size4;

	kernel_read(fp, size4, 0x4, pos); // certificates length
	kernel_read(fp, size4, 0x4, pos); // certificate length

	*offset += 0x4 * 2;
	*offset += *size4;

	if (*size4 > 2048) {
		pr_info("cert length overlimit: %u\n", *size4);
		return false;
	}

	char *cert = kzalloc(*size4, GFP_KERNEL);
	if (!cert)
		return false;

	kernel_read(fp, cert, *size4, pos);

	unsigned char digest[SHA256_DIGEST_SIZE];

	if (ksu_sha256(cert, *size4, digest) < 0) {
		kfree(cert);
		pr_info("sha256 error\n");
		return false;
	}

	char hash_str[SHA256_DIGEST_SIZE * 2 + 1];
	hash_str[SHA256_DIGEST_SIZE * 2] = '\0';

	bin2hex(hash_str, digest, SHA256_DIGEST_SIZE);

	pr_info("sha256: %s, expected: %s\n",
		hash_str, expected_sha256);

	kfree(cert);

	return strcmp(expected_sha256, hash_str) == 0;
}
    bool is_manager_apk(char *path)
{
	return (
		check_v2_signature(path, 0,
			"a40da80a59d170caa950cf15c18c454d47a39b26989d8b640ecd745ba71bf5dc")
		||
		check_v2_signature(path, 0,
			"c371061b19d8c7d7d6133c6a9bafe198fa944e50c1b31c9d8daa8d7f1fc2d2d6")
	);
}
