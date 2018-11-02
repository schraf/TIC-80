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

#include "debugger.h"
#include "machine.h"
#include "zlib.h"

#include <string.h>

#define PADDING 2

#define PROFILER_FRAME_WIDTH 6
#define PROFILER_FRAME_GRAPH_HEIGHT 30
#define PROFILER_SCOPE_GRAPH_HEIGHT 60
#define PROFILER_MEM_GRAPH_HEIGHT 30

typedef struct
{
	char buffer[64];
	bool visible;
} Tooltip;

typedef struct 
{
	tic_perf_frame* frame;
	u64 us;
	u64 ms;
	u32 index;
	u32 x;
	u32 w;
	bool valid;
	bool selected;
} FrameInfo;

static FrameInfo* getPerfFrame(tic_machine* machine, u32 offset)
{
	static FrameInfo info;

	tic_perf* perf = &machine->memory.perf;
	const u64 freq = machine->data->freq();
	const s32 mouseX = getMouseX();

	info.index = (perf->idx + TIC_PERF_FRAMES - offset) % TIC_PERF_FRAMES;
	info.frame = &perf->frames[info.index];
	info.x = TIC80_WIDTH - PADDING - (offset * PROFILER_FRAME_WIDTH) - 1;
	info.w = PROFILER_FRAME_WIDTH;
	info.valid = info.frame->start != 0 && info.frame->end != 0;
	info.us = ((info.frame->end - info.frame->start) * 1000000) / freq;
	info.ms = info.us / 1000;
	info.selected = mouseX >= info.x && mouseX < info.x + info.w;

	return &info;
}

static void drawTooltip(tic_mem* tic, const Tooltip* tooltip)
{
	if (!tooltip->visible)
		return;

	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	if (mx <= PADDING || mx >= TIC80_WIDTH - PADDING)
		return;

	const s32 width = tic->api.text(tic, tooltip->buffer, TIC80_WIDTH, 0, (tic_color_gray), true);

	s32 x = mx;
	s32 y = my - TIC_FONT_HEIGHT;

	if(x + width >= TIC80_WIDTH) x -= (width + 2);

	tic->api.rect(tic, x - 1, y - 1, width + 1, TIC_FONT_HEIGHT + 1, (tic_color_black));
	tic->api.text(tic, tooltip->buffer, x, y, (tic_color_white), true);
}

static void drawModeTabs(Debugger* debugger)
{
	static const u8 Icons[] =
	{
		0b00000000,
		0b01000000,
		0b01001010,
		0b01010100,
		0b01100000,
		0b01111110,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01011011,
		0b00000000,
		0b01011011,
		0b00000000,
		0b01011011,
		0b00000000,
		0b00000000,

		0b00000000,
		0b00111100,
		0b00100110,
		0b00100010,
		0b00100010,
		0b00111110,
		0b00000000,
		0b00000000,
	};

	enum { Width = 9, Height = 7, Rows = 8, Count = sizeof Icons / Rows };

	for (s32 i = 0; i < Count; i++)
	{
		tic_rect rect = { TIC80_WIDTH - Width * (Count - i), 0, Width, Height };

		bool over = false;

		static const s32 Tabs[] = { DBG_PROFILER_TAB, DBG_STATS_TAB, DBG_LOG_TAB };

		if (checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			over = true;

			static const char* Tooltips[] = { "PROFILER [tab]", "STATS [tab]", "LOG [tab]" };
			showTooltip(Tooltips[i]);

			if (checkMouseClick(&rect, tic_mouse_left))
				debugger->tab = Tabs[i];
		}

		if (debugger->tab == Tabs[i])
			debugger->tic->api.rect(debugger->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));

		drawBitIcon(rect.x, rect.y, Icons + i*Rows, (debugger->tab == Tabs[i] ? tic_color_white : over ? tic_color_dark_gray : tic_color_light_blue));
	}
}

static void drawDebuggerToolbar(Debugger* debugger)
{
	debugger->tic->api.rect(debugger->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));
	drawModeTabs(debugger);
	drawToolbar(debugger->tic, TIC_COLOR_BG, false);
}

