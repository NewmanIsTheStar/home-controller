/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"

#include "lwip/sockets.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#include "flash.h"
//#include "weather.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
// #include "rmtsw.h"

#include "pluto.h"
// #include "powerwall.h"
// #include "led_strip.h"

#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif



extern uint32_t unix_time;
extern WEB_VARIABLES_T web;
extern NON_VOL_VARIABLES_T config;


#define SYSTEM_SSI_TAGS \
    x(time) \
    x(dogtme) \
    x(status) \
    x(tz) \
    x(dss) \
    x(dse) \
    x(ts1) \
    x(ts2) \
    x(ts3) \
    x(ts4) \
    x(dlse) \
    x(ssid) \
    x(wpass) \
    x(dhcp) \
    x(ipad) \
    x(nmsk) \
    x(ltme) \
    x(slog) \
    x(ghsh) \
    x(dstu) \
    x(spdu) \
    x(tmpu) \
    x(uau) \
    x(stck) \
    x(ipaddr) \
    x(netmsk) \
    x(gatewy) \
    x(msck) \
    x(bfail) \
    x(cfail) \
    x(sfail) \
    x(simpe) \
    x(mweek) \
    x(colour) \
    x(swlhst) \
    x(swlurl) \
    x(swlfle) \
    x(sloge) \
    x(pertyp) \
    x(wific) \
    x(gway) \
    x(pernme) \
    x(gpiou) \
    x(gpioif) \
    x(gpioih) \
    x(gpioil) \
    x(gpiooh) \
    x(gpiool) \
    x(macadr) \
    x(hostn) \
    x(mquser) \
    x(mqpass) \
    x(mqaddr) \
    x(uptme)


