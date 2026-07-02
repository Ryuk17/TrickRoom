
#include "voice.h"
#include <winsock.h> 
#include "queue.h"
#include "sock.h"

queue_t snd_squeue;
queue_t snd_rqueue;

unsigned char snd_rbuffer[MIC_BUFFER_SIZE * 5];
unsigned char snd_sbuffer[MIC_BUFFER_SIZE * 5];


Voice::Voice()
{
}

int Voice::init(Audio &audio)
{

#ifdef VOICECHAT
#ifndef DEDICATED


	int ret;


	alGenSources(1, &mic_source);
	alGenBuffers(NUM_PONG, (unsigned int *)&mic_buffer[0]);
	alSourcei(mic_source, AL_SOURCE_RELATIVE, AL_TRUE);

	alGenSources(1, &decode_source);
	alGenBuffers(NUM_PONG, (unsigned int *)&decode_buffer[0]);
	alSourcei(decode_source, AL_SOURCE_RELATIVE, AL_TRUE);
#endif

	//OPUS_APPLICATION_AUDIO -- music
	//OPUS_APPLICATION_VOIP -- voice
	//OPUS_APPLICATION_RESTRICTED_LOWDELAY -- low delay voice
	encoder = opus_encoder_create(VOICE_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &ret);
	if (ret < 0)
	{
		printf("failed to create an encoder: %s\n", opus_strerror(ret));
		return -1;
	}


	int complexity = 10;

	ret = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity));
	if (ret < 0)
	{
		printf("failed to set complexity: %s\n", opus_strerror(ret));
		return -1;
	}

	/*
	ret = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
	if (ret < 0)
	{
	printf("failed to set bitrate: %s\n", opus_strerror(ret));
	return -1;
	}
	*/

	decoder = opus_decoder_create(VOICE_SAMPLE_RATE, 1, &ret);
	if (ret < 0)
	{
		printf("failed to create decoder: %s\n", opus_strerror(ret));
		return -1;
	}
#endif
	return 0;
}

int Voice::encode(unsigned short *pcm, unsigned int size, unsigned char *data, int &num_bytes)
{
	// Encode the frame.
	num_bytes = opus_encode(encoder, (opus_int16 *)pcm, (MIC_BUFFER_SIZE >> 1), data, MAX_PACKET_SIZE);
	if (num_bytes < 0)
	{
		printf("encode failed: %s\n", opus_strerror(num_bytes));
		return -1;
	}

	return 0;
}


int Voice::decode(unsigned char *data, int compressed_size, unsigned short *pcm, unsigned int max_size)
{
	int frame_size;

//	alBufferData(uiBuffer, AL_FORMAT_MONO16, mic_pcm[pong], pcm_size, VOICE_SAMPLE_RATE);

	frame_size = opus_decode(decoder, data, compressed_size, (opus_int16 *)pcm, (max_size), 0);
	if (frame_size < 0)
	{
		printf("decoder failed: %s\n", opus_strerror(frame_size));
		return -1;
	}

	return frame_size * 2;
}

int Voice::voice_send(Audio &audio, int sock, const char *ip, int port)
{
	static int pong = 0;
	static bool buffers_full = false; // first time around buffers are empty, different logic required
	int buffersProcessed = 0;
	bool local_echo = false;
	static voicemsg_t msg;
	int pcm_size;
	unsigned int uiBuffer;

	if (audio.microphone == NULL)
	{
		return 0;
	}


	if (buffers_full)
	{
		if (local_echo)
		{
			alGetSourcei(mic_source, AL_BUFFERS_PROCESSED, &buffersProcessed);
			if (buffersProcessed == 0)
			{
				ALenum state;

				alGetSourcei(mic_source, AL_SOURCE_STATE, &state);

				if (state == AL_STOPPED)
				{
					audio.play(mic_source);
				}
				return 0;
			}

			alSourceUnqueueBuffers(mic_source, 1, &uiBuffer);
			int al_err = alGetError();
			if (al_err != AL_NO_ERROR)
			{
				return 0;
			}
		}

		pcm_size = MIC_BUFFER_SIZE;
		audio.capture_sample(mic_pcm[pong], pcm_size);
		if (local_echo)
		{
			alBufferData(uiBuffer, AL_FORMAT_MONO16, mic_pcm[pong], pcm_size, VOICE_SAMPLE_RATE);
			alSourceQueueBuffers(mic_source, 1, &uiBuffer);
		}

	}
	else
	{
		pcm_size = MIC_BUFFER_SIZE;
		int ret = audio.capture_sample(mic_pcm[pong], pcm_size);
		if (ret == 0)
		{
			// microphone doesnt have enough data, take a break thread
			//Sleep(0);
		}

		if (local_echo)
		{
			alBufferData(mic_buffer[pong], AL_FORMAT_MONO16, mic_pcm[pong], pcm_size, VOICE_SAMPLE_RATE);
			int al_err = alGetError();
			if (al_err != AL_NO_ERROR)
			{
				printf("Error alBufferData\n");
			}
			alSourceQueueBuffers(mic_source, 1, &mic_buffer[pong]);
		}
	}

	if (pcm_size == 0)
	{
		return 0;
	}

	int num_bytes = 0;
	encode(mic_pcm[pong], pcm_size, msg.data, num_bytes);

	static int seq = 0;
	msg.magic = 1337;
	msg.sequence = seq++;
	msg.size = num_bytes + VOICE_HEADER;

	enqueue(&snd_squeue, (unsigned char *)&msg, msg.size);

	while (snd_squeue.size > 0 && sock != -1)
	{
		int dsize = dequeue_peek(&snd_squeue, &snd_sbuffer[0], VOICE_HEADER);
		voicemsg_t *head = (voicemsg_t *)&snd_sbuffer[0];
		if (head->magic == 1337)
		{
			dsize = dequeue(&snd_squeue, &snd_sbuffer[0], head->size);
		}
		else
		{
			// missing header, drop 4 bytes and try again
			dsize = dequeue(&snd_squeue, &snd_sbuffer[0], 4);
			continue;
		}

		int sent = socket_sendto(sock, (char *)&snd_sbuffer[0], dsize, ip, port);
		if (sent == -1)
		{
			int ret = WSAGetLastError();

			switch (ret)
			{
			case WSAEHOSTUNREACH:
				printf("Fatal Error: router sent ICMP packet (destination unreachable)\n");
				break;
			case WSAEWOULDBLOCK:
				printf("Would block, using select()\r\n");
				return 0;
			case WSAENETUNREACH:
				printf("network unreachable\r\n");
				break;
			default:
				printf("Fatal Error: %d\n", ret);
				break;
			}

		}

		if (sent > 0 && sent < dsize)
		{
			enqueue_front(&snd_squeue, &snd_sbuffer[sent], dsize - sent);
		}
	}


	pong++;
	if (pong >= NUM_PONG)
	{
		pong = 0;
		if (local_echo)
		{
			if (buffers_full == false)
			{
				buffers_full = true;
				audio.play(mic_source);
			}
		}
	}



	return 0;
}

