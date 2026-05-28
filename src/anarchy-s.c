#define _GNU_SOURCE
/*
 * anarchy-s  --  Draw the Circle-A anarchy symbol in your terminal.
 *
 * Inspired by cool-s (https://github.com/fourzerosix/cool-s).
 * Animates the Circle-A: the universal symbol of anarchism.
 * Born in 1964 Paris. Spread worldwide by punk rock and spray cans.
 *
 * Construction stages:
 *   1. Top arc of circle        (yellow)
 *   2. Bottom arc of circle     (yellow)
 *   3. Left leg of A  /         (cyan)
 *   4. Right leg of A \         (magenta)
 *   5. Crossbar       -         (red)
 *
 * Geometry: square-pixel space (sq-pixels: 0..19 x 0..19)
 *   Circle: center (9,9), radius 8
 *   Terminal: col = ox + sq_x * 2 * scale
 *             row = oy + sq_y * scale
 *
 * Build: gcc -O2 -o anarchy-s src/anarchy-s.c -lm
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

/* ── Global state ─────────────────────────────────────────────────────────── */
static volatile int g_quit = 0;
static int          g_punk = 0;


/* ── ANSI ──────────────────────────────────────────────────────────────────── */
#define CLEAR  "\033[2J"
#define HOME   "\033[H"
#define HIDE   "\033[?25l"
#define SHOW   "\033[?25h"
#define RESET  "\033[0m"
#define BOLD   "\033[1m"

static const char *COLOR_DEFAULT[6] = {
    "\033[93m", "\033[93m",  /* circle top/bot -- yellow  */
    "\033[96m",              /* left leg       -- cyan    */
    "\033[95m",              /* right leg      -- magenta */
    "\033[91m",              /* crossbar       -- red     */
    "\033[97m",              /* final wash     -- white   */
};
static const char *COLOR_PUNK[6] = {
    "\033[91m","\033[91m","\033[91m","\033[91m","\033[91m","\033[91m",
};
static const char **COLORS = NULL;
#define N_COLORS 6

/* ── Canvas ────────────────────────────────────────────────────────────────── */
#define MAX_W 512
#define MAX_H 256
typedef struct { char ch; int stage; } Cell;
static Cell canvas[MAX_H][MAX_W];
static int cw, ch_;
static int g_tcols=80, g_trows=24;

static void get_term(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0 && ws.ws_col>0) {
        g_tcols=ws.ws_col; g_trows=ws.ws_row;
    }
    cw  = g_tcols<MAX_W ? g_tcols : MAX_W;
    ch_ = g_trows<MAX_H ? g_trows : MAX_H;
}

static void canvas_clear(void) {
    for(int y=0;y<ch_;y++) for(int x=0;x<cw;x++)
        { canvas[y][x].ch=' '; canvas[y][x].stage=0; }
}

static void canvas_put(int col, int row, char ch, int stage) {
    if(col<0||col>=cw||row<0||row>=ch_) return;
    canvas[row][col].ch=ch; canvas[row][col].stage=stage;
}

static void canvas_render(int plain) {
    printf(HOME);
    int last=-1;
    for(int y=0;y<ch_-1;y++){
        for(int x=0;x<cw-1;x++){
            Cell *c=&canvas[y][x];
            if(!plain && c->stage!=last){
                printf(RESET);
                if(c->stage>0&&c->stage<=N_COLORS)
                    printf(BOLD"%s",COLORS[c->stage-1]);
                last=c->stage;
            }
            putchar(c->ch);
        }
        putchar('\n');
    }
    printf(RESET); fflush(stdout);
}

/* ── Circle char: pick from tangent direction ──────────────────────────────── */
/* dx,dy = offset from circle center. Tangent slope = -dx/dy */
static char circle_char(int dx, int dy) {
    double t;
    if(dy==0) return '|';   /* left/right: vertical tangent */
    if(dx==0) return '-';   /* top/bottom: horizontal tangent */
    t = -(double)dx / (double)dy;
    if(fabs(t)<0.5)  return '-';
    if(fabs(t)>2.0)  return '|';
    return (t>0) ? '\\' : '/';
}