/*List of SSI tags used in html files
  Notes:-
    1. This list is used to create a matching enum and string constant for each SSI tag
    2. Only append to end of list.  Do not insert, delete or reorder the existing items!
    3. Related items are assumed to be in sequence in the code e.g. days of week
*/
#define APPLICATION_SSI_TAGS \
    x(usurped)   \
    x(temp)      \
    x(wind)      \
    x(rain)      \
    x(lstpck)    \
    x(rainwk)    \
    x(sun)       \
    x(mon)       \
    x(tue)       \
    x(wed)       \
    x(thu)       \
    x(fri)       \
    x(sat)       \
    x(strt1)     \
    x(strt2)     \
    x(strt3)     \
    x(strt4)     \
    x(strt5)     \
    x(strt6)     \
    x(strt7)     \
    x(dur1)      \
    x(dur2)      \
    x(dur3)      \
    x(dur4)      \
    x(dur5)      \
    x(dur6)      \
    x(dur7)      \
    x(ecoip)     \
    x(wkrn)      \
    x(dyrn)      \
    x(wndt)      \
    x(rly)       \
    x(gpio)      \
    x(lpat)      \
    x(lspd)      \
    x(lpin)      \
    x(lrgbw)     \
    x(lnum)      \
    x(gvea)      \
    x(wthr)      \
    x(day1)      \
    x(day2)      \
    x(day3)      \
    x(day4)      \
    x(day5)      \
    x(day6)      \
    x(day7)      \
    x(gvee)      \
    x(gvei)      \
    x(gveu)      \
    x(gves)      \
    x(lie)       \
    x(lia)       \
    x(liu)       \
    x(lis)       \
    x(lstsvn)    \
    x(wfail)     \
    x(gfail)     \
    x(calpge)    \
    x(porpge)    \
    x(wse)       \
    x(rsadr1)    \
    x(rsadr2)    \
    x(rsadr3)    \
    x(rsadr4)    \
    x(rsadr5)    \
    x(rsadr6)    \
    x(rse)       \
    x(soilm1)    \
    x(soilt1)    \
    x(z1d1d)     \
    x(z1d2d)     \
    x(z1d3d)     \
    x(z1d4d)     \
    x(z1d5d)     \
    x(z1d6d)     \
    x(z1d7d)     \
    x(z2d1d)     \
    x(z2d2d)     \
    x(z2d3d)     \
    x(z2d4d)     \
    x(z2d5d)     \
    x(z2d6d)     \
    x(z2d7d)     \
    x(z3d1d)     \
    x(z3d2d)     \
    x(z3d3d)     \
    x(z3d4d)     \
    x(z3d5d)     \
    x(z3d6d)     \
    x(z3d7d)     \
    x(z4d1d)     \
    x(z4d2d)     \
    x(z4d3d)     \
    x(z4d4d)     \
    x(z4d5d)     \
    x(z4d6d)     \
    x(z4d7d)     \
    x(z5d1d)     \
    x(z5d2d)     \
    x(z5d3d)     \
    x(z5d4d)     \
    x(z5d5d)     \
    x(z5d6d)     \
    x(z5d7d)     \
    x(z6d1d)     \
    x(z6d2d)     \
    x(z6d3d)     \
    x(z6d4d)     \
    x(z6d5d)     \
    x(z6d6d)     \
    x(z6d7d)     \
    x(z7d1d)     \
    x(z7d2d)     \
    x(z7d3d)     \
    x(z7d4d)     \
    x(z7d5d)     \
    x(z7d6d)     \
    x(z7d7d)     \
    x(z8d1d)     \
    x(z8d2d)     \
    x(z8d3d)     \
    x(z8d4d)     \
    x(z8d5d)     \
    x(z8d6d)     \
    x(z8d7d)     \
    x(clpat)     \
    x(cltran)    \
    x(clreq)     \
    x(z1gpio)    \
    x(z2gpio)    \
    x(z3gpio)    \
    x(z4gpio)    \
    x(z5gpio)    \
    x(z6gpio)    \
    x(z7gpio)    \
    x(z8gpio)    \
    x(z1viz)     \
    x(z2viz)     \
    x(z3viz)     \
    x(z4viz)     \
    x(z5viz)     \
    x(z6viz)     \
    x(z7viz)     \
    x(z8viz)     \
    x(zmax)      \
    x(rpage)     \
    x(z1bviz)    \
    x(z2bviz)    \
    x(z3bviz)    \
    x(z4bviz)    \
    x(z5bviz)    \
    x(z6bviz)    \
    x(z7bviz)    \
    x(z8bviz)    \
    x(z1iviz)    \
    x(z2iviz)    \
    x(z3iviz)    \
    x(z4iviz)    \
    x(z5iviz)    \
    x(z6iviz)    \
    x(z7iviz)    \
    x(z8iviz)    \
    x(z1zviz)    \
    x(z2zviz)    \
    x(z3zviz)    \
    x(z4zviz)    \
    x(z5zviz)    \
    x(z6zviz)    \
    x(z7zviz)    \
    x(z8zviz)    \
    x(z1dur)     \
    x(irgnow)    \
    x(sp1viz)    \
    x(sp2viz)    \
    x(sp3viz)    \
    x(sp4viz)    \
    x(sp5viz)    \
    x(sp6viz)    \
    x(sp7viz)    \
    x(sp8viz)    \
    x(sp9viz)    \
    x(sp10viz)   \
    x(sp11viz)   \
    x(sp12viz)   \
    x(sp1nme)    \
    x(sp2nme)    \
    x(sp3nme)    \
    x(sp4nme)    \
    x(sp5nme)    \
    x(sp6nme)    \
    x(sp7nme)    \
    x(sp8nme)    \
    x(sp9nme)    \
    x(sp10nme)    \
    x(sp11nme)    \
    x(sp12nme)    \
    x(sp13nme)    \
    x(sp14nme)    \
    x(sp15nme)    \
    x(sp16nme)    \
    x(sp1tmp)    \
    x(sp2tmp)    \
    x(sp3tmp)    \
    x(sp4tmp)    \
    x(sp5tmp)    \
    x(sp6tmp)    \
    x(sp7tmp)    \
    x(sp8tmp)    \
    x(sp9tmp)    \
    x(sp10tmp)    \
    x(sp11tmp)    \
    x(sp12tmp)    \
    x(sp13tmp)    \
    x(sp14tmp)    \
    x(sp15tmp)    \
    x(sp16tmp)    \
    x(sp17tmp)    \
    x(sp18tmp)    \
    x(sp19tmp)    \
    x(sp20tmp)    \
    x(sp21tmp)    \
    x(sp22tmp)    \
    x(sp23tmp)    \
    x(sp24tmp)    \
    x(sp25tmp)    \
    x(sp26tmp)    \
    x(sp27tmp)    \
    x(sp28tmp)    \
    x(sp29tmp)    \
    x(sp30tmp)    \
    x(sp31tmp)    \
    x(sp32tmp)    \
    x(ts1st)    \
    x(ts2st)    \
    x(ts3st)    \
    x(ts4st)    \
    x(ts5st)    \
    x(ts6st)    \
    x(ts7st)    \
    x(ts8st)    \
    x(ts9st)    \
    x(ts10st)    \
    x(ts11st)    \
    x(ts12st)    \
    x(ts13st)    \
    x(ts14st)    \
    x(ts15st)    \
    x(ts16st)    \
    x(ts17st)    \
    x(ts18st)    \
    x(ts19st)    \
    x(ts20st)    \
    x(ts21st)    \
    x(ts22st)    \
    x(ts23st)    \
    x(ts24st)    \
    x(ts25st)    \
    x(ts26st)    \
    x(ts27st)    \
    x(ts28st)    \
    x(ts29st)    \
    x(ts30st)    \
    x(ts31st)    \
    x(ts32st)    \
    x(ts1en)    \
    x(ts2en)    \
    x(ts3en)    \
    x(ts4en)    \
    x(ts5en)    \
    x(ts6en)    \
    x(ts7en)    \
    x(ts8en)    \
    x(ts9en)    \
    x(ts10en)    \
    x(ts11en)    \
    x(ts12en)    \
    x(ts13en)    \
    x(ts14en)    \
    x(ts15en)    \
    x(ts16en)    \
    x(ts1vz)    \
    x(ts2vz)    \
    x(ts3vz)    \
    x(ts4vz)    \
    x(ts5vz)    \
    x(ts6vz)    \
    x(ts7vz)    \
    x(ts8vz)    \
    x(ts9vz)    \
    x(ts10vz)    \
    x(ts11vz)    \
    x(ts12vz)    \
    x(ts13vz)    \
    x(ts14vz)    \
    x(ts15vz)    \
    x(ts16vz)   \
    x(ts17vz)   \
    x(ts18vz)   \
    x(ts19vz)   \
    x(ts20vz)   \
    x(ts21vz)   \
    x(ts22vz)   \
    x(ts23vz)   \
    x(ts24vz)   \
    x(ts25vz)   \
    x(ts26vz)   \
    x(ts27vz)   \
    x(ts28vz)   \
    x(ts29vz)   \
    x(ts30vz)   \
    x(ts31vz)   \
    x(ts32vz)   \
    x(ts1in)    \
    x(ts2in)    \
    x(ts3in)    \
    x(ts4in)    \
    x(ts5in)    \
    x(ts6in)    \
    x(ts7in)    \
    x(ts8in)    \
    x(ts9in)    \
    x(ts10in)    \
    x(ts11in)    \
    x(ts12in)    \
    x(ts13in)    \
    x(ts14in)    \
    x(ts15in)    \
    x(ts16in)    \
    x(pwip)      \
    x(pwhost)    \
    x(pwpass)    \
    x(pwgdhd)    \
    x(pwgdci)    \
    x(pwblhd)    \
    x(pwblhe)    \
    x(pwblcd)    \
    x(pwblce)    \
    x(tday)      \
    x(tpst)      \
    x(tptmp)     \
    x(tphtmp)     \
    x(tpctmp)     \
    x(tsaddvz)   \
    x(tg0_0) \
    x(tg0_1) \
    x(tg0_2) \
    x(tg0_3) \
    x(tg0_4) \
    x(tg0_5) \
    x(tg0_6) \
    x(tg0_7) \
    x(tg1_0) \
    x(tg1_1) \
    x(tg1_2) \
    x(tg1_3) \
    x(tg1_4) \
    x(tg1_5) \
    x(tg1_6) \
    x(tg1_7) \
    x(tg2_0) \
    x(tg2_1) \
    x(tg2_2) \
    x(tg2_3) \
    x(tg2_4) \
    x(tg2_5) \
    x(tg2_6) \
    x(tg2_7) \
    x(tg3_0) \
    x(tg3_1) \
    x(tg3_2) \
    x(tg3_3) \
    x(tg3_4) \
    x(tg3_5) \
    x(tg3_6) \
    x(tg3_7) \
    x(tg4_0) \
    x(tg4_1) \
    x(tg4_2) \
    x(tg4_3) \
    x(tg4_4) \
    x(tg4_5) \
    x(tg4_6) \
    x(tg4_7) \
    x(tg5_0) \
    x(tg5_1) \
    x(tg5_2) \
    x(tg5_3) \
    x(tg5_4) \
    x(tg5_5) \
    x(tg5_6) \
    x(tg5_7) \
    x(tg6_0) \
    x(tg6_1) \
    x(tg6_2) \
    x(tg6_3) \
    x(tg6_4) \
    x(tg6_5) \
    x(tg6_6) \
    x(tg6_7) \
    x(tg7_0) \
    x(tg7_1) \
    x(tg7_2) \
    x(tg7_3) \
    x(tg7_4) \
    x(tg7_5) \
    x(tg7_6) \
    x(tg7_7) \
    x(tct)   \
    x(tcs)   \
    x(thgpio) \
    x(tcgpio) \
    x(tfgpio) \
    x(tsadr1)    \
    x(tsadr2)    \
    x(tsadr3)    \
    x(tsadr4)    \
    x(tsadr5)    \
    x(tsadr6)    \
    x(tchs) \
    x(tccs) \
    x(grids) \
    x(batp) \
    x(tacgpio) \
    x(tadgpio) \
    x(tlcgpio) \
    x(tldgpio) \
    x(tbugpio) \
    x(tbdgpio) \
    x(tbmgpio) \
    x(htclm) \
    x(mhonm) \
    x(mconm) \
    x(mhoffm) \
    x(mcoffm) \
    x(tint) \
    x(tpsm1) \
    x(tpsm2) \
    x(tpsm3) \
    x(tpsm4) \
    x(tpsm5) \
    x(tpsm6) \
    x(sp1mde) \
    x(sp2mde) \
    x(sp3mde) \
    x(sp4mde) \
    x(sp5mde) \
    x(sp6mde) \
    x(sp7mde) \
    x(sp8mde) \
    x(sp9mde) \
    x(sp10mde) \
    x(sp11mde) \
    x(sp12mde) \
    x(sp13mde) \
    x(sp14mde) \
    x(sp15mde) \
    x(sp16mde) \
    x(sp17mde) \
    x(sp18mde) \
    x(sp19mde) \
    x(sp20mde) \
    x(sp21mde) \
    x(sp22mde) \
    x(sp23mde) \
    x(sp24mde) \
    x(sp25mde) \
    x(sp26mde) \
    x(sp27mde) \
    x(sp28mde) \
    x(sp29mde) \
    x(sp30mde) \
    x(sp31mde) \
    x(sp32mde) \
    x(hvachys)\
    x(this1) \
    x(this2) \
    x(this3) \
    x(this4) \
    x(this5) \
    x(this6) \
    x(this7) \
    x(this8) \
    x(this9) \
    x(this10) \
    x(this11) \
    x(this12) \
    x(this13) \
    x(this14) \
    x(this15) \
    x(this16) \
    x(this17) \
    x(this18) \
    x(this19) \
    x(this20) \
    x(this21) \
    x(this22) \
    x(this23) \
    x(this24) \
    x(this25) \
    x(this26) \
    x(this27) \
    x(this28) \
    x(this29) \
    x(this30) \
    x(this31) \
    x(this32) \
    x(this33) \
    x(this34) \
    x(this35) \
    x(this36) \
    x(this37) \
    x(this38) \
    x(this39) \
    x(this40) \
    x(this41) \
    x(this42) \
    x(this43) \
    x(this44) \
    x(this45) \
    x(this46) \
    x(this47) \
    x(this48) \
    x(this49) \
    x(this50) \
    x(this51) \
    x(this52) \
    x(this53) \
    x(this54) \
    x(this55) \
    x(this56) \
    x(this57) \
    x(this58) \
    x(this59) \
    x(this60) \
    x(this61) \
    x(this62) \
    x(this63) \
    x(this64) \
    x(this65) \
    x(this66) \
    x(this67) \
    x(this68) \
    x(this69) \
    x(this70) \
    x(this71) \
    x(this72) \
    x(this73) \
    x(this74) \
    x(this75) \
    x(this76) \
    x(this77) \
    x(this78) \
    x(this79) \
    x(this80) \
    x(this81) \
    x(this82) \
    x(this83) \
    x(this84) \
    x(this85) \
    x(this86) \
    x(this87) \
    x(this88) \
    x(this89) \
    x(this90) \
    x(this91) \
    x(this92) \
    x(this93) \
    x(this94) \
    x(this95) \
    x(this96) \
    x(this97) \
    x(this98) \
    x(this99) \
    x(this100) \
    x(cmplte)   \
    x(c1d1d)     \
    x(c1d2d)     \
    x(c1d3d)     \
    x(c1d4d)     \
    x(c1d5d)     \
    x(c1d6d)     \
    x(c1d7d)     \
    x(c2d1d)     \
    x(c2d2d)     \
    x(c2d3d)     \
    x(c2d4d)     \
    x(c2d5d)     \
    x(c2d6d)     \
    x(c2d7d)     \
    x(c3d1d)     \
    x(c3d2d)     \
    x(c3d3d)     \
    x(c3d4d)     \
    x(c3d5d)     \
    x(c3d6d)     \
    x(c3d7d)     \
    x(c4d1d)     \
    x(c4d2d)     \
    x(c4d3d)     \
    x(c4d4d)     \
    x(c4d5d)     \
    x(c4d6d)     \
    x(c4d7d)     \
    x(c5d1d)     \
    x(c5d2d)     \
    x(c5d3d)     \
    x(c5d4d)     \
    x(c5d5d)     \
    x(c5d6d)     \
    x(c5d7d)     \
    x(c6d1d)     \
    x(c6d2d)     \
    x(c6d3d)     \
    x(c6d4d)     \
    x(c6d5d)     \
    x(c6d6d)     \
    x(c6d7d)     \
    x(c7d1d)     \
    x(c7d2d)     \
    x(c7d3d)     \
    x(c7d4d)     \
    x(c7d5d)     \
    x(c7d6d)     \
    x(c7d7d)     \
    x(c8d1d)     \
    x(c8d2d)     \
    x(c8d3d)     \
    x(c8d4d)     \
    x(c8d5d)     \
    x(c8d6d)     \
    x(c8d7d)     \
    x(ctrt1)     \
    x(ctrt2)     \
    x(ctrt3)     \
    x(ctrt4)     \
    x(ctrt5)     \
    x(ctrt6)     \
    x(ctrt7)     \
    x(tempth)    \
    x(disbri)    \
    x(ttma)      \
    x(ttgrd)     \
    x(ttpred)    \
    x(disdig)    \
    x(rs1viz)    \
    x(rs2viz)    \
    x(rs3viz)    \
    x(rs4viz)    \
    x(rs5viz)    \
    x(rs6viz)    \
    x(rs7viz)    \
    x(rs8viz)    \
    x(rsrly1)    \
    x(rsrly2)    \
    x(rsrly3)    \
    x(rsrly4)    \
    x(rsrly5)    \
    x(rsrly6)    \
    x(rsrly7)    \
    x(rsrly8)    \
    x(rs1nme)    \
    x(rs2nme)    \
    x(rs3nme)    \
    x(rs4nme)    \
    x(rs5nme)    \
    x(rs6nme)    \
    x(rs7nme)    \
    x(rs8nme)    \
    x(rsoff1)    \
    x(rsoff2)    \
    x(rsoff3)    \
    x(rsoff4)    \
    x(rsoff5)    \
    x(rsoff6)    \
    x(rsoff7)    \
    x(rsoff8)    \
    x(rs1gpio)   \
    x(rs2gpio)   \
    x(rs3gpio)   \
    x(rs4gpio)   \
    x(rs5gpio)   \
    x(rs6gpio)   \
    x(rs7gpio)   \
    x(rs8gpio)   \
    x(rsmax)     \
    x(rs1actr1)    \
    x(rs1actr2)    \
    x(rs1actr3)    \
    x(rs1actr4)    \
    x(rs1actr5)    \
    x(rs1actr6)    \
    x(rs1actr7)    \
    x(rs1actr8)    \
    x(rs2actr1)    \
    x(rs2actr2)    \
    x(rs2actr3)    \
    x(rs2actr4)    \
    x(rs2actr5)    \
    x(rs2actr6)    \
    x(rs2actr7)    \
    x(rs2actr8)    \
    x(rs3actr1)    \
    x(rs3actr2)    \
    x(rs3actr3)    \
    x(rs3actr4)    \
    x(rs3actr5)    \
    x(rs3actr6)    \
    x(rs3actr7)    \
    x(rs3actr8)    \
    x(rs4actr1)    \
    x(rs4actr2)    \
    x(rs4actr3)    \
    x(rs4actr4)    \
    x(rs4actr5)    \
    x(rs4actr6)    \
    x(rs4actr7)    \
    x(rs4actr8)    \
    x(rs5actr1)    \
    x(rs5actr2)    \
    x(rs5actr3)    \
    x(rs5actr4)    \
    x(rs5actr5)    \
    x(rs5actr6)    \
    x(rs5actr7)    \
    x(rs5actr8)    \
    x(rs6actr1)    \
    x(rs6actr2)    \
    x(rs6actr3)    \
    x(rs6actr4)    \
    x(rs6actr5)    \
    x(rs6actr6)    \
    x(rs6actr7)    \
    x(rs6actr8)    \
    x(rs7actr1)    \
    x(rs7actr2)    \
    x(rs7actr3)    \
    x(rs7actr4)    \
    x(rs7actr5)    \
    x(rs7actr6)    \
    x(rs7actr7)    \
    x(rs7actr8)    \
    x(rs8actr1)    \
    x(rs8actr2)    \
    x(rs8actr3)    \
    x(rs8actr4)    \
    x(rs8actr5)    \
    x(rs8actr6)    \
    x(rs8actr7)    \
    x(rs8actr8)    \
    x(rs9actr1)    \
    x(rs9actr2)    \
    x(rs9actr3)    \
    x(rs9actr4)    \
    x(rs9actr5)    \
    x(rs9actr6)    \
    x(rs9actr7)    \
    x(rs9actr8)    \
    x(rs10actr1)    \
    x(rs10actr2)    \
    x(rs10actr3)    \
    x(rs10actr4)    \
    x(rs10actr5)    \
    x(rs10actr6)    \
    x(rs10actr7)    \
    x(rs10actr8)    \
    x(rs11actr1)    \
    x(rs11actr2)    \
    x(rs11actr3)    \
    x(rs11actr4)    \
    x(rs11actr5)    \
    x(rs11actr6)    \
    x(rs11actr7)    \
    x(rs11actr8)    \
    x(rs12actr1)    \
    x(rs12actr2)    \
    x(rs12actr3)    \
    x(rs12actr4)    \
    x(rs12actr5)    \
    x(rs12actr6)    \
    x(rs12actr7)    \
    x(rs12actr8)    \
    x(rs13actr1)    \
    x(rs13actr2)    \
    x(rs13actr3)    \
    x(rs13actr4)    \
    x(rs13actr5)    \
    x(rs13actr6)    \
    x(rs13actr7)    \
    x(rs13actr8)    \
    x(rs14actr1)    \
    x(rs14actr2)    \
    x(rs14actr3)    \
    x(rs14actr4)    \
    x(rs14actr5)    \
    x(rs14actr6)    \
    x(rs14actr7)    \
    x(rs14actr8)    \
    x(rs15actr1)    \
    x(rs15actr2)    \
    x(rs15actr3)    \
    x(rs15actr4)    \
    x(rs15actr5)    \
    x(rs15actr6)    \
    x(rs15actr7)    \
    x(rs15actr8)    \
    x(rs16actr1)    \
    x(rs16actr2)    \
    x(rs16actr3)    \
    x(rs16actr4)    \
    x(rs16actr5)    \
    x(rs16actr6)    \
    x(rs16actr7)    \
    x(rs16actr8)    \
    x(rs17actr1)    \
    x(rs17actr2)    \
    x(rs17actr3)    \
    x(rs17actr4)    \
    x(rs17actr5)    \
    x(rs17actr6)    \
    x(rs17actr7)    \
    x(rs17actr8)    \
    x(rs18actr1)    \
    x(rs18actr2)    \
    x(rs18actr3)    \
    x(rs18actr4)    \
    x(rs18actr5)    \
    x(rs18actr6)    \
    x(rs18actr7)    \
    x(rs18actr8)    \
    x(rs19actr1)    \
    x(rs19actr2)    \
    x(rs19actr3)    \
    x(rs19actr4)    \
    x(rs19actr5)    \
    x(rs19actr6)    \
    x(rs19actr7)    \
    x(rs19actr8)    \
    x(rs20actr1)    \
    x(rs20actr2)    \
    x(rs20actr3)    \
    x(rs20actr4)    \
    x(rs20actr5)    \
    x(rs20actr6)    \
    x(rs20actr7)    \
    x(rs20actr8)    \
    x(rs21actr1)    \
    x(rs21actr2)    \
    x(rs21actr3)    \
    x(rs21actr4)    \
    x(rs21actr5)    \
    x(rs21actr6)    \
    x(rs21actr7)    \
    x(rs21actr8)    \
    x(rs22actr1)    \
    x(rs22actr2)    \
    x(rs22actr3)    \
    x(rs22actr4)    \
    x(rs22actr5)    \
    x(rs22actr6)    \
    x(rs22actr7)    \
    x(rs22actr8)    \
    x(rs23actr1)    \
    x(rs23actr2)    \
    x(rs23actr3)    \
    x(rs23actr4)    \
    x(rs23actr5)    \
    x(rs23actr6)    \
    x(rs23actr7)    \
    x(rs23actr8)    \
    x(rs24actr1)    \
    x(rs24actr2)    \
    x(rs24actr3)    \
    x(rs24actr4)    \
    x(rs24actr5)    \
    x(rs24actr6)    \
    x(rs24actr7)    \
    x(rs24actr8)    \
    x(rs25actr1)    \
    x(rs25actr2)    \
    x(rs25actr3)    \
    x(rs25actr4)    \
    x(rs25actr5)    \
    x(rs25actr6)    \
    x(rs25actr7)    \
    x(rs25actr8)    \
    x(rs26actr1)    \
    x(rs26actr2)    \
    x(rs26actr3)    \
    x(rs26actr4)    \
    x(rs26actr5)    \
    x(rs26actr6)    \
    x(rs26actr7)    \
    x(rs26actr8)    \
    x(rs27actr1)    \
    x(rs27actr2)    \
    x(rs27actr3)    \
    x(rs27actr4)    \
    x(rs27actr5)    \
    x(rs27actr6)    \
    x(rs27actr7)    \
    x(rs27actr8)    \
    x(rs28actr1)    \
    x(rs28actr2)    \
    x(rs28actr3)    \
    x(rs28actr4)    \
    x(rs28actr5)    \
    x(rs28actr6)    \
    x(rs28actr7)    \
    x(rs28actr8)    \
    x(rs29actr1)    \
    x(rs29actr2)    \
    x(rs29actr3)    \
    x(rs29actr4)    \
    x(rs29actr5)    \
    x(rs29actr6)    \
    x(rs29actr7)    \
    x(rs29actr8)    \
    x(rs30actr1)    \
    x(rs30actr2)    \
    x(rs30actr3)    \
    x(rs30actr4)    \
    x(rs30actr5)    \
    x(rs30actr6)    \
    x(rs30actr7)    \
    x(rs30actr8)    \
    x(rs31actr1)    \
    x(rs31actr2)    \
    x(rs31actr3)    \
    x(rs31actr4)    \
    x(rs31actr5)    \
    x(rs31actr6)    \
    x(rs31actr7)    \
    x(rs31actr8)    \
    x(rs32actr1)    \
    x(rs32actr2)    \
    x(rs32actr3)    \
    x(rs32actr4)    \
    x(rs32actr5)    \
    x(rs32actr6)    \
    x(rs32actr7)    \
    x(rs32actr8)    \
    x(rsst)         \
    x(rs1st)        \
    x(rs2st)        \
    x(rs3st)        \
    x(rs4st)        \
    x(rs5st)        \
    x(rs6st)        \
    x(rs7st)        \
    x(rs8st)        \
    x(rs9st)        \
    x(rs10st)       \
    x(rs11st)       \
    x(rs12st)       \
    x(rs13st)       \
    x(rs14st)       \
    x(rs15st)       \
    x(rs16st)       \
    x(rs17st)       \
    x(rs18st)       \
    x(rs19st)       \
    x(rs20st)       \
    x(rs21st)       \
    x(rs22st)       \
    x(rs23st)       \
    x(rs24st)       \
    x(rs25st)       \
    x(rs26st)       \
    x(rs27st)       \
    x(rs28st)       \
    x(rs29st)       \
    x(rs30st)       \
    x(rs31st)       \
    x(rs32st)       \
    x(rs1vz)            \
    x(rs2vz)            \
    x(rs3vz)            \
    x(rs4vz)            \
    x(rs5vz)            \
    x(rs6vz)            \
    x(rs7vz)            \
    x(rs8vz)            \
    x(rs9vz)            \
    x(rs10vz)            \
    x(rs11vz)            \
    x(rs12vz)            \
    x(rs13vz)            \
    x(rs14vz)            \
    x(rs15vz)            \
    x(rs16vz)            \
    x(rs17vz)            \
    x(rs18vz)            \
    x(rs19vz)            \
    x(rs20vz)            \
    x(rs21vz)            \
    x(rs22vz)            \
    x(rs23vz)            \
    x(rs24vz)            \
    x(rs25vz)            \
    x(rs26vz)            \
    x(rs27vz)            \
    x(rs28vz)            \
    x(rs29vz)            \
    x(rs30vz)            \
    x(rs31vz)            \
    x(rs32vz)            \
    x(rsaddvz)           \
    x(rsact1)            \
    x(rsact2)            \
    x(rsact3)            \
    x(rsact10)       \
    x(rsact11)       \
    x(rsact12)       \
    x(rsact20)       \
    x(rsact21)       \
    x(rsact22)       \
    x(rsact30)       \
    x(rsact31)       \
    x(rsact32)       \
    x(rsact40)       \
    x(rsact41)       \
    x(rsact42)       \
    x(rsact50)       \
    x(rsact51)       \
    x(rsact52)       \
    x(rsact60)       \
    x(rsact61)       \
    x(rsact62)       \
    x(rsact70)       \
    x(rsact71)       \
    x(rsact72)       \
    x(rsact80)       \
    x(rsact81)       \
    x(rsact82)       \
    x(rsday)         \
    x(rscpy)         \
    x(rscvz1)        \
    x(rscvz2)        \
    x(rscvz3)        \
    x(rscvz4)        \
    x(rscvz5)        \
    x(rscvz6)        \
    x(rscvz7)        \
    x(rscvz8)        \
    x(rscvz9)        \
    x(rscvz10)       \
    x(rscvz11)       \
    x(home)          \
    x(rs1nc)            \
    x(rs2nc)            \
    x(rs3nc)            \
    x(rs4nc)            \
    x(rs5nc)            \
    x(rs6nc)            \
    x(rs7nc)            \
    x(rs8nc) \
    x(rsbulb1) \
    x(rsbulb2) \
    x(rsbulb3) \
    x(rsbulb4) \
    x(rsbulb5) \
    x(rsbulb6) \
    x(rsbulb7) \
    x(rsbulb8) \
    x(eanme) \
    x(tfnme) \
    x(afnme)


    
