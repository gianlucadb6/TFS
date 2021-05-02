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
		fprintf( stderr, "im in %s %d \n", "function", 1);


	// Step 1: Read inode bitmap from disk
	unsigned char ibm[BLOCK_SIZE];
	bio_read(1, (void*)ibm);
	
	// Step 2: Traverse inode bitmap to find an available slot
	int i = 1; 
	int flag = 0;
	while(i<1024) {
		if(get_bitmap(ibm, i) == 0){ 
			flag =1; 
			break;
		}
		i++;
	}	

	// Step 3: Update inode bitmap and write to disk 
	if(flag){
	set_bitmap(ibm, i);
	bio_write(1, (void*)ibm);
	return i; 
	} 

	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
			fprintf( stderr, "im in %s %d \n", "function", 2);



	// Step 1: Read data bitmap from disk
	unsigned char dbm[BLOCK_SIZE];
	bio_read(2, (void*)dbm);
	
	// Step 2: Traverse data bitmap to find an available slot
	int i = 0;
	int flag = 0;

	while(i<16384) {
		if(get_bitmap(dbm, i) == 0){ // find first open spot in bm 
			flag =1; 
			break;
		}
		i++;
	}	

	// Step 3: Update data bitmap and write to disk 
	
	if(flag){
		set_bitmap(dbm, i);
		bio_write(2, (void*)dbm);
		return i; 
	} 
	
	return -1; // if no spots open flag wont be set return -1
	
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
		fprintf( stderr, "im in %s %d \n", "function", 3);



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
	fprintf( stderr, "im in %s %d \n", "function", 4);



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
		fprintf( stderr, "im in %s %d \n", "function", 5);



	// Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* in = malloc(sizeof(struct inode));
	readi(ino, in);
	
	int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); // corrected computation
	//int rootDBlock = 3 + numBlocksForInodes; 
	
	// Step 2: Get data block of current directory from inode
	int i;
	int numDirent = floor(BLOCK_SIZE/sizeof(struct dirent));
	for(i = 0; i < 16; ++i) {
		int block = in->direct_ptr[i];
		if(block == -1){ // leave if you hit an empty block 
			continue;
		}
		char buf[BLOCK_SIZE];
		bio_read(3 + numBlocksForInodes+block, (void*)buf); // need to read from data block section
		int j;
		
		// Step 3: Read directory's data block and check each directory entry.
		//If the name matches, then copy directory entry to dirent structure
		for(j = 0; j < numDirent; ++j) {
			struct dirent* d = (struct dirent*) &(buf[sizeof(struct dirent) * j]); 
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
		fprintf( stderr, "im in %s %d \n", "function", 6);



	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	int i; 
	int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); 
	int dirent_per_block = floor(BLOCK_SIZE/sizeof(struct dirent) );
	for(i=0;i<16;i++){ 
		if(dir_inode.direct_ptr[i]!= -1) {
		char buf[BLOCK_SIZE];
		bio_read(3 + numBlocksForInodes+dir_inode.direct_ptr[i], (void*)buf); // need to read from data block section
		int j;
		
		struct dirent * ptr = (struct dirent *)buf; 
		for(j=0;j<dirent_per_block;j++){ 

// Step 2: Check if fname (directory name) is already used in other entries
				if(strcmp((ptr+j)->name,fname) == 0 ){ 
						perror("duplicate name"); 
						return -1;
				}
			}
		}
	}


	// Step 3: Add directory entry in dir_inode's data block and write to disk
	for(i=0;i<16;i++){ 
		if(dir_inode.direct_ptr[i]!= -1) {
		
		char buf[BLOCK_SIZE];
		bio_read(3 + numBlocksForInodes+dir_inode.direct_ptr[i], (void*)buf); // need to read from data block section
		int j;
		struct dirent * ptr = (struct dirent *)buf; 
		for(j=0;j<dirent_per_block;j++){ 

// Step 2: Check if fname (directory name) is already used in other entries
				if((ptr+j)->valid == 1025 ){ 
					(ptr +j)->valid = 1; 
					(ptr+j)->ino  = f_ino;
					strncpy((ptr+j)->name,fname,name_len); 
					(ptr+j)->len = name_len;
					bio_write( 3 + numBlocksForInodes+dir_inode.direct_ptr[i] ,buf); 
					return 1;
				}
			}
		
		} 
	else if( dir_inode.direct_ptr[i]== -1){ 
		int new_block = get_avail_blkno(); 
		if(new_block  == -1){ 
				return -1;
		} 
		
		dir_inode.direct_ptr[i] = new_block; // new block
		writei(dir_inode.ino,&dir_inode );

		char buf[BLOCK_SIZE]; 	
		bio_read(3 + numBlocksForInodes+ new_block, (void*)buf); 
		struct dirent * ptr =  (struct dirent *)buf; 
		
		ptr->valid =1; 
		strncpy((ptr)->name,fname,name_len); 
		(ptr)->ino = f_ino; 
		(ptr)->len = name_len;
			int j; 
		//	dir_entry =	(struct dirent*) &(buf[j*sizeof(struct dirent)]);

			for(j=1;j<dirent_per_block;j++){ 
					(ptr+j)->valid = 1025; 
					(ptr+j)->len = 1025; 
					(ptr+j)->name[0] = '\0'; 
					(ptr+j)->ino = 1025;
			}
		
		bio_write( 3 + numBlocksForInodes+new_block ,buf);  
		
			return 1;
		
	} 
	return -1;
}


	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {
		fprintf( stderr, "im in %s %d \n", "function", 7);

	int i; 
	int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); 
	int dirent_per_block = floor(BLOCK_SIZE/sizeof(struct dirent) );
	int flag = 0;
	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
for(i=0;i<16;i++){ 
		if(dir_inode.direct_ptr[i]!= -1) {
		char buf[BLOCK_SIZE];
		bio_read(3 + numBlocksForInodes+dir_inode.direct_ptr[i], (void*)buf); // need to read from data block section
		int j;
		
		struct dirent * ptr = (struct dirent *)buf; 
		for(j=0;j<dirent_per_block;j++){ 
	// Step 2: Check if fname exist

				if(strcmp((ptr+j)->name,fname) == 0 ){ 
// Step 3: If exist, then remove it from dir_inode's data block and write to disk					
						(ptr+j)->ino = 1025; 
						(ptr+j)->valid = 1025;
						(prt+j)->len = 1025 
						memset((ptr+j)->name,'\0',208);
						bio_write(3 + numBlocksForInodes+dir_inode.direct_ptr[i], (void*)buf); 
						return 0;
				}
			}
		}
	}





	return 0; // returns if dir entry does not exist
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	fprintf( stderr, "im in %s %d \n", "function", 8);



	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
/*
int i = 0; 
	// we need to bound the length of the search
	while(path[i] != '/' &&  path[i] != '\0') {
		++i;
	}
	char* fname = malloc(sizeof(char)*(i+2));
	strncpy(fname, path, i+2);	
	struct dirent* dir = malloc(sizeof(struct dirent));
	if(dir_find(ino, fname, strlen(fname), dir) < 0) {
		return -1;	
	}
	struct inode* in = malloc(sizeof(struct inode));
	readi(dir->ino, in);
	if(in->type == IS_REG_FILE ||in->type == IS_DIR  ) {
		memcpy(inode, in, sizeof(struct inode));
	}else {
		path += i + 1;
		get_node_by_path(path, dir->ino, inode);
	}
	return 0; */

	if( path == NULL || strcmp(path,"") ==0 || *(path) != '/' ){ 
		perror("bad path");
		return -1;
	}  
	
	struct dirent* root_dir = (struct dirent *)malloc(sizeof(struct dirent));
	char root_name [] = "/";
	if(dir_find(0, root_name, 1, root_dir) < 0) {
		return -1;	
	}
	struct inode* in_root = malloc(sizeof(struct inode));
	readi(root_dir->ino, in_root);
	free(root_dir);
	
	if(strcmp(root_name,path) ==0){ 
			memcpy(inode, in_root, sizeof(struct inode));
			free(in_root);
			return 1;
		}

	int j = 0;  // do this to account for / being at the start of the path
	const char * ptr = path;
	const char * ptr2 = path; 
	ptr++;
	ptr2++;
	printf("input path: %s\n",path);
	// here we can get the root inode
	
	int ino_iterator = in_root->ino; // we start at roots inode num
	free(in_root);
	while( *(ptr) != '\0'){ 
	//	printf("%s \n",ptr);
		j=0;
		while(*(ptr2 +j)!= '\0'){
				if(*(ptr2+j) == '/' ){ 
					j++;
					break;
				}  
				j++;
			}  
	
		if(*(ptr2+j-1) == '/'){ // handles case where we can find a '/' at the end
		char * path_piece = (char *)malloc( sizeof(char)*(j+1)); 
		*(path_piece+j-1) = '\0'; 
		
		strncpy(path_piece,ptr,j-1); 
	// we can get the dir name we want to search here	
			struct dirent* dir = (struct dirent *)malloc(sizeof(struct dirent));
			
			if(dir_find(ino_iterator,path_piece, strlen(path_piece), dir) < 0) {
				return -1;	
			}
			struct inode* in_cur = malloc(sizeof(struct inode));
			readi(dir->ino, in_cur);
			free(dir);
			ino_iterator = in_cur->ino;
			
			printf("name %s %ld length \n",path_piece,strlen(path_piece));
			
			free(in_cur);
			free(path_piece);
			
			ptr= ptr+j;
			ptr2 = ptr;	 
		} 
		else if(*(ptr2+j-1)!= '/' ){ // handles the case in which we can't find a '/' at the end 
		char * path_piece = (char *)malloc( sizeof(char)*(j)); 
		*(path_piece+j) = '\0'; 
		
		strncpy(path_piece,ptr,j); 
			struct dirent* dir = (struct dirent *)malloc(sizeof(struct dirent));
			
			if(dir_find(ino_iterator,path_piece, strlen(path_piece), dir) < 0) {
				return -1;	
			}
			struct inode* in_cur = malloc(sizeof(struct inode));
			readi(dir->ino, in_cur);
			free(dir);
		
		printf("name %s %ld length \n",path_piece,strlen(path_piece));
		memcpy(inode, in_cur, sizeof(struct inode));
		free(in_cur); 	
		free(path_piece);
		return 1;
	//	ptr= ptr+j;
	//	ptr2 = ptr;	 

		}
	}
return -1;	

}

