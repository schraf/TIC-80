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

#include "map.h"
#include "history.h"

#define SHEET_COLS (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE)

#define MAP_WIDTH (TIC80_WIDTH)
#define MAP_HEIGHT (TIC80_HEIGHT - TOOLBAR_SIZE)
#define MAP_X (0)
#define MAP_Y (TOOLBAR_SIZE)

#define MAX_SCROLL_X (TIC_MAP_WIDTH * TIC_SPRITESIZE)
#define MAX_SCROLL_Y (TIC_MAP_HEIGHT * TIC_SPRITESIZE)

#define ICON_SIZE 7

#define MIN_SCALE 1
#define MAX_SCALE 4
#define FILL_STACK_SIZE (TIC_MAP_WIDTH*TIC_MAP_HEIGHT)

static void normalizeMap(s32* x, s32* y)
{
	while(*x < 0) *x += MAX_SCROLL_X;
	while(*y < 0) *y += MAX_SCROLL_Y;
	while(*x >= MAX_SCROLL_X) *x -= MAX_SCROLL_X;
	while(*y >= MAX_SCROLL_Y) *y -= MAX_SCROLL_Y;
}

static SDL_Point getTileOffset(Map* map)
{
	return (SDL_Point){(map->sheet.rect.w - 1)*TIC_SPRITESIZE / 2, (map->sheet.rect.h - 1)*TIC_SPRITESIZE / 2};
}

static void getMouseMap(Map* map, s32* x, s32* y)
{
	SDL_Point offset = getTileOffset(map);

	s32 mx = getMouseX() + map->scroll.x - offset.x;
	s32 my = getMouseY() + map->scroll.y - offset.y;

	normalizeMap(&mx, &my);

	*x = mx / TIC_SPRITESIZE;
	*y = my / TIC_SPRITESIZE;
}

static s32 drawWorldButton(Map* map, s32 x, s32 y)
{
	static const u8 WorldIcon[] =
	{
		0b00000000,
		0b00011100,
		0b00100010,
		0b01001001,
		0b00100010,
		0b00011100,
		0b00000000,
		0b00000000,
	};

	enum{Size = 8};

	x -= Size;

	SDL_Rect rect = {x, y, Size, ICON_SIZE};

	bool over = false;

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		over = true;

		showTooltip("WORLD MAP [tab]");

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			setStudioMode(TIC_WORLD_MODE);
	}

	drawBitIcon(x, y, WorldIcon, over ? (tic_color_dark_gray) : (tic_color_light_blue));

	return x;

}

static s32 drawGridButton(Map* map, s32 x, s32 y)
{
	static const u8 GridIcon[] =
	{
		0b00000000,
		0b00101000,
		0b01111100,
		0b00101000,
		0b01111100,
		0b00101000,
		0b00000000,
		0b00000000,
	};

	x -= ICON_SIZE;

	SDL_Rect rect = {x, y, ICON_SIZE, ICON_SIZE};

	bool over = false;

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		over = true;

		showTooltip("SHOW/HIDE GRID [`]");

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
			map->canvas.grid = !map->canvas.grid;
	}

	drawBitIcon(x, y, GridIcon, map->canvas.grid ? (tic_color_black) : over ? (tic_color_dark_gray) : (tic_color_light_blue));

	return x;
}

static s32 drawSheetButton(Map* map, s32 x, s32 y)
{
	static const u8 DownIcon[] = 
	{
		0b00000000,
		0b00000000,
		0b01111100,
		0b00111000,
		0b00010000,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	static const u8 UpIcon[] = 
	{
		0b00000000,
		0b00000000,
		0b00010000,
		0b00111000,
		0b01111100,
		0b00000000,
		0b00000000,
		0b00000000,
	};

	x -= ICON_SIZE;

	SDL_Rect rect = {x, y, ICON_SIZE, ICON_SIZE};

	bool over = false;
	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		over = true;
		showTooltip("SHOW TILES [shift]");

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
		{
			map->sheet.show = !map->sheet.show;
		}
	}

	drawBitIcon(rect.x, rect.y, map->sheet.show ? UpIcon : DownIcon, over ? (tic_color_dark_gray) : (tic_color_light_blue));

	return x;
}

