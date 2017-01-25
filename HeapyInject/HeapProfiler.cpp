#include "HeapProfiler.h"

#include <Windows.h>
#include <stdio.h>
#include "dbghelp.h"

#include <algorithm>
#include <iomanip>

StackTrace::StackTrace() : hash(0){
	memset(backtrace, 0, sizeof(void*)*backtraceSize);
}

void StackTrace::trace(){
	int framesCnt = CaptureStackBackTrace(0, backtraceSize, backtrace, 0);
	// Compute simple polynomial hash of the stack trace.
	// Note: CaptureStackBackTrace returns plain sum of all pointers as BackTraceHash.
	const size_t BASE = sizeof(size_t) > 4 ? 11400714819323198485ULL : 2654435769U;
	hash = 0;
	for (int i = 0; i < framesCnt; i++)
		hash = hash * BASE + (size_t)backtrace[i];
	//0 and -1 are special values in hash table
	if (hash == (StackHash)0 || hash == (StackHash)-1)
		hash = 0xDEAFBEEF;
}

void StackTrace::print(std::ostream &stream) const {
	HANDLE process = GetCurrentProcess();

	const int MAXSYMBOLNAME = 128 - sizeof(IMAGEHLP_SYMBOL);
	char symbol64_buf[sizeof(IMAGEHLP_SYMBOL) + MAXSYMBOLNAME] = {0};
	IMAGEHLP_SYMBOL *symbol = reinterpret_cast<IMAGEHLP_SYMBOL*>(symbol64_buf);
	symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
	symbol->MaxNameLength = MAXSYMBOLNAME - 1;

	// Print out stack trace. Skip the first frame (that's our hook function.)
	for(size_t i = 1; i < backtraceSize; ++i){ 
		if(backtrace[i]){
			// Output stack frame symbols if available.
			if(SymGetSymFromAddr(process, (DWORD64)backtrace[i], 0, symbol)){

				stream << "    " << symbol->Name;

				// Output filename + line info if available.
				IMAGEHLP_LINE lineSymbol = {0};
				lineSymbol.SizeOfStruct = sizeof(IMAGEHLP_LINE);
				DWORD displacement;
				if(SymGetLineFromAddr(process, (DWORD64)backtrace[i], &displacement, &lineSymbol)){
					stream << "    " << lineSymbol.FileName << ":" << lineSymbol.LineNumber;
				}
				

				stream << "    (" << std::setw(sizeof(void*)*2) << std::setfill('0') << backtrace[i] <<  ")\n";
			}else{
				stream << "    <no symbol> " << "    (" << std::setw(sizeof(void*)*2) << std::setfill('0') << backtrace[i] <<  ")\n";
			}
		}else{
			break;
		}
	}
}

HeapProfiler::HeapProfiler()
	: stackTraces((StackHash)0, (StackHash)-1)
	, ptrs(NULL, (void*)(size_t)-1)
{}

void HeapProfiler::malloc(void *ptr, size_t size, const StackTrace &trace){
	std::lock_guard<std::mutex> lk(mutex);

	if (ptrs.Find(ptr))
		return;   //two buffers at same address!

	// Locate or create this stacktrace in the allocations map.
	CallStackInfo newStackInfo = {trace, 0};
	auto pStack = stackTraces.Insert(trace.hash, newStackInfo, false);

	// Store the size for this allocation this stacktraces allocation map.
	pStack->value.totalSize += size;

	// Store the stracktrace hash of this allocation in the pointers map.
	PointerInfo ptrInfo = {trace.hash, size};
	ptrs.Insert(ptr, ptrInfo);
}

void HeapProfiler::free(void *ptr, const StackTrace &trace){
	std::lock_guard<std::mutex> lk(mutex);

	// On a free we remove the pointer from the ptrs map and the
	// allocating stack traces map.
	auto iter = ptrs.Find(ptr);
	if(iter){
		auto &stackInfo = stackTraces.Find(iter->value.stack)->value;
		stackInfo.totalSize -= iter->value.size;
		ptrs.Remove(iter);
	}else{
		// Do anything with wild pointer frees?
	}
}

void HeapProfiler::getAllocationSiteReport(std::vector<std::pair<StackTrace, size_t>> &allocs){
	std::lock_guard<std::mutex> lk(mutex);
	allocs.clear();

	stackTraces.ForEach([&allocs](StackHash key, const CallStackInfo &value) {
		allocs.push_back(std::make_pair(value.trace, value.totalSize));
	});
}

void HeapProfiler::printStats(std::ostream &stream) {
	stream << "Number of all allocating stack traces: " << stackTraces.Size() << std::endl;
	stream << "Number of currently allocated pointers: " << ptrs.Size() << std::endl;
	stream << "Sizes of hash tables: " << stackTraces.MemSize() / double(1<<20) << " MB   and   " << ptrs.MemSize() / double(1<<20) << " MB" << std::endl;
}