//enum used to index array of pointers to SSI string constants  e.g. index 0 is SSI_usurped
enum ssi_index
{
#define x(name) SSI_ ## name,
SYSTEM_SSI_TAGS
APPLICATION_SSI_TAGS
#undef x
};

// array of pointers to SSI string constants
const char * ssi_tags[] =
{
#define x(name) #name,
SYSTEM_SSI_TAGS
APPLICATION_SSI_TAGS
#undef x
};


u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen, void *connection_state)
{
    size_t printed;
    char timestamp[50];
    uint32_t us_now;
    long temp;
    long upper;
    long lower;
    int i;
    bool new_thermostat_period_found = false;
    int grid_x = 0;
    int grid_y = 0;
    bool first_item_printed = false;
    char gpio_list[192];
    int schedule_slot;
    int schedule_relay;

    switch(iIndex) {

        // *** system SSI start ***
        case SSI_time: // time
        {
            if(!get_timestamp(timestamp, sizeof(timestamp), false, false)) {
                printed = snprintf(pcInsert, iInsertLen, "%s", timestamp);
            }
            else {
                printed = snprintf(pcInsert, iInsertLen, "Time unavailable");   
            }
        }
        break;
        case SSI_dogtme: // dogtme
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.watchdog_timestring); 
        }  
        break; 
        case SSI_status: // status
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.status_message); 
        }  
        break;
        case SSI_tz:
        {
            if (cfg->timezone_offset > 0)
            {
                // leading + sign added
                if (cfg->timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "+%d", cfg->timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "+%d:%d", cfg->timezone_offset/60, abs(cfg->timezone_offset%60));    
                }                
            }
            else
            {
                // leading - sign automatically added
                if (cfg->timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "%d", cfg->timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "%d:%d", cfg->timezone_offset/60, abs(cfg->timezone_offset%60));    
                }
            }
        }
        break;
        case SSI_dss:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->daylightsaving_start);
        }
        break;
        case SSI_dse:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->daylightsaving_end);
        }
        break;   
        case SSI_ts1:    // ts1
        case SSI_ts2:    // ts2
        case SSI_ts3:    // ts3
        case SSI_ts4:    // ts4
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->time_server[iIndex-SSI_ts1]); 
        }  
        break; 
        case SSI_dlse:    // dlse -- daylight saving enable
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->daylightsaving_enable?"checked":""); 
        }  
        break;    
       case SSI_ssid: //ssid
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->wifi_ssid);
        }               
        break;    
        case SSI_wpass: //wpass
        {
            //printed = snprintf(pcInsert, iInsertLen, "%s", cfg->wifi_password);
            printed = snprintf(pcInsert, iInsertLen, "********");
        }               
        break;    
        case SSI_dhcp: //dhcp
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->dhcp_enable?"checked":"");
        }               
        break;    
        case SSI_ipad: //ipad
        {
            if (!cfg->dhcp_enable)
            {
                if (strncasecmp(cfg->ip_address, "automatic+via+DHCP", sizeof(cfg->ip_address))==0)
                {
                    //cfg->ip_address[0] = 0;
                    STRNCPY(cfg->ip_address, web.ip_address_string, sizeof(cfg->ip_address));   
                }
                printed = snprintf(pcInsert, iInsertLen, "%s", cfg->ip_address);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }            
        }               
        break;    
        case SSI_nmsk: //nmsk
        {
            if (!cfg->dhcp_enable)
            {
                if (strncasecmp(cfg->network_mask, "automatic+via+DHCP", sizeof(cfg->network_mask))==0)
                {
                    //cfg->network_mask[0] = 0;
                    STRNCPY(cfg->network_mask, web.network_mask_string, sizeof(cfg->network_mask));  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", cfg->network_mask);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }                          
        }               
        break;  
        case SSI_ltme: //ltme
        {
            printed = get_local_time_string(pcInsert, iInsertLen);
        }               
        break;
        case SSI_slog: //slog
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->syslog_server_ip);
        }
        break; 
        case SSI_ghsh: //ghsh
        {
            #ifdef USE_GIT_HASH_AS_VERSION
            printed = snprintf(pcInsert, iInsertLen, "%s", GITHASH);
            #else
            printed = snprintf(pcInsert, iInsertLen, "%s", PLUTO_VER);
            #endif
        }                        
        break;
        case SSI_dstu: //dstu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_archaic_units?"inches":"mm"); 
        }                        
        break;  
        case SSI_spdu: //spdu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_archaic_units?"ft/s":"m/s"); 
        }                        
        break;  
        case SSI_tmpu: //tmpu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_archaic_units?"F":"C"); 
        }                        
        break;
        case SSI_uau: //uau
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_archaic_units?"checked":"");
        }               
        break; 
        case SSI_stck: //stck
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.stack_message);
        }               
        break; 
        case SSI_ipaddr: //ipaddr
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.ip_address_string);
        }               
        break;   
        case SSI_netmsk: //netmsk
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.network_mask_string);
        }               
        break;   
        case SSI_gatewy: //gatewy
        {
            if (!cfg->dhcp_enable)
            {
                if (strncasecmp(cfg->gateway, "automatic+via+DHCP", sizeof(cfg->gateway))==0)
                {
                    //cfg->gateway[0] = 0;
                    STRNCPY(cfg->gateway, web.gateway_string, sizeof(cfg->gateway));                  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", cfg->gateway);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }                          
        }                   
        break;
        case SSI_msck: //msck
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.socket_max);
        }               
        break;  
        case SSI_bfail: //bfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.bind_failures);
        }               
        break;  
        case SSI_cfail: //cfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.connect_failures);
        }               
        break;  
        case SSI_sfail: //sfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.syslog_transmit_failures);
        }               
        break;  
        case SSI_simpe: //simpe
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_simplified_english?"checked":"");
        }  
        break;         
        case SSI_mweek: //mweek
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_monday_as_week_start?"checked":"");
        }  
        break;         
        case SSI_colour: //colour
        {
            if (cfg->use_simplified_english)
            {
                printed = snprintf(pcInsert, iInsertLen, "color");
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "colour");
            }
        }                                                                                                                                                   
        break; 
        case SSI_swlhst: //swlhst
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_server);
        }               
        break; 
        case SSI_swlurl: //swlurl
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_url);
        }               
        break; 
        case SSI_swlfle: //swlfle
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.software_file);
        }               
        break;    
        case SSI_sloge: //sloge
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->syslog_enable?"checked":"");
        } 
        break;
        case SSI_pertyp: //pertyp
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", cfg->personality);
        } 
        break; 
        case SSI_wific: //wific
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->wifi_country);
        } 
        break;  
        case SSI_gway: //gway
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.gateway_string);
        }                       
        break;
        case SSI_pernme: //pernme
        {
            switch(cfg->personality)
            {
            case SPRINKLER_USURPER:
                printed = snprintf(pcInsert, iInsertLen, "Sprinkler Usurper");
                break;
            case SPRINKLER_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "Sprinkler Controller");
                break;
            case LED_STRIP_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "LED Controller");
                break;
            case HVAC_THERMOSTAT:
                printed = snprintf(pcInsert, iInsertLen, "HVAC Thermostat");
                break;     
            case HOME_CONTROLLER:
                printed = snprintf(pcInsert, iInsertLen, "Home Controller");
                break;                                             
            default:
            case NO_PERSONALITY:
                printed = snprintf(pcInsert, iInsertLen, "No personality");
                break;
            }
        }             
        break;
        case SSI_gpiou:
        {
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_UNINITIALIZED);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);
        }
        break;  
        case SSI_gpioif:
        {

            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_FLOATING);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);
        
        }
        break; 
        case SSI_gpioih:
        {
               
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_PULLED_HIGH);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);

        }
        break;   
        case SSI_gpioil:
        {            
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_INPUT_PULLED_LOW);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);        
        }
        break;          
        case SSI_gpiooh:
        {               
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_OUTPUT_HIGH);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);        
        }
        break; 
        case SSI_gpiool:
        {              
            print_gpio_pins_matching_default(gpio_list, sizeof(gpio_list), GP_OUTPUT_LOW);
            printed = snprintf(pcInsert, iInsertLen, "%s", gpio_list);       
        }
        break;
        case SSI_macadr:                        
        {
            printed = snprintf(pcInsert, iInsertLen, "%02x:%02x:%02x:%02x:%02x:%02x\n", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);              
        }
        break; 
        case SSI_hostn: // hostn
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->host_name);
        }               
        break; 
        case SSI_mquser: // mquser
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->mqtt_user);
        }               
        break; 
        case SSI_mqpass: // mqpass
        {
            //printed = snprintf(pcInsert, iInsertLen, "%s", cfg->mqtt_password);
            printed = snprintf(pcInsert, iInsertLen, "********");
        }               
        break; 
        case SSI_mqaddr: // mqaddr
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->mqtt_broker_address);
        }               
        break;
        case SSI_uptme: // uptme
        {
            printed = get_delta_string_from_delta_seconds(pcInsert, iInsertLen, unix_time - web.boot_time);
            //printed = snprintf(pcInsert, iInsertLen, "%d", unix_time - web.boot_time);
        }               
        break;

        // *** system SSI end ***
        /************************************************************************************************************************* */

        // *** application  SSI start ***

        case SSI_tfnme:  // text file name
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.edit_text_filename);    
        }
        break;       
        case SSI_afnme:  // automation file name
        {
            CLIP(((WEB_SESSION_STATE_T *)connection_state)->automation_file_number, 0, 31);
            printed = snprintf(pcInsert, iInsertLen, "automation%02d", ((WEB_SESSION_STATE_T *)connection_state)->automation_file_number); 
        }
        break;             
        case SSI_eanme:  // automation descriptive name
        {
            CLIP(((WEB_SESSION_STATE_T *)connection_state)->automation_file_number, 0, 31);
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->automation_name[((WEB_SESSION_STATE_T *)connection_state)->automation_file_number]);    
        }
        break;        
        case SSI_usurped:  // usurped
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.last_usurped_timestring);    
        }
        break;
        case SSI_cmplte:  // cmplte
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", web.last_completed_timestring);    
        }
        break;        
        
        case SSI_temp: // temp
        {
            if (!cfg->use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%c%d.%d", web.outside_temperature<0?'-':' ', abs(web.outside_temperature/10), abs(web.outside_temperature%10)); 
            }
            else
            {
                temp = (web.outside_temperature*9)/5 + 320;
                printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':' ', abs(temp)/10, abs(temp%10));
            }              
        }
        break;
        case SSI_wind: // wind
        {         
            if (!cfg->use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.wind_speed/10, web.wind_speed%10); 
            }
            else
            {
                temp = (web.wind_speed*3281 + 500)/1000;
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
            }              
        } 
        break;  
        case SSI_rain: // rain
        {
            if (!cfg->use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.daily_rain/10, web.daily_rain%10); 
            }
            else
            {
                temp = (10*web.daily_rain + 127)/254;
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
            }            
        }  
        break;
        case SSI_lstpck: // lstpck
        {
            if (web.us_last_rx_packet)
            {
                us_now = time_us_32();
                printed = snprintf(pcInsert, iInsertLen, "%lu s", (us_now - web.us_last_rx_packet)/1000000);   
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "never");   
            }
        }  
        break;     
  
        case SSI_rainwk: // rainwk
        {          
            if (!cfg->use_archaic_units)
            {
                printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.weekly_rain/10, web.weekly_rain%10); 
            }
            else
            {
                temp = (10*web.weekly_rain + 127)/254;
                printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
            }             
        }  
        break;   

        // case SSI_sun:    // sun
        // case SSI_mon:    // mon
        // case SSI_tue:    // tue
        // case SSI_wed:    // wed
        // case SSI_thu:    // thu
        // case SSI_fri:    // fri
        // case SSI_sat:    // sat
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->day_schedule_enable[iIndex-SSI_sun]?"ON":"OFF"); 
        // }  
        // break;
        // case SSI_strt1:
        // case SSI_strt2:
        // case SSI_strt3:
        // case SSI_strt4:
        // case SSI_strt5:
        // case SSI_strt6:
        // case SSI_strt7:
        // {
        //     if (cfg->day_schedule_enable[iIndex-SSI_strt1])
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%02d : %02d", cfg->day_start[iIndex-SSI_strt1]/60, cfg->day_start[iIndex-SSI_strt1]%60);
        //     }
        //     else
        //     {
        //         if (true /*cfg->personality == SPRINKLER_USURPER*/)
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, "-- : --"); 
        //         }
        //         else
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, " "); 
        //         }                
        //     }
        // }
        // break;
        // case SSI_ctrt1:
        // case SSI_ctrt2:
        // case SSI_ctrt3:
        // case SSI_ctrt4:
        // case SSI_ctrt5:
        // case SSI_ctrt6:
        // case SSI_ctrt7:
        // {
        //     if (cfg->day_schedule_enable[iIndex-SSI_ctrt1])
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%02d : %02d", cfg->day_start[iIndex-SSI_ctrt1]/60, cfg->day_start[iIndex-SSI_ctrt1]%60);
        //     }
        //     else
        //     {

        //         printed = snprintf(pcInsert, iInsertLen, "&nbsp;");             
        //     }
        // }
        // break;        
        // case SSI_dur1:
        // case SSI_dur2:
        // case SSI_dur3:
        // case SSI_dur4:
        // case SSI_dur5:
        // case SSI_dur6:
        // case SSI_dur7:
        // {
        //     if (cfg->day_schedule_enable[iIndex-SSI_dur1])
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->day_duration[iIndex-SSI_dur1]);    
        //     }
        //     else
        //     {                 
        //         if (true /*cfg->personality == SPRINKLER_USURPER*/)
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, "--"); 
        //         }
        //         else
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, " "); 
        //         } 
        //     }
        // }
        // break;
        // case SSI_ecoip: //ecoip
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->weather_station_ip);
        // }               
        // break;
        // case SSI_wkrn: //wkrn
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d.%d", cfg->rain_week_threshold/10, cfg->rain_week_threshold%10);   
        // }               
        // break;        
        // case SSI_dyrn: //dyrn
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d.%d", cfg->rain_day_threshold/10, cfg->rain_day_threshold%10);
        // }               
        // break; 
        // case SSI_wndt: //wndt
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d.%d", cfg->wind_threshold/10, cfg->wind_threshold%10);
        // }               
        // break;
         // case SSI_rly: //rly
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->relay_normally_open?"checked":"");
        // }               
        // break; 
        // case SSI_gpio: //gpio
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->gpio_number);
        // }               
        // break;   
        // case SSI_lpat: //lpat
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(cfg->led_pattern)); 
        // }               
        // break; 
        // case SSI_lspd: //lspd
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->led_speed);
        // }               
        // break;     
        // case SSI_lpin: //lpin
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->led_pin);
        // }               
        // break;                                      
        // case SSI_lrgbw: //lrgbw
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->led_rgbw);
        // }               
        // break;  
        // case SSI_lnum: //lnum
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->led_number);
        // }               
        // break;  
    
        // case SSI_gvea: //gvea
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->govee_light_ip);
        // } 
        // break;   
        // case SSI_wthr: //wthr
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->weather_station_ip);
        // } 
        // break; 
        // case SSI_day1:    // day1 (sun)
        // case SSI_day2:    // day2 (mon)
        // case SSI_day3:    // day3 (tue)
        // case SSI_day4:    // day4 (wed)
        // case SSI_day5:    // day5 (thu)
        // case SSI_day6:    // day6 (fri)
        // case SSI_day7:    // day7 (sat)
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->day_schedule_enable[iIndex-SSI_day1]?"checked":""); 
        // }  
        // break;
        // case SSI_gvee: //gvee
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_govee_to_indicate_irrigation_status?"checked":""); 
        // }
        // break;
        // case SSI_gvei: //gvei
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d.%d.%d", cfg->govee_irrigation_active_red, cfg->govee_irrigation_active_green, cfg->govee_irrigation_active_blue);
        // }
        // break;
        // case SSI_gveu: //gveu
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d.%d.%d", cfg->govee_irrigation_usurped_red, cfg->govee_irrigation_usurped_green, cfg->govee_irrigation_usurped_blue);
        // }
        // break;
        // case SSI_gves: //gves
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->govee_sustain_duration);
        // }
        // break;                        
        // case SSI_lie: //lie
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->use_led_strip_to_indicate_irrigation_status?"checked":""); 
        // }
        // break;
        // case SSI_lia: //lia
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(cfg->led_pattern_when_irrigation_active));           
        // }
        // break; 
        // case SSI_liu: //liu
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", get_pattern_name(cfg->led_pattern_when_irrigation_terminated));            
        // }
        // break; 
        // case SSI_lis: //lis
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->led_sustain_duration);
        // }
        // break; 

        // case SSI_lstsvn: //lstsvn
        // {
        //     if (!cfg->use_archaic_units)
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.trailing_seven_days_rain/10, web.trailing_seven_days_rain%10); 
        //     }
        //     else
        //     {
        //         temp = (10*web.trailing_seven_days_rain + 127)/254;
        //         printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", temp/10, temp%10);
        //     }                
            
        // }                        
        // break;    
        case SSI_wfail: //wfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.weather_station_transmit_failures);
        }               
        break;  
        case SSI_gfail: //gfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.govee_transmit_failures);
        }               
        break;   
        case SSI_calpge: //calpge
        {
            if (web.access_point_mode)
            {
                printed = snprintf(pcInsert, iInsertLen, "/network.shtml");
            }
            else
            {
                switch(cfg->personality)
                {
                default:
                case NO_PERSONALITY:
                    //printf("redirecting to personality.shtml (%d)\n", cfg->personality);
                    printed = snprintf(pcInsert, iInsertLen, "/personality.shtml");
                    break;
                case SPRINKLER_USURPER:
                    if (cfg->use_monday_as_week_start)
                    {
                        //printf("redirecting to landscape_monday.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/landscape_monday.shtml");
                    }
                    else
                    {
                        //printf("redirecting to landscape.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/landscape.shtml");
                    }
                    break;
                case SPRINKLER_CONTROLLER:
                    if (cfg->use_monday_as_week_start)
                    {
                        //printf("redirecting to landscape_monday.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/zm_landscape.shtml");
                    }
                    else
                    {
                        //printf("redirecting to landscape.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/zs_landscape.shtml");
                    }
                    break;                
                case LED_STRIP_CONTROLLER:
                    printed = snprintf(pcInsert, iInsertLen, "/led_controller.shtml");
                    break;
                case HVAC_THERMOSTAT:
                    if (cfg->use_monday_as_week_start)
                    {
                        //printf("redirecting to landscape_monday.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/tm_thermostat.shtml");
                    }
                    else
                    {
                        //printf("redirecting to landscape.shtml\n");
                        printed = snprintf(pcInsert, iInsertLen, "/ts_thermostat.shtml");
                    }            
                    break;     
                case HOME_CONTROLLER:
                        printed = snprintf(pcInsert, iInsertLen, "/dashboard.shtml");
                        break;                                
                }
            }
        }                                                                                                                                                   
        break; 
        case SSI_porpge: //porpge
        {
            if (web.access_point_mode)
            {
                printed = snprintf(pcInsert, iInsertLen, "/network.shtml");
            }
            else
            {            
                switch(cfg->personality)
                {
                default:
                case NO_PERSONALITY:
                    printed = snprintf(pcInsert, iInsertLen, "/personality.shtml");
                    break;
                case SPRINKLER_USURPER:
                    if (cfg->use_monday_as_week_start)
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/monday.shtml");
                    }
                    else
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/sunday.shtml");
                    }
                    break;
                case SPRINKLER_CONTROLLER:
                    if (cfg->use_monday_as_week_start)
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/monday.shtml");
                    }
                    else
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/sunday.shtml");
                    }
                    break;                
                case LED_STRIP_CONTROLLER:
                    printed = snprintf(pcInsert, iInsertLen, "/led_controller.shtml");
                    break;
                case HVAC_THERMOSTAT:
                    if (cfg->use_monday_as_week_start)
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/t_schedule.cgi?day=1");
                    }
                    else
                    {
                        printed = snprintf(pcInsert, iInsertLen, "/t_schedule.cgi?day=0");
                    }            
                    break;  
                case HOME_CONTROLLER:
                        printed = snprintf(pcInsert, iInsertLen, "/dashboard.shtml");
                        break;                      
                }
            }
        }                                                                                                                                                   
        break;        
        // case SSI_wse: //wse
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->weather_station_enable?"checked":"");
        // }   
        // break;   
                                    
        // case SSI_rsadr1: //rsdar1
        // case SSI_rsadr2: //rsdar2
        // case SSI_rsadr3: //rsdar3
        // case SSI_rsadr4: //rsdar4
        // case SSI_rsadr5: //rsdar5
        // case SSI_rsadr6: //rsdar6               
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->led_strip_remote_ip[iIndex-SSI_rsadr1]); 
        // }                     
        // break; 
        // case SSI_rse: //rse
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->led_strip_remote_enable?"checked":"");
        // } 
        // break;

        case SSI_soilm1: //soilm1
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", web.soil_moisture[0]);
        }                       
        break;    
        // case SSI_soilt1: //soilt1
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->soil_moisture_threshold[0]);
        // }                       
        // break;   
        // case SSI_z1d1d:
        // case SSI_z1d2d:
        // case SSI_z1d3d:
        // case SSI_z1d4d:
        // case SSI_z1d5d:
        // case SSI_z1d6d:
        // case SSI_z1d7d:
        // case SSI_z2d1d:
        // case SSI_z2d2d:
        // case SSI_z2d3d:
        // case SSI_z2d4d:
        // case SSI_z2d5d:
        // case SSI_z2d6d:
        // case SSI_z2d7d:
        // case SSI_z3d1d:
        // case SSI_z3d2d:
        // case SSI_z3d3d:
        // case SSI_z3d4d:
        // case SSI_z3d5d:
        // case SSI_z3d6d:
        // case SSI_z3d7d:
        // case SSI_z4d1d:
        // case SSI_z4d2d:
        // case SSI_z4d3d:
        // case SSI_z4d4d:
        // case SSI_z4d5d:
        // case SSI_z4d6d:
        // case SSI_z4d7d:
        // case SSI_z5d1d:
        // case SSI_z5d2d:
        // case SSI_z5d3d:
        // case SSI_z5d4d:
        // case SSI_z5d5d:
        // case SSI_z5d6d:
        // case SSI_z5d7d:
        // case SSI_z6d1d:
        // case SSI_z6d2d:
        // case SSI_z6d3d:
        // case SSI_z6d4d:
        // case SSI_z6d5d:
        // case SSI_z6d6d:
        // case SSI_z6d7d:
        // case SSI_z7d1d:
        // case SSI_z7d2d:
        // case SSI_z7d3d:
        // case SSI_z7d4d:
        // case SSI_z7d5d:
        // case SSI_z7d6d:
        // case SSI_z7d7d:
        // case SSI_z8d1d:
        // case SSI_z8d2d:
        // case SSI_z8d3d:
        // case SSI_z8d4d:
        // case SSI_z8d5d:
        // case SSI_z8d6d:
        // case SSI_z8d7d:
        // {  
        //     if (cfg->day_schedule_enable[(iIndex-SSI_z1d1d)%7])
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->zone_duration[(iIndex-SSI_z1d1d)/7][(iIndex-SSI_z1d1d)%7]);
        //     }
        //     else
        //     {
        //         if (true /*cfg->personality == SPRINKLER_USURPER*/)
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, "--");  
        //         }
        //         else
        //         {
        //             printed = snprintf(pcInsert, iInsertLen, " ");  
        //         }
                
        //     }                                 
        // }
        // break;
        // case SSI_c1d1d:
        // case SSI_c1d2d:
        // case SSI_c1d3d:
        // case SSI_c1d4d:
        // case SSI_c1d5d:
        // case SSI_c1d6d:
        // case SSI_c1d7d:
        // case SSI_c2d1d:
        // case SSI_c2d2d:
        // case SSI_c2d3d:
        // case SSI_c2d4d:
        // case SSI_c2d5d:
        // case SSI_c2d6d:
        // case SSI_c2d7d:
        // case SSI_c3d1d:
        // case SSI_c3d2d:
        // case SSI_c3d3d:
        // case SSI_c3d4d:
        // case SSI_c3d5d:
        // case SSI_c3d6d:
        // case SSI_c3d7d:
        // case SSI_c4d1d:
        // case SSI_c4d2d:
        // case SSI_c4d3d:
        // case SSI_c4d4d:
        // case SSI_c4d5d:
        // case SSI_c4d6d:
        // case SSI_c4d7d:
        // case SSI_c5d1d:
        // case SSI_c5d2d:
        // case SSI_c5d3d:
        // case SSI_c5d4d:
        // case SSI_c5d5d:
        // case SSI_c5d6d:
        // case SSI_c5d7d:
        // case SSI_c6d1d:
        // case SSI_c6d2d:
        // case SSI_c6d3d:
        // case SSI_c6d4d:
        // case SSI_c6d5d:
        // case SSI_c6d6d:
        // case SSI_c6d7d:
        // case SSI_c7d1d:
        // case SSI_c7d2d:
        // case SSI_c7d3d:
        // case SSI_c7d4d:
        // case SSI_c7d5d:
        // case SSI_c7d6d:
        // case SSI_c7d7d:
        // case SSI_c8d1d:
        // case SSI_c8d2d:
        // case SSI_c8d3d:
        // case SSI_c8d4d:
        // case SSI_c8d5d:
        // case SSI_c8d6d:
        // case SSI_c8d7d:
        // {  
        //     if (cfg->day_schedule_enable[(iIndex-SSI_c1d1d)%7])
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d min", cfg->zone_duration[(iIndex-SSI_c1d1d)/7][(iIndex-SSI_c1d1d)%7]);
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "&nbsp;");    
        //     }                                 
        // }
        // break;        
        // case SSI_clpat: //clpat
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", web.led_current_pattern);
        // } 
        // break;
        // case SSI_cltran: //cltran
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", web.led_current_transition_delay);
        // } 
        // break;  
        // case SSI_clreq: //clreq
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", web.led_last_request_ip);
        // } 
        // break;         
        // case SSI_z1gpio:
        // case SSI_z2gpio:
        // case SSI_z3gpio:
        // case SSI_z4gpio:
        // case SSI_z5gpio:
        // case SSI_z6gpio:
        // case SSI_z7gpio:
        // case SSI_z8gpio:
        // {     
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->zone_gpio[(iIndex-SSI_z1gpio)%8]);             
        // }
        // break;
        // case SSI_z1viz:
        // case SSI_z2viz:
        // case SSI_z3viz:
        // case SSI_z4viz:
        // case SSI_z5viz:
        // case SSI_z6viz:
        // case SSI_z7viz:
        // case SSI_z8viz:
        // {
        //     if ((iIndex-SSI_z1viz)%8 >= cfg->zone_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // break; 
        // case SSI_zmax: //zmax
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->zone_max);
        // }   
        // break;     
        // case SSI_rpage: //rpage
        // {
        //     switch(cfg->personality)
        //     {
        //     default:
        //     case NO_PERSONALITY:
        //     case SPRINKLER_USURPER:   
        //     case LED_STRIP_CONTROLLER:         
        //         printed = snprintf(pcInsert, iInsertLen, "/relay.shtml");
        //         break;
        //     case SPRINKLER_CONTROLLER:
        //         printed = snprintf(pcInsert, iInsertLen, "/z_relay.shtml");
        //         break;
        //     case HVAC_THERMOSTAT:
        //         printed = snprintf(pcInsert, iInsertLen, "/t_gpio.shtml");
        //         break;                
        //     }            
            
        // }   
        // break;                  
        // case SSI_z1bviz:
        // case SSI_z2bviz:
        // case SSI_z3bviz:
        // case SSI_z4bviz:
        // case SSI_z5bviz:
        // case SSI_z6bviz:
        // case SSI_z7bviz:
        // case SSI_z8bviz:
        // {
        //     if ((iIndex-SSI_z1bviz)%8 >= cfg->zone_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         //printed = snprintf(pcInsert, iInsertLen, "style=\"width:14.285714286%%\"");
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"width:13%%\"");                
        //     }             
        // }
        // break; 
        // case SSI_z1iviz:
        // case SSI_z2iviz:
        // case SSI_z3iviz:
        // case SSI_z4iviz:
        // case SSI_z5iviz:
        // case SSI_z6iviz:
        // case SSI_z7iviz:
        // case SSI_z8iviz:
        // {
        //     if ((iIndex-SSI_z1iviz)%8 >= cfg->zone_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"font-size: 28px;\"");
        //     }             
        // }
        // break;   
        // case SSI_z1zviz:
        // case SSI_z2zviz:
        // case SSI_z3zviz:
        // case SSI_z4zviz:
        // case SSI_z5zviz:
        // case SSI_z6zviz:
        // case SSI_z7zviz:
        // case SSI_z8zviz:
        // {
        //     if ((iIndex-SSI_z1zviz)%8 >= cfg->zone_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         //printed = snprintf(pcInsert, iInsertLen, "style=\"width:14.285714286%%\"");
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"width:2%%\"");                
        //     }             
        // } 
        // break;      
        // case SSI_z1dur:
        // {
        //     if (cfg->personality == SPRINKLER_USURPER)
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "Duration");
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "Zone 1 Duration");
        //     }
        // }
        // break;

        // case SSI_irgnow: //irgnow
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", web.irrigation_test_enable?"checked":"");
        // }   
        // break;
        // case SSI_sp1viz:
        // case SSI_sp2viz:
        // case SSI_sp3viz:
        // case SSI_sp4viz:
        // case SSI_sp5viz:
        // case SSI_sp6viz:
        // case SSI_sp7viz:
        // case SSI_sp8viz:
        // case SSI_sp9viz:
        // case SSI_sp10viz:
        // case SSI_sp11viz:
        // case SSI_sp12viz:
        // {
        //     if ((iIndex-SSI_sp1viz)%12 >= cfg->setpoint_number)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // break;         
        // case SSI_sp1nme: //sp1nme
        // case SSI_sp2nme: //sp2nme
        // case SSI_sp3nme: //sp3nme
        // case SSI_sp4nme: //sp4nme
        // case SSI_sp5nme: //sp5nme
        // case SSI_sp6nme: //sp6nme
        // case SSI_sp7nme: //sp7nme
        // case SSI_sp8nme: //sp8nme
        // case SSI_sp9nme: //sp9nme
        // case SSI_sp10nme: //sp10nme
        // case SSI_sp11nme: //sp11nme
        // case SSI_sp12nme: //sp12nme
        // case SSI_sp13nme: //sp13nme
        // case SSI_sp14nme: //sp14nme
        // case SSI_sp15nme: //sp15nme
        // case SSI_sp16nme: //sp16nme
        // {
        //     //printed = snprintf(pcInsert, iInsertLen, "%s", cfg->setpoint_name[iIndex-SSI_sp1nme]); 
        //     printf("error - setpoint_name is not longer supported\n");
        // }                     
        // break; 
        case SSI_sp1tmp: // sp1tmp = automation state
        case SSI_sp2tmp: 
        case SSI_sp3tmp: 
        case SSI_sp4tmp: 
        case SSI_sp5tmp: 
        case SSI_sp6tmp: 
        case SSI_sp7tmp: 
        case SSI_sp8tmp: 
        case SSI_sp9tmp: 
        case SSI_sp10tmp: 
        case SSI_sp11tmp: 
        case SSI_sp12tmp: 
        case SSI_sp13tmp: 
        case SSI_sp14tmp: 
        case SSI_sp15tmp: 
        case SSI_sp16tmp: 
        case SSI_sp17tmp:
        case SSI_sp18tmp:
        case SSI_sp19tmp:
        case SSI_sp20tmp:
        case SSI_sp21tmp:
        case SSI_sp22tmp:
        case SSI_sp23tmp:
        case SSI_sp24tmp:
        case SSI_sp25tmp:
        case SSI_sp26tmp:
        case SSI_sp27tmp:
        case SSI_sp28tmp:
        case SSI_sp29tmp:
        case SSI_sp30tmp:
        case SSI_sp31tmp:
        case SSI_sp32tmp:                
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->automation_state[iIndex-SSI_sp1tmp]?"enabled":"disabled");
        }                     
        break;
        case SSI_ts1st:  // ts1st = automation name
        case SSI_ts2st:
        case SSI_ts3st:
        case SSI_ts4st:
        case SSI_ts5st:
        case SSI_ts6st:
        case SSI_ts7st:
        case SSI_ts8st:
        case SSI_ts9st:
        case SSI_ts10st:
        case SSI_ts11st:
        case SSI_ts12st:
        case SSI_ts13st:
        case SSI_ts14st:
        case SSI_ts15st:
        case SSI_ts16st:
        case SSI_ts17st:
        case SSI_ts18st:
        case SSI_ts19st:
        case SSI_ts20st:
        case SSI_ts21st:
        case SSI_ts22st:
        case SSI_ts23st:
        case SSI_ts24st:
        case SSI_ts25st:
        case SSI_ts26st:
        case SSI_ts27st:
        case SSI_ts28st:
        case SSI_ts29st:
        case SSI_ts30st:
        case SSI_ts31st:
        case SSI_ts32st:                        
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", cfg->automation_name[iIndex-SSI_ts1st]);            
        }
        break; 
        case SSI_sp1mde:  // sp1mde = automation last triggered timestamp
        case SSI_sp2mde:
        case SSI_sp3mde:
        case SSI_sp4mde:
        case SSI_sp5mde:
        case SSI_sp6mde:
        case SSI_sp7mde:
        case SSI_sp8mde:
        case SSI_sp9mde:
        case SSI_sp10mde:
        case SSI_sp11mde:
        case SSI_sp12mde:
        case SSI_sp13mde:
        case SSI_sp14mde:
        case SSI_sp15mde:
        case SSI_sp16mde:
        case SSI_sp17mde:
        case SSI_sp18mde:
        case SSI_sp19mde:
        case SSI_sp20mde:
        case SSI_sp21mde:
        case SSI_sp22mde:
        case SSI_sp23mde:
        case SSI_sp24mde:
        case SSI_sp25mde:
        case SSI_sp26mde:
        case SSI_sp27mde:
        case SSI_sp28mde:
        case SSI_sp29mde:
        case SSI_sp30mde:
        case SSI_sp31mde:
        case SSI_sp32mde:                        
        {
            if (!get_local_timestamp_from_unix_time(cfg->automation_triggered[iIndex-SSI_sp1mde], timestamp, sizeof(timestamp)))
            {
                printed = snprintf(pcInsert, iInsertLen, "%s", timestamp);
            }         
        }
        break;         
        // case SSI_ts1en:
        // case SSI_ts2en:
        // case SSI_ts3en:
        // case SSI_ts4en:
        // case SSI_ts5en:
        // case SSI_ts6en:
        // case SSI_ts7en:
        // case SSI_ts8en:
        // case SSI_ts9en:
        // case SSI_ts10en:
        // case SSI_ts11en:
        // case SSI_ts12en:
        // case SSI_ts13en:
        // case SSI_ts14en:
        // case SSI_ts15en:
        // case SSI_ts16en:
        // {
        //     //printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_period_end_mow[iIndex-SSI_ts1en]); 
        //     //printed = mow_to_string(pcInsert, iInsertLen, cfg->thermostat_period_end_mow[iIndex-SSI_ts1en]);
        //     printf("error - thermostat_period_end_mow is no longer supported\n");
        // }
        // break;                  
        case SSI_ts1vz:
        case SSI_ts2vz:
        case SSI_ts3vz:
        case SSI_ts4vz:
        case SSI_ts5vz:
        case SSI_ts6vz:
        case SSI_ts7vz:
        case SSI_ts8vz:
        case SSI_ts9vz:
        case SSI_ts10vz:
        case SSI_ts11vz:
        case SSI_ts12vz:
        case SSI_ts13vz:
        case SSI_ts14vz:
        case SSI_ts15vz:
        case SSI_ts16vz:
        case SSI_ts17vz:
        case SSI_ts18vz:
        case SSI_ts19vz:
        case SSI_ts20vz:
        case SSI_ts21vz:
        case SSI_ts22vz:
        case SSI_ts23vz:
        case SSI_ts24vz:
        case SSI_ts25vz:
        case SSI_ts26vz:
        case SSI_ts27vz:
        case SSI_ts28vz:
        case SSI_ts29vz:
        case SSI_ts30vz:
        case SSI_ts31vz:
        case SSI_ts32vz:                        
        {
            // if ((get_day_from_mow(cfg->setpoint_start_mow[iIndex-SSI_ts1vz]) != web.thermostat_day) ||
            //     (cfg->setpoint_start_mow[iIndex-SSI_ts1vz] <0))
            // {     
            //     printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
            // }
            // else
            {
                printed = 0;
            }             
        }
        break;   
        // case SSI_ts1in:
        // case SSI_ts2in:
        // case SSI_ts3in:
        // case SSI_ts4in:
        // case SSI_ts5in:
        // case SSI_ts6in:
        // case SSI_ts7in:
        // case SSI_ts8in:
        // case SSI_ts9in:
        // case SSI_ts10in:
        // case SSI_ts11in:
        // case SSI_ts12in:
        // case SSI_ts13in:
        // case SSI_ts14in:
        // case SSI_ts15in:
        // case SSI_ts16in:
        // {
        //     //TODO RANGE CHECKING!!!
        //     //printed = snprintf(pcInsert, iInsertLen, "%s", cfg->setpoint_name[cfg->thermostat_period_setpoint_index[iIndex-SSI_ts1in]]); 
        //     printf("error - thermostat_period_setpoint_index is no longer supported\n");
        // }
        // break;                      
        // case SSI_pwip:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->powerwall_ip);
        // }
        // break;
        // case SSI_pwhost:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", cfg->powerwall_hostname);
        // }
        // break; 
        // case SSI_pwpass:
        // {
        //     //printed = snprintf(pcInsert, iInsertLen, "%s", cfg->powerwall_password);
        //     printed = snprintf(pcInsert, iInsertLen, "********");
        // }
        // break;
        // case SSI_pwgdhd:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_heating_setpoint_decrease/10, cfg->grid_down_heating_setpoint_decrease%10);
        // }
        // break;        
        // case SSI_pwgdci:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_cooling_setpoint_increase/10, cfg->grid_down_cooling_setpoint_increase%10);            
        // }
        // break;
        // case SSI_pwblhd:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_heating_disable_battery_level/10, cfg->grid_down_heating_disable_battery_level%10);            
        // }
        // break;        
        // case SSI_pwblhe:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_heating_enable_battery_level/10, cfg->grid_down_heating_enable_battery_level%10);            
        // }
        // break;
        // case SSI_pwblcd:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_cooling_disable_battery_level/10, cfg->grid_down_cooling_disable_battery_level%10);            
        // }
        // break;   
        // case SSI_pwblce:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%ld.%ld", cfg->grid_down_cooling_enable_battery_level/10, cfg->grid_down_cooling_enable_battery_level%10);            
        // }
        // break; 
        // case SSI_tday:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", day_name(web.thermostat_day));            
        // }
        // break;   
        // case SSI_tpst:
        // {
        //     CLIP(web.thermostat_period_row, 0, NUM_ROWS(cfg->setpoint_start_mow));
        //     //printed = mow_to_string(pcInsert, iInsertLen, cfg->setpoint_start_mow[web.thermostat_period_row]);
        //     printed = mow_to_time_string(pcInsert, iInsertLen, cfg->setpoint_start_mow[web.thermostat_period_row]);            
        // }
        // break; 
        // case SSI_tptmp:
        // {
        //     CLIP(web.thermostat_period_row, 0, NUM_ROWS(cfg->setpoint_temperaturex10));

        //     if (cfg->setpoint_temperaturex10[web.thermostat_period_row] > SETPOINT_TEMP_UNDEFINED)
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->setpoint_temperaturex10[web.thermostat_period_row]/10); 
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        // }
        // break;  
        // case SSI_tphtmp:
        // {
        //     CLIP(web.thermostat_period_row, 0, NUM_ROWS(cfg->setpoint_heating_temperaturex10));

        //     if (cfg->setpoint_heating_temperaturex10[web.thermostat_period_row] > SETPOINT_TEMP_UNDEFINED)
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->setpoint_heating_temperaturex10[web.thermostat_period_row]/10); 
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        // }
        // break;  
        // case SSI_tpctmp:
        // {
        //     CLIP(web.thermostat_period_row, 0, NUM_ROWS(cfg->setpoint_cooling_temperaturex10));

        //     if (cfg->setpoint_cooling_temperaturex10[web.thermostat_period_row] > SETPOINT_TEMP_UNDEFINED)
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->setpoint_cooling_temperaturex10[web.thermostat_period_row]/10); 
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        // }
        // break;          
        // case SSI_tpsm1:
        // case SSI_tpsm2:
        // case SSI_tpsm3:
        // case SSI_tpsm4:
        // case SSI_tpsm5:
        // case SSI_tpsm6:        
        // {
        //     CLIP(web.thermostat_period_row, 0, NUM_ROWS(cfg->setpoint_mode));

        //     if (cfg->setpoint_mode[web.thermostat_period_row] == (iIndex-SSI_tpsm1))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "selected"); 
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, ""); 
        //     }
        // }
        // break;         
        // case SSI_tsaddvz:
        // {
        //     for(i=0; i < NUM_ROWS(cfg->setpoint_start_mow); i++)
        //     {
        //         if (cfg->setpoint_start_mow[i] < 0)
        //         {
        //             new_thermostat_period_found = true;
        //             break;
        //         }
        //     }
        //     if (!new_thermostat_period_found)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }            
        // }
        // break;
//         case SSI_tg0_0:
//         case SSI_tg0_1:
//         case SSI_tg0_2:
//         case SSI_tg0_3:
//         case SSI_tg0_4:
//         case SSI_tg0_5:
//         case SSI_tg0_6:
//         case SSI_tg0_7:
//         case SSI_tg1_0:
//         case SSI_tg1_1:
//         case SSI_tg1_2:
//         case SSI_tg1_3:
//         case SSI_tg1_4:
//         case SSI_tg1_5:
//         case SSI_tg1_6:
//         case SSI_tg1_7:
//         case SSI_tg2_0:
//         case SSI_tg2_1:
//         case SSI_tg2_2:
//         case SSI_tg2_3:
//         case SSI_tg2_4:
//         case SSI_tg2_5:
//         case SSI_tg2_6:
//         case SSI_tg2_7:
//         case SSI_tg3_0:
//         case SSI_tg3_1:
//         case SSI_tg3_2:
//         case SSI_tg3_3:
//         case SSI_tg3_4:
//         case SSI_tg3_5:
//         case SSI_tg3_6:
//         case SSI_tg3_7:
//         case SSI_tg4_0:
//         case SSI_tg4_1:
//         case SSI_tg4_2:
//         case SSI_tg4_3:
//         case SSI_tg4_4:
//         case SSI_tg4_5:
//         case SSI_tg4_6:
//         case SSI_tg4_7:
//         case SSI_tg5_0:
//         case SSI_tg5_1:
//         case SSI_tg5_2:
//         case SSI_tg5_3:
//         case SSI_tg5_4:
//         case SSI_tg5_5:
//         case SSI_tg5_6:
//         case SSI_tg5_7:
//         case SSI_tg6_0:
//         case SSI_tg6_1:
//         case SSI_tg6_2:
//         case SSI_tg6_3:
//         case SSI_tg6_4:
//         case SSI_tg6_5:
//         case SSI_tg6_6:
//         case SSI_tg6_7:
//         case SSI_tg7_0:
//         case SSI_tg7_1:
//         case SSI_tg7_2:
//         case SSI_tg7_3:
//         case SSI_tg7_4:
//         case SSI_tg7_5:
//         case SSI_tg7_6:
//         case SSI_tg7_7:
//         {
//             grid_x = (iIndex-SSI_tg0_0)%8;  //TODO: compute array length
//             grid_y = (iIndex-SSI_tg0_0)/8;

