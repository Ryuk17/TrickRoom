#include "zoomy.h"


int Zoomy::connect_sframe_sock = -1;
int Zoomy::enable_video = 1;
bool Zoomy::enable_voice = false;
bool Zoomy::mute_microphone = false;

// globals (Large buffers cant be on stack)
queue_t squeue;
queue_t rqueue;

unsigned char blue_check[FRAME_SIZE];
unsigned char red_check[FRAME_SIZE];
unsigned char grey_check[FRAME_SIZE];


unsigned char rbuffer[FRAME_SIZE * 10];
unsigned char sbuffer[FRAME_SIZE * 10];

unsigned char recv_image[FRAME_SIZE];
unsigned char cap_image[FRAME_SIZE];
unsigned char cap_image_last[FRAME_SIZE];



Zoomy::Zoomy()
{
	initialized = false;
	server_sock = -1;	// listen socket
	client_rframe_sock = -1;	// client socket from listen

	connect_state = DISCONNECTED;
	client_state = DISCONNECTED;
	listen_port = 65535;
	connect_port = 65534;

	sprintf(connect_ip, "127.0.0.1");
	sprintf(listen_ip, "127.0.0.1");
	sprintf(client_ip, "");
}

void Zoomy::init(void *param1, void *param2)
{
	hwnd = (HWND)param1;

	GetClientRect(hwnd, &client_area);


	char path[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\zoomy.ini"));

	listen_port = GetPrivateProfileInt(TEXT("zoomy"), TEXT("listen"), 65535, path);
	connect_port = GetPrivateProfileInt(TEXT("zoomy"), TEXT("connect"), 65534, path);
	GetPrivateProfileString(TEXT("zoomy"), TEXT("ip"), "127.0.0.1", connect_ip, MAX_PATH, path);
	GetPrivateProfileString(TEXT("zoomy"), TEXT("localip"), "127.0.0.1", listen_ip, MAX_PATH, path);
	enable_video = GetPrivateProfileInt(TEXT("zoomy"), TEXT("capture"), 1, path);


	// Create a checkerboard pattern
	unsigned int *ipixel = (unsigned int *)blue_check;
	for (int i = 0; i < WIDTH; i++)
	{
		for (int j = 0; j < HEIGHT; j++)
		{
			unsigned char c = (((i & 0x8) == 0) ^ ((j & 0x8) == 0)) * 255;
			ipixel[i + j * WIDTH + 0] = c;
		}
	}


	ipixel = (unsigned int *)red_check;
	for (int i = 0; i < WIDTH; i++)
	{
		for (int j = 0; j < HEIGHT; j++)
		{
			unsigned char c = (((i & 0x8) == 0) ^ ((j & 0x8) == 0)) * 255;
			ipixel[i + j * WIDTH + 0] = RGB(0, 0, c); // RGB is backwards so red is blue etc
		}
	}

	ipixel = (unsigned int *)grey_check;
	for (int i = 0; i < WIDTH; i++)
	{
		for (int j = 0; j < HEIGHT; j++)
		{
			unsigned char c = (((i & 0x8) == 0) ^ ((j & 0x8) == 0)) * 255;
			ipixel[i + j * WIDTH + 0] = RGB(c / 4, c / 4, c / 4);
		}
	}

	memcpy(cap_image, grey_check, FRAME_SIZE);
	memcpy(recv_image, grey_check, FRAME_SIZE);

	if (enable_video)
	{
		printf("Starting Camera\r\n");
		camhwnd = capCreateCaptureWindow("camera window", WS_CHILD | WS_VISIBLE, WIDTH, HEIGHT, WIDTH, HEIGHT, hwnd, 0);
	}
	else
	{
		printf("*** Camera not enabled ***\r\n");
		memcpy(cap_image, blue_check, FRAME_SIZE);
	}


	listen_socket(server_sock, listen_port);
	set_sock_options(server_sock);



}

void Zoomy::capture()
{
	int rsize = 0;

	if (initialized == false)
	{
		return;
	}

	if (client_rframe_sock == SOCKET_ERROR)
	{
		handle_listen(server_sock, client_rframe_sock, client_ip);
	}
	else
	{
		read_socket(client_rframe_sock, (char *)rbuffer, rsize);
		enqueue(&rqueue, rbuffer, rsize);
	}

	while (rqueue.size >= FRAME_PACKET_SIZE)
	{
		dequeue_peek(&rqueue, rbuffer, sizeof(header_t));

		header_t *header = (header_t *)rbuffer;

		if (header->magic == 0xDEAFB4B3 && header->size == FRAME_SIZE / 2)
		{
			dequeue(&rqueue, rbuffer, FRAME_PACKET_SIZE);
			yuy2_to_rgb(&rbuffer[sizeof(header_t)], (COLORREF *)recv_image);
			InvalidateRect(hwnd, &client_area, FALSE);
			Zoomy::enable_voice = true;
		}
		else
		{
			// drop 4 bytes and try again
			dequeue(&rqueue, rbuffer, 4);
			continue;
		}
	}

	while (squeue.size >= FRAME_PACKET_SIZE &&
		connect_sframe_sock != SOCKET_ERROR &&
		connect_state == CONNECTED)
	{
		dequeue(&squeue, sbuffer, FRAME_PACKET_SIZE);
		int ret = send(connect_sframe_sock, (char *)sbuffer, FRAME_PACKET_SIZE, 0);
		if (ret == -1)
		{
			int err = WSAGetLastError();

			switch (err)
			{
			case WSAEWOULDBLOCK:
				break;
			case WSAECONNRESET:
				connect_state = DISCONNECTED;
				connect_sframe_sock = -1;
				break;
			case WSAENOTSOCK:
				connect_state = DISCONNECTED;
				connect_sframe_sock = -1;
				break;
			default:
				printf("send returned -1 error %d\r\n", err);
				connect_state = DISCONNECTED;
				break;
			}
			break;
		}
		else if (ret > 0 && ret < FRAME_PACKET_SIZE)
		{
			// partial send occurred (full buffer?)
			// better to just drop the frame
//			enqueue_front(&squeue, &sbuffer[ret], FRAME_SIZE / 2 - ret);
		}


	}

	// Old way of getting frame RGB from window
	// Using RAW YUY2 means half the size using callback
#if 0
	if (sock != SOCKET_ERROR && capture)
	{
		getBitmapFromWindow(camhwnd, 0, 0, WIDTH, HEIGHT, cap_image);
		// prevent duplicate frames
		if (memcmp(cap_image, cap_image_last, FRAME_SIZE) != 0)
		{
			enqueue(&squeue, cap_image, FRAME_SIZE);
			memcpy(cap_image_last, cap_image, FRAME_SIZE);
		}
		else
		{
			printf("Duplicate frame\r\n");
		}
	}
#endif

	if (connect_sframe_sock != SOCKET_ERROR && enable_video == 0)
	{
		static int i = 0;
		// send checkerbox

		if (i % 500 == 0)
		{
			printf("Sending checkbox %d\r\n", i++);

			header_t header;

			header.magic = 0xDEAFB4B3;
			header.size = FRAME_SIZE / 2;
			enqueue(&squeue, (unsigned char *)&header, sizeof(header_t));
			enqueue(&squeue, cap_image, FRAME_SIZE / 2);
		}
	}

	Sleep(0);
}

void Zoomy::step()
{
	// Dont attempt anything until video is streaming
	if (initialized == false)
		return;

	// Ensure we update textout
	InvalidateRect(hwnd, &client_area, FALSE);

	if (connect_sframe_sock == -1 || connect_state == DISCONNECTED)
	{
		int ret = connect_socket(connect_ip, connect_port, connect_sframe_sock);
		if (ret == 0)
		{
			connect_state = CONNECTED;
		}
		else
		{
			//				memset(&rqueue, 0, sizeof(queue_t));
			connect_state = DISCONNECTED;
		}
	}

	if (client_rframe_sock == SOCKET_ERROR)
	{
		// add check box to rqueue to show on remote side, note redish hue now to YUYV format
		//enqueue(&rqueue, red_check, FRAME_SIZE);
		client_state = DISCONNECTED;
	}
	else
	{
		client_state = CONNECTED;
	}

}

void Zoomy::resize(int width, int height)
{

	static bool once = false;

	if (enable_video && once == false)
	{
		once = true;
		// setup frame rate
		//			CAPTUREPARMS CaptureParms;
		//			float FramesPerSec = 30.0; // 30 frames per second

		//			capCaptureGetSetup(camhwnd, &CaptureParms, sizeof(CAPTUREPARMS));
		//			CaptureParms.dwRequestMicroSecPerFrame = (DWORD)(1.0e6 / FramesPerSec);
		//			capCaptureSetSetup(camhwnd, &CaptureParms, sizeof(CAPTUREPARMS));



		CAPDRIVERCAPS CapDrvCaps;

		capDriverGetCaps(camhwnd, &CapDrvCaps, sizeof(CAPDRIVERCAPS));

		if (CapDrvCaps.fHasOverlay)
			capOverlay(camhwnd, TRUE); //for speedup

		SetWindowPos(camhwnd, 0, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
		capSetCallbackOnFrame(camhwnd, frameCallback);
		SendMessage(camhwnd, WM_CAP_DRIVER_CONNECT, 0, 0);


		// setup resolution
		BITMAPINFO psVideoFormat = { 0 };

		capGetVideoFormat(camhwnd, &psVideoFormat, sizeof(psVideoFormat));
		psVideoFormat.bmiHeader.biWidth = WIDTH;
		psVideoFormat.bmiHeader.biHeight = HEIGHT;
		psVideoFormat.bmiHeader.biSizeImage = WIDTH * HEIGHT * 2;
		capSetVideoFormat(camhwnd, &psVideoFormat, sizeof(psVideoFormat));


//		SendMessage(camhwnd, WM_CAP_DLG_VIDEOFORMAT, 0, 0);
//		capDlgVideoCompression(camhwnd);

		SendMessage(camhwnd, WM_CAP_SET_SCALE, true, 0);
		SendMessage(camhwnd, WM_CAP_SET_PREVIEWRATE, 16, 0);
		SendMessage(camhwnd, WM_CAP_SET_PREVIEW, true, 0);
	}
	initialized = true;

}

void Zoomy::destroy()
{

}

void Zoomy::paint(HDC hdc)
{
	draw_pixels(hdc, DISPLAY_WIDTH, 0, WIDTH, HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT, recv_image);

	if (enable_video == 0)
	{
		// capture will draw himself, so no need to do anything
		draw_pixels(hdc, 0, 0, WIDTH, HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT, cap_image);
	}

	char state[MAX_PATH];

	sprintf(state, "Connect sframe socket %d, connected=%d. client rframe socket %d, connected=%d voice enable=%d (space)",
		connect_sframe_sock,
		connect_state == CONNECTED,
		client_rframe_sock,
		client_state == CONNECTED,
		enable_voice
	);
//	TextOut(hdc, 50, 500, state, strlen(state));

	sprintf(state, "ip %s local %s client %s",
		connect_ip,
		listen_ip,
		client_ip
	);
//	TextOut(hdc, 50, 550, state, strlen(state));
}

void Zoomy::draw_pixels(HDC hdc, int xoff, int yoff, int width, int height, int scalew, int scaleh, unsigned char *data)
{
	HBITMAP hBitmap, hOldBitmap;
	HDC hdcMem;

	hBitmap = CreateCompatibleBitmap(hdc, width, height);
	SetBitmapBits(hBitmap, sizeof(int) * width * height, data);
	hdcMem = CreateCompatibleDC(hdc);
	hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

	// This scaling is a little strange because Stretch maintains aspect ratios
	StretchBlt(hdc, xoff, yoff, scalew, scaleh, hdcMem, 0, 0, width, height, SRCCOPY);
	SelectObject(hdcMem, hOldBitmap);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}

void Zoomy::keydown(int key)
{
	switch (key)
	{
	case VK_SPACE:
		mute_microphone = !mute_microphone;
		break;
	}
}

// Note: lpVHdr has YUY2 data not RGB
LRESULT Zoomy::frameCallback(HWND hWnd, LPVIDEOHDR lpVHdr)
{
	if (connect_sframe_sock != SOCKET_ERROR && enable_video)
	{
		// prevent duplicate frames
		if (memcmp(cap_image, cap_image_last, FRAME_SIZE / 2) != 0)
		{
			header_t header;

			header.magic = 0xDEAFB4B3;
			header.size = lpVHdr->dwBytesUsed;
			enqueue(&squeue, (unsigned char *)&header, sizeof(header_t));
			enqueue(&squeue, lpVHdr->lpData, lpVHdr->dwBytesUsed);
			memcpy(cap_image_last, lpVHdr->lpData, lpVHdr->dwBytesUsed);
		}
		else
		{
			printf("Duplicate frame\r\n");
		}
	}

	return 0;
}

// Could probably throw this into WMU_CAPTURE and it would be fine without an extra thread, but I like how compartmentalized it is
DWORD WINAPI Zoomy::VoiceThread(LPVOID lpParam)
{
	static Audio audio;
	static Voice voice;
	static int udp_connect = -1;
	static int udp_listen = -1;
	static int udp_port_connect = 65533;
	static int udp_port_listen = 65532;
	static char connect_ip[MAX_PATH] = "127.0.0.1";
	static char listen_ip[MAX_PATH] = "127.0.0.1";

	char path[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\zoomy.ini"));


	udp_port_connect = GetPrivateProfileInt(TEXT("zoomy"), TEXT("udp_connect"), 65533, path);
	udp_port_listen = GetPrivateProfileInt(TEXT("zoomy"), TEXT("udp_listen"), 65532, path);
	GetPrivateProfileString(TEXT("zoomy"), TEXT("ip"), "127.0.0.1", connect_ip, MAX_PATH, path);
	GetPrivateProfileString(TEXT("zoomy"), TEXT("localip"), "127.0.0.1", listen_ip, MAX_PATH, path);


	socket_connect(udp_connect, connect_ip, udp_port_connect);
	socket_bind(udp_listen, NULL, udp_port_listen);


	audio.init();
	voice.init(audio);
	audio.capture_start();


	while (1)
	{
		if (enable_voice)
		{
			voice.voice_recv(audio, udp_listen, connect_ip, udp_port_listen);

			if (mute_microphone == false)
			{
				voice.voice_send(audio, udp_connect, connect_ip, udp_port_connect);
			}
			Sleep(0);
		}
		else
		{
			Sleep(1000);
		}
	}


	return 0;
}


int Zoomy::listen_socket(int &sock, unsigned short port)
{
	struct sockaddr_in	servaddr;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(sock, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	listen(sock, 3);
	printf("listening on port %d\n", port);
	return 0;
}

int Zoomy::set_sock_options(int sock)
{
	unsigned long nonblock = 1;
	ioctlsocket(sock, FIONBIO, &nonblock);
	unsigned int sndbuf;
	unsigned int rcvbuf;

	socklen_t arglen = sizeof(int);

	getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
	printf("SO_SNDBUF = %d\n", sndbuf);
	if (sndbuf < 65507)
	{
		sndbuf = 65507; //default 8192
		setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof(sndbuf));
		printf("Setting SO_SNDBUF to %d\n", sndbuf);
		getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
		printf("SO_SNDBUF = %d\n", sndbuf);
	}

	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
	printf("SO_RCVBUF = %d\n", rcvbuf);
	if (rcvbuf < 65507)
	{
		rcvbuf = 65507; //default 8192
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, sizeof(rcvbuf));
		printf("Setting SO_RCVBUF to %d\n", rcvbuf);
		getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
		printf("SO_RCVBUF = %d\n", rcvbuf);
	}


	int flag = 1;
	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	return 0;
}


int Zoomy::connect_socket(char *ip_addr, unsigned short port, int &sock)
{
	struct sockaddr_in	servaddr;
	int ret;

	if (sock == -1)
	{
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		set_sock_options(sock);

		memset(&servaddr, 0, sizeof(struct sockaddr_in));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = inet_addr(ip_addr);
		servaddr.sin_port = htons(port);

		// 3 way handshake
		printf("Attempting to connect to %s:%d\n", ip_addr, port);
		ret = connect(sock, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));

		if (ret == SOCKET_ERROR)
		{
			int err = WSAGetLastError();

			switch (err)
			{
			case WSAETIMEDOUT:
				printf("Fatal Error: Connecting to %s timed out.\n", ip_addr);
				break;
			case WSAECONNREFUSED:
				printf("Fatal Error: %s refused connection.\n(Server program is not running)\n", ip_addr);
				break;
			case WSAEHOSTUNREACH:
				printf("Fatal Error: router sent ICMP packet (destination unreachable)\n");
				break;
			case WSAEWOULDBLOCK:
				printf("Would block, using select()\r\n");
				return -1;
			default:
				printf("Fatal Error: %d\n", ret);
				break;
			}

			return -1;
		}

		printf("TCP handshake completed with %s\n", ip_addr);
		return 0;
	}
	else
	{

		struct timeval timeout;
		fd_set read_set;
		fd_set write_set;

		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_SET(sock, &read_set);
		FD_SET(sock, &write_set);

		timeout.tv_sec = 0;
		timeout.tv_usec = 5;

		int ret = select(sock + 1, &read_set, &write_set, NULL, &timeout);
		if (ret < 0)
		{
			printf("select() failed ");
			return -1;
		}
		else if (ret == 0)
		{
			static int count = 0;
			printf("select() timed out %d of 10\r\n", count);
			count++;

			if (count >= 10)
			{
				count = 0;
				printf("Resetting socket\r\n");
				closesocket(sock);
				sock = -1;
			}
			return -1;
		}

		if (FD_ISSET(sock, &read_set) || FD_ISSET(sock, &write_set))
		{
			unsigned int err = -1;
			int len = 4;
			int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
			if (ret != 0)
			{
				printf("getsockopt failed\r\n");
			}

			if (err == 0)
			{
				printf("TCP handshake completed with %s\n", ip_addr);
				return 0;
			}
			else
			{
				printf("SO_ERROR was not zero\r\n");
				closesocket(sock);
				sock = -1;
			}

		}
	}

	return -1;
}

void Zoomy::handle_listen(int &sock, int &csock, char *ipstr)
{
	struct sockaddr_in csockaddr;
	int addrlen = sizeof(sockaddr);

	csock = accept(sock, (sockaddr *)&csockaddr, &addrlen);
	if (csock != -1)
	{
		inet_ntop(AF_INET, &(csockaddr.sin_addr), ipstr, MAX_PATH);
		printf("Accepted connection from %s\n", ipstr);
		set_sock_options(csock);
	}
	else
	{
		int err = WSAGetLastError();

		switch (err)
		{
		case WSAETIMEDOUT:
			printf("Fatal Error: timed out.\n");
			break;
		case WSAECONNREFUSED:
			printf("Fatal Error: refused connection.\n(Server program is not running)\n");
			break;
		case WSAEHOSTUNREACH:
			printf("Fatal Error: router sent ICMP packet (destination unreachable)\n");
			break;
		case WSAEWOULDBLOCK:
			return;
		default:
			printf("Fatal Error: %d\n", err);
			break;
		}

		return;

	}
}


void Zoomy::read_socket(int &csock, char *buffer, int &size)
{
	if (csock == -1)
		return;

	size = 0;
	while (1)
	{
		int ret = 0;
		ret = recv(csock, &buffer[size], FRAME_PACKET_SIZE, 0);
		if (ret > 0)
		{
//			printf("Read %d bytes from socket\r\n", ret);
			size += ret;
			if (size >= FRAME_PACKET_SIZE)
			{
//				printf("Read at least one frame\r\n");
				break;
			}
		}
		else if (ret == 0)
		{
			break;
		}
		else if (ret < 0)
		{
			int err = WSAGetLastError();

			switch (err)
			{
			case WSAETIMEDOUT:
				csock = SOCKET_ERROR;
				break;
			case WSAECONNREFUSED:
				csock = SOCKET_ERROR;
				break;
			case WSAEHOSTUNREACH:
				csock = SOCKET_ERROR;
				break;
			case WSAEWOULDBLOCK:
				break;
			default:
				printf("Fatal Error: %d\n", err);
				csock = SOCKET_ERROR;
				break;
			}
			break;
		}
	}
}

void Zoomy::yuy2_to_rgb(unsigned char *yuvData, COLORREF *data)
{
	int j = 0;

	for (int i = 0; i < WIDTH * HEIGHT; i += 2)
	{
		//YUY2 to ABGR
		BYTE Y0 = yuvData[j++];
		BYTE U = yuvData[j++];
		BYTE Y1 = yuvData[j++];
		BYTE V = yuvData[j++];

		data[i] = RGB2(
			(unsigned char)((float)Y0 + (1.4075f*(float)(V - 128))),
			(unsigned char)((float)Y0 + (0.3455f*(float)(U - 128) - (0.7169f*(float)(V - 128)))),
			(unsigned char)((float)Y0 + (1.7790f*(float)(U - 128)))
		);

		data[i + 1] = RGB2(
			(unsigned char)((float)Y1 + (1.4075f*(float)(V - 128))),
			(unsigned char)((float)Y1 + (0.3455f*(float)(U - 128) - (0.7169f*(float)(V - 128)))),
			(unsigned char)((float)Y1 + (1.7790f*(float)(U - 128)))
		);
	}
}



