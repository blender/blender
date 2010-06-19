/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef GEN_MAP_H
#define GEN_MAP_H

template <class Key, class Value>
class GEN_Map {
private:
    struct Entry {
        Entry (Entry *next, Key key, Value value) :
            m_next(next),
            m_key(key),
            m_value(value) {}

        Entry *m_next;
        Key    m_key;
        Value  m_value;
    };
 
public:
    GEN_Map(int num_buckets = 100) : m_num_buckets(num_buckets) {
        m_buckets = new Entry *[num_buckets];
        for (int i = 0; i < num_buckets; ++i) {
            m_buckets[i] = 0;
        }
    }

	GEN_Map(const GEN_Map& map)
	{
		m_num_buckets = map.m_num_buckets;
		m_buckets = new Entry *[m_num_buckets];

		for (int i = 0; i < m_num_buckets; ++i) {
			m_buckets[i] = 0;

			for(Entry *entry = map.m_buckets[i]; entry; entry=entry->m_next)
				insert(entry->m_key, entry->m_value);
		}
	}
    
    int size() { 
        int count=0;
        for (int i=0;i<m_num_buckets;i++)
        {
            Entry* bucket = m_buckets[i];
            while(bucket)
            {
                bucket = bucket->m_next;
                count++;
            }
        }
        return count;
    }
    
    Value* at(int index) {
        int count=0;
        for (int i=0;i<m_num_buckets;i++)
        {
            Entry* bucket = m_buckets[i];
            while(bucket)
            {
                if (count==index)
                {
                    return &bucket->m_value;
                }
                bucket = bucket->m_next;
                count++;
            }
        }
        return 0;
    }

    Key* getKey(int index) {
        int count=0;
        for (int i=0;i<m_num_buckets;i++)
        {
            Entry* bucket = m_buckets[i];
            while(bucket)
            {
                if (count==index)
                {
                    return &bucket->m_key;
                }
                bucket = bucket->m_next;
                count++;
            }
        }
        return 0;
    }
    
    void clear() {
        for (int i = 0; i < m_num_buckets; ++i) {
            Entry *entry_ptr = m_buckets[i];
            
            while (entry_ptr != 0) {
                Entry *tmp_ptr = entry_ptr->m_next;
                delete entry_ptr;
                entry_ptr = tmp_ptr;
            }
            m_buckets[i] = 0;
        }
    }
    
    ~GEN_Map() {
        clear();
        delete [] m_buckets;
    }
    
    void insert(const Key& key, const Value& value) {
        Entry *entry_ptr = m_buckets[key.hash() % m_num_buckets];
        while ((entry_ptr != 0) && !(key == entry_ptr->m_key)) {
            entry_ptr = entry_ptr->m_next;
        }
        
        if (entry_ptr != 0) {
            entry_ptr->m_value = value;
        }
        else {
            Entry **bucket = &m_buckets[key.hash() % m_num_buckets];
            *bucket = new Entry(*bucket, key, value);
        }
    }

    void remove(const Key& key) {
        Entry **entry_ptr = &m_buckets[key.hash() % m_num_buckets];
        while ((*entry_ptr != 0) && !(key == (*entry_ptr)->m_key)) {
            entry_ptr = &(*entry_ptr)->m_next;
        }
        
        if (*entry_ptr != 0) {
            Entry *tmp_ptr = (*entry_ptr)->m_next;
            delete *entry_ptr;
            *entry_ptr = tmp_ptr;
        }
    }

    Value *operator[](Key key) {
        Entry *bucket = m_buckets[key.hash() % m_num_buckets];
        while ((bucket != 0) && !(key == bucket->m_key)) {
            bucket = bucket->m_next;
        }
        return bucket != 0 ? &bucket->m_value : 0;
    }
    
private:
    int     m_num_buckets;
    Entry **m_buckets;
};

#endif


