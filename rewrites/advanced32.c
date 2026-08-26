/* Shared clean-room implementation for Frost and Fire, Zooommm!, GeoBounce. */
#include <windows.h>
#include <math.h>
#include "admkit.h"
#if (defined(ADM_FROST) + defined(ADM_ZOOM) + defined(ADM_GEOBOUNCE)) != 1
#error Define exactly one advanced module variant
#endif
static ADM_CANVAS g_canvas;

#if defined(ADM_FROST)
static uint32_t frost_color(double value, int palette)
{
    int level = (int)((value + 1.0) * 127.5);
    if (level < 0) level = 0;
    if (level > 255) level = 255;
    switch (palette) {
    case 1: return ((uint32_t)(level / 3) << 16) | ((uint32_t)(level / 2) << 8) | (uint32_t)level;
    case 2: return (uint32_t)level * 0x010101u;
    case 4: return ((uint32_t)level << 16) | ((uint32_t)(255 - level) << 8) | 255u;
    case 6: return adm_color((unsigned)(8 + 8 * sin(level * .05)));
    case 7: return adm_color((unsigned)level / 6u);
    case 8: return adm_color((unsigned)(level * level) / 512u);
    default: return adm_color((unsigned)level / 16u + (unsigned)palette * 2u);
    }
}
static int render(AD_MODULE32 *p)
{
    int w, h, size = p->iControlValue[0], palette = p->iControlValue[1];
    int fast = p->iControlValue[2] != 0, x, y;
    double now = GetTickCount() / 1000.0, frequency;
    adm_canvas_fit(p, fast ? 480 : 640, fast ? 270 : 360, &w, &h);
    if (!adm_canvas_resize(&g_canvas, w, h)) return 0;
    if (size < 10) size = 10; if (size > 100) size = 100;
    if (palette < 0 || palette > 8) palette = 0;
    frequency = .050 - size * .00032;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        double value = sin(x * frequency + now) + cos(y * frequency * 1.3 - now * .7) + sin((x + y) * frequency * .7 + now * .4);
        g_canvas.pixels[(size_t)y * w + x] = frost_color(value / 3.0, palette);
    }
    g_canvas.frame++; g_canvas.has_frame = 1; adm_canvas_present(&g_canvas, p); return 1;
}
#define MODULE_SEED 0x46524F53u

#elif defined(ADM_ZOOM)
static DWORD g_started;
static uint32_t zoom_color(int iteration, int mode, DWORD phase)
{
    if (iteration >= 72) return 0;
    switch (mode) {
    case 0: return adm_color((unsigned)iteration / 4u + phase / 50u);
    case 5: case 6: case 7: return adm_color((unsigned)iteration / (unsigned)(mode - 3));
    case 11: case 12: case 13: return adm_color((unsigned)(8 + 8 * sin(iteration * (mode - 9) * .13)));
    case 19: return adm_color((unsigned)(iteration * 7) + phase);
    case 20: return adm_color((unsigned)iteration / 12u);
    default: return adm_color((unsigned)(iteration * (mode + 1)) + phase / 80u);
    }
}
static int render(AD_MODULE32 *p)
{
    int w, h, mode = p->iControlValue[0], speed = p->iControlValue[1], x, y;
    DWORD delay = (DWORD)p->iControlValue[2] * 30u, now = GetTickCount(), elapsed, active;
    double zoom, span, cx = -.743643887, cy = .131825904;
    adm_canvas_fit(p, 640, 360, &w, &h);
    if (!adm_canvas_resize(&g_canvas, w, h)) return 0;
    if (mode < 0 || mode > 20) mode = 0; if (!g_started) g_started = now;
    elapsed = now - g_started; active = elapsed > delay ? elapsed - delay : 0;
    if (active >= 90000u) { g_started = now; elapsed = active = 0; }
    zoom = exp((active / 1000.0) * (.03 + speed * .0015)); span = 2.2 / zoom;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        double cr = cx + ((double)x / w - .5) * span * w / h;
        double ci = cy + ((double)y / h - .5) * span, zr = 0, zi = 0;
        int iteration;
        for (iteration = 0; iteration < 72 && zr*zr + zi*zi <= 4; iteration++) { double next = zr*zr - zi*zi + cr; zi = 2*zr*zi + ci; zr = next; }
        g_canvas.pixels[(size_t)y * w + x] = zoom_color(iteration, mode, elapsed);
    }
    g_canvas.frame++; g_canvas.has_frame = 1; adm_canvas_present(&g_canvas, p); return 1;
}
#define MODULE_SEED 0x5A4F4F4Du