static s32 drawToolButton(Map* map, s32 x, s32 y, const u8* Icon, s32 width, const char* tip, s32 mode)
{
	x -= width;

	SDL_Rect rect = {x, y, width, ICON_SIZE};

	bool over = false;
	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		over = true;

		showTooltip(tip);

		if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
		{
			map->mode = mode;
		}
	}

	drawBitIcon(rect.x, rect.y, Icon, map->mode == mode ? (tic_color_black) : over ? (tic_color_dark_gray) : (tic_color_light_blue));

	return x;
}

static s32 drawFillButton(Map* map, s32 x, s32 y)
{
	static const u8 Icon[] = 
	{
		0b00000000,
		0b00001000,
		0b00000100,
		0b00111110,
		0b01011100,
		0b01001000,
		0b00000000,
		0b00000000,
	};

	enum{Size = 8};

	return drawToolButton(map, x, y, Icon, Size, "FILL [4]", MAP_FILL_MODE);
}

static s32 drawSelectButton(Map* map, s32 x, s32 y)
{
	static const u8 Icon[] = 
	{
		0b00000000,
		0b01010100,
		0b00000000,
		0b01000100,
		0b00000000,
		0b01010100,
		0b00000000,
		0b00000000,
	};

	return drawToolButton(map, x, y, Icon, ICON_SIZE, "SELECT [3]", MAP_SELECT_MODE);
}

static s32 drawHandButton(Map* map, s32 x, s32 y)
{
	static const u8 Icon[] = 
	{
		0b00000000,
		0b00011000,
		0b00011100,
		0b01011100,
		0b00111100,
		0b00011000,
		0b00000000,
		0b00000000,
	};

	return drawToolButton(map, x, y, Icon, ICON_SIZE, "DRAG MAP [2]", MAP_DRAG_MODE);
}

static s32 drawPenButton(Map* map, s32 x, s32 y)
{
	static const u8 Icon[] = 
	{
		0b00000000,
		0b00001000,
		0b00010100,
		0b00101000,
		0b01010000,
		0b01100000,
		0b00000000,
		0b00000000,
	};

	return drawToolButton(map, x, y, Icon, ICON_SIZE, "DRAW [1]", MAP_DRAW_MODE);
}

static void drawTileIndex(Map* map, s32 x, s32 y)
{
	s32 index = -1;

	if(map->sheet.show)
	{
		SDL_Rect rect = {TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, TOOLBAR_SIZE, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};
		
		if(checkMousePos(&rect))
		{
			s32 mx = getMouseX() - rect.x;
			s32 my = getMouseY() - rect.y;

			mx /= TIC_SPRITESIZE;
			my /= TIC_SPRITESIZE;

			index = mx + my * SHEET_COLS;
		}
	}
	else
	{
		SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

		if(checkMousePos(&rect))
		{
			s32 tx = 0, ty = 0;
			getMouseMap(map, &tx, &ty);
			index = map->tic->api.map_get(map->tic, &map->tic->cart.gfx, tx, ty);
		}
	}

	if(index >= 0)
	{
		char buf[] = "#999";
		sprintf(buf, "#%03i", index);
		map->tic->api.text(map->tic, buf, x, y, (tic_color_light_blue));
	}
}

