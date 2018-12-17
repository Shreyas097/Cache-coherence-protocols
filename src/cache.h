/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

#define processor_op 1
#define bus_op 2

#define read 0
#define update 1

/****add new states, based on the protocol****/
enum{
	invalid = 0,
	modified,
	shared,
	exclusive,
	shared_clean,
	shared_modified
};

class cacheLine 
{
public:
   ulong tag;
   ulong Flags;   // 0:invalid, 1:valid, 2:dirty 
   ulong seq; 
 
//public:
   cacheLine()            { tag = 0; Flags = 0; }
   ulong getTag()         { return tag; }
   ulong getFlags()			{ return Flags;}
   ulong getSeq()         { return seq; }
   void setSeq(ulong Seq)			{ seq = Seq;}
   void setFlags(ulong flags)			{  Flags = flags;}
   void setTag(ulong a)   { tag = a; }
   void invalidate()      { tag = 0; Flags = invalid; }//useful function
   bool isValid()         { return ((Flags) != invalid); }
};

class Cache
{
public:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads,readMisses,writes,writeMisses,writeBacks;
   ulong mem_access, flushes, bus_rdx, interventions, invalidations, cache2cache;	
   int num_procs;
   int bus_upgr;
   int bus_update;

   //******///
   //add coherence counters here///
   //******///

   cacheLine **cache;
   ulong calcTag(ulong addr)     { return (addr >> (log2Blk) );}
   ulong calcIndex(ulong addr)  { return ((addr >> log2Blk) & tagMask);}
   ulong calcAddr4Tag(ulong tag)   { return (tag << (log2Blk));}
   
//public:
    ulong currentCycle;  
	Cache **caches;
     
    Cache(int,int,int,int,Cache *c[]);
   ~Cache() { delete cache;}
   
   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine * findLine(ulong addr);
   cacheLine * getLRU(ulong);
   
   ulong getRM(){return readMisses;} 
   ulong getWM(){return writeMisses;} 
   ulong getReads(){return reads;}
   ulong getWrites(){return writes;}
   ulong getWB(){return writeBacks;}
   
   void writeBack(ulong)   {writeBacks++;}
   void printStats(int);
   void updateLRU(cacheLine *);

   //******///
   //add other functions to handle bus transactions///
   //******///
   
   void MSI(int,ulong,uchar,int);
   void MESI(int,ulong,uchar,int);
   void Dragon(int,ulong,uchar,int,int);
   
   
   ulong get_invalidations() { return invalidations; }
   ulong get_interventions() { return interventions; } 
   ulong get_flushes() { return flushes;	}
   ulong get_bus_rdx()   { return bus_rdx;  }
   
   int check_for_c(ulong);

};

#endif
