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

#define PROFILER_FRAME_GRAPH_HEIGHT 30
#define PROFILER_SCOPE_GRAPH_HEIGHT 60
#define PROFILER_MEM_GRAPH_HEIGHT 30

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))

typedef struct Tooltip Tooltip;

struct Tooltip
{
	char buffer[64];
	bool visible;
};

static void drawTooltip(tic_mem* tic, const Tooltip* tooltip)
{
	if (!tooltip->visible)
		return;

	const s32 mx = getMouseX();
	const s32 my = getMouseY();

	if (mx <= PADDING || mx >= TIC80_WIDTH - PADDING)
		return;

	const s32 width = tic->api.text(tic, tooltip->buffer, TIC80_WIDTH, 0, (tic_color_gray), false);

	s32 x = mx;
	s32 y = my - TIC_FONT_HEIGHT;

	if(x + width >= TIC80_WIDTH) x -= (width + 2);

	tic->api.rect(tic, x - 1, y - 1, width + 1, TIC_FONT_HEIGHT + 1, (tic_color_black));
	tic->api.text(tic, tooltip->buffer, x, y, (tic_color_white), false);
}

static void drawModeTabs(Debugger* debugger)
{
	static const u8 Icons[] =
	{
		0b00000000,
		0b00110000,
		0b01001001,
		0b01001001,
		0b01001001,
		0b00000110,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01000000,
		0b01000100,
		0b01010100,
		0b01010101,
		0b01010101,
		0b00000000,
		0b00000000,

		0b00000000,
		0b01000000,
		0b01000100,
		0b01010100,
		0b01010101,
		0b01010101,
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

	Tooltip tooltip;
	tooltip.visible = false;

//	if (checkMousePos(&rect))
//	{
//		const u32 selected = (perf->idx + TIC_PERF_FRAMES + ((mx - x) / 6) + 1) % TIC_PERF_FRAMES;
//
//		if (selected != debugger->src->selected)
//			debugger->src->selected = selected;
//	}
//
//	debugger->tic->api.rect_border(debugger->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_white));
//
//	if (machine->data == NULL)
//		return;
//
//	const u64 freq = machine->data->freq();
//
//	// we skip the active frame since it has not finished writing data to it
//	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
//	{
//		const u32 idx = (perf->idx + TIC_PERF_FRAMES - f) % TIC_PERF_FRAMES;
//		tic_perf_frame* frame = &perf->frames[idx];
//
//		if (frame->start == 0 || frame->end == 0)
//			continue;
//
//		const u64 us = ((frame->end - frame->start) * 1000000) / freq;
//		const u64 ms = us / 1000;
//		const u64 bh = max(min(ms, h-2), 3);
//		const u32 bx = x+w-(f*6)-1;
//		const u32 by = y+(h-bh)-1;
//		const u8 c1 = ms <= 16 ? tic_color_light_green : tic_color_red;
//		const u8 c2 = ms <= 16 ? tic_color_green : tic_color_dark_red;
//
//		debugger->tic->api.rect(debugger->tic, bx, by, 6, bh, c1);
//		debugger->tic->api.rect_border(debugger->tic, bx, by, 6, bh, c2);
//
//		if (my > y && my < y+h && idx == debugger->src->selected)
//		{
//			tooltip.visible = true;
//
//			if (us < 1000)
//				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u uS", (u32)us);
//			else
//				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u MS", (u32)ms);
//
//			debugger->tic->api.rect_border(debugger->tic, bx, y+1, 6, h-1, tic_color_white);
//		}
//	}
//
//	drawTooltip(debugger->tic, &tooltip);
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

	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + PROFILER_FRAME_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = PROFILER_SCOPE_GRAPH_HEIGHT;
	const s32 mx = getMouseX();
	const s32 my = getMouseY();
	const u64 freq = machine->data->freq();

	debugger->tic->api.rect(debugger->tic, x, y, w, h, (tic_color_black));
	debugger->tic->api.rect_border(debugger->tic, x, y, w, h, (tic_color_white));

	const tic_perf_frame* frame = &perf->frames[debugger->src->selected];

	if (frame->start == 0 || frame->end == 0)
		return;

	const u64 frametime = frame->end - frame->start;
	const double time2pixels = (double)frametime / (w - 2);
	const u32 maxdepth = (h-2) / 8;

	Tooltip tooltip;
	tooltip.visible = false;

	for (u32 i = 0; i < frame->scope_count; ++i)
	{
		const tic_perf_scope* scope = &frame->scopes[i];
		const u32 depth = getProfilerScopeDepth(frame, scope);

		if (depth > 0 && depth < maxdepth && scope->start != 0 && scope->end != 0)
		{
			const u64 scopetime = scope->end - scope->start;
			const u32 scopex = (u32)((scope->start - frame->start) / time2pixels) + x + 1;
			const u32 scopey = y+1+((depth-1)*8);
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

				debugger->tic->api.text(debugger->tic, name, scopex+1, scopey+1, tic_color_white, false);
			}

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
		}
	}

	drawTooltip(debugger->tic, &tooltip);
}

