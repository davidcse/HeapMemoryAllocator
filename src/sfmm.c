/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "sfmm.h"
#include <sfmmHelper.h>

/*
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
sf_free_header* freelist_current = NULL;
#define PAGE_SIZE 4096

			/* Main Methods */
/*
 * A function that implements malloc
 * @param size : size of bytes to be malloced. 
 */
void* sf_malloc(size_t size) {
	/*Upper limit of size is 4GB */
	unsigned long long maxSize = (1024*1024*1024);
	maxSize = maxSize * 4;

	/* If greater than 4GB, exit*/
	if(size>maxSize){
		errno = ENOMEM;
		perror(strerror(errno));
		return NULL;
	}else if(size ==0){
		return NULL;
	}

	sf_free_header* freeblock = findFirstFitPolicy(size, freelist_head);
	if(freeblock == NULL){
		initializeFreelistHeader(size);
		freeblock = findFirstFitPolicy(size,freelist_head);
	}
	updateBlockToAllocated(freeblock,size);
	
	/*Update the freelist in regards to this block */
	if((freeblock->prev) != NULL)
		((freeblock->prev)->next) = (freeblock->next);
	else if((freeblock-> next)!=NULL){ /* The fit block is the freelist_head */
		freelist_head = (freeblock->next); 
	}else{  
		/* The fit block is the only node, which is the freelist_head*/
		/* Replace the freelist_head with new free block */
		alignHeap();
		freelist_head = createAlignedBlock(1);
		freelist_head->next = NULL;
		freelist_head->prev = NULL; 
	}
	
	/* return the payload */
	return getPayloadPtr(freeblock);
	
}

void sf_free(void *ptr) {
	/* Check for valid ptr to be freed */
	if(validateAddress(ptr) == true){
		/* Update Header Block*/
		sf_free_header* freeThisBlock = ptr;
		(freeThisBlock->header).alloc = 0;
		(freeThisBlock->header).requested_size=0;
		unsigned long blockSize = (((freeThisBlock->header).block_size)<<4);

		/* Update the footer block */
		void* footerBlock = (void*)(((char*)ptr)+blockSize-8); /* footer starts 8 bytes before end of block*/
		sf_footer* footerStructPtr = footerBlock; /* Grab the struct*/
		footerStructPtr->alloc=0; /* Update the footer alloc bit */

		/*Put the free block into the front of the freelist */
		freeThisBlock->next = freelist_head;
		freelist_head = freeThisBlock;
	}
}

void* sf_realloc(void *ptr, size_t size) {

    return NULL;
}

void* sf_calloc(size_t nmemb, size_t size) {
	unsigned long totalSize = (nmemb*size);
	void* payload = sf_malloc(totalSize);
	return memset(payload,0, totalSize);
}


						/* Memory Policy Methods */

/* A function that finds the free block according to the find next fit policy */
void* findNextFitPolicy(size_t requested_size){
	/* pointer to an sf_free_head, in start of search*/
	sf_free_header* anchor = freelist_current; /* Stopping point*/
	sf_free_header* curr = anchor; 

	if(curr == NULL)
		return NULL; 
	else if( ((curr->header.block_size)<<4) > requested_size )
		return curr;

	/* Iterate up to one full cycle. */
	/* Stop if we find the requested size before then, or if we reach node before anchor point */
	while (((curr->next) != anchor) && (((curr->header.block_size)<<4)<requested_size) ){
		/* Reached end of list, continue search at the start of free_list */
		if(((curr->next)== NULL) && (freelist_head!=NULL) &&(freelist_head!= anchor)){
			/* If we can continue iteration at the beginning of linked list*/
			curr = freelist_head;
		}
		else if(((curr->next) == NULL) && (freelist_head== anchor)){
			/* Special Case: we are in a single node list. Don't continue iteration */
			return NULL;
		} 
		else curr = (curr->next); /* Regular iteration through range of linked list */
	}

	/* Either we completed one cycle, and are at the node before anchor*/
	/* or we have found our requested_size*/
	if(curr->header.block_size >= requested_size)
		return curr; /* Return a valid block */ 
	else{
		/* No block of size requested exists */
		return NULL;
	} 

}

/* A helper function that helps execute the findFirstFitPolicy
 * @param requested_size: the size of the requested block 
 * @param freelist_head: the header of the freelist.
 * @returns: sf_free_header* node containing the first block to fit the policy, else NULL. 
 */
