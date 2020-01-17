#include <stdint.h>
#include <stdio.h>
#include "gzm.h"
#include "vector.h"

#define Z64_VERSION           Z64_OOT10

#ifndef Z64_VERSION
# error no z64 version specified
#endif

#define Z64_OOT10             0x00
#define Z64_OOT11             0x01

struct movie_input
{
  z64_controller_t  raw;
  uint16_t          pad_delta;
};

struct movie_seed
{
  int32_t           frame_idx;
  uint32_t          old_seed;
  uint32_t          new_seed;
};

struct movie_oca_input
{
  int32_t           frame_idx;
  uint16_t          pad;
  int8_t            adjusted_x;
  int8_t            adjusted_y;
};

struct movie_oca_sync
{
  int32_t           frame_idx;
  int32_t           audio_frames;
};

struct movie_room_load
{
  int32_t           frame_idx;
};

struct gz
{
  _Bool             ready;
  z64_controller_t  movie_input_start;
  struct vector     movie_input;
  struct vector     movie_seed;
  struct vector     movie_oca_input;
  struct vector     movie_oca_sync;
  struct vector     movie_room_load;
  int               movie_frame;
  int               movie_seed_pos;
  int               movie_oca_input_pos;
  int               movie_oca_sync_pos;
  int               movie_room_load_pos;
};

static struct gz    gz;

static void z_to_movie(int movie_frame, z64_input_t *zi, _Bool reset)
{
  struct movie_input *mi = vector_at(&gz.movie_input, movie_frame);
  z64_controller_t *raw_prev;
  if (movie_frame == 0) {
    raw_prev = &gz.movie_input_start;
    raw_prev->x = zi->raw.x - zi->x_diff;
    raw_prev->y = zi->raw.y - zi->y_diff;
    raw_prev->pad = (zi->raw.pad |
                     (~zi->pad_pressed & zi->pad_released)) &
                    ~(zi->pad_pressed & ~zi->pad_released);
  }
  else {
    struct movie_input *mi_prev = vector_at(&gz.movie_input, movie_frame - 1);
    raw_prev = &mi_prev->raw;
  }
  mi->raw = zi->raw;
  mi->pad_delta = (~mi->raw.pad & raw_prev->pad & zi->pad_pressed) |
                  (mi->raw.pad & ~raw_prev->pad & zi->pad_released) |
                  (zi->pad_pressed & zi->pad_released);
  mi->pad_delta |= reset << 7;
}

void gzm_start(void)
{
  if (gz.ready)
    return;

  vector_init(&gz.movie_input, sizeof(struct movie_input));
  vector_init(&gz.movie_seed, sizeof(struct movie_seed));
  vector_init(&gz.movie_oca_input, sizeof(struct movie_oca_input));
  vector_init(&gz.movie_oca_sync, sizeof(struct movie_oca_sync));
  vector_init(&gz.movie_room_load, sizeof(struct movie_room_load));
  gz.movie_frame = 0;
  gz.movie_seed_pos = 0;
  gz.movie_oca_input_pos = 0;
  gz.movie_oca_sync_pos = 0;
  gz.movie_room_load_pos = 0;

  gz.ready = 1;
}

static uint16_t swap16(uint16_t v)
{
  return ((v << 8) & 0xFF00) | ((v >> 8) & 0x00FF);
}

static uint32_t swap32(uint32_t v)
{
  return ((v << 24) & 0xFF000000) | ((v << 8)  & 0x00FF0000) |
         ((v >> 8)  & 0x0000FF00) | ((v >> 24) & 0x000000FF);
}

