#define SCREEN_WIDTH    854
#define SCREEN_HEIGHT   480

#include <arpa/inet.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <vanilla.h>
#include <unistd.h>

AVFrame *present_frame;
AVFrame *decoding_frame;
SDL_mutex *decoding_mutex;
int decoding_ready = 0;
AVCodecContext *video_codec_ctx;
AVCodecParserContext *video_parser;
AVPacket *video_packet;

// HACK: Easy macro to test between desktop and RPi (even though ARM doesn't necessarily mean RPi)
#ifdef __arm__
#define RASPBERRY_PI
#endif

int decode_frame(const void *data, size_t size)
{
	int ret;

    // Parse this data for packets
	while (size) {
		ret = av_parser_parse2(video_parser, video_codec_ctx, &video_packet->data, &video_packet->size,
                               data, size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		
		data += ret;
		size -= ret;

		if (video_packet->size) {
			// Send packet to decoder
			ret = avcodec_send_packet(video_codec_ctx, video_packet);
			if (ret < 0) {
				fprintf(stderr, "Failed to send packet to decoder: %i\n", ret);
				return 1;
			}

			// Retrieve frame from decoder
			ret = avcodec_receive_frame(video_codec_ctx, decoding_frame);
			if (ret == AVERROR(EAGAIN)) {
				// Decoder wants another packet before it can output a frame. Silently exit.
			} else if (ret < 0) {
				fprintf(stderr, "Failed to receive frame from decoder: %i\n", ret);
			} else {
				SDL_LockMutex(decoding_mutex);
				
				// Swap frames
				AVFrame *tmp = decoding_frame;
				decoding_frame = present_frame;
				present_frame = tmp;

				// Un-ref frame
				av_frame_unref(decoding_frame);

				// Signal we have a frame
				decoding_ready = 1;
				
				SDL_UnlockMutex(decoding_mutex);
			}
		}
	}
}

int run_backend(void *data)
{
	vanilla_event_t event;

	while (vanilla_wait_event(&event)) {
		if (event.type == VANILLA_EVENT_VIDEO) {
			decode_frame(event.data, event.size);
		}
	}

	return 0;
}

void logger(const char *s, va_list args)
{
	vfprintf(stderr, s, args);
}

// #define NO_DISPLAY
int main(int argc, const char **argv)
{
	// Initialize SDL2
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
	
	SDL_Window* window = NULL;
	SDL_Surface* screenSurface = NULL;
	if (SDL_Init(SDL_INIT_VIDEO/* | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER*/) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

#ifndef NO_DISPLAY
	SDL_ShowCursor(SDL_DISABLE);

	window = SDL_CreateWindow(
				"Vanilla Pi",
				SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				SCREEN_WIDTH, SCREEN_HEIGHT,
				SDL_WINDOW_SHOWN
			);
	if (window == NULL) {
		fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 1;
	}
#endif

	// Initialize FFmpeg
#ifdef RASPBERRY_PI
	const AVCodec *codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
#else
	const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
#endif
	if (!codec) {
		fprintf(stderr, "No decoder was available\n");
		return 1;
	}

    video_codec_ctx = avcodec_alloc_context3(codec);
	if (!video_codec_ctx) {
		fprintf(stderr, "Failed to allocate codec context\n");
		return 1;
	}

	int ffmpeg_err = avcodec_open2(video_codec_ctx, codec, NULL);
    if (ffmpeg_err < 0) {
		fprintf(stderr, "Failed to open decoder: %i\n", ffmpeg_err);
		return 1;
	}

	decoding_frame = av_frame_alloc();
	present_frame = av_frame_alloc();
	if (!decoding_frame || !present_frame) {
		fprintf(stderr, "Failed to allocate AVFrame\n");
		return 1;
	}

	video_packet = av_packet_alloc();
	if (!video_packet) {
		fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
		return 1;
	}

	video_parser = av_parser_init(codec->id);
	if (!video_parser) {
        fprintf(stderr, "Failed to create codec parser\n");
        exit(1);
    }

	// Install logging debugger
	vanilla_install_logger(logger);

	// Start Vanilla
#ifdef RASPBERRY_PI
	vanilla_start(0);
#else
	vanilla_start(ntohl(inet_addr("127.0.0.1")));
#endif

	// Launch backend on second thread
	SDL_Thread *backend_thread = SDL_CreateThread(run_backend, "Backend", NULL);

#ifndef NO_DISPLAY
	// Create main video display texture
	SDL_Texture *main_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

	int delay = 16;

	while (1) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				vanilla_stop();
				break;
			}
		}

		// If a frame is available, present it here
		SDL_LockMutex(decoding_mutex);
		if (decoding_ready) {
#ifndef NO_DISPLAY
			SDL_UpdateYUVTexture(main_texture, NULL,
				present_frame->data[0], present_frame->linesize[0],
				present_frame->data[1], present_frame->linesize[1],
				present_frame->data[2], present_frame->linesize[2]
			);
#endif
		}
		SDL_UnlockMutex(decoding_mutex);

#ifndef NO_DISPLAY
		SDL_RenderCopy(renderer, main_texture, NULL, NULL);
		SDL_RenderPresent(renderer);
#endif
	}
	vanilla_stop();

	SDL_WaitThread(backend_thread, NULL);

	av_parser_close(video_parser);
	av_frame_free(&present_frame);
	av_frame_free(&decoding_frame);
	av_packet_free(&video_packet);
    avcodec_free_context(&video_codec_ctx);

#ifndef NO_DISPLAY
	SDL_DestroyTexture(main_texture);
	SDL_DestroyRenderer(renderer);
#endif
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}