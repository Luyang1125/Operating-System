/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"
#include "inode.h"

//#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

#define SFS_SUPERBLOCK 0

typedef struct __attribute__((packed)) {
	uint32_t size_sfs;
	uint32_t num_data_blks; // Total number of data blocks on disk.
	uint32_t num_free_blks; // Total number of free blocks.
	uint32_t num_inodes; // Total number of inodes on disk.
	uint32_t bitmap_inode; //inode bitmap address
	uint32_t bitmap_datablock; //data block bitmap address
	uint32_t inode_root;  // Root directory.
	uint32_t datablock_start;
} sfs_superblock;

/*
 * Use the root directory to get the full path for the input relative path
 */
static void sfs_fullpath(char buffer[PATH_MAX], const char *path){
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());

    disk_open(SFS_DATA->diskfile);
    struct stat* statbuf = (struct stat*)malloc(sizeof(struct stat));
//	log_msg("start");
    int status = stat(SFS_DATA->diskfile, statbuf);
	log_stat(statbuf);
//	log_msg("\n	catched the diskfile with return %d", status);
			char ibitmap[SFS_INODES_NUM];
			char dbitmap[SFS_DATA_BLKS_NUM];
   		/**** Check for first time initialization.****/
		if (statbuf->st_size == 0) {
			log_msg("\n	start initialization");
    			/**** Step 1: Write super block to disk file ****/
			sfs_superblock SB = {
				.size_sfs = SFS_TOTAL_SIZE,
				.num_data_blks = SFS_DATA_BLKS_NUM,
				.num_free_blks = SFS_DATA_BLKS_NUM,
				.num_inodes = SFS_INODES_NUM,
				.bitmap_inode = SFS_INODE_BITMAP_BLKS, 
				.bitmap_datablock = SFS_DATA_BITMAP_BLKS,
				.inode_root = SFS_INODE,
				.datablock_start = SFS_DATA_BLK
			};
			block_write_padded(SFS_SUPERBLOCK, &SB, sizeof(sfs_superblock));
			//block_write(SFS_SUPERBLOCK, &SB);

			/**** Step 2: Write inode bitmap ****/
			//memset(ibitmap, "0", BLOCK_SIZE);
			memset(ibitmap, 1, sizeof(ibitmap));
			memset(ibitmap, 0, sizeof(char));
			block_write_padded(SFS_INODE_BITMAP_BLKS, ibitmap, sizeof(ibitmap));
			
			/**** Step 3: Write datablock bitmap ****/
			memset(dbitmap, 1, sizeof(dbitmap));
			block_write_padded(SFS_DATA_BITMAP_BLKS, dbitmap, sizeof(dbitmap));

			/**** Step 4:  Write inode blocks ****/
			char blk[BLOCK_SIZE];
			memset(blk, 0, sizeof(blk));
			int i;
			for(i = 0; i < SFS_INODE_BLKS_NUM; i++){
				log_msg("\n	writing the %d block(inode)", SFS_INODE + i);
				block_write(SFS_INODE + i, blk);
			}
			for(i = 0; i < SFS_DATA_BLKS_NUM; i++){
                                log_msg("\n     writing the %d block(datablock)", SFS_DATA_BLK + i);
                                block_write(SFS_DATA_BLK + i, blk);
                        }

			/**** Step 5: initialize inode root ****/
			sfs_inode_t ino_root;
			memset(&ino_root, 0, sizeof(ino_root));
			ino_root.iid = 0; 
			ino_root.flag = SFS_FLAG_DIR;
			ino_root.size = 0;
			ino_root.mode = S_IFDIR | 0755;
			ino_root.num_link = 0;
			ino_root.nblocks = 0;
			ino_root.accessed_time = ino_root.modified_time = ino_root.inode_modified_time = time(NULL);
			block_write(SFS_INODE, &ino_root);
		}
		else{
			log_msg("\n	the statbuf->st_size %d != 0", statbuf->st_size);
		}

		
			
		/***********check**********/
		char buffer[BLOCK_SIZE];
		sfs_superblock check;
		block_read(SFS_SUPERBLOCK, buffer);
		memcpy(&check, buffer, sizeof(check));
		log_msg("\n	supper block:\n               total size:%d\n               total data blks:%d\n               total inode number:%d\n               total free data blk number:%d\n               inode bitmap:%d\n               data blk bitmap:%d\n               inode root:%d\n               datablock begin:%d\n", check.size_sfs, check.num_data_blks, check.num_inodes, check.num_free_blks, check.bitmap_inode, check.bitmap_datablock, check.inode_root, check.datablock_start);
			
		block_read(SFS_INODE_BITMAP_BLKS, buffer);
		char ibitmap_check[SFS_INODES_NUM];
		memcpy(ibitmap_check, buffer, sizeof(ibitmap_check));
		int j;
		log_msg("\n	inode bitmap:");
		for(j = 0; j < SFS_INODES_NUM; j++){
			log_msg("%d", ibitmap_check[j]);
		}

		block_read(SFS_DATA_BITMAP_BLKS, buffer);
		char dbitmap_check[SFS_DATA_BLKS_NUM];
		memcpy(dbitmap_check, buffer, sizeof(dbitmap_check));
                log_msg("\n	datablock bitmap:");
                for(j = 0; j < SFS_DATA_BLKS_NUM; j++){
                        log_msg("%d", dbitmap_check[j]);
                }

		block_read(SFS_INODE, buffer);
		sfs_inode_t ino_root_check;
		memcpy(&ino_root_check, buffer, sizeof(ino_root_check));
		log_msg("\n\n	inode root:\n	inode num:%d\n           inode type:%d\n           inode time:%d\n           inode mode:%d\n at block %d", ino_root_check.iid, ino_root_check.flag, ino_root_check.accessed_time, ino_root_check.mode, SFS_INODE);
		/***************************/
			

	return SFS_DATA;
}
/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
    disk_close();