static void drawMapToolbar(Map* map, s32 x, s32 y)
{
	map->tic->api.rect(map->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

	drawTileIndex(map, TIC80_WIDTH/2 - TIC_FONT_WIDTH, y);

	x = drawSheetButton(map, TIC80_WIDTH, 0);
	x = drawFillButton(map, x, 0);
	x = drawSelectButton(map, x, 0);
	x = drawHandButton(map, x, 0);
	x = drawPenButton(map, x, 0);

	x = drawGridButton(map, x - 5, 0);
	x = drawWorldButton(map, x, 0);
}

static void drawSheet(Map* map, s32 x, s32 y)
{
	if(!map->sheet.show)return;

	SDL_Rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

	map->tic->api.rect_border(map->tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, (tic_color_white));

	if(checkMousePos(&rect))
	{
		setCursor(SDL_SYSTEM_CURSOR_HAND);

		if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
		{
			s32 mx = getMouseX() - rect.x;
			s32 my = getMouseY() - rect.y;

			mx /= TIC_SPRITESIZE;
			my /= TIC_SPRITESIZE;

			if(map->sheet.drag)
			{
				s32 rl = SDL_min(mx, map->sheet.start.x);
				s32 rt = SDL_min(my, map->sheet.start.y);
				s32 rr = SDL_max(mx, map->sheet.start.x);
				s32 rb = SDL_max(my, map->sheet.start.y);

				map->sheet.rect = (SDL_Rect){rl, rt, rr-rl+1, rb-rt+1};

				map->mode = MAP_DRAW_MODE;
			}
			else
			{
				map->sheet.drag = true;
				map->sheet.start = (SDL_Point){mx, my};
			}
		}
		else
		{
			if(map->sheet.drag)
				map->sheet.show = false;
			
			map->sheet.drag = false;
		}
	}

	for(s32 j = 0, index = 0; j < rect.h; j += TIC_SPRITESIZE)
		for(s32 i = 0; i < rect.w; i += TIC_SPRITESIZE, index++)
			map->tic->api.sprite(map->tic, &map->tic->cart.gfx, index, x + i, y + j, NULL, 0);

	{
		s32 bx = map->sheet.rect.x * TIC_SPRITESIZE - 1 + x;
		s32 by = map->sheet.rect.y * TIC_SPRITESIZE - 1 + y;
		s32 bw = map->sheet.rect.w * TIC_SPRITESIZE + 2;
		s32 bh = map->sheet.rect.h * TIC_SPRITESIZE + 2;

		map->tic->api.rect_border(map->tic, bx, by, bw, bh, (tic_color_white));
	}
}

static void drawCursorPos(Map* map, s32 x, s32 y)
{
	char pos[] = "999:999";

	s32 tx = 0, ty = 0;
	getMouseMap(map, &tx, &ty);

	sprintf(pos, "%03i:%03i", tx, ty);

	s32 width = map->tic->api.text(map->tic, pos, TIC80_WIDTH, 0, (tic_color_gray));

	s32 px = x + (TIC_SPRITESIZE + 3);
	if(px + width >= TIC80_WIDTH) px = x - (width + 2);

	s32 py = y - (TIC_FONT_HEIGHT + 2);
	if(py <= TOOLBAR_SIZE) py = y + (TIC_SPRITESIZE + 3);

	map->tic->api.rect(map->tic, px - 1, py - 1, width + 1, TIC_FONT_HEIGHT + 1, (tic_color_white));
	map->tic->api.text(map->tic, pos, px, py, (tic_color_light_blue));
}

static void setMapSprite(Map* map, s32 x, s32 y)
{
	s32 mx = map->sheet.rect.x;
	s32 my = map->sheet.rect.y;

	for(s32 j = 0; j < map->sheet.rect.h; j++)
		for(s32 i = 0; i < map->sheet.rect.w; i++)
			map->tic->api.map_set(map->tic, &map->tic->cart.gfx, (x+i)%TIC_MAP_WIDTH, (y+j)%TIC_MAP_HEIGHT, (mx+i) + (my+j) * SHEET_COLS);

	history_add(map->history);
}

static void drawTileCursor(Map* map)
{
	if(map->scroll.active)
		return;

	SDL_Point offset = getTileOffset(map);

	s32 mx = getMouseX() + map->scroll.x - offset.x;
	s32 my = getMouseY() + map->scroll.y - offset.y;

	mx -= mx % TIC_SPRITESIZE;
	my -= my % TIC_SPRITESIZE;

	mx += -map->scroll.x;
	my += -map->scroll.y;

	s32 width = map->sheet.rect.w * TIC_SPRITESIZE + 2;
	s32 height = map->sheet.rect.h * TIC_SPRITESIZE + 2;

	map->tic->api.rect_border(map->tic, mx - 1, my - 1, 
		width, height, (tic_color_white));

	{
		s32 sx = map->sheet.rect.x;
		s32 sy = map->sheet.rect.y;

		for(s32 j = 0, ty=my; j < map->sheet.rect.h; j++, ty+=TIC_SPRITESIZE)
			for(s32 i = 0, tx=mx; i < map->sheet.rect.w; i++, tx+=TIC_SPRITESIZE)
				map->tic->api.sprite(map->tic, &map->tic->cart.gfx, (sx+i) + (sy+j) * SHEET_COLS, tx, ty, NULL, 0);					
	}

	drawCursorPos(map, mx, my);
}

static void processMouseDrawMode(Map* map)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	setCursor(SDL_SYSTEM_CURSOR_HAND);

	drawTileCursor(map);

	if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
	{
		s32 tx = 0, ty = 0;
		getMouseMap(map, &tx, &ty);

		if(map->canvas.draw)
		{
			s32 w = tx - map->canvas.start.x;
			s32 h = ty - map->canvas.start.y;

			if(w % map->sheet.rect.w == 0 && h % map->sheet.rect.h == 0)
				setMapSprite(map, tx, ty);
		}
		else
		{
			map->canvas.draw	= true;
			map->canvas.start = (SDL_Point){tx, ty};
		}
	}
	else
	{
		map->canvas.draw	= false;
	}

	if(checkMouseDown(&rect, SDL_BUTTON_MIDDLE))
	{
		s32 tx = 0, ty = 0;
		getMouseMap(map, &tx, &ty);
		s32 index = map->tic->api.map_get(map->tic, &map->tic->cart.gfx, tx, ty);

		map->sheet.rect = (SDL_Rect){index % SHEET_COLS, index / SHEET_COLS, 1, 1};
	}
}

