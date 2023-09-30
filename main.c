#include <ctype.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h> 
#include <X11/extensions/Xinerama.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <utils.h>

#define MONITOR 2
#define WINDOW_SIZE 500

#define BG 0xF8F8F2
#define FG 0x282A36

typedef struct {
	Window   window;
	GC       graphical_context;
} Panel;

Panel* panels;

static Display* display;
static int      screen;

static XIM xim;
static XIC xic;

static char*  text = "\n\0";
static size_t text_size = 2;
static int cursor_y = 0;

static char**  texts;
static int  texts_size = 0;
static int*  texts_sizes;

typedef struct {
	int x;
	int y;
} Pos;

void print_array(char *buffer) {
	for (int i = 0; i < text_size; i++) {
		printf("%d, ", buffer[i]);
	}
	printf("\n");
}

void print_array_int(int *buffer, int n) {
	for (int i = 0; i < n; i++) {
		printf("%i, ", buffer[i]);
	}
	printf("\n");
}

int open_file() {
	FILE *file = fopen("/home/carlo/.local/share/trax.md", "r");

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		perror("Could not open file");
		return 1;
	}

	long bufsize = ftell(file);
	if (bufsize == -1) {
		perror("Could not open file");
		return 1;
	}

	text = malloc(sizeof(char) * (bufsize + 1));

	if (fseek(file, 0, SEEK_SET) != 0) {
		perror("Could not open file");
		return 1;
	}

	size_t newLen = fread(text, sizeof(char), bufsize, file);

	if (ferror(file) != 0) {
		fputs("Error reading file", stderr);
		return 1;
	} else {
		text[newLen++] = '\0';
	}

	text_size = newLen; 

	fclose(file);

	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#')
			texts_size++;
	}

	texts = malloc(sizeof(size_t) * texts_size);
	texts_sizes = malloc(sizeof(int) * texts_size);

	for (int i = 0; i < texts_size; i++) {
		texts_sizes[i] = 0;
	}

	int y = -1;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#')
			y++;

		texts_sizes[y]++;
	}

	for (int i = 0; i < texts_size; i++) {
		texts[i] = malloc(texts_sizes[i] + 1);
	}

	int x = 0;
	y = -1;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#') {
			if (y >= 0)
				texts[y][x] = '\0';
			y++;
			x = 0;
			texts[y][x] = text[i];
			continue;
		}
		texts[y][x] = text[i];
		x++;
	}

	return 0;
}

int save() {
	FILE *file;
	if ((file = fopen("/home/carlo/.local/share/trax.md", "w")) == NULL)
		return 1;

	fprintf(file, "%s", text);
	fclose(file);

	return 0;
}

void reset_panels() {
	panels = realloc(panels, sizeof(Panel) * texts_size);

	Window root_window = RootWindow(display, screen);

	XineramaScreenInfo* info;
	int number_of_screens;
	info = XineramaQueryScreens(display, &number_of_screens);

	// TODO: use argument to get the desired monitor
	int monitor_number = MONITOR;
	int x = info[monitor_number].x_org;
	int y = info[monitor_number].y_org;
	int width = info[monitor_number].width;

	XFree(info);

	for (int i = 0; i < texts_size; i++) {
		XSetWindowAttributes set_window_attributes;
		set_window_attributes.override_redirect = True;
		set_window_attributes.background_pixel = BG;
		set_window_attributes.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

		panels[i].window = XCreateWindow(display, root_window, x + i * (width - WINDOW_SIZE) / (texts_size > 1 ? texts_size - 1 : 1), y, WINDOW_SIZE, WINDOW_SIZE, 0, CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWEventMask, &set_window_attributes);

		XClassHint class_hint = {"trax", "trax"};
		XSetClassHint(display, panels[i].window, &class_hint);

		panels[i].graphical_context = XCreateGC(display, panels[i].window, 0,0);        

		xim = XOpenIM(display, NULL, NULL, NULL);

		xic = XCreateIC(
			xim,
			XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, root_window,
			XNFocusWindow, root_window,
			NULL
		);

		XClearWindow(display, panels[i].window);
		XMapRaised(display, panels[i].window);
	}
}

int init_x() {
	panels = malloc(sizeof(Panel) * texts_size);

	display = XOpenDisplay(NULL);
   	screen  = DefaultScreen(display);

	reset_panels();

	return 1;
};

int grab_keyboard() {
	if (XGrabKeyboard(display, DefaultRootWindow(display), True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
		return 1;
	return 0;
}

void free_panels() {
	for (int i = 0; i < texts_size; i++) {
		XFreeGC(display, panels[i].graphical_context);
		XDestroyWindow(display, panels[i].window);
	}
}

void close_x() {
	free_panels();
	free(panels);
	XCloseDisplay(display);	
	save();

	for (int i = 0; i < texts_size; i++) {
		free(texts[i]);
	}
	free(texts_sizes);
	free(text);
	exit(0);
};

void redraw() {
	for (int i = 0; i < texts_size; i++) {
		XClearWindow(display, panels[i].window);
	}
};

int _cursor_x(int cursor_y) {
	int cursor_x = 0;
	int y = 0;
	for (int i = 0; i < text_size; i++) {
		cursor_x++;
		if (text[i] == '\n') {
			if (y == cursor_y)
				return cursor_x - 2;
			cursor_x = 0;
			y++;
		}
	}

	printf("E01 / cursor_x: Imposible state detected! Missing \\n");
	close_x();

	return cursor_x;
}

int cursor_x() {
	return _cursor_x(cursor_y);
}

int all_number_of_lines() {
	int result = 0;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '\n')
			result++;
	}
	return result;
}

