/* $Id: Cache.hpp 21152 2009-06-25 11:57:19Z ben2610 $
 * Cache.hpp
 *
 *  Created on: Feb 24, 2009
 *      Author: benoit tbolsee
 */

#ifndef CACHE_HPP_
#define CACHE_HPP_

#include <map>

namespace iTaSC {

#define CACHE_LOOKUP_TABLE_SIZE				128
#define CACHE_DEFAULT_BUFFER_SIZE			32768
#define CACHE_CHANNEL_EXTEND_SIZE			10
#define CACHE_MAX_ITEM_SIZE					0x3FFF0

/* macro to get the alignement gap after an item header */
#define CACHE_ITEM_GAPB(item)				(unsigned int)(((size_t)item+sizeof(CacheItem))&(sizeof(void*)-1))
/* macro to get item data position, item=CacheItem pointer */
#define CACHE_ITEM_DATA_POINTER(item)		(void*)((unsigned char*)item+sizeof(CacheItem)+CACHE_ITEM_GAPB(item))
/* macro to get item size in 32bit words from item address and length, item=CacheItem pointer */
#define CACHE_ITEM_SIZEW(item,length)		(unsigned int)((sizeof(CacheItem)+CACHE_ITEM_GAPB(item)+(((length)+3)&~0x3))>>2)
/* macto to move from one item to the next, item=CacheItem pointer, updated by the macro */
#define CACHE_NEXT_ITEM(item)				((item)+(item)->m_sizeW)
#define CACHE_BLOCK_ITEM_ADDR(chan,buf,block)	(&(buf)->m_firstItem+(((unsigned int)(block)<<chan->m_positionToBlockShiftW)+(buf)->lookup[block].m_offsetW))
#define CACHE_ITEM_ADDR(buf,pos)			(&(buf)->m_firstItem+(pos))
#define CACHE_ITEM_POSITIONW(buf,item)		(unsigned int)(item-&buf->m_firstItem)

typedef unsigned int CacheTS;

struct Timestamp 
{
	double realTimestamp;
	double realTimestep;
	CacheTS cacheTimestamp;
	unsigned int numstep:8;
	unsigned int substep:1;
	unsigned int reiterate:1;
	unsigned int cache:1;
	unsigned int update:1;
	unsigned int interpolate:1;
	unsigned int dummy:19;

	Timestamp() { memset(this, 0, sizeof(Timestamp)); }
};

/* utility function to return second timestamp to millisecond */
inline void setCacheTimestamp(Timestamp& timestamp)
{
	if (timestamp.realTimestamp < 0.0 || timestamp.realTimestamp > 4294967.295)
		timestamp.cacheTimestamp = 0;
	else
		timestamp.cacheTimestamp = (CacheTS)(timestamp.realTimestamp*1000.0+0.5);
}


/*
class Cache:
Top level class, only one instance of this class should exists.
A device (=constraint, object) uses this class to create a cache entry for its data.
A cache entry is divided into cache channels, each providing a separate buffer for cache items.
The cache channels must be declared by the devices before they can be used.
The device must specify the largest cache item (limited to 256Kb) so that the cache
buffer can be organized optimally.
Cache channels are identified by small number (starting from 0) allocated by the cache system.
Cache items are inserted into cache channels ordered by timestamp. Writing is always done
at the end of the cache buffer: writing an item automatically clears all items with 
higher timestamp.
A cache item is an array of bytes provided by the device; the format of the cache item is left 
to the device. 
The device can retrieve a cache item associated with a certain timestamp. The cache system
returns a pointer that points directly in the cache buffer to avoid unnecessary copy. 
The pointer is guaranteed to be pointer aligned so that direct mapping to C structure is possible 
(=32 bit aligned on 32 systems and 64 bit aligned on 64 bits system).

Timestamp = rounded time in millisecond.
*/

struct CacheEntry;
struct CacheBuffer;
struct CacheItem;
struct CacheChannel;

class Cache 
{
private:
	/* map between device and cache entry.
	   Dynamically updated when more devices create cache channels */
	typedef std::map<const void *, struct CacheEntry*> CacheMap;
	CacheMap  m_cache;
	const CacheItem *getCurrentCacheItemInternal(const void *device, int channel, CacheTS timestamp);
   
public:
	Cache();
	~Cache();
	/* add a cache channel, maxItemSize must be < 256k.
	   name : channel name, truncated at 31 characters
	   msxItemSize : maximum size of item in bytes, items of larger size will be rejected
	   return value >= 0: channel id, -1: error */
	int addChannel(const void *device, const char *name, unsigned int maxItemSize);

	/* delete a cache channel (and all associated buffers and items) */
	int deleteChannel(const void *device, int channel);
	/* delete all channels of a device and remove the device from the map */
	int deleteDevice(const void *device);
	/* removes all cache items, leaving the special item at timestamp=0. 
	   if device=NULL, apply to all devices. */
	void clearCacheFrom(const void *device, CacheTS timestamp);

	/* add a new cache item
	   channel: the cache channel (as returned by AddChannel
	   data, length: the cache item and length in bytes
	                 If data is NULL, the memory is allocated in the cache but not writen
	   return: error: NULL, success: pointer to item in cache */
	void *addCacheItem(const void *device, int channel, CacheTS timestamp, void *data, unsigned int length);