/* ── Bresenham circle in sq-pixel space ────────────────────────────────────── */
/* Draws arc from t0_deg to t1_deg (screen angles: 0=right,90=down,180=left,270=top)
   Steps through the full circle but only places pixels in the arc range.       */
static void draw_arc(int cx, int cy, int r,
                     double t0, double t1,
                     int scale, int ox, int oy,
                     int stage, int delay_us, int plain) {
    int x=0, y=r, d=1-r;
    while(x<=y && !g_quit) {
        /* 8 symmetric points */
        struct { int px,py,dx,dy; } pts[8]={
            {cx+x,cy+y, x, y},{cx-x,cy+y,-x, y},
            {cx+x,cy-y, x,-y},{cx-x,cy-y,-x,-y},
            {cx+y,cy+x, y, x},{cx-y,cy+x,-y, x},
            {cx+y,cy-x, y,-x},{cx-y,cy-x,-y,-x},
        };
        for(int i=0;i<8;i++){
            int px=pts[i].px, py=pts[i].py;
            /* angle of this point in screen coords (0=right,90=down) */
            double ang=atan2((double)(py-cy),(double)(px-cx))*180.0/M_PI;
            if(ang<0) ang+=360.0;
            /* check if in arc range (handle wrap-around) */
            int in_arc=0;
            /* Normalise t1 so it's always > t0 (handle wrap past 360) */
            double t1n = t1;
            double angn = ang;
            if(t1n > 360.0) {
                /* e.g. t0=270 t1=450: shift angles < t0 up by 360 */
                if(angn < t0) angn += 360.0;
                in_arc = (angn >= t0 && angn <= t1n);
            } else if(t0 > t1n) {
                in_arc = (ang >= t0 || ang <= t1n);
            } else {
                in_arc = (ang >= t0 && ang <= t1n);
            }
            if(!in_arc) continue;
            char ch=circle_char(pts[i].dx,pts[i].dy);
            int col=ox+px*2*scale;
            int row=oy+py*scale;
            canvas_put(col,  row,ch,stage);
            canvas_put(col+1,row,ch,stage);
        }
        if(delay_us>0){ canvas_render(plain); usleep(delay_us); }
        if(d<0) d+=2*x+3;
        else { d+=2*(x-y)+5; y--; }
        x++;
    }
}

/* ── Bresenham line in sq-pixel space ──────────────────────────────────────── */
static void draw_seg(int x0,int y0,int x1,int y1,
                     int scale,int ox,int oy,
                     char ch,int stage,int delay_us,int plain) {
    int X0=x0*scale, Y0=y0*scale;
    int X1=x1*scale, Y1=y1*scale;
    int dx=abs(X1-X0),sx=X0<X1?1:-1;
    int dy=-abs(Y1-Y0),sy=Y0<Y1?1:-1;
    int err=dx+dy,e2;
    for(;;){
        int col=ox+X0*2, row=oy+Y0;
        canvas_put(col,  row,ch,stage);
        canvas_put(col+1,row,ch,stage);
        if(delay_us>0){ canvas_render(plain); usleep(delay_us); }
        if(X0==X1&&Y0==Y1) break;
        e2=2*err;
        if(e2>=dy){err+=dy;X0+=sx;}
        if(e2<=dx){err+=dx;Y0+=sy;}
    }
}

/* ── Sparks ────────────────────────────────────────────────────────────────── */
#define MAX_SPARKS 150
typedef struct{int x,y,vx,vy,life,stage;char ch;}Spark;
static Spark sparks[MAX_SPARKS];
static int n_sparks=0;
static const char SCHARS[]="*+.'`o";

static void spark_emit(int x,int y,int stage,int count){
    for(int k=0;k<count&&n_sparks<MAX_SPARKS;k++,n_sparks++){
        sparks[n_sparks].x=x+(rand()%7)-3;
        sparks[n_sparks].y=y+(rand()%5)-2;
        sparks[n_sparks].vx=(rand()%3)-1;
        sparks[n_sparks].vy=(rand()%3)-2;
        sparks[n_sparks].life=3+rand()%5;
        sparks[n_sparks].stage=stage>0?stage:1;
        sparks[n_sparks].ch=SCHARS[rand()%(sizeof(SCHARS)-1)];
    }
}
static void sparks_tick(void){
    for(int i=0;i<n_sparks;i++){
        if(sparks[i].life<=0) continue;
        canvas_put(sparks[i].x,sparks[i].y,' ',0);
        sparks[i].x+=sparks[i].vx; sparks[i].y+=sparks[i].vy;
        sparks[i].life--;
        if(sparks[i].life>0)
            canvas_put(sparks[i].x,sparks[i].y,sparks[i].ch,sparks[i].stage);
    }
}