sf_free_header* findFirstFitPolicy(unsigned long requested_size, sf_free_header* head){
	/* Store the current node */
	sf_free_header* curr = head; 

	/*Base case*/
	if(curr == NULL)
		return NULL;

	/* Check current head for the free block before going through the list */
	if ((((curr->header).block_size)<<4) >= requested_size){ 
		/* We've found the block */
		return curr;
	}

	/* first block is not a fit, go through the link-list, looking for the requested size*/
	while (((curr->next) != NULL) && ( (((curr->header).block_size)<<4) < requested_size) ){
		curr = (curr->next);
	}

	/* Either we are at the last node of the free list, or we have found our requested_size*/
	if((((curr->header).block_size<<4))>= requested_size)
		return curr; /* Return node containing the valid block */ 
	else
		return NULL; /* No block of size requested exists */
}

/* A function that sets the allocated bit as non-free, as well as writes the requested size for the block. 
 * It also makes sure to update the freelist. 
 * @param blockPointer: the pointer to the beginning of the block
 * @param size: the size of the block's requested_size field to be set. 
 */
void updateBlockToAllocated(sf_free_header* block,size_t size){
	/* Get access to footer struct */
	unsigned long blockSize = (((block->header).block_size)<<4); /* Grab Block size  */
	void* endOfBlock = (void*)(((char*)block) + blockSize); /* ptr to end of block */
	sf_footer* footer = (void*)(((char*)endOfBlock)-8); /* ptr to beginning of footer struct */

	/* Update header struct */
	((block->header).alloc) = 1;
	(block->header).requested_size = size;

	/* Update footer struct */
	(footer->alloc) = 1;
}

/*A function that updates the status of the newly freed block from allocated to free. However,
 * Other functions must handle the coalescing and linking of this block to the freelist after 
 * this function*/
void* updateBlockToFree(sf_free_header* allocatedBlock){
	if(allocatedBlock == NULL)
		return ((void*)-1);
	/* Get access to footer struct */
	unsigned long blockSize = (((allocatedBlock->header).block_size)<<4); /* Grab Block size  */
	void* endOfBlock = (void*)(((char*)allocatedBlock) + blockSize); /* ptr to end of block */
	sf_footer* footer = (void*)(((char*)endOfBlock)-8); /* ptr to beginning of footer struct */

	/* Update header struct */
	((allocatedBlock->header).alloc) = 0;
	(allocatedBlock->header).requested_size = 0;

	/* Update footer struct */
	(footer->alloc) = 0;

	return (void*)allocatedBlock;

}

/* A function that requests for more free heap blocks to be inserted into the explicit free list
 * @param size: the size of the requested free block to be inserted into the front of the freelist
 */
void initializeFreelistHeader(size_t size){
	/*size for at least payload, boundary tags*/
	int requestSize = calculateRequestSize(size);

	/* extra size for padding */
	if( (requestSize - size) < (4*sizeof(unsigned long))) /*Room for padding/ header tags*/
		requestSize = requestSize + PAGE_SIZE; /* Increment by whole new multiple of page_size*/

	/* expand heap to create new block */
	unsigned long multipleOfPageSize = requestSize/4096;
	sf_free_header* newBlock = createAlignedBlock(multipleOfPageSize);

	/*Attach the node to the front of the list*/
	(newBlock->next) = freelist_head;
	freelist_head = newBlock;	
}


				/*Methods to Create Blocks*/

/* A function that creates a header boundary within the existing heap. Not asking for additional memory. 
 * @param startingAddress : starting address of the header
 * @param alloc : allocated bits
 * @param block_size: block size
 * @param requested_size: requested size 
 * @return : return address of where header ends.
*/
void* insertHeader(void* startingAddress, int alloc, int block_size, int requested_size){
	sf_free_header* headerStart = startingAddress;
	headerStart->header.alloc = alloc;
	headerStart->header.block_size = (block_size>>4);
	headerStart->header.requested_size = requested_size;
	return ((char*) startingAddress + block_size-8);
}

/* A function that inserts a footer boundary, by asking for more heap memory.
 * Appends to the end of the (header + payload).
 * Note: Must be able to allocate enough space, since this is an insertion. 
 * @param startingAddress : starting address of the footer
 * @param alloc : allocated bits
 * @param block_size: block size
 * @return : return address of where footer ends. 
 */
void* insertFooter(void* startingAddress, int alloc, unsigned long block_size){
	sf_footer* footer = (void*)((char*)startingAddress); /* since sf_footer is only 4 bytes, but footer bound is 8 bytes */
	footer->alloc = alloc;
	footer->block_size = (block_size>>4);
	return (void*)((char*) startingAddress+4);
}

