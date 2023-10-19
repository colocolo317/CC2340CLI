/******************************************************************************

@file  app_data.c

@brief This file contains the Data Stream application functionality.

Group: WCS, BTS
$Target Device: DEVICES $

******************************************************************************
$License: BSD3 2022 $
******************************************************************************
$Release Name: PACKAGE NAME $
$Release Date: PACKAGE RELEASE DATE $
*****************************************************************************/

//*****************************************************************************
//! Includes
//*****************************************************************************
#include <string.h>
#include <time.h>
#include <ti/drivers/GPIO.h>
#include <common/Profiles/data_stream/data_stream_profile.h>
#include <common/BLEAppUtil/inc/bleapputil_api.h>
#include <common/MenuModule/menu_module.h>
#include <app_main.h>
#include <trans_uartApi.h>

//*****************************************************************************
//! Defines
//*****************************************************************************
#define DS_CCC_UPDATE_NOTIFICATION_ENABLED  1

//*****************************************************************************
//! Globals
//*****************************************************************************

//*****************************************************************************
//!LOCAL FUNCTIONS
//*****************************************************************************

static void DS_onCccUpdateCB( uint16 connHandle, uint16 pValue );
static void DS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len );

//*****************************************************************************
//!APPLICATION CALLBACK
//*****************************************************************************
// Data Stream application callback function for incoming data
static DSP_cb_t ds_profileCB =
{
  DS_onCccUpdateCB,
  DS_incomingDataCB
};

//*****************************************************************************
//! Functions
//*****************************************************************************
/*********************************************************************
 * @fn      DS_onCccUpdateCB
 *
 * @brief   Callback from Data_Stream_Profile indicating ccc update
 *
 * @param   cccUpdate - pointer to data structure used to store ccc update
 *
 * @return  SUCCESS or stack call status
 *
 * REMARK: UART RX to BLE char
 */
static void DS_onCccUpdateCB( uint16 connHandle, uint16 pValue )
{
    // MenuModule closed, no function in here
  if ( pValue == DS_CCC_UPDATE_NOTIFICATION_ENABLED)
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                      "DataStream status: CCC Update - connectionHandle: "
                      MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                      "Notifications enabled", connHandle);
  }
  else
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE, 0,
                      "DataStream status: CCC Update - connectionHandle: "
                      MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                      "Notifications disabled", connHandle);
  }
}

/*********************************************************************
 * @fn      DS_incomingDataCB
 *
 * @brief   Callback from Data_Stream_Profile indicating incoming data
 *
 * @param   dataIn - pointer to data structure used to store incoming data
 *
 * @return  SUCCESS or stack call status
 *
 * REMARK: BLE char to UART TX
 */
static void DS_incomingDataCB( uint16 connHandle, char *pValue, uint16 len )
{
  bStatus_t status = SUCCESS;
  char dataOut[] = "Data size is too long";
  //char printData[len+1];
  uint16 i = 0;

  // Clear lines
  MenuModule_clearLines(APP_MENU_PROFILE_STATUS_LINE1, APP_MENU_PROFILE_STATUS_LINE3);

  // Toggle LEDs to indicate that data was received
  GPIO_toggle( CONFIG_GPIO_LED_RED );
  GPIO_toggle( CONFIG_GPIO_LED_GREEN );

  // The incoming data length was too large
  if ( len == 0 )
  {
    MenuModule_printf(APP_MENU_PROFILE_STATUS_LINE1, 0,
                      "DataStream status: Incoming data - connectionHandle: "
                      MENU_MODULE_COLOR_YELLOW "%d " MENU_MODULE_COLOR_RESET
                      "Error: " MENU_MODULE_COLOR_RED "%s" MENU_MODULE_COLOR_RESET,
                      connHandle, dataOut);

    // Send error message over GATT notification
    status = DSP_sendData( (uint8 *)dataOut, sizeof( dataOut ) );
  }
  // New data received from peer device
  else
  {
    int_fast16_t uart_status =  trans_uartTxSend((uint8*)pValue, len);

    // [Canceled] Change upper case to lower case and lower case to upper case
    // [Canceled] Echo the incoming data over GATT notification
  }
}

/*********************************************************************
 * @fn      DataStream_start
 *
 * @brief   This function is called after stack initialization,
 *          the purpose of this function is to initialize and
 *          register the Data Stream profile.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t DataStream_start( void )
{
  bStatus_t status = SUCCESS;

  status = DSP_start( &ds_profileCB );
  if( status != SUCCESS )
  {
    // Return status value
    return status;
  }

  // Set LEDs
  GPIO_write( CONFIG_GPIO_LED_RED, CONFIG_LED_OFF );
  GPIO_write( CONFIG_GPIO_LED_GREEN, CONFIG_LED_ON );

  return ( SUCCESS );
}
