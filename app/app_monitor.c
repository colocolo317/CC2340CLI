/*
 * app_monitor.c
 *
 *  Created on: 2023/10/20
 *      Author: ch.wang
 */

#include <string.h>
#include <common/BLEAppUtil/inc/bleapputil_api.h>
#include <app_main.h>

extern uint8_t profileRole;
static uint8 bleInit = MONITOR_BLE_NOINIT;
static uint8 advState = MONITOR_ADV_OFF;
static uint8 numConn = 0;

static char strRole[20] = {'\0'};
static char strBleInit[7] = {'\0'};
static char strAdvState[4] = {'\0'};

AppMonitor_report_t report =
{
     .role = strRole,
     .initYet = strBleInit,
     .advOnOff= strAdvState,
     .connNum = 0
};

void Monitor_updateState(AppMonitor_state_type_e state_type, uint8 value)
{
    switch(state_type)
    {
    case APP_MONITOR_STATE_BLEROLE:
        // should not happen by now.
        break;
    case APP_MONITOR_STATE_BLEINIT:
        bleInit = value;
        break;
    case APP_MONITOR_STATE_ADV_ON_OFF:
        advState = value;
        break;
    case APP_MONITOR_STATE_CONN_NUM:
        numConn = value;
        break;
    default:
        break;
    }
}

AppMonitor_report_t Monitor_getStateReport(void)
{
    if(strlen(strRole) == 0)
    {
        if(profileRole & BLEAPPUTIL_CENTRAL_ROLE)
        {
            strcat(strRole,"Central");
        }
        if(profileRole & BLEAPPUTIL_PERIPHERAL_ROLE)
        {
            if(strlen(strRole) != 0) { strcat(strRole,", "); }
            strcat(strRole,"Peripheral");
        }
        if(profileRole & BLEAPPUTIL_BROADCASTER_ROLE)
        {
            if(strlen(strRole) != 0) { strcat(strRole,", "); }
            strcat(strRole,"Broadcaster");
        }
        if(profileRole & BLEAPPUTIL_OBSERVER_ROLE)
        {
            if(strlen(strRole) != 0) { strcat(strRole,", "); }
            strcat(strRole,"Observer");
        }
    }

    if(bleInit){
        strcpy(strBleInit,"INIT");
    }else{
        strcpy(strBleInit,"NOINIT");
    }

    if(advState){
        strcpy(strAdvState, "ON");
    }else{
        strcpy(strAdvState, "OFF");
    }

    report.connNum = numConn;

    return report;
}

uint8 Monitor_getState(AppMonitor_state_type_e state_type)
{
	uint8 ret = 0;
	switch(state_type)
	{
	case APP_MONITOR_STATE_BLEINIT:
		ret = bleInit;
		break;
	case APP_MONITOR_STATE_ADV_ON_OFF:
		ret = advState;
		break;
	case APP_MONITOR_STATE_CONN_NUM:
		ret = numConn;
		break;
	default:
		break;
	}

	return ret;
}