/* A function that creates a block in the heap, in the following format 
 * header + space + footer = (multiple * PAGE_SIZE bytes)
 * function returns block of size (multiple * PAGE_SIZE bytes)
 * @param multipleOfPageSize : the multiple amount of PAGE_SIZE. 
 * @return: The return address of the starting point of the block 
 */
void* createAlignedBlock(int multipleOfPageSize){
	/* Create space for header and payload/pointers */
	unsigned long wholeBlockSize = (multipleOfPageSize * PAGE_SIZE);
	int headAndPayload =  wholeBlockSize -8;      /* -8 for span of footer boundary */
	void* headPtr = sf_sbrk(headAndPayload); 	 /* Ask for space*/
	if(headPtr == ((void*)-1)){
		errno = ENOMEM;
		perror(strerror(errno));
		return NULL;
	}

	/*TEST CODE
	void* endOfHeadPayloadBlock =(void*) ((char*)headPtr + headAndPayload);*/

	/*Initialize header */
	insertHeader(headPtr,0,wholeBlockSize,0); 

	/*Initialize Footer */ 		   
	void* footerStartPtr = sf_sbrk(8);
	if(footerStartPtr == ((void*)-1)){
		perror(strerror(errno));
		return NULL;
	}
	insertFooter(footerStartPtr,0,wholeBlockSize);
	return headPtr;
}

/* A function returning the number of bytes that is a multiple of 4096, that will fit the requested size 
 * @param size: size request
 * @return : the returned multiple of 4096.
 */
int calculateRequestSize(size_t size){
	/*Calculate what multiple of 4096 to use */
	int multiple = size/4096;
	int leftover = size%4096;

	/*Calculate request size */
	int requestSize=0;
	if(leftover==0)
	    requestSize = multiple*4096;
	else requestSize = ((multiple+1)*4096);

	return requestSize;
}


						/* METHODS: Heap Pointers */

/* A function that takes the start pointer to a block, then returns the pointer to the
 * payload, by skipping the header block, then aligning. 
 * @param node: the pointer to the block
 * @return : the pointer to the payload of the block. 
 */
void* getPayloadPtr(void* node){
	void* tempPtr = (void*)(((char*)node)+8); /*address after skipping the sf_header*/
	void* alignedPayloadStartAddress = alignPtr(tempPtr);
	return alignedPayloadStartAddress;

	/* IGNORE OLD CODE
	char* offset = (((char*)node)+8); 
	if( ((uintptr_t)offset)%16 != 0){
		offset = offset +(((uintptr_t)offset)%16);
	}
	return (void*)offset; */
}

void* alignHeap(){
	void* currentHeap = sf_sbrk(0);
	uintptr_t heapAddress = ((uintptr_t)currentHeap);
	if( heapAddress % 16 ==8)
		return currentHeap;
	else{

		int val = heapAddress%16;
		int offset = 0;
		if(val < 8)
			offset = 8 - val;
		else if(val>8)
			offset = (16-val)+8;
		currentHeap = sf_sbrk(offset);
		currentHeap = (void*)(((char*)currentHeap)+offset);

		/*ALTERNATE CODE FOR WHEN ALIGNING HEAP ADDRESS TO 16.
		int remainder = heapAddress%16;
		int offset = 16-remainder;
		currentHeap = sf_sbrk(offset);
		currentHeap = (void*)(((char*)currentHeap)+offset);*/
	}
	return currentHeap;
}

/* A function that aligns the pointer within the heap */
void* alignPtr(void* pointer){
	uintptr_t pointerAddress = ((uintptr_t)pointer);
	if( pointerAddress % 16 ==0)
		return pointer;
	else{
		int remainder = pointerAddress%16;
		int offset = 16-remainder;
		pointer = (void*)(((char*)pointer)+offset);
	}
	return pointer;
}

			/* To Be Implemented Methods */

/* This is a helper function that performs coalesce on the next block */
void* coalesceForward(sf_free_header* head){
	if(head ==NULL)
		return ((void*)-1);
	/* Jump to the next block */
	sf_footer* footer = headerTravelToFooter(head);
	if(head->next!=NULL){
		
		/* Calculate addresses for comparison*/
		uintptr_t nextAddressBlock = (uintptr_t)addressOf(head->next);
		uintptr_t thisAddressBlock  = (uintptr_t)((char*)footer+8);
		
		/* Check if we can move to a next boundary block */
		if((((uintptr_t)sf_sbrk(0)) - thisAddressBlock ) <0) return ((void*)-1); /* ERROR:exceed current heap */
	
		/* Check if the boundary calculations are equal */
		if(nextAddressBlock==thisAddressBlock){
			return (void*) merge(head,(sf_free_header*)addressOf(head->next));
		}
	}
	return head; /* Unable to coalesce forward */
}

