#!/usr/bin/env python3
"""Deterministic 2x resolution SNES-style case overlay for the PS3 coverflow.
Logical outer size 275x250; artwork aperture (18,42,240,168).
Uses the Python standard library only; the window is fully transparent.
"""
from pathlib import Path
import math
import struct
import sys
import zlib

S=2
W,H=275*S,250*S
pixels=bytearray(W*H*4)
def clamp(n):
    return max(0,min(255,int(n)))
def pixel(x,y,rgb,a=255):
    if x<0 or y<0 or x>=W or y>=H or not a: return
    k=(y*W+x)*4
    if a==255:
        pixels[k:k+4]=bytes((*rgb,255))
    else:
        old=pixels[k+3]; out=a+(old*(255-a)+127)//255
        if out:
            for j in range(3):
                pixels[k+j]=(rgb[j]*a+pixels[k+j]*old*(255-a)//255+out//2)//out
            pixels[k+3]=out
def rounded(x,y,x0,y0,x1,y1,r):
    if x<x0 or x>=x1 or y<y0 or y>=y1: return False
    cx=min(max(x,x0+r),x1-r)
    cy=min(max(y,y0+r),y1-r)
    return (x-cx)**2+(y-cy)**2<=r*r
def rectangle(x0,y0,x1,y1,rgb,a=255):
    for y in range(max(0,int(round(y0*S))),min(H,int(round(y1*S)))):
        for x in range(max(0,int(round(x0*S))),min(W,int(round(x1*S)))):
            pixel(x,y,rgb,a)
def diamond(cx,cy,r,rgb,a=255):
    X,Y,R=round(cx*S),round(cy*S),round(r*S)
    for y in range(max(0,Y-R),min(H,Y+R+1)):
        for x in range(max(0,X-R),min(W,X+R+1)):
            if abs(x-X)+abs(y-Y)<R: pixel(x,y,rgb,a)
# Authentic molded-plastic perimeter, double bevel, metallic surface texture.
for y in range(H):
    fy=(y+.5)/S
    for x in range(W):
        fx=(x+.5)/S
        if not rounded(fx,fy,6,4.2,270.6,244.8,10.2):
            if rounded(fx,fy,4.2,6,273,248.1,11):
                pixel(x,y,(4,7,17),100)
            continue
        grain=((x*29+y*41)%19-9)*.14
        lum=51+11*(1-fy/250)+8*math.sin(y*.02)+4*fx/275+grain
        if fy<11: lum+=21-(fy-4)*1.8
        if fy>234: lum-=(fy-234)*1.15
        if fx<13: lum-=16
        elif fx<18: lum+=12
        if fx>258: lum+=(4 if fx<263 else -12)
        if fy<40: lum+=22+4*math.sin(fx*.15)
        if fy>211: lum+=14
        pixel(x,y,(clamp(lum),clamp(lum+2),clamp(lum+13)))
# Case lips, ribs, inner window gasket and brushed-aluminum highlights.
for box,color,a in [
    ((7,9,269,10),(181,185,201),198),
    ((9,11,267,11.8),(29,31,47),215),
    ((7.5,37,269,40),(27,27,44),235),
    ((9,39,266,40),(164,167,186),180),
    ((7.5,211,268,213),(22,24,37),242),
    ((8.5,214,268,215),(158,163,179),174),
    ((7.5,239,268,241),(21,24,36),220),
    ((8,40,14.5,211),(39,40,61),180),
    ((261,40,267,211),(23,24,41),220),
    ((14.5,40,16.5,212),(185,189,206),177),
    ((259,41,261.5,212),(174,176,191),120),
    ((16.5,41,18,211),(11,14,23),220),
    ((258,41,259.5,211),(8,10,20),220),
    ((21.5,15.5,250,32.3),(23,24,39),255),
    ((22,16.2,250,17.1),(112,110,138),172),
    ((22,31,250,31.9),(0,0,0),180),
    ((22,34,251,35.1),(112,91,164),238),
    ((22,35.2,251,36),(46,38,76),200),
    ((21,218,251,235),(31,33,47),202),
    ((21,219,251,219.7),(167,173,187),118),
    ((21,233.3,251,234),(0,0,0),170),
]:
    rectangle(*box,color,a)
for yy in range(52,204,14):
    rectangle(9.5,yy,12.4,yy+6.5,(109,112,142),118)
for i,color in enumerate(((124,108,174),(153,139,195),(110,99,171))):
    rectangle(25+i*9,21.1,31+i*9,25.8,color,235)
    rectangle(25+i*9,21.1,31+i*9,21.8,(217,203,249),155)
for i,color in enumerate(((167,121,196),(103,142,204),(202,166,113))):
    rectangle(222+i*8,221,228+i*8,230,color,225)
# Embossed details on the title strip and lower plastic plate.
FONT={
'A':("01110","10001","10001","11111","10001","10001","10001"),
'B':("11110","10001","10001","11110","10001","10001","11110"),
'D':("11110","10001","10001","10001","10001","10001","11110"),
'E':("11111","10000","10000","11110","10000","10000","11111"),
'F':("11111","10000","10000","11110","10000","10000","10000"),
'I':("11111","00100","00100","00100","00100","00100","11111"),
'N':("10001","11001","10101","10011","10001","10001","10001"),
'O':("01110","10001","10001","10001","10001","10001","01110"),
'P':("11110","10001","10001","11110","10000","10000","10000"),
'R':("11110","10001","10001","11110","10100","10010","10001"),
'S':("01111","10000","10000","01110","00001","00001","11110"),
'T':("11111","00100","00100","00100","00100","00100","00100"),
'U':("10001","10001","10001","10001","10001","10001","01110"),
'1':("00100","01100","00100","00100","00100","00100","01110"),
'6':("01111","10000","10000","11110","10001","10001","01110"),
'-':("00000","00000","00000","11111","00000","00000","00000"),
' ':("00000",)*7}
def lettering(label,cx,y,scale_x,scale_y,color,a):
    left=cx-((len(label)*6-1)*scale_x)*.5
    for char in label:
        for row,bits in enumerate(FONT.get(char,FONT[' '])):
            for col,v in enumerate(bits):
                if v=='1':
                    rectangle(left+col*scale_x,y+row*scale_y,
                              left+(col+.82)*scale_x,y+(row+.76)*scale_y,color,a)
        left+=6*scale_x
for label,cx,y,sx,sy in (("SUPER NINTENDO",148,19.2,1.35,1.28),
                          ("16-BIT",77,222,1.35,1.30)):
    lettering(label,cx,y+.65,sx,sy,(3,4,13),145)
    lettering(label,cx,y,sx,sy,(228,231,241),239)
for cx,cy in ((12,18),(263,18),(12,230),(263,230)):
    diamond(cx,cy,3.3,(17,18,26),210)
    diamond(cx-.4,cy-.5,2.2,(126,128,144),225)
    rectangle(cx-1.35,cy-.35,cx+1.35,cy+.45,(35,37,52))
    rectangle(cx-.5,cy-1.25,cx+.4,cy+1.5,(35,37,52),180)
# Clear *exactly* the 240x168 logo/art window, letting covers show through.
for y in range(42*S,210*S):
    offset=(y*W+18*S)*4
    pixels[offset:offset+240*S*4]=bytes(240*S*4)
def chunk(kind,data):
    return (struct.pack('>I',len(data))+kind+data+
            struct.pack('>I',zlib.crc32(kind+data)&0xffffffff))
raw=bytearray()
for y in range(H):
    raw.append(0)
    raw.extend(pixels[y*W*4:(y+1)*W*4])
result=(b'\x89PNG\r\n\x1a\n'+
        chunk(b'IHDR',struct.pack('>IIBBBBB',W,H,8,6,0,0,0))+
        chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b''))
output=Path(sys.argv[1] if len(sys.argv)>1 else 'pkgfiles/USRDIR/CASES/snes.png')
output.parent.mkdir(parents=True,exist_ok=True)
output.write_bytes(result)
print("SNES case",output,W,H,"aperture",240,168,"bytes",len(result))