/* 
 * Make file system
 */
int tfs_mkfs() {
		fprintf( stderr, "im in %s %d \n", "function", 9);



	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	struct superblock* sb = (struct superblock *)malloc(sizeof(struct superblock));
	sb->magic_num = 6; 
	sb->max_inum = MAX_INUM;
	sb->max_dnum = MAX_DNUM;
	sb->i_bitmap_blk = 1;
	sb->d_bitmap_blk = 2;
	sb->i_start_blk = 3;
	sb->d_start_blk = 3 + ceil((MAX_INUM * sizeof(struct inode)) / BLOCK_SIZE);
	bio_write(0, (void*)sb);	
	free(sb);
	//free sb?

	// initialize inode bitmap
	
	unsigned char ibm[BLOCK_SIZE]; 
	unsigned char dbm[BLOCK_SIZE]; 
	memset(ibm,'\0',BLOCK_SIZE); 
	memset(dbm,'\0',BLOCK_SIZE); 
	set_bitmap(ibm, 0);
 	set_bitmap(dbm, 0);
	bio_write(1, (void*)ibm);
	bio_write(2, (void*)dbm);

	// update bitmap information for root directory

int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); // corrected computation
	
	int rootDBlock = 3 + numBlocksForInodes; // first block of data blocks region
	char buf[BLOCK_SIZE];
	
	bio_read(rootDBlock, (void*)buf);
	int direntsPerBlock = floor(BLOCK_SIZE/sizeof(struct dirent));
		struct dirent* entry = (struct dirent*) &(buf[0*sizeof(struct dirent)]);
			entry->valid = 1; 
			entry->ino = 0; 
			memset(entry->name,'\0',208); 
			entry->name[0] = '/';
			entry->len = 1; 

	for(int i = 1; i < direntsPerBlock; i++) { 
	
		struct dirent* entry = (struct dirent*) &(buf[i*sizeof(struct dirent)]);
		entry->valid = 1025; 
		entry->ino = 1025; 
		memset(entry->name,'\0',208); 
		entry->len = 1025;
			}
	
	bio_write(rootDBlock,buf);
	
	// update inode for root directory
	struct inode* rootInode = malloc(sizeof(struct inode));
	rootInode->ino = 0;
	rootInode->valid = 1; 
	rootInode->size = 0; 
	rootInode->type = IS_DIR;
	rootInode->link = 2; 
	rootInode->direct_ptr[0] = 0;
	
	for(int i = 1; i < 16; i++) {
				rootInode->direct_ptr[i] = -1;
	}
		
	rootInode->vstat.st_mode = S_IFDIR | S_IRWXU;	
	rootInode->vstat.st_uid = getuid(); 
	rootInode->vstat.st_gid = getgid(); 
	rootInode->vstat.st_size = 1*BLOCK_SIZE;
	rootInode->vstat.st_nlink = 2; 
	rootInode->vstat.st_atime = time(NULL); 
	rootInode->vstat.st_mtime = time(NULL); 

	writei(0, rootInode);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
		fprintf( stderr, "im in %s %d \n", "function", 10);



	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path) < 0) {
		tfs_mkfs();
	}
	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk
	// struct superblock* superBlock;

	 superBlock = ( struct superblock *)malloc(BLOCK_SIZE);
	bio_read(0, (void*)superBlock);


	return NULL;
}

