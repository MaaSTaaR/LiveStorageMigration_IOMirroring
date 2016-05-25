/**
 * Live Storage Migration - I/O Mirroring Implemetation of VMWare ESX (http://xenon.stanford.edu/~talg/papers/USENIXATC11/atc11-svmotion.pdf)
 *
 * Mohammed Q. Hussain (http://www.maastaar.net) under supervision of Prof. Hussain Almohri (http://www.halmohri.com)
 *
 * License: GNU GPL
 *
 */
 
// [MQH] This code is based on Miklos Szeredi <miklos@szeredi.hu> work "fusexmp.c". The original code licensed under GNU GPL.
// It has been modified to work as I/O Mirror for live storage migration by Mohammed Q. Hussain.
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include <unistd.h>
					
// [MQH]
char REAL_PATH[ 500 ], DESTINATION_PATH[ 500 ];
long writeCounter = 1;
int client, connectionRes = -1, initialCopyDone = 0;

// [MQH] 1 April 2016
off_t stringToOffset( const char* str ) 
{
    off_t ret;
    int size = strlen( str );
    int i;
    
    for ( i = ( size - 1 ); i >= 0; i-- ) 
    {
    	int currDigit = ( str[ i ] - '0' );
    	off_t magnitude = pow( 10, ( double ) ( size - ( i + 1 ) ) );
    	
    	if ( i != ( size - 1 ) )
	    	ret += currDigit * magnitude;
	    else
	    	ret = currDigit;
    }
    
    return ret;
}

const char *full(const char *path) /* add mountpoint to path */;//shut up bogus gcc warnings

const char *getFullPath( const char *path, const char *dir )
{
	// This part from the original code
	char *ep, *buff;

	buff = strdup(path+1); if (buff == NULL) exit(1);

	ep = buff + strlen(buff) - 1; if (*ep == '/') *ep = '\0'; /* don't think this ever happens */
	
	if (*buff == '\0') strcpy(buff, "."); /* (but this definitely does...) */
	
	// ... //
	
	// [MQH]
	char *realPath;
  
	realPath = malloc( ( sizeof( buff ) + sizeof( dir ) ) * 500 );
  
	strcpy( realPath, dir );
	strcat( realPath, buff );

	return realPath;
}

const char *full(const char *path) /* add mountpoint to path */
{
	return getFullPath( path, REAL_PATH );
}