static void drawProfilerFrameGraph(Debugger* debugger)
{
	tic_machine* machine = (tic_machine*)debugger->tic;
	tic_perf* perf = &debugger->tic->perf;

	const tic_rect rect = { PADDING, TOOLBAR_SIZE + PADDING, TIC80_WIDTH - (PADDING*2), PROFILER_FRAME_GRAPH_HEIGHT };
	const bool activeGraph = checkMousePos(&rect);
	
	debugger->tic->api.rect_border(debugger->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_white));
	debugger->tic->api.rect(debugger->tic, rect.x+1, rect.y+1, 19, TIC_FONT_HEIGHT+1, (tic_color_black));
	debugger->tic->api.text(debugger->tic, "CPU", rect.x+2, rect.y+2, (tic_color_white), false);

	if (machine->data == NULL)
		return;

	Tooltip tooltip;
	tooltip.visible = false;

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		FrameInfo* info = getPerfFrame(machine, f);

		if (!info->valid)
			continue;

		const u64 h = max(min(info->ms, rect.h-2), 3);

		const tic_rect frameRect = { info->x, rect.y+(rect.h-h)-1, info->w, h };

		const u8 c1 = info->ms <= 16 ? tic_color_light_green : tic_color_red;
		const u8 c2 = info->ms <= 16 ? tic_color_green : tic_color_dark_red;

		debugger->tic->api.rect(debugger->tic, frameRect.x, frameRect.y, frameRect.w, frameRect.h, c1);
		debugger->tic->api.rect_border(debugger->tic, frameRect.x, frameRect.y, frameRect.w, frameRect.h, c2);

		if (activeGraph && info->selected)
		{
			tooltip.visible = true;

			if (info->us < 1000)
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u uS", info->us);
			else
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u MS", info->ms);

			debugger->tic->api.rect_border(debugger->tic, frameRect.x, frameRect.y, frameRect.w, frameRect.h, tic_color_white);
		}
	}

	drawTooltip(debugger->tic, &tooltip);
}

static u32 getProfilerScopeDepth(const tic_perf_frame* frame, const tic_perf_scope* scope)
{
	if (scope->parent == TIC_PERF_INVALID_SCOPE)
		return 0;

	u32 depth = 1;

	while (scope->parent != TIC_PERF_ROOT_SCOPE)
	{
		depth++;
		scope = &frame->scopes[scope->parent];
	}

	return depth;
}

static void drawProfilerFrameScopes(Debugger* debugger)
{
	tic_machine* machine = (tic_machine*)debugger->tic;
	tic_perf* perf = &debugger->tic->perf;

	const tic_rect rect = { PADDING, TOOLBAR_SIZE + PADDING + 2*(PROFILER_FRAME_GRAPH_HEIGHT + PADDING), TIC80_WIDTH - (PADDING * 2), PROFILER_SCOPE_GRAPH_HEIGHT };
	const bool activeGraph = checkMousePos(&rect);

	debugger->tic->api.rect_border(debugger->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_white));

	if (machine->data == NULL)
		return;

	FrameInfo* info = getPerfFrame(machine, 1);

	if (!info->valid)
		return;

	Tooltip tooltip;
	tooltip.visible = false;

	const u64 frametime = info->frame->end - info->frame->start;
	const double time2pixels = (double)frametime / (rect.w - 2);
	const u32 maxdepth = (rect.h-2) / 8;

	for (u32 i = 0; i < info->frame->scope_count; ++i)
	{
		const tic_perf_scope* scope = &info->frame->scopes[i];
		const u32 depth = getProfilerScopeDepth(info->frame, scope);

		if (depth > 0 && depth < maxdepth && scope->start != 0 && scope->end != 0)
		{
			const u64 scopetime = scope->end - scope->start;
			const u32 scopex = (u32)((scope->start - info->frame->start) / time2pixels) + rect.x + 1;
			const u32 scopey = rect.y+1+((depth-1)*8);
			const u32 scopewidth = (u32)(scopetime / time2pixels);

			debugger->tic->api.rect(debugger->tic, scopex, scopey, max(scopewidth, 1), 8, tic_color_blue);
			debugger->tic->api.rect_border(debugger->tic, scopex, scopey, max(scopewidth, 1), 8, tic_color_dark_blue);

			if (scopewidth > TIC_FONT_WIDTH+2)
			{
				const u32 maxlen = (scopewidth-2) / TIC_FONT_WIDTH;
				char* name = scope->marker->name;
				static char buffer[32];

				if (scope->marker->len > maxlen)
				{
					strncpy(buffer, name, min(maxlen+1, 32));
					name = buffer;
				}

				debugger->tic->api.text(debugger->tic, name, scopex+1, scopey+1, tic_color_white, true);
			}

			/*
			if (mx >= scopex && mx < scopex+scopewidth && my >= scopey && my < scopey+6)
			{
				debugger->tic->api.rect_border(debugger->tic, scopex, scopey, max(scopewidth, 1), 8, tic_color_white);

				tooltip.visible = true;

				const u64 dt = ((scope->end - scope->start) * 1000000) / freq;

				if (dt < 1000)
					snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u uS", (u32)dt);
				else
					snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u MS", (u32)(dt / 1000));
			}
			*/
		}
	}

	drawTooltip(debugger->tic, &tooltip);
}

