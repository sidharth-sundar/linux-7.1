// SPDX-License-Identifier: GPL-2.0

#include <linux/quotaops.h>
#include <linux/uuid.h>

#include "jrnl_comp_ext4.h"
#include "xattr.h"
#include "jrnl_comp_ext4_jrnl_comp_jbd2.h"

static void jrnl_comp_ext4_fname_from_fscrypt_name(struct jrnl_comp_ext4_filename *dst,
					 const struct fscrypt_name *src)
{
	memset(dst, 0, sizeof(*dst));

	dst->usr_fname = src->usr_fname;
	dst->disk_name = src->disk_name;
	dst->hinfo.hash = src->hash;
	dst->hinfo.minor_hash = src->minor_hash;
	dst->crypto_buf = src->crypto_buf;
}

int jrnl_comp_ext4_fname_setup_filename(struct inode *dir, const struct qstr *iname,
			      int lookup, struct jrnl_comp_ext4_filename *fname)
{
	struct fscrypt_name name;
	int err;

	err = fscrypt_setup_filename(dir, iname, lookup, &name);
	if (err)
		return err;

	jrnl_comp_ext4_fname_from_fscrypt_name(fname, &name);

	err = jrnl_comp_ext4_fname_setup_ci_filename(dir, iname, fname);
	if (err)
		jrnl_comp_ext4_fname_free_filename(fname);

	return err;
}

int jrnl_comp_ext4_fname_prepare_lookup(struct inode *dir, struct dentry *dentry,
			      struct jrnl_comp_ext4_filename *fname)
{
	struct fscrypt_name name;
	int err;

	err = fscrypt_prepare_lookup(dir, dentry, &name);
	if (err)
		return err;

	jrnl_comp_ext4_fname_from_fscrypt_name(fname, &name);

	err = jrnl_comp_ext4_fname_setup_ci_filename(dir, &dentry->d_name, fname);
	if (err)
		jrnl_comp_ext4_fname_free_filename(fname);
	return err;
}

void jrnl_comp_ext4_fname_free_filename(struct jrnl_comp_ext4_filename *fname)
{
	struct fscrypt_name name;

	name.crypto_buf = fname->crypto_buf;
	fscrypt_free_filename(&name);

	fname->crypto_buf.name = NULL;
	fname->usr_fname = NULL;
	fname->disk_name.name = NULL;

	jrnl_comp_ext4_fname_free_ci_filename(fname);
}

static bool uuid_is_zero(__u8 u[16])
{
	int i;

	for (i = 0; i < 16; i++)
		if (u[i])
			return false;
	return true;
}

int jrnl_comp_ext4_ioctl_get_encryption_pwsalt(struct file *filp, void __user *arg)
{
	struct super_block *sb = file_inode(filp)->i_sb;
	struct jrnl_comp_ext4_sb_info *sbi = JRNL_COMP_EXT4_SB(sb);
	int err, err2;
	jrnl_comp_handle_t *handle;

	if (!jrnl_comp_ext4_has_feature_encrypt(sb))
		return -EOPNOTSUPP;

	if (uuid_is_zero(sbi->s_es->s_encrypt_pw_salt)) {
		err = mnt_want_write_file(filp);
		if (err)
			return err;
		handle = jrnl_comp_ext4_journal_start_sb(sb, JRNL_COMP_EXT4_HT_MISC, 1);
		if (IS_ERR(handle)) {
			err = PTR_ERR(handle);
			goto pwsalt_err_exit;
		}
		err = jrnl_comp_ext4_journal_get_write_access(handle, sb, sbi->s_sbh,
						    JRNL_COMP_EXT4_JTR_NONE);
		if (err)
			goto pwsalt_err_journal;
		lock_buffer(sbi->s_sbh);
		generate_random_uuid(sbi->s_es->s_encrypt_pw_salt);
		jrnl_comp_ext4_superblock_csum_set(sb);
		unlock_buffer(sbi->s_sbh);
		err = jrnl_comp_ext4_handle_dirty_metadata(handle, NULL, sbi->s_sbh);
pwsalt_err_journal:
		err2 = jrnl_comp_ext4_journal_stop(handle);
		if (err2 && !err)
			err = err2;
pwsalt_err_exit:
		mnt_drop_write_file(filp);
		if (err)
			return err;
	}

	if (copy_to_user(arg, sbi->s_es->s_encrypt_pw_salt, 16))
		return -EFAULT;
	return 0;
}

static int jrnl_comp_ext4_get_context(struct inode *inode, void *ctx, size_t len)
{
	return jrnl_comp_ext4_xattr_get(inode, JRNL_COMP_EXT4_XATTR_INDEX_ENCRYPTION,
				 JRNL_COMP_EXT4_XATTR_NAME_ENCRYPTION_CONTEXT, ctx, len);
}