static void drawProfilerMemoryUsage(Debugger* debugger)
{
	tic_perf* perf = &debugger->tic->perf;

	const u32 x = PADDING;
	const u32 y = TOOLBAR_SIZE + PADDING + PROFILER_FRAME_GRAPH_HEIGHT + PADDING + PROFILER_SCOPE_GRAPH_HEIGHT + PADDING;
	const u32 w = TIC80_WIDTH - (2 * PADDING);
	const u32 h = PROFILER_MEM_GRAPH_HEIGHT;
	const s32 mx = getMouseX();
	const s32 my = getMouseY();
	u32 maxmem = 0;

	debugger->tic->api.rect(debugger->tic, x, y, w, h, (tic_color_black));
	debugger->tic->api.rect_border(debugger->tic, x, y, w, h, (tic_color_white));

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		const u32 idx = (perf->idx + TIC_PERF_FRAMES - f) % TIC_PERF_FRAMES;
		tic_perf_frame* frame = &perf->frames[idx];

		if (frame->start == 0 || frame->end == 0)
			continue;

		maxmem = max(frame->mem_usage, maxmem);
	}

	const u32 bytesPerPixel = maxmem / (PROFILER_MEM_GRAPH_HEIGHT - 2);
	u32 lastMemUsage = perf->frames[perf->idx].mem_usage;

	Tooltip tooltip;
	tooltip.visible = false;

	for (u32 f = 1; f < TIC_PERF_FRAMES; ++f)
	{
		const u32 idx = (perf->idx + TIC_PERF_FRAMES - f) % TIC_PERF_FRAMES;
		tic_perf_frame* frame = &perf->frames[idx];

		if (frame->start == 0 || frame->end == 0)
			continue;

		const u32 bh1 = maxmem == 0 ? 1 : max(1, frame->mem_usage / bytesPerPixel);
		const u32 bx1 = x+w-(f*6)-1;
		const u32 by1 = y+(h-bh1)-1;

		const u32 bh2 = maxmem == 0 ? 1 : max(1, lastMemUsage / bytesPerPixel);
		const u32 bx2 = x+w-(f*6)-1+6;
		const u32 by2 = y+(h-bh2)-1;

		debugger->tic->api.line(debugger->tic, bx1, by1, bx2, by2, tic_color_yellow);

		lastMemUsage = frame->mem_usage;

		if (mx >= bx1 && mx < bx2 && my > y && my < y+h)
		{
			tooltip.visible = true;

			if (frame->mem_usage < 1024)
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u B", frame->mem_usage);
			else if (frame->mem_usage < 1024*1024)
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u KB", frame->mem_usage / 1024);
			else
				snprintf(tooltip.buffer, sizeof(tooltip.buffer), "%u MB", frame->mem_usage / 1024*1024);
		}
	}

	drawTooltip(debugger->tic, &tooltip);
}

static void profilerTick(Debugger* debugger)
{
	drawProfilerFrameGraph(debugger);
	drawProfilerFrameScopes(debugger);
	drawProfilerMemoryUsage(debugger);
}

static void statsTick(Debugger* debugger)
{
}

static void logTick(Debugger* debugger)
{
}

static void tick(Debugger* debugger)
{
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

