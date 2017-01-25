#pragma once
#include <ostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <mutex>
#include "HashTable.h"

const int backtraceSize = 64;
typedef size_t StackHash;

struct StackTrace{
	void *backtrace[backtraceSize];
	StackHash hash;

	StackTrace();
	void trace(); 
	void print(std::ostream &stream) const;
};

class HeapProfiler{
public:
	HeapProfiler();
	void malloc(void *ptr, size_t size, const StackTrace &trace);
	void free(void *ptr, const StackTrace &trace);

	// Return a list of allocation sites (a particular stack trace) and the amount
	// of memory currently allocated by each site.
	void getAllocationSiteReport(std::vector<std::pair<StackTrace, size_t>> &allocs);
	void printStats(std::ostream &stream);
private:
	std::mutex mutex;
	struct CallStackInfo {
		StackTrace trace;
		size_t totalSize;
	};
	struct PointerInfo {
		StackHash stack;
		size_t size;
	};

	HashTable<StackHash, CallStackInfo> stackTraces;
	HashTable<void*, PointerInfo> ptrs;
};
