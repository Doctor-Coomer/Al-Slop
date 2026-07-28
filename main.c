#include <SDL3/SDL_timer.h>
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <time.h>
#include <stdlib.h>

#include "al_face.hpp"

#include "al_talk.hpp"
static Uint8 *al_talk_wav_data = NULL;
static Uint32 al_talk_wav_data_len = 0;

#include "al_swish.hpp"
static Uint8 *al_swish_wav_data = NULL;
static Uint32 al_swish_wav_data_len = 0;

#include "al_dance.hpp"
static Uint8 *al_dance_wav_data = NULL;
static Uint32 al_dance_wav_data_len = 0;


static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioSpec spec;


static SDL_Texture *al_face = NULL; // aka: idle face
static SDL_Texture *al_talk[4]  = {NULL, NULL, NULL, NULL};
static SDL_Texture *al_swish[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static SDL_Texture *al_dance[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

#define SDL_SECONDS_TO_MS(n) SDL_NS_TO_MS(SDL_SECONDS_TO_NS(n))

#define SDL_MS_TO_SECONDS(n) SDL_NS_TO_SECONDS(SDL_MS_TO_NS(n))

Sint32 SDL_rand_range(Sint32 lower, Sint32 upper) {
  return SDL_rand(upper-lower+1) + lower;
}

enum AlHeadState {
  IDLE,
  TALK,
  SWISH,
  DANCE
};

struct AlHead {
  SDL_FRect rect;
  enum AlHeadState state;
  SDL_Texture *current_face;
  float scale;
  Uint64 previous; // In MS
  Uint64 delta; // In MS
  Uint64 delta_count; // Track how many times we have passed a delta amount of time
  Uint64 previous_wait_seconds; // In Seconds
  Uint64 wait_seconds; // In Seconds
  Uint64 talk_state_iterations;
  Uint64 swish_state_iterations;
  Uint64 dance_state_iterations;
  bool do_sound;
  bool do_clone;
  SDL_AudioStream* stream;
  void (*draw)(struct AlHead *);
  void (*sound)(struct AlHead *);
  void (*think)(struct AlHead *);
  struct AlHead *(*new)(SDL_FRect, float);
  void (*free)(struct AlHead *);
};
void AlHead_sound(struct AlHead *head) {
  const Uint64 now = SDL_GetTicks();

  if (now - SDL_SECONDS_TO_MS(head->previous_wait_seconds) < SDL_SECONDS_TO_MS(head->wait_seconds)) {
    return;
  }


  if (head->do_sound == false) {
    return;
  }

  switch (head->state) {
  case TALK:
    {
      if (SDL_GetAudioStreamQueued(head->stream) < (int)al_talk_wav_data_len && head->current_face == al_talk[0]) {
	SDL_PutAudioStreamData(head->stream, al_talk_wav_data, al_talk_wav_data_len);
      }
      break;
    }

  case SWISH:
    {
      if (SDL_GetAudioStreamQueued(head->stream) < (int)al_swish_wav_data_len && head->current_face == al_swish[0]) {
	SDL_PutAudioStreamData(head->stream, al_swish_wav_data, al_swish_wav_data_len);
      }
      break;
    }

  case DANCE:
    {
      if (SDL_GetAudioStreamQueued(head->stream) < (int)al_dance_wav_data_len) {
	SDL_PutAudioStreamData(head->stream, al_dance_wav_data, al_dance_wav_data_len);
      }
      break;
    }
    
  }
  
  head->do_sound = false;
}
void AlHead_draw(struct AlHead *head) {

  SDL_FRect final_rect;

  final_rect.h = head->current_face->h * head->scale;
  final_rect.w = head->current_face->w * head->scale;

  if (head->state == DANCE) { // Center about the X axis
    final_rect.x = head->rect.x + (al_face->w * head->scale - final_rect.w)/2.f;
    final_rect.y = head->rect.y + (al_face->h * head->scale - final_rect.h);
  } else if (head->state == SWISH) { // Center about both axis
    final_rect.x = head->rect.x + (al_face->w * head->scale - final_rect.w)/2.f;
    final_rect.y = head->rect.y + (al_face->h * head->scale - final_rect.h)/2.f;
  } else { // 1:1
    final_rect.x = head->rect.x;
    final_rect.y = head->rect.y;
  }
  
  SDL_RenderTexture(renderer, head->current_face, NULL, &final_rect);
}
void AlHead_think(struct AlHead *head) {
  const Uint64 now = SDL_GetTicks();
  
  if (now - head->previous >= head->delta) {

    if (now - SDL_SECONDS_TO_MS(head->previous_wait_seconds) >= SDL_SECONDS_TO_MS(head->wait_seconds)) {
      //printf("AlHead_think: %p %ld\n", head, now - head->previous);
      
      head->previous_wait_seconds = SDL_MS_TO_SECONDS(now);
      head->wait_seconds = 0;

      head->do_sound = true;

      switch (head->state) {
      case IDLE:
        head->current_face = al_face;
        break;

      case TALK:
	{
	  if (head->delta_count <= head->talk_state_iterations) {
	    head->current_face = al_talk[head->delta_count % 4];
	  } else {
	    head->current_face = al_face;
	    head->state = SWISH;
	    head->swish_state_iterations = SDL_rand_range(7*3, 21*3);
	    head->wait_seconds = SDL_rand_range(1, 4);
	    head->do_sound = false;
	  }
	  
	  break;
	}

      case SWISH:
	{
	  if (head->delta_count <= head->swish_state_iterations) {
	    head->current_face = al_swish[head->delta_count % 7];
	  } else {
	    head->current_face = al_face;
	    head->state = DANCE;
	    head->dance_state_iterations = SDL_rand_range(7*3, 21*3);
	    head->wait_seconds = SDL_rand_range(2, 7);
	    head->do_sound = false;
	  }

          break;
	}

      case DANCE:
	{
	  if (head->delta_count <= head->dance_state_iterations) {
	    head->current_face = al_dance[head->delta_count % 6];
	  } else {
	    head->current_face = al_face;
	    head->state = TALK;
	    head->talk_state_iterations = SDL_rand_range(10*3, 24*3);
	    head->wait_seconds = SDL_rand_range(2, 7);
	    head->do_sound = false;
	    SDL_ClearAudioStream(head->stream); // Cut dance sound off early
	    head->do_clone = true;
	  }
	  
	  break;
	}
      }

      
      
      ++head->delta_count;
    } else {
      head->delta_count = 0;
    }
    
    head->previous = now;
  }
}
void AlHead_free(struct AlHead *head) {
  SDL_ClearAudioStream(head->stream);
  free(head);
}
struct AlHead *AlHead_new(SDL_FRect rect, float scale) {
  struct AlHead *head = malloc(sizeof(struct AlHead));

  head->draw = &AlHead_draw;
  head->sound = &AlHead_sound;
  head->think = &AlHead_think;
  head->new = &AlHead_new;
  head->free = &AlHead_free;

  head->rect = rect;
  head->current_face = al_face;
  head->scale = scale;
  head->state = TALK;

  head->previous = SDL_GetTicks(); 
  head->delta = SDL_MS_PER_SECOND/30;
  head->delta_count = 0;
  
  head->previous_wait_seconds = SDL_MS_TO_SECONDS(SDL_GetTicks());
  head->wait_seconds = 0;

  head->talk_state_iterations = SDL_rand_range(10*3, 24*3);
  head->swish_state_iterations = 0; // Value set in AlHead_think
  head->dance_state_iterations = 0; // Value set in AlHead_think

  head->do_sound = false;
  head->do_clone = false;

  head->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (!head->stream) {
    SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
  }

  // SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to
  // start!
  SDL_ResumeAudioStreamDevice(head->stream);


  SDL_SetAudioStreamFrequencyRatio(head->stream, SDL_rand_range(90, 140)/100.f);

  
  return head;
}

static struct AlHead *alhead;

struct HeadVector {
  struct AlHead **arr;
  unsigned long len;
};

struct HeadVector *vector_init(struct AlHead *head) {
  struct HeadVector *head_vector = malloc(sizeof(struct HeadVector));
  head_vector->arr = malloc(sizeof(struct AlHead *));

  head_vector->arr[0] = head;
  head_vector->len = 1;

  return head_vector;
}

void vector_push(struct HeadVector *head_vector, struct AlHead *head) {
  head_vector->arr = realloc(head_vector->arr, sizeof(struct HeadVector)*(head_vector->len+1));
  head_vector->arr[head_vector->len] = head;
  head_vector->len++;
};

void vector_pop(struct HeadVector *head_vector) {
  struct AlHead *head = head_vector->arr[head_vector->len-1];
  head->free(head);
  
  head_vector->arr = realloc(head_vector->arr, sizeof(struct HeadVector)*(head_vector->len-1));
  head_vector->len--;
}

static struct HeadVector* head_vector;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {

  SDL_srand(time(NULL));

  SDL_SetAppMetadata("Al Slop", "1.0", "Al Slop");

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  /* we don't _need_ a window for audio-only things but it's good policy to have
   * one. */
  if (!SDL_CreateWindowAndRenderer("Al Slop", WINDOW_WIDTH, WINDOW_HEIGHT,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // We're just playing a single thing here, so we'll use the simplified option.
  // We are always going to feed audio in as mono, float32 data at 8000Hz.
  // The stream will convert it to whatever the hardware wants on the other
  // side.

  if (!SDL_LoadWAV_IO(SDL_IOFromMem(al_talk_wav, al_talk_wav_len), true, &spec, &al_talk_wav_data, &al_talk_wav_data_len)) {
    return SDL_APP_FAILURE;
  }

  if (!SDL_LoadWAV_IO(SDL_IOFromMem(al_swish_wav, al_swish_wav_len), true, &spec, &al_swish_wav_data, &al_swish_wav_data_len)) {
    return SDL_APP_FAILURE;
  }

  if (!SDL_LoadWAV_IO(SDL_IOFromMem(al_dance_wav, al_dance_wav_len), true, &spec, &al_dance_wav_data, &al_dance_wav_data_len)) {
    return SDL_APP_FAILURE;
  }

  

  SDL_Surface *surface = NULL;

  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_face_png, al_face_png_len), true);
  al_face = SDL_CreateTextureFromSurface(renderer, surface);

  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_talk1_png, al_talk1_png_len), true);
  al_talk[0] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_talk2_png, al_talk2_png_len), true);
  al_talk[1] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_talk3_png, al_talk3_png_len), true);
  al_talk[2] = SDL_CreateTextureFromSurface(renderer, surface);
  al_talk[3] = al_face;
  
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish1_png, al_swish1_png_len), true);
  al_swish[0] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish2_png, al_swish2_png_len), true);
  al_swish[1] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish3_png, al_swish3_png_len), true);
  al_swish[2] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish4_png, al_swish4_png_len), true);
  al_swish[3] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish5_png, al_swish5_png_len), true);
  al_swish[4] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish6_png, al_swish6_png_len), true);
  al_swish[5] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_swish7_png, al_swish7_png_len), true);
  al_swish[6] = SDL_CreateTextureFromSurface(renderer, surface);
  
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance1_png, al_dance1_png_len), true);
  al_dance[0] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance2_png, al_dance2_png_len), true);
  al_dance[1] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance3_png, al_dance3_png_len), true);
  al_dance[2] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance4_png, al_dance4_png_len), true);
  al_dance[3] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance5_png, al_dance5_png_len), true);
  al_dance[4] = SDL_CreateTextureFromSurface(renderer, surface);
  surface = SDL_LoadPNG_IO(SDL_IOFromMem(al_dance6_png, al_dance6_png_len), true);
  al_dance[5] = SDL_CreateTextureFromSurface(renderer, surface);
  
  SDL_DestroySurface(surface); /* done with this, the texture has a copy of the pixels now. */

  SDL_FRect rect = {WINDOW_WIDTH/2.f - (al_face->w*0.4)/2.f, WINDOW_HEIGHT/2.f - (al_face->h*0.4)/2.f, al_face->w, al_face->h};
  alhead = AlHead_new(rect, 0.4f);

  head_vector = vector_init(alhead);
  
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
  }
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {

  // Clone
  unsigned long len = head_vector->len;
  for (unsigned int i = 0; i < len; ++i) {
    struct AlHead* head = head_vector->arr[i];
    if (head->do_clone == true) {
      SDL_FRect rect = {SDL_rand_range(0, WINDOW_WIDTH-50), SDL_rand_range(0, WINDOW_HEIGHT-50), al_face->w, al_face->h};
      vector_push(head_vector, head->new(rect, 0.1));
      head->do_clone = false;
    }
  }
  
  // Think
  for (unsigned int i = 0; i < head_vector->len; ++i) {
    struct AlHead* head = head_vector->arr[i];
    head->think(head);
  }
  bool SDL_SetAudioStreamFrequencyRatio(SDL_AudioStream *stream, float ratio);

  // Sound + Render
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(renderer);
  for (unsigned int i = 0; i < head_vector->len; ++i) {
    struct AlHead* head = head_vector->arr[i];
    head->sound(head);
    head->draw(head);
  }    
  SDL_RenderPresent(renderer);

  
  if (head_vector->len > 15) {
    unsigned long len = head_vector->len-1;
    for (unsigned int i = 0; i < len; ++i) {
      vector_pop(head_vector);
    }
    
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  /* SDL will clean up the window/renderer for us. */
}
