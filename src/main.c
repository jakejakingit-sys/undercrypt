#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define SW 320
#define SH 240
#define FPS_TICKS 546
#define MAX_BULLETS 32
#define MAX_SHOTS 6
#define MAX_PARTS 40
#define ROOM_W 20
#define ROOM_H 13
#define Q 4

enum { C_VOID, C_BG, C_FLOOR, C_FLOOR2, C_WALL, C_WALLHI, C_RUNE, C_RUNE_D,
       C_RED, C_ORANGE, C_GOLD, C_WHITE, C_MINT, C_BLUE, C_PURPLE, C_GREEN };
enum { ST_TITLE, ST_EXPLORE, ST_BOSS, ST_WIN, ST_LOSE, ST_PAUSE };
typedef struct { int x,y,vx,vy; uint8_t life,on,kind; } Shot;
typedef struct { int x,y,vx,vy; uint8_t life,on,kind; } Bullet;
typedef struct { int x,y,vx,vy,life,col; } Particle;
typedef struct { int x,y,w,h; } Block;

static uint16_t rng=0x91E7;
static uint32_t ticks;
static int state=ST_TITLE, floor_no, room_no, hp=6, hp_max=6, boss_hp=9, boss_max=9;
static int px=45,py=120, face=1, fire_cd,dash_cd,dash_time,echo_time,echo_x,echo_y;
static int iframes,shot_level,score,room_seed,block_n,enemy_x,enemy_y,enemy_hp;
static int boss_id,boss_clock,boss_warn_x,boss_warn_y,boss_phase,door_open;
static int paused_state=ST_EXPLORE;
static bool prev_dash,prev_enter,prev_clear,enemy_alive,heal_present;
static Block blocks[8]; static Shot shots[MAX_SHOTS]; static Bullet bullets[MAX_BULLETS]; static Particle particles[MAX_PARTS];
static const int DIR_X[16]={16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6,0,6,11,15};
static const int DIR_Y[16]={0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
static const char *const BOSS_NAMES[3]={"THE BONE REGENT","THE GLASS WIDOW","THE HOLLOW ENGINE"};

static uint16_t rnd(void){ rng^=rng<<7; rng^=rng>>9; rng^=rng<<8; return rng; }
static int ri(int n){return n?(int)(rnd()%(uint16_t)n):0;}
static int absi(int n){return n<0?-n:n;}
static int clampi(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static void color(int c){gfx_SetColor((uint8_t)c);}
static void set_theme(int theme){
  static const uint8_t base[16][3]={{0,0,0},{8,8,18},{22,24,38},{32,38,56},{58,64,78},{106,112,122},{180,164,130},{104,50,68},{235,54,58},{246,132,54},{255,204,105},{255,244,213},{80,242,220},{63,152,201},{112,74,168},{86,207,132}};
  static const uint8_t tones[3][11][3]={
    {{8,8,18},{22,24,38},{32,38,56},{58,64,78},{180,164,130},{180,164,130},{104,50,68},{80,242,220},{63,152,201},{112,74,168},{86,207,132}},
    {{3,15,23},{10,34,48},{18,57,71},{34,62,78},{111,163,174},{91,218,234},{38,128,163},{113,246,232},{54,151,199},{82,116,192},{78,207,170}},
    {{18,4,22},{44,11,37},{73,17,50},{79,36,58},{167,103,106},{255,160,93},{156,61,93},{255,214,114},{212,91,76},{164,75,155},{178,177,111}}
  };
  static const uint8_t idx[11]={C_BG,C_FLOOR,C_FLOOR2,C_WALL,C_WALLHI,C_RUNE,C_RUNE_D,C_MINT,C_BLUE,C_PURPLE,C_GREEN};
  uint16_t pal[16];
  for(int i=0;i<16;i++)pal[i]=gfx_RGBTo1555(base[i][0],base[i][1],base[i][2]);
  for(int i=0;i<11;i++){int k=idx[i];pal[k]=gfx_RGBTo1555(tones[theme%3][i][0],tones[theme%3][i][1],tones[theme%3][i][2]);}
  gfx_SetPalette(pal,sizeof(pal),0);
}
static void box(int x,int y,int w,int h,int c){color(c);gfx_FillRectangle(x,y,w,h);}
static void line(int x,int y,int x2,int y2,int c){color(c);gfx_Line(x,y,x2,y2);}
static void txt(const char*s,int x,int y,int c){gfx_SetTextFGColor((uint8_t)c);gfx_PrintStringXY(s,x,y);}
static void center(const char*s,int y,int c){int n=0;while(s[n])n++;txt(s,160-n*4,y,c);}
static void number(int n,int x,int y,int c){gfx_SetTextFGColor((uint8_t)c);gfx_SetTextXY(x,y);gfx_PrintUInt((unsigned)n,1);}
static bool overlap(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){return ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by;}
static void sparkle(int x,int y,int col,int count){for(int i=0;i<count;i++)for(int j=0;j<MAX_PARTS;j++)if(!particles[j].life){particles[j]=(Particle){x,y,(ri(9)-4)*Q,(ri(9)-6)*Q,14+ri(22),col};break;}}
static void add_bullet(int x,int y,int tx,int ty,int speed,int kind){
  int dx=tx-x,dy=ty-y,m=absi(dx)>absi(dy)?absi(dx):absi(dy);if(!m)m=1;
  for(int i=0;i<MAX_BULLETS;i++)if(!bullets[i].on){bullets[i]=(Bullet){x*Q,y*Q,dx*speed*Q/m,dy*speed*Q/m,150,1,(uint8_t)kind};break;}
}
static void add_shot(int x,int y,int tx,int ty){
  int dx=tx-x,dy=ty-y,m=absi(dx)>absi(dy)?absi(dx):absi(dy);if(!m)m=1;
  for(int i=0;i<MAX_SHOTS;i++)if(!shots[i].on){shots[i]=(Shot){x*Q,y*Q,dx*6*Q/m,dy*6*Q/m,70,1,0};break;}
}
static bool blocked(int x,int y){
  if(x<14||x>306||y<35||y>229)return true;
  for(int i=0;i<block_n;i++)if(overlap(x-4,y-5,9,11,blocks[i].x,blocks[i].y,blocks[i].w,blocks[i].h))return true;
  return false;
}
static void make_room(void){
  px=30;py=132;
  room_seed=(int)rnd();rng^=(uint16_t)(floor_no*193+room_no*71+room_seed);
  block_n=0;heal_present=(ri(3)==0);door_open=false;enemy_alive=false;enemy_hp=0;
  memset(shots,0,sizeof(shots));memset(bullets,0,sizeof(bullets));
  for(int i=0;i<5;i++){
    int w=14+ri(22),h=12+ri(18),x=35+ri(250-w),y=48+ri(150-h);
    if(overlap(x,y,w,h,px-30,py-30,60,60)||overlap(x,y,w,h,150,103,16,18)||x>250)continue;
    blocks[block_n++]=(Block){x,y,w,h};
  }
  if(room_no<4){enemy_alive=true;enemy_hp=2+shot_level/2;enemy_x=210+ri(65);enemy_y=68+ri(125);}
  px=30;py=132;face=1;
}
static void start_run(void){
  uint16_t seed=(uint16_t)(timer_Get(1)^timer_Get(2)^ticks^0xB17D);rng=seed?seed:0xA531;
  floor_no=0;room_no=0;hp=6;hp_max=6;shot_level=0;score=0;dash_cd=echo_time=fire_cd=iframes=0;
  memset(particles,0,sizeof(particles));set_theme(0);make_room();state=ST_EXPLORE;
}
static void start_boss(void){
  state=ST_BOSS;boss_id=floor_no;boss_max=boss_id==2?12:9;boss_hp=boss_max;
  boss_clock=0;boss_phase=0;boss_warn_x=px;boss_warn_y=py;door_open=false;
  px=42;py=132;block_n=0;room_seed=(int)rnd();
  memset(bullets,0,sizeof(bullets));memset(shots,0,sizeof(shots));
}
static void next_room(void){
  if(room_no==4){
    floor_no++;if(floor_no>=3){state=ST_WIN;return;}
    room_no=0;hp=clampi(hp+1,0,hp_max);shot_level++;set_theme(floor_no);
  }else room_no++;
  if(room_no==4)start_boss();else{make_room();state=ST_EXPLORE;}
}
static int target_x(void){return echo_time?echo_x:px;}
static int target_y(void){return echo_time?echo_y:py;}
static void spawn_aimed_fan(int n,int spread){
  int tx=boss_warn_x,ty=boss_warn_y;
  for(int k=0;k<n;k++){int off=(k-(n-1)/2)*spread;add_bullet(160,54,tx+off,ty,2+boss_id,1);}
}
static void spawn_ring(int cx,int cy,int count,int gap){
  int rot=(int)(ticks/130)%16;
  for(int i=0;i<count;i++){
    int a=(i*16/count+rot)&15;
    if(gap&&((a-rot+16)&15)<gap)continue;
    int tx=cx+DIR_X[a]*5,ty=cy+DIR_Y[a]*5;
    add_bullet(cx,cy,tx,ty,2+boss_id,2);
  }
}
static void boss_update(void){
  if(door_open){return;}
  boss_clock++;
  int half=boss_max/2, fast=boss_hp<=half;
  if(boss_id==0){
    if(boss_clock==1){boss_warn_x=target_x();boss_warn_y=target_y();}
    if(boss_clock==48){spawn_aimed_fan(fast?7:5,fast?24:30);}
    if(boss_clock==82){boss_warn_x=target_x();boss_warn_y=target_y();}
    if(boss_clock==112)spawn_aimed_fan(fast?5:3,26);
    if(boss_clock>=170){boss_clock=0;}
  }else if(boss_id==1){
    int period=fast?132:166, t=boss_clock%period;
    if(t==1){boss_warn_x=target_x();boss_warn_y=target_y();}
    if(t==48){boss_phase=1;}
    if(t==72){boss_phase=0;}
    if(t==period-1)boss_phase=0;
  }else{
    int period=fast?110:148,t=boss_clock%period;
    if(t==1){boss_warn_x=target_x();boss_warn_y=target_y();}
    if(t==42)spawn_ring(160,88,fast?16:12,4);
    if(t==72&&fast)spawn_aimed_fan(5,23);
  }
  for(int i=0;i<MAX_BULLETS;i++)if(bullets[i].on){Bullet*b=&bullets[i];b->x+=b->vx;b->y+=b->vy;if(b->life)b->life--;if(!b->life||b->x<-12*Q||b->x>332*Q||b->y<-12*Q||b->y>252*Q)b->on=0;
    if(b->on&&!iframes&&absi(b->x/Q-px)<8&&absi(b->y/Q-py)<8){b->on=0;hp--;iframes=46;sparkle(px,py,C_RED,9);if(hp<=0)state=ST_LOSE;}}
  if(boss_id==1&&boss_phase&&!iframes&&absi(px-boss_warn_x)<10){hp--;iframes=46;sparkle(px,py,C_RED,9);if(hp<=0)state=ST_LOSE;}
  for(int i=0;i<MAX_SHOTS;i++)if(shots[i].on){Shot*s=&shots[i];s->x+=s->vx;s->y+=s->vy;if(s->life)s->life--;if(!s->life||s->x<-10*Q||s->x>330*Q||s->y<-10*Q||s->y>250*Q)s->on=0;
    if(s->on&&!door_open&&absi(s->x/Q-160)<24&&absi(s->y/Q-58)<30){s->on=0;boss_hp--;score+=10;sparkle(160,72,C_GOLD,5);if(boss_hp<=0){boss_hp=0;door_open=true;boss_phase=2;memset(bullets,0,sizeof(bullets));memset(shots,0,sizeof(shots));}}
  }
}
static void explore_update(int left,int right,int up,int down,bool fire){
  int dx=right-left,dy=down-up;if(dx||dy){face=dx?dx:face;int nx=px+dx*(dash_time?5:2),ny=py+dy*(dash_time?5:2);if(!blocked(nx,py))px=nx;if(!blocked(px,ny))py=ny;}
  if(fire&&fire_cd==0){
    if(state==ST_EXPLORE&&enemy_alive)add_shot(px,py,enemy_x,enemy_y);
    fire_cd=shot_level?9:13;
  }
  if(state==ST_BOSS){boss_update();return;}
  if(state==ST_EXPLORE&&enemy_alive){
    int tx=target_x(),ty=target_y();
    if(enemy_x<tx-2)enemy_x++;else if(enemy_x>tx+2)enemy_x--;
    if(enemy_y<ty-2)enemy_y++;else if(enemy_y>ty+2)enemy_y--;
    if(!iframes&&absi(px-enemy_x)<11&&absi(py-enemy_y)<11){hp--;iframes=50;sparkle(px,py,C_RED,8);if(hp<=0)state=ST_LOSE;}
    for(int i=0;i<MAX_SHOTS;i++)if(shots[i].on){Shot*s=&shots[i];s->x+=s->vx;s->y+=s->vy;if(s->life)s->life--;if(!s->life)s->on=0;if(s->on&&absi(s->x/Q-enemy_x)<10&&absi(s->y/Q-enemy_y)<10){s->on=0;enemy_hp--;sparkle(enemy_x,enemy_y,C_GOLD,4);if(enemy_hp<=0){enemy_alive=false;door_open=true;score+=50;}}}
  }
  if(heal_present&&absi(px-158)<10&&absi(py-112)<10){heal_present=false;hp=clampi(hp+1,0,hp_max);score+=25;sparkle(158,112,C_MINT,12);}
  if(door_open&&px>298&&py>102&&py<154){if(room_no==4){next_room();if(state!=ST_EXPLORE)return;}else{next_room();return;}}
}
static void draw_backdrop(void){
  gfx_FillScreen(C_BG);
  for(int y=0;y<SH;y+=16)for(int x=0;x<SW;x+=16){int v=((x/16*13+y/16*7+room_seed)&7);box(x,y,15,15,(v==0)?C_FLOOR2:C_FLOOR);if(v==0){box(x+3,y+5,2,1,C_WALLHI);box(x+10,y+11,1,2,C_WALL);}}
  box(0,26,320,3,C_WALLHI);box(0,237,320,3,C_WALLHI);box(0,26,3,214,C_WALLHI);box(317,26,3,214,C_WALLHI);
  for(int x=12;x<310;x+=24){box(x,28,3,2,C_WALL);box(x,234,3,2,C_WALL);}
  for(int i=0;i<block_n;i++){Block*b=&blocks[i];box(b->x+2,b->y+3,b->w,b->h,C_VOID);box(b->x,b->y,b->w,b->h,C_WALL);box(b->x+2,b->y+2,b->w-4,2,C_WALLHI);box(b->x+4,b->y+b->h-4,b->w-8,2,C_BG);}
  if(door_open){box(302,104,15,50,C_GOLD);box(306,108,12,42,C_VOID);box(309,114,6,30,C_RUNE_D);}
  else {box(305,104,12,50,C_WALL);box(308,108,9,42,C_BG);}
  if(heal_present){box(155,108,6,9,C_MINT);box(152,111,12,4,C_WHITE);}
}
static void draw_player(void){
  if(iframes&&(ticks&3)==0)return;
  if(echo_time){box(echo_x-5,echo_y-6,10,12,C_BLUE);box(echo_x-3,echo_y-4,6,8,C_BG);box(echo_x-1,echo_y-2,3,4,C_MINT);}
  /* tiny lantern diver */
  box(px-4,py-5,9,11,C_VOID);box(px-3,py-6,7,10,C_MINT);box(px-2,py-7,5,2,C_WHITE);
  box(px-2,py-2,5,4,C_GOLD);box(px-1,py-1,2,2,C_WHITE);box(px-4,py+3,3,3,C_BLUE);box(px+2,py+3,3,3,C_BLUE);
  box(px+(face>0?4:-6),py-3,2,2,C_RUNE);
}
static void draw_enemy(void){
  if(!enemy_alive)return;
  box(enemy_x-8,enemy_y-5,17,12,C_VOID);box(enemy_x-7,enemy_y-6,15,10,C_PURPLE);box(enemy_x-5,enemy_y-8,11,3,C_RUNE_D);
  box(enemy_x-4,enemy_y-2,3,3,C_RED);box(enemy_x+2,enemy_y-2,3,3,C_RED);box(enemy_x-2,enemy_y+2,5,2,C_VOID);
  box(enemy_x-5,enemy_y+5,3,3,C_RUNE);box(enemy_x+3,enemy_y+5,3,3,C_RUNE);
}
static void draw_boss(void){
  int bob=(int)(ticks/10%3)-1;
  if(boss_id==0){
    box(126,48+bob,68,42,C_VOID);box(130,43+bob,60,41,C_WALLHI);box(139,38+bob,42,11,C_WALLHI);
    box(139,57+bob,13,8,C_VOID);box(168,57+bob,13,8,C_VOID);box(143,59+bob,5,4,C_RED);box(172,59+bob,5,4,C_RED);
    box(153,74+bob,14,4,C_VOID);for(int i=0;i<5;i++)box(154+i*3,78+bob,2,5,C_WHITE);
    for(int i=0;i<5;i++){box(133+i*11,39+bob,3,8,C_GOLD);box(135+i*11,35+bob,2,7,C_WALLHI);}
  }else if(boss_id==1){
    box(127,45+bob,66,42,C_VOID);box(132,48+bob,56,34,C_BLUE);box(139,40+bob,42,9,C_RUNE_D);
    for(int i=0;i<5;i++){line(136+i*10,73+bob,124+i*18,94,C_BLUE);line(136+i*10,73+bob,124+i*18,94,C_RUNE);}
    box(148,55+bob,24,20,C_PURPLE);box(153,59+bob,14,12,C_WHITE);box(158,62+bob,5,5,C_RED);
    box(135,82+bob,50,3,C_RUNE);box(140,87+bob,40,2,C_GOLD);
  }else{
    box(126,43+bob,68,49,C_VOID);box(131,47+bob,58,40,C_PURPLE);box(138,40+bob,44,8,C_RUNE_D);
    box(145,52+bob,30,27,C_VOID);box(149,55+bob,22,21,C_WHITE);box(154,58+bob,12,14,C_RED);box(158,62+bob,4,6,C_VOID);
    for(int i=0;i<4;i++){box(133+i*16,87+bob,8,3,C_GOLD);box(136+i*16,90+bob,3,5,C_RUNE);}
  }
  /* core / boss health */
  box(108,100,104,7,C_VOID);box(110,102,boss_max*100/12,3,C_RUNE_D);box(110,102,boss_hp*100/12,3,C_RED);
  for(int i=0;i<boss_hp;i++)box(110+i*8,102,4,3,C_GOLD);
}
static void draw_bullets(void){
  for(int i=0;i<MAX_BULLETS;i++)if(bullets[i].on){Bullet*b=&bullets[i];int x=b->x/Q,y=b->y/Q,c=b->kind==3?C_RED:b->kind==2?C_BLUE:C_ORANGE;
    box(x-3,y-3,7,7,c);box(x-1,y-1,3,3,C_WHITE);}
  for(int i=0;i<MAX_SHOTS;i++)if(shots[i].on){Shot*s=&shots[i];box(s->x/Q-2,s->y/Q-2,5,5,C_MINT);box(s->x/Q-1,s->y/Q-1,3,3,C_WHITE);}
}
static void draw_particles(void){for(int i=0;i<MAX_PARTS;i++)if(particles[i].life){Particle*p=&particles[i];p->x+=p->vx/Q;p->y+=p->vy/Q;p->life--;box(p->x,p->y,2,2,p->col);}}
static void draw_hud(void){
  box(0,0,320,25,C_VOID);txt("UNDERCRYPT",5,5,C_GOLD);txt("FLOOR",105,5,C_WALLHI);number(floor_no+1,145,5,C_WHITE);txt("ROOM",168,5,C_WALLHI);number(room_no+1,205,5,C_WHITE);txt("HP",239,5,C_RED);
  for(int i=0;i<hp_max;i++)box(258+i*9,6,6,8,i<hp?C_RED:C_WALL);
  txt("ARROWS MOVE",5,14,C_WALLHI);txt("2ND FIRE",98,14,C_MINT);txt("ALPHA ECHO DASH",180,14,C_BLUE);
}
static void draw_room(void){
  draw_backdrop();
  if(state==ST_BOSS||(state==ST_PAUSE&&paused_state==ST_BOSS)){draw_boss();
    if(!door_open){
    if(boss_id==0){
      int t=boss_clock%170;
      if(t<48||(t>=82&&t<112)){int x=boss_warn_x,y=boss_warn_y;box(x-9,y-1,19,2,C_RED);box(x-1,y-9,2,19,C_RED);box(x-3,y-3,6,6,C_WHITE);}
    }else if(boss_id==1){
      int t=boss_clock%(boss_hp<=boss_max/2?132:166);
      if(t<49){int x=boss_warn_x;for(int y=28;y<235;y+=10)box(x-3,y,7,5,C_RED);box(x-7,229,15,3,C_WHITE);}
      if(boss_phase){int x=boss_warn_x;box(x-7,28,15,207,C_RED);box(x-2,28,5,207,C_WHITE);}
    }else{
      int t=boss_clock%(boss_hp<=boss_max/2?110:148);
      if(t<42){int rot=(int)(ticks/130)%16;for(int i=0;i<16;i++){int a=(i+rot)&15;int gap=(i<4);line(160,88,160+DIR_X[a]*4,88+DIR_Y[a]*4,gap?C_MINT:C_RED);}}
    }
    }
  }else if(enemy_alive){draw_enemy();}
  draw_bullets();draw_player();draw_particles();draw_hud();
  if(state==ST_BOSS){center(BOSS_NAMES[boss_id],112,C_GOLD);if(door_open)center("GUARDIAN DEFEATED",124,C_MINT);else if(boss_phase==1)center("BEAM! MOVE!",124,C_RED);else if(boss_id==2&&boss_clock%(boss_hp<=boss_max/2?110:148)<42)center("SAFE GAP IS MINT",124,C_MINT);else center("WATCH THE MARK",124,C_WALLHI);}
  if(state==ST_EXPLORE){if(enemy_alive)center("CLEAR THE WISP",216,C_WALLHI);else if(room_no==4)center("THE GUARDIAN WAITS BEYOND",216,C_GOLD);else center("EXIT OPEN  >",216,C_MINT);}
}
static void draw_title(void){
  gfx_FillScreen(C_VOID);
  for(int i=0;i<26;i++){int x=(i*83+(int)(ticks/3))%320,y=(i*47+9)%220;box(x,y,2,2,(i%4)?C_BLUE:C_MINT);}
  center("UNDERCRYPT",34,C_GOLD);center("A DUNGEON OF MOVING LIGHT",50,C_WALLHI);
  draw_boss();
  box(47,153,226,53,C_BG);box(50,156,220,47,C_WALL);
  center("ENTER TO DESCEND",162,C_WHITE);center("ARROWS MOVE  |  2ND FIRE",178,C_MINT);center("ALPHA LEAVES AN ECHO",192,C_BLUE);
  center("THREE GUARDIANS. ONE WAY OUT.",222,C_GOLD);
}
static void draw_end(bool win){
  gfx_FillScreen(C_VOID);for(int i=0;i<32;i++){int x=(i*71+ticks)%320,y=(i*43+ticks/2)%240;box(x,y,2,2,(i&1)?C_BLUE:C_GOLD);}
  if(win){center("THE CRYPT REMEMBERS YOU",58,C_GOLD);center("The last seal breaks. Dawn reaches the stone.",82,C_WALLHI);center("SCORE",125,C_MINT);number(score,167,125,C_WHITE);center("ENTER TO DESCEND AGAIN",184,C_WHITE);center("CLEAR TO QUIT",204,C_WALLHI);}
  else{center("THE UNDERCRYPT CLAIMS ANOTHER",65,C_RED);center("Your echo fades between the walls.",91,C_WALLHI);center("FLOOR REACHED",132,C_GOLD);number(floor_no+1,196,132,C_WHITE);center("ENTER TO TRY AGAIN",177,C_WHITE);center("CLEAR TO QUIT",197,C_WALLHI);}
}
static void paint(void){
  if(state==ST_TITLE)draw_title();else if(state==ST_WIN||state==ST_LOSE)draw_end(state==ST_WIN);else draw_room();
  if(state==ST_PAUSE){box(64,82,192,70,C_VOID);center("PAUSED",98,C_GOLD);center("ENTER RESUME   CLEAR TITLE",122,C_WHITE);}
}
static void handle(void){
  kb_Scan();bool l=(kb_Data[7]&kb_Left)!=0,r=(kb_Data[7]&kb_Right)!=0,u=(kb_Data[7]&kb_Up)!=0,d=(kb_Data[7]&kb_Down)!=0;
  bool fire=(kb_Data[1]&kb_2nd)!=0,dash=(kb_Data[2]&kb_Alpha)!=0,enter=(kb_Data[6]&kb_Enter)!=0,clear=(kb_Data[6]&kb_Clear)!=0;
  if(state==ST_TITLE){if(enter&&!prev_enter)start_run();}
  else if(state==ST_WIN||state==ST_LOSE){if(enter&&!prev_enter)start_run();if(clear&&!prev_clear)state=ST_TITLE;}
  else if(state==ST_PAUSE){if(enter&&!prev_enter)state=paused_state;if(clear&&!prev_clear)state=ST_TITLE;}
  else if(clear&&!prev_clear){paused_state=state;state=ST_PAUSE;}
  else{
    if(dash&&!prev_dash&&dash_cd==0){dash_time=10;dash_cd=90;echo_time=48;echo_x=px;echo_y=py;iframes=12;sparkle(px,py,C_MINT,8);}
    if(state==ST_BOSS){int dx=r-l,dy=d-u;if(dx||dy){face=dx?dx:face;int nx=px+dx*(dash_time?5:2),ny=py+dy*(dash_time?5:2);if(!blocked(nx,py))px=nx;if(!blocked(px,ny))py=ny;}if(fire&&fire_cd==0&&!door_open){add_shot(px,py,160,60);fire_cd=shot_level?9:13;}boss_update();}
    else explore_update(l,r,u,d,fire);
  }
  if(state==ST_PAUSE&&enter&&!prev_enter)state=ST_EXPLORE;
  prev_dash=dash;prev_enter=enter;prev_clear=clear;
  if(state!=ST_TITLE&&state!=ST_WIN&&state!=ST_LOSE&&state!=ST_PAUSE){if(iframes)iframes--;if(fire_cd)fire_cd--;if(dash_time)dash_time--;if(dash_cd)dash_cd--;if(echo_time)echo_time--;}
}
int main(void){
  gfx_Begin();gfx_SetDrawBuffer();gfx_SetTextScale(1,1);set_theme(0);gfx_SetTransparentColor(0);
  timer_Enable(1,TIMER_32K,TIMER_NOINT,TIMER_UP);timer_Enable(2,TIMER_32K,TIMER_NOINT,TIMER_UP);
  for(;;){timer_Set(1,0);handle();ticks++;paint();gfx_SwapDraw();while(timer_Get(1)<FPS_TICKS){}if(kb_Data[6]&kb_Clear&&state==ST_TITLE)break;}
  gfx_End();return 0;
}