/* This is a helper function that performs coalesce on the previous block */
void* coalesceBackward(sf_free_header* head){	
	if(head==NULL)
		return ((void*)-1);
	if(head->prev!=NULL){
		
		/* Calculate addresses for comparison*/
		uintptr_t prevAddressBlock = (addressOf(headerTravelToFooter(head->prev))+8);
		uintptr_t thisAddressBlock = (uintptr_t)((char*)head);
		
		/* Check if we can move to a prev boundary block */
		if((thisAddressBlock ) <0) return ((void*)-1); /*ERROR:exceed current heap */

		/* Check if the boundary calculations are equal */
		if(prevAddressBlock==thisAddressBlock){
			return merge((sf_free_header*)addressOf(head->prev),head);
			
		}
	}
	return head; /* Unable to coalesce backwards */
}

/* This is a helper function that merges the two free blocks. Inputs assumed to be mergeable:
 * meaning that the two blocks are adjacent in memory, and that they're both free blocks. 
*/
void* merge(sf_free_header* block1, sf_free_header* block2){
	if((block1==NULL)||block2==NULL) return ((void*)-1);
	sf_footer* newFooter = headerTravelToFooter(block2); /* Travel to footer struct */
	sf_free_header* temp = block2->next; /* Temp pointer */

	/* calculate stats for new block */
	unsigned long block2Size = ((newFooter->block_size)<<4);
	unsigned long block1Size = ((block1->header).block_size<<4);
	unsigned long newSize = block2Size +block1Size; 

	/* Update block 1*/
	((block1->header).block_size) = (newSize>>4);
	((block1->header).requested_size) = 0;
	((block1->header).alloc) = 0;

	/* update block 2*/
	(newFooter->block_size) = (newSize>>4);
	(newFooter->alloc) = 0;

	/* Update links */
	block1->next = temp;
	if(temp!=NULL)
		temp->prev = block1;
	return block1;
}

/*A function that performs coalescence if it is possible. 
 * @param freeblock: sf_free_header*.
 * @return: the beginning pointer to the new coalesced block if possible. 
 *
 */
void* coalesce(sf_free_header* coalesceBlock){
	if(coalesceBlock==NULL)
		return ((void*)-1);
	else {
		coalesceBlock = coalesceForward(coalesceBlock);
		if(coalesceBlock==((void*)-1)){
			return ((void*)-1);
		}
		coalesceBlock = coalesceBackward(coalesceBlock);
		if(coalesceBlock==((void*)-1)){
			return ((void*)-1);

		}
		return coalesceBlock;
	}
	/* check next node if it's a freelist_current , if so make sure to update its status */
}

/* A function that inserts the freed block (based on increasing order of address, 
 * instead of the LIFO policy) into the freelist
 * @param insertNewFreedBlock: sf_header*. The new freed block to be inserted, address ascending order. 
 * @return the start of the free block that was inserted, and coalesced if possible. 
 */
void* insertFreedBlockAddress(sf_free_header* insertNewFreedBlock){
	if(freelist_head ==NULL){ /* CASE: No prev freelist*/
		/* update status to free, then make it head of list*/
		freelist_head = updateBlockToFree(insertNewFreedBlock);
		return freelist_head;
	
	}else if( addressOf(freelist_head) > addressOf(insertNewFreedBlock)){
		/* CASE: new freedblock address before current freelist head. Make freed block new head*/
		updateBlockToFree(insertNewFreedBlock);    /* make block status free */
		insertNewFreedBlock->next = freelist_head; 
		freelist_head = insertNewFreedBlock;
		return coalesce(freelist_head);
	}
	else{ /* CASE: new inserted block appears after the freelist head */
		sf_free_header* curr = freelist_head;
		/* Search thru list for where the new freed block belongs */
		while((curr->next)!=NULL){
			if(addressOf(curr->next) > addressOf(insertNewFreedBlock)){
				/* Link new freed block into between the two nodes */
				insertNewFreedBlock->next = curr->next;
				insertNewFreedBlock->prev = curr;

				/* Break previous links, by attaching them to new freed block */
				((curr-> next)->prev) = insertNewFreedBlock;
				curr->next = insertNewFreedBlock;

				/* Coalesce the blocks if we can */
				return coalesce(insertNewFreedBlock); /* coalesce and return */

			}else curr = curr->next;/*Continue iterating through list*/
		}
		/* reached end of free list without finding a block of higher address */
		if((curr->next) == NULL){ /* curr = last node of the list*/
			insertNewFreedBlock->prev = curr;
			curr->next = insertNewFreedBlock;
			return coalesce(insertNewFreedBlock);
		}
	}
	return coalesce(insertNewFreedBlock);
}

			/* SIMPLE HELPER METHODS */