//	free(SFS_DATA->state_inodes);
//	SFS_DATA->state_inodes = NULL;

//  	free(SFS_DATA->state_data_blocks);
//  	SFS_DATA->state_data_blocks = NULL;

//  	SFS_DATA->free_inodes = NULL;
//  	SFS_DATA->free_data_blocks = NULL;
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int ino = 0;

int sfs_getattr(const char *path, struct stat *statbuf){
    int retstat = 0;
    char fpath[PATH_MAX];
   
  	log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);
	int sign = path_ino_mapping(ino, path);
	if(sign > SFS_INODES_NUM){
		log_msg("\n	parent ino %d\n", ino);
      		retstat = -ENOENT;
	}
	else{
		ino = sign;
		log_msg("\n	get inode %d\n", ino);
		sfs_inode_t file;
		get_inode(ino, &file);
		fill_stat_from_ino(&file, statbuf);
		log_stat(statbuf);
	}
	return retstat;
}    

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    //log_msg("mode %d\n", mode);
    	uint32_t test = create_inode(path, ino, mode);
    //path_ino_mapping(ino, path);
        log_msg("\n     get inode %d\n", ino);
        sfs_inode_t check;
        get_inode(ino, &check);
	log_msg("\ncheck !!! inode id %d, flag %d, mode %d, num links %d: link1-%d, name1-%s", check.iid, check.flag, check.mode, check.num_link, check.blk_ptrs[0], check.ptr_name[0]);
	log_fi(fi);
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_msg("\n     path %s", path);

    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
  log_msg("\nI can print something??\n");
    int retstat = -ENOENT;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

        path_ino_mapping(ino, path);
	//if file exist
	if(ino < SFS_INODES_NUM){
		sfs_inode_t file;
		get_inode(ino, &file);
		//if file can be access
		if(S_ISREG(file.flag)){
			retstat = 0;
		}
	}
	return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
	ino = 0;

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_fi(fi);
        log_msg("\n     path %s", path);


	return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

        path_ino_mapping(ino, path);
	sfs_inode_t file;
	get_inode(ino, &file);
	if(file.flag == SFS_FLAG_FILE){
		//if inode is full, step into next inode
		while(file.num_link == SFS_PTR_NUM_PER_INODE){
			get_inode(file.blk_ptrs[SFS_PTR_NUM_PER_INODE - 1], &file);
		}
		if(file.num_link == SFS_PTR_NUM_PER_INODE - 1){
			//if rest of space is smaller than the needed
			if(file.num_link * BLOCK_SIZE - file.size < strlen(buf)){
				//do rest of block buf
				//create new datablock and pointers
			}
		}

	}	

    return retstat;
}


/** Create directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_msg("\n     path %s", path);


    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_msg("\n     path %s", path);
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
//                fill_fi_from_ino(&file, fi);
        }
        log_fi(fi);
        log_msg("\n     path %s", path);

	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 *
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\nsfs_readdir(path=\"%s\")\n", path);

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_fi(fi);
        log_msg("\n     path %s", path);


    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

        path_ino_mapping(ino, path);
        if(ino < SFS_INODES_NUM){
                log_msg("\n     get inode %d", ino);
                sfs_inode_t file;
                get_inode(ino, &file);
        }
        log_fi(fi);
        log_msg("\n     path %s", path);
    

    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

/*
  Find a free inode through inode bitmap
 */
int find_free_inode(){
  char inodeBitMap[BLOCK_SIZE];
  block_read(SFS_INODE_BITMAP_BLKS, inodeBitMap);
  int i;
  for(i = 0; i < SFS_INODES_NUM; i++){
    if(inodeBitMap[i] == '1'){
      return i;
    }else{
      perror("No available iNode!\n");
    }
  }
}

/*
  Find a free data block through datablcok bitmap
 */
int find_free_datablock(){
  char datablockBitMap[BLOCK_SIZE];
  block_read(SFS_DATA_BITMAP_BLKS, datablockBitMap);
  int i;
  for(i= 0; i < SFS_DATA_BLKS_NUM; i++){
    if(datablockBitMap[i] == '1'){
      return i;
    }else{
      perror("No available data block!\n");
    }
  }
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = realpath(argv[argc-2], NULL); // Save the absolute path of the diskfile
    printf("%s", sfs_data->diskfile);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
//    sfs_data->free_data_blocks = NULL;
//    sfs_data->free_inodes = NULL;
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
