#ifndef QUEUE_H
#define QUEUE_H
#define SIZE_QUEUE (1000 * 1024) // 1MB

typedef struct
{
	unsigned int size;
	unsigned int tail;
	unsigned int head;
	char buffer[SIZE_QUEUE];
} queue_t;

int enqueue(queue_t *queue, unsigned char *buffer, int size);
int dequeue(queue_t *queue, unsigned char *buffer, int size);
int enqueue_front(queue_t *queue, unsigned char *buffer, int size);
int dequeue_peek(queue_t *queue, unsigned char *buffer, int size);
#endif