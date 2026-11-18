/* V15.52: generated screenshot data, verified pixel margins.
 * Crucially, cannot infer Sony-emulator cause from margins; only
 * classify observed near-black pixels in the provided actual screenshot.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sf_ps1_border_measure.h"
enum{W=640,H=360};
static unsigned char *pixels;
static void colorfill(int left,int top,int right,int bottom)
{
    int x,y;
    memset(pixels,0,(size_t)W*H*4);
    for(y=top;y<H-bottom;y++)for(x=left;x<W-right;x++) {
        unsigned char *p=pixels+((size_t)y*W+x)*4;
        p[0]=255;p[1]=110;p[2]=160;p[3]=180;
    }
}
int main(void)
{
    SfPs1BorderResult r;
    int rc;
    pixels=(unsigned char*)malloc((size_t)W*H*4);
    assert(pixels);
    colorfill(85,38,75,42);
    rc=sf_ps1_analyze_rgba(pixels,W,H,W*4,&r);
    assert(rc==0&&r.valid&&!r.ambiguous);
    assert(r.left==85&&r.right==75&&r.top==38&&r.bottom==42);
    assert(r.active_width==480&&r.active_height==280);
    assert(r.central_nonblack_permille>950);
    colorfill(0,0,0,0);
    rc=sf_ps1_analyze_rgba(pixels,W,H,W*4,&r);
    assert(rc==0&&r.valid);
    assert(r.left==0&&r.right==0&&r.top==0&&r.bottom==0);
    colorfill(120,0,120,0);
    rc=sf_ps1_analyze_rgba(pixels,W,H,W*4,&r);
    assert(rc==0&&r.valid&&r.left==120&&r.right==120);
    colorfill(0,0,0,0);
    memset(pixels,0,(size_t)W*H*4);
    rc=sf_ps1_analyze_rgba(pixels,W,H,W*4,&r);
    assert(rc==-2&&!r.valid&&r.ambiguous);
    assert(sf_ps1_analyze_rgba(NULL,W,H,W*4,&r)==-1);
    assert(sf_ps1_analyze_rgba(pixels,20,H,20*4,&r)==-1);
    assert(sf_ps1_analyze_rgba(pixels,W,H,W*3,&r)==-1);
    free(pixels);
    puts("PASS V15.52: exact symmetric/asymmetric bar measurements, border-free, all-black inconclusive, bad-image rejection");
    return 0;
}