/* ── Signal / cleanup ──────────────────────────────────────────────────────── */
static void on_sigint(int s){(void)s;g_quit=1;}
static void cleanup(void){printf(SHOW RESET"\n");fflush(stdout);}

/* ── Recolor ────────────────────────────────────────────────────────────────── */
static void recolor(int stage){
    for(int y=0;y<ch_;y++) for(int x=0;x<cw;x++)
        if(canvas[y][x].ch!=' ') canvas[y][x].stage=stage;
}

/* ── Bakunin explosion ──────────────────────────────────────────────────────── */
#define MAX_DEBRIS 600
typedef struct{float x,y,vx,vy;int life,maxlife,stage;char ch;}Debris;
static Debris debris[MAX_DEBRIS];
static int n_debris=0;
static const char DCHARS[]="Ao*+.'|/\\-~^";
static const char FCHARS[]="Ao@*#";

static void bakunin_explode(int plain, int delay_us) {
    n_debris=0;
    float fcx=(float)cw/2.0f, fcy=(float)ch_/2.0f;
    for(int y=0;y<ch_&&n_debris<MAX_DEBRIS;y++)
        for(int x=0;x<cw&&n_debris<MAX_DEBRIS;x++){
            if(canvas[y][x].ch==' ') continue;
            float dx=x-fcx, dy=y-fcy;
            float dist=sqrtf(dx*dx+dy*dy); if(dist<0.1f)dist=0.1f;
            float spd=0.4f+((float)rand()/RAND_MAX)*1.4f;
            debris[n_debris]=(Debris){
                .x=(float)x, .y=(float)y,
                .vx=(dx/dist)*spd+((float)rand()/RAND_MAX-0.5f)*0.6f,
                .vy=(dy/dist)*spd*0.5f+((float)rand()/RAND_MAX-0.5f)*0.3f,
                .life=20+rand()%20, .maxlife=0,
                .stage=canvas[y][x].stage,
                .ch=DCHARS[rand()%(sizeof(DCHARS)-1)]
            };
            debris[n_debris].maxlife=debris[n_debris].life;
            n_debris++;
        }
    if(!plain)
        for(int fl=0;fl<4&&!g_quit;fl++){
            canvas_clear();
            for(int y=0;y<ch_-1;y++) for(int x=0;x<cw-1;x++)
                if((x+y+fl)%(2+fl)==0){
                    canvas[y][x].ch=FCHARS[rand()%(sizeof(FCHARS)-1)];
                    canvas[y][x].stage=fl%2?N_COLORS:5;
                }
            canvas_render(plain); usleep(40000);
        }
    for(int f=0;f<45&&!g_quit;f++){
        canvas_clear();
        if(!plain){
            float rad_r=f*1.4f;
            for(int th=0;th<360;th+=2){
                float r2=th*M_PI/180.0f;
                canvas_put((int)(fcx+cosf(r2)*rad_r*2.0f),
                           (int)(fcy+sinf(r2)*rad_r),
                           'o', f<10?5:f<20?4:3);
            }
        }
        for(int i=0;i<n_debris;i++){
            if(debris[i].life<=0) continue;
            debris[i].x+=debris[i].vx; debris[i].y+=debris[i].vy;
            debris[i].vy+=0.03f; debris[i].life--;
            char dc=debris[i].life>debris[i].maxlife*2/3?debris[i].ch:
                    debris[i].life>debris[i].maxlife/3?'.':'`';
            canvas_put((int)debris[i].x,(int)debris[i].y,dc,
                       debris[i].life>debris[i].maxlife/2?debris[i].stage:1);
        }
        canvas_render(plain);
        usleep(delay_us>0?(delay_us<30000?30000:delay_us):50000);
    }
    for(int f=0;f<15&&!g_quit;f++){
        for(int i=0;i<n_debris;i++){
            if(debris[i].life<=0) continue;
            canvas_put((int)debris[i].x,(int)debris[i].y,' ',0);
            debris[i].y+=0.2f; debris[i].life-=2;
            if(debris[i].life>0)
                canvas_put((int)debris[i].x,(int)debris[i].y,'.',1);
        }
        canvas_render(plain); usleep(80000);
    }
    canvas_clear(); canvas_render(plain);
}

