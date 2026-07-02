#include "queue.h"

int enqueue(queue_t *queue, unsigned char *buffer, int size)
{
	int        i;

	if (queue->size == SIZE_QUEUE)
	{
		return 0;
	}

	for (i = 0; i < size && queue->size != SIZE_QUEUE; i++)
	{
		queue->buffer[queue->tail++] = buffer[i];
		queue->size++;
		if (queue->tail == SIZE_QUEUE)
			queue->tail = 0;
	}
	return i;
}

int enqueue_front(queue_t *queue, unsigned char *buffer, int size)
{
	int        i;

	if (queue->size == SIZE_QUEUE)
		return 0;

	for (i = 0; i < size && queue->size != SIZE_QUEUE; i++)
	{
		if (queue->head == 0)
		{
			queue->head = SIZE_QUEUE;
		}
		queue->buffer[--queue->head] = buffer[(size - 1) - i];
		queue->size++;
	}
	return i;
}


int dequeue(queue_t *queue, unsigned char *buffer, int size)
{
	int        i;

	for (i = 0; i < size && queue->size != 0; i++)
	{
		buffer[i] = queue->buffer[queue->head++];
		//queue->buffer[queue->head - 1] = ' '; // empty space for debugging
		queue->size--;
		if (queue->head == SIZE_QUEUE)
		{
			queue->head = 0;
		}
	}
	return i;
}

int dequeue_peek(queue_t *queue, unsigned char *buffer, int size)
{
	int        i;
	int		head = queue->head;
	int		qsize = queue->size;

	for (i = 0; i < size && qsize != 0; i++)
	{
		buffer[i] = queue->buffer[head++];
		//queue->buffer[queue->head - 1] = ' '; // empty space for debugging
		qsize--;
		if (head == SIZE_QUEUE)
		{
			head = 0;
		}
	}
	return i;
}