static void processScrolling(Map* map, bool pressed)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	if(map->scroll.active)
	{
		if(pressed)
		{
			map->scroll.x = map->scroll.start.x - getMouseX();
			map->scroll.y = map->scroll.start.y - getMouseY();

			normalizeMap(&map->scroll.x, &map->scroll.y);

			setCursor(SDL_SYSTEM_CURSOR_HAND);
		}
		else map->scroll.active = false;
	}
	else if(checkMousePos(&rect))
	{
		if(pressed)
		{
			map->scroll.active = true;

			map->scroll.start.x = getMouseX() + map->scroll.x;
			map->scroll.start.y = getMouseY() + map->scroll.y;
		}
	}
}

static void processMouseDragMode(Map* map)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	processScrolling(map, checkMouseDown(&rect, SDL_BUTTON_LEFT) || 
		checkMouseDown(&rect, SDL_BUTTON_RIGHT));
}

static void resetSelection(Map* map)
{
	map->select.rect = (SDL_Rect){0,0,0,0};
}

static void drawSelectionRect(Map* map, s32 x, s32 y, s32 w, s32 h)
{
	enum{Step = 3};
	u8 color = (tic_color_white);

	s32 index = map->tickCounter / 10;
	for(s32 i = x; i < (x+w); i++) 		{map->tic->api.pixel(map->tic, i, y, index++ % Step ? color : 0);} index++;
	for(s32 i = y; i < (y+h); i++) 		{map->tic->api.pixel(map->tic, x + w-1, i, index++ % Step ? color : 0);} index++;
	for(s32 i = (x+w-1); i >= x; i--) 	{map->tic->api.pixel(map->tic, i, y + h-1, index++ % Step ? color : 0);} index++;
	for(s32 i = (y+h-1); i >= y; i--) 	{map->tic->api.pixel(map->tic, x, i, index++ % Step ? color : 0);}
}

static void drawPasteData(Map* map)
{
	s32 w = map->paste[0];
	s32 h = map->paste[1];

	u8* data = map->paste + 2;

	s32 mx = getMouseX() + map->scroll.x - (w - 1)*TIC_SPRITESIZE / 2;
	s32 my = getMouseY() + map->scroll.y - (h - 1)*TIC_SPRITESIZE / 2;

	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
	{
		normalizeMap(&mx, &my);

		mx /= TIC_SPRITESIZE;
		my /= TIC_SPRITESIZE;

		for(s32 j = 0; j < h; j++)
			for(s32 i = 0; i < w; i++)
				map->tic->api.map_set(map->tic, &map->tic->cart.gfx, (mx+i)%TIC_MAP_WIDTH, (my+j)%TIC_MAP_HEIGHT, data[i + j * w]);

		history_add(map->history);

		SDL_free(map->paste);
		map->paste = NULL;
	}
	else
	{
		mx -= mx % TIC_SPRITESIZE;
		my -= my % TIC_SPRITESIZE;

		mx += -map->scroll.x;
		my += -map->scroll.y;

		drawSelectionRect(map, mx-1, my-1, w*TIC_SPRITESIZE+2, h*TIC_SPRITESIZE+2);
		
		for(s32 j = 0; j < h; j++)
			for(s32 i = 0; i < w; i++)
				map->tic->api.sprite(map->tic, &map->tic->cart.gfx, data[i + j * w], mx + i*TIC_SPRITESIZE, my + j*TIC_SPRITESIZE, NULL, 0);
	}
}

