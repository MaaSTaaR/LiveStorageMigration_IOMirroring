/**
 * Live Storage Migration - I/O Mirroring Implemetation of VMWare ESX (http://xenon.stanford.edu/~talg/papers/USENIXATC11/atc11-svmotion.pdf)
 *
 * Mohammed Q. Hussain (http://www.maastaar.net) under supervision of Prof. Hussain Almohri (http://www.halmohri.com)
 *
 * License: GNU GPL
 *
 */
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include "server.h"

#define BUFFER_SIZE 4096

char sourceFilename[ 500 ], destinationFilename[ 500 ], nonFlatSourceFilename[ 500 ], nonFlatDestinationFilename[ 500 ];

off_t offset = 0;
pthread_mutex_t offsetMutex = PTHREAD_MUTEX_INITIALIZER;
int copyingDone = 0;

void *copyWorker()
{
	char buf[ BUFFER_SIZE ];
	
	time_t startedAt = time( NULL );
	
	int sourceFd = open( sourceFilename, O_RDONLY );
	int destinationFd = open( destinationFilename, O_WRONLY | O_CREAT, 0666 );
	
	int res;
	do
	{
		pthread_mutex_lock( &offsetMutex );
		
		res = pread( sourceFd, &buf, BUFFER_SIZE, offset );
		
		if ( res == 0 )
			break;
		
		pwrite( destinationFd, buf, res, offset );
		
		offset += BUFFER_SIZE;
		
		pthread_mutex_unlock( &offsetMutex );
		
	} while ( res == BUFFER_SIZE );
	
	close( sourceFd );
	close( destinationFd );
	
	time_t endedAt = time( NULL );
	
	printf( "Copying Done! in %ld seconds\n", ( endedAt - startedAt ) );
	
	copyingDone = 1;
}

void copyNonFlatFile()
{
	char buf[ BUFFER_SIZE ];
	off_t nonFlatOffset = 0;
	
	time_t startedAt = time( NULL );
	
	int nonFlatSourceFd = open( nonFlatSourceFilename, O_RDONLY );
	int nonFlatDestinationFd = open( nonFlatDestinationFilename, O_WRONLY | O_CREAT, 0666 );
	
	int res;
	do
	{
		res = pread( nonFlatSourceFd, &buf, BUFFER_SIZE, nonFlatOffset );
		
		if ( res == 0 )
			break;
		
		pwrite( nonFlatDestinationFd, buf, res, nonFlatOffset );
		
		nonFlatOffset += BUFFER_SIZE;		
	} while ( res == BUFFER_SIZE );
	
	close( nonFlatSourceFd );
	close( nonFlatDestinationFd );
	
	time_t endedAt = time( NULL );
	
	printf( "Non-Flat Copying Done! in %ld seconds\n", ( endedAt - startedAt ) );
}

void runDestinationVM()
{
	char storageCommandBuffer[ 500 ], destinationChangeUUIDCommand[ 500 ];
	
	time_t startedAt = time( NULL );
	
	sprintf( destinationChangeUUIDCommand, "VBoxManage internalcommands sethduuid \"%s\"", nonFlatDestinationFilename );
	sprintf( storageCommandBuffer, "VBoxManage storageattach Destination --medium \"%s\" --storagectl \"IDE\" --port 0 --device 0 --type hdd", nonFlatDestinationFilename );
	
	system( destinationChangeUUIDCommand );
	//system( storageCommandBuffer );
	//system( "VBoxManage startvm Destination" );
	
	time_t endedAt = time( NULL );
	
	printf( "Running Destination in %ld seconds\n", ( endedAt - startedAt ) );
}

void *sendOffset( void *recvConnection )
{
	int connection =  ( intptr_t ) recvConnection;
	char offsetBuffer[ 50 ];
	char clientMessage[ 2000 ];
	
	do
	{
		memset( offsetBuffer, 0, 50 );
		memset( clientMessage, 0, 2000 );
		
		recv( connection , clientMessage , 2000 , 0 );
		
		if ( copyingDone == 0 )
		{
			pthread_mutex_lock( &offsetMutex );
	
			printf( "\tCopying Suspended for a while\n" );
	
			sprintf( offsetBuffer, "%zu", offset );
		
			printf( "\t\tTo Be Sent Offset = %s\n", offsetBuffer );
			printf( "\t\tTHE Offset = %zu\n", offset );
		
			sendMessage( connection, offsetBuffer );
	
			// Stop copying until the filesystem tells us to resume
			printf( "\tWaiting the Client to Resume Copying\n" );
			recv( connection , clientMessage , 2000 , 0 );
	
			printf( "\tCopying Process has been resumed\n" );
			printf( "\t==========\n" );
	
			pthread_mutex_unlock( &offsetMutex );
		}
		else
		{
			if ( strcmp( clientMessage, "SUSPENDING" ) != 0 )
			{
				printf( "\tSending to the filesystem that the process done!\n" );
		
				strcpy( offsetBuffer, "DONE" );
				sendMessage( connection, offsetBuffer );
			}
			else if ( strcmp( clientMessage, "SUSPENDED" ) == 0 )
			{
				printf( "\n\nSource Suspended\n\n\n" );
			}
		}
	} while ( strcmp( clientMessage, "CLOSE" ) != 0 );
	
	close( connection );
	
	printf( "\tConnection Closed\n" );
	
	copyNonFlatFile();
	runDestinationVM();
}

void connectionHandler( int connection )
{
	pthread_t thread;
	
	pthread_create( &thread, NULL, sendOffset, ( void * ) ( intptr_t ) connection );
}

void *runServer()
{
	createServer( connectionHandler );
}

void getVMsInfo( int debugMode )
{
	if ( debugMode == 1 )
	{
		strcpy( sourceFilename, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Source/Mint Source-flat.vmdk" );
		strcpy( nonFlatSourceFilename, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Source/Mint Source.vmdk" );
		strcpy( destinationFilename, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Destination/Mint Source-flat.vmdk" );
		strcpy( nonFlatDestinationFilename, "/media/maastaar/b9d26bcf-e76e-43da-aa79-c53367b81fa2/maastaar/VirtualBox VMs/Mint Destination/Mint Source.vmdk" );
	}
	else
	{
		printf( "Source File Path: " );
		fgets( sourceFilename, sizeof( sourceFilename ), stdin );
		
		printf( "Source File Path (non-flat): " );
		fgets( nonFlatSourceFilename, sizeof( nonFlatSourceFilename ), stdin );
			
		printf( "Destination File Path (Will be created automatically): " );
		fgets( destinationFilename, sizeof( destinationFilename ), stdin );
		
		printf( "Destination File Path (non-flat) (Will be created automatically): " );
		fgets( nonFlatDestinationFilename, sizeof( nonFlatDestinationFilename ), stdin );

		sourceFilename[ strlen( sourceFilename ) - 1 ] = '\0';
		nonFlatSourceFilename[ strlen( nonFlatSourceFilename ) - 1 ] = '\0';
		destinationFilename[ strlen( destinationFilename ) - 1 ] = '\0';
		nonFlatDestinationFilename[ strlen( nonFlatDestinationFilename ) - 1 ] = '\0';
	}
}

main()
{
	pthread_t copyWorkerThread, serverThread;
	
	getVMsInfo( 1 );
	
	pthread_create( &copyWorkerThread, NULL, copyWorker, NULL );
	pthread_create( &serverThread, NULL, runServer, NULL );
	
	pthread_join( copyWorkerThread, NULL );
	pthread_join( serverThread, NULL );
	
	printf( "Done!" );
}
