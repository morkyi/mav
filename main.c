#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "miniaudio.h"

typedef struct {
	ma_device  device;
	ma_context context;
	float     *samples; //heap allocated
	ma_uint32  capacity;
	ma_uint32  write_index;
	ma_uint32  read_index;
} s_ma_data;

typedef struct {
	SDL_Window   *window;
	SDL_Renderer *renderer;
	int w, h;
} s_sdl_data;

//Globals
s_ma_data  audio_data;
s_sdl_data sdl_data = {NULL, NULL, 1200, 400};
//

void data_callback(ma_device *device, void *output, const void *input,
		   ma_uint32 frame_count)
{
	s_ma_data* data = (s_ma_data*)device->pUserData;
	if (data->capacity == 0 || data->samples == NULL) return;

	const float *p_float = (const float*)input;

	for (ma_uint32 i = 0; i < frame_count; ++i) {
		float mono = 0.0f;
		int channels = device->capture.channels;
		for (int ch = 0; ch < channels; ++ch) {
			mono += p_float[i * channels + ch];
		}
		mono /= channels;

		//now write mono into ring buffer
		data->samples[data->write_index] = mono;
		data->write_index = (data->write_index + 1) % data->capacity;
	}
}

void select_device()
{
	ma_result result;
	ma_device_info *p_playback_device_infos;
	ma_uint32 playback_device_count;
	ma_uint32 u32_device;
	ma_device_info *p_capture_device_infos;
	ma_uint32 capture_device_count;

	if (ma_context_init(NULL, 0, NULL, &audio_data.context)
							!= MA_SUCCESS) {
		printf("Failed to initialize ma context.\n");
		exit(1);
	}

	result = ma_context_get_devices(&audio_data.context,
					&p_playback_device_infos,
					&playback_device_count,
					&p_capture_device_infos,
					&capture_device_count);
	if (result != MA_SUCCESS) {
		printf("Failed to retrieve device information.\n");
		exit(1);
	}

	printf("Capture devices\n");
	for (u32_device = 0; u32_device < capture_device_count; ++u32_device)
	{
		printf("    %u: %s\n", u32_device,
		       p_capture_device_infos[u32_device].name);
	}

	printf("Select the device you want by typing its number.\n");

	int user_choice; //TODO unsigned
	scanf("%d", &user_choice);

	if (user_choice < 0 || (ma_uint32)user_choice
					>= playback_device_count) {
		printf("Invalid device number.\n");
		exit(1);
	}
	//else user_choice is good

	ma_device_config config = ma_device_config_init(ma_device_type_capture);
	config.capture.pDeviceID = &p_capture_device_infos[user_choice].id;
	config.capture.format = ma_format_f32;
	config.capture.channels = 0;
	config.playback.channels = 0;
	config.sampleRate = 0;
	config.dataCallback = data_callback;
	config.pUserData = &audio_data;

	if (ma_device_init(&audio_data.context, &config, &audio_data.device)
							!= MA_SUCCESS) {
		printf("Failed ma_device_init\n");
		exit(1);
	}

	ma_uint32 sr = audio_data.device.sampleRate;
	ma_uint32 ch = audio_data.device.capture.channels;
	if (sr == 0) sr = 44100;
	if (ch == 0) ch = 2;
	audio_data.capacity = sr * ch * 0.1;
	printf("sample rate: %d, channels: %d, capacity: %d\n",
	       audio_data.device.sampleRate,
	       audio_data.device.capture.channels,
	       audio_data.capacity);
	audio_data.samples = malloc(audio_data.capacity * sizeof(float));
	audio_data.write_index = 0;
	audio_data.read_index = 0;
}

void cleanup()
{
	ma_device_uninit(&audio_data.device);
	ma_context_uninit(&audio_data.context);
	free(audio_data.samples);
	SDL_DestroyRenderer(sdl_data.renderer);
	SDL_DestroyWindow(sdl_data.window);
	SDL_Quit();
}

