// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "profiler.h"
#include "machine.h"
#include "zlib.h"

#define PADDING 2
#define FRAME_GRAPH_HEIGHT 30
#define SCOPE_GRAPH_HEIGHT 60
#define MEM_GRAPH_HEIGHT 30

static void drawProfilerToolbar(Profiler* profiler)
{
	profiler->tic->api.rect(profiler->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));
	drawToolbar(profiler->tic, TIC_COLOR_BG, false);
}

static void drawFrameGraph(Profiler* profiler)
{
	tic_machine* machine = (tic_machine*)profiler->tic;
	tic_perf* perf = &profiler->tic->perf;

	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = FRAME_GRAPH_HEIGHT;
	const u64 freq = machine->data->freq();
	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	if (mx > x+1 && mx < x+w-2 && my > y && my < y+h)
	{
		const u32 selected = (perf->idx + TIC_PERF_FRAMES + ((mx - x) / 6) + 1) % TIC_PERF_FRAMES;

		if (selected != profiler->data.selected)
			profiler->data.selected = selected;
	}

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		const u32 idx = (perf->idx + TIC_PERF_FRAMES - f) % TIC_PERF_FRAMES;
		tic_perf_frame* frame = &perf->frames[idx];

		if (frame->start == 0 || frame->end == 0)
			continue;

		const u64 dt = ((frame->end - frame->start) * 1000) / freq;
		const u64 bh = SDL_max(SDL_min(dt, h), 3);
		const u32 bx = x+w-(f*6)-1;
		const u32 by = y+(h-bh)-1;
		const u8 c1 = dt <= 16 ? tic_color_light_green : tic_color_red;
		const u8 c2 = dt <= 16 ? tic_color_green : tic_color_dark_red;

		profiler->tic->api.rect(profiler->tic, bx, by, 6, bh, c1);
		profiler->tic->api.rect_border(profiler->tic, bx, by, 6, bh, c2);

		if (idx == profiler->data.selected)
			profiler->tic->api.rect_border(profiler->tic, bx, y+1, 6, h-1, tic_color_white);
	}

	tic_perf_frame* frame = &perf->frames[profiler->data.selected];

	if (frame->start == 0 || frame->end == 0)
		return;

	const u64 dt = ((frame->end - frame->start) * 1000) / freq;

	char buffer[32];

	SDL_snprintf(buffer, 32, "TIME: %u MS", (u32)dt);
	profiler->tic->api.text(profiler->tic, buffer, x + 2, y + 2, tic_color_white);

}

static void drawFrameScopes(Profiler* profiler)
{
	tic_perf* perf = &profiler->tic->perf;

	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + FRAME_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = SCOPE_GRAPH_HEIGHT;
	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));

	const tic_perf_frame* frame = &perf->frames[profiler->data.selected];

	if (frame->start == 0 || frame->end == 0)
		return;

	const u64 frametime = frame->end - frame->start;
	const double time2pixels = (double)frametime / (w - 2);
	const u32 maxdepth = (h-2) / 6;
	u32 depth = 0;

	for (u32 i = 0; i < frame->scope_count; ++i)
	{
		const tic_perf_scope* scope = &frame->scopes[i];

		if (scope->parent != TIC_PERF_ROOT_SCOPE)
			continue;

		if (depth < maxdepth && scope->start != 0 && scope->end != 0)
		{
			const u64 scopetime = scope->end - scope->start;
			const u32 scopex = (u32)((scope->start - frame->start) / time2pixels) + x + 1;
			const u32 scopey = y+1+((depth-1)*6);
			const u32 scopewidth = (u32)(scopetime / time2pixels);

			profiler->tic->api.rect(profiler->tic, scopex, scopey, SDL_max(scopewidth, 1), 6, tic_color_blue);

			if (scopewidth > TIC_FONT_WIDTH+2)
			{
				const u32 maxlen = (scopewidth-2) / TIC_FONT_WIDTH;
				char* name = scope->marker->name;
				static char buffer[32];

				if (scope->marker->len > maxlen)
				{
					SDL_strlcpy(buffer, name, SDL_min(maxlen+1, 32));
					name = buffer;
				}

				profiler->tic->api.text(profiler->tic, name, scopex+1, scopey, tic_color_white);
			}

			if (mx >=  scopex && mx < scopex+scopewidth && my >= scopey && my < scopey+6)
			{
				profiler->tic->api.rect_border(profiler->tic, scopex, scopey, SDL_max(scopewidth, 1), 6, tic_color_white);
				/* TODO: draw popup with scope stats */
			}
		}
	}
}

static void drawMemoryUsage(Profiler* profiler)
{
	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + FRAME_GRAPH_HEIGHT + PADDING + SCOPE_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = MEM_GRAPH_HEIGHT;

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));
}

static void tick(Profiler* profiler)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()));

	profiler->tic->api.clear(profiler->tic, (tic_color_gray));

	drawFrameGraph(profiler);
	drawProfilerToolbar(profiler);
	drawFrameScopes(profiler);
	drawMemoryUsage(profiler);
}

void initProfiler(Profiler* profiler, tic_mem* tic)
{
	*profiler = (Profiler)
	{
		.tic = tic,
		.tick = tick,
		.data =
		{
			.selected = 0,
		},
	};
}
