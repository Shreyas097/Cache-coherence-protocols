/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{
	int proc_number;
	uchar op;
	ulong addr;
	ifstream fin;
	FILE * pFile;

	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }

	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc= atoi(argv[2]);
	int blk_size   = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:Dragon*/
	char *fname =  (char *)malloc(20);
 	fname = argv[6];

	
	//****************************************************//
	//**printf("===== Simulator configuration =====\n");**//
	//*******print out simulator configuration here*******//
	//****************************************************//
	
	printf("\n===== 506 Personal information =====");
	printf("\nShreyas Srinivasan");
	printf("\nssrini22");
	printf("\nECE492 Students? NO");
	printf("\n===== 506 SMP Simulator configuration =====");
	if(protocol == 0)
		printf("\nL1_SIZE: %d \nL1_ASSOC: %d \nL1_BLOCKSIZE: %d \nNUMBER OF PROCESSORS: %d \nCOHERENCE PROTOCOL: MSI\n", cache_size, cache_assoc, blk_size, num_processors);
	else if(protocol == 1)
		printf("\nL1_SIZE: %d \nL1_ASSOC: %d \nL1_BLOCKSIZE: %d \nNUMBER OF PROCESSORS: %d \nCOHERENCE PROTOCOL: MESI\n", cache_size, cache_assoc, blk_size, num_processors);
	else if(protocol == 2)
		printf("\nL1_SIZE: %d \nL1_ASSOC: %d \nL1_BLOCKSIZE: %d \nNUMBER OF PROCESSORS: %d \nCOHERENCE PROTOCOL: Dragon\n", cache_size, cache_assoc, blk_size, num_processors);
	
	cout<<"TRACE FILE: trace/"<<fname;
	//*********************************************//
        //*****create an array of caches here**********//
	//*********************************************//	

	Cache **caches = new Cache *[num_processors];
	int i;
	for(i=0;i<num_processors;i++)
	{
		caches[i] = new Cache(cache_size, cache_assoc, blk_size,num_processors,caches);
	}



	pFile = fopen (fname,"r");
	if(pFile == 0)
	{   
		printf("Trace file problem\n");
		exit(0);
	}
	///******************************************************************//
	//**read trace file,line by line,each(processor#,operation,address)**//
	//*****propagate each request down through memory hierarchy**********//
	//*****by calling cachesArray[processor#]->Access(...)***************//
	///******************************************************************//
	int total = 0;
	while (fscanf(pFile, "%d %c %lx", &proc_number, &op, &addr) != EOF) 
	{
		++total;
		if(op == 'w')
		{
			//printf("\n Write operation by Processor %d of address %x", proc_number, addr);
			if(protocol == 0)
				caches[proc_number]->MSI(proc_number,addr,op,processor_op);
			else if(protocol == 1)
				caches[proc_number]->MESI(proc_number,addr,op,processor_op);
			else if(protocol == 2)
				caches[proc_number]->Dragon(proc_number,addr,op,processor_op,read);
		}
		else if(op == 'r')
		{
			//printf("\n Read operation by Processor %d of address %x", proc_number, addr);
			if(protocol == 0)
				caches[proc_number]->MSI(proc_number,addr,op,processor_op);
			else if(protocol == 1)
				caches[proc_number]->MESI(proc_number,addr,op,processor_op);
			else if(protocol == 2)
				caches[proc_number]->Dragon(proc_number,addr,op,processor_op,read);
		}
	}
			

		
	
	fclose(pFile);

	//********************************//
	//print out all caches' statistics //
	//********************************//
	for(i=0;i<num_processors;i++)
	{
			printf("\n============ Simulation results (Cache %d) ============",i);
		caches[i]->printStats(protocol);
	}
	printf("\n");
	
}
