/******************************************************************************

 @file  data_stream_server.h

 @brief This file contains the Data Stream service definitions and prototypes.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2010 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

#ifndef DATASTREAMSERVER_H
#define DATASTREAMSERVER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */
// Service UUID
// FIXED: data_stream_server.h didn't ref this one
#define DSS_SERV_UUID 0xFFF0

// Characteristic defines
#define DSS_DATAIN_ID   0
#define DSS_DATAIN_UUID 0xFFF1

// Characteristic defines
#define DSS_DATAOUT_ID   1
#define DSS_DATAOUT_UUID 0xFFF2

// Maximum allowed length for incoming data
#define DSS_MAX_DATA_IN_LEN 128

/*********************************************************************
 * TYPEDEFS
 */
// Data structure used to store incoming data
typedef struct
{
  uint16 connHandle;
  uint16 len;
  char pValue[];
} DSS_dataIn_t;

// Data structure used to store ccc update
typedef struct
{
  uint16 connHandle;
  uint16 value;
} DSS_cccUpdate_t;

/*********************************************************************
 * Profile Callbacks
 */
// Callback to indicate client characteristic configuration has been updated
typedef void (*DSS_onCccUpdate_t)( char *pValue );

// Callback when data is received
typedef void (*DSS_incomingData_t)( char *pValue );

typedef struct
{
  DSS_onCccUpdate_t         pfnOnCccUpdateCB;     // Called when client characteristic configuration has been updated
  DSS_incomingData_t        pfnIncomingDataCB;    // Called when receiving data
} DSS_cb_t;

/*********************************************************************
 * API FUNCTIONS
 */

/*********************************************************************
 * @fn      DSS_addService
 *
 * @brief   This function initializes the Data Stream Server service
 *          by registering GATT attributes with the GATT server.
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t DSS_addService( void );

/*
 * @fn      DSS_registerProfileCBs
 *
 * @brief   Registers the profile callback function. Only call
 *          this function once.
 *
 * @param   profileCallback - pointer to profile callback.
 *
 * @return  SUCCESS or INVALIDPARAMETER
 */
bStatus_t DSS_registerProfileCBs( DSS_cb_t *profileCallback );

/*
 * @fn      DSS_setParameter
 *
 * @brief   Set a Data Stream Service parameter.
 *
 * @param   param - Characteristic UUID
 * @param   pValue - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 * @param   len - length of data to write
 *
 * @return  SUCCESS or stack call status
 */
bStatus_t DSS_setParameter( uint8 param, void *pValue, uint16 len);

gattAttribute_t* DSS_getDefaultNotifyGatt(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* DATASTREAMSERVER_H */