static void tfs_destroy(void *userdata) {
		fprintf( stderr, "im in %s %d \n", "function", 11);



	// Step 1: De-allocate in-memory data structures
			
	// Step 2: Close diskfile
		dev_close();
}

static int tfs_getattr(const char *path, struct stat *stbuf) {
	fprintf( stderr, "im in %s %d \n", "function", 12);



	// Step 1: call get_node_by_path() to get inode from path
	struct inode* inode = malloc(sizeof(struct inode));
	
	if(get_node_by_path(path, 0, inode)<0){ 
		return -ENOENT; // returning the proper erronos is KEYYY
	}

	// Step 2: fill attribute of file into stbuf from inode
	stbuf->st_ino = inode->ino;
	stbuf->st_nlink = inode->link;
	int i;
	int size = 0;
	for(i=0;i<16;i++){ 
			if(inode->direct_ptr[i] != -1){ 
			size++; 
			}
	}	 
	stbuf->st_uid = getuid(); 
	stbuf->st_gid = getgid();
	stbuf->st_size = size * BLOCK_SIZE;
	stbuf->st_blksize = BLOCK_SIZE;
	stbuf->st_blocks = size;
	stbuf->st_mode = S_IFDIR | S_IRWXU;	
	stbuf->st_atime = time(NULL); 
	stbuf->st_mtime = time(NULL); 



	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
	fprintf( stderr, "im in %s %d \n", "function", 13);


	// Step 1: Call get_node_by_path() to get inode from path
	struct inode * get_inode = (struct inode *)malloc(sizeof(struct inode));
	
	int retval  = get_node_by_path( path,0,get_inode);
	
	free(get_inode);
	if( retval <0){ 
		return -1;
	}
	
	// Step 2: If not find, return -1

	return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	fprintf( stderr, "im in %s %d \n", "function", 14);

	// Step 1: Call get_node_by_path() to get inode from path
		struct inode * get_inode = (struct inode *)malloc(sizeof(struct inode));
	
		get_node_by_path( path,0,get_inode);
		
				 
		
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	
	int i,j;  
		int dirent_per_block  = floor(BLOCK_SIZE/sizeof(struct dirent));
		int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); 
		char buf[BLOCK_SIZE];
		struct dirent * dir_entry;
		for(i=0;i<16;i++){ 
			if(get_inode->direct_ptr[i]!=-1){
			bio_read( 3+ get_inode->direct_ptr[i] + numBlocksForInodes,(void *)buf);
			for(j=0;j<dirent_per_block;j++){ 
				dir_entry =	(struct dirent*) &(buf[j*sizeof(struct dirent)]);
				if((dir_entry)->len!= 1025){
						char name_hold[208]; 
						char r_name[2] = {'/','\0'};
						memset(name_hold,'\0',208);
						strncpy(name_hold,(dir_entry)->name,(dir_entry)->len);
							if(strcmp(name_hold,r_name)!= 0){
								filler( buffer, name_hold, NULL, 0 ); 
								}	
						}
				} 
		 }
	}
	/* 
//if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
//	{
		filler( buffer, "file54", NULL, 0 );
		filler( buffer, "file349", NULL, 0 );
	//}
*/
	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
