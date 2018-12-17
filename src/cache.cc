/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include "cache.h"
using namespace std;

Cache::Cache(int s,int a,int b,int num_processors,Cache **cache1)
{
   ulong i, j;
   reads = readMisses = writes = 0; 
   writeMisses = writeBacks = currentCycle = 0;
   

   size       = (ulong)(s);
   lineSize   = (ulong)(b);
   assoc      = (ulong)(a);   
   sets       = (ulong)((s/b)/a);
   numLines   = (ulong)(s/b);
   log2Sets   = (ulong)(log2(sets));   
   log2Blk    = (ulong)(log2(b));   
  
   //*******************//
   //initialize your counters here//
   //*******************//
   
   mem_access = flushes = bus_rdx = interventions = invalidations = cache2cache = 0;
   num_procs = num_processors;
   
   caches = cache1;
 
   tagMask =0;
   for(i=0;i<log2Sets;i++)
   {
		tagMask <<= 1;
        tagMask |= 1;
   }
   
   /**create a two dimentional cache, sized as cache[sets][assoc]**/ 
   cache = new cacheLine*[sets];
   for(i=0; i<sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for(j=0; j<assoc; j++) 
      {
	   cache[i][j].invalidate();
      }
   }      
   
}

/**you might add other parameters to Access()
since this function is an entry point 
to the memory hierarchy (i.e. caches)**/

//MSI protocol
void Cache::MSI(int proc_number, ulong addr,uchar op,int operation)
{
	int i;
	currentCycle++;/*per cache global counter to maintain LRU order 
			among cache ways, updated on every cache access*/
			
    if(operation == processor_op)
	{    	
		if(op == 'w') writes++;
		else          reads++;

		cacheLine * line = findLine(addr);

		if(line == NULL)/*miss*/
		{
			if(op == 'w') writeMisses++;
			else readMisses++;

			cacheLine *newline = fillLine(addr);
			if(op == 'w') 
			{
				newline->setFlags(modified);    
				++bus_rdx;
				for(i=0;i<num_procs;i++)
				{
					if(i!=proc_number)
						caches[i]->MSI(i,addr,op,bus_op);
				}
			}
			
			if(op == 'r')
			{
				newline->setFlags(shared);
				for(i=0;i<num_procs;i++)
				{
					if(i!=proc_number)
						caches[i]->MSI(i,addr,op,bus_op);
				}
			}
		
		}
		else
		{
		/**since it's a hit, update LRU and update dirty flag**/
			updateLRU(line);
			if(line->getFlags() == shared)
			{
				if(op == 'w') 
				{
					line->setFlags(modified);
					++bus_rdx;
					++mem_access;
					for(i=0;i<num_procs;i++)
					{
						if(i!=proc_number)
							caches[i]->MSI(i,addr,op,bus_op);
					}
				}
				else if(op == 'r')
					line->setFlags(shared);
			}
				
		}
	}
	else if(operation == bus_op)
	{
		cacheLine * line = findLine(addr);
		if(line!=NULL)
		{
			if(line->getFlags() == modified)
			{
				if(op == 'r')
				{
					line->setFlags(shared);
					++interventions;
					writeBack(addr);
					++flushes;
				}
				if(op == 'w')
				{
					line->setFlags(invalid);
					++invalidations;
					writeBack(addr);
					++flushes;
				}
			}
			else if(line->getFlags() == shared)
			{
				if(op == 'w')
				{
					line->setFlags(invalid);
					++invalidations;
				}
			}
		}
	}
}

