/* Shared clean-room implementation for Gravity, Punch Out, and Can of Worms. */

#include <windows.h>
#include <math.h>
#include "admkit.h"

#if (defined(ADM_GRAVITY) + defined(ADM_PUNCH) + defined(ADM_WORMS)) != 1
#error Define exactly one of ADM_GRAVITY, ADM_PUNCH, or ADM_WORMS
#endif

static ADM_CANVAS g_canvas;

#if defined(ADM_GRAVITY)
typedef struct { double x, y, dx, dy; unsigned color; } BALL;
static BALL g_balls[7];
static int g_initialized;
static int render(AD_MODULE32 *p)
{
    int w,h,count=p->iControlValue[0],size=p->iControlValue[1],i,resized;
    adm_canvas_fit(p,1280,720,&w,&h); resized=g_canvas.width!=w||g_canvas.height!=h;
    if(!adm_canvas_resize(&g_canvas,w,h))return 0;
    if(count<1)count=1;if(count>7)count=7;if(size<10)size=10;if(size>30)size=30;
    if(resized)g_initialized=0;
    if(!g_initialized){for(i=0;i<7;i++){g_balls[i].x=adm_random_below(&g_canvas,w);g_balls[i].y=adm_random_below(&g_canvas,h);g_balls[i].dx=2+adm_random_below(&g_canvas,4);g_balls[i].dy=-3-adm_random_below(&g_canvas,4);g_balls[i].color=(unsigned)i*2u;}g_initialized=1;}
    if(p->iControlValue[2])adm_canvas_clear(&g_canvas,0);else adm_canvas_fade(&g_canvas,31,32);
    for(i=0;i<count;i++){BALL*b=&g_balls[i];b->dy+=0.28;b->x+=b->dx;b->y+=b->dy;if(b->x<size/2||b->x>=w-size/2){b->dx=-b->dx;b->x+=b->dx;}if(b->y>=h-size/2){b->y=h-size/2-1;b->dy=-fabs(b->dy)*0.88;}if(b->y<size/2){b->y=size/2;b->dy=fabs(b->dy);}adm_filled_ellipse(&g_canvas,(int)b->x,(int)b->y,size/2,size/2,adm_color(b->color+g_canvas.frame/20u));}
    g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;
}
#define MODULE_SEED 0x47524156u
#elif defined(ADM_PUNCH)
static int g_x,g_y,g_radius,g_shape;
static int render(AD_MODULE32 *p)
{
    int w,h,shape=p->iControlValue[0],maximum=p->iControlValue[1],speed=p->iControlValue[2],resized;uint32_t color;
    adm_canvas_fit(p,1280,720,&w,&h);resized=g_canvas.width!=w||g_canvas.height!=h;if(!adm_canvas_resize(&g_canvas,w,h))return 0;
    if(resized)g_radius=0;
    if(maximum<10)maximum=10;if(maximum>120)maximum=120;if(speed<1)speed=1;if(speed>9)speed=9;
    if(g_radius==0){g_x=adm_random_below(&g_canvas,w);g_y=adm_random_below(&g_canvas,h);g_shape=shape==4?adm_random_below(&g_canvas,4):shape;adm_canvas_clear(&g_canvas,0);}
    color=adm_color(g_canvas.frame/3u);g_radius+=speed;
    if(w<3||h<3){adm_put_pixel(&g_canvas,w/2,h/2,color);g_radius=0;}
    else
    if(g_shape==0)adm_ellipse(&g_canvas,g_x,g_y,g_radius,g_radius,color);else if(g_shape==1)adm_ellipse(&g_canvas,g_x,g_y,g_radius,g_radius/2+1,color);else if(g_shape==2)adm_rectangle(&g_canvas,g_x-g_radius,g_y-g_radius,g_x+g_radius,g_y+g_radius,color);else adm_rectangle(&g_canvas,g_x-g_radius,g_y-g_radius/2,g_x+g_radius,g_y+g_radius/2,color);
    if(g_radius>=maximum)g_radius=0;g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;
}
#define MODULE_SEED 0x50554E43u
#else
typedef struct { double x[20],y[20],angle;unsigned color; } WORM;
static WORM g_worms[20];static int g_initialized;
static int render(AD_MODULE32 *p)
{
    int w,h,wiggle=p->iControlValue[0],segments=p->iControlValue[1],count=p->iControlValue[2],i,j,resized;double turn;
    adm_canvas_fit(p,1280,720,&w,&h);resized=g_canvas.width!=w||g_canvas.height!=h;if(!adm_canvas_resize(&g_canvas,w,h))return 0;
    if(segments<2)segments=2;if(segments>20)segments=20;if(count<1)count=1;if(count>20)count=20;if(resized)g_initialized=0;
    if(!g_initialized){for(i=0;i<20;i++){double x=adm_random_below(&g_canvas,w),y=adm_random_below(&g_canvas,h);g_worms[i].angle=adm_random_below(&g_canvas,6284)/1000.0;for(j=0;j<20;j++){g_worms[i].x[j]=x-cos(g_worms[i].angle)*j*4;g_worms[i].y[j]=y-sin(g_worms[i].angle)*j*4;}g_worms[i].color=(unsigned)i;}g_initialized=1;}
    adm_canvas_fade(&g_canvas,15,16);turn=wiggle>=75?.30:wiggle>=50?.18:wiggle>=25?.08:0;
    for(i=0;i<count;i++){WORM*q=&g_worms[i];for(j=segments-1;j>0;j--){q->x[j]=q->x[j-1];q->y[j]=q->y[j-1];}q->angle+=(adm_random_below(&g_canvas,2001)-1000)/1000.0*turn;q->x[0]+=cos(q->angle)*4;q->y[0]+=sin(q->angle)*4;if(q->x[0]<0||q->x[0]>=w)q->angle=3.141592653589793-q->angle;if(q->y[0]<0||q->y[0]>=h)q->angle=-q->angle;if(q->x[0]<0)q->x[0]=0;if(q->x[0]>=w)q->x[0]=w-1;if(q->y[0]<0)q->y[0]=0;if(q->y[0]>=h)q->y[0]=h-1;for(j=1;j<segments;j++)adm_line(&g_canvas,(int)q->x[j-1],(int)q->y[j-1],(int)q->x[j],(int)q->y[j],adm_color(q->color+j/4u));}
    g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;
}
#define MODULE_SEED 0x574F524Du
#endif

__declspec(dllexport) int AD_STDCALL Module(AD_MODULE32*p){if(!p||p->cbSize<AD_MODULE32_SIZE)return 1;switch(p->dwMessage){case AD_MSG_MODULESELECTED:adm_canvas_release(&g_canvas);adm_seed(&g_canvas,MODULE_SEED^GetTickCount());
#if defined(ADM_GRAVITY)||!defined(ADM_PUNCH)
g_initialized=0;
#else
g_radius=0;
#endif
return AD_OK;case AD_MSG_PREINITIALIZE:return AD_OK;case AD_MSG_BLANK:case AD_MSG_DRAWFRAME:return render(p)?AD_OK:1;case AD_MSG_PAINT:if(!g_canvas.has_frame)return render(p)?AD_OK:1;adm_canvas_present(&g_canvas,p);return AD_OK;case AD_MSG_CLOSE:case AD_MSG_MODULEDESELECTED:adm_canvas_release(&g_canvas);return AD_OK;default:return AD_OK;}}
BOOL WINAPI DllMain(HINSTANCE i,DWORD r,LPVOID p){(void)p;if(r==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(i);if(r==DLL_PROCESS_DETACH)adm_canvas_release(&g_canvas);return TRUE;}