#include "ThreadCache.h"
#include "CentralCache.h"
#include "PageCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t byte_size)
{
	FreeList* list = &_freeList[index];
	assert(list->Empty());

	// ������Cache��ȡһ�������Ķ��󣬲��뵽��ǰ��ThreadCahche,���ص�һ������
	// ÿ�λ�ȡ�������ǲ��ϱ仯�ģ���ʼ��С������������������������ӵ�����Ƶ��������㷨
	// ������num_to_move�Ͳ���������
	size_t num_to_move = SizeClass::NumMoveSize(byte_size);
	size_t batch_size = std::min<int>(num_to_move, list->MaxSize());
	void *start, *end;
	int fetch_count = CentralCache::GetInstance()->FetchRangeObj(start, end, batch_size, byte_size);
	assert(fetch_count);

	if (fetch_count > 1)
	{
		list->PushRange(NEXT_OBJ(start), end, fetch_count - 1);
	}

	// ÿ��һ���ڴ���������ˣ�maxsize+1���´��������ȡһ������
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

	//1.��ǰ�̶߳ѵ������������У�ֱ��������������ȡ
	//2.û�������Ķѵ�����������ȡ
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