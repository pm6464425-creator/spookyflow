/* V15.46: actual diagnostic logger A/B tests (not mocked).
 * Tests a SECOND persistent audio-only file and proves:
 * - game launch/port state clues are preserved across sessions;
 * - 1,500 noisy non-audio scan events never pollute it;
 * - records are flushed while the app is still running;
 * - cap/rotation replaces the SAME single txt without new files;
 * - existing SNES report continues to work independently. */
#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_AUDIO "/tmp/spookyflow_v15_46_audio_test.txt"
static char buf[135000];
static int count(const char *s, const char *needle)
{
    int n=0;
    size_t len=strlen(needle);
    while((s=strstr(s,needle))!=NULL) { ++n;s+=len; }
    return n;
}
static size_t read_file(const char *p)
{
    FILE *f=fopen(p,"rb");
    size_t n;
    assert(f);
    n=fread(buf,1,sizeof(buf)-1,f);
    assert(!ferror(f));
    buf[n]=0;
    fclose(f);
    return n;
}
int main(int argc, char **argv)
{
    const char *snes;
    size_t n;
    int i;
    assert(argc==2);
    snes=argv[1];
    (void)remove(TEST_AUDIO);
    (void)remove(snes);

    assert(log_open(snes)==0);
    for(i=0;i<500;i++) {
        log_printf("snes: ROM_FOUND filename=spam%04d.smc",i);
        log_printf("covers: gpu snapshot spam%04d",i);
        log_printf("cobra: debug ignore%04d",i);
    }
    log_printf("audio: DIAG_SYSTEM=V15.46 AUDIO_ONLY_RPT");
    log_printf("audio: DIAG_UI_AUDIO_MODE=ISOLATED no_audioInit no_audioPortOpen");
    log_printf("audio: DIAG_LAUNCH_REQUEST platform=2 game_selected=YES");
    log_printf("audio: DIAG_BACKEND platform=PS3 stage=COBRA_MOUNT rc=0");
    log_printf("audio: DIAG_DISC_PHASE=PRE_RELEASE ui_port_state=0");
    log_printf("audio: HANDOFF_STOP_ALREADY_CLOSED");
    log_printf("audio: DIAG_DISC_PHASE=AFTER_RELEASE ui_port_state=0");
    log_printf("audio: DIAG_DISC_NEXT_STEP=RETURN_TO_XMB_GAME_AUDIO_NOT_ACCESSIBLE");
    /* The report must be durable BEFORE XMB handoff, not only on exit. */
    n=read_file(TEST_AUDIO);
    assert(n<8000);
    assert(strstr(buf,"DIAGNOSTICO EXCLUSIVO DE AUDIO V15.46"));
    assert(strstr(buf,"DIAG_BACKEND platform=PS3"));
    assert(strstr(buf,"HANDOFF_STOP_ALREADY_CLOSED"));
    assert(!strstr(buf,"ROM_FOUND"));
    assert(!strstr(buf,"covers:"));
    assert(!strstr(buf,"cobra:"));
    log_close();

    /* Opening on next visit must preserve the handoff sequence. */
    assert(log_open(snes)==0);
    log_printf("audio: DIAG_SYSTEM=V15.46 SESSION_2");
    log_printf("audio: DIAG_LAUNCH_REQUEST platform=SNES");
    log_printf("audio: DIAG_SNES_PHASE=PRE_SPAWN ui_port_state=0");
    log_printf("audio: DIAG_SNES_OWN_AUDIO=UNOBSERVABLE_AFTER_PROCESS_REPLACEMENT");
    log_close();
    n=read_file(TEST_AUDIO);
    assert(n<10000);
    assert(count(buf,"NUEVO INICIO DE SPOOKYFLOW - AUDIO V15.46")==2);
    assert(strstr(buf,"platform=PS3"));
    assert(strstr(buf,"platform=SNES"));
    assert(!strstr(buf,"ROM_FOUND"));

    /* Two more sessions should not destroy evidence early. */
    assert(log_open(snes)==0);
    log_printf("audio: DIAG_THIRD_SESSION");
    log_close();
    n=read_file(TEST_AUDIO);
    assert(count(buf,"NUEVO INICIO DE SPOOKYFLOW - AUDIO V15.46")==3);
    assert(strstr(buf,"DIAG_THIRD_SESSION"));
    assert(strstr(buf,"DIAG_BACKEND platform=PS3"));

    /* 1000 valid audio-only lines ensure hard cap, then one-file rotation. */
    assert(log_open(snes)==0);
    for(i=0;i<1000;i++)
       log_printf("audio: DIAG_STRESS%04d stage=synthetic_written_in_host_testOnly_01234567890123456789012345678901234567890",i);
    log_close();
    n=read_file(TEST_AUDIO);
    assert(n<=65536);
    assert(strstr(buf,"DIAG_STRESS"));

    assert(log_open(snes)==0);
    log_printf("audio: DIAG_AFTER_ROTATION=ONLY_CURRENT_AUDIO_EVIDENCE");
    log_close();
    n=read_file(TEST_AUDIO);
    assert(n<4096);
    assert(strstr(buf,"DIAG_AFTER_ROTATION"));
    assert(!strstr(buf,"DIAG_BACKEND platform=PS3"));
    assert(count(buf,"NUEVO INICIO DE SPOOKYFLOW - AUDIO V15.46")==1);
    /* A separate SNES TXT must still exist: do not replace its older log. */
    n=read_file(snes);
    assert(strstr(buf,"DIAGNOSTICO DE ARRANQUE SNES V15.40"));
    assert(n<96000);
    (void)remove(TEST_AUDIO);
    (void)remove(snes);
    puts("PASS V15.46 dedicated PS3 AUDIO-ONLY report: 1500 noise suppressed, 5 sessions, durable handoffs, capped same-file rotation");
    return 0;
}
