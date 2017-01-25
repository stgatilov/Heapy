#pragma once

#include <memory>

//Note: both Key and Value must be POD types!
//Their constructors, destructors, and copying operators are not called correctly.
template<class Key, class Value, class HashFunc = std::hash<Key>>
class HashTable {
	struct Entry {
		Key key;
		Value value;
	};

	Key emptyKey, removedKey;
	HashFunc hfunc;
	size_t cellsCnt, cellsMask;
	size_t elementsCnt, usedCnt;
	Entry *cells;

	void Allocate(size_t cnt) {
		cellsCnt = cnt, cellsMask = cnt-1;
		cells = (Entry*)malloc(cellsCnt * sizeof(Entry));
		for (size_t i = 0; i < cellsCnt; i++)
			cells[i].key = emptyKey;
	}
	void HandleOverflow() {
		Entry *oldCells = cells;
		size_t oldSize = cellsCnt;
		//do not increase table size unless number of alive elements is above 50% of its size
		size_t newSize = (elementsCnt > cellsCnt / 2 ? cellsCnt * 2 : cellsCnt);
		Allocate(newSize);
		usedCnt = elementsCnt = 0;
		for (size_t i = 0; i < oldSize; i++) {
			Key key = oldCells[i].key;
			if (key != emptyKey && key != removedKey)
				Insert(key, oldCells[i].value);
		}
		free(oldCells);
	}

public:
	HashTable(Key emptyKey, Key removedKey, HashFunc &hfunc = HashFunc())
		: emptyKey(emptyKey), removedKey(removedKey), hfunc(hfunc)
		, elementsCnt(0), usedCnt(0)
	{
		Allocate(16);
	}
	~HashTable() {
		free(cells);
	}

	size_t Size() const { return elementsCnt; }
	size_t MemSize() const { return cellsCnt * sizeof(Entry); }
	Entry *Find(Key key) const {
		ptrdiff_t i = hfunc(key) & cellsMask;
		for (; cells[i].key != emptyKey; i = (i+1) & cellsMask)
			if (cells[i].key == key)
				return &cells[i];
		return NULL;
	}
	Entry *Insert(Key key, const Value &value, bool overwrite = true) {
		//clear or reallocate table if exceeded 87.5% max load factor
		if (usedCnt + 1 > cellsCnt - cellsCnt / 8)
			HandleOverflow();
		ptrdiff_t i = hfunc(key) & cellsMask, removed = -1;
		for (; cells[i].key != emptyKey; i = (i+1) & cellsMask) {
			if (cells[i].key == key) {
				if (overwrite) cells[i].value = value;
				return &cells[i];
			}
			if (cells[i].key == removedKey && removed == -1)
				removed = i;
		}
		if (removed >= 0) {
			i = removed;
			usedCnt--;
		}
		elementsCnt++;
		usedCnt++;
		cells[i].key = key;
		cells[i].value = value;
		return &cells[i];
	}
	void Remove(Entry *entry) {
		entry->key = removedKey;
		elementsCnt--;
	}
	Entry *Remove(Key key) {
		Entry *ptr = Find(key);
		if (ptr) Remove(ptr);
	}
	template<class Lambda> void ForEach(Lambda &lambda) const {
		for (size_t i = 0; i < cellsCnt; i++)
			if (cells[i].key != emptyKey && cells[i].key != removedKey)
				lambda(cells[i].key, cells[i].value);
	}
};
