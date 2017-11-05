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
#include "zlib.h"

#define SAMPLES_PER_FRAME 39
#define PADDING 2
#define FRAME_GRAPH_HEIGHT 30
#define SCOPE_GRAPH_HEIGHT 60
#define MARKER_DATA_HEIGHT 30

static u32 hashString(const char* string)
{
	const size_t len = SDL_strlen(string);
	return adler32(0, (const Bytef*)string, len);
}

ProfileMarker* getProfileMarker(Profiler* profiler, const char* name)
{
	const u32 hash = hashString(name);
	ProfileMarker* marker = profiler->data.markers;

	while (marker != NULL)
	{
		if (marker->hash == hash)
		{
			marker->ref++;
			return marker;
		}

		marker = marker->next;
	}

	if (profiler->data.markerPool != NULL)
	{
		marker = profiler->data.markerPool;
		profiler->data.markerPool = marker->next;
	}
	else
	{
		marker = (ProfileMarker*)SDL_malloc(sizeof(ProfileMarker));
	}

	marker->hash = hash;
	marker->color = 0;
	marker->ref = 1;
	marker->len = SDL_strlen(name);
	marker->name = SDL_strdup(name);
	marker->next = profiler->data.markers;

	return marker;
}

static void drawProfilerToolbar(Profiler* profiler)
{
	profiler->tic->api.rect(profiler->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));
	drawToolbar(profiler->tic, TIC_COLOR_BG, false);
}

static void drawFrameGraph(Profiler* profiler)
{
	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = FRAME_GRAPH_HEIGHT;
	const u64 freq = SDL_GetPerformanceFrequency();
	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	if (mx > x+1 && mx < x+w-2 && my > y && my < y+h)
	{
		const u32 selected = (profiler->data.idx + PROFILE_FRAMES + ((mx - x) / 6) + 1) % PROFILE_FRAMES;

		if (selected != profiler->data.selectedFrame)
		{
			profiler->data.selectedFrame = selected;
			profiler->data.selectedScope = NULL;
		}
	}

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));

	for (u32 f = 1; f < PROFILE_FRAMES; ++f)
	{
		const u32 idx = (profiler->data.idx + PROFILE_FRAMES - f) % PROFILE_FRAMES;
		ProfileFrame* frame = &profiler->data.frames[idx];

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

		if (idx == profiler->data.selectedFrame)
			profiler->tic->api.rect_border(profiler->tic, bx, y+1, 6, h-1, tic_color_white);
	}

	ProfileFrame* frame = &profiler->data.frames[profiler->data.selectedFrame];

	if (frame->start == 0 || frame->end == 0)
		return;

	const u64 dt = ((frame->end - frame->start) * 1000) / freq;

	char buffer[32];

	SDL_snprintf(buffer, 32, "FRAME: %u", frame->frame);
	profiler->tic->api.text(profiler->tic, buffer, x + 2, y + 2, tic_color_white);

	SDL_snprintf(buffer, 32, "TIME: %u MS", (u32)dt);
	profiler->tic->api.text(profiler->tic, buffer, x + 2, y + 2 + TIC_FONT_HEIGHT + 1, tic_color_white);

}

static void drawFrameScopes(Profiler* profiler)
{
	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + FRAME_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = SCOPE_GRAPH_HEIGHT;
	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));

	ProfileFrame* frame = &profiler->data.frames[profiler->data.selectedFrame];

	if (frame->start == 0 || frame->end == 0)
		return;

	const u64 frametime = frame->end - frame->start;
	const double time2pixels = (double)frametime / (w - 2);
	const u32 maxdepth = (h-2) / 6;
	u32 depth = 0;

	ProfileScope* scope = &frame->root;

	while (scope != NULL)
	{
		if (scope->child != NULL)
		{
			scope = scope->child;
			depth++;
		}
		else if (scope->sibling != NULL)
		{
			scope = scope->sibling;
		}
		else if (scope->parent != NULL && scope->parent != &frame->root)
		{
			scope = scope->parent;
			depth--;
		}
		else
		{
			break;
		}

		if (scope != NULL && depth < maxdepth && scope->start != 0 && scope->end != 0)
		{
			const u64 scopetime = scope->end - scope->start;
			const u32 scopex = (u32)((scope->start - frame->start) / time2pixels) + x + 1;
			const u32 scopey = y+1+((depth-1)*6);
			const u32 scopewidth = (u32)(scopetime / time2pixels);

			if (mx >=  scopex && mx < scopex+scopewidth && my >= scopey && my < scopey+6)
				profiler->data.selectedScope = scope;

			profiler->tic->api.rect(profiler->tic, scopex, scopey, SDL_max(scopewidth, 1), 6, scope->marker->color);

			if (profiler->data.selectedScope == scope)
				profiler->tic->api.rect_border(profiler->tic, scopex, scopey, SDL_max(scopewidth, 1), 6, tic_color_white);

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
		}
	}
}

