#include "ThreadCache.h"
#include "CentralCache.h"
#include "PageCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t byte_size)
{
	FreeList* list = &_freeList[index];
	assert(list->Empty());

	// 从中心Cache获取一定数量的对象，插入到当前的ThreadCahche,返回第一个对象
	// 每次获取的数量是不断变化的，开始很小，后续慢慢增大，类似网络中拥塞控制的慢启动算法
	// 增长到num_to_move就不再增长了
	size_t num_to_move = SizeClass::NumMoveSize(byte_size);
	size_t batch_size = std::min<int>(num_to_move, list->MaxSize());
	void *start, *end;
	int fetch_count = CentralCache::GetInstance()->FetchRangeObj(start, end, batch_size, byte_size);
	assert(fetch_count);

	if (fetch_count > 1)
	{
		list->PushRange(NEXT_OBJ(start), end, fetch_count - 1);
	}

	// 每次一批内存对象用完了，maxsize+1，下次批量多获取一个对象
	if (list->MaxSize() < num_to_move)
	{
		list->SetMaxSize(list->MaxSize() + 1);
	}

	return start;
}

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);

	size = SizeClass::RoundUp(size);

	//1.当前线程堆的自由链表中有，直接在自由链表中取
	//2.没有则到中心堆的自由链表中取
	size_t index = SizeClass::Index(size);
	FreeList* list = &_freeList[index];
	if (list->Empty()) {
		return FetchFromCentralCache(index, size);
	}
	else {
		return list->Pop();
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	size_t index = SizeClass::Index(size);
	FreeList* list = &_freeList[index];
	list->Push(ptr);

	if (list->Size() > list->MaxSize())
	{
		ListTooLong(list, size);
	}
}

void ThreadCache::ListTooLong(FreeList* list, size_t size)
{
	//size_t num_to_move = SizeClass::NumMoveSize(size);
	//while (list->Size() > num_to_move)
	//{
	//	void* start = nullptr, *end = nullptr;
	//	list->PopRange(start, end, num_to_move);
	//	CentralCache::GetInstance()->ReleaseListToSpans(start, end, size);
	//}

	size_t num_to_move = list->Size();
	void* start, *end;
	list->PopRange(start, end, num_to_move);
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}