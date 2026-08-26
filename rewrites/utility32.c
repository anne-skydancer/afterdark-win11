/* Shared clean-room implementation for DOS Shell, Puzzle, and Globe. */
#include <windows.h>
#include <math.h>
#include "admkit.h"

#if (defined(ADM_DOSSHELL) + defined(ADM_PUZZLE) + defined(ADM_GLOBE)) != 1
#error Define exactly one utility module variant
#endif
static ADM_CANVAS g_canvas;

#if defined(ADM_DOSSHELL)
static DWORD g_started;static int g_random_scheme;
static COLORREF text_color(int scheme){static const COLORREF c[]={RGB(255,190,60),RGB(60,255,100),RGB(235,235,235),RGB(100,220,255)};return c[scheme&3];}
static int render(AD_MODULE32*p){int w,h,tw=p->rcClient.right-p->rcClient.left,th=p->rcClient.bottom-p->rcClient.top,scheme=p->iControlValue[1],speed=p->iControlValue[2],line,font_height;DWORD now=GetTickCount(),period;HDC dc;RECT box;HBRUSH brush;HFONT font,old_font;const char*items[]={"AFTERDRK  <DIR>","MODULES   <DIR>","README   TXT","CONFIG   SYS","MEMORY   640K","COMMAND  COM"};adm_canvas_fit(p,960,540,&w,&h);if(!adm_canvas_resize(&g_canvas,w,h))return 0;if(scheme<0||scheme>4)scheme=1;if(scheme==4)scheme=g_random_scheme;period=speed>=90?80:speed>=60?180:speed>=30?400:800;if(!g_started)g_started=now;line=(int)((now-g_started)/period)%6;adm_canvas_clear(&g_canvas,0);adm_canvas_present(&g_canvas,p);dc=p->hDC;font_height=th/40;if(font_height<12)font_height=12;font=CreateFontA(font_height,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,FIXED_PITCH|FF_MODERN,"Terminal");old_font=(HFONT)SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,text_color(scheme));box.left=tw/32;box.top=th/28;box.right=tw-tw/32;box.bottom=th-th/28;brush=CreateSolidBrush(text_color(scheme));FrameRect(dc,&box,brush);DeleteObject(brush);TextOutA(dc,tw/20,th/16,"MS-DOS SHELL",12);TextOutA(dc,tw/20,th/9,"File  Options  View  Help",25);for(int i=0;i<6;i++){if(i==line){SetBkColor(dc,text_color(scheme));SetTextColor(dc,RGB(0,0,0));SetBkMode(dc,OPAQUE);}TextOutA(dc,tw/12,th/5+i*th/20,items[i],lstrlenA(items[i]));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,text_color(scheme));}TextOutA(dc,tw/20,th-th/10,"C:\\AFTERDARK> _",16);SelectObject(dc,old_font);DeleteObject(font);g_canvas.frame++;g_canvas.has_frame=1;return 1;}
#define SEED 0x444F5353u
#elif defined(ADM_PUZZLE)
static int g_tiles[64],g_n,g_blank;static DWORD g_last_move;
static void init_board(int n){int total=n*n,i;g_n=n;for(i=0;i<total;i++)g_tiles[i]=i;g_blank=total-1;for(i=0;i<total*20;i++){int x=g_blank%n,y=g_blank/n,c[4],k=0;if(x)c[k++]=g_blank-1;if(x+1<n)c[k++]=g_blank+1;if(y)c[k++]=g_blank-n;if(y+1<n)c[k++]=g_blank+n;int next=c[adm_random_below(&g_canvas,k)];g_tiles[g_blank]=g_tiles[next];g_tiles[next]=total-1;g_blank=next;}g_last_move=GetTickCount();}
static int render(AD_MODULE32*p){int w,h,size=p->iControlValue[0],speed=p->iControlValue[1],invert=p->iControlValue[3]!=0,n=size==0?8:size==1?6:4,total,i,cell,ox,oy;DWORD now=GetTickCount(),interval=speed==2?80:speed==1?220:500;adm_canvas_fit(p,960,720,&w,&h);if(!adm_canvas_resize(&g_canvas,w,h))return 0;if(g_n!=n)init_board(n);adm_canvas_clear(&g_canvas,invert?0x00FFFFFFu:0);if(w<n||h<n){adm_put_pixel(&g_canvas,w/2,h/2,invert?0:adm_color(14));g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;}if(now-g_last_move>=interval){int x=g_blank%n,y=g_blank/n,c[4],k=0;if(x)c[k++]=g_blank-1;if(x+1<n)c[k++]=g_blank+1;if(y)c[k++]=g_blank-n;if(y+1<n)c[k++]=g_blank+n;int next=c[adm_random_below(&g_canvas,k)];g_tiles[g_blank]=g_tiles[next];g_tiles[next]=n*n-1;g_blank=next;g_last_move=now;}cell=(w<h?w:h)*9/(n*10);ox=(w-cell*n)/2;oy=(h-cell*n)/2;total=n*n;for(i=0;i<total;i++){int x=ox+(i%n)*cell,y=oy+(i/n)*cell;if(i==g_blank)continue;uint32_t fill=invert?0:adm_color((unsigned)g_tiles[i]);adm_filled_triangle(&g_canvas,x+1,y+1,x+cell-2,y+1,x+1,y+cell-2,fill,adm_color(14));adm_filled_triangle(&g_canvas,x+cell-2,y+1,x+cell-2,y+cell-2,x+1,y+cell-2,fill,adm_color(14));}g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;}
#define SEED 0x50555A5Au
#else
static DWORD g_started;static int g_show_map;
static int render(AD_MODULE32*p){int w,h,tilt=p->iControlValue[0],speed=p->iControlValue[1],cx,cy,r,lat,lon;DWORD now=GetTickCount();double phase,tilt_r;adm_canvas_fit(p,960,720,&w,&h);if(!adm_canvas_resize(&g_canvas,w,h))return 0;if(tilt<-45)tilt=-45;if(tilt>45)tilt=45;if(!g_started)g_started=now;phase=(now-g_started)/1000.0*(0.15+speed*0.006);tilt_r=tilt*0.0174532925199433;cx=w/2;cy=h/2;r=(w<h?w:h)*42/100;adm_canvas_clear(&g_canvas,0);adm_ellipse(&g_canvas,cx,cy,r,r,adm_color(14));for(lat=-60;lat<=60;lat+=20){int oldx=0,oldy=0,first=1;for(lon=0;lon<=360;lon+=5){double la=lat*0.0174532925199433,lo=lon*0.0174532925199433+phase,x=cos(la)*cos(lo),y=sin(la),z=cos(la)*sin(lo),yy=y*cos(tilt_r)-z*sin(tilt_r),zz=y*sin(tilt_r)+z*cos(tilt_r);int sx=cx+(int)(x*r),sy=cy-(int)(yy*r);if(zz>=0&&!first)adm_line(&g_canvas,oldx,oldy,sx,sy,adm_color((unsigned)(lat+60)/20u+3u));oldx=sx;oldy=sy;first=zz<0;}}for(lon=0;lon<360;lon+=20){int oldx=0,oldy=0,first=1;for(lat=-90;lat<=90;lat+=3){double la=lat*0.0174532925199433,lo=lon*0.0174532925199433+phase,x=cos(la)*cos(lo),y=sin(la),z=cos(la)*sin(lo),yy=y*cos(tilt_r)-z*sin(tilt_r),zz=y*sin(tilt_r)+z*cos(tilt_r);int sx=cx+(int)(x*r),sy=cy-(int)(yy*r);if(zz>=0&&!first)adm_line(&g_canvas,oldx,oldy,sx,sy,adm_color((unsigned)lon/20u));oldx=sx;oldy=sy;first=zz<0;}}if(g_show_map){for(int band=-2;band<=2;band++){int x0=cx-r*3/4,x1=cx+r*3/4,y0=cy+band*r/5+(int)(sin(phase+band)*r/8);adm_line(&g_canvas,x0,y0,x1,y0+(band&1?r/6:-r/6),adm_color(2u+(unsigned)(band+2)));}}g_canvas.frame++;g_canvas.has_frame=1;adm_canvas_present(&g_canvas,p);return 1;}
#define SEED 0x474C4F42u
#endif