//             CLIP(grid_x, 0, 7);
//             CLIP(grid_y, 0, 7);

//             switch(grid_x)
//             {
//             case 0:
//                 printed = mow_to_time_string(pcInsert, iInsertLen, web.thermostat_grid[grid_x][grid_y]);
//                 break;
//             default:
//                 if (web.thermostat_grid[grid_x][grid_y] > SETPOINT_TEMP_UNDEFINED)
//                 {
//                     printed = snprintf(pcInsert, iInsertLen, "%d&deg;", (web.thermostat_grid[grid_x][grid_y]+5)/10); 
//                 }
//                 else if (web.thermostat_grid[grid_x][grid_y] == SETPOINT_TEMP_INVALID_OFF)
//                 {
//                     printed = snprintf(pcInsert, iInsertLen, "OFF");
//                 }
//                 else if (web.thermostat_grid[grid_x][grid_y] == SETPOINT_TEMP_INVALID_FAN)
//                 {
//                     printed = snprintf(pcInsert, iInsertLen, "FAN");
//                 }                
//                 else
//                 {
//                     printed = snprintf(pcInsert, iInsertLen, "&nbsp;"); 
//                 }
//                 break;
//             }
//         }                     
//         break; 
//         case SSI_tct:  // thermostat current temperature
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%c%d.%d", web.thermostat_temperature<0?'-':' ', abs(web.thermostat_temperature/10), abs(web.thermostat_temperature%10)); 
//             // if (!cfg->use_archaic_units)
//             // {
//             //     printed = snprintf(pcInsert, iInsertLen, "%c%d.%d", web.thermostat_temperature<0?'-':' ', abs(web.thermostat_temperature/10), abs(web.thermostat_temperature%10)); 
//             // }
//             // else
//             // {
//             //     temp = (web.thermostat_temperature*9)/5 + 320;
//             //     printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':' ', abs(temp)/10, abs(temp%10));
//             // }  
//         }       
//         break;  
// #ifdef INCORPORATE_THERMOSTAT              
//         case SSI_tcs:  // thermostat current setpoint
//         {
//             //temp = update_current_setpoints();
//             temp = web.thermostat_set_point;

