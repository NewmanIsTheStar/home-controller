/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "lwip/sockets.h"
#include "lwip/apps/httpd.h"
#include "calendar.h"
#include "utility.h"
#include "application_config.h"
#include "pluto.h"
#include "system_web.h"
#include "hc_task.h"
#include "system_ssi.h"

#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif


extern uint32_t unix_time;
extern SYS_WEB_VARIABLES_T sys_web;
extern APP_CONFIG_T config;


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
    size_t printed = 0;
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.watchdog_timestring); 
        }  
        break; 
        case SSI_status: // status
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.status_message); 
        }  
        break;
        case SSI_tz:
        {
            if (sys->timezone_offset > 0)
            {
                // leading + sign added
                if (sys->timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "+%d", sys->timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "+%d:%d", sys->timezone_offset/60, abs(sys->timezone_offset%60));    
                }                
            }
            else
            {
                // leading - sign automatically added
                if (sys->timezone_offset%60 == 0)
                {
                    // normal time zone with whole number of hours
                    printed = snprintf(pcInsert, iInsertLen, "%d", sys->timezone_offset/60);                 
                }
                else
                {   // unusual time zone with hours and minutes
                    printed = snprintf(pcInsert, iInsertLen, "%d:%d", sys->timezone_offset/60, abs(sys->timezone_offset%60));    
                }
            }
        }
        break;
        case SSI_dss:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->daylightsaving_start);
        }
        break;
        case SSI_dse:
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->daylightsaving_end);
        }
        break;   
        case SSI_ts1:    // ts1
        case SSI_ts2:    // ts2
        case SSI_ts3:    // ts3
        case SSI_ts4:    // ts4
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->time_server[iIndex-SSI_ts1]); 
        }  
        break; 
        case SSI_dlse:    // dlse -- daylight saving enable
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->daylightsaving_enable?"checked":""); 
        }  
        break;    
       case SSI_ssid: //ssid
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->wifi_ssid);
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->dhcp_enable?"checked":"");
        }               
        break;    
        case SSI_ipad: //ipad
        {
            if (!sys->dhcp_enable)
            {
                if (strncasecmp(sys->ip_address, "automatic+via+DHCP", sizeof(sys->ip_address))==0)
                {
                    //sys->ip_address[0] = 0;
                    STRNCPY(sys->ip_address, sys_web.ip_address_string, sizeof(sys->ip_address));   
                }
                printed = snprintf(pcInsert, iInsertLen, "%s", sys->ip_address);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }            
        }               
        break;    
        case SSI_nmsk: //nmsk
        {
            if (!sys->dhcp_enable)
            {
                if (strncasecmp(sys->network_mask, "automatic+via+DHCP", sizeof(sys->network_mask))==0)
                {
                    //sys->network_mask[0] = 0;
                    STRNCPY(sys->network_mask, sys_web.network_mask_string, sizeof(sys->network_mask));  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", sys->network_mask);
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->syslog_server_ip);
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_archaic_units?"inches":"mm"); 
        }                        
        break;  
        case SSI_spdu: //spdu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_archaic_units?"ft/s":"m/s"); 
        }                        
        break;  
        case SSI_tmpu: //tmpu
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_archaic_units?"F":"C"); 
        }                        
        break;
        case SSI_uau: //uau
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_archaic_units?"checked":"");
        }               
        break; 
        case SSI_stck: //stck
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.stack_message);
        }               
        break; 
        case SSI_ipaddr: //ipaddr
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.ip_address_string);
        }               
        break;   
        case SSI_netmsk: //netmsk
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.network_mask_string);
        }               
        break;   
        case SSI_gatewy: //gatewy
        {
            if (!sys->dhcp_enable)
            {
                if (strncasecmp(sys->gateway, "automatic+via+DHCP", sizeof(sys->gateway))==0)
                {
                    //sys->gateway[0] = 0;
                    STRNCPY(sys->gateway, sys_web.gateway_string, sizeof(sys->gateway));                  
                }                
                printed = snprintf(pcInsert, iInsertLen, "%s", sys->gateway);
            }
            else
            {
                printed = snprintf(pcInsert, iInsertLen, "automatic via DHCP");
            }                          
        }                   
        break;
        case SSI_msck: //msck
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", sys_web.socket_max);
        }               
        break;  
        case SSI_bfail: //bfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", sys_web.bind_failures);
        }               
        break;  
        case SSI_cfail: //cfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", sys_web.connect_failures);
        }               
        break;  
        case SSI_sfail: //sfail
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", sys_web.syslog_transmit_failures);
        }               
        break;  
        case SSI_simpe: //simpe
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_simplified_english?"checked":"");
        }  
        break;         
        case SSI_mweek: //mweek
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->use_monday_as_week_start?"checked":"");
        }  
        break;         
        case SSI_colour: //colour
        {
            if (sys->use_simplified_english)
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.software_server);
        }               
        break; 
        case SSI_swlurl: //swlurl
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.software_url);
        }               
        break; 
        case SSI_swlfle: //swlfle
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.software_file);
        }               
        break;    
        case SSI_sloge: //sloge
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->syslog_enable?"checked":"");
        } 
        break;
        case SSI_pertyp: //pertyp
        {
            printed = snprintf(pcInsert, iInsertLen, "%d", sys->personality);
        } 
        break; 
        case SSI_wific: //wific
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->wifi_country);
        } 
        break;  
        case SSI_gway: //gway
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys_web.gateway_string);
        }                       
        break;
        case SSI_pernme: //pernme
        {
            switch(sys->personality)
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
            printed = snprintf(pcInsert, iInsertLen, "%02x:%02x:%02x:%02x:%02x:%02x\n", sys_web.mac[0], sys_web.mac[1], sys_web.mac[2], sys_web.mac[3], sys_web.mac[4], sys_web.mac[5]);              
        }
        break; 
        case SSI_hostn: // hostn
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->host_name);
        }               
        break; 
        case SSI_mquser: // mquser
        {
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->mqtt_user);
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
            printed = snprintf(pcInsert, iInsertLen, "%s", sys->mqtt_broker_address);
        }               
        break;
        case SSI_uptme: // uptme
        {
            printed = get_delta_string_from_delta_seconds(pcInsert, iInsertLen, unix_time - sys_web.boot_time);
            //printed = snprintf(pcInsert, iInsertLen, "%d", unix_time - sys_web.boot_time);
        }               
        break;
        case SSI_glat: // lattitude
        {
            printed = snprintf(pcInsert, iInsertLen, "%f", sys->latitude); 
        }                        
        break; 
        case SSI_glng: // longitude
        {
            printed = snprintf(pcInsert, iInsertLen, "%f", sys->longitude); 
        }                        
        break; 

        // *** system SSI end ***
        /************************************************************************************************************************* */

        default:
        {
            printed = application_ssi_handler(iIndex, pcInsert, iInsertLen, connection_state);

            if (printed == 0)
            {
                printed = snprintf(pcInsert, iInsertLen, "Unhandled SSI tag");
            }
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