void gzm_stop(void)
{
  if (!gz.ready)
    return;

  gz.movie_input_start.pad = swap16(gz.movie_input_start.pad);
  for (int i = 0; i < (int)gz.movie_input.size; ++i) {
    struct movie_input *mi = vector_at(&gz.movie_input, i);
    mi->raw.pad = swap16(mi->raw.pad);
    mi->pad_delta = swap16(mi->pad_delta);
  }
  for (int i = 0; i < (int)gz.movie_seed.size; ++i) {
    struct movie_seed *ms = vector_at(&gz.movie_seed, i);
    ms->frame_idx = swap32(ms->frame_idx);
    ms->old_seed = swap32(ms->old_seed);
    ms->new_seed = swap32(ms->new_seed);
  }
  for (int i = 0; i < (int)gz.movie_oca_input.size; ++i) {
    struct movie_oca_input *oi = vector_at(&gz.movie_oca_input, i);
    oi->frame_idx = swap32(oi->frame_idx);
    oi->pad = swap16(oi->pad);
  }
  for (int i = 0; i < (int)gz.movie_oca_sync.size; ++i) {
    struct movie_oca_sync *os = vector_at(&gz.movie_oca_sync, i);
    os->frame_idx = swap32(os->frame_idx);
    os->audio_frames = swap32(os->audio_frames);
  }
  for (int i = 0; i < (int)gz.movie_room_load.size; ++i) {
    struct movie_room_load *rl = vector_at(&gz.movie_room_load, i);
    rl->frame_idx = swap32(rl->frame_idx);
  }
  uint32_t n_input = swap32(gz.movie_input.size);
  uint32_t n_seed = swap32(gz.movie_seed.size);
  uint32_t n_oca_input = swap32(gz.movie_oca_input.size);
  uint32_t n_oca_sync = swap32(gz.movie_oca_sync.size);
  uint32_t n_room_load = swap32(gz.movie_room_load.size);

  FILE *f = fopen("movie.gzm", "wb");
  fwrite(&n_input, sizeof(n_input), 1, f);
  fwrite(&n_seed, sizeof(n_seed), 1, f);
  fwrite(&gz.movie_input_start, sizeof(gz.movie_input_start), 1, f);
  fwrite(gz.movie_input.begin,
         gz.movie_input.element_size, gz.movie_input.size, f);
  fwrite(gz.movie_seed.begin,
         gz.movie_seed.element_size, gz.movie_seed.size, f);
  fwrite(&n_oca_input, sizeof(n_oca_input), 1, f);
  fwrite(&n_oca_sync, sizeof(n_oca_sync), 1, f);
  fwrite(&n_room_load, sizeof(n_room_load), 1, f);
  fwrite(gz.movie_oca_input.begin,
         gz.movie_oca_input.element_size, gz.movie_oca_input.size, f);
  fwrite(gz.movie_oca_sync.begin,
         gz.movie_oca_sync.element_size, gz.movie_oca_sync.size, f);
  fwrite(gz.movie_room_load.begin,
         gz.movie_room_load.element_size, gz.movie_room_load.size, f);
  fclose(f);

  vector_destroy(&gz.movie_input);
  vector_destroy(&gz.movie_seed);
  vector_destroy(&gz.movie_oca_input);
  vector_destroy(&gz.movie_oca_sync);
  vector_destroy(&gz.movie_room_load);

  gz.ready = 0;
}

void gzm_record_input(z64_input_t *zi)
{
  if (!gz.ready)
    return;

  if (gz.movie_frame >= (int)gz.movie_input.size) {
    if (gz.movie_input.size == gz.movie_input.capacity)
      vector_reserve(&gz.movie_input, 128);
    vector_push_back(&gz.movie_input, 1, NULL);
  }
  z_to_movie(gz.movie_frame++, zi, 0);
}

void gzm_record_seed(uint32_t old_seed, uint32_t new_seed)
{
  if (!gz.ready)
    return;

  struct movie_seed *ms;
  ms = vector_at(&gz.movie_seed, gz.movie_seed_pos);
  if (!ms || ms->frame_idx != gz.movie_frame)
    ms = vector_insert(&gz.movie_seed, gz.movie_seed_pos, 1, NULL);
  ms->frame_idx = gz.movie_frame;
  ms->old_seed = old_seed;
  ms->new_seed = new_seed;
  ++gz.movie_seed_pos;
}

void gzm_record_oca_input(uint16_t pad, int8_t adjusted_x, int8_t adjusted_y)
{
  if (!gz.ready)
    return;

  struct movie_oca_input *oi;
  oi = vector_at(&gz.movie_oca_input, gz.movie_oca_input_pos);
  if (!oi || oi->frame_idx != gz.movie_frame)
    oi = vector_insert(&gz.movie_oca_input, gz.movie_oca_input_pos, 1, NULL);
  oi->frame_idx = gz.movie_frame;
  oi->pad = pad;
  oi->adjusted_x = adjusted_x;
  oi->adjusted_y = adjusted_y;
  ++gz.movie_oca_input_pos;
}

void gzm_record_oca_sync(int audio_frames)
{
  if (!gz.ready)
    return;

  if (audio_frames == 3)
    return;

  struct movie_oca_sync *os;
  os = vector_at(&gz.movie_oca_sync, gz.movie_oca_sync_pos);
  if (!os || os->frame_idx != gz.movie_frame)
    os = vector_insert(&gz.movie_oca_sync, gz.movie_oca_sync_pos, 1, NULL);
  os->frame_idx = gz.movie_frame;
  os->audio_frames = audio_frames;
  ++gz.movie_oca_sync_pos;
}

void gzm_record_room_load(void)
{
  if (!gz.ready)
    return;

  struct movie_room_load *rl;
  rl = vector_at(&gz.movie_room_load, gz.movie_room_load_pos);
  if (!rl || rl->frame_idx != gz.movie_frame)
    rl = vector_insert(&gz.movie_room_load, gz.movie_room_load_pos, 1, NULL);
  rl->frame_idx = gz.movie_frame;
  ++gz.movie_room_load_pos;
}

