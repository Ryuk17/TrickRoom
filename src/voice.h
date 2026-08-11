
#include <vector>
#include "audio.h"
#include <opus.h>

/* Opus codec + playback rate: SRC output of the AEC→NS→AGC2→SRC pipeline.
   (VOICE_SAMPLE_RATE in audio.h stays 48kHz: mic capture + pipeline rate) */
#define VOICE_ENCODE_SAMPLE_RATE 16000

#ifndef VOICE_H
#define VOICE_H
#define VOICECHAT





typedef struct
{
	unsigned int magic;
	unsigned int size;
	unsigned int sequence;
	unsigned char data[MAX_PACKET_SIZE];
} voicemsg_t;

typedef struct
{
	char			socketname[32];
	unsigned int	qport;
	int				ent_id;
	unsigned short int	client_sequence;
	unsigned short int	server_sequence;
	unsigned int	last_time;
//	input_t			input;
//	netinfo_t		netinfo;
	bool			needs_state;
//	vec3			position_delta;
} client_t;


class Voice
{
public:
	Voice();
	int init(Audio &audio);
	void destroy();
	int encode(unsigned short *pcm, unsigned int size, unsigned char *data, int &num_bytes);
	int decode(unsigned char *data, int compressed_size, unsigned short *pcm, unsigned int max_size);
	int voice_send(Audio &audio, int sock, const char *ip, int port);
	int voice_recv(Audio &audio, int sock, const char *ip, int port);

	char server[128];
private:
#ifdef VOICECHAT
	OpusEncoder *encoder;
	OpusDecoder *decoder;
#endif
	
//	Socket		sock;
	unsigned short qport;
	unsigned short int		voice_send_sequence;
	unsigned short int		voice_recv_sequence;


#define NUM_PONG 8
	unsigned int mic_buffer[NUM_PONG];
	unsigned int mic_source;
	unsigned short mic_pcm[NUM_PONG][MIC_BUFFER_SIZE];
	unsigned short proc_pcm[NUM_PONG][MIC_BUFFER_SIZE / 3];	/* pipeline out @16k */

	unsigned int decode_buffer[NUM_PONG];
	unsigned short decode_pcm[NUM_PONG][MIC_BUFFER_SIZE];
	unsigned int decode_source;

};

#endif