static void normalizeMapRect(s32* x, s32* y)
{
	while(*x < 0) *x += TIC_MAP_WIDTH;
	while(*y < 0) *y += TIC_MAP_HEIGHT;
	while(*x >= TIC_MAP_WIDTH) *x -= TIC_MAP_WIDTH;
	while(*y >= TIC_MAP_HEIGHT) *y -= TIC_MAP_HEIGHT;
}

static void processMouseSelectMode(Map* map)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	if(checkMousePos(&rect))
	{
		if(map->paste)
			drawPasteData(map);
		else
		{
			if(checkMouseDown(&rect, SDL_BUTTON_LEFT))
			{
				s32 mx = getMouseX() + map->scroll.x;
				s32 my = getMouseY() + map->scroll.y;

				mx /= TIC_SPRITESIZE;
				my /= TIC_SPRITESIZE;

				normalizeMapRect(&mx, &my);

				if(map->select.drag)
				{
					s32 rl = SDL_min(mx, map->select.start.x);
					s32 rt = SDL_min(my, map->select.start.y);
					s32 rr = SDL_max(mx, map->select.start.x);
					s32 rb = SDL_max(my, map->select.start.y);

					map->select.rect = (SDL_Rect){rl, rt, rr - rl + 1, rb - rt + 1};
				}
				else
				{
					map->select.drag = true;
					map->select.start = (SDL_Point){mx, my};
					map->select.rect = (SDL_Rect){map->select.start.x, map->select.start.y, 1, 1};
				}
			}
			else if(map->select.drag)
			{
				map->select.drag = false;

				if(map->select.rect.w <= 1 && map->select.rect.h <= 1)
					resetSelection(map);
			}
		}
	}
}

typedef struct
{
	SDL_Point* data;
	SDL_Point* head;
} FillStack;

static bool push(FillStack* stack, s32 x, s32 y)
{
	if(stack->head == NULL)
	{
		stack->head = stack->data;
		stack->head->x = x;
		stack->head->y = y;

		return true;
	}

	if(stack->head < (stack->data + FILL_STACK_SIZE-1))
	{		
		stack->head++;
		stack->head->x = x;
		stack->head->y = y;

		return true;
	}

	return false;
}

static bool pop(FillStack* stack, s32* x, s32* y)
{
	if(stack->head > stack->data)
	{
		*x = stack->head->x;
		*y = stack->head->y;

		stack->head--;

		return true;
	}

	if(stack->head == stack->data)
	{
		*x = stack->head->x;
		*y = stack->head->y;

		stack->head = NULL;

		return true;
	}

	return false;
}

static void fillMap(Map* map, s32 x, s32 y, u8 tile)
{
	if(tile == (map->sheet.rect.x + map->sheet.rect.y * SHEET_COLS)) return;

	static FillStack stack = {NULL, NULL};

	if(!stack.data)
		stack.data = (SDL_Point*)SDL_malloc(FILL_STACK_SIZE * sizeof(SDL_Point));

	stack.head = NULL;

	static const s32 dx[4] = {0, 1, 0, -1};
	static const s32 dy[4] = {-1, 0, 1, 0};

	if(!push(&stack, x, y)) return;

	s32 mx = map->sheet.rect.x;
	s32 my = map->sheet.rect.y;

	struct
	{
		s32 l;
		s32 t;
		s32 r;
		s32 b;
	}clip = { 0, 0, TIC_MAP_WIDTH, TIC_MAP_HEIGHT };

	if (map->select.rect.w > 0 && map->select.rect.h > 0)
	{
		clip.l = map->select.rect.x;
		clip.t = map->select.rect.y;
		clip.r = map->select.rect.x + map->select.rect.w;
		clip.b = map->select.rect.y + map->select.rect.h;
	}

	while(pop(&stack, &x, &y))
	{
		for(s32 j = 0; j < map->sheet.rect.h; j++)
			for(s32 i = 0; i < map->sheet.rect.w; i++)
				map->tic->api.map_set(map->tic, &map->tic->cart.gfx, x+i, y+j, (mx+i) + (my+j) * SHEET_COLS);

		for(s32 i = 0; i < COUNT_OF(dx); i++) 
		{
			s32 nx = x + dx[i]*map->sheet.rect.w;
			s32 ny = y + dy[i]*map->sheet.rect.h;

			if(nx >= clip.l && nx < clip.r && ny >= clip.t && ny < clip.b) 
			{
				bool match = true;
				for(s32 j = 0; j < map->sheet.rect.h; j++)
					for(s32 i = 0; i < map->sheet.rect.w; i++)
						if(map->tic->api.map_get(map->tic, &map->tic->cart.gfx, nx+i, ny+j) != tile)
							match = false;

				if(match)
				{
					if(!push(&stack, nx, ny)) return;
				}
			}
		}
	}
}