int number_of_lines(int window_index) {
	int result = 0;
	for (int i = 0; i < texts_sizes[window_index]; i++) {
		if (texts[window_index][i] == '\n')
			result++;
	}
	return result;
}

int _cursor_y(int cursor_y) {
	int y = 0;
	int result = 0;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#')
			result = 0;
		if (y == cursor_y)
			return result;
		if (text[i] == '\n') {
			y++;
			result++;
		}
	}

	return result;
}


int _cursor_index(int cursor_y) {
	int y = 0;
	int i = 0;
	for (; i < text_size; i++) {
		if (text[i] == '\n') {
			if (y == cursor_y)
				return i;
			y++;
		}
	}

	printf("E01 / cursor_index: Imposible state detected! Missing \\n");
	close_x();

	return i;
}

int cursor_index() {
	return _cursor_index(cursor_y);
}

void resize() {
	for (int i = 0; i < texts_size; i++) {
		XResizeWindow(display, panels[i].window, WINDOW_SIZE, number_of_lines(i) * 16 + 8);
	}
};

int cursor_window_index() {
	int result = -1;
	for (int i = 0; i < cursor_index(); i++) {
		if (text[i] == '#')
			result++;
	}

	return result;
}

void draw_cursor() {
	if (!strlen(text))
		return;

	int cursor_window = cursor_window_index();

	XFillRectangle(display, panels[cursor_window].window, panels[cursor_window].graphical_context, cursor_x() * 6 + 12, _cursor_y(cursor_y) * 16 + 6, 6, 11);

	if (strlen(text))
		XDrawRectangle(display, panels[cursor_window].window, panels[cursor_window].graphical_context, 2, (_cursor_y(cursor_y) + 1) * 16 - 12 - 1, 495, 18);
}

void draw_lines() {
	int x = 1;
	int y = 1;
	int w = -1;

	for (int i = 0; i < text_size; i++) {
		if (text[i] == '\n') {
			y++;
			x = 1;
			continue;
		}
		if (text[i] == '#') {
			w++;
			y = 1;
			x = 1;
		}
		if (!text[i])
			continue;
		XDrawString(display, panels[w].window, panels[w].graphical_context, x * 6, y * 16 + 1, &text[i], 1);
		x++;
	}
}

void reset_texts() {
	texts_size = 0;

	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#')
			texts_size++;
	}

	texts = realloc(texts, sizeof(size_t) * texts_size);
	texts_sizes = realloc(texts_sizes, sizeof(int) * texts_size);

	for (int i = 0; i < texts_size; i++) {
		texts_sizes[i] = 0;
	}

	int y = -1;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#')
			y++;

		texts_sizes[y]++;
	}

	for (int i = 0; i < texts_size; i++) {
		texts[i] = malloc(texts_sizes[i] + 1);
	}

	int x = 0;
	y = -1;
	for (int i = 0; i < text_size; i++) {
		if (text[i] == '#') {
			if (y >= 0)
				texts[y][x] = '\0';
			y++;
			x = 0;
			texts[y][x] = text[i];
			continue;
		}
		texts[y][x] = text[i];
		x++;
	}
}

void draw() {
	reset_texts();
	resize();
	for (int i = 0; i < texts_size; i++) {
		XClearWindow(display, panels[i].window);
		XSetForeground(display, panels[i].graphical_context, FG);
	}
	draw_lines();
	draw_cursor();
}

void insert_to_text(char c) {
	text = (char *) realloc(text, ++text_size * sizeof(char));
	int cursor_i = cursor_index();
	memmove(&text[cursor_i + 1], &text[cursor_i], text_size - cursor_i - 1);
	text[cursor_index()] = c;

	if (c == '\n')
		cursor_y = cursor_y < all_number_of_lines() ? cursor_y + 1 : cursor_y;

	if (c == '#')
		free_panels();

	reset_texts();

	if (c == '#')
		reset_panels();
}

void remove_from_text() {
	int cursor_i = cursor_index();
	if (text_size <= 3 || cursor_i <= 1) return;
	if (text[cursor_i - 1] == '\n') {
		cursor_y--;
	}

	char removed_char = text[cursor_i - 1];

	memcpy(&text[cursor_i - 1], &text[cursor_i], text_size - cursor_i);
	text = realloc(text, --text_size * sizeof(char));

	if (removed_char == '#')
		free_panels();

	reset_texts();

	if (removed_char == '#')
		reset_panels();
};

void on_key_press(XKeyEvent* event) {
	int len;
	KeySym key_symbol;
	Status status;
	char buffer[64];
	len = XmbLookupString(xic, event, buffer, sizeof(buffer), &key_symbol, &status);

	switch (key_symbol) {
	case XK_Escape:
		close_x();
		break;
	case XK_Up:
		cursor_y = cursor_y > 0 ? cursor_y - 1 : 0;
		break;
	case XK_Down:
		cursor_y = cursor_y < all_number_of_lines() - 1 ? cursor_y + 1 : cursor_y;
		break;
	case XK_BackSpace:
		remove_from_text();
		break;
	case XK_Return:
		insert_to_text('\n');
		break;
	default:
		if (!iscntrl((unsigned char) *buffer))
			insert_to_text(buffer[0]);
		break;
	}

	draw();
}

int main () {
	XEvent event;
	KeySym key;

	text = malloc(sizeof(char) * 2);
	text[0] = '\n';
	text[1] = '\0';

	if (open_file() == 1) {
		return 1;
	}
	if (!init_x())
		return 2;
	if (!grab_keyboard())
		return 3;

	while(1) {		
		XNextEvent(display, &event);

		if (event.type==Expose && event.xexpose.count==0) {
			redraw();
			draw();
		}
		if (event.type==KeyPress)
			on_key_press(&event.xkey);
	}
}