/* ── No-gods fire finale ────────────────────────────────────────────────────── */
/* Fills screen upward with fire chars, burns bright then fades to ash */
static void fire_finale(int plain) {
    if(plain) return;
    static const char *fire_chars[]={".","'","`","^","*","(",")","&","#","@"};
    static const int   fire_stages[]={1,1,5,5,4,4,3,3,2,2};
    int nf=10;
    /* rise: fire crawls up from bottom */
    for(int wave=0;wave<ch_&&!g_quit;wave++){
        for(int x=0;x<cw-1;x++){
            int row=ch_-2-wave+(rand()%3)-1;
            if(row<0) row=0;
            if(row>=ch_-1) continue;
            int fi=rand()%nf;
            canvas_put(x,row,fire_chars[fi][0],fire_stages[fi]);
        }
        canvas_render(plain); usleep(18000);
    }
    /* burn bright */
    for(int f=0;f<8&&!g_quit;f++){
        for(int y=0;y<ch_-1;y++)
            for(int x=0;x<cw-1;x++){
                int fi=rand()%nf;
                canvas[y][x].ch=fire_chars[fi][0];
                canvas[y][x].stage=fire_stages[fi];
            }
        canvas_render(plain); usleep(60000);
    }
    /* die down */
    for(int wave=0;wave<ch_&&!g_quit;wave++){
        int row=wave;
        for(int x=0;x<cw-1;x++) canvas_put(x,row,' ',0);
        /* dim lower rows */
        for(int y=wave;y<ch_-1;y++)
            for(int x=0;x<cw-1;x++)
                if(canvas[y][x].ch!=' ' && canvas[y][x].stage>1)
                    canvas[y][x].stage=1;
        canvas_render(plain); usleep(25000);
    }
    canvas_clear(); canvas_render(plain);
}

/* ── Dripping blood paint (punk mode finale) ────────────────────────────────── */
static void drip_blood(int plain) {
    if(plain) return;
    /* start drips from every lit pixel, fall down, fill screen */
    typedef struct{int x; float y; float speed; int active;}Drip;
    int nd=0;
    static Drip drips[MAX_W];
    /* seed drips from bottom of each column that has lit pixels */
    for(int x=0;x<cw-1&&nd<MAX_W;x++){
        for(int y=ch_-2;y>=0;y--){
            if(canvas[y][x].ch!=' '){
                drips[nd].x=x; drips[nd].y=(float)y;
                drips[nd].speed=0.3f+((float)rand()/RAND_MAX)*0.5f;
                drips[nd].active=1;
                nd++; break;
            }
        }
    }
    int running=1;
    while(running&&!g_quit){
        running=0;
        for(int i=0;i<nd;i++){
            if(!drips[i].active) continue;
            running=1;
            drips[i].y+=drips[i].speed;
            if(drips[i].y>=ch_-2){ drips[i].active=0; continue; }
            /* paint trail */
            int iy=(int)drips[i].y;
            for(int dy=0;dy<=2&&iy-dy>=0;dy++){
                char dc= dy==0?'|': dy==1?';':'.';
                canvas_put(drips[i].x, iy-dy, dc, 5);
                if(drips[i].x+1<cw-1)
                    canvas_put(drips[i].x+1, iy-dy, dc, 5);
            }
        }
        canvas_render(plain); usleep(30000);
    }
    /* flood fill bottom up */
    for(int y=ch_-2;y>=0&&!g_quit;y--){
        for(int x=0;x<cw-1;x++){
            canvas[y][x].ch=(rand()%3==0)?'*':' ';
            if(canvas[y][x].ch!=' ') canvas[y][x].stage=5;
        }
        /* keep pixels above current row */
        canvas_render(plain); usleep(12000);
    }
    canvas_clear(); canvas_render(plain);
}

static void draw_one_at(int scale,int ox,int oy,int delay,int plain,int instant);

/* ── Kropotkin handshake ────────────────────────────────────────────────────── */
/* Two symbols slide in from opposite edges and meet in the middle */
static void kropotkin_slide(int scale, int s_w, int s_h, int delay, int plain) {
    int gap=4*scale;
    int total=s_w*2+gap;
    int final_ox1=(cw-total)/2;
    int final_ox2=final_ox1+s_w+gap;
    int oy=(ch_-s_h)/2; if(oy<1)oy=1;
    /* slide from edges */
    int steps=final_ox1+2;
    for(int step=0;step<=steps&&!g_quit;step++){
        canvas_clear();
        int ox1=step-final_ox1-2;      /* comes from left: starts off-screen */
        int ox2=cw-(step*(cw-final_ox2)/steps); /* from right */
        /* clamp */
        if(ox1<-(s_w)) ox1=-(s_w);
        draw_one_at(scale,ox1,oy,0,plain,1);
        draw_one_at(scale,ox2,oy,0,plain,1);
        canvas_render(plain);
        usleep(delay>0?delay/3:8000);
    }
    /* bounce */
    for(int b=0;b<3&&!g_quit;b++){
        canvas_clear();
        int ox1=final_ox1+(b%2?2:-2);
        int ox2=final_ox2+(b%2?-2:2);
        draw_one_at(scale,ox1,oy,0,plain,1);
        draw_one_at(scale,ox2,oy,0,plain,1);
        canvas_render(plain); usleep(80000);
    }
    /* settle */
    canvas_clear();
    draw_one_at(scale,final_ox1,oy,0,plain,1);
    draw_one_at(scale,final_ox2,oy,0,plain,1);
    canvas_render(plain);
}

/* ── Options ────────────────────────────────────────────────────────────────── */
typedef struct {
    int fast,delay_us,scale;
    int rainbow,crass,bakunin,kropotkin,punk;
    int nogods,plain;
} Opts;

static void usage(const char *prog){
    printf(
      "Usage: %s [OPTIONS]\n\n"
      "Draw the Circle-A anarchy symbol in your terminal.\n\n"
      "Options:\n"
      "  -f                Fast mode (no animation)\n"
      "  -d USECS          Per-pixel delay (default: 30000)\n"
      "  -s SCALE          Scale 1-8 (default: 2, auto-fits)\n"
      "  -r                Rainbow finale\n"
      "  -c, --crass       Crass mode: all red, aggressive speed\n"
      "  -p, --punk        Punk: A extends outside circle, blood-drip finale\n"
      "  -b, --bakunin     Bakunin: builds then explodes\n"
      "  -k, --kropotkin   Mutual aid: two symbols slide in and meet\n"
      "  -n, --nogods      \"No gods, no masters\" + fire finale\n"
      "  --plain           No color\n"
      "  -h, --help        Help\n\n"
      "Examples:\n"
      "  anarchy-s           Full animation\n"
      "  anarchy-s -p        Punk mode -- graffiti A with blood drip\n"
      "  anarchy-s -b        Builds then explodes (Bakunin)\n"
      "  anarchy-s -k        Two symbols slide in and meet (Kropotkin)\n"
      "  anarchy-s -n        Fire finale with No gods, no masters\n"
      "  anarchy-s -c -b     Crass + Bakunin: chaotic red destruction\n",
      prog);
}

/* ── Draw one Circle-A ──────────────────────────────────────────────────────── */
/*
 * sq-pixel geometry (circle center 9,9 radius 8):
 *   Normal A:  peak (9,3)  feet (5,15)(13,15)  crossbar y=10
 *   Punk A:    peak (9,0)  feet (3,18)(15,18)  crossbar y=10
 */
static void draw_one_at(int scale, int ox, int oy, int delay, int plain,
                        int instant) {
    int d = instant ? 0 : delay;
    int ns = instant ? 0 : 1; /* no sparks in instant mode */

    /* Stage 1: top arc (270deg -> 90deg going clockwise, i.e. upper half) */
    draw_arc(9,9,8, 270,450, scale,ox,oy, 1, d, plain);
    if(!ns&&d>0&&!g_quit){
        spark_emit(ox+9*2*scale, oy+1*scale, 1, 5);
        sparks_tick(); canvas_render(plain); usleep(d*3);
    }

    /* Stage 2: bottom arc (90deg -> 270deg) */
    draw_arc(9,9,8, 90,270, scale,ox,oy, 2, d, plain);
    if(!ns&&d>0&&!g_quit){
        spark_emit(ox+9*2*scale, oy+17*scale, 2, 5);
        sparks_tick(); canvas_render(plain); usleep(d*3);
    }

    /* A coordinates */
    int atx,aty,alx,aly,arx,ary;
    if(g_punk){
        atx=9;  aty=0;
        alx=3;  aly=18;
        arx=15; ary=18;
    } else {
        atx=9;  aty=3;
        alx=5;  aly=15;
        arx=13; ary=15;
    }
    int bar_y=10;
    double frac=(double)(aly-bar_y)/(double)(aly-aty);
    int blx=(int)(alx+frac*(atx-alx));
    int brx=(int)(arx+frac*(atx-arx));

    /* Stage 3: left leg / */
    draw_seg(alx,aly,atx,aty,scale,ox,oy,'/',3,d,plain);
    if(!ns&&d>0&&!g_quit){
        spark_emit(ox+atx*2*scale,oy+aty*scale,3,5);
        sparks_tick(); canvas_render(plain); usleep(d*3);
    }

    /* Stage 4: right leg \ */
    draw_seg(atx,aty,arx,ary,scale,ox,oy,'\\',4,d,plain);
    if(!ns&&d>0&&!g_quit){
        spark_emit(ox+arx*2*scale,oy+ary*scale,4,5);
        sparks_tick(); canvas_render(plain); usleep(d*3);
    }

    /* Stage 5: crossbar */
    draw_seg(blx,bar_y,brx,bar_y,scale,ox,oy,'-',5,d,plain);
    if(!ns&&d>0&&!g_quit){
        spark_emit(ox+brx*2*scale,oy+bar_y*scale,5,5);
        sparks_tick(); canvas_render(plain); usleep(d*3);
    }
}


/* ── Main draw ──────────────────────────────────────────────────────────────── */
static void draw_anarchy(const Opts *o) {
    get_term(); canvas_clear();
    g_punk = o->punk;

    int scale=o->scale;
    /* symbol spans sq 0..19 in both axes: 20 sq-pixels
       width = 20*2*scale cols, height = 20*scale rows */
    while(scale>1 && (20*scale>ch_-3 || 40*scale>cw-4)) scale--;

    int s_w=20*2*scale;
    int s_h=20*scale;
    int delay=o->fast?0:o->delay_us;
    if(o->crass&&!o->fast) delay=delay*2/3;

    if(o->kropotkin){
        kropotkin_slide(scale,s_w,s_h,delay,o->plain);
    } else {
        int ox=(cw-s_w)/2; int oy=(ch_-s_h)/2;
        if(ox<1){ox=1;} if(oy<1){oy=1;}
        draw_one_at(scale,ox,oy,delay,o->plain,0);
    }

    if(g_quit) return;

    /* finale wash */
    if(!o->fast&&delay>0){
        usleep(300000);
        recolor(N_COLORS); canvas_render(o->plain); usleep(400000);
        if(o->rainbow&&!g_quit){
            for(int p=0;p<20&&!g_quit;p++){
                for(int y=0;y<ch_;y++) for(int x=0;x<cw;x++)
                    if(canvas[y][x].ch!=' ')
                        canvas[y][x].stage=((p+x/3+y)%(N_COLORS-1))+1;
                canvas_render(o->plain); usleep(150000);
            }
            recolor(N_COLORS); canvas_render(o->plain); usleep(300000);
        }
    } else {
        canvas_render(o->plain);
    }

    /* tagline */
    if(!g_quit&&!o->bakunin&&!o->punk&&!o->nogods){
        const char *tag=o->crass?"~ anarchy-s ~  [ Do what thou wilt ]"
                                :"~ anarchy-s ~";
        int ox2=(cw-s_w)/2; if(ox2<1)ox2=1;
        int tx=ox2+(s_w-(int)strlen(tag))/2;
        int ty=(ch_-s_h)/2+s_h+2;
        if(tx<0)tx=0;
        for(int i=0;tag[i]&&tx+i<cw-1;i++) canvas_put(tx+i,ty,tag[i],5);
        canvas_render(o->plain);
    }

    if(!g_quit&&o->nogods&&!o->bakunin){
        const char *tag="No gods, no masters";
        int ox2=(cw-s_w)/2; if(ox2<1)ox2=1;
        int tx=ox2+(s_w-(int)strlen(tag))/2;
        int ty=(ch_-s_h)/2+s_h+2;
        if(tx<0)tx=0;
        for(int i=0;tag[i]&&tx+i<cw-1;i++) canvas_put(tx+i,ty,tag[i],5);
        canvas_render(o->plain);
        if(!o->fast&&delay>0){ usleep(800000); fire_finale(o->plain); }
    }

    if(o->punk&&!g_quit){
        if(!o->fast&&delay>0){ usleep(500000); drip_blood(o->plain); }
    }

    if(o->bakunin&&!g_quit){
        if(!o->fast) usleep(600000);
        bakunin_explode(o->plain,delay);
    }
}

/* ── main ───────────────────────────────────────────────────────────────────── */
int main(int argc,char *argv[]){
    Opts o={.fast=0,.delay_us=30000,.scale=2,
            .rainbow=0,.crass=0,.bakunin=0,.kropotkin=0,
            .punk=0,.nogods=0,.plain=0};

    for(int i=1;i<argc;i++){
        if     (!strcmp(argv[i],"-f"))                          o.fast=1;
        else if(!strcmp(argv[i],"-r"))                          o.rainbow=1;
        else if(!strcmp(argv[i],"-c")||!strcmp(argv[i],"--crass"))   o.crass=1;
        else if(!strcmp(argv[i],"-p")||!strcmp(argv[i],"--punk"))    o.punk=1;
        else if(!strcmp(argv[i],"-b")||!strcmp(argv[i],"--bakunin")) o.bakunin=1;
        else if(!strcmp(argv[i],"-k")||!strcmp(argv[i],"--kropotkin"))o.kropotkin=1;
        else if(!strcmp(argv[i],"-n")||!strcmp(argv[i],"--nogods"))   o.nogods=1;
        else if(!strcmp(argv[i],"--plain"))                     o.plain=1;
        else if(!strcmp(argv[i],"-d")&&i+1<argc){
            o.delay_us=atoi(argv[++i]); if(o.delay_us<0)o.delay_us=0;
        } else if(!strcmp(argv[i],"-s")&&i+1<argc){
            o.scale=atoi(argv[++i]);
            if(o.scale<1){o.scale=1;} if(o.scale>8){o.scale=8;}
        } else if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")){
            usage(argv[0]); return 0;
        } else {
            fprintf(stderr,"Unknown option: %s\n",argv[i]);
            usage(argv[0]); return 1;
        }
    }

    COLORS=(o.punk||o.crass)?COLOR_PUNK:COLOR_DEFAULT;

    srand((unsigned)time(NULL));
    signal(SIGINT,on_sigint);
    atexit(cleanup);

    if(!o.fast){printf(CLEAR HIDE);fflush(stdout);}
    draw_anarchy(&o);

    if(!o.fast&&!g_quit&&!o.bakunin&&!o.punk&&!o.nogods){
        printf("\033[%d;1H",g_trows);
        const char *msg="[ press any key to exit ]";
        int pad=(g_tcols-(int)strlen(msg))/2;
        for(int i=0;i<pad;i++) putchar(' ');
        printf(BOLD"\033[97m%s"RESET,msg); fflush(stdout);
        struct termios oldt,newt;
        tcgetattr(STDIN_FILENO,&oldt); newt=oldt;
        newt.c_lflag&=~(ICANON|ECHO);
        tcsetattr(STDIN_FILENO,TCSANOW,&newt);
        getchar();
        tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
    }
    printf(CLEAR HOME SHOW RESET); fflush(stdout);
    return 0;
}