static void processMouseFillMode(Map* map)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	setCursor(SDL_SYSTEM_CURSOR_HAND);

	drawTileCursor(map);

	if(checkMouseClick(&rect, SDL_BUTTON_LEFT))
	{
		s32 tx = 0, ty = 0;
		getMouseMap(map, &tx, &ty);

		fillMap(map, tx, ty, map->tic->api.map_get(map->tic, &map->tic->cart.gfx, tx, ty));
		history_add(map->history);
	}
}

static void drawSelection(Map* map)
{
	SDL_Rect* sel = &map->select.rect;

	if(sel->w > 0 && sel->h > 0)
	{
		s32 x = sel->x * TIC_SPRITESIZE - map->scroll.x;
		s32 y = sel->y * TIC_SPRITESIZE - map->scroll.y;
		s32 w = sel->w * TIC_SPRITESIZE;
		s32 h = sel->h * TIC_SPRITESIZE;

		while(x+w<0)x+=MAX_SCROLL_X;
		while(y+h<0)y+=MAX_SCROLL_Y;
		while(x+w>=MAX_SCROLL_X)x-=MAX_SCROLL_X;
		while(y+h>=MAX_SCROLL_Y)y-=MAX_SCROLL_Y;

		drawSelectionRect(map, x-1, y-1, w+2, h+2);
	}
}

static void drawGrid(Map* map)
{
	s32 scrollX = map->scroll.x % TIC_SPRITESIZE;
	s32 scrollY = map->scroll.y % TIC_SPRITESIZE;

	for(s32 j = -scrollY; j <= TIC80_HEIGHT-scrollY; j += TIC_SPRITESIZE)
	{
		if(j >= 0 && j < TIC80_HEIGHT)
			for(s32 i = 0; i < TIC80_WIDTH; i++)
			{
				s32 index = i + j*TIC80_WIDTH;
				u8 color = tic_tool_peek4(map->tic->ram.vram.screen.data, index);
				tic_tool_poke4(map->tic->ram.vram.screen.data, index, (color+1)%TIC_PALETTE_SIZE);
			}
	}

	for(s32 j = -scrollX; j <= TIC80_WIDTH-scrollX; j += TIC_SPRITESIZE)
	{
		if(j >= 0 && j < TIC80_WIDTH)
			for(s32 i = 0; i < TIC80_HEIGHT; i++)
			{
				if((i+scrollY) % TIC_SPRITESIZE)
				{
					s32 index = j + i*TIC80_WIDTH;
					u8 color = tic_tool_peek4(map->tic->ram.vram.screen.data, index);
					tic_tool_poke4(map->tic->ram.vram.screen.data, index, (color+1)%TIC_PALETTE_SIZE);						
				}
			}
	}
}