static int jrnl_comp_ext4_set_context(struct inode *inode, const void *ctx, size_t len,
							void *fs_data)
{
	jrnl_comp_handle_t *handle = fs_data;
	int res, res2, credits, retries = 0;

	/*
	 * Encrypting the root directory is not allowed because e2fsck expects
	 * lost+found to exist and be unencrypted, and encrypting the root
	 * directory would imply encrypting the lost+found directory as well as
	 * the filename "lost+found" itself.
	 */
	if (inode->i_ino == JRNL_COMP_EXT4_ROOT_INO)
		return -EPERM;

	if (WARN_ON_ONCE(IS_DAX(inode) && i_size_read(inode)))
		return -EINVAL;

	if (jrnl_comp_ext4_test_inode_flag(inode, JRNL_COMP_EXT4_INODE_DAX))
		return -EOPNOTSUPP;

	res = jrnl_comp_ext4_convert_inline_data(inode);
	if (res)
		return res;

	/*
	 * If a journal handle was specified, then the encryption context is
	 * being set on a new inode via inheritance and is part of a larger
	 * transaction to create the inode.  Otherwise the encryption context is
	 * being set on an existing inode in its own transaction.  Only in the
	 * latter case should the "retry on ENOSPC" logic be used.
	 */

	if (handle) {
		/*
		 * Since the inode is new it is ok to pass the
		 * XATTR_CREATE flag. This is necessary to match the
		 * remaining journal credits check in the set_handle
		 * function with the credits allocated for the new
		 * inode.
		 */
		res = jrnl_comp_ext4_xattr_set_handle(handle, inode,
					    JRNL_COMP_EXT4_XATTR_INDEX_ENCRYPTION,
					    JRNL_COMP_EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
					    ctx, len, XATTR_CREATE);
		if (!res) {
			jrnl_comp_ext4_set_inode_flag(inode, JRNL_COMP_EXT4_INODE_ENCRYPT);
			jrnl_comp_ext4_clear_inode_state(inode,
					JRNL_COMP_EXT4_STATE_MAY_INLINE_DATA);
			/*
			 * Update inode->i_flags - S_ENCRYPTED will be enabled,
			 * S_DAX may be disabled
			 */
			jrnl_comp_ext4_set_inode_flags(inode, false);
		}
		return res;
	}

	res = dquot_initialize(inode);
	if (res)
		return res;
retry:
	res = jrnl_comp_ext4_xattr_set_credits(inode, len, false /* is_create */,
				     &credits);
	if (res)
		return res;

	handle = jrnl_comp_ext4_journal_start(inode, JRNL_COMP_EXT4_HT_MISC, credits);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	res = jrnl_comp_ext4_xattr_set_handle(handle, inode, JRNL_COMP_EXT4_XATTR_INDEX_ENCRYPTION,
				    JRNL_COMP_EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
				    ctx, len, 0);
	if (!res) {
		jrnl_comp_ext4_set_inode_flag(inode, JRNL_COMP_EXT4_INODE_ENCRYPT);
		/*
		 * Update inode->i_flags - S_ENCRYPTED will be enabled,
		 * S_DAX may be disabled
		 */
		jrnl_comp_ext4_set_inode_flags(inode, false);
		res = jrnl_comp_ext4_mark_inode_dirty(handle, inode);
		if (res)
			JRNL_COMP_EXT4_ERROR_INODE(inode, "Failed to mark inode dirty");
	}
	res2 = jrnl_comp_ext4_journal_stop(handle);

	if (res == -ENOSPC && jrnl_comp_ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;
	if (!res)
		res = res2;
	return res;
}

static const union fscrypt_policy *jrnl_comp_ext4_get_dummy_policy(struct super_block *sb)
{
	return JRNL_COMP_EXT4_SB(sb)->s_dummy_enc_policy.policy;
}

static bool jrnl_comp_ext4_has_stable_inodes(struct super_block *sb)
{
	return jrnl_comp_ext4_has_feature_stable_inodes(sb);
}

const struct fscrypt_operations jrnl_comp_ext4_cryptops = {
	.inode_info_offs	= (int)offsetof(struct jrnl_comp_ext4_inode_info, i_crypt_info) -
				  (int)offsetof(struct jrnl_comp_ext4_inode_info, vfs_inode),
	.needs_bounce_pages	= 1,
	.has_32bit_inodes	= 1,
	.supports_subblock_data_units = 1,
	.legacy_key_prefix	= "jrnl_comp_ext4:",
	.get_context		= jrnl_comp_ext4_get_context,
	.set_context		= jrnl_comp_ext4_set_context,
	.get_dummy_policy	= jrnl_comp_ext4_get_dummy_policy,
	.empty_dir		= jrnl_comp_ext4_empty_dir,
	.has_stable_inodes	= jrnl_comp_ext4_has_stable_inodes,
};