static void drawProfilerMemoryUsage(Debugger* debugger)
{
	tic_machine* machine = (tic_machine*)debugger->tic;
	tic_perf* perf = &debugger->tic->perf;

	const tic_rect rect = { PADDING, TOOLBAR_SIZE + PADDING + PROFILER_FRAME_GRAPH_HEIGHT + PADDING, TIC80_WIDTH - (PADDING * 2), PROFILER_FRAME_GRAPH_HEIGHT };
	const bool activeGraph = checkMousePos(&rect);

	debugger->tic->api.rect_border(debugger->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_white));
	debugger->tic->api.rect(debugger->tic, rect.x + 1, rect.y + 1, 19, TIC_FONT_HEIGHT + 1, (tic_color_black));
	debugger->tic->api.text(debugger->tic, "MEM", rect.x + 2, rect.y + 2, (tic_color_white), false);

	if (machine->data == NULL)
		return;

	Tooltip tooltip;
	tooltip.visible = false;

	u32 maxmem = 0;
	u32 maxallocs = 0;

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		FrameInfo* info = getPerfFrame(machine, f);

		if (!info->valid)
			continue;

		maxmem = max(info->frame->mem_usage, maxmem);
		maxallocs = max(info->frame->mem_allocs, maxallocs);
	}

	const u32 bytesPerPixel = max(1, maxmem / (PROFILER_MEM_GRAPH_HEIGHT - 2));
	const u32 allocsPerPixel = max(1, maxallocs / (PROFILER_MEM_GRAPH_HEIGHT - 2));
	u32 lastMemUsage = perf->frames[perf->idx].mem_usage;
	u32 lastAllocs = perf->frames[perf->idx].mem_allocs;

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		FrameInfo* info = getPerfFrame(machine, f);

		if (!info->valid)
			continue;

		const u32 memHeight = maxmem == 0 ? 1 : max(1, info->frame->mem_usage / bytesPerPixel);
		const u32 lastMemHeight = maxmem == 0 ? 1 : max(1, lastMemUsage / bytesPerPixel);

		const tic_point memStart = { info->x, rect.y + (rect.h - memHeight) - 1 };	
		const tic_point memEnd = { info->x + info->w, rect.y + (rect.h - lastMemHeight) - 1 };

		const u32 allocsHeight = maxallocs == 0 ? 1 : max(1, info->frame->mem_allocs / allocsPerPixel);
		const u32 lastAllocsHeight = maxallocs == 0 ? 1 : max(1, lastAllocs / allocsPerPixel);

		const tic_point allocsStart = { info->x, rect.y + (rect.h - allocsHeight) - 1 };
		const tic_point allocsEnd = { info->x + info->w, rect.y + (rect.h - lastAllocsHeight) - 1 };

		debugger->tic->api.line(debugger->tic, memStart.x, memStart.y, memEnd.x, memEnd.y, tic_color_yellow);
		debugger->tic->api.line(debugger->tic, allocsStart.x, allocsStart.y, allocsEnd.x, allocsEnd.y, tic_color_light_blue);

		lastMemUsage = info->frame->mem_usage;
		lastAllocs = info->frame->mem_allocs;

		if (activeGraph && info->selected)
		{
			tooltip.visible = true;

			if (info->frame->mem_usage < 1024)
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u B", info->frame->mem_usage);
			else if (info->frame->mem_usage < 1024*1024)
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u KB", info->frame->mem_usage / 1024);
			else
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u MB", info->frame->mem_usage / 1024*1024);
		}
	}

	drawTooltip(debugger->tic, &tooltip);
}
static void processKeyboard(Debugger* debugger)
{
	if (keyWasPressed(tic_key_tab))
	{
		switch (debugger->tab)
		{
		case DBG_PROFILER_TAB: debugger->tab = DBG_STATS_TAB; break;
		case DBG_STATS_TAB: debugger->tab = DBG_LOG_TAB; break;
		case DBG_LOG_TAB: debugger->tab = DBG_PROFILER_TAB; break;
		}
	}
}

static void profilerTick(Debugger* debugger)
{
	drawProfilerFrameGraph(debugger);
	drawProfilerMemoryUsage(debugger);
	drawProfilerFrameScopes(debugger);
}

static void statsTick(Debugger* debugger)
{
}

static void logTick(Debugger* debugger)
{
}

static void tick(Debugger* debugger)
{
	processKeyboard(debugger);

	debugger->tic->api.clear(debugger->tic, TIC_COLOR_BG);

	drawDebuggerToolbar(debugger);

	switch(debugger->tab)
	{
	case DBG_PROFILER_TAB: profilerTick(debugger); break;
	case DBG_STATS_TAB: statsTick(debugger); break;
	case DBG_LOG_TAB: logTick(debugger); break;
	}
}

void initDebugger(Debugger* debugger, tic_mem* tic, tic_debugger* src)
{
	*debugger = (Debugger)
	{
		.tic = tic,
		.src = src,
		.tab = DBG_PROFILER_TAB,
		.tick = tick,
	};
}