#else
typedef struct V3 { double x, y, z; } V3;
static double g_px = 100, g_py = 100, g_dx = 1, g_dy = .75;
static DWORD g_started;
static int vertices(int shape, V3 *v, double *edge)
{
    static const V3 tetra[]={{1,1,1},{-1,-1,1},{-1,1,-1},{1,-1,-1}};
    static const V3 cube[]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    static const V3 octa[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const double phi=1.618033988749895, inv=.618033988749895; int n=0,a,b,c,i;
    if(shape==0){for(i=0;i<4;i++)v[i]=tetra[i];*edge=sqrt(8.0);return 4;}
    if(shape==1){for(i=0;i<8;i++)v[i]=cube[i];*edge=2;return 8;}
    if(shape==2){for(i=0;i<6;i++)v[i]=octa[i];*edge=sqrt(2.0);return 6;}
    if(shape==3){for(a=-1;a<=1;a+=2)for(b=-1;b<=1;b+=2)for(c=-1;c<=1;c+=2)v[n++]=(V3){a,b,c};for(a=-1;a<=1;a+=2)for(b=-1;b<=1;b+=2){v[n++]=(V3){0,a*inv,b*phi};v[n++]=(V3){a*inv,b*phi,0};v[n++]=(V3){a*phi,0,b*inv};}*edge=2*inv;return n;}
    for(a=-1;a<=1;a+=2)for(b=-1;b<=1;b+=2){v[n++]=(V3){0,a,b*phi};v[n++]=(V3){a,b*phi,0};v[n++]=(V3){a*phi,0,b};}*edge=2;return n;
}
static double distance(V3 a,V3 b){double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrt(x*x+y*y+z*z);}
static int render(AD_MODULE32 *p)
{
    V3 v[20],q[20]; double edge,maxlen=1,time,movement; int w,h,shape=p->iControlValue[0],size=p->iControlValue[1],speed=p->iControlValue[2],faces=p->iControlValue[3],n,i,j,k,r,cx,cy; DWORD now=GetTickCount();
    adm_canvas_fit(p,960,720,&w,&h); if(!adm_canvas_resize(&g_canvas,w,h))return 0;
    if(shape<0||shape>4)shape=0;if(size<10)size=10;if(size>100)size=100;if(speed<0)speed=0;if(speed>100)speed=100;if(faces<0||faces>2)faces=2;
    if(!g_started)g_started=now;time=(now-g_started)/1000.0*(.2+speed*.012);movement=.35+speed*.035;n=vertices(shape,v,&edge);
    for(i=0;i<n;i++){double len=sqrt(v[i].x*v[i].x+v[i].y*v[i].y+v[i].z*v[i].z);if(len>maxlen)maxlen=len;}
    r=(w<h?w:h)*size/240;if(r<2)r=2;g_px+=g_dx*movement;g_py+=g_dy*movement;
    if(g_px<r||g_px>=w-r){g_dx=-g_dx;g_px+=g_dx*movement;}if(g_py<r||g_py>=h-r){g_dy=-g_dy;g_py+=g_dy*movement;}
    if(g_px<r)g_px=r;if(g_px>=w-r)g_px=w-r-1;if(g_py<r)g_py=r;if(g_py>=h-r)g_py=h-r-1;cx=(int)g_px;cy=(int)g_py;adm_canvas_clear(&g_canvas,0);
    for(i=0;i<n;i++){double x=v[i].x*cos(time)-v[i].z*sin(time),z=v[i].x*sin(time)+v[i].z*cos(time),y=v[i].y*cos(time*.7)-z*sin(time*.7);q[i]=(V3){cx+x*r/maxlen,cy+y*r/maxlen,z/maxlen};}
    if(faces!=1)for(i=0;i<n;i++)for(j=i+1;j<n;j++)if(fabs(distance(v[i],v[j])-edge)<.02)for(k=j+1;k<n;k++)if(fabs(distance(v[i],v[k])-edge)<.02&&fabs(distance(v[j],v[k])-edge)<.02){uint32_t fill=adm_color((unsigned)(i+j+k));adm_filled_triangle(&g_canvas,(int)q[i].x,(int)q[i].y,(int)q[j].x,(int)q[j].y,(int)q[k].x,(int)q[k].y,fill,faces==2?adm_color(14):fill);}
    for(i=0;i<n;i++)for(j=i+1;j<n;j++)if(fabs(distance(v[i],v[j])-edge)<.02)adm_line(&g_canvas,(int)q[i].x,(int)q[i].y,(int)q[j].x,(int)q[j].y,adm_color((unsigned)(i*3+j)));
    g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;
}
#define MODULE_SEED 0x47454F42u
#endif

__declspec(dllexport) int AD_STDCALL Module(AD_MODULE32 *p)
{
    if(!p||p->cbSize<AD_MODULE32_SIZE)return 1;
    switch(p->dwMessage){case AD_MSG_MODULESELECTED:adm_canvas_release(&g_canvas);adm_seed(&g_canvas,MODULE_SEED^GetTickCount());
#if defined(ADM_ZOOM)||defined(ADM_GEOBOUNCE)
    g_started=0;
#endif
    return AD_OK;case AD_MSG_PREINITIALIZE:return AD_OK;case AD_MSG_BLANK:case AD_MSG_DRAWFRAME:return render(p)?AD_OK:1;case AD_MSG_PAINT:if(!g_canvas.has_frame)return render(p)?AD_OK:1;adm_canvas_present(&g_canvas,p);return AD_OK;case AD_MSG_CLOSE:case AD_MSG_MODULEDESELECTED:adm_canvas_release(&g_canvas);return AD_OK;default:return AD_OK;}
}
BOOL WINAPI DllMain(HINSTANCE i,DWORD r,LPVOID p){(void)p;if(r==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(i);if(r==DLL_PROCESS_DETACH)adm_canvas_release(&g_canvas);return TRUE;}
