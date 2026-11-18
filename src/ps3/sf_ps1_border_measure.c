/* V15.52 measured PS1 black-border detector.
 * Pixel format-independent for packed 8-bit RGBA/ARGB:
 * opaque-black has at least 3 near-black channels among 4 bytes.
 * We measure dark *bands*, not the source code inside Sony's emulator.
 */
#include "sf_ps1_border_measure.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>
static int is_black(const unsigned char *p)
{
    unsigned dark=0,i;
    for(i=0;i<4;i++)if(p[i]<=20)dark++;
    return dark>=3;
}
static int dark_row(const unsigned char *pixels,unsigned w,unsigned pitch,
                    unsigned y,unsigned offset)
{
    unsigned n=w<384?w:384, black=0,j;
    const unsigned char *row=pixels+(size_t)y*pitch;
    if(n<1)return 1;
    for(j=0;j<n;j++){
        unsigned x=(unsigned)(((uint64_t)j*w)/n);
        if(is_black(row+(size_t)x*4))black++;
    }
    (void)offset;
    return black*1000u>=n*995u;
}
static int dark_col(const unsigned char *pixels,unsigned h,unsigned pitch,
                    unsigned x)
{
    unsigned n=h<384?h:384,black=0,j;
    for(j=0;j<n;j++){
        unsigned y=(unsigned)(((uint64_t)j*h)/n);
        if(is_black(pixels+(size_t)y*pitch+(size_t)x*4))black++;
    }
    return black*1000u>=n*995u;
}
int sf_ps1_analyze_rgba(const unsigned char *pixels,unsigned w,unsigned h,
                        unsigned pitch,SfPs1BorderResult *o)
{
    unsigned i,n=0, nonblack=0;
    if(!o)return -1;
    memset(o,0,sizeof(*o));
    if(!pixels || w<100 || h<100 || w>4096 || h>2160 ||
       pitch<(uint64_t)w*4u || pitch>65536u)return -1; /* max 141 MB for 2160 rows */
    o->width=w;o->height=h;
    while(o->top<h/3 && dark_row(pixels,w,pitch,o->top,0))o->top++;
    while(o->bottom<h/3 && dark_row(pixels,w,pitch,h-1-o->bottom,0))o->bottom++;
    while(o->left<w/3 && dark_col(pixels,h,pitch,o->left))o->left++;
    while(o->right<w/3 && dark_col(pixels,h,pitch,w-1-o->right))o->right++;
    o->active_width=w-o->left-o->right;
    o->active_height=h-o->top-o->bottom;
    /* Sample the interior, not just boundaries. Fully black screens,
       loading fades, pause overlays and cutscenes are inconclusive. */
    for(i=0;i<64u*36u;i++){
        unsigned x=o->left+(unsigned)(((uint64_t)(i%64u)*o->active_width+o->active_width/2u)/64u);
        unsigned y=o->top+(unsigned)(((uint64_t)(i/64u)*o->active_height+o->active_height/2u)/36u);
        if(x>=w)x=w-1;
        if(y>=h)y=h-1;
        if(!is_black(pixels+(size_t)y*pitch+(size_t)x*4))nonblack++;
        n++;
    }
    o->central_nonblack_permille=(nonblack*1000u)/n;
    if(o->central_nonblack_permille<80) {
        o->ambiguous=1;
        return -2;
    }
    o->valid=1;
    /* Strong evidence of symmetric frame bars, but could also be black
       content authored by the game. This is a *measurement* not etiology. */
    if((o->top+o->bottom)>(h*55u)/100u ||
       (o->left+o->right)>(w*55u)/100u)
        o->ambiguous=1;
    return 0;
}