static void drawMarkerData(Profiler* profiler)
{
	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + FRAME_GRAPH_HEIGHT + PADDING + SCOPE_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = MARKER_DATA_HEIGHT;

	profiler->tic->api.rect(profiler->tic, x, y, w, h, (tic_color_black));
	profiler->tic->api.rect_border(profiler->tic, x, y, w, h, (tic_color_white));

	ProfileFrame* frame = &profiler->data.frames[profiler->data.selectedFrame];

	if (frame->start == 0 || frame->end == 0)
		return;

	/*
	ProfileScope* scope = profiler->data.selectedScope;

	if (scope != NULL)
	{
		const u64 scopetime = ((scope->end - scope->start) * 1000000) / freq;

		SDL_snprintf(buffer, 32, "%s: %u US", scope->marker->name, (u32)scopetime);
		profiler->tic->api.text(profiler->tic, buffer, x + 2, y + 2 + 2 * (TIC_FONT_HEIGHT + 1), tic_color_white);
	}
	*/
}

static void tick(Profiler* profiler)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()));

	profiler->tic->api.clear(profiler->tic, (tic_color_gray));

	drawFrameGraph(profiler);
	drawProfilerToolbar(profiler);
	drawFrameScopes(profiler);
	drawMarkerData(profiler);
}

static void freeProfileScope(Profiler* profiler, ProfileScope* scope)
{
	if (scope == profiler->data.selectedScope)
		profiler->data.selectedScope = NULL;

	scope->marker->ref--;

	if (scope->marker->ref == 0)
	{
		SDL_free(scope->marker->name);
		scope->marker->name = NULL;
		scope->marker->next = profiler->data.markerPool;
		profiler->data.markerPool = scope->marker;
		scope->marker = NULL;
	}

	if (scope->child != NULL)
		freeProfileScope(profiler, scope->child);

	if (scope->sibling != NULL)
		freeProfileScope(profiler, scope->sibling);

	scope->parent = profiler->data.scopePool;
	profiler->data.scopePool = scope;
}

static void beginFrame(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;

	profiler->data.frame++;
	profiler->data.idx = profiler->data.frame % PROFILE_FRAMES;
	
	ProfileFrame* frame = &profiler->data.frames[profiler->data.idx];

	ProfileScope* scope = frame->root.child;

	while (scope != NULL)
	{
		if (scope->child != NULL)
		{
			scope = scope->child;
		}
		else if (scope->sibling != NULL)
		{
			scope = scope->sibling;
		}
		else
		{
			ProfileScope* next = scope->parent;
			freeProfileScope(profiler, scope);
			scope = next;
		}

		if (scope == &frame->root)
			break;
	}

	frame->frame = profiler->data.frame;
	frame->start = SDL_GetPerformanceCounter();
	frame->end = 0;
	frame->current = &frame->root;

	frame->root.marker = NULL;
	frame->root.start = frame->start;
	frame->root.end = 0;
	frame->root.parent = NULL;
	frame->root.child = NULL;
	frame->root.sibling = NULL;
}

static void endFrame(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;

	ProfileFrame* frame = &profiler->data.frames[profiler->data.idx];
	frame->end = SDL_GetPerformanceCounter();
	profiler->data.selectedFrame = profiler->data.idx;
}

static void beginScope(Profiler* profiler, const char* name, u8 color)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;

	ProfileFrame* frame = &profiler->data.frames[profiler->data.idx];

	if (frame->start == 0 || frame->end != 0)
		return;

	ProfileMarker* marker = getProfileMarker(profiler, name);
	marker->color = color;

	ProfileScope* scope = NULL;

	if (profiler->data.scopePool != NULL)
	{
		scope = profiler->data.scopePool;
		profiler->data.scopePool = scope->parent;
	}
	else
	{
		scope = (ProfileScope*)SDL_malloc(sizeof(ProfileScope));
	}

	scope->marker = marker;
	scope->start = SDL_GetPerformanceCounter();
	scope->end = 0;
	scope->parent = frame->current;
	scope->child = NULL;
	scope->sibling = scope->parent->child;
	scope->parent->child = scope;
	frame->current = scope;
}

static void endScope(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;

	ProfileFrame* frame = &profiler->data.frames[profiler->data.idx];

	if (frame->start == 0 || frame->end != 0)
		return;

	ProfileScope* scope = frame->current;
	scope->end = SDL_GetPerformanceCounter();

	if (scope->parent != NULL) // only skipped for root
		frame->current = scope->parent;
}

void initProfiler(Profiler* profiler, tic_mem* tic)
{
	*profiler = (Profiler)
	{
		.tic = tic,
		.tick = tick,
		.data =
		{
			.frame = 0,
			.idx = 0,
			.selectedFrame = 0,
			.selectedScope = NULL,
			.markers = NULL,
			.scopePool = NULL,
			.markerPool = NULL,
		},
		.beginFrame = beginFrame,
		.endFrame = endFrame,
		.beginScope = beginScope,
		.endScope = endScope,
	};

	SDL_zero(profiler->data.frames);
}