int get_valid_int()
{
	char buffer[16];
	char *endptr;

	while (1) {
		if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
			printf("Error reading input\n");
			exit(1);
		}

		char *newline = strchr(buffer, '\n');
		if (newline == NULL) {
			printf("Input too long, try again\n");
			int c;
			while ((c = getchar()) != '\n' && c != EOF);
			continue;
		}
		*newline = '\0';

		if (buffer[0] == '\0') {
			printf("Please enter a number\n");
			continue;
		}

		long value = strtol(buffer, &endptr, 10);
		if (*endptr != '\0') {
			printf("Invalid number, please try again\n");
			continue;
		}

		return (int)value;
	}
}

unsigned int set_fps()
{
	printf("Type in your fps. Must be greater than 0.\n");
	int value;
	do {
		value = get_valid_int();
		if (value <= 0)
			printf("FPS must be greater than 0, try again.\n");
	} while (value <= 0);
	return (unsigned int)value;
}

void render_init()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		exit(1);
	}

	sdl_data.window = SDL_CreateWindow("MAV", SDL_WINDOWPOS_CENTERED,
						SDL_WINDOWPOS_CENTERED,
						sdl_data.w,
						sdl_data.h,
						SDL_WINDOW_RESIZABLE);
	if (sdl_data.window == NULL) {
		printf("SDL_CreateWindow error: %s\n", SDL_GetError());
		exit(1);
	}

	sdl_data.renderer = SDL_CreateRenderer(sdl_data.window, -1,
						 SDL_RENDERER_ACCELERATED);
	if (sdl_data.renderer == NULL) {
		printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
		exit(1);
	}
}

void render_loop(unsigned int fps)
{
	bool running = true;
	SDL_Event event;

	while (running) {
		SDL_GetRendererOutputSize(sdl_data.renderer, &sdl_data.w,
					  &sdl_data.h);

		//HANDLE QUIT
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT)
				running = false;
		}

		SDL_SetRenderDrawColor(sdl_data.renderer, 0, 0, 0, 255);
		SDL_RenderClear(sdl_data.renderer);

		//SNAPSHOT:
		float *snapshot = malloc(sdl_data.w * sizeof(float));
		ma_uint32 N = sdl_data.w;
		ma_uint32 start = (audio_data.write_index - N
					+ audio_data.capacity)
					% audio_data.capacity;
		for (ma_uint32 i = 0; i < N; ++i)
			snapshot[i] = audio_data.samples[
				(start + i) % audio_data.capacity
			];

		//NORMALIZE:
		float peak = 0.0f;
		for (ma_uint32 i = 0; i < N; ++i) {
			float abs_val = snapshot[i] < 0 ?
					-snapshot[i] : snapshot[i];
			if (abs_val > peak) peak = abs_val;
		}
		if (peak > 0.0001f)
			for (ma_uint32 i = 0; i < N; ++i)
				snapshot[i] /= peak;

		//DRAW WAVEFORM:
		SDL_SetRenderDrawColor(sdl_data.renderer, 255, 255, 255, 255);
		for (ma_uint32 i = 0; i < N - 1; ++i) {
			int x1 = i;
			int x2 = i + 1;
			int y1 = (sdl_data.h / 2) - (int)(snapshot[i]
				* (sdl_data.h / 2)
			);
			int y2 = (sdl_data.h / 2) - (int)(snapshot[i+1]
				* (sdl_data.h / 2)
			);
			SDL_RenderDrawLine(sdl_data.renderer, x1, y1, x2, y2);
		}

		free(snapshot);
		SDL_RenderPresent(sdl_data.renderer);

		//fps
		SDL_Delay(1000 / fps);
	}
}

int main()
{
	printf("Welcome. This is MAV: M's Audio Visualizer. It will find your audio devices, ask you to chose a device which is hosting the audio you're playing, and then it will open a window showing the soundwave.\n");
	select_device();
	ma_device_start(&audio_data.device);
	unsigned int fps = set_fps();
	render_init();
	render_loop(fps);
	cleanup();
	return 0;
}
