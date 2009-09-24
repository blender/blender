/* $Id: Cache.cpp 21152 2009-06-25 11:57:19Z ben2610 $
 * Cache.cpp
 *
 *  Created on: Feb 24, 2009
 *      Author: benoit bolsee
 */
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include "Cache.hpp"

#include <math.h>

namespace iTaSC {

CacheEntry::~CacheEntry()
{
   for (unsigned int id=0; id < m_count; id++)
		m_channelArray[id].clear();
   if (m_channelArray)
	   free(m_channelArray);
}

CacheItem *CacheChannel::_findBlock(CacheBuffer *buffer, unsigned short timeOffset, unsigned int *retBlock)
{
	// the timestamp is necessarily in this buffer
	unsigned int lowBlock, highBlock, midBlock;
	if (timeOffset <= buffer->lookup[0].m_timeOffset) {
		// special case: the item is in the first block, search from start
		*retBlock = 0;
		return &buffer->m_firstItem;
	}
	// general case, the item is in the middle of the buffer
	// before doing a dycotomic search, we will assume that timestamp
	// are regularly spaced so that we can try to locate the block directly
	highBlock = buffer->m_lastItemPositionW>>m_positionToBlockShiftW;
	lowBlock = midBlock = (timeOffset*highBlock)/(buffer->m_lastTimestamp-buffer->m_firstTimestamp);
	// give some space for security
	if (lowBlock > 0)
		lowBlock--;
	if (timeOffset <= buffer->lookup[lowBlock].m_timeOffset) {
		// bad guess, but we know this block is a good high block, just use it
		highBlock = lowBlock;
		lowBlock = 0;
	} else {
		// ok, good guess, now check the high block, give some space
		if (midBlock < highBlock)
			midBlock++;
		if (timeOffset <= buffer->lookup[midBlock].m_timeOffset) {
			// good guess, keep that block as the high block
			highBlock = midBlock;
		}
	}
	// the item is in a different block, do a dycotomic search
	// the timestamp is alway > lowBlock and <= highBlock
	while (1) {
		midBlock = (lowBlock+highBlock)/2;
		if (midBlock == lowBlock) {
			// low block and high block are contigous, we can start search from the low block
			break;
		} else if (timeOffset <= buffer->lookup[midBlock].m_timeOffset) {
			highBlock = midBlock;
		} else {
			lowBlock = midBlock;
		}
	}
	assert (lowBlock != highBlock);
	*retBlock = highBlock;
	return CACHE_BLOCK_ITEM_ADDR(this,buffer,lowBlock);
}

void CacheChannel::clear()
{
	CacheBuffer *buffer, *next;
	for (buffer=m_firstBuffer; buffer != 0; buffer = next) {
		next = buffer->m_next;
		free(buffer);
	}
	m_firstBuffer = NULL;
	m_lastBuffer = NULL;
	if (initItem) {
		free(initItem);
		initItem = NULL;
	}
}

CacheBuffer* CacheChannel::allocBuffer()
{
	CacheBuffer* buffer;
	if (!m_busy)
		return NULL;
	buffer = (CacheBuffer*)malloc(CACHE_BUFFER_HEADER_SIZE+(m_bufferSizeW<<2));
	if (buffer) {
		memset(buffer, 0, CACHE_BUFFER_HEADER_SIZE);
	}
	return buffer;
}

CacheItem* CacheChannel::findItemOrLater(unsigned int timestamp, CacheBuffer **rBuffer)
{
	CacheBuffer* buffer;
	CacheItem *item, *limit;
	if (!m_busy)
		return NULL;
	if (timestamp == 0 && initItem) {
		*rBuffer = NULL;
		return initItem;
	}
	for (buffer=m_firstBuffer; buffer; buffer = buffer->m_next) {
		if (buffer->m_firstFreePositionW == 0)
			// buffer is empty, this must be the last and we didn't find the timestamp
			return NULL;
		if (timestamp < buffer->m_firstTimestamp) {
			*rBuffer = buffer;
			return &buffer->m_firstItem;
		}
		if (timestamp <= buffer->m_lastTimestamp) {
			// the timestamp is necessarily in this buffer
			unsigned short timeOffset = (unsigned short)(timestamp-buffer->m_firstTimestamp);
			unsigned int highBlock;
			item = _findBlock(buffer, timeOffset, &highBlock);
			// now we do a linear search until we find a timestamp that is equal or higher
			// we should normally always find an item but let's put a limit just in case
			limit = CACHE_BLOCK_ITEM_ADDR(this,buffer,highBlock);
			while (item<=limit && item->m_timeOffset < timeOffset )
				item = CACHE_NEXT_ITEM(item);
			assert(item<=limit);
			*rBuffer = buffer;
			return item;
		}
		// search in next buffer
	}
	return NULL;
}

CacheItem* CacheChannel::findItemEarlier(unsigned int timestamp, CacheBuffer **rBuffer)
{
	CacheBuffer *buffer, *prevBuffer;
	CacheItem *item, *limit, *prevItem;
	if (!m_busy)
		return NULL;
	if (timestamp == 0)
		return NULL;
	for (prevBuffer=NULL, buffer=m_firstBuffer; buffer; prevBuffer = buffer, buffer = buffer->m_next) {
		if (buffer->m_firstFreePositionW == 0)
			// buffer is empty, this must be the last and we didn't find the timestamp
			return NULL;
		if (timestamp <= buffer->m_firstTimestamp) {
			if (prevBuffer == NULL) {
				// no item before, except the initial item
				*rBuffer = NULL;
				return initItem;
			}
			// the item is necessarily the last one of previous buffer
			*rBuffer = prevBuffer;
			return CACHE_ITEM_ADDR(prevBuffer,prevBuffer->m_lastItemPositionW);
		}
		if (timestamp <= buffer->m_lastTimestamp) {
			// the timestamp is necessarily in this buffer
			unsigned short timeOffset = (unsigned short)(timestamp-buffer->m_firstTimestamp);
			unsigned int highBlock;
			item = _findBlock(buffer, timeOffset, &highBlock);
			// now we do a linear search until we find a timestamp that is equal or higher
			// we should normally always find an item but let's put a limit just in case
			limit = CACHE_BLOCK_ITEM_ADDR(this,buffer,highBlock);
			prevItem = NULL;
			while (item<=limit && item->m_timeOffset < timeOffset) {
				prevItem = item;
				item = CACHE_NEXT_ITEM(item);
			}
			assert(item<=limit && prevItem!=NULL);
			*rBuffer = buffer;
			return prevItem;
		}
		// search in next buffer
	}
	// pass all buffer, the last item is the last item of the last buffer
	if (prevBuffer == NULL) {
		// no item before, except the initial item
		*rBuffer = NULL;
		return initItem;
	}
	// the item is necessarily the last one of previous buffer
	*rBuffer = prevBuffer;
	return CACHE_ITEM_ADDR(prevBuffer,prevBuffer->m_lastItemPositionW);
}


Cache::Cache()
{
}

Cache::~Cache()
{
	CacheMap::iterator it;
	for (it=m_cache.begin(); it!=m_cache.end(); it=m_cache.begin()) {
		deleteDevice(it->first);
	}
}

int Cache::addChannel(const void *device, const char *name, unsigned int maxItemSize)
{
	CacheMap::iterator it = m_cache.find(device);
	CacheEntry *entry;
	CacheChannel *channel;
	unsigned int id;

	if (maxItemSize > 0x3FFF0)
		return -1;

	if (it == m_cache.end()) {
		// device does not exist yet, create a new entry
		entry = new CacheEntry();
		if (entry == NULL)
			return -1;
		if (!m_cache.insert(CacheMap::value_type(device,entry)).second)
			return -1;
	} else {
		entry = it->second;
	}
	// locate a channel with the same name and reuse
	for (channel=entry->m_channelArray, id=0; id<entry->m_count; id++, channel++) {
		if (channel->m_busy && !strcmp(name, channel->m_name)) {
			// make this channel free again
			deleteChannel(device, id);
			// there can only be one channel with the same name
			break;
		}
	}
	for (channel=entry->m_channelArray, id=0; id<entry->m_count; id++, channel++) {
		// locate a free channel
		if (!channel->m_busy)
			break;
	}
	if (id == entry->m_count) {
		// no channel free, create new channels
		int newcount = entry->m_count + CACHE_CHANNEL_EXTEND_SIZE;
		channel = (CacheChannel*)realloc(entry->m_channelArray, newcount*sizeof(CacheChannel));
		if (channel == NULL)
			return -1;
		entry->m_channelArray = channel;
		memset(&entry->m_channelArray[entry->m_count], 0, CACHE_CHANNEL_EXTEND_SIZE*sizeof(CacheChannel));
		entry->m_count = newcount;
		channel = &entry->m_channelArray[id];
	}
	// compute the optimal buffer size
	// The buffer size must be selected so that
	// - it does not contain more than 1630 items (=1s of cache assuming 25 items per second)
	// - it contains at least one item
	// - it's not bigger than 256kb and preferably around 32kb
	// - it a multiple of 4
	unsigned int bufSize = 1630*(maxItemSize+4);
	if (bufSize >= CACHE_DEFAULT_BUFFER_SIZE)
		bufSize = CACHE_DEFAULT_BUFFER_SIZE;
	if (bufSize < maxItemSize+16)
		bufSize = maxItemSize+16;
	bufSize = (bufSize + 3) & ~0x3;
	// compute block size and offset bit mask
	// the block size is computed so that
	// - it is a power of 2
	// - there is at least one item per block
	// - there is no more than CACHE_LOOKUP_TABLE_SIZE blocks per buffer
	unsigned int blockSize = bufSize/CACHE_LOOKUP_TABLE_SIZE;
	if (blockSize < maxItemSize+12)
		blockSize = maxItemSize+12;
	// find the power of 2 that is immediately larger than blockSize
	unsigned int m;
	unsigned int pwr2Size = blockSize;
	while ((m = (pwr2Size & (pwr2Size-1))) != 0)
		pwr2Size = m;
	blockSize = (pwr2Size < blockSize) ? pwr2Size<<1 : pwr2Size;
	// convert byte size to word size because all positions and size are expressed in 32 bit words
	blockSize >>= 2;
	channel->m_blockSizeW = blockSize;
	channel->m_bufferSizeW = bufSize>>2;
	channel->m_firstBuffer = NULL;
	channel->m_lastBuffer = NULL;
	channel->m_busy = 1;
	channel->initItem = NULL;
	channel->m_maxItemSizeB = maxItemSize;
	strncpy(channel->m_name, name, sizeof(channel->m_name));
	channel->m_name[sizeof(channel->m_name)-1] = 0;
	channel->m_positionToOffsetMaskW = (blockSize-1);
	for (m=0; blockSize!=1; m++, blockSize>>=1);
	channel->m_positionToBlockShiftW = m;
	return (int)id;
}

int Cache::deleteChannel(const void *device, int id)
{
	CacheMap::iterator it = m_cache.find(device);
	CacheEntry *entry;

	if (it == m_cache.end()) {
		// device does not exist
		return -1;
	}
	entry = it->second;
	if (id < 0 || id >= (int)entry->m_count || !entry->m_channelArray[id].m_busy)
		return -1;
	entry->m_channelArray[id].clear();
	entry->m_channelArray[id].m_busy = 0;
	return 0;
}

int Cache::deleteDevice(const void *device)
{
	CacheMap::iterator it = m_cache.find(device);
	CacheEntry *entry;

	if (it == m_cache.end()) {
		// device does not exist
		return -1;
	}
	entry = it->second;
	delete entry;
	m_cache.erase(it);
	return 0;
}

void Cache::clearCacheFrom(const void *device, CacheTS timestamp)
{
	CacheMap::iterator it = (device) ? m_cache.find(device) : m_cache.begin();
	CacheEntry *entry;
	CacheChannel *channel;
	CacheBuffer *buffer, *nextBuffer, *prevBuffer;
	CacheItem *item, *prevItem, *nextItem;
	unsigned int positionW, block;

	while (it != m_cache.end()) {
		entry = it->second;
		for (unsigned int ch=0; ch<entry->m_count; ch++) {
			channel = &entry->m_channelArray[ch];
			if (channel->m_busy) {
				item = channel->findItemOrLater(timestamp, &buffer);
				if (item ) {
					if (!buffer) {
						// this is possible if we return the special timestamp=0 item, delete all buffers
						channel->clear();
					} else {
						// this item and all later items will be removed, clear any later buffer
						while ((nextBuffer = buffer->m_next) != NULL) {
							buffer->m_next = nextBuffer->m_next;
							free(nextBuffer);
						}
						positionW = CACHE_ITEM_POSITIONW(buffer,item);
						if (positionW == 0) {
							// this item is the first one of the buffer, remove the buffer completely
							// first find the buffer just before it
							nextBuffer = channel->m_firstBuffer;
							prevBuffer = NULL;
							while (nextBuffer != buffer) {
								prevBuffer = nextBuffer;
								nextBuffer = nextBuffer->m_next;
								// we must quit this loop before reaching the end of the list
								assert(nextBuffer);
							}
							free(buffer);
							buffer = prevBuffer;
							if (buffer == NULL)
								// this was also the first buffer
								channel->m_firstBuffer = NULL;
						} else {
							// removing this item means finding the previous item to make it the last one
							block = positionW>>channel->m_positionToBlockShiftW;
							if (block == 0) {
								// start from first item, we know it is not our item because positionW > 0
								prevItem = &buffer->m_firstItem;
							} else {
								// no need to check the current block, it will point to our item or a later one
								// but the previous block will be a good start for sure.
								block--;
								prevItem = CACHE_BLOCK_ITEM_ADDR(channel,buffer,block);
							}
							while ((nextItem = CACHE_NEXT_ITEM(prevItem)) < item)
								prevItem = nextItem;
							// we must have found our item
							assert(nextItem==item);
							// now set the buffer
							buffer->m_lastItemPositionW = CACHE_ITEM_POSITIONW(buffer,prevItem);
							buffer->m_firstFreePositionW = positionW;
							buffer->m_lastTimestamp = buffer->m_firstTimestamp + prevItem->m_timeOffset;
							block = buffer->m_lastItemPositionW>>channel->m_positionToBlockShiftW;
							buffer->lookup[block].m_offsetW = buffer->m_lastItemPositionW&channel->m_positionToOffsetMaskW;
							buffer->lookup[block].m_timeOffset = prevItem->m_timeOffset;
						}
						// set the channel
						channel->m_lastBuffer = buffer;
						if (buffer) {
							channel->m_lastTimestamp = buffer->m_lastTimestamp;
							channel->m_lastItemPositionW = buffer->m_lastItemPositionW;
						}
					}
				}
			}
		}
		if (device)
			break;
		++it;
	}
}

void *Cache::addCacheItem(const void *device, int id, unsigned int timestamp, void *data, unsigned int length)
{
	CacheMap::iterator it = m_cache.find(device);
	CacheEntry *entry;
	CacheChannel *channel;
	CacheBuffer *buffer, *next;
	CacheItem *item;
	unsigned int positionW, sizeW, block;

	if (it == m_cache.end()) {
		// device does not exist
		return NULL;
	}
	entry = it->second;
	if (id < 0 || id >= (int) entry->m_count || !entry->m_channelArray[id].m_busy)
		return NULL;
	channel = &entry->m_channelArray[id];
	if (length > channel->m_maxItemSizeB)
		return NULL;
	if (timestamp == 0) {
		// initial item, delete all buffers
		channel->clear();
		// and create initial item
		item = NULL;
		// we will allocate the memory, which is always pointer aligned => compute size
		// with NULL will give same result.
		sizeW = CACHE_ITEM_SIZEW(item,length);
		item = (CacheItem*)calloc(sizeW, 4);
		item->m_sizeW = sizeW;
		channel->initItem = item;
	} else {
		if (!channel->m_lastBuffer) {
			// no item in buffer, insert item at first position of first buffer
			positionW = 0;
			if ((buffer = channel->m_firstBuffer) == NULL) {
				buffer = channel->allocBuffer();
				channel->m_firstBuffer = buffer;
			}
		} else if (timestamp > channel->m_lastTimestamp) {
			// this is the normal case: we are writing past lastest timestamp
			buffer = channel->m_lastBuffer;
			positionW = buffer->m_firstFreePositionW;
		} else if (timestamp == channel->m_lastTimestamp) {
			// common case, rewriting the last timestamp, just reuse the last position
			buffer = channel->m_lastBuffer;
			positionW = channel->m_lastItemPositionW;
		} else {
			// general case, write in the middle of the buffer, locate the timestamp
			// (or the timestamp just after), clear this item and all future items,
			// and write at that position
			item = channel->findItemOrLater(timestamp, &buffer);
			if (item == NULL) {
				// this should not happen
				return NULL;
			}
			// this item will become the last one of this channel, clear any later buffer
			while ((next = buffer->m_next) != NULL) {
				buffer->m_next = next->m_next;
				free(next);
			}
			// no need to update the buffer, this will be done when the item is written
			positionW = CACHE_ITEM_POSITIONW(buffer,item);
		}
		item = CACHE_ITEM_ADDR(buffer,positionW);
		sizeW = CACHE_ITEM_SIZEW(item,length);
		// we have positionW pointing where we can put the item
		// before we do that we have to check if we can:
		// - enough room
		// - timestamp not too late
		if ((positionW+sizeW > channel->m_bufferSizeW) ||
			(positionW > 0 && timestamp >= buffer->m_firstTimestamp+0x10000)) {
			// we must allocate a new buffer to store this item
			// but before we must make sure that the current buffer is consistent
			if (positionW != buffer->m_firstFreePositionW) {
				// This means that we were trying to write in the middle of the buffer.
				// We must set the buffer right with positionW being the last position
				// and find the item before positionW to make it the last.
				block = positionW>>channel->m_positionToBlockShiftW;
				CacheItem *previousItem, *nextItem;
				if (block == 0) {
					// start from first item, we know it is not our item because positionW > 0
					previousItem = &buffer->m_firstItem;
				} else {
					// no need to check the current block, it will point to our item or a later one
					// but the previous block will be a good start for sure.
					block--;
					previousItem = CACHE_BLOCK_ITEM_ADDR(channel,buffer,block);
				}
				while ((nextItem = CACHE_NEXT_ITEM(previousItem)) < item)
					previousItem = nextItem;
				// we must have found our item
				assert(nextItem==item);
				// now set the buffer
				buffer->m_lastItemPositionW = CACHE_ITEM_POSITIONW(buffer,previousItem);
				buffer->m_firstFreePositionW = positionW;
				buffer->m_lastTimestamp = buffer->m_firstTimestamp + previousItem->m_timeOffset;
				block = buffer->m_lastItemPositionW>>channel->m_positionToBlockShiftW;
				buffer->lookup[block].m_offsetW = buffer->m_lastItemPositionW&channel->m_positionToOffsetMaskW;
				buffer->lookup[block].m_timeOffset = previousItem->m_timeOffset;
				// and also the channel, just in case
				channel->m_lastBuffer = buffer;
				channel->m_lastTimestamp = buffer->m_lastTimestamp;
				channel->m_lastItemPositionW = buffer->m_lastItemPositionW;
			}
			// now allocate a new buffer
			buffer->m_next = channel->allocBuffer();
			if (buffer->m_next == NULL)
				return NULL;
			buffer = buffer->m_next;
			positionW = 0;
			item = &buffer->m_firstItem;
			sizeW = CACHE_ITEM_SIZEW(item,length);
		}
		// all check passed, ready to write the item
		item->m_sizeW = sizeW;
		if (positionW == 0) {
			item->m_timeOffset = 0;
			buffer->m_firstTimestamp = timestamp;
		} else {
			item->m_timeOffset = (unsigned short)(timestamp-buffer->m_firstTimestamp);
		}
		buffer->m_lastItemPositionW = positionW;
		buffer->m_firstFreePositionW = positionW+sizeW;
		buffer->m_lastTimestamp = timestamp;
		block = positionW>>channel->m_positionToBlockShiftW;
		buffer->lookup[block].m_offsetW = positionW&channel->m_positionToOffsetMaskW;
		buffer->lookup[block].m_timeOffset = item->m_timeOffset;
		buffer->m_lastItemPositionW = CACHE_ITEM_POSITIONW(buffer,item);
		buffer->m_firstFreePositionW = buffer->m_lastItemPositionW+item->m_sizeW;
		channel->m_lastBuffer = buffer;
		channel->m_lastItemPositionW = positionW;
		channel->m_lastTimestamp = timestamp;
	}
	// now copy the item
	void *itemData = CACHE_ITEM_DATA_POINTER(item);
	if (data)
		memcpy(itemData, data, length);
	return itemData;
}

const void *Cache::getPreviousCacheItem(const void *device, int id, unsigned int *timestamp)
{
	CacheMap::iterator it;
	CacheEntry *entry;
	CacheChannel *channel;
	CacheBuffer *buffer;
	CacheItem *item;

	if (device) {
		it = m_cache.find(device);	
	} else {
		it = m_cache.begin();
	}
	if (it == m_cache.end()) {
		// device does not exist
		return NULL;
	}
	entry = it->second;
	if (id < 0 || id >= (int) entry->m_count || !entry->m_channelArray[id].m_busy)
		return NULL;
	channel = &entry->m_channelArray[id];
	if ((item = channel->findItemEarlier(*timestamp,&buffer)) == NULL)
		return NULL;
	*timestamp = (buffer) ? buffer->m_firstTimestamp+item->m_timeOffset : 0;
	return CACHE_ITEM_DATA_POINTER(item);
}

const CacheItem *Cache::getCurrentCacheItemInternal(const void *device, int id, CacheTS timestamp)
{
	CacheMap::iterator it = m_cache.find(device);
	CacheEntry *entry;
	CacheChannel *channel;
	CacheBuffer *buffer;
	CacheItem *item;

	if (it == m_cache.end()) {
		// device does not exist
		return NULL;
	}
	entry = it->second;
	if (id < 0 || id >= (int) entry->m_count || !entry->m_channelArray[id].m_busy)
		return NULL;
	channel = &entry->m_channelArray[id];
	if ((item = channel->findItemOrLater(timestamp,&buffer)) == NULL)
		return NULL;
	if (buffer && buffer->m_firstTimestamp+item->m_timeOffset != timestamp)
		return NULL;
	return item;
}

const void *Cache::getCurrentCacheItem(const void *device, int channel, unsigned int timestamp)
{
	const CacheItem *item = getCurrentCacheItemInternal(device, channel, timestamp);
	return (item) ? CACHE_ITEM_DATA_POINTER(item) : NULL;
}

double *Cache::addCacheVectorIfDifferent(const void *device, int channel, CacheTS timestamp, double *newdata, unsigned int length, double threshold)
{
	const CacheItem *item = getCurrentCacheItemInternal(device, channel, timestamp);
	unsigned int sizeW = CACHE_ITEM_SIZEW(item,length*sizeof(double));
	if (!item || item->m_sizeW != sizeW)
		return (double*)addCacheItem(device, channel, timestamp, newdata, length*sizeof(double));
	double *olddata = (double*)CACHE_ITEM_DATA_POINTER(item);
	if (!length)
		return olddata;
	double *ref = olddata;
	double *v = newdata;
	unsigned int i;
	for (i=length; i>0; --i) {
		if (fabs(*v-*ref) > threshold)
			break;
		*ref++ = *v++;
	}
	if (i) 
		olddata = (double*)addCacheItem(device, channel, timestamp, newdata, length*sizeof(double));
	return olddata;
}

}