//	printf("fifteen here \n");	
	fprintf( stderr, "im in %s %d \n", "function", 15);
	
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	const char * ptr = path; 
	int i = 0;
	int slash_count  =0;
	while(*(ptr+i )!= '\0' ){ 
			if(*(ptr+i) == '/'){ 
				slash_count++;
			} 
		i++;
	} 
	int k =0;	
	int store_s = slash_count;
	while(*(ptr+k )!= '\0' ){ 
			if(*(ptr+k) == '/'){ 
				store_s--;
			}  
			if(store_s == 0){ 
				break;
			}
		k++;
	} 
	
	char * parent = (char *)malloc(sizeof(char) *(k+2)); 
	memset(parent,'\0',2);
	char * new_child = ( char *)malloc(sizeof(char) *( (strlen(path) - k) + 1)); 
	memset(new_child,'\0',strlen(path) - k );


	strncpy(parent,path,k+1); 
	if(*(parent+k) == '/' ){ 
	*(parent+k) = '\0';
	}


	ptr = ptr+k+1;
	strncpy(new_child,ptr,strlen(path)-k); 

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode * parent_node = (struct inode *)malloc(sizeof(struct inode));  	 
	get_node_by_path(parent,0,parent_node);
	// Step 3: Call get_avail_ino() to get an available inode number
	int child_ino =  get_avail_ino(); 
	if(child_ino == -1){ 
			return -1;
	}
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*parent_node, child_ino, new_child, strlen(new_child));
	// Step 5: Update inode for target directory
	
	struct inode * child_node = (struct inode *)malloc(sizeof(struct inode)); 
	readi(child_ino,child_node); 
	
	child_node->ino =  child_ino; 
	child_node->valid = 1; 
	child_node->size = 1; // one block to start
	child_node->type = IS_DIR; 
	child_node->link =2;
	child_node->direct_ptr[0] = get_avail_blkno(); // first block needed for 
	int m;
	
	for(m=1;m<16;m++){ 
		child_node->direct_ptr[m] = -1;
	}
	
	int numBlocksForInodes = floor((sizeof(struct inode)*MAX_INUM)/BLOCK_SIZE ); 
	int dirent_per_block = floor(BLOCK_SIZE/sizeof(struct dirent) );
	
	char databuf[BLOCK_SIZE];
	bio_read( child_node->direct_ptr[0]+3+numBlocksForInodes,databuf);
	
	// initalizes the dirent in the first data block to standard default values
	struct dirent * dir_ptr = (struct dirent *)databuf; 
	int l;
	for(l=0;l<dirent_per_block;l++){ 
			(dir_ptr+l)->valid = 1025; 
			(dir_ptr+l)->ino = 1025;
			(dir_ptr+l)->len = 1025; 
			memset((dir_ptr+l)->name,'\0',208);
	}
	
	bio_write( child_node->direct_ptr[0]+3+numBlocksForInodes,databuf);
	
	child_node->vstat.st_mode = S_IFDIR | S_IRWXU; //might want to set this to input mode_t	
	child_node->vstat.st_uid = getuid(); 
	child_node->vstat.st_gid = getgid(); 
	child_node->vstat.st_size = 1*BLOCK_SIZE;
	child_node->vstat.st_nlink = 2; 
	child_node->vstat.st_atime = time(NULL); 
	child_node->vstat.st_mtime = time(NULL); 

	// Step 6: Call writei() to write inode to disk
	writei(child_ino,child_node);


	return 0;
}