static void drawMap(Map* map)
{
	SDL_Rect rect = {MAP_X, MAP_Y, MAP_WIDTH, MAP_HEIGHT};

	s32 scrollX = map->scroll.x % TIC_SPRITESIZE;
	s32 scrollY = map->scroll.y % TIC_SPRITESIZE;

	map->tic->api.map(map->tic, &map->tic->cart.gfx, map->scroll.x / TIC_SPRITESIZE, map->scroll.y / TIC_SPRITESIZE, 
		TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, -scrollX, -scrollY, 0, 1);

	if(map->canvas.grid || map->scroll.active)
		drawGrid(map);

	{
		s32 screenScrollX = map->scroll.x % TIC80_WIDTH;
		s32 screenScrollY = map->scroll.y % TIC80_HEIGHT;

		map->tic->api.line(map->tic, 0, TIC80_HEIGHT - screenScrollY, TIC80_WIDTH, TIC80_HEIGHT - screenScrollY, (tic_color_gray));
		map->tic->api.line(map->tic, TIC80_WIDTH - screenScrollX, 0, TIC80_WIDTH - screenScrollX, TIC80_HEIGHT, (tic_color_gray));
	}

	if(!map->sheet.show && checkMousePos(&rect))
	{
		if(getKeyboard()[SDL_SCANCODE_SPACE])
		{
			processScrolling(map, checkMouseDown(&rect, SDL_BUTTON_LEFT) || checkMouseDown(&rect, SDL_BUTTON_RIGHT));
		}
		else
		{
			static void(*const Handlers[])(Map*) = {processMouseDrawMode, processMouseDragMode, processMouseSelectMode, processMouseFillMode};

			Handlers[map->mode](map);

			if(map->mode != MAP_DRAG_MODE)
				processScrolling(map, checkMouseDown(&rect, SDL_BUTTON_RIGHT));
		}
	}

	drawSelection(map);
}

static void processKeyboard(Map* map)
{
	enum{Step = 1};

	if(getKeyboard()[SDL_SCANCODE_UP]) map->scroll.y -= Step;
	if(getKeyboard()[SDL_SCANCODE_DOWN]) map->scroll.y += Step;
	if(getKeyboard()[SDL_SCANCODE_LEFT]) map->scroll.x -= Step;
	if(getKeyboard()[SDL_SCANCODE_RIGHT]) map->scroll.x += Step;

	static const u8 Scancodes[] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};

	for(s32 i = 0; i < COUNT_OF(Scancodes); i++)
		if(getKeyboard()[Scancodes[i]])
		{
			normalizeMap(&map->scroll.x, &map->scroll.y);
			break;
		}
}

static void undo(Map* map)
{
	history_undo(map->history);
}

static void redo(Map* map)
{
	history_redo(map->history);
}

static void copySelectionToClipboard(Map* map)
{
	SDL_Rect* sel = &map->select.rect;

	if(sel->w > 0 && sel->h > 0)
	{	
		s32 size = sel->w * sel->h + 2;
		u8* buffer = SDL_malloc(size);

		if(buffer)
		{
			buffer[0] = sel->w;
			buffer[1] = sel->h;

			u8* ptr = buffer + 2;

			for(s32 j = sel->y; j < sel->y+sel->h; j++)
				for(s32 i = sel->x; i < sel->x+sel->w; i++)
				{
					s32 x = i, y = j;
					normalizeMapRect(&x, &y);

					s32 index = x + y * TIC_MAP_WIDTH;
					*ptr++ = map->tic->cart.gfx.map.data[index];
				}		

			toClipboard(buffer, size, true);
			SDL_free(buffer);			
		}
	}
}

static void copyToClipboard(Map* map)
{
	copySelectionToClipboard(map);
	resetSelection(map);
}

static void deleteSelection(Map* map)
{
	SDL_Rect* sel = &map->select.rect;

	if(sel->w > 0 && sel->h > 0)
	{
		for(s32 j = sel->y; j < sel->y+sel->h; j++)
			for(s32 i = sel->x; i < sel->x+sel->w; i++)
			{
				s32 x = i, y = j;
				normalizeMapRect(&x, &y);

				s32 index = x + y * TIC_MAP_WIDTH;
				map->tic->cart.gfx.map.data[index] = 0;
			}

		history_add(map->history);
	}
}

static void cutToClipboard(Map* map)
{
	copySelectionToClipboard(map);
	deleteSelection(map);
	resetSelection(map);
}

static void copyFromClipboard(Map* map)
{
	if(SDL_HasClipboardText())
	{
		char* clipboard = SDL_GetClipboardText();

		if(clipboard)
		{
			s32 size = strlen(clipboard)/2;

			if(size > 2)
			{
				u8* data = SDL_malloc(size);

				str2buf(clipboard, strlen(clipboard), data, true);

				if(data[0] * data[1] == size - 2)
				{
					map->paste = data;
					map->mode = MAP_SELECT_MODE;
				}
				else SDL_free(data);
			}

			SDL_free(clipboard);
		}
	}
}

