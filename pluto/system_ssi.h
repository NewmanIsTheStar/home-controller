#ifndef SYSTEM_SSI_H
#define SYSTEM_SSI_H

#include "application_ssi.h"

/*List of SSI tags used in html files
  Notes:-
    1. This list is used to create a matching enum and string constant for each SSI tag
    2. Only append to end of list.  Do not insert, delete or reorder the existing items!
    3. Related items are assumed to be in sequence in the code
*/
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
    x(uptme) \
    x(glat) \
    x(glng)

    
//enum used to index array of pointers to SSI string constants  e.g. index 0 is SSI_usurped
enum ssi_index
{
#define x(name) SSI_ ## name,
SYSTEM_SSI_TAGS
APPLICATION_SSI_TAGS
#undef x
};


#endif
