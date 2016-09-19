/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "CodeCacheTypes.hpp"
#include "runtime/CodeCacheManager.hpp"        // for TR::CodeCacheManager

namespace OMR
{

void
CodeCacheHashTable::dumpHashUnresolvedMethod(void)
   {
   // compute the hash key for name/classLoader
   CodeCacheHashEntry *entry;
   mcc_printf("=======================================");
   mcc_printf("");
   mcc_printf("\n");
   for (int32_t i=0; i < _size; i++)
      {
      entry = _buckets[i];
      if (entry)
         mcc_printf("index = %d, constPool = 0x%x  cpIndex = %d\n", i, entry->_info._unresolved._constPool,entry->_info._unresolved._constPoolIndex);
      }
   return;
   }

// Allocate a trampoline hash table
//
CodeCacheHashTable *
CodeCacheHashTable::allocate(TR::CodeCacheManager *manager)
   {
   CodeCacheHashTable *hashTable = static_cast<CodeCacheHashTable *>(manager->getMemory(sizeof(CodeCacheHashTable)));
   if (!hashTable)
      return NULL;

   new (hashTable) CodeCacheHashTable();

   TR::CodeCacheConfig & config = manager->codeCacheConfig();
   hashTable->_size = std::max<size_t>(config.codeCacheKB()*2/3, 1);
   hashTable->_buckets = static_cast<CodeCacheHashEntry**>(manager->getMemory(sizeof(CodeCacheHashEntry *) * hashTable->_size));
   if (!hashTable->_buckets)
      {
      manager->freeMemory(hashTable);
      return NULL;
      }

   // reset all the buckets
   for (int32_t i = 0; i < hashTable->_size; i++)
      hashTable->_buckets[i] = NULL;

   return hashTable;
   }

// Allocate and partition a slab of Hash Entries
//
// Use a slab of partitioned entries to prevent fragmentation. Eventually the
// entries will have to be released back onto the slab free list on method
// unloads during the reclamation process.
//
CodeCacheHashEntrySlab *
CodeCacheHashEntrySlab::allocate(TR::CodeCacheManager *manager, size_t slabSize)
   {
   CodeCacheHashEntrySlab *slab = static_cast<CodeCacheHashEntrySlab *>(manager->getMemory(sizeof(CodeCacheHashEntrySlab)));
   if (!slab)
      return NULL;

   new (slab) CodeCacheHashEntrySlab;

   slab->_segment = static_cast<uint8_t *>(manager->getMemory(slabSize));
   if (!slab->_segment)
      {
      manager->freeMemory(slab);
      return NULL;
      }

   slab->_heapAlloc = slab->_segment;
   slab->_heapTop = slab->_segment + slabSize;
   slab->_next = NULL;

   return slab;
   }

// Free a slab of Hash Entries
//
void
CodeCacheHashEntrySlab::free(TR::CodeCacheManager *manager)
   {
   if (_segment)
      manager->freeMemory(_segment);

   manager->freeMemory(this);
   }

// Hash functions for unresolved and resolved methods
//
CodeCacheHashKey
CodeCacheHashTable::hashUnresolvedMethod(void *constPool, int32_t constPoolIndex)
   {
   return ((CodeCacheHashKey) constPool)  * constPoolIndex;
   }


CodeCacheHashKey
CodeCacheHashTable::hashResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   return (CodeCacheHashKey) method;
   }

// Find an existing unresolved method in the hash table
//
CodeCacheHashEntry *
CodeCacheHashTable::findUnresolvedMethod(void *constPool, int32_t constPoolIndex)
   {
   CodeCacheHashKey key = hashUnresolvedMethod(constPool, constPoolIndex);
   CodeCacheHashEntry *entry;
   for (entry = _buckets[key % _size]; entry; entry = entry->_next)
      {
      if (entry->_info._unresolved._constPool == constPool &&
          entry->_info._unresolved._constPoolIndex == constPoolIndex)
         return entry;
      }
   return NULL;
   }

// Find an existing resolved method in the hash table
//
CodeCacheHashEntry *
CodeCacheHashTable::findResolvedMethod(TR_OpaqueMethodBlock *method)
   {
   // compute the hash key for name/classLoader
   CodeCacheHashKey key = hashResolvedMethod(method);
   CodeCacheHashEntry *entry;
   for (entry = _buckets[key % _size]; entry; entry = entry->_next)
      {
      if (entry->_info._resolved._method == method)
         return entry;
      }
   return NULL;
   }

// Add an entry to the hash table
//
void
CodeCacheHashTable::add(CodeCacheHashEntry *entry)
   {
   size_t bucket = entry->_key % _size;
   entry->_next = _buckets[bucket];
   _buckets[bucket] = entry;
   }


// Remove an entry from the hash table
//
bool
CodeCacheHashTable::remove(CodeCacheHashEntry *entry)
   {
   CodeCacheHashEntry **prev;
   for (prev = _buckets + (entry->_key % _size); *prev; prev = &((*prev)->_next))
      {
      if (*prev == entry)
         {
         *prev = entry->_next;
         entry->_next = NULL;
         return true;
         }
      }
   return false;
   }

}