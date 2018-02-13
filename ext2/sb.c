/*
 * Phoenix-RTOS
 *
 * ext2
 *
 * sb.c
 *
 * Copyright 2017 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdlib.h>

#include "ext2.h"
#include "sb.h"
#include "mbr.h"
#include "block.h"
#include "pc-ata.h"


int ext2_read_sb(u32 sect)
{

	int ret = 0;
	ext2->sb = malloc(SUPERBLOCK_SIZE);
	/* this is assumption that sector size is 512,
	 * superblock starts on 1024 byte of ext2 partition */
	ret = ata_read((sect + 2) * SECTOR_SIZE, (char *)ext2->sb, SUPERBLOCK_SIZE);

	if (ret != SUPERBLOCK_SIZE) {
		printf("ext2: superblock read error %d\n", ret);
		goto error;
	}
	if (ext2->sb->magic != EXT2_MAGIC) {
		printf("ext2: not an ext2 partition 0x%x\n", ext2->sb->magic);
		goto error;
	}

	return EOK;

error:
	free(ext2->sb);
	ext2->sb = NULL;
	return -EFAULT;
}

int ext2_write_sb(void)
{
	int ret = 0;

	ret = ata_write((ext2->sb_sect + 2) * SECTOR_SIZE, (char *)ext2->sb, SUPERBLOCK_SIZE);

	if (ret != SUPERBLOCK_SIZE) {
		printf("ext2: superblock write error %d\n", ret);
		return -EFAULT;
	}

	return EOK;
}

void ext2_init_sb(int pentry)
{

	ext2->block_size = SUPERBLOCK_SIZE << ext2->sb->log_block_size;
	ext2->blocks_count = ext2->sb->blocks_count;
	ext2->blocks_in_group = ext2->sb->blocks_in_group;
	ext2->inode_size = ext2->sb->inode_size;
	ext2->inodes_count = ext2->sb->inodes_count;
	ext2->inodes_in_group = ext2->sb->inodes_in_group;
	ext2->gdt_size = 1 + (ext2->sb->blocks_count - 1) / ext2->sb->blocks_in_group;
	ext2->gdt =  malloc(ext2->gdt_size * sizeof(group_desc_t));
	if(ext2->mbr != NULL)
		ext2->first_block = (ext2->mbr->pent[pentry].first_sect_lba * SECTOR_SIZE) + BLOCKBASE;
	else
		ext2->first_block = BLOCKBASE;

	read_blocks(2, ext2->gdt, ext2->gdt_size * sizeof(group_desc_t));

	//TODO: features check
}


int ext2_init(void)
{
	int i = 0;
	int ret = -EFAULT;

	ext2->mbr = read_mbr();
	if (!ext2->mbr) {
		printf("ext2: no mbr found %s\n", "");
		/* even if there is no partition table we can still
		 * try to read superblock */
		if (ext2_read_sb(0) == EOK) {
			ext2->sb_sect = 0;
			ext2_init_sb(i);
			return EOK;
		}
		return -EFAULT;
	}

	//check_partitions
	/* check for protective mbr */
	if (ext2->mbr->pent[0].type == PENTRY_PROTECTIVE) {
		/* this is where gpt should be read
		 * but we do not support it right now */
		printf("ext2: gpt is not supported... %s\n", "");
		return -EFAULT;
	}

	/* we only mount first ext2 partition found in mbr */
	for (i = 0; i < 4; i++) {
		if(ext2->mbr->pent[i].type == PENTRY_LINUX) {
			if ((ret = ext2_read_sb(ext2->mbr->pent[i].first_sect_lba)) == EOK) {
				ext2->sb_sect = ext2->mbr->pent[i].first_sect_lba;
				break;
			}
		}
	}

	ext2_init_sb(i);
	return ret;
}