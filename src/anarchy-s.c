#define _GNU_SOURCE
/*
 * anarchy-s  --  Draw the Circle-A anarchy symbol in your terminal.
 *
 * Inspired by cool-s (https://github.com/fourzerosix/cool-s).
 * Animates the construction of the Circle-A: the most universally
 * recognized symbol of anarchism, born in 1960s Paris and spread
 * worldwide by punk rock, protest, and spray cans.
 *
 * Construction stages:
 *   Stage 1: Top arc of circle       (upper half, left to right)
 *   Stage 2: Bottom arc of circle    (lower half, right to left)
 *   Stage 3: Left leg of A  /        
 *   Stage 4: Right leg of A \        
 *   Stage 5: Crossbar       -        
 *
 * Geometry: square-pixel coordinates (x: 0..18, y: 0..18).
 *   terminal col = ox + sq_x * 2    (x2 aspect correction)
 *   terminal row = oy + sq_y
 *
 * Flags:
 *   -f            Fast mode (instant)
 *   -d USECS      Per-pixel delay (default: 30000)
 *   -s SCALE      Scale 1-8 (default: 2, auto-fits terminal)
 *   -r            Rainbow finale
 *   -c, --crass   Crass mode: full red, aggressive speed
 *   -b, --bakunin Bakunin mode: "the urge to destroy is a creative urge"
 *                 Symbol explodes outward at the end
 *   -k, --kropotkin  Mutual aid: draw two symbols side by side
 *   -n, --nogods  "No gods, no masters" tagline
 *   --punk        Punk color scheme (red on black background simulation)
 *   --plain       No color
 *   -h, --help    Help
 *
 * Build:   gcc -O2 -o anarchy-s src/anarchy-s.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

/* ── ANSI ──────────────────────────────────────────────────────────────────── */
#define CLEAR   "\033[2J"
#define HOME    "\033[H"
#define HIDE    "\033[?25l"
#define SHOW    "\033[?25h"
#define RESET   "\033[0m"
#define BOLD    "\033[1m"

/* Stage colors */
static const char *STAGE_COLOR_DEFAULT[] = {
    "\033[93m",   /* 1  circle top    -- bright yellow  */
    "\033[93m",   /* 2  circle bottom -- bright yellow  */
    "\033[96m",   /* 3  left leg /    -- bright cyan    */
    "\033[95m",   /* 4  right leg \   -- bright magenta */
    "\033[91m",   /* 5  crossbar -    -- bright red     */
    "\033[97m",   /* 6  final wash    -- bright white   */
};

static const char *STAGE_COLOR_PUNK[] = {
    "\033[91m",   /* 1 */
    "\033[91m",   /* 2 */
    "\033[91m",   /* 3 */
    "\033[91m",   /* 4 */
    "\033[91m",   /* 5 */
    "\033[91m",   /* 6 */
};

static const char **STAGE_COLOR = NULL;
#define N_COLORS 6

/* ── Canvas ────────────────────────────────────────────────────────────────── */
#define MAX_W 512
#define MAX_H 256
typedef struct { char ch; int stage; } Cell;
static Cell canvas[MAX_H][MAX_W];
static int cw, ch_;
static int g_tcols = 80, g_trows = 24;

static void get_term(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        g_tcols = ws.ws_col; g_trows = ws.ws_row;
    }
    cw  = g_tcols < MAX_W ? g_tcols : MAX_W;
    ch_ = g_trows < MAX_H ? g_trows : MAX_H;
}

static void canvas_clear(void) {
    for (int y = 0; y < ch_; y++)
        for (int x = 0; x < cw; x++) {
            canvas[y][x].ch = ' '; canvas[y][x].stage = 0;
        }
}

static void canvas_put(int col, int row, char ch, int stage) {
    if (col < 0 || col >= cw || row < 0 || row >= ch_) return;
    canvas[row][col].ch = ch; canvas[row][col].stage = stage;
}

static void canvas_render(int plain) {
    printf(HOME);
    int last = -1;
    for (int y = 0; y < ch_ - 1; y++) {
        for (int x = 0; x < cw - 1; x++) {
            Cell *c = &canvas[y][x];
            if (!plain && c->stage != last) {
                printf(RESET);
                if (c->stage > 0 && c->stage <= N_COLORS)
                    printf(BOLD "%s", STAGE_COLOR[c->stage - 1]);
                last = c->stage;
            }
            putchar(c->ch);
        }
        putchar('\n');
    }
    printf(RESET); fflush(stdout);
}

/* ── Sparks ────────────────────────────────────────────────────────────────── */
#define MAX_SPARKS 150
typedef struct { int x, y, vx, vy, life, stage; char ch; } Spark;
static Spark sparks[MAX_SPARKS];
static int n_sparks = 0;
static const char SCHARS[] = "*+.'`o";

static void spark_emit(int x, int y, int stage, int count) {
    for (int k = 0; k < count && n_sparks < MAX_SPARKS; k++, n_sparks++) {
        sparks[n_sparks].x     = x + (rand()%7) - 3;
        sparks[n_sparks].y     = y + (rand()%5) - 2;
        sparks[n_sparks].vx    = (rand()%3) - 1;
        sparks[n_sparks].vy    = (rand()%3) - 2;
        sparks[n_sparks].life  = 3 + rand()%5;
        sparks[n_sparks].stage = stage > 0 ? stage : 1;
        sparks[n_sparks].ch    = SCHARS[rand()%(sizeof(SCHARS)-1)];
    }
}

static void sparks_tick(void) {
    for (int i = 0; i < n_sparks; i++) {
        if (sparks[i].life <= 0) continue;
        canvas_put(sparks[i].x, sparks[i].y, ' ', 0);
        sparks[i].x += sparks[i].vx; sparks[i].y += sparks[i].vy;
        sparks[i].life--;
        if (sparks[i].life > 0)
            canvas_put(sparks[i].x, sparks[i].y, sparks[i].ch, sparks[i].stage);
    }
}

/* ── Signal / cleanup ──────────────────────────────────────────────────────── */
static volatile int g_quit = 0;
static void on_sigint(int s) { (void)s; g_quit = 1; }
static void cleanup(void)    { printf(SHOW RESET "\n"); fflush(stdout); }

/* ── Circle drawing (midpoint / parametric) ────────────────────────────────── */
/*
 * Draw an arc from angle t0 to t1 (degrees, screen y-down).
 * 0=right, 90=down, 180=left, 270=up.
 * sq-pixel center (cx,cy), radius R.
 * Terminal: col = ox + (cx+cos(t)*R)*2*scale, row = oy + (cy+sin(t)*R)*scale
 */
static char arc_char(double t) {
    /* pick char based on tangent direction */
    double tn = fmod(t + 360.0, 360.0);
    if ((tn >= 337.5 || tn < 22.5) || (tn >= 157.5 && tn < 202.5)) return '|';
    if ((tn >= 22.5  && tn < 67.5) || (tn >= 202.5 && tn < 247.5)) return '\\';
    if ((tn >= 67.5  && tn < 112.5)|| (tn >= 247.5 && tn < 292.5)) return '-';
    return '/';
}

static void draw_arc(double cx, double cy, double R,
                     double t0_deg, double t1_deg, int steps,
                     int scale, int ox, int oy,
                     int stage, int delay_us, int plain) {
    double step = (t1_deg - t0_deg) / (double)steps;
    if (step == 0) step = 1.0;
    for (int i = 0; i <= steps && !g_quit; i++) {
        double t = t0_deg + i * step;
        double rad = t * M_PI / 180.0;
        int sqx = (int)round(cx + R * cos(rad));
        int sqy = (int)round(cy + R * sin(rad));
        int col  = ox + sqx * 2 * scale;
        int row  = oy + sqy * scale;
        char ch  = arc_char(t);
        canvas_put(col,   row, ch, stage);
        canvas_put(col+1, row, ch, stage);
        if (delay_us > 0) { canvas_render(plain); usleep(delay_us); }
    }
}

/* ── Bresenham line (sq-pixel space) ───────────────────────────────────────── */
static void draw_seg(int x0, int y0, int x1, int y1,
                     int scale, int ox, int oy,
                     char ch, int stage,
                     int delay_us, int plain) {
    int X0 = x0*scale, Y0 = y0*scale;
    int X1 = x1*scale, Y1 = y1*scale;
    int dx = abs(X1-X0), sx = X0<X1?1:-1;
    int dy = -abs(Y1-Y0), sy = Y0<Y1?1:-1;
    int err = dx+dy, e2;
    for(;;) {
        int col = ox + X0*2, row = oy + Y0;
        canvas_put(col,   row, ch, stage);
        canvas_put(col+1, row, ch, stage);
        if (delay_us>0) { canvas_render(plain); usleep(delay_us); }
        if (X0==X1&&Y0==Y1) break;
        e2=2*err;
        if (e2>=dy){err+=dy;X0+=sx;}
        if (e2<=dx){err+=dx;Y0+=sy;}
    }
}

/* ── Bakunin explosion ──────────────────────────────────────────────────────── */
/*  "The urge to destroy is also a creative urge." -- Mikhail Bakunin, 1842  */
#define MAX_DEBRIS 600
typedef struct { float x,y,vx,vy; int life,maxlife,stage; char ch; } Debris;
static Debris debris[MAX_DEBRIS];
static int    n_debris = 0;
static const char DCHARS[] = "Ao*+.'|/\\-~^";
static const char FCHARS[] = "Ao@*#$%&";

static void bakunin(int plain, int delay_us) {
    n_debris = 0;
    float fcx = (float)cw / 2.0f, fcy = (float)ch_ / 2.0f;
    for (int y = 0; y < ch_ && n_debris < MAX_DEBRIS; y++) {
        for (int x = 0; x < cw && n_debris < MAX_DEBRIS; x++) {
            if (canvas[y][x].ch == ' ') continue;
            float dx = x - fcx, dy = y - fcy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 0.1f) dist = 0.1f;
            float speed = 0.4f + ((float)rand()/RAND_MAX) * 1.4f;
            debris[n_debris].x       = x;
            debris[n_debris].y       = y;
            debris[n_debris].vx      = (dx/dist)*speed + ((float)rand()/RAND_MAX-0.5f)*0.6f;
            debris[n_debris].vy      = (dy/dist)*speed*0.5f + ((float)rand()/RAND_MAX-0.5f)*0.3f;
            debris[n_debris].life    = 20 + rand()%20;
            debris[n_debris].maxlife = debris[n_debris].life;
            debris[n_debris].stage   = canvas[y][x].stage;
            debris[n_debris].ch      = DCHARS[rand()%(sizeof(DCHARS)-1)];
            n_debris++;
        }
    }

    /* flash */
    if (!plain) {
        for (int fl = 0; fl < 4 && !g_quit; fl++) {
            canvas_clear();
            int stg = fl%2 ? N_COLORS : 5;
            for (int y = 0; y < ch_-1; y++)
                for (int x = 0; x < cw-1; x++)
                    if ((x+y+fl)%(2+fl)==0) {
                        canvas[y][x].ch    = FCHARS[rand()%(sizeof(FCHARS)-1)];
                        canvas[y][x].stage = stg;
                    }
            canvas_render(plain);
            usleep(40000);
        }
    }

    /* shockwave + debris */
    for (int f = 0; f < 45 && !g_quit; f++) {
        canvas_clear();
        if (!plain) {
            float rad_r = f * 1.4f;
            for (int th = 0; th < 360; th += 2) {
                float r = th * M_PI / 180.0f;
                int rx = (int)(fcx + cosf(r)*rad_r*2.0f);
                int ry = (int)(fcy + sinf(r)*rad_r);
                canvas_put(rx, ry, 'o', f<10?5:f<20?4:3);
            }
        }
        for (int i = 0; i < n_debris; i++) {
            if (debris[i].life <= 0) continue;
            debris[i].x += debris[i].vx;
            debris[i].y += debris[i].vy;
            debris[i].vy += 0.03f;
            debris[i].life--;
            char dc = debris[i].life > debris[i].maxlife*2/3 ? debris[i].ch :
                      debris[i].life > debris[i].maxlife/3   ? '.' : '`';
            int sc = debris[i].life > debris[i].maxlife/2 ? debris[i].stage : 1;
            canvas_put((int)debris[i].x, (int)debris[i].y, dc, sc);
        }
        canvas_render(plain);
        usleep(delay_us>0 ? (delay_us<30000?30000:delay_us) : 50000);
    }

    /* ash */
    for (int f = 0; f < 15 && !g_quit; f++) {
        for (int i = 0; i < n_debris; i++) {
            if (debris[i].life <= 0) continue;
            canvas_put((int)debris[i].x, (int)debris[i].y, ' ', 0);
            debris[i].y += 0.2f;
            debris[i].life -= 2;
            if (debris[i].life > 0)
                canvas_put((int)debris[i].x, (int)debris[i].y, '.', 1);
        }
        canvas_render(plain);
        usleep(80000);
    }
    canvas_clear(); canvas_render(plain);
}

/* ── Recolor ────────────────────────────────────────────────────────────────── */
static void recolor(int stage) {
    for (int y=0;y<ch_;y++)
        for (int x=0;x<cw;x++)
            if (canvas[y][x].ch!=' ') canvas[y][x].stage=stage;
}

/* ── Options ────────────────────────────────────────────────────────────────── */
typedef struct {
    int fast, delay_us, scale;
    int rainbow, crass, bakunin, kropotkin, punk;
    int nogods, plain;
} Opts;

static void usage(const char *prog) {
    printf(
      "Usage: %s [OPTIONS]\n\n"
      "Draw the Circle-A anarchy symbol in your terminal.\n\n"
      "Options:\n"
      "  -f                Fast mode (no animation)\n"
      "  -d USECS          Per-pixel delay in microseconds (default: 30000)\n"
      "  -s SCALE          Scale 1-8 (default: 2, auto-fits terminal)\n"
      "  -r                Rainbow finale\n"
      "  -c, --crass       Crass mode: all-red, faster, more aggressive\n"
      "  -b, --bakunin     Bakunin: symbol explodes at the end\n"
      "                    (\"The urge to destroy is also a creative urge\")\n"
      "  -k, --kropotkin   Mutual aid: draw two symbols side by side\n"
      "  -n, --nogods      Show \"No gods, no masters\" tagline\n"
      "  --punk            Punk color scheme (all red)\n"
      "  --plain           No color output\n"
      "  -h, --help        Show this help\n\n"
      "Examples:\n"
      "  anarchy-s                   Animated Circle-A\n"
      "  anarchy-s -c                Crass mode -- fast, red, loud\n"
      "  anarchy-s -b                Bakunin -- builds then destroys\n"
      "  anarchy-s -k                Two symbols, mutual aid\n"
      "  anarchy-s -f -s 4           Instant, large\n"
      "  anarchy-s -d 60000 -r       Slow with rainbow\n"
      "  anarchy-s -n                With tagline\n",
      prog);
}

/* ── Draw one Circle-A ──────────────────────────────────────────────────────── */
/*
 * sq-pixel geometry (before scaling), centered at (9,9):
 *   Circle: radius 8
 *   A top peak: (9, 2)
 *   A bottom left: (4, 14)
 *   A bottom right: (14, 14)
 *   A crossbar: y=9, x from ~6 to ~12
 */
static void draw_one(int scale, int ox, int oy, int delay, int plain,
                     int no_sparks) {
    double cx = 9.0, cy = 9.0, R = 8.0;

    /* arc resolution: more steps at larger scale */
    int arc_steps = 60 + scale * 20;

    /* Stage 1: top arc (180 -> 360, i.e. left over top to right) */
    draw_arc(cx,cy,R, 180, 360, arc_steps, scale, ox, oy, 1, delay, plain);
    if (!no_sparks && delay>0 && !g_quit) {
        spark_emit(ox+(int)(cx+R)*2*scale, oy+(int)cy*scale, 1, 5);
        sparks_tick(); canvas_render(plain); usleep(delay*3);
    }

    /* Stage 2: bottom arc (0 -> 180, right around bottom to left) */
    draw_arc(cx,cy,R, 0, 180, arc_steps, scale, ox, oy, 2, delay, plain);
    if (!no_sparks && delay>0 && !g_quit) {
        spark_emit(ox+(int)(cx-R)*2*scale, oy+(int)cy*scale, 2, 5);
        sparks_tick(); canvas_render(plain); usleep(delay*3);
    }

    /* A coordinates in sq-pixel space */
    int atx = 9,  aty = 2;   /* top peak */
    int alx = 4,  aly = 14;  /* bottom left */
    int arx = 14, ary = 14;  /* bottom right */
    int bar_y = 9;
    /* crossbar endpoints: on the legs at bar_y */
    double frac = (double)(aly - bar_y) / (double)(aly - aty);
    int blx = (int)(alx + frac*(atx - alx));
    int brx = (int)(arx + frac*(atx - arx));

    /* Stage 3: left leg / */
    draw_seg(alx, aly, atx, aty, scale, ox, oy, '/', 3, delay, plain);
    if (!no_sparks && delay>0 && !g_quit) {
        spark_emit(ox+atx*2*scale, oy+aty*scale, 3, 5);
        sparks_tick(); canvas_render(plain); usleep(delay*3);
    }

    /* Stage 4: right leg \ */
    draw_seg(atx, aty, arx, ary, scale, ox, oy, '\\', 4, delay, plain);
    if (!no_sparks && delay>0 && !g_quit) {
        spark_emit(ox+arx*2*scale, oy+ary*scale, 4, 5);
        sparks_tick(); canvas_render(plain); usleep(delay*3);
    }

    /* Stage 5: crossbar - */
    draw_seg(blx, bar_y, brx, bar_y, scale, ox, oy, '-', 5, delay, plain);
    if (!no_sparks && delay>0 && !g_quit) {
        spark_emit(ox+brx*2*scale, oy+bar_y*scale, 5, 5);
        sparks_tick(); canvas_render(plain); usleep(delay*3);
    }
}

/* ── Main draw ──────────────────────────────────────────────────────────────── */
static void draw_anarchy(const Opts *o) {
    get_term();
    canvas_clear();

    /* Symbol bounding box in sq-pixels: 18x18 (diameter + margin).
       Terminal: width = 18*2*scale, height = 18*scale */
    int scale = o->scale;
    while (scale > 1 && (18*scale > ch_-3 || 36*scale > cw-4))
        scale--;

    int s_w = 18 * 2 * scale;
    int s_h = 18 * scale;

    int delay = o->fast ? 0 : o->delay_us;
    if (o->crass && !o->fast) delay = delay * 2 / 3;  /* crass: faster */

    if (o->kropotkin) {
        /* Mutual aid: two symbols side by side */
        int gap   = 4 * scale;
        int total = s_w * 2 + gap;
        int ox1   = (cw - total) / 2;
        int ox2   = ox1 + s_w + gap;
        int oy    = (ch_ - s_h) / 2;
        if (ox1 < 1) ox1 = 1;
        if (oy  < 1) oy  = 1;

        draw_one(scale, ox1, oy, delay, o->plain, 0);
        if (!g_quit) draw_one(scale, ox2, oy, delay, o->plain, 0);
    } else {
        int ox = (cw - s_w) / 2;
        int oy = (ch_ - s_h) / 2;
        if (ox < 1) ox = 1;
        if (oy < 1) oy = 1;
        draw_one(scale, ox, oy, delay, o->plain, 0);
    }

    if (g_quit) return;

    /* finale */
    if (!o->fast && delay > 0) {
        usleep(300000);
        recolor(N_COLORS);
        canvas_render(o->plain);
        usleep(400000);

        if (o->rainbow && !g_quit) {
            for (int p = 0; p < 20 && !g_quit; p++) {
                for (int y=0;y<ch_;y++)
                    for (int x=0;x<cw;x++)
                        if (canvas[y][x].ch!=' ')
                            canvas[y][x].stage = ((p+x/3+y)%(N_COLORS-1))+1;
                canvas_render(o->plain);
                usleep(150000);
            }
            recolor(N_COLORS);
            canvas_render(o->plain);
            usleep(300000);
        }
    } else {
        canvas_render(o->plain);
    }

    /* tagline */
    if (!g_quit && !o->bakunin) {
        const char *tag = o->nogods
            ? "No gods, no masters"
            : o->crass
            ? "~ anarchy-s ~  [ Do what thou wilt ]"
            : "~ anarchy-s ~";
        int s_w2 = o->kropotkin ? 18*2*scale*2+4*scale : 18*2*scale;
        int ox_tag = (cw - s_w2) / 2;
        int tx = ox_tag + (s_w2 - (int)strlen(tag)) / 2;
        int ty = (ch_ - s_h) / 2 + s_h + 2;
        if (tx < 0) tx = 0;
        for (int i = 0; tag[i] && tx+i < cw-1; i++)
            canvas_put(tx+i, ty, tag[i], 5);
        canvas_render(o->plain);
    }

    /* Bakunin */
    if (o->bakunin && !g_quit) {
        if (!o->fast) usleep(600000);
        bakunin(o->plain, delay);
    }
}

