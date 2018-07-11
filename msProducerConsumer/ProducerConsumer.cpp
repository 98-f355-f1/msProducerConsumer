#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFFER_SIZE 10
#define PRODUCER_SLEEP_TIME_MS 1000
#define CONSUMER_SLEEP_TIME_MS 2000

LONG Buffer[BUFFER_SIZE];
LONG LastItemProduced;
ULONG QueueSize;
ULONG QueueStartOffset;

ULONG TotalItemsProduced;
ULONG TotalItemsConsumed;

CONDITION_VARIABLE BufferNotEmpty;
CONDITION_VARIABLE BufferNotFull;
CRITICAL_SECTION BufferLock;

BOOL StopRequested;

DWORD WINAPI ProducerThreadProc(PVOID p)
{
	ULONG ProducerId = (ULONG)(ULONG_PTR)p;
	DWORD dwMilliSec = PRODUCER_SLEEP_TIME_MS;

	while (true)
	{
		// Produce a new item
		Sleep(rand() % PRODUCER_SLEEP_TIME_MS);
		ULONG Item = InterlockedIncrement(&LastItemProduced);

		EnterCriticalSection(&BufferLock);

		while (QueueSize == BUFFER_SIZE && StopRequested == FALSE)
		{
			// Buffer is full - sleep so consumers can get items
			SleepConditionVariableCS(&BufferNotFull, &BufferLock, dwMilliSec);
		}

		if (StopRequested == TRUE)
		{
			LeaveCriticalSection(&BufferLock);
			break;
		}

		// Insert the item at the end of the queue and increment size
		// wraps around when at the end
		Buffer[(QueueStartOffset + QueueSize) % BUFFER_SIZE] = Item;
		QueueSize++;
		TotalItemsProduced++;

		printf("Producer %u: item %2d, queue size %2u\r\n", ProducerId, Item, QueueSize);

		LeaveCriticalSection(&BufferLock);

		// If a consumer is waiting, wake it
		WakeConditionVariable(&BufferNotEmpty);
	}

	printf("Producer %u exiting\r\n", ProducerId);
	return 0;
}

DWORD WINAPI ConsumerThreadProc(PVOID p)
{
	ULONG ConsumerId = (ULONG)(ULONG_PTR)p;
	DWORD dwMilliSec = CONSUMER_SLEEP_TIME_MS;

	while (true)
	{
		EnterCriticalSection(&BufferLock);

		while (QueueSize == 0 && StopRequested == FALSE)
		{
			// Buffer is empty - sleep so producers can create items
			SleepConditionVariableCS(&BufferNotEmpty, &BufferLock, dwMilliSec);
		}

		if (StopRequested == TRUE)
		{
			LeaveCriticalSection(&BufferLock);
			break;
		}

		// Consume the first available item

		LONG Item = Buffer[QueueStartOffset];

		QueueSize--;
		QueueStartOffset++;
		TotalItemsConsumed++;

		if (QueueStartOffset == BUFFER_SIZE)
		{
			QueueStartOffset = 0;
		}

		printf("Consumer %u: item %2d, queue size %2u\r\n", ConsumerId, Item, QueueSize);

		LeaveCriticalSection(&BufferLock);

		// If a producer is waiting, wake it up
		WakeConditionVariable(&BufferNotFull);

		// Simulate processing of the item
		Sleep(rand() % CONSUMER_SLEEP_TIME_MS);

		printf("Consumer %u exiting\r\n", ConsumerId);
	}
	return 0;
}

int main(void)
{
	InitializeConditionVariable(&BufferNotFull);
	InitializeConditionVariable(&BufferNotEmpty);

	InitializeCriticalSection(&BufferLock);

	HANDLE hProducer;
	HANDLE hConsumer;
	DWORD Id;
	PVOID x;

	for (UINT64 i = 1; i < 3; i++) {
		x = (PVOID)(UINT64)i;
		// Substituted (PVOID)number variable for "for" loop
		hProducer = CreateThread(NULL, 0, ProducerThreadProc, x, 0, &Id);
	}

	for (UINT i = 1; i < 6; i++) {
		x = (PVOID)(UINT64)i;
		// Substituted (PVOID)number variable for "for" loop
		hConsumer = CreateThread(NULL, 0, ConsumerThreadProc, x, 0, &Id);
	}

	puts("Press enter to stop...");
	getchar();

	EnterCriticalSection(&BufferLock);
	StopRequested = TRUE;
	LeaveCriticalSection(&BufferLock);

	WakeAllConditionVariable(&BufferNotFull);
	WakeAllConditionVariable(&BufferNotEmpty);

	for (UINT i = 1; i < 3; i++)
		WaitForSingleObject(hProducer, INFINITE);

	for (UINT i = 1; i < 6; i++)
		WaitForSingleObject(hConsumer, INFINITE);

	printf("TotalItemsProduced: %u, TotalItemsConsumed: %u\r\n", TotalItemsProduced, TotalItemsConsumed);

	return 0;
}