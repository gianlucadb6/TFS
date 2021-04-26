/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26
#define IS_DIR 0
#define IS_REG_FILE 1

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock* superBlock;
struct inode* rootInode;
bitmap_t* inBM;
bitmap_t* dBM;
/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	
	// Step 1: Read inode bitmap from disk
	char buf[BLOCK_SIZE];
	bio_read(1, (void*)buf);
	bitmap_t* bitmap  = malloc(BLOCK_SIZE);
	memcpy(bitmap, buf, BLOCK_SIZE);
	
	// Step 2: Traverse inode bitmap to find an available slot
	int i = 0;
	while(get_bitmap(*bitmap, i) == 1) {
		++i;
	}	

	// Step 3: Update inode bitmap and write to disk 
	set_bitmap(*bitmap, i);
	bio_write(1, (void*)bitmap);
	free(bitmap);
	return i;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	
	// Step 1: Read data bitmap from disk
	char buf[BLOCK_SIZE];
	bio_read(2, (void*)buf);
	bitmap_t* bitmap  = malloc(BLOCK_SIZE);
	memcpy(bitmap, buf, BLOCK_SIZE);
	
	// Step 2: Traverse data bitmap to find an available slot
	int i = 0;
	while(get_bitmap(*bitmap, i) == 1) {
		++i;
	}	

	// Step 3: Update data bitmap and write to disk 
	set_bitmap(*bitmap, i);
	bio_write(2, (void*)bitmap);
	free(bitmap);
	return i;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
	int inodesPblock = floor(BLOCK_SIZE / sizeof(struct inode));
	int blockNum = (int)(ino/inodesPblock);
	//int n = (ino * sizeof(struct inode))/BLOCK_SIZE floored?^^^ 	

	// Step 2: Get offset of the inode in the inode on-disk block
	int inodeOffset = ino % inodesPblock;	

	// Step 3: Read the block from disk and then copy into inode structure
	char buf[BLOCK_SIZE];
	bio_read(3+blockNum, (void*)buf);
	struct inode* modiNode = (struct inode*) &(buf[inodeOffset*sizeof(struct inode)]);
	memcpy(inode, modiNode, sizeof(struct inode));

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int inodesPblock = floor(BLOCK_SIZE / sizeof(struct inode));
	int blockNum = (int)(ino/inodesPblock); 	

	// Step 2: Get the offset in the block where this inode resides on disk
	int inodeOffset = ino % inodesPblock;

	// Step 3: Write inode to disk 
	char buf[BLOCK_SIZE];
	bio_read(3+blockNum, (void*)buf);
	struct inode* modiNode = (struct inode*) &(buf[inodeOffset*sizeof(struct inode)]);
	memcpy(modiNode, inode, sizeof(struct inode));
	bio_write(3+blockNum, (void*)buf);

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* in = malloc(sizeof(struct inode));
	readi(ino, in);
	
	// Step 2: Get data block of current directory from inode
	int i;
	int numDirent = floor(BLOCK_SIZE/sizeof(struct dirent));
	for(i = 0; i < 16; ++i) {
		int block = in->direct_ptr[i];
		char buf[BLOCK_SIZE];
		bio_read(block, (void*)buf);
		int j;
		
		// Step 3: Read directory's data block and check each directory entry.
		//If the name matches, then copy directory entry to dirent structure
		for(j = 0; j < numDirent; ++j) {
			struct dirent* d = (struct dirent*) &(buf[sizeof(struct dirent) * i]); 
			if(name_len == d->len) {
				if(strcmp(fname, d->name) == 0) {
					memcpy(dirent, d, sizeof(struct dirent));
					return 0;
				}
			}
		}
	}

	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode

	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode

	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {

	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	int i = 0;
	while(path[i] != '/' || path[i] != '\0') {
		++i;
	}
	char* fname = malloc(sizeof(char)*(i+1));
	strncpy(fname, path, i);	
	struct dirent* dir = malloc(sizeof(struct dirent));
	if(dir_find(ino, fname, strlen(fname), dir) < 0) {
		return -1;	
	}
	struct inode* in = malloc(sizeof(struct inode));
	readi(dir->ino, in);
	if(in->type == IS_REG_FILE) {
		memcpy(inode, in, sizeof(struct inode));
	}else {
		path += i + 1;
		get_node_by_path(path, dir->ino, inode);
	}
	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	struct superblock* sb = malloc(sizeof(struct superblock));
	sb->magic_num = 6; 
	sb->max_inum = MAX_INUM;
	sb->max_dnum = MAX_DNUM;
	sb->i_bitmap_blk = 1;
	sb->d_bitmap_blk = 2;
	sb->i_start_blk = 3;
	sb->d_start_blk = 3 + ceil((MAX_INUM * sizeof(struct inode)) / BLOCK_SIZE);
	bio_write(0, (void*)sb);	
	//free sb?

	// initialize inode bitmap
	int total = ceil(MAX_INUM / 8);
	char ibm[total];//dynamically allocate?
	for(int i = 0; i < total; ++i) {
		ibm[i] = '\0';
	}
	bio_write(1, (void*)ibm);

	// initialize data block bitmap
	total = ceil(MAX_DNUM / 8);
	char dbm[total];//dynam?
	for(int i = 0; i < total; ++i) {
		dbm[i] = '\0';
	}
	bio_write(2, (void*)dbm);

	// update bitmap information for root directory

	//inode 0 set to inuse for root
	total = ceil(MAX_INUM / 8);
	bitmap_t inodeBM = (unsigned char*)malloc(total); 
	bio_read(1, inodeBM);
	set_bitmap(inodeBM, 0);
	bio_write(1, (void*)inodeBM);
	free(inodeBM);
	//data block 0 set to inuse for root
	total = ceil(MAX_DNUM / 8);
	bitmap_t dataBM = (unsigned char*)malloc(total);
	bio_read(2, dataBM);
	set_bitmap(dataBM, 0);
	bio_write(2, (void*)dataBM);
	free(dataBM);
	//partition root's data block for dirent entries
	int numBlocksForInodes = floor(BLOCK_SIZE / sizeof(struct inode)); 
	int rootDBlock = 3 + numBlocksForInodes;
	char* buf[BLOCK_SIZE];
	bio_read(rootDBlock, (void*)buf);
	int direntsPerBlock = floor(BLOCK_SIZE/sizeof(struct dirent));
	for(int i = 0; i < direntsPerBlock; ++i) {
		struct dirent* entry = (struct dirent*) &(buf[i*sizeof(struct dirent)]);
		entry->valid = -1;	
	}

	// update inode for root directory
	struct inode* rootInode = malloc(sizeof(struct inode));
	rootInode->ino = 0;
	rootInode->valid = 1; 
	rootInode->size = 0; 
	rootInode->type = IS_DIR;
	rootInode->link = 2; 
	for(int i = 0; i < 16; ++i) {
		if(i==0) {
			rootInode->direct_ptr[i] = rootDBlock;
		}
		rootInode->direct_ptr[i] = -1;
	}
	time_t current_time = time(NULL);
	rootInode->vstat.st_atime = current_time;
	rootInode->vstat.st_mtime = current_time;	
	writei(0, rootInode);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path) < 0) {
		tfs_mkfs();
	}
	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk
	bio_read(0, (void*)superBlock);


	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	//get_node_by_path(path);

	// Step 2: fill attribute of file into stbuf from inode

	stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_nlink  = 2;
	time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk


	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;
	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);
	return fuse_stat;
}