//             printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", temp<0?'-':' ', abs(temp)/10, abs(temp%10));                           
//         }
//         break;
// #endif
//         case SSI_thgpio:
//         {
//             if (gpio_valid(cfg->heating_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->heating_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break;
//         case SSI_tcgpio:
//         {
//             if (gpio_valid(cfg->cooling_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->cooling_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }            
//         }
//         break; 
//         case SSI_tfgpio:
//         {
//             if (gpio_valid(cfg->fan_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->fan_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }            
//         }
//         break;                  
//         case SSI_tsadr1: //tsdar1
//         case SSI_tsadr2: //tsdar2
//         case SSI_tsadr3: //tsdar3
//         case SSI_tsadr4: //tsdar4
//         case SSI_tsadr5: //tsdar5
//         case SSI_tsadr6: //tsdar6               
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%s", cfg->temperature_sensor_remote_ip[iIndex-SSI_tsadr1]); 
//         }                     
//         break;      
//         case SSI_tempth: //SSI_tempth
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d.%d", cfg->outside_temperature_threshold/10, cfg->outside_temperature_threshold%10);   
//         }               
//         break;          
// #ifdef INCORPORATE_THERMOSTAT  
//         case SSI_tint:  // thermostat indoor temperature
//         {
//             lower = web.thermostat_temperature;
//             upper = web.thermostat_heating_set_point + cfg->thermostat_hysteresis;

//             printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld", lower<0?'-':' ', abs(lower)/10, abs(lower%10));
//         }
//         break;            
//         case SSI_tchs:  // thermostat current heating thresholds
//         {
//             switch (web.thermostat_effective_mode)
//             {
//             case HVAC_AUTO:
//             case HVAC_HEATING_ONLY:
//             case HVAC_HEAT_AND_COOL:
//                 lower = web.thermostat_heating_set_point - cfg->thermostat_hysteresis;
//                 upper = web.thermostat_heating_set_point + cfg->thermostat_hysteresis;

//                 printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld to %c%ld.%ld %s%s", lower<0?'-':' ', abs(lower)/10, abs(lower%10), upper<0?'-':' ', abs(upper)/10, abs(upper%10), "&deg;", cfg->use_archaic_units?"F":"C");
//                 break;
//             default:
//             case HVAC_OFF:
//             case HVAC_COOLING_ONLY:
//             case HVAC_FAN_ONLY:
//                 printed = snprintf(pcInsert, iInsertLen, "undefined");
//                 break;                
//             }
//         }
//         break;
//         case SSI_tccs:  // thermostat current cooling thresholds
//         {
//             switch (web.thermostat_effective_mode)
//             {
//             case HVAC_AUTO:
//             case HVAC_COOLING_ONLY:
//             case HVAC_HEAT_AND_COOL:
//                 lower = web.thermostat_cooling_set_point - cfg->thermostat_hysteresis;
//                 upper = web.thermostat_cooling_set_point + cfg->thermostat_hysteresis;

//                 printed = snprintf(pcInsert, iInsertLen, "%c%ld.%ld to %c%ld.%ld %s%s", lower<0?'-':' ', abs(lower)/10, abs(lower%10), upper<0?'-':' ', abs(upper)/10, abs(upper%10), "&deg;", cfg->use_archaic_units?"F":"C");
//                 break;
//             default:
//             case HVAC_OFF:            
//             case HVAC_HEATING_ONLY:
//             case HVAC_FAN_ONLY:
//                 printed = snprintf(pcInsert, iInsertLen, "undefined");
//                 break;                
//             }            
//         }
//         break;   
//         case SSI_grids:  // grid status
//         {
//             switch(web.powerwall_grid_status)
//             {
//                 case GRID_DOWN:
//                 printed = snprintf(pcInsert, iInsertLen, "DOWN");
//                 break;

//                 case GRID_UP:
//                 printed = snprintf(pcInsert, iInsertLen, "UP");
//                 break;