__declspec(dllexport)int AD_STDCALL Module(AD_MODULE32*p){if(!p||p->cbSize<AD_MODULE32_SIZE)return 1;switch(p->dwMessage){case AD_MSG_MODULESELECTED:adm_canvas_release(&g_canvas);adm_seed(&g_canvas,SEED^GetTickCount());
#if defined(ADM_DOSSHELL)
g_started=0;g_random_scheme=adm_random_below(&g_canvas,4);
#elif defined(ADM_PUZZLE)
g_n=0;
#else
g_started=0;
g_show_map=0;
#endif
return AD_OK;case AD_MSG_PREINITIALIZE:return AD_OK;case AD_MSG_BUTTON:
#if defined(ADM_GLOBE)
if((p->dwParam&0xFFFFu)==2u){g_show_map=!g_show_map;return render(p)?AD_OK:1;}return AD_OK;
#else
return AD_OK;
#endif
case AD_MSG_BLANK:case AD_MSG_DRAWFRAME:return render(p)?AD_OK:1;case AD_MSG_PAINT:
#if defined(ADM_DOSSHELL)
return render(p)?AD_OK:1;
#else
if(!g_canvas.has_frame)return render(p)?AD_OK:1;adm_canvas_present(&g_canvas,p);return AD_OK;
#endif
case AD_MSG_CLOSE:case AD_MSG_MODULEDESELECTED:adm_canvas_release(&g_canvas);return AD_OK;default:return AD_OK;}}
BOOL WINAPI DllMain(HINSTANCE i,DWORD r,LPVOID p){(void)p;if(r==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(i);if(r==DLL_PROCESS_DETACH)adm_canvas_release(&g_canvas);return TRUE;}