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

#pragma once

#include "studio.h"

typedef struct ProfileMarker ProfileMarker;

struct ProfileMarker
{
	u32 hash;
	u32 color;
	u32 ref;
	u16 len;
	char* name;
	ProfileMarker* next;
};

typedef struct ProfileScope ProfileScope;

struct ProfileScope
{
	ProfileMarker* marker;
	u64 start;
	u64 end;
	ProfileScope* parent;
	ProfileScope* child;
	ProfileScope* sibling;
};

typedef struct ProfileFrame ProfileFrame;

struct ProfileFrame
{
	u32 frame;
	u64 start;
	u64 end;
	ProfileScope root;
	ProfileScope* current;
};

typedef struct ProfileData ProfileData;

struct ProfileData
{
	u32 frame;
	u32 idx;
	u32 selectedFrame;
	ProfileScope* selectedScope;
	ProfileFrame frames[PROFILE_FRAMES];
	ProfileMarker* markers;
	ProfileScope* scopePool;
	ProfileMarker* markerPool;
};

typedef struct Profiler Profiler;

struct Profiler
{
	tic_mem* tic;

	ProfileData data;

	void(*tick)(Profiler* profiler);
	void(*beginFrame)(Profiler* profiler);
	void(*endFrame)(Profiler* profiler);
	void(*beginScope)(Profiler* profiler, const char* name, u8 color);
	void(*endScope)(Profiler* profiler);
};

void initProfiler(Profiler* profiler, tic_mem* tic);