	/* specialized function to add a vector of double in the cache
	   It will first check if a vector exist already in the cache for the same timestamp
	   and compared the cached vector with the new values. 
	   If all values are within threshold, the vector is updated but the cache is not deleted
	   for the future timestamps. */
	double *addCacheVectorIfDifferent(const void *device, int channel, CacheTS timestamp, double *data, unsigned int length, double threshold);

	/* returns the cache item with timestamp that is just before the given timestamp.
	   returns the data pointer or NULL if there is no cache item before timestamp.
	   On return, timestamp is updated with the actual timestamp of the item being returned.
	   Note that the length of the item is not returned, it is up to the device to organize
	   the data so that length can be retrieved from the data if needed.
	   Device can NULL, it will then just look the first channel available, useful to 
	   test the status of the cache. */
	const void *getPreviousCacheItem(const void *device, int channel, CacheTS *timestamp);

	/* returns the cache item with the timestamp that is exactly equal to the given timestamp
	   If there is no cache item for this timestamp, returns NULL.*/
	const void *getCurrentCacheItem(const void *device, int channel, CacheTS timestamp);

};

/* the following structures are not internal use only, they should not be used directly */

struct CacheEntry 
{
	CacheChannel *m_channelArray;		// array of channels, automatically resized if more channels are created
	unsigned int m_count;				// number of channel in channelArray
	CacheEntry() : m_channelArray(NULL), m_count(0) {}
	~CacheEntry();
};

struct CacheChannel
{
	CacheItem* initItem;				// item corresponding to timestamp=0
	struct CacheBuffer *m_firstBuffer;	// first buffer of list
	struct CacheBuffer *m_lastBuffer;		// last buffer of list to which an item was written
	char m_name[32];						// channel name
	unsigned char m_busy;					// =0 if channel is free, !=0 when channel is in use
	unsigned char m_positionToBlockShiftW;	// number of bits to shift a position in word to get the block number
	unsigned short m_positionToOffsetMaskW;   // bit mask to apply on a position in word to get offset in a block
	unsigned int m_maxItemSizeB;			// maximum item size in bytes
	unsigned int m_bufferSizeW;			// size of item buffer in word to allocate when a new buffer must be created
	unsigned int m_blockSizeW;			// block size in words of the lookup table
	unsigned int m_lastTimestamp;			// timestamp of the last item that was written
	unsigned int m_lastItemPositionW;		// position in words in lastBuffer of the last item written
	void clear();
	CacheBuffer* allocBuffer();
	CacheItem* findItemOrLater(unsigned int timestamp, CacheBuffer **rBuffer);
	CacheItem* findItemEarlier(unsigned int timestamp, CacheBuffer **rBuffer);
	// Internal function: finds an item in a buffer that is < timeOffset
	// timeOffset must be a valid offset for the buffer and the buffer must not be empty
	// on return highBlock contains the block with items above or equal to timeOffset
	CacheItem *_findBlock(CacheBuffer *buffer, unsigned short timeOffset, unsigned int *highBlock);
};

struct CacheBlock {
	unsigned short m_timeOffset;		// timestamp relative to m_firstTimestamp
	unsigned short m_offsetW;			// position in words of item relative to start of block
};

/* CacheItem is the header of each item in the buffer, must be 32bit
   Items are always 32 bits aligned and size is the number of 32 bit words until the
   next item header, including an eventual pre and post padding gap for pointer alignment */
struct CacheItem
{
	unsigned short m_timeOffset;		// timestamp relative to m_firstTimestamp
	unsigned short m_sizeW;			// size of item in 32 bit words
	// item data follows header immediately or after a gap if position is not pointer aligned
};

// Buffer header
// Defined in a macro to avoid sizeof() potential problem.
//	next				for linked list. = NULL for last buffer
//  m_firstTimestamp		timestamp of first item in this buffer
//  m_lastTimestamp		timestamp of last item in this buffer
//						m_lastTimestamp must be < m_firstTimestamp+65536
//  m_lastItemPositionW	position in word of last item written
//  m_firstFreePositionW	position in word where a new item can be written, 0 if buffer is empty
//  lookup				lookup table for fast access to item by timestamp
//						The buffer is divided in blocks of 2**n bytes with n chosen so that 
//						there are no more than CACHE_LOOKUP_TABLE_SIZE blocks and that each 
//						block will contain at least one item. 
//						Each element of the lookup table gives the timestamp and offset 
//						of the last cache item occupying (=starting in) the corresponding block.
#define CACHE_HEADER \
	struct CacheBuffer *m_next;		\
	unsigned int m_firstTimestamp;	\
	unsigned int m_lastTimestamp;		\
									\
	unsigned int m_lastItemPositionW;	\
	unsigned int m_firstFreePositionW;\
	struct CacheBlock lookup[CACHE_LOOKUP_TABLE_SIZE]

struct CacheBufferHeader {
	CACHE_HEADER;
};
#define CACHE_BUFFER_HEADER_SIZE	(sizeof(struct CacheBufferHeader))
struct CacheBuffer
{
	CACHE_HEADER;
	struct CacheItem m_firstItem;			// the address of this field marks the start of the buffer
};


}

#endif /* CACHE_HPP_ */