//                 default: 
//                 case GRID_UNKNOWN:
//                 printed = snprintf(pcInsert, iInsertLen, "UNKNOWN");
//                 break;                                 
//             }                                     
//         }
//         break;  
//         case SSI_batp:  // battery percentage
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d.%d", web.powerwall_battery_percentage/10, web.powerwall_battery_percentage%10);                           
//         }
//         break;         
//         case SSI_tacgpio:  // temperature sensor clock
//         {
//             if (gpio_valid(cfg->thermostat_temperature_sensor_clock_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_temperature_sensor_clock_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break;  
//         case SSI_tadgpio: // temperature sensor data
//         {
//             if (gpio_valid(cfg->thermostat_temperature_sensor_data_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_temperature_sensor_data_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_tlcgpio: // display clock
//         {
//             if (gpio_valid(cfg->thermostat_seven_segment_display_clock_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_seven_segment_display_clock_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_tldgpio: // display data
//         {
//             if (gpio_valid(cfg->thermostat_seven_segment_display_data_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_seven_segment_display_data_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_tbugpio:  // button up
//         {
//             if (gpio_valid(cfg->thermostat_increase_button_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_increase_button_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_tbdgpio:   // button down
//         {
//             if (gpio_valid(cfg->thermostat_decrease_button_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_decrease_button_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_tbmgpio:   // button mode
//         {
//             if (gpio_valid(cfg->thermostat_mode_button_gpio))
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_mode_button_gpio);
//             }
//             else
//             {
//                 printed = snprintf(pcInsert, iInsertLen, "none");
//             }
//         }
//         break; 
//         case SSI_htclm:  // heating to cooling lockuout
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->heating_to_cooling_lockout_mins);                           
//         }
//         break;
//         case SSI_mhonm:  // minimum heating on time
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->minimum_heating_on_mins);                           
//         }
//         break;
//         case SSI_mconm:  // minimum cooling on time
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->minimum_cooling_on_mins);                           
//         }
//         break;
//         case SSI_mhoffm:  // minimum heating off time
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->minimum_heating_off_mins);                           
//         }
//         break;
//         case SSI_mcoffm:  // minimum cooling off time
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->minimum_cooling_off_mins);                           
//         }
//         break;              
//         case SSI_hvachys:  // hysteresis
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d.%d", cfg->thermostat_hysteresis/10, cfg->thermostat_hysteresis%10);                    
//         }
//         break;
//         case SSI_this1:
//         case SSI_this2:
//         case SSI_this3:
//         case SSI_this4:
//         case SSI_this5:
//         case SSI_this6:
//         case SSI_this7:
//         case SSI_this8:
//         case SSI_this9:
//         case SSI_this10:
//         case SSI_this11:
//         case SSI_this12:
//         case SSI_this13:
//         case SSI_this14:
//         case SSI_this15:
//         case SSI_this16:
//         case SSI_this17:
//         case SSI_this18:
//         case SSI_this19:
//         case SSI_this20: 
//         case SSI_this21:
//         case SSI_this22:
//         case SSI_this23:
//         case SSI_this24:
//         case SSI_this25:
//         case SSI_this26:
//         case SSI_this27:
//         case SSI_this28:
//         case SSI_this29:
//         case SSI_this30:
//         case SSI_this31:
//         case SSI_this32:
//         case SSI_this33:
//         case SSI_this34:
//         case SSI_this35:
//         case SSI_this36:
//         case SSI_this37:
//         case SSI_this38:
//         case SSI_this39:
//         case SSI_this40:
//         case SSI_this41:
//         case SSI_this42:
//         case SSI_this43:
//         case SSI_this44:
//         case SSI_this45:
//         case SSI_this46:
//         case SSI_this47:
//         case SSI_this48:
//         case SSI_this49:
//         case SSI_this50:
//         case SSI_this51:
//         case SSI_this52:
//         case SSI_this53:
//         case SSI_this54:
//         case SSI_this55:
//         case SSI_this56:
//         case SSI_this57:
//         case SSI_this58:
//         case SSI_this59:
//         case SSI_this60:
//         case SSI_this61:
//         case SSI_this62:
//         case SSI_this63:
//         case SSI_this64:
//         case SSI_this65:
//         case SSI_this66:
//         case SSI_this67:
//         case SSI_this68:
//         case SSI_this69:
//         case SSI_this70:
//         case SSI_this71:
//         case SSI_this72:
//         case SSI_this73:
//         case SSI_this74:
//         case SSI_this75:
//         case SSI_this76:
//         case SSI_this77:
//         case SSI_this78:
//         case SSI_this79:
//         case SSI_this80:
//         case SSI_this81:
//         case SSI_this82:
//         case SSI_this83:
//         case SSI_this84:
//         case SSI_this85:
//         case SSI_this86:
//         case SSI_this87:
//         case SSI_this88:
//         case SSI_this89:
//         case SSI_this90:
//         case SSI_this91:
//         case SSI_this92:
//         case SSI_this93:
//         case SSI_this94:
//         case SSI_this95:
//         case SSI_this96:
//         case SSI_this97:
//         case SSI_this98:
//         case SSI_this99:
//         case SSI_this100:                                       
//         // temperature history
//         {
//             printed = print_temperature_history(pcInsert, iInsertLen, (iIndex-SSI_this1)*4, 4);                                   
//         }         
//         break;   
//         case SSI_disbri: // display brightness
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_display_brightness);
//         }
//         break;
//         case SSI_ttma:  // thermostat temperature moving average
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", web.thermostat_temperature_moving_average);                           
//         }
//         break;
//         case SSI_ttgrd:  // thermostat temperature gradient
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", web.thermostat_temperature_gradient);                           
//         }
//         break;
//         case SSI_ttpred:  // thermostat temperature prediction (samples until target reached)
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", web.thermostat_temperature_prediction);                           
//         }
//         break;    
//         case SSI_disdig:  // display number of digits
//         {
//             printed = snprintf(pcInsert, iInsertLen, "%d", cfg->thermostat_display_num_digits);                           
//         }
//         break;                   
// #endif                   
        // case SSI_rs1viz:
        // case SSI_rs2viz:
        // case SSI_rs3viz:
        // case SSI_rs4viz:
        // case SSI_rs5viz:
        // case SSI_rs6viz:
        // case SSI_rs7viz:
        // case SSI_rs8viz:
        // {
        //     if ((iIndex-SSI_rs1viz)%8 >= cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // break; 
        // case SSI_rsrly1:
        // case SSI_rsrly2:
        // case SSI_rsrly3:
        // case SSI_rsrly4:
        // case SSI_rsrly5:
        // case SSI_rsrly6:
        // case SSI_rsrly7:
        // case SSI_rsrly8:
        // {  
        //     if ((iIndex-SSI_rsrly1)%8 >= cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        //     else
        //     {
        //         // original shows config values                
        //         // printed =  snprintf(pcInsert, iInsertLen, "<td><input type=\"radio\" id=\"rs%d\" name=\"rsrly%d\" value=\"ON\" %s></td>", (iIndex-SSI_rsrly1)%8+1, (iIndex-SSI_rsrly1)%8+1, cfg->rmtsw_relay_default_state[(iIndex-SSI_rsrly1)%8]?"checked":"");
        //         // printed += snprintf(pcInsert+printed, iInsertLen-printed, "<td><label for=\"inact\">ON</label></td>");

        //         // show live values -- conflating live state with configuration
        //         printed =  snprintf(pcInsert, iInsertLen, "<td><input type=\"radio\" id=\"rs%d\" name=\"rsrly%d\" value=\"ON\" %s></td>", (iIndex-SSI_rsrly1)%8+1, (iIndex-SSI_rsrly1)%8+1, web.rmtsw_relay_desired_state[(iIndex-SSI_rsrly1)%8]?"checked":"");
        //         printed += snprintf(pcInsert+printed, iInsertLen-printed, "<td><label for=\"inact\">ON</label></td>");                
        //     }                       
        // }
        // break; 
        // case SSI_rsoff1:
        // case SSI_rsoff2:
        // case SSI_rsoff3:
        // case SSI_rsoff4:
        // case SSI_rsoff5:
        // case SSI_rsoff6:
        // case SSI_rsoff7:
        // case SSI_rsoff8:
        // {  
        //     if ((iIndex-SSI_rsoff1)%8 >= cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        //     else
        //     {                
        //         // printed += snprintf(pcInsert, iInsertLen, "<td><input type=\"radio\" id=\"rs%d\" name=\"rsrly%d\" value=\"OFF\" %s></td>", (iIndex-SSI_rsoff1)%8+1, (iIndex-SSI_rsoff1)%8+1, cfg->rmtsw_relay_default_state[(iIndex-SSI_rsoff1)%8]?"":"checked");                
        //         // printed += snprintf(pcInsert+printed, iInsertLen-printed, "<td><label for=\"inact\">OFF</label></td>");

        //         // show live values -- conflating live state with configuration
        //         printed += snprintf(pcInsert, iInsertLen, "<td><input type=\"radio\" id=\"rs%d\" name=\"rsrly%d\" value=\"OFF\" %s></td>", (iIndex-SSI_rsoff1)%8+1, (iIndex-SSI_rsoff1)%8+1, web.rmtsw_relay_desired_state[(iIndex-SSI_rsoff1)%8]?"":"checked");                
        //         printed += snprintf(pcInsert+printed, iInsertLen-printed, "<td><label for=\"inact\">OFF</label></td>");                
        //     }                       
        // }
        // break;         
        // case SSI_rs1nme:
        // case SSI_rs2nme:
        // case SSI_rs3nme:
        // case SSI_rs4nme:
        // case SSI_rs5nme:
        // case SSI_rs6nme:
        // case SSI_rs7nme:
        // case SSI_rs8nme:
        // {  
        //     if ((iIndex-SSI_rs1nme)%8 >= cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        //     else
        //     {                
        //         //printed = snprintf(pcInsert, iInsertLen, "<td><label for=\"rlynme\">&emsp;%s</label></td>", cfg->rmtsw_relay_name[(iIndex-SSI_rsrly1)%8]);
        //         printed = snprintf(pcInsert, iInsertLen, "%s", cfg->rmtsw_relay_name[(iIndex-SSI_rs1nme)%8]);                
        //     }                       
        // }
        // break;  
        // case SSI_rs1gpio:
        // case SSI_rs2gpio:
        // case SSI_rs3gpio:
        // case SSI_rs4gpio:
        // case SSI_rs5gpio:
        // case SSI_rs6gpio:
        // case SSI_rs7gpio:
        // case SSI_rs8gpio:
        // {     
             
        //     if (gpio_valid(cfg->rmtsw_relay_gpio[(iIndex-SSI_rs1gpio)%8]))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "%d", cfg->rmtsw_relay_gpio[(iIndex-SSI_rs1gpio)%8]);
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "none");
        //     }            
        // }
        // break;  
        // case SSI_rsmax: //rsmax
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%d", cfg->rmtsw_relay_max);
        // }   
        // break;                              
        // case SSI_rs1actr1: 
        // case SSI_rs2actr1: 
        // case SSI_rs3actr1: 
        // case SSI_rs4actr1: 
        // case SSI_rs5actr1: 
        // case SSI_rs6actr1: 
        // case SSI_rs7actr1: 
        // case SSI_rs8actr1: 
        // case SSI_rs9actr1: 
        // case SSI_rs10actr1: 
        // case SSI_rs11actr1: 
        // case SSI_rs12actr1: 
        // case SSI_rs13actr1: 
        // case SSI_rs14actr1: 
        // case SSI_rs15actr1: 
        // case SSI_rs16actr1: 
        // case SSI_rs17actr1:
        // case SSI_rs18actr1:
        // case SSI_rs19actr1:
        // case SSI_rs20actr1:
        // case SSI_rs21actr1:
        // case SSI_rs22actr1:
        // case SSI_rs23actr1:
        // case SSI_rs24actr1:
        // case SSI_rs25actr1:
        // case SSI_rs26actr1:
        // case SSI_rs27actr1:
        // case SSI_rs28actr1:
        // case SSI_rs29actr1:
        // case SSI_rs30actr1:
        // case SSI_rs31actr1:
        // case SSI_rs32actr1:
        // case SSI_rs1actr2: 
        // case SSI_rs2actr2: 
        // case SSI_rs3actr2: 
        // case SSI_rs4actr2: 
        // case SSI_rs5actr2: 
        // case SSI_rs6actr2: 
        // case SSI_rs7actr2: 
        // case SSI_rs8actr2: 
        // case SSI_rs9actr2: 
        // case SSI_rs10actr2: 
        // case SSI_rs11actr2: 
        // case SSI_rs12actr2: 
        // case SSI_rs13actr2: 
        // case SSI_rs14actr2: 
        // case SSI_rs15actr2: 
        // case SSI_rs16actr2: 
        // case SSI_rs17actr2:
        // case SSI_rs18actr2:
        // case SSI_rs19actr2:
        // case SSI_rs20actr2:
        // case SSI_rs21actr2:
        // case SSI_rs22actr2:
        // case SSI_rs23actr2:
        // case SSI_rs24actr2:
        // case SSI_rs25actr2:
        // case SSI_rs26actr2:
        // case SSI_rs27actr2:
        // case SSI_rs28actr2:
        // case SSI_rs29actr2:
        // case SSI_rs30actr2:
        // case SSI_rs31actr2:
        // case SSI_rs32actr2:
        // case SSI_rs1actr3: 
        // case SSI_rs2actr3: 
        // case SSI_rs3actr3: 
        // case SSI_rs4actr3: 
        // case SSI_rs5actr3: 
        // case SSI_rs6actr3: 
        // case SSI_rs7actr3: 
        // case SSI_rs8actr3: 
        // case SSI_rs9actr3: 
        // case SSI_rs10actr3: 
        // case SSI_rs11actr3: 
        // case SSI_rs12actr3: 
        // case SSI_rs13actr3: 
        // case SSI_rs14actr3: 
        // case SSI_rs15actr3: 
        // case SSI_rs16actr3: 
        // case SSI_rs17actr3:
        // case SSI_rs18actr3:
        // case SSI_rs19actr3:
        // case SSI_rs20actr3:
        // case SSI_rs21actr3:
        // case SSI_rs22actr3:
        // case SSI_rs23actr3:
        // case SSI_rs24actr3:
        // case SSI_rs25actr3:
        // case SSI_rs26actr3:
        // case SSI_rs27actr3:
        // case SSI_rs28actr3:
        // case SSI_rs29actr3:
        // case SSI_rs30actr3:
        // case SSI_rs31actr3:
        // case SSI_rs32actr3:
        // case SSI_rs1actr4: 
        // case SSI_rs2actr4: 
        // case SSI_rs3actr4: 
        // case SSI_rs4actr4: 
        // case SSI_rs5actr4: 
        // case SSI_rs6actr4: 
        // case SSI_rs7actr4: 
        // case SSI_rs8actr4: 
        // case SSI_rs9actr4: 
        // case SSI_rs10actr4: 
        // case SSI_rs11actr4: 
        // case SSI_rs12actr4: 
        // case SSI_rs13actr4: 
        // case SSI_rs14actr4: 
        // case SSI_rs15actr4: 
        // case SSI_rs16actr4: 
        // case SSI_rs17actr4:
        // case SSI_rs18actr4:
        // case SSI_rs19actr4:
        // case SSI_rs20actr4:
        // case SSI_rs21actr4:
        // case SSI_rs22actr4:
        // case SSI_rs23actr4:
        // case SSI_rs24actr4:
        // case SSI_rs25actr4:
        // case SSI_rs26actr4:
        // case SSI_rs27actr4:
        // case SSI_rs28actr4:
        // case SSI_rs29actr4:
        // case SSI_rs30actr4:
        // case SSI_rs31actr4:
        // case SSI_rs32actr4:
        // case SSI_rs1actr5: 
        // case SSI_rs2actr5: 
        // case SSI_rs3actr5: 
        // case SSI_rs4actr5: 
        // case SSI_rs5actr5: 
        // case SSI_rs6actr5: 
        // case SSI_rs7actr5: 
        // case SSI_rs8actr5: 
        // case SSI_rs9actr5: 
        // case SSI_rs10actr5: 
        // case SSI_rs11actr5: 
        // case SSI_rs12actr5: 
        // case SSI_rs13actr5: 
        // case SSI_rs14actr5: 
        // case SSI_rs15actr5: 
        // case SSI_rs16actr5: 
        // case SSI_rs17actr5:
        // case SSI_rs18actr5:
        // case SSI_rs19actr5:
        // case SSI_rs20actr5:
        // case SSI_rs21actr5:
        // case SSI_rs22actr5:
        // case SSI_rs23actr5:
        // case SSI_rs24actr5:
        // case SSI_rs25actr5:
        // case SSI_rs26actr5:
        // case SSI_rs27actr5:
        // case SSI_rs28actr5:
        // case SSI_rs29actr5:
        // case SSI_rs30actr5:
        // case SSI_rs31actr5:
        // case SSI_rs32actr5:
        // case SSI_rs1actr6: 
        // case SSI_rs2actr6: 
        // case SSI_rs3actr6: 
        // case SSI_rs4actr6: 
        // case SSI_rs5actr6: 
        // case SSI_rs6actr6: 
        // case SSI_rs7actr6: 
        // case SSI_rs8actr6: 
        // case SSI_rs9actr6: 
        // case SSI_rs10actr6: 
        // case SSI_rs11actr6: 
        // case SSI_rs12actr6: 
        // case SSI_rs13actr6: 
        // case SSI_rs14actr6: 
        // case SSI_rs15actr6: 
        // case SSI_rs16actr6: 
        // case SSI_rs17actr6:
        // case SSI_rs18actr6:
        // case SSI_rs19actr6:
        // case SSI_rs20actr6:
        // case SSI_rs21actr6:
        // case SSI_rs22actr6:
        // case SSI_rs23actr6:
        // case SSI_rs24actr6:
        // case SSI_rs25actr6:
        // case SSI_rs26actr6:
        // case SSI_rs27actr6:
        // case SSI_rs28actr6:
        // case SSI_rs29actr6:
        // case SSI_rs30actr6:
        // case SSI_rs31actr6:
        // case SSI_rs32actr6:
        // case SSI_rs1actr7: 
        // case SSI_rs2actr7: 
        // case SSI_rs3actr7: 
        // case SSI_rs4actr7: 
        // case SSI_rs5actr7: 
        // case SSI_rs6actr7: 
        // case SSI_rs7actr7: 
        // case SSI_rs8actr7: 
        // case SSI_rs9actr7: 
        // case SSI_rs10actr7: 
        // case SSI_rs11actr7: 
        // case SSI_rs12actr7: 
        // case SSI_rs13actr7: 
        // case SSI_rs14actr7: 
        // case SSI_rs15actr7: 
        // case SSI_rs16actr7: 
        // case SSI_rs17actr7:
        // case SSI_rs18actr7:
        // case SSI_rs19actr7:
        // case SSI_rs20actr7:
        // case SSI_rs21actr7:
        // case SSI_rs22actr7:
        // case SSI_rs23actr7:
        // case SSI_rs24actr7:
        // case SSI_rs25actr7:
        // case SSI_rs26actr7:
        // case SSI_rs27actr7:
        // case SSI_rs28actr7:
        // case SSI_rs29actr7:
        // case SSI_rs30actr7:
        // case SSI_rs31actr7:
        // case SSI_rs32actr7:
        // case SSI_rs1actr8: 
        // case SSI_rs2actr8: 
        // case SSI_rs3actr8: 
        // case SSI_rs4actr8: 
        // case SSI_rs5actr8: 
        // case SSI_rs6actr8: 
        // case SSI_rs7actr8: 
        // case SSI_rs8actr8: 
        // case SSI_rs9actr8: 
        // case SSI_rs10actr8: 
        // case SSI_rs11actr8: 
        // case SSI_rs12actr8: 
        // case SSI_rs13actr8: 
        // case SSI_rs14actr8: 
        // case SSI_rs15actr8: 
        // case SSI_rs16actr8: 
        // case SSI_rs17actr8:
        // case SSI_rs18actr8:
        // case SSI_rs19actr8:
        // case SSI_rs20actr8:
        // case SSI_rs21actr8:
        // case SSI_rs22actr8:
        // case SSI_rs23actr8:
        // case SSI_rs24actr8:
        // case SSI_rs25actr8:
        // case SSI_rs26actr8:
        // case SSI_rs27actr8:
        // case SSI_rs28actr8:
        // case SSI_rs29actr8:
        // case SSI_rs30actr8:
        // case SSI_rs31actr8:
        // case SSI_rs32actr8:                                                                        
        // {
        //     schedule_slot =  (iIndex-SSI_rs1actr1) / 8;
        //     schedule_relay = (iIndex-SSI_rs1actr1) % 8;

        //     //printf("row = %d bit = %d", schedule_slot, schedule_relay);

        //     if (cfg->rmtsw_relay_schedule_action_on[schedule_slot] & (1<<schedule_relay))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "ON"); 
        //         //printf("ON\n");  
        //     }
        //     else if (cfg->rmtsw_relay_schedule_action_off[schedule_slot] & (1<<schedule_relay))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "OFF");
        //         //printf("OFF\n"); 
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "X");
        //        // printf("X\n");
        //     }

        //     // {
        //     //     static int done =0;
        //     //     int l;
        //     //     int m;
        //     //     if (!done)
        //     //     {
        //     //         for(l=1; l<=32; l++)
        //     //         {
        //     //             for(m=1; m<=8; m++)
        //     //             {
        //     //                 printf("x(rs%dactr%d)    \\\n", l, m);
        //     //             }
        //     //         }
        //     //         done = 1;
        //     //     }
        //     // }
        // }                     
        // break;
        // case SSI_rsst:
        // {
        //     CLIP(web.rmtsw_relay_period_row, 0, NUM_ROWS(cfg->rmtsw_relay_schedule_start_mow));
        //     //printed = mow_to_string(pcInsert, iInsertLen, cfg->setpoint_start_mow[web.rmtsw_relay_period_row]);
        //     printed = mow_to_time_string(pcInsert, iInsertLen, cfg->rmtsw_relay_schedule_start_mow[web.rmtsw_relay_period_row]);            
        // }
        // break;   
        // case SSI_rs1st:
        // case SSI_rs2st:
        // case SSI_rs3st:
        // case SSI_rs4st:
        // case SSI_rs5st:
        // case SSI_rs6st:
        // case SSI_rs7st:
        // case SSI_rs8st:
        // case SSI_rs9st:
        // case SSI_rs10st:
        // case SSI_rs11st:
        // case SSI_rs12st:
        // case SSI_rs13st:
        // case SSI_rs14st:
        // case SSI_rs15st:
        // case SSI_rs16st:
        // case SSI_rs17st:
        // case SSI_rs18st:
        // case SSI_rs19st:
        // case SSI_rs20st:
        // case SSI_rs21st:
        // case SSI_rs22st:
        // case SSI_rs23st:
        // case SSI_rs24st:
        // case SSI_rs25st:
        // case SSI_rs26st:
        // case SSI_rs27st:
        // case SSI_rs28st:
        // case SSI_rs29st:
        // case SSI_rs30st:
        // case SSI_rs31st:
        // case SSI_rs32st:                        
        // {
        //     printed = mow_to_time_string(pcInsert, iInsertLen, cfg->rmtsw_relay_schedule_start_mow[iIndex-SSI_rs1st]);            
        // }
        // break;         
        // case SSI_rs1vz:
        // case SSI_rs2vz:
        // case SSI_rs3vz:
        // case SSI_rs4vz:
        // case SSI_rs5vz:
        // case SSI_rs6vz:
        // case SSI_rs7vz:
        // case SSI_rs8vz:
        // case SSI_rs9vz:
        // case SSI_rs10vz:
        // case SSI_rs11vz:
        // case SSI_rs12vz:
        // case SSI_rs13vz:
        // case SSI_rs14vz:
        // case SSI_rs15vz:
        // case SSI_rs16vz:
        // case SSI_rs17vz:
        // case SSI_rs18vz:
        // case SSI_rs19vz:
        // case SSI_rs20vz:
        // case SSI_rs21vz:
        // case SSI_rs22vz:
        // case SSI_rs23vz:
        // case SSI_rs24vz:
        // case SSI_rs25vz:
        // case SSI_rs26vz:
        // case SSI_rs27vz:
        // case SSI_rs28vz:
        // case SSI_rs29vz:
        // case SSI_rs30vz:
        // case SSI_rs31vz:
        // case SSI_rs32vz:                        
        // {
        //     if ((get_day_from_mow(cfg->rmtsw_relay_schedule_start_mow[iIndex-SSI_rs1vz]) != web.rmtsw_relay_day) ||
        //         (cfg->rmtsw_relay_schedule_start_mow[iIndex-SSI_rs1vz] <0))
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // break; 
        // case SSI_rsaddvz:
        // {
        //     for(i=0; i < NUM_ROWS(cfg->rmtsw_relay_schedule_start_mow); i++)
        //     {
        //         if (cfg->rmtsw_relay_schedule_start_mow[i] < 0)
        //         {
        //             new_thermostat_period_found = true;
        //             break;
        //         }
        //     }
        //     if (!new_thermostat_period_found)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"display:none;\"");
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }            
        // }
        // break;        
        // case SSI_rsact10:  // penultimate digit is relay number and ultimate is action number
        // case SSI_rsact11:
        // case SSI_rsact12:                
        // case SSI_rsact20:
        // case SSI_rsact21:
        // case SSI_rsact22:       
        // case SSI_rsact30:
        // case SSI_rsact31:
        // case SSI_rsact32:
        // case SSI_rsact40:
        // case SSI_rsact41:
        // case SSI_rsact42:
        // case SSI_rsact50:
        // case SSI_rsact51:
        // case SSI_rsact52:
        // case SSI_rsact60:
        // case SSI_rsact61:
        // case SSI_rsact62:
        // case SSI_rsact70:
        // case SSI_rsact71:
        // case SSI_rsact72:
        // case SSI_rsact80:
        // case SSI_rsact81:
        // case SSI_rsact82:                                                      
        // {
        //     CLIP(web.rmtsw_relay_period_row, 0, NUM_ROWS(cfg->rmtsw_relay_schedule_start_mow));

        //     schedule_relay = (iIndex-SSI_rsact10) / 3;   // relay number
        //     schedule_slot  = (iIndex-SSI_rsact10) % 3;   // action number

        //     //printf("tag = %d row = %d relay = %d action = %d  OFF = %08b  ON = %08bn ", iIndex, web.rmtsw_relay_period_row, schedule_relay, schedule_slot, cfg->rmtsw_relay_schedule_action_off[web.rmtsw_relay_period_row], cfg->rmtsw_relay_schedule_action_on[web.rmtsw_relay_period_row]);

        //     if ((cfg->rmtsw_relay_schedule_action_on[web.rmtsw_relay_period_row] & (1<<schedule_relay)) && (schedule_slot == RMSW_ACTION_ON))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "selected");
        //         //printf("relay %d ON\n", schedule_relay);   
        //     }
        //     else if ((cfg->rmtsw_relay_schedule_action_off[web.rmtsw_relay_period_row] & (1<<schedule_relay)) && (schedule_slot == RMSW_ACTION_OFF))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "selected");
        //         //printf("relay %d OFF\n", schedule_relay);   
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "");  
        //         //printf("relay %d blank assuming browser will pick first value X\n", schedule_relay);
        //     }
        //     // TODO: may need to select do nothing option if first option not automatically used by browser
        // }
        // break;         
        // case SSI_rsday:
        // {
        //     printed = snprintf(pcInsert, iInsertLen, "%s", day_name(web.rmtsw_relay_day));            
        // }
        // break; 
        // case SSI_rscvz1:
        // case SSI_rscvz2:        
        // case SSI_rscvz3:        
        // case SSI_rscvz4:        
        // case SSI_rscvz5:        
        // case SSI_rscvz6:        
        // case SSI_rscvz7:        
        // case SSI_rscvz8:        
        // case SSI_rscvz9:        
        // case SSI_rscvz10: 
        // case SSI_rscvz11:                       
        // {
        //     if ((iIndex-SSI_rscvz1) < (cfg->rmtsw_relay_max + 2))
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "");
        //     }
        //     else
        //     {
        //         printed = snprintf(pcInsert, iInsertLen, "style=\"visibility:collapse;\""); 
        //     }                       
        // }
        // break;
        case SSI_home:
        {
            if (web.access_point_mode)
            {
                printed = snprintf(pcInsert, iInsertLen, "/network.shtml");
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "/rs_relay_default.shtml");
            }                       
        }
        break;                  

        // case SSI_rs1nc:
        // case SSI_rs2nc:
        // case SSI_rs3nc:
        // case SSI_rs4nc:
        // case SSI_rs5nc:
        // case SSI_rs6nc:
        // case SSI_rs7nc:
        // case SSI_rs8nc:
        // {
        //     if ((iIndex-SSI_rs1nc)%8 < cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "%s", cfg->rmtsw_relay_normally_closed[iIndex-SSI_rs1nc]?"checked":""); 
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // // break; 
        // case SSI_rsbulb1:
        // case SSI_rsbulb2:
        // case SSI_rsbulb3:
        // case SSI_rsbulb4:
        // case SSI_rsbulb5:
        // case SSI_rsbulb6:
        // case SSI_rsbulb7:
        // case SSI_rsbulb8:
        // {
        //     if ((iIndex-SSI_rsbulb1) < cfg->rmtsw_relay_max)
        //     {     
        //         printed = snprintf(pcInsert, iInsertLen, "%d%s", (iIndex-SSI_rsbulb1+1), web.rmtsw_relay_desired_state[iIndex-SSI_rsbulb1]?"&#x1F7E2;":"&#x1F534;"); 
        //     }
        //     else
        //     {
        //         printed = 0;
        //     }             
        // }
        // break;                                  


         // *** application  SSI end ***
        /************************************************************************************************/

        default:
        {
            printed = snprintf(pcInsert, iInsertLen, "Unhandled SSI tag");    
        }
        break;                 
    }


    return ((u16_t)printed);
}

void ssi_init(void)
{
    // configure SSI handler
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
}


