#ifndef ZOOMY_H
#define ZOOMY_H

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <vfw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "queue.h"
#include "sock.h"

#include "audio.h"
#include "voice.h"


// WebCam resolution
#define WIDTH 320
#define HEIGHT 240
#define FRAME_SIZE (WIDTH * HEIGHT * 4)


#define FRAME_PACKET_SIZE (FRAME_SIZE / 2 + sizeof(header_t))

// Scale it up for display
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 480

//RGB macro, but backwads BGR for yuy2 conversion
#define RGB2(b,g,r)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))


class Zoomy
{
public:
	Zoomy();
	void init(void *param1, void *param2);
	void capture();
	void resize(int width, int height);
	void step();
	void keydown(int key);
	void paint(HDC hdc);
	void destroy();

	// Normally these classes attempt to be operating system agnostic... but not today
	static DWORD WINAPI VoiceThread(LPVOID lpParam);
	static LRESULT frameCallback(HWND hWnd, LPVIDEOHDR lpVHdr);


	// Used by callback (only has hwnd and data pointer, might reference by hwnd somehow later)
	static int connect_sframe_sock;
	static int enable_video;
	static bool enable_voice;
	static bool mute_microphone;


private:

	// bitmap functions
	void draw_pixels(HDC hdc, int xoff, int yoff, int width, int height, int scalew, int scaleh, unsigned char *data);
	void yuy2_to_rgb(unsigned char *yuvData, COLORREF *data);

	// TCP socket functions
	int listen_socket(int &sock, unsigned short port);
	void read_socket(int &csock, char *buffer, int &size);
	int connect_socket(char *ip_addr, unsigned short port, int &sock);
	int set_sock_options(int sock);
	void handle_listen(int &sock, int &csock, char *ipstr);



	bool initialized;		// flag set after first WM_SIZE to ensure we are capturing video
	int server_sock;		// listen socket
	int client_rframe_sock;	// client socket from listen

	client_state_t connect_state; // state flags independent of socks
	client_state_t client_state;  // state flags independent of socks
	int listen_port;			  // Port to listen on (.ini configurable)
	int connect_port;			  // Port to connect to (.ini configurable)
	char connect_ip[512];		  // IP to connect to
	char listen_ip[512];	      // IP to listen on (really can be 127.0.0.1) ini configurable
	char client_ip[512];		  // IP of a client that connets to our listen port

	// again usually class is operating system agnostic
	HWND hwnd;
	HWND camhwnd;
	RECT client_area;
};

#endif