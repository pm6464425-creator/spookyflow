/* Real filenames from the user's 26 NTFS BDISO movies. */
#include "movie_spanish.h"
#include <stdio.h>
#include <string.h>
static int pass,failed;
#define CHECK(expr) do {if(expr)pass++;else {failed++;printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);}}while(0)
static void movie(const char *file,const char *expected)
{
    char title[256]="";
    int rc=movie_spanish_title(file,NULL,title,sizeof(title));
    CHECK(rc==1);
    if(rc)CHECK(strcmp(title,expected)==0);
    CHECK(movie_filename_is_3d(file)==1);
}
int main(void)
{
    char text[256]="";
    movie("/dev_usb000/BDISO/1991 Terminator 2 el Juicio Final 3D.iso","Terminator 2: El juicio final");
    movie("2001 Shrek 3D.iso","Shrek");
    movie("2004 Yo, Robot 3D.iso","Yo, robot");
    movie("2006 Cars 3D-001.iso","Cars: Una aventura sobre ruedas");
    movie("2008 Journey to the Center of the Earth 3D-001.iso","Viaje al centro de la Tierra");
    movie("2008 Jumper 3D.iso","Jumper");
    movie("2008 Kung Fu Panda 3D.iso","Kung Fu Panda");
    movie("2009 Avatar 3D-001.iso","Avatar");
    movie("2009 Ice Age Dawn of the Dinosaurs 3D.iso","La era de hielo 3: El origen de los dinosaurios");
    movie("2009 Under the Sea IMAX 3D.iso","Bajo el mar");
    movie("2010 Clash of the Titans 3D-001.iso","Furia de titanes");
    movie("2010 How to Train Your Dragon 3D.iso","Cómo entrenar a tu dragón");
    movie("2011 Final Destination V 3D-001.iso","Destino final 5");
    movie("2011 Rio 3D-001.iso","Río");
    movie("2011 Thor 3D.iso","Thor");
    movie("2012 Dredd 3D.iso","Dredd");
    movie("2013 47 Ronin 3D.iso","47 Ronin");
    movie("2014 How to Train Your Dragon II 3D.iso","Cómo entrenar a tu dragón 2");
    movie("2014 Mr. Peabody & Sherman 3D.iso","Las aventuras de Peabody y Sherman");
    movie("2014 X-Men Days of Future Past 3D.iso","X-Men: Días del futuro pasado");
    movie("2015 In the Heart of the Sea 3D.iso","En el corazón del mar");
    movie("2015 Pixels 3D.iso","Pixeles");
    movie("2016 Batman v Superman Dawn of Justice 3D.iso","Batman vs. Superman: El origen de la justicia");
    movie("2016 Doctor Strange 3D.iso","Doctor Strange: Hechicero supremo");
    movie("2016 Suicide Squad 3D_2.iso","Escuadrón suicida");
    movie("2016 Warcraft The Beginning 3D.iso","Warcraft: El primer encuentro de dos mundos");
    /* V7 wrote an English sidecar; it must no longer control the title. */
    CHECK(movie_spanish_title("1991 Terminator 2 el Juicio Final 3D.iso",
        "Terminator 2: Judgment Day",text,sizeof(text))==1);
    CHECK(strcmp(text,"Terminator 2: El juicio final")==0);
    /* A customized Spanish title must take precedence. */
    CHECK(movie_spanish_title("1991 Terminator 2 el Juicio Final 3D.iso",
        "Mi película de acción favorita",text,sizeof(text))==0);
    CHECK(!movie_filename_is_3d("/dev_usb000/BDISO/2011 Thor.iso"));
    CHECK(!movie_filename_is_3d("THE3DLEGACY.iso"));
    CHECK(movie_filename_is_3d("movie 3D_2.iso"));
    printf("BDISO Spanish/3D C tests: %d passed, %d failed\n",pass,failed);
    return failed!=0;
}