static void processKeydown(Map* map, SDL_Keycode keycode)
{
	SDL_Keymod keymod = SDL_GetModState();

	switch(getClipboardEvent(keycode))
	{
	case TIC_CLIPBOARD_CUT: cutToClipboard(map); break;
	case TIC_CLIPBOARD_COPY: copyToClipboard(map); break;
	case TIC_CLIPBOARD_PASTE: copyFromClipboard(map); break;
	default: break;
	}
	
	if(keymod & TIC_MOD_CTRL)
	{
		switch(keycode)
		{
		case SDLK_z: undo(map); break;
		case SDLK_y: redo(map); break;
		}
	}
	else
	{
		switch(keycode)
		{
		case SDLK_TAB: setStudioMode(TIC_WORLD_MODE); break;
		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
			map->mode = keycode - SDLK_1;
			break;
		case SDLK_DELETE:
			deleteSelection(map);
			break;
		case SDLK_BACKQUOTE:
			map->canvas.grid = !map->canvas.grid;
			break;
		}
	}

	if(keymod & KMOD_SHIFT)
		map->sheet.show = true;
}

static void processKeyup(Map* map, SDL_Keycode keycode)
{
	SDL_Keymod keymod = SDL_GetModState();

	if(!(keymod & KMOD_SHIFT))
		map->sheet.show = false;
}

static void processGesture(Map* map)
{
	SDL_Point point = {0, 0};

	if(getGesturePos(&point))
	{
		if(map->scroll.gesture)
		{
			map->scroll.x = map->scroll.start.x - point.x;
			map->scroll.y = map->scroll.start.y - point.y;

			normalizeMap(&map->scroll.x, &map->scroll.y);
		}
		else
		{
			map->scroll.start.x = point.x + map->scroll.x;
			map->scroll.start.y = point.y + map->scroll.y;
			map->scroll.gesture = true;
		}
	}
	else map->scroll.gesture = false;
}

static void tick(Map* map)
{
	map->tickCounter++;

	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_KEYDOWN:
			processKeydown(map, event->key.keysym.sym);
			break;
		case SDL_KEYUP:
			processKeyup(map, event->key.keysym.sym);
			break;
		}
	}

	processKeyboard(map);
	processGesture(map);

	map->tic->api.clear(map->tic, TIC_COLOR_BG);

	drawMap(map);
	drawSheet(map, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, TOOLBAR_SIZE);
	drawMapToolbar(map, TIC80_WIDTH - 9*TIC_FONT_WIDTH, 1);
	drawToolbar(map->tic, TIC_COLOR_BG, false);
}

static void onStudioEvent(Map* map, StudioEvent event)
{
	switch(event)
	{
	case TIC_TOOLBAR_CUT: cutToClipboard(map); break;
	case TIC_TOOLBAR_COPY: copyToClipboard(map); break;
	case TIC_TOOLBAR_PASTE: copyFromClipboard(map); break;
	case TIC_TOOLBAR_UNDO: undo(map); break;
	case TIC_TOOLBAR_REDO: redo(map); break;
	default: break;
	}
}

static void scanline(tic_mem* tic, s32 row)
{
	memcpy(tic->ram.vram.palette.data, row < TOOLBAR_SIZE ? tic->config.palette.data : tic->cart.palette.data, sizeof(tic_palette));
}

void initMap(Map* map, tic_mem* tic)
{
	if(map->history) history_delete(map->history);
	
	*map = (Map)
	{
		.tic = tic,
		.tick = tick,
		.mode = MAP_DRAW_MODE,
		.canvas = 
		{
			.grid = true,
			.draw = false,
			.start = {0, 0},
		},
		.sheet = 
		{
			.show = false,
			.rect = {0, 0, 1, 1},
			.start = {0, 0},
			.drag = false,
		},
		.select = 
		{
			.rect = {0, 0, 0, 0},
			.start = {0, 0},
			.drag = false,
		},
		.paste = NULL,
		.tickCounter = 0,
		.scroll = 
		{
			.x = 0, 
			.y = 0, 
			.active = false,
			.gesture = false,
			.start = {0, 0},
		},
		.history = history_create(&tic->cart.gfx.map, sizeof tic->cart.gfx.map),
		.event = onStudioEvent,
		.scanline = scanline,
	};

	normalizeMap(&map->scroll.x, &map->scroll.y);
}