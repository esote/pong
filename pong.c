/*
 * Pong is a digital tennis game.
 * Copyright (C) 2019  Esote
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define B_S	'o'
#define P_S	ACS_CKBOARD

#define DEFAULT_DELAY	50
#define PATH_MAX	4096

struct ball {
	size_t x;
	size_t y;
	int dx;
	int dy;
};

struct paddle {
	size_t x;
	size_t y;
};

static int	collision(size_t const, struct paddle const *const,
			size_t const, size_t const);
static void	update_b(size_t const, size_t const, size_t const,
			struct ball *const, struct paddle const *const,
			struct paddle const *const);
static void	update_auto_r(size_t const, size_t const,
			struct ball const *const, struct paddle *const);
static int	update_controlled(WINDOW *const, size_t const, size_t const,
			int const, struct paddle *const, struct paddle *const);
static char *	render(WINDOW *const, size_t const, size_t const, size_t const,
			struct ball const *const, struct paddle const *const,
			struct paddle const *const);
static void	save_score(void);

static size_t score_l;
static size_t score_r;

int
main(int argc, char *argv[])
{
	WINDOW *win;
	char *msg;
	struct ball b;
	struct paddle l;
	struct paddle r;
	size_t p_h;
	size_t h;
	size_t w;
	int delay;
	int manual_r;
	int opt;

	delay = DEFAULT_DELAY;
	manual_r = 0;

	while ((opt = getopt(argc, argv, "d:r")) != -1) {
		switch (opt) {
		case 'd':
			delay = atoi(optarg);
			break;
		case 'r':
			manual_r = 1;
			break;
		default:
			errx(1, "usage: pong [-r] [-d delay]");
		}
	}

	if (optind < argc) {
		fprintf(stderr, "%s: invalid option -- '%s'\n", argv[0],
			argv[optind]);
		errx(1, "usage: pong [-r] [-d delay]");
	}

	if (delay < 0) {
		errx(1, "invalid delay '%d'", delay);
	}

	msg = NULL;
	srand(time(NULL));

	win = initscr();

	if (win == NULL) {
		errx(1, "initwin");
	}

	if (refresh() == ERR) {
		errx(1, "first refresh");
	}

	if (curs_set(0) == ERR) {
		errx(1, "curs_set");
	}

	if (noecho() == ERR) {
		errx(1, "noecho");
	}

	if (raw() == ERR) {
		errx(1, "raw");
	}

	wtimeout(win, delay);
	getmaxyx(win, h, w);

	p_h = h >> 2;

	b = (struct ball) {
		.x = w >> 1,
		.y = h >> 1,
		.dx = 1,
		.dy = -1
	};

	l = (struct paddle) {
		.x = 4,
		.y = h >> 1
	};

	r = (struct paddle) {
		.x = w - 5,
		.y = h >> 1
	};

	do {
		getmaxyx(win, h, w);

		if (w < 32 || h < 8) {
			msg = "dimensions too small";
			break;
		}

		if (b.y == 0 || b.y > h - 1 || b.x == 0 || b.x > w - 1) {
			b.x = w >> 1;
			b.y = h >> 1;
		}

		r.x = w - 5;
		p_h = h >> 2;

		if (l.y + p_h >= h) {
			l.y = h - p_h - 1;
		}

		if (r.y + p_h >= h) {
			r.y = h - p_h - 1;
		}

		if (!update_controlled(win, h, p_h, manual_r, &l, &r)) {
			break;
		}

		update_b(h, w, p_h, &b, &l, &r);

		if (!manual_r) {
			update_auto_r(h, p_h, &b, &r);
		}
	} while ((msg = render(win, h, w, p_h, &b, &l, &r)) == NULL);

	save_score();

	if (delwin(win) == ERR) {
		errx(1, "delwin");
	}

	if (endwin() == ERR) {
		errx(1, "endwin");
	}

	if (msg != NULL) {
		errx(1, msg);
	}

	return 0;
}

static int
collision(size_t const p_h, struct paddle const *const p, size_t const f_x,
	size_t const f_y)
{
	return f_x == p->x && f_y >= p->y && f_y < p->y + p_h;
}

static void
update_b(size_t const h, size_t const w, size_t const p_h,
	struct ball *const b, struct paddle const *const l,
	struct paddle const *const r)
{
	size_t const f_x = b->x + b->dx;
	size_t const f_y = b->y + b->dy;

	if (collision(p_h, l, f_x, f_y) || collision(p_h, r, f_x, f_y)) {
		b->dx = -b->dx;
	} else if (f_x == 0) {
		score_r++;
		b->x = w - 4;
		b->y = rand() % (p_h - 1) + r->y;
		b->dx = -1;
	} else if (f_x == w - 1) {
		score_l++;
		b->x = 5;
		b->y = rand() % (p_h - 1) + l->y;
		b->dx = 1;
	}

	if (f_y == 0) {
		b->dy = 1;
	} else if (f_y == h - 1) {
		b->dy = -1;
	}

	b->x += b->dx;
	b->y += b->dy;
}

static void
update_auto_r(size_t const h, size_t const p_h, struct ball const *const b,
	struct paddle *const r)
{
	size_t f_by;
	size_t f_py;
	size_t d;

	f_by = b->y + b->y * (r->x - b->x);

	if (f_by >= h) {
		return;
	}

	d = r->y + (p_h >> 1);

	f_py = r->y;

	if (b->y > d) {
		f_py++;
	} else if (b->y < d) {
		f_py--;
	}

	if (f_py + p_h < h && f_py > 0) {
		r->y = f_py;
	}
}

static int
update_controlled(WINDOW *const win, size_t const h, size_t const p_h,
	int const manual, struct paddle *const l, struct paddle *const r)
{
	switch (wgetch(win)) {
	case 'w':
		if (l->y - 1 != 0) {
			l->y--;
		}
		break;
	case 's':
		if (l->y + p_h < h - 1) {
			l->y++;
		}
		break;
	case 'o':
		if (manual && r->y - 1 != 0) {
			r->y--;
		}
		break;
	case 'l':
		if (manual && r->y + p_h < h - 1) {
			r->y++;
		}
		break;
	case 'q':
		return 0;
	}

	return 1;
}

static char *
render(WINDOW *const win, size_t const h, size_t const w, size_t const p_h,
	struct ball const *const b, struct paddle const *const l,
	struct paddle const *const r)
{
	size_t i;

	if (score_l == SIZE_MAX || score_r == SIZE_MAX) {
		return "game over max score reached";
	}

	if (werase(win)) {
		return "werase";
	}

	(void)wborder(win, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD,
		ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD, ACS_CKBOARD);

	(void)mvwaddch(win, b->y, b->x, B_S);

	for (i = 1; i < h - 1; i += 2) {
		(void)mvwaddch(win, i, w >> 1, ACS_CKBOARD);
	}

	for (i = 0; i < p_h; ++i) {
		(void)mvwaddch(win, l->y + i, l->x, P_S);
		(void)mvwaddch(win, r->y + i, r->x, P_S);
	}

	(void)mvwprintw(win, 1, 6, "[left: %zu]", score_l);
	(void)mvwprintw(win, 1, (w >> 1) + 2, "[right: %zu]", score_r);

	if (score_l == 47988) {
		(void)mvwprintw(win, 0, 2, "[416c6c20476f6f64205468696e6773]");
	}

	(void)mvwprintw(win, h - 2, 6, "[quit q; up/down w/s]");

	if (wrefresh(win) == ERR) {
		return "wrefresh";
	}

	return NULL;
}

static void
save_score(void)
{
	FILE *scores;
	char const *home;
	char *pretty_now;
	char path[PATH_MAX];
	time_t now;
	int ret;

#ifdef _GNU_SOURCE
	home = secure_getenv("HOME");
#else
	home = getenv("HOME");
#endif

	if (home == NULL || *home == '\0') {
		err(1, "getenv (scores: %zu, %zu)", score_l, score_r);
	}

	ret = snprintf(path, sizeof(path), "%s/%s", home, ".pong.scores");

	if (ret < 0 || ret >= PATH_MAX) {
		errno = ENAMETOOLONG;
		err(1, "%s/%s (scores: %zu, %zu)", home, ".pong.scores", score_l,
			score_r);
	}

	scores = fopen(path, "a");

	if (scores == NULL) {
		err(1, "fopen %s (scores: %zu, %zu)", path, score_l, score_r);
	}

	now = time(NULL);
	pretty_now = asctime(localtime(&now));

	if (fprintf(scores, "left: %zu, right: %zu (time: %s)\n",
		score_l, score_r, strtok(pretty_now, "\n")) < 0) {
		errx(1, "fprintf (scores: %zu, %zu)", score_l, score_r);
	}

	if (fclose(scores) == EOF) {
		err(1, "fclose");
	}
}
