// SPDX-License-Identifier: GPL-2.0+
/*
 *  Software partition device (UCLASS_PARTITION)
 *
 *  Copyright (c) 2021 Linaro Limited
 *			Author: AKASHI Takahiro
 */

#define LOG_CATEGORY UCLASS_PARTITION

#include <blk.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <vsprintf.h>
#include <dm/device-internal.h>
#include <dm/lists.h>

/**
 * disk_blk_part_validate() - Check whether access to partition is within limits
 *
 * @dev: Device (partition udevice)
 * @start: Start block for the access(from start of partition)
 * @blkcnt: Number of blocks to access (within the partition)
 * @return 0 on valid block range, or -ve on error.
 */
static int disk_blk_part_validate(struct udevice *dev, lbaint_t start, lbaint_t blkcnt)
{
	struct disk_part *part = dev_get_uclass_plat(dev);

	if (device_get_uclass_id(dev) != UCLASS_PARTITION)
		return -ENOSYS;

	if (start >= part->gpt_part_info.size)
		return -E2BIG;

	if ((start + blkcnt) > part->gpt_part_info.size)
		return -ERANGE;

	return 0;
}

/**
 * disk_blk_part_offset() - Compute offset from start of block device
 *
 * @dev: Device (partition udevice)
 * @start: Start block for the access (from start of partition)
 * @return Start block for the access (from start of block device)
 */
static lbaint_t disk_blk_part_offset(struct udevice *dev, lbaint_t start)
{
	struct disk_part *part = dev_get_uclass_plat(dev);

	return start + part->gpt_part_info.start;
}

/*
 * BLOCK IO APIs
 */
/**
 * disk_blk_read() - Read from a block device partition
 *
 * @dev: Device to read from (partition udevice)
 * @start: Start block for the read (from start of partition)
 * @blkcnt: Number of blocks to read (within the partition)
 * @buffer: Place to put the data
 * @return number of blocks read (which may be less than @blkcnt),
 * or -ve on error. This never returns 0 unless @blkcnt is 0
 */
unsigned long disk_blk_read(struct udevice *dev, lbaint_t start,
			    lbaint_t blkcnt, void *buffer)
{
	int ret = disk_blk_part_validate(dev, start, blkcnt);

	if (ret)
		return ret;

	return blk_read(dev_get_parent(dev), disk_blk_part_offset(dev, start),
			blkcnt, buffer);
}

/*
 * Downstream safety patch, never to be upstreamed: this build's storage must be READ-ONLY, enforced by
 * the linker rather than by good behaviour. disk_blk_write() and
 * disk_blk_erase() are removed and .write/.erase are left out of the
 * KEEP'd blk_part_ops below, so the partition block devices carry no
 * write path and blk_write()/blk_erase() lose their last reference and
 * are dropped by --gc-sections. blk_dwrite() on a partition device then
 * fails with -ENOSYS at runtime (drivers/block/blk-uclass.c).
 */

UCLASS_DRIVER(partition) = {
	.id		= UCLASS_PARTITION,
	.per_device_plat_auto	= sizeof(struct disk_part),
	.name		= "partition",
};

static const struct blk_ops blk_part_ops = {
	.read	= disk_blk_read,
};

U_BOOT_DRIVER(blk_partition) = {
	.name		= "blk_partition",
	.id		= UCLASS_PARTITION,
	.ops		= &blk_part_ops,
};