int Voice::voice_recv(Audio &audio, int sock, const char *ip, int port)
{
	unsigned int pcm_size;
	int ret = 1;
	static int pong = 0;
	static bool buffers_full = false;
	int buffersProcessed = 0;
	unsigned int uiBuffer;
	bool remote_echo = true;

	static voicemsg_t msg;

	// recv any data on socket and add to rqueue
	do
	{
		char client[MAX_PATH];

		strncpy(client, ip, MAX_PATH - 1);
		ret = socket_recvfrom(sock, (char *)&snd_rbuffer[0], MIC_BUFFER_SIZE, client, port, MAX_PATH);
		if (ret > 0)
		{
			enqueue(&snd_rqueue, (unsigned char *)&snd_rbuffer[0], ret);

			if (snd_rqueue.size > 1024)
				break;
		}
		else if (ret == -1)
		{
			int err = WSAGetLastError();

			if (err != WSAEWOULDBLOCK)
			{
				printf("recv returned -1 error %d\r\n", err);
				return -1;
			}
		}
	} while (ret > 0);


	// check queue for a sound segment and add it to sound buffer chain
	while (snd_rqueue.size > 0)
	{
		dequeue_peek(&snd_rqueue, (unsigned char *)&msg, VOICE_HEADER);

		if (snd_rqueue.size >= msg.size && msg.magic == 1337)
		{
			dequeue(&snd_rqueue, (unsigned char *)&msg, msg.size);
			if (remote_echo)
			{
				// decode opus data

				pcm_size = decode(msg.data, msg.size - VOICE_HEADER, decode_pcm[pong], MIC_BUFFER_SIZE);

				if (msg.sequence < 100)
				{
					printf("PCM decode msg.size %d pcm_size %d sequence %d\r\n", msg.size, pcm_size, msg.sequence);
				}
				else if (msg.sequence == 100)
				{
					printf("Got 100 packets, probably working\r\n");
				}


				static unsigned int last_sequence = 0;
				if (msg.sequence < last_sequence)
				{
					printf("out of sequence %d < %d, dropping\r\n", msg.sequence, last_sequence);
					return 0;
				}
				last_sequence = msg.sequence;


				if (buffers_full)
				{
					alSourceUnqueueBuffers(decode_source, 1, &uiBuffer);
					alBufferData(uiBuffer, AL_FORMAT_MONO16, decode_pcm[pong], pcm_size, VOICE_SAMPLE_RATE);
					alSourceQueueBuffers(decode_source, 1, &uiBuffer);
				}
				else
				{
					alBufferData(decode_buffer[pong], AL_FORMAT_MONO16, decode_pcm[pong], pcm_size, VOICE_SAMPLE_RATE);
					alSourceQueueBuffers(decode_source, 1, &decode_buffer[pong]);
				}

				pong++;
				if (pong >= NUM_PONG)
				{
					pong = 0;

					if (buffers_full == false)
					{
						// now that we have some sound queued up we can start playing
						buffers_full = true;

						ALenum state;

						alGetSourcei(decode_source, AL_SOURCE_STATE, &state);

						if (state == AL_STOPPED || AL_INITIAL)
						{
							// if we stopped due to a data hiccup, start playing again
							if (remote_echo)
								audio.play(decode_source);
						}
					}
				}
			}
		}
		else
		{
			// Either bad data or not enough data
			if (msg.magic != 1337)
			{
				// unexpected data, throw away four bytes and try again
				dequeue(&snd_rqueue, (unsigned char *)&msg, 4);
				printf("Unexpected data, throwing away 4 bytes from rqueue\r\n");
			}
			else
			{
				break;
			}
		}
	}

	// Check if we have any queued sound buffers to play
	if (remote_echo)
	{

#ifndef DEDICATED
		if (buffers_full)
		{
			alGetSourcei(decode_source, AL_BUFFERS_PROCESSED, &buffersProcessed);
			if (buffersProcessed == 0)
			{
				ALenum state;

				alGetSourcei(decode_source, AL_SOURCE_STATE, &state);

				if (state == AL_STOPPED)
				{
					// if we stopped due to a data hiccup, start playing again
					audio.play(decode_source);
				}
				return 0;
			}
		}
#endif
	}

	return 0;
}



void Voice::destroy()
{
#ifdef VOICECHAT
	opus_encoder_destroy(encoder);
	opus_decoder_destroy(decoder);
#endif

}

