/*
*  Oguz Demir 21201712 - Kaan Mert Berkem 21200573
*
*  CS342-1 Project4 - Buddy Allocator Implementation	
*
*
*/

#include <stdlib.h>
#include <stdio.h>


//Minimum request is 256 bytes and maximum request is 64KB
//But the allocation information will be stored on blocked themselves. (24Bytes)
//So that, if the request is power of 2, it should be rounded to one level up.
//As a result minumum allocation is 512 = 2^9 bytes, maximum allocation is 128KB = 2^17,
//Chunk can be upto 32MB = 2^25, so from 32MB to 512B, there is 17 levels. 

//In a block, first 8 bytes is for information about allocation or free.
//Second 8 bytes for size of the block.
//Third 8 bytes(last 8 bytes of information part) is pointer to next free block with same size. 
// Long int is used for adressing, so NULL is implied by -1. 

//Levels of free blocks. The need is actually 17 as mentioned, but for easy indexing, array with size 26 is used.
static const int MAX_LEVELS = 26;
long int free_levels[26];

//Base adress is the adress of start of the chunk.
long int baseAdress;


//Powers of two, not to compute again and again.
int twospowers[26] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,65536,131072 ,
							262144 ,524288 ,1048576 ,2097152 ,4194304 ,8388608 ,16777216 ,33554432};

//If chunk is small, it cannot provide the maximum allocation(128KB),(if the chunk is 32-64 or 128KB)
int maxAllocation;

//Maximum level;for example 32MB- level = 25
int level;

//Size will hold the chunk size
int size;

int binit(void *chunkpointer, int chunksize)
{
	//Converting kb to byte
	chunksize *= 1024;
	baseAdress = (long int) chunkpointer;
	size = chunksize;

	//Checking chunk size
	if(chunksize < twospowers[15] || chunksize > twospowers[25])
	{
		printf("Size mismatch; chunk should be between 32KB and 32MB\n");
		return -1;
	}
	
	//Initialize pointers of free blocks
	int i;
	for(i = 0; i < MAX_LEVELS; i++)
		free_levels[i] = -1;
	
	//Finding level
	for(i = 15; i < 26; i++)
	{
		if(chunksize == twospowers[i])
		{
			level = i;
			maxAllocation = twospowers[i-1]; //For the small sizes, maximum allocation is half of original size.
			break;
		}
	}
	//If the loop came to end, there is no exact match with two's powers.
	if(i==26)
	{
		printf("Chunk size should be power of 2.\n");
		return -1;
	}
	
	//Correcting the half sizes of bigger chunks to limited size (128KB)
	if(maxAllocation > twospowers[17])
		maxAllocation = twospowers[17];

	//Adding the chunk as a free block to corresponding list
	long int x = (long int)chunkpointer;
	free_levels[level] = x;
	long int * ptr = (long int *) chunkpointer;
	ptr[0] = 0;			//Setting block free
	ptr[1] = twospowers[level];	//Setting block's size
	ptr[2] = -1;			//Setting next pointer to NULL

	
	return (0);		// if success
}	

void *balloc(int objectsize)
{
	//Checking object size
	if(objectsize < twospowers[8] || objectsize > twospowers[16])
		return NULL;

	//Required size is requested + information storage
	int requiredSize = objectsize + 24;
	
	//Checking allocation limits.
	if(requiredSize > maxAllocation)
	{
		return NULL;
	}

	//Finding level of allocated block; for example if 4KB will be allocated, level is 12. 
	int requiredLevel;	
	int i;
	for(i = 9 ; i < 18 ; i++)
	{	
		if(requiredSize < twospowers[i])
		{
			requiredLevel = i;
			break;
		}
	}

	//If there is free block on the list of corresponding level,
	//Allocated it and advance the list to next node.
	if(free_levels[requiredLevel] != -1)
	{
		long int* p = (long int *)free_levels[requiredLevel];
		long int* p2 =  (long int *)(free_levels[requiredLevel]+24);	
		long int nextFree = p[2];
		p[0] = 1;
		p[2] = -1;

		free_levels[requiredLevel] = nextFree;
		return p2;
	}

	//If there is no free block, going to one level up until find a free block
	int n = requiredLevel;
	
	while(n <= level && free_levels[n] == -1)
		n++;

	//If no free blocks, it means no space.
	if(n > level)
	{
		return(NULL);
	}

	//After finding the free block, it is splited until the required level.
	//After each split, first part is sent to loop again to split again and second part is added to free list of corresponding level.
	long int* splitPointer = (long int *)free_levels[n];
	long int nextFree = splitPointer[2];
	free_levels[n] = nextFree;


	for(i = n; i > requiredLevel; i--)
	{

		long int secondPart = (long int)splitPointer + twospowers[i-1];
		long int *secondPointer = (long int *)secondPart;
		secondPointer[0] = 0;
		secondPointer[1] = twospowers[i-1];
		secondPointer[2] = -1;
		free_levels[i-1] = secondPart;

		splitPointer[0] = 0;
		splitPointer[1] = twospowers[i-1];
		splitPointer[2] = -1;
		
	}
	splitPointer[0] = 1;
	return (long int*)((long int)(splitPointer) + 24);	//Start of usable memory for object is +24 because first 24 includes the information
}

//Deleting node from free list with list level and node adress(kind of id)
void deleteNode(int levelNo, long int adr)
{
	if(free_levels[levelNo] == adr)
	{
		long int* p = (long int *)adr;
		free_levels[levelNo] = p[2];
		return;
	}
	long int padress = free_levels[levelNo];
	while(padress != -1)
	{
		long int *ptr = (long int *)padress;
		if(ptr[2] == adr)
		{
			long int *x = (long int *) ptr[2];
			ptr[2] = x[2];
			return;
		}
		padress = ptr[2];	
	}
}

//Checking if anything is suitable for merging after dealloc
void listCheck(int levelNo, long int* freed)
{
	
	long int padress = free_levels[levelNo];

	long int givenAdr = (long int) freed;
	
	//Finding buddy of freed block
	int j = (givenAdr - baseAdress) / freed[1];

	//If j is even, its buddy is block after that,
	//If not, its buddy is block before it. 

	//So for a freed block, we check the entire free list of same level to see whether its buddy is free or not.
	long int searched = givenAdr;
	if(j%2 == 0)
	{
		searched += freed[1];
	}
	else
		searched -= freed[1];

	while(padress != -1)		//Search until end of the list
	{
		long int *ptr = (long int *)padress;
		if(padress == searched)
		{	
			//If a free buddy is found, delete the buddy from list,
			//Merge them in order, and sent the merged block to one level up if there is any suitable merges.
			deleteNode(levelNo,padress);

			if(padress< givenAdr)
			{
				ptr[1] = ptr[1]*2;
				listCheck(levelNo + 1 , ptr);
			}
			else
			{
				freed[1] *= 2;
				listCheck(levelNo + 1 , freed);
			}
			return;
		}
		padress = ptr[2];	
	}
	//If there is no merge in the loop, there is no suitable merge, so freed block is added to free list.
	padress = free_levels[levelNo];
	freed[2] = padress;
	free_levels[levelNo] = (long int)freed;

}


void bfree(void *objectptr)
{
	if(objectptr == NULL)
		return;
	long int x = (long int)objectptr;
	x-=24;			//-24 to locate actual block adress, because +24 was given in the allocation
	
	//Assigning pointer to calculated adress
	long int *p = (long int *)x;
	
	p[0] = 0;	//Mark the block free.
	int i;
	for(i = 0; i < 26 ; i++)	//Finding the level of freed block and send it to listCheck.
	{
		if(twospowers[i] == p[1])
		{
			listCheck(i,p);
			break;	
		}
	}

	return;
}

//Debug print for seeing linked lists of free blocks
void printFrees(void)
{
	int i;
	for(i = 0 ; i < 26; i++)
	{
		long int padress = free_levels[i];
		while(padress != -1)
		{
			long int *ptr = (long int *)padress;
			printf("Level:%d,free block adress: %ld\n",i,(long int)ptr);
			padress = ptr[2];	
		}
	}
}

void bprint(void)
{
	printf("\n\nPrinting the chunk status \n");
	//Each block knows whether it is allocated or not and its size.
	//So print starts from the begining, and advance with the size of block.
	//Reports the block information according to size and allocation information.
	long int currentAdress = baseAdress;

	long int endPoint = baseAdress + size;

	while(currentAdress < endPoint)
	{
		long int *p = (long int*) currentAdress;

		if(p[0] == 0)
			printf("Address: %ld,\tfree,\tsize: %ld\n", currentAdress, p[1] );
		else if(p[0] == 1)
			printf("Address: %ld,\tallc,\tsize: %ld\n", currentAdress, p[1] );
		else
			printf("Something WRONG!! \n\n");

		currentAdress += p[1];
	}
	//printFrees();
	return;
}