static int tfs_rmdir(const char *path) {
	fprintf( stderr, "im in %s %d \n", "function", 16);
	
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	const char * ptr = path; 
	int i = 0;
	int slash_count  =0;
	while(*(ptr+i )!= '\0' ){ 
			if(*(ptr+i) == '/'){ 
				slash_count++;
			} 
		i++;
	} 
	int k =0;	
	int store_s = slash_count;
	while(*(ptr+k )!= '\0' ){ 
			if(*(ptr+k) == '/'){ 
				store_s--;
			}  
			if(store_s == 0){ 
				break;
			}
		k++;
	} 
	
	char * parent = (char *)malloc(sizeof(char) *(k+2)); 
	memset(parent,'\0',2);
	char * new_child = ( char *)malloc(sizeof(char) *( (strlen(path) - k) + 1)); 
	memset(new_child,'\0',strlen(path) - k );


	strncpy(parent,path,k+1); 
	if(*(parent+k) == '/' ){ 
	*(parent+k) = '\0';
	}


	ptr = ptr+k+1;
	strncpy(new_child,ptr,strlen(path)-k); 


	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode * child_node = (struct inode *)malloc(sizeof(struct inode));	
	get_node_by_path(path, 0, child_node); 
	
	// Step 3: Clear data block bitmap of target directory
	
	// Step 4: Clear inode bitmap and its data block
	

	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode * parent_node = (struct inode *)malloc(sizeof(struct inode));	
	get_node_by_path(parent, 0, parent_node); 
	
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
 		dir_remove(parent_node, new_child, str_len(new_child)); 
	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	fprintf( stderr, "im in %s %d \n", "function", 17);
	
	printf("yo\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
fprintf( stderr, "im in %s %d \n", "function", 18);
	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
fprintf( stderr, "im in %s %d \n", "function", 19);

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
fprintf( stderr, "im in %s %d \n", "function", 20);


	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {
	fprintf( stderr, "im in %s %d \n", "function", 21);


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
	printf("in tfs.c main\n");
	int fuse_stat;
	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");
	printf("%s \n",diskfile_path);
	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);
	printf("made it here\n");
	return fuse_stat;
}