static int do_getattr(const char *path, struct stat *stbuf)
{
    int res;

path = full(path);
//printf("fusexmp: getattr(%s)\n", path);
    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_access(const char *path, int mask)
{
    int res;

path = full(path);
//printf("fusexmp: access(%s)\n", path);
    res = access(path, mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_readlink(const char *path, char *buf, size_t size)
{
    int res;

path = full(path);
//printf("fusexmp: readlink(%s)\n", path);
    res = readlink(path, buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}


static int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;

    (void) offset;
    (void) fi;

path = full(path);
//printf("fusexmp: readdir(%s)\n", path);
    dp = opendir(path);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int do_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
       is more portable */
path = full(path);
//printf("fusexmp: mknod(%s)\n", path);
    if (S_ISREG(mode)) {
        res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
        res = mkfifo(path, mode);
    else
        res = mknod(path, mode, rdev);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_mkdir(const char *path, mode_t mode)
{
    int res;

path = full(path);
//printf("fusexmp: mkdir(%s)\n", path);
    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_unlink(const char *path)
{
    int res;

path = full(path);
//printf("fusexmp: unlink(%s)\n", path);
    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_rmdir(const char *path)
{
    int res;

path = full(path);
//printf("fusexmp: rmdir(%s)\n", path);
    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_symlink(const char *from, const char *to)
{
    int res;

from = full(from);
to = full(to);
//printf("fusexmp: symlink(%s, %s)\n", from, to);
    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_rename(const char *from, const char *to)
{
    int res;

from = full(from);
to = full(to);
//printf("fusexmp: rename(%s, %s)\n", from, to);
    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_link(const char *from, const char *to)
{
    int res;

from = full(from);
to = full(to);
//printf("fusexmp: link(%s, %s)\n", from, to);
    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_chmod(const char *path, mode_t mode)
{
    int res;

path = full(path);
//printf("fusexmp: chmod(%s)\n", path);
    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;

path = full(path);
//printf("fusexmp: lchown(%s)\n", path);
    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_truncate(const char *path, off_t size)
{
    int res;

path = full(path);
//printf("fusexmp: truncate(%s)\n", path);
    res = truncate(path, size);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

path = full(path);
//printf("fusexmp: utimes(%s)\n", path);
    res = utimes(path, tv);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi)
{
    int res;

path = full(path);
//printf("fusexmp: open(%s)\n", path);
    res = open(path, fi->flags);
    if (res == -1)
        return -errno;

    close(res);
    return 0;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    int fd;
    int res;

    (void) fi;
path = full(path);
//printf("fusexmp: read(%s)\n", path);
    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -errno;

    res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static int do_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

path = full(path);
//printf("fusexmp: statvfs(%s)\n", path);
    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_release(const char *path, struct fuse_file_info *fi)
{
// Release is called when there are no more open handles.  This is where
// we do whatever action we want to with the file as all updates are
// now complete.  For example, calling gpg to encrypt it, or rsync
// to transfer it to disaster-recovery storage

// OR look at fi->flags for write access, and assume if opened
// for write, it will have been written to

path = full(path);
//printf("fusexmp: release(%s) flags=%02x\n", path, fi->flags);
    if ((fi->flags&1) != 0) {
      printf("fusexmp TRIGGER: save file to /mnt/backup/%s\n", path);
    }
    (void) path;
    (void) fi;
    return 0;
}

static int do_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi)
{
    /* Just a stub.  This method is optional and can safely be left
       unimplemented */

path = full(path);
//printf("fusexmp: fsync(%s)\n", path);
    (void) path;
    (void) isdatasync;
    (void) fi;
    return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int do_setxattr(const char *path, const char *name, const char *value,
                        size_t size, int flags)
{
    int res;
path = full(path);
//printf("fusexmp: setxattr(%s)\n", path);
    res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int do_getxattr(const char *path, const char *name, char *value,
                    size_t size)
{
    int res;

path = full(path);
//printf("fusexmp: getxattr(%s)\n", path);
    res = lgetxattr(path, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int do_listxattr(const char *path, char *list, size_t size)
{
    int res;

path = full(path);
//printf("fusexmp: listxattr(%s)\n", path);
    res = llistxattr(path, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int do_removexattr(const char *path, const char *name)
{
    int res;

path = full(path);
//printf("fusexmp: removexattr(%s)\n", path);
    res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

static void *do_init(void)
{
  printf("fusexmp: init()\n");
  // trick to allow mounting as an overlay - doesn't work on freebsd
  //fchdir(save_dir);
  //close(save_dir);
  return NULL;
}

// ... //

// [MQH] 30 March 2016
void writeToDestination( const char *filename, const char *buffer, size_t bufferSize, off_t offset )
{
	const char *destinationFile = getFullPath( filename, DESTINATION_PATH );
	
	printf( "\n\t == writeToDestination == \n" );
	
	int fd = open( destinationFile, O_WRONLY );
	
    if (fd == -1)
    {
    	printf( "\t\t[open] Error in writeToDestination %d\n", errno );
    	return;
    }
    
    int res = pwrite( fd, buffer, bufferSize, offset );
    
   	if (res == -1)
   	{
   		printf( "\t\t[pwrite] Error in writeToDestination %d\n", errno );
   		return;
    }

    close(fd);
    
    printf( "writeToDestination: Done!\n" );
}

// [MQH] 1 April 2016
void connectToDeamon()
{
	struct sockaddr_in address;
	
	client = socket( PF_INET, SOCK_STREAM, 0 );
	
	address.sin_family = AF_INET;
	address.sin_port = htons( 5390 );
	address.sin_addr.s_addr = inet_addr( "127.0.0.1" );
	
	memset( address.sin_zero, '\0', sizeof( address.sin_zero ) );
	
	connectionRes = connect( client, (struct sockaddr *) &address, sizeof( address ) );
}

// [MQH] 31 March 2016
off_t getNextOffsetToBeCopied( int *copyingDone )
{
//	struct sockaddr_in address;
	char buffer[ 1024 ];
	
	// First Time Connection
	if ( connectionRes == -1 )
		connectToDeamon();
	
	// There is no copying deamon working
	if ( connectionRes == -1 )
	{
		close( client );
		return 0;
	}
	
	write( client, "offset", strlen( "offset" ) );
	
	memset( buffer, 0, 1024 );
	
	recv( client, buffer, 1024, 0 );
	
	printf( "\n\n%s\n\n", buffer );
	if ( strcmp( buffer, "DONE" ) == 0 )
	{
		printf( "Copying Done" );
		*copyingDone = 1;
		return 0;
	}
	
	return stringToOffset( buffer );
}

void resumeCopying()
{
	write( client, "resume", strlen( "resume" ) );
}

// [MQH] Modified for our project. The most important function
static int do_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
	printf( "Write Process\n" );
						
	const char *filename = path;
	int copyingDone = 0, fd, res;

    (void) fi;
	
	path = full(path);
	//printf("fusexmp: write #%ld\n", writeCounter);//(%s)\n", path);
	
    fd = open( path, O_WRONLY );
    if (fd == -1)
    {
    	printf( "\tOpen File Error %d\n", errno );
        return -errno;
    }
    
    res = pwrite(fd, buf, size, offset);
    if (res == -1)
    {
    	printf( "\tWrite File Error %d\n", errno );
        
        return -errno;
    }
        
    int c = close(fd);
    
    if ( initialCopyDone == 1 )
   	{
   		// [MQH] Copying completed. We could shut the source down and run the destination.
   		
   		printf( "Write Mirrored\n" );
   		writeToDestination( filename, buf, size, offset );
    	return res;
    }
     
    // ... //
    
    off_t nextOffset = getNextOffsetToBeCopied( &copyingDone );
	
	// There is a working copy process
	if ( connectionRes != -1 )
	{
		if ( copyingDone == 0 )
		{
			if ( offset >= nextOffset )
			{
				// Do Nothing. It's just here for the readability
				//printf( "\t\tJust Do write on the source. It will be copied when we resume copying process\n" );
			}
			else if ( offset < nextOffset )
			{
				printf( "\t\tMust be mirrored to the destination\n" );
				writeToDestination( filename, buf, size, offset );
			}
		
			resumeCopying();
		}
		else
		{
			printf( "\t\tCopying Process Done\n" );
			writeToDestination( filename, buf, size, offset );
			
			// ... //
			
			initialCopyDone = 1;
			
			write( client, "CLOSE", strlen( "CLOSE" ) );
			close( client );
		}
    }
    
    return res;
}

// ... //

static struct fuse_operations fuse_oper = {
//    .init       = do_init, /* fusexmp.c:486: warning: initialization from incompatible pointer type */
    .getattr	= do_getattr,
    .access	= do_access,
    .readlink	= do_readlink,
    .readdir	= do_readdir,
    .mknod	= do_mknod,
    .mkdir	= do_mkdir,
    .symlink	= do_symlink,
    .unlink	= do_unlink,
    .rmdir	= do_rmdir,
    .rename	= do_rename,
    .link	= do_link,
    .chmod	= do_chmod,
    .chown	= do_chown,
    .truncate	= do_truncate,
    .utimens	= do_utimens,
    .open	= do_open,
    .read	= do_read,
    .write	= do_write,
    .statfs	= do_statfs,
    .release	= do_release,
    .fsync	= do_fsync,
#ifdef HAVE_SETXATTR
    .setxattr	= do_setxattr,
    .getxattr	= do_getxattr,
    .listxattr	= do_listxattr,
    .removexattr= do_removexattr,
#endif
};

// [MQH] 2 April 2016
void getVMsInfo( int debugMode )
{
	if ( debugMode == 1 )
	{
		strcpy( REAL_PATH, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Source/" );
		strcpy( DESTINATION_PATH, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Destination/" );
	}
	else
	{
		printf( "Real path of source virtual disk's directory (WITH SLASH IN THE END): " );
		fgets( REAL_PATH, sizeof( REAL_PATH ), stdin );
		
		printf( "\nPath of destination virtual disk's directory (WITH SLASH IN THE END): " );
		fgets( DESTINATION_PATH, sizeof( DESTINATION_PATH ), stdin );	
		
		// Eliminates "\n"
		REAL_PATH[ strlen( REAL_PATH ) - 1 ] = '\0';
		DESTINATION_PATH[ strlen( DESTINATION_PATH ) - 1 ] = '\0';
	
		printf( "\nReal path = %s", REAL_PATH );	
		printf( "\nThe path of destination = %s", DESTINATION_PATH );
	}
}

int main( int argc, char *argv[] )
{
	getVMsInfo( 1 );
	
	return fuse_main( argc, argv, &fuse_oper, NULL );
}