//MESI protocol
void Cache::MESI(int proc_number, ulong addr,uchar op,int operation)
{
	int i;
	currentCycle++;/*per cache global counter to maintain LRU order 
			among cache ways, updated on every cache access*/
			
	int c = 0;
	for(i=0;i<num_procs;i++)
	{
		if(i!=proc_number)
		{
			c = caches[i]->check_for_c(addr);
			if(c)
				break;
		}
	}
	
			
    if(operation == processor_op)
	{    	
		bus_upgr = 0;
		if(op == 'w') writes++;
		else          reads++;

		cacheLine * line = findLine(addr);

		if(line == NULL)/*miss*/
		{
			if(op == 'w') writeMisses++;
			else readMisses++;

			cacheLine *newline = fillLine(addr);
			if(op == 'w') 
			{
				newline->setFlags(modified);    
				++bus_rdx;
				if(c)
					++cache2cache;
				for(i=0;i<num_procs;i++)
				{
					if(i!=proc_number)
						caches[i]->MESI(i,addr,op,bus_op);
				}
			}
			
			if(op == 'r')
			{
				if(!c)
				{
					newline->setFlags(exclusive);
					for(i=0;i<num_procs;i++)
					{
						if(i!=proc_number)
							caches[i]->MESI(i,addr,op,bus_op);
					}
				}
				else if(c)
				{
					newline->setFlags(shared);
					++cache2cache;
					for(i=0;i<num_procs;i++)
					{
						if(i!=proc_number)
							caches[i]->MESI(i,addr,op,bus_op);
					}
				}
			}
		
		}
		else
		{
		/**since it's a hit, update LRU and update dirty flag**/
			updateLRU(line);
			if(line->getFlags() == exclusive)
			{
				if(op == 'w')
					line->setFlags(modified);
			}
			else if(line->getFlags() == shared)
			{
				if(op == 'w') 
				{
					line->setFlags(modified);
					bus_upgr = 1;
					for(i=0;i<num_procs;i++)
					{
						if(i!=proc_number)
							caches[i]->MESI(i,addr,op,bus_op);
					}
				}
			}
				
		}
	}
	else if(operation == bus_op)
	{
		cacheLine * line = findLine(addr);
		if(line!=NULL)
		{
			if(line->getFlags() == modified)
			{
//				if(bus_upgr == 0)
//				{
					if(op == 'r')
					{
						line->setFlags(shared);
						++interventions;
						writeBack(addr);
						++flushes;
					}
					if(op == 'w')
					{
						line->setFlags(invalid);
						++invalidations;
						writeBack(addr);
						++flushes;
					}
//				}
			}
			else if(line->getFlags() == shared)
			{
				if(bus_upgr == 0)
				{
					if(op == 'w')
					{
						line->setFlags(invalid);
						++invalidations;
					}
				}
				else if(bus_upgr == 1)
				{
					if(op == 'w')
					{
						line->setFlags(invalid);
						++invalidations;
					}
				}
			}
			else if(line->getFlags() == exclusive)
			{
				if(bus_upgr == 0)
				{
					if(op == 'w')
					{
						line->setFlags(invalid);
						++invalidations;
					}
					else if(op == 'r')
					{
						line->setFlags(shared);
						++interventions;
					}
				}
			}
						
		}
	}
}

//Dragon Protocol
//MESI protocol
void Cache::Dragon(int proc_number, ulong addr,uchar op,int operation,int bus_type)
{
	int i;
	currentCycle++;/*per cache global counter to maintain LRU order 
			among cache ways, updated on every cache access*/
			
	int c = 0;
	for(i=0;i<num_procs;i++)
	{
		if(i!=proc_number)
		{
			c = caches[i]->check_for_c(addr);
			if(c)
				break;
		}
	}
	
			
    if(operation == processor_op)
	{    	
		bus_update = 0;
		if(op == 'w') writes++;
		else          reads++;

		if(op == 'w')
		{
			cacheLine * line = findLine(addr);

			if(line == NULL)/*miss*/
			{
				++writeMisses;
				
				if(c)
				{
					cacheLine *newline = fillLine(addr);
					newline->setFlags(shared_modified);
					
					for(i=0;i<num_procs;i++)
					{
						bus_type = read;
						if(i!=proc_number)
							caches[i]->Dragon(i,addr,op,bus_op,bus_type);
					}
					bus_update = 1;
					for(i=0;i<num_procs;i++)
					{
						bus_type = update;
						if(i!=proc_number)
							caches[i]->Dragon(i,addr,op,bus_op,bus_type);
					}
				}
				else if(c == 0)
				{
					cacheLine *newline = fillLine(addr);
					newline->setFlags(modified);
					
					for(i=0;i<num_procs;i++)
					{
						bus_type = read;
						if(i!=proc_number)
							caches[i]->Dragon(i,addr,op,bus_op,bus_type);
					}
				}
			}
			else	
			{
				updateLRU(line);
				if(line->getFlags() == exclusive)
					line->setFlags(modified);
				else if(line->getFlags() == shared_modified)
				{
					if(c)
					{
						bus_update = 1;
						for(i=0;i<num_procs;i++)
						{
							bus_type = update;
							if(i!=proc_number)
								caches[i]->Dragon(i,addr,op,bus_op,bus_type);
						}
					}
					else if(c == 0)
					{
						line->setFlags(modified);
						bus_update = 1;
						for(i=0;i<num_procs;i++)
						{
							bus_type = update;
							if(i!=proc_number)
								caches[i]->Dragon(i,addr,op,bus_op,bus_type);
						}
					}
				}
				else if(line->getFlags() == shared_clean)
				{
					if(c)
					{
						line->setFlags(shared_modified);
						bus_update = 1;
						for(i=0;i<num_procs;i++)
						{
							bus_type = update;
							if(i!=proc_number)
								caches[i]->Dragon(i,addr,op,bus_op,bus_type);
						}
					}
					else if(c == 0)
					{
						line->setFlags(modified);
						bus_update = 1;
						for(i=0;i<num_procs;i++)
						{
							bus_type = update;
							if(i!=proc_number)
								caches[i]->Dragon(i,addr,op,bus_op,bus_type);
						}
					}
				}	
			}
		}
			
		if(op == 'r')
		{
			cacheLine * line = findLine(addr);

			if(line == NULL)/*miss*/
			{
				++readMisses;
				if(c)
				{
					cacheLine *newline = fillLine(addr);
					newline->setFlags(shared_clean);
					
					for(i=0;i<num_procs;i++)
					{
						bus_type = read;
						if(i!=proc_number)
							caches[i]->Dragon(i,addr,op,bus_op,bus_type);
					}
				}
				else if(c == 0)
				{
					cacheLine *newline = fillLine(addr);
					newline->setFlags(exclusive);
					
					for(i=0;i<num_procs;i++)
					{
						bus_type = read;
						if(i!=proc_number)
							caches[i]->Dragon(i,addr,op,bus_op,bus_type);
					}
				}
			}
			else
				updateLRU(line);
		}
		
	}
	else if(operation == bus_op)
	{
		cacheLine * line = findLine(addr);
		if(line!=NULL)
		{
			if(line->getFlags() == exclusive)
			{
				if(bus_type == read)	
				{
					line->setFlags(shared_clean);
					++interventions;
				}
			}
			else if(line->getFlags() == modified)
			{
				if(bus_type == read)
				{
					line->setFlags(shared_modified);
					++interventions;
					++flushes;
				}
			}
			else if(line->getFlags() == shared_modified)
			{
				if(bus_type == read)
				{
					++flushes;
				}
				else if(bus_type == update)
				{
						line->setFlags(shared_clean);
						ulong tag = calcTag(addr);
						line->setTag(tag);
				}
			}
			else if(line->getFlags() == shared_clean)
			{
				if(bus_type == update)
				{
						ulong tag = calcTag(addr);
						line->setTag(tag);
				}
			}			
		}
	}
}

