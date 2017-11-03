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

#define SAMPLES_PER_FRAME 

static void tick(Profiler* profiler)
{
}

static void beginFrame(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;
}

static void endFrame(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;
}

static void beginScope(Profiler* profiler, const char* name)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;
}

static void endScope(Profiler* profiler)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;
}

void initProfiler(Profiler* profiler, tic_mem* tic)
{
	*profiler = (Profiler)
	{
		.tic = tic,
		.tick = tick,
                .data =
                {
                    frame = 0,
                    scopePool = NULL,
                },
		.beginFrame = beginFrame,
		.endFrame = endFrame,
		.beginScope = beginScope,
		.endScope = endScope,
	};
}