void gzm_record_reset(void)
{
  if (!gz.ready)
    return;

  if (gz.movie_frame > 0) {
    struct movie_input *mi = vector_at(&gz.movie_input, gz.movie_frame - 1);
    mi->pad_delta |= (1 << 7);
  }
}

static void input_hook(void)
{
  if (!gz.ready)
    return;

  unsigned long long int word;
  rdword = &word;
#if Z64_VERSION == Z64_OOT10
  const uint32_t input_addr = 0x801C84B4;
#elif Z64_VERSION == Z64_OOT11
  const uint32_t input_addr = 0x801C8674;
#endif
  z64_input_t zi;
  address = input_addr + 0x0000;
  read_rdramh();
  zi.raw.pad = (uint16_t)word;
  address = input_addr + 0x0002;
  read_rdramb();
  zi.raw.x = (uint8_t)word;
  address = input_addr + 0x0003;
  read_rdramb();
  zi.raw.y = (uint8_t)word;
  address = input_addr + 0x000C;
  read_rdramh();
  zi.pad_pressed = (uint16_t)word;
  address = input_addr + 0x000E;
  read_rdramb();
  zi.x_diff = (uint8_t)word;
  address = input_addr + 0x000F;
  read_rdramb();
  zi.y_diff = (uint8_t)word;
  address = input_addr + 0x0012;
  read_rdramh();
  zi.pad_released = (uint16_t)word;

  gzm_record_input(&zi);
}

static void seed_hook(void)
{
  if (!gz.ready)
    return;

  unsigned long long int word;
  rdword = &word;
#if Z64_VERSION == Z64_OOT10
  const uint32_t random_addr = 0x80105440;
#elif Z64_VERSION == Z64_OOT11
  const uint32_t random_addr = 0x80105600;
#endif
  address = random_addr;
  read_rdram();
  uint32_t old_seed = (uint32_t)word;
  /* arg0 is in v1 at this point, moved into a0 in the delay slot */
  uint32_t new_seed = (uint32_t)reg[3];

  gzm_record_seed(old_seed, new_seed);
}

static void oca_input_hook(void)
{
  if (!gz.ready)
    return;

  uint16_t pad = (uint16_t)reg[15];
  int8_t adjusted_x = (int8_t)reg[25];
  int8_t adjusted_y = (int8_t)reg[8];

  gzm_record_oca_input(pad, adjusted_x, adjusted_y);
}

static void oca_sync_hook(void)
{
  if (!gz.ready)
    return;

  gzm_record_oca_sync((uint32_t)reg[4]);
}

static void room_load_hook(void)
{
  if (!gz.ready)
    return;

  if ((int32_t)reg[2] == -1)
    gzm_record_room_load();
}

static void hook(void (*func)(void))
{
  static int eax, ebx, ecx, edx, esp, ebp, esi, edi;

  mov_m32_reg32((unsigned long*)&eax, EAX);
  mov_m32_reg32((unsigned long*)&ebx, EBX);
  mov_m32_reg32((unsigned long*)&ecx, ECX);
  mov_m32_reg32((unsigned long*)&edx, EDX);
  mov_m32_reg32((unsigned long*)&esp, ESP);
  mov_m32_reg32((unsigned long*)&ebp, EBP);
  mov_m32_reg32((unsigned long*)&esi, ESI);
  mov_m32_reg32((unsigned long*)&edi, EDI);

  mov_reg32_imm32(EAX, (unsigned long)func);
  call_reg32(EAX);

  mov_reg32_m32(EAX, (unsigned long*)&eax);
  mov_reg32_m32(EBX, (unsigned long*)&ebx);
  mov_reg32_m32(ECX, (unsigned long*)&ecx);
  mov_reg32_m32(EDX, (unsigned long*)&edx);
  mov_reg32_m32(ESP, (unsigned long*)&esp);
  mov_reg32_m32(EBP, (unsigned long*)&ebp);
  mov_reg32_m32(ESI, (unsigned long*)&esi);
  mov_reg32_m32(EDI, (unsigned long*)&edi);
}

void gzm_hook(precomp_instr *instr)
{
  switch (instr->addr) {
#if Z64_VERSION == Z64_OOT10
    case 0x800A16B4 : hook(input_hook);      break;
    case 0x8009AC80 : hook(seed_hook);       break;
    case 0x800C1F78 : hook(oca_input_hook);  break;
    case 0x800C2ED0 : hook(oca_sync_hook);   break;
    case 0x80080BD8 : hook(room_load_hook);  break;
#elif Z64_VERSION == Z64_OOT11
    case 0x800A16BC : hook(input_hook);      break;
    case 0x8009AC90 : hook(seed_hook);       break;
    case 0x800C1FC8 : hook(oca_input_hook);  break;
    case 0x800C2F2C : hook(oca_sync_hook);   break;
    case 0x80080BD8 : hook(room_load_hook);  break;
#endif
    default         :                        break;
  }
}

#include "vector.c"