// Check for C
int Cache::check_for_c(ulong addr)
{
   int temp = 0;
   ulong tag, i;

   tag = calcTag(addr);
   i   = calcIndex(addr);

  for(ulong j=0; j<assoc; j++)
   if(cache[i][j].isValid())   //Either SHARED or MODIFIED or EXCLUSIVE
     if(cache[i][j].getTag() == tag)
      {
           temp = 1; break; 
      }

   return temp;
}


/*look up line*/
cacheLine * Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;
   
   pos = assoc;
   tag = calcTag(addr);
   i   = calcIndex(addr);
  
   for(j=0; j<assoc; j++)
	if(cache[i][j].isValid())
	        if(cache[i][j].getTag() == tag)
		{
		     pos = j; break; 
		}
   if(pos == assoc)
	return NULL;
   else
	return &(cache[i][pos]); 
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
  line->setSeq(currentCycle);  
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine * Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min    = currentCycle;
   i      = calcIndex(addr);
   
   for(j=0;j<assoc;j++)
   {
      if(cache[i][j].isValid() == 0) return &(cache[i][j]);     
   }   
   for(j=0;j<assoc;j++)
   {
	 if(cache[i][j].getSeq() <= min) { victim = j; min = cache[i][j].getSeq();}
   } 
   assert(victim != assoc);
   
   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine * victim = getLRU(addr);
   updateLRU(victim);
  
   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{ 
   ulong tag;
  
   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   if((victim->getFlags() == modified) || (victim->getFlags() == shared_modified)) 
	   writeBack(addr);
    	
   tag = calcTag(addr);   
   victim->setTag(tag);
   victim->setFlags(invalid);    
   /**note that this cache line has been already 
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats(int protocol)
{ 
	float miss_rate;
	float rm,wm,r,w;
	r = reads;
	w = writes;
	rm = readMisses;
	wm = writeMisses;
	miss_rate = ((rm+wm)/(r+w))*100;
	ulong memAccess;
	char percent_symbol = '%';
	
	if(protocol == 0)
		memAccess = mem_access+readMisses+writeMisses+writeBacks;
	else if(protocol == 1)
		memAccess = readMisses+writeMisses+writeBacks-cache2cache;
	else if(protocol == 2)
		memAccess = readMisses+writeMisses+writeBacks;
		
	
	printf("\n 01. number of reads:     \t %lu",reads);
	printf("\n 02. number of read misses:\t %lu",readMisses);
	printf("\n 03. number of writes:     \t %lu",writes);
	printf("\n 04. number of write misses:\t %lu",writeMisses);
	printf("\n 05. total miss rate:		 %.2f", miss_rate);
	printf("%c", percent_symbol);
	printf("\n 06. number of writebacks: \t %lu", writeBacks);
	printf("\n 07. number of cache-to-cache transfers: %lu",cache2cache);
	printf("\n 08. number of memory transactions:  %lu", memAccess);
	printf("\n 09. number of interventions:	 %lu",interventions);
	printf("\n 10. number of invalidations:\t %lu",invalidations);
	printf("\n 11. number of flushes:    \t %lu", flushes);
	printf("\n 12. number of BusRdX:     \t %lu", bus_rdx);
}