/* ── main ───────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    Opts o = { .fast=0, .delay_us=30000, .scale=2,
               .rainbow=0, .crass=0, .bakunin=0, .kropotkin=0,
               .punk=0, .nogods=0, .plain=0 };

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i],"-f"))                           o.fast=1;
        else if (!strcmp(argv[i],"-r"))                           o.rainbow=1;
        else if (!strcmp(argv[i],"-c")||!strcmp(argv[i],"--crass"))    o.crass=1;
        else if (!strcmp(argv[i],"-b")||!strcmp(argv[i],"--bakunin"))  o.bakunin=1;
        else if (!strcmp(argv[i],"-k")||!strcmp(argv[i],"--kropotkin"))o.kropotkin=1;
        else if (!strcmp(argv[i],"-n")||!strcmp(argv[i],"--nogods"))   o.nogods=1;
        else if (!strcmp(argv[i],"--punk"))                       o.punk=1;
        else if (!strcmp(argv[i],"--plain"))                      o.plain=1;
        else if (!strcmp(argv[i],"-d")&&i+1<argc) {
            o.delay_us=atoi(argv[++i]); if(o.delay_us<0)o.delay_us=0;
        } else if (!strcmp(argv[i],"-s")&&i+1<argc) {
            o.scale=atoi(argv[++i]);
            if(o.scale<1){o.scale=1;} if(o.scale>8){o.scale=8;}
        } else if (!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")) {
            usage(argv[0]); return 0;
        } else {
            fprintf(stderr,"Unknown option: %s\n",argv[i]);
            usage(argv[0]); return 1;
        }
    }

    /* color scheme */
    STAGE_COLOR = (o.punk || o.crass) ? STAGE_COLOR_PUNK : STAGE_COLOR_DEFAULT;

    srand((unsigned)time(NULL));
    signal(SIGINT, on_sigint);
    atexit(cleanup);

    if (!o.fast) { printf(CLEAR HIDE); fflush(stdout); }

    draw_anarchy(&o);

    if (!o.fast && !g_quit && !o.bakunin) {
        printf("\033[%d;1H", g_trows);
        const char *msg = "[ press any key to exit ]";
        int pad = (g_tcols-(int)strlen(msg))/2;
        for (int i=0;i<pad;i++) putchar(' ');
        printf(BOLD "\033[97m%s" RESET, msg);
        fflush(stdout);
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt; newt.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }

    printf(CLEAR HOME SHOW RESET); fflush(stdout);
    return 0;
}