/* A function that takes a sf_free_header * , and returns its address in heap */
uintptr_t addressOf(sf_free_header* block){
	return ((uintptr_t)((void*)block));
}

/* A function that jumps from header struct to footer struct within one block entity */
void* headerTravelToFooter(sf_free_header* start){
	unsigned long blockSize = (((start->header).block_size)<<4);
	return ((void*) (((char*)start)+blockSize-8));
}

/* A function that jumps from footer struct to header struct within one block entity */
void* footerTravelToHeader(sf_footer* end){
	unsigned long blockSize = ((end->block_size)<<4);
	return ((void*) (((char*)end)+8-blockSize));
}


			/* HELPER METHODS FOR SF_FREE() */

bool checkPointerToAllocatedBlockHead(void*ptr){
	/* Check for size of ptr, avoid seg fault*/
	uintptr_t topOfHeapAddress = ((uintptr_t) sf_sbrk(0)); 
	uintptr_t ptrAddress = ((uintptr_t) ptr); 
	if((topOfHeapAddress - ptrAddress)< 0) return false; /* ptr address is beyond heap */
	else if ((topOfHeapAddress-ptrAddress)< 32) return false; /*At least 32 bytes */

	/* Assured enough memory to cast after above checks*/
	sf_header* headPtr = ptr;
	/* Grab data*/
	unsigned long headerAlloc = headPtr->alloc;
	if(headerAlloc!=1) return false;

	/*Compare header's block size and request size */
	unsigned long headerBlockSize = ((headPtr->block_size)<<4); 
	unsigned long headerRequestedSize = (headPtr->requested_size); 
	if(headerRequestedSize>headerBlockSize) return false; /* violates definition of valid block struct */

	/*Compare footer's block size and request size */
	uintptr_t totalRangeOfBlock = ((uintptr_t)ptr + headerBlockSize); /*End of footer */
	if( (topOfHeapAddress-totalRangeOfBlock) < 0) return false; /* Footer can't exist past top of heap */
	uintptr_t footerAddress = totalRangeOfBlock-8; /* start of the presumed footer address*/

	/*Should now NOT cause a segfault*/
	sf_footer* footerPtr = (void*)footerAddress;
	unsigned long footerBlockSize = ((footerPtr->block_size)<<4); 
	unsigned long footerAlloc = (footerPtr->alloc); 
	if(footerAlloc!=1) return false; /*should be an allocated block footer */
	if(footerBlockSize!=headerBlockSize) return false;

	return true; /* Passed all tests */
}

bool validateAddress(void* ptr){
	/* Check null */
	if(ptr==NULL)
		return false;
	/*Check for cases */
	if(checkPointerToAllocatedBlockHead(ptr)==true ){
		return true;
	}
	/* failed tests */
	return false;
}


			/* EXTRA UNNECESSARY METHODS */

/* A function that retrieves the long at address of the block pointer */
unsigned long  getLongAtPointer(void* pointer){
	return (*(unsigned long *)pointer);
}
/* A function that puts the long at address of the block pointer */
void putLongAtPointer(void* pointer, unsigned long value){
	(*(unsigned long *)(pointer)) = value;
}
/*A function that reads the requested size from the pointer block */
unsigned long readRequestedSize(void* ptr){
	/* Initialize Vars */
	unsigned long addressContents = (*((unsigned long*)ptr));
	unsigned long requestedSize = addressContents & 0xffffffff00000000;
	return (requestedSize>>32);
}
/* Helper function that determines whether the address of the pointer is aligned, by modulo 16 */
bool isAligned(void* address){
	uintptr_t addressInt = ((uintptr_t)address);
	if( addressInt % 16 ==0)
		return true;
	else return false;
}

/* A function that reads the block size from the block header */
unsigned long readBlockSize(void* ptr){
	/* Initialize Vars */
	unsigned long addressContents = (*((unsigned long*)ptr));
	unsigned long blockSize = addressContents & 0xfffffff0;
	return blockSize;
}
/* This is a helper function that reads the allocation bit and determines if the block is free*/
unsigned long readAllocatedBit(void* ptr){
	unsigned long addressContents = (*((unsigned long*)ptr));
	if((addressContents & 0x1) == 1) return true;
	else return false;
}


