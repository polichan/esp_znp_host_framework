/**************************************************************************************************
 * Filename:       dataSendRcv.c
 * Description:    This file contains dataSendRcv application.
 *
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "znp.h"
#include "rpc.h"
#include "mtSys.h"
#include "mtZdo.h"
#include "mtAf.h"
#include "mtParser.h"
#include "rpcTransport.h"
#include "dbgPrint.h"
#include "hostConsole.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPES
 */

/*********************************************************************
 * LOCAL VARIABLE
 */

// init ZDO device state
devStates_t devState = DEV_HOLD;
uint8_t gSrcEndPoint = 1;
uint8_t gDstEndPoint = 1;

/***********************************************************************/

static const char *TAG = "znp";
/*********************************************************************
 * LOCAL FUNCTIONS
 */
// ZDO Callbacks
static uint8_t mtSysPingIndCb(PingSrspFormat_t *msg);
static uint8_t mtZdoStateChangeIndCb(uint8_t newDevState);
static uint8_t mtZdoSimpleDescRspCb(SimpleDescRspFormat_t *msg);
static uint8_t mtZdoActiveEpRspCb(ActiveEpRspFormat_t *msg);
static uint8_t mtZdoEndDeviceAnnceIndCb(EndDeviceAnnceIndFormat_t *msg);

static uint8_t mtZdoMgmtLqiRspCb(MgmtLqiRspFormat_t *msg);

// SYS Callbacks

static uint8_t mtSysResetIndCb(ResetIndFormat_t *msg);

// AF callbacks
static uint8_t mtAfDataConfirmCb(DataConfirmFormat_t *msg);
static uint8_t mtAfIncomingMsgCb(IncomingMsgFormat_t *msg);

// helper functions
static uint8_t setNVStartup(uint8_t startupOption);
static uint8_t setNVChanList(uint32_t chanList);
static uint8_t setNVPanID(uint32_t panId);
static uint8_t setNVDevType(uint8_t devType);
static int32_t startNetwork(void);
static int32_t registerAf(void);

/*********************************************************************
 * CALLBACK FUNCTIONS
 */

// SYS callbacks
static mtSysCb_t mtSysCb =
    {mtSysPingIndCb, NULL, NULL, mtSysResetIndCb, NULL, NULL, NULL, NULL, NULL, NULL,
     NULL, NULL, NULL, NULL};

static mtZdoCb_t mtZdoCb =
    {NULL,                     // MT_ZDO_NWK_ADDR_RSP
     NULL,                     // MT_ZDO_IEEE_ADDR_RSP
     NULL,                     // MT_ZDO_NODE_DESC_RSP
     NULL,                     // MT_ZDO_POWER_DESC_RSP
     mtZdoSimpleDescRspCb,     // MT_ZDO_SIMPLE_DESC_RSP
     mtZdoActiveEpRspCb,       // MT_ZDO_ACTIVE_EP_RSP
     NULL,                     // MT_ZDO_MATCH_DESC_RSP
     NULL,                     // MT_ZDO_COMPLEX_DESC_RSP
     NULL,                     // MT_ZDO_USER_DESC_RSP
     NULL,                     // MT_ZDO_USER_DESC_CONF
     NULL,                     // MT_ZDO_SERVER_DISC_RSP
     NULL,                     // MT_ZDO_END_DEVICE_BIND_RSP
     NULL,                     // MT_ZDO_BIND_RSP
     NULL,                     // MT_ZDO_UNBIND_RSP
     NULL,                     // MT_ZDO_MGMT_NWK_DISC_RSP
     mtZdoMgmtLqiRspCb,        // MT_ZDO_MGMT_LQI_RSP
     NULL,                     // MT_ZDO_MGMT_RTG_RSP
     NULL,                     // MT_ZDO_MGMT_BIND_RSP
     NULL,                     // MT_ZDO_MGMT_LEAVE_RSP
     NULL,                     // MT_ZDO_MGMT_DIRECT_JOIN_RSP
     NULL,                     // MT_ZDO_MGMT_PERMIT_JOIN_RSP
     mtZdoStateChangeIndCb,    // MT_ZDO_STATE_CHANGE_IND
     mtZdoEndDeviceAnnceIndCb, // MT_ZDO_END_DEVICE_ANNCE_IND
     NULL,                     // MT_ZDO_SRC_RTG_IND
     NULL,                     // MT_ZDO_BEACON_NOTIFY_IND
     NULL,                     // MT_ZDO_JOIN_CNF
     NULL,                     // MT_ZDO_NWK_DISCOVERY_CNF
     NULL,                     // MT_ZDO_CONCENTRATOR_IND_CB
     NULL,                     // MT_ZDO_LEAVE_IND
     NULL,                     // MT_ZDO_STATUS_ERROR_RSP
     NULL,                     // MT_ZDO_MATCH_DESC_RSP_SENT
     NULL, NULL};

static mtAfCb_t mtAfCb =
    {
        mtAfDataConfirmCb, // MT_AF_DATA_CONFIRM
        mtAfIncomingMsgCb, // MT_AF_INCOMING_MSG
        NULL,              // MT_AF_INCOMING_MSG_EXT
        NULL,              // MT_AF_DATA_RETRIEVE
        NULL,              // MT_AF_REFLECT_ERROR
};
typedef struct
{
    uint16_t ChildAddr;
    uint8_t Type;

} ChildNode_t;

typedef struct
{
    uint16_t NodeAddr;
    uint8_t Type;
    uint8_t ChildCount;
    ChildNode_t childs[256];
} Node_t;

Node_t nodeList[64];
uint8_t nodeCount = 0;

/********************************************************************
 * START OF SYS CALL BACK FUNCTIONS
 */

static uint8_t mtSysResetIndCb(ResetIndFormat_t *msg)
{

    consolePrint(TAG, "ZNP Version: %d.%d.%d", msg->MajorRel, msg->MinorRel,
                 msg->HwRev);
    return 0;
}

static uint8_t mtSysPingIndCb(PingSrspFormat_t *msg)
{
    consolePrint(TAG, "ping: %s", msg);
    return 0;
}

/********************************************************************
 * START OF ZDO CALL BACK FUNCTIONS
 */

/********************************************************************
 * @fn     Callback function for ZDO State Change Indication

 * @brief  receives the AREQ status and specifies the change ZDO state
 *
 * @param  uint8 zdoState
 *
 * @return SUCCESS or FAILURE
 */

static uint8_t mtZdoStateChangeIndCb(uint8_t newDevState)
{

    switch (newDevState)
    {
    case DEV_HOLD:
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Initialized - not started automatically");
        break;
    case DEV_INIT:
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Initialized - not connected to anything");
        break;
    case DEV_NWK_DISC:
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Discovering PAN's to join");
        consolePrint(TAG, "Network Discovering");
        break;
    case DEV_NWK_JOINING:
        dbg_print(TAG, PRINT_LEVEL_INFO, "mtZdoStateChangeIndCb: Joining a PAN");
        consolePrint(TAG, "Network Joining");
        break;
    case DEV_NWK_REJOIN:
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: ReJoining a PAN, only for end devices");
        consolePrint(TAG, "Network Rejoining");
        break;
    case DEV_END_DEVICE_UNAUTH:
        consolePrint(TAG, "Network Authenticating");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Joined but not yet authenticated by trust center");
        break;
    case DEV_END_DEVICE:
        consolePrint(TAG, "Network Joined");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Started as device after authentication");
        break;
    case DEV_ROUTER:
        consolePrint(TAG, "Network Joined");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Device joined, authenticated and is a router");
        break;
    case DEV_COORD_STARTING:
        consolePrint(TAG, "Network Starting");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Started as Zigbee Coordinator");
        break;
    case DEV_ZB_COORD:
        consolePrint(TAG, "Network Started");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Started as Zigbee Coordinator");
        break;
    case DEV_NWK_ORPHAN:
        consolePrint(TAG, "Network Orphaned");
        dbg_print(TAG, PRINT_LEVEL_INFO,
                  "mtZdoStateChangeIndCb: Device has lost information about its parent");
        break;
    default:
        dbg_print(TAG, PRINT_LEVEL_INFO, "mtZdoStateChangeIndCb: unknown state");
        break;
    }

    zigbeeNetworkState->devState = (devStates_t)newDevState;

    return SUCCESS;
}
static uint8_t mtZdoSimpleDescRspCb(SimpleDescRspFormat_t *msg)
{

    if (msg->Status == MT_RPC_SUCCESS)
    {
        consolePrint(TAG, "\tEndpoint: 0x%02X", msg->Endpoint);
        consolePrint(TAG, "\tProfileID: 0x%04X", msg->ProfileID);
        consolePrint(TAG, "\tDeviceID: 0x%04X", msg->DeviceID);
        consolePrint(TAG, "\tDeviceVersion: 0x%02X", msg->DeviceVersion);
        consolePrint(TAG, "\tNumInClusters: %d", msg->NumInClusters);
        uint32_t i;
        for (i = 0; i < msg->NumInClusters; i++)
        {
            consolePrint(TAG, "\t\tInClusterList[%d]: 0x%04X", i,
                         msg->InClusterList[i]);
        }
        consolePrint(TAG, "\tNumOutClusters: %d", msg->NumOutClusters);
        for (i = 0; i < msg->NumOutClusters; i++)
        {
            consolePrint(TAG, "\t\tOutClusterList[%d]: 0x%04X", i,
                         msg->OutClusterList[i]);
        }
        consolePrint(TAG, "");
    }
    else
    {
        consolePrint(TAG, "SimpleDescRsp Status: FAIL 0x%02X", msg->Status);
    }

    return msg->Status;
}

static uint8_t mtZdoMgmtLqiRspCb(MgmtLqiRspFormat_t *msg)
{
    uint8_t devType = 0;
    uint8_t devRelation = 0;
    MgmtLqiReqFormat_t req;
    if (msg->Status == MT_RPC_SUCCESS)
    {
        nodeList[nodeCount].NodeAddr = msg->SrcAddr;
        nodeList[nodeCount].Type = (msg->SrcAddr == 0 ? DEVICETYPE_COORDINATOR : DEVICETYPE_ROUTER);
        nodeList[nodeCount].ChildCount = 0;
        uint32_t i;
        for (i = 0; i < msg->NeighborLqiListCount; i++)
        {
            devType = msg->NeighborLqiList[i].DevTyp_RxOnWhenIdle_Relat & 3;
            devRelation = ((msg->NeighborLqiList[i].DevTyp_RxOnWhenIdle_Relat >> 4) & 7);
            if (devRelation == 1 || devRelation == 3)
            {
                uint8_t cCount = nodeList[nodeCount].ChildCount;
                nodeList[nodeCount].childs[cCount].ChildAddr =
                    msg->NeighborLqiList[i].NetworkAddress;
                nodeList[nodeCount].childs[cCount].Type = devType;
                nodeList[nodeCount].ChildCount++;
                if (devType == DEVICETYPE_ROUTER)
                {
                    req.DstAddr = msg->NeighborLqiList[i].NetworkAddress;
                    req.StartIndex = 0;
                    zdoMgmtLqiReq(&req);
                }
            }
        }
        nodeCount++;
    }
    else
    {
        consolePrint(TAG, "MgmtLqiRsp Status: FAIL 0x%02X", msg->Status);
    }

    return msg->Status;
}

static uint8_t mtZdoActiveEpRspCb(ActiveEpRspFormat_t *msg)
{

    // SimpleDescReqFormat_t simReq;
    consolePrint(TAG, "NwkAddr: 0x%04X", msg->NwkAddr);
    if (msg->Status == MT_RPC_SUCCESS)
    {
        consolePrint(TAG, "Number of Endpoints: %d\nActive Endpoints: ",
                     msg->ActiveEPCount);
        uint32_t i;
        for (i = 0; i < msg->ActiveEPCount; i++)
        {
            consolePrint(TAG, "0x%02X\t", msg->ActiveEPList[i]);
        }
        consolePrint(TAG, "");
    }
    else
    {
        consolePrint(TAG, "ActiveEpRsp Status: FAIL 0x%02X", msg->Status);
    }

    return msg->Status;
}

static uint8_t mtZdoEndDeviceAnnceIndCb(EndDeviceAnnceIndFormat_t *msg)
{

    ActiveEpReqFormat_t actReq;
    actReq.DstAddr = msg->NwkAddr;
    actReq.NwkAddrOfInterest = msg->NwkAddr;

    consolePrint(TAG, "\nNew device joined network.");
    zdoActiveEpReq(&actReq);
    return 0;
}

/********************************************************************
 * AF CALL BACK FUNCTIONS
 */

static uint8_t mtAfDataConfirmCb(DataConfirmFormat_t *msg)
{

    if (msg->Status == MT_RPC_SUCCESS)
    {
        consolePrint(TAG, "Message transmited Succesfully!");
    }
    else
    {
        consolePrint(TAG, "Message failed to transmit");
    }
    return msg->Status;
}
static uint8_t mtAfIncomingMsgCb(IncomingMsgFormat_t *msg)
{

    consolePrint(TAG,
                 "\nIncoming Message from Endpoint 0x%02X and Address 0x%04X:",
                 msg->SrcEndpoint, msg->SrcAddr);
    msg->Data[msg->Len] = '\0';
    consolePrint(TAG, "%s", (char *)msg->Data);
    consolePrint(TAG,
                 "\nEnter message to send or type CHANGE to change the destination \nor QUIT to exit:");

    return 0;
}

/********************************************************************
 * HELPER FUNCTIONS
 */
// helper functions for building and sending the NV messages
static uint8_t setNVStartup(uint8_t startupOption)
{
    uint8_t status;
    OsalNvWriteFormat_t nvWrite;

    // sending startup option
    nvWrite.Id = ZCD_NV_STARTUP_OPTION;
    nvWrite.Offset = 0;
    nvWrite.Len = 1;
    nvWrite.Value[0] = startupOption;
    status = sysOsalNvWrite(&nvWrite);

    dbg_print(TAG, PRINT_LEVEL_INFO, "");

    dbg_print(TAG, PRINT_LEVEL_INFO, "NV Write Startup Option cmd sent[%d]...",
              status);

    return status;
}

static uint8_t setNVDevType(uint8_t devType)
{
    uint8_t status;
    OsalNvWriteFormat_t nvWrite;

    // setting dev type
    nvWrite.Id = ZCD_NV_LOGICAL_TYPE;
    nvWrite.Offset = 0;
    nvWrite.Len = 1;
    nvWrite.Value[0] = devType;
    status = sysOsalNvWrite(&nvWrite);

    dbg_print(TAG, PRINT_LEVEL_INFO, "");
    dbg_print(TAG, PRINT_LEVEL_INFO, "NV Write Device Type cmd sent... [%d]",
              status);

    return status;
}

static uint8_t setNVPanID(uint32_t panId)
{
    uint8_t status;
    OsalNvWriteFormat_t nvWrite;

    dbg_print(TAG, PRINT_LEVEL_INFO, "");
    dbg_print(TAG, PRINT_LEVEL_INFO, "NV Write PAN ID cmd sending...");

    nvWrite.Id = ZCD_NV_PANID;
    nvWrite.Offset = 0;
    nvWrite.Len = 2;
    nvWrite.Value[0] = LO_UINT16(panId);
    nvWrite.Value[1] = HI_UINT16(panId);
    status = sysOsalNvWrite(&nvWrite);

    dbg_print(TAG, PRINT_LEVEL_INFO, "");
    dbg_print(TAG, PRINT_LEVEL_INFO, "NV Write PAN ID cmd sent...[%d]", status);

    return status;
}

static uint8_t setNVChanList(uint32_t chanList)
{
    OsalNvWriteFormat_t nvWrite;
    uint8_t status;
    // setting chanList
    nvWrite.Id = ZCD_NV_CHANLIST;
    nvWrite.Offset = 0;
    nvWrite.Len = 4;
    nvWrite.Value[0] = BREAK_UINT32(chanList, 0);
    nvWrite.Value[1] = BREAK_UINT32(chanList, 1);
    nvWrite.Value[2] = BREAK_UINT32(chanList, 2);
    nvWrite.Value[3] = BREAK_UINT32(chanList, 3);
    status = sysOsalNvWrite(&nvWrite);

    dbg_print(TAG, PRINT_LEVEL_INFO, "");
    dbg_print(TAG, PRINT_LEVEL_INFO, "NV Write Channel List cmd sent...[%d]",
              status);

    return status;
}

uint8_t dType;
static int32_t startNetwork(void)
{
    uint8_t devType = DEVICETYPE_COORDINATOR;
    int32_t status = MT_RPC_SUCCESS;
    uint8_t newNwk = 0;

    status = setNVStartup(
        ZCD_STARTOPT_CLEAR_STATE | ZCD_STARTOPT_CLEAR_CONFIG);
    newNwk = 1;

    if (status != MT_RPC_SUCCESS)
    {
        dbg_print(TAG, PRINT_LEVEL_WARNING, "network start failed");
        return -1;
    }
    consolePrint(TAG, "Resetting ZNP");
    ResetReqFormat_t resReq;
    resReq.Type = 1;
    sysResetReq(&resReq);
    // flush the rsp
    rpcWaitMqClientMsg(5000);

    if (newNwk)
    {
#ifndef CC26xx
        // 协调器
        devType = DEVICETYPE_COORDINATOR;
        status = setNVDevType(devType);
        if (status != MT_RPC_SUCCESS)
        {
            dbg_print(TAG, PRINT_LEVEL_WARNING, "setNVDevType failed");
            return 0;
        }
#endif // CC26xx
       // Select random PAN ID for Coord and join any PAN for RTR/ED
        status = setNVPanID(0xFFFF);
        if (status != MT_RPC_SUCCESS)
        {
            dbg_print(TAG, PRINT_LEVEL_WARNING, "setNVPanID failed");
            return -1;
        }
        // 默认 11 信道
        status = setNVChanList(1 << atoi("11"));
        if (status != MT_RPC_SUCCESS)
        {
            dbg_print(TAG, PRINT_LEVEL_INFO, "setNVPanID failed");
            return -1;
        }
    }

    registerAf();
    consolePrint(TAG, "EndPoint: 1");

    status = zdoInit();
    if (status == NEW_NETWORK)
    {
        dbg_print(TAG, PRINT_LEVEL_INFO, "zdoInit NEW_NETWORK");
        status = MT_RPC_SUCCESS;
    }
    else if (status == RESTORED_NETWORK)
    {
        dbg_print(TAG, PRINT_LEVEL_INFO, "zdoInit RESTORED_NETWORK");
        status = MT_RPC_SUCCESS;
    }
    else
    {
        dbg_print(TAG, PRINT_LEVEL_INFO, "zdoInit failed");
        status = -1;
    }

    dbg_print(TAG, PRINT_LEVEL_INFO, "process zdoStatechange callbacks");

    // flush AREQ ZDO State Change messages
    while (status != -1)
    {
        status = rpcWaitMqClientMsg(5000);

        if (((devType == DEVICETYPE_COORDINATOR) && (zigbeeNetworkState->devState == DEV_ZB_COORD)) || ((devType == DEVICETYPE_ROUTER) && (zigbeeNetworkState->devState == DEV_ROUTER)) || ((devType == DEVICETYPE_ENDDEVICE) && (zigbeeNetworkState->devState == DEV_END_DEVICE)))
        {
            break;
        }
    }
    // set startup option back to keep configuration in case of reset
    status = setNVStartup(0);
    if (zigbeeNetworkState->devState < DEV_END_DEVICE)
    {
        // start network failed
        return -1;
    }

    return 0;
}

static int32_t registerAf(void)
{
    int32_t status = 0;
    RegisterFormat_t reg;

    reg.EndPoint = 1;
    reg.AppProfId = 0x0104;
    reg.AppDeviceId = 0x0100;
    reg.AppDevVer = 1;
    reg.LatencyReq = 0;
    reg.AppNumInClusters = 1;
    reg.AppInClusterList[0] = 0x0006;
    reg.AppNumOutClusters = 0;

    status = afRegister(&reg);
    return status;
}

static void displayDevices(void)
{
    ActiveEpReqFormat_t actReq;
    int32_t status;

    MgmtLqiReqFormat_t req;

    req.DstAddr = 0;
    req.StartIndex = 0;
    nodeCount = 0;
    zdoMgmtLqiReq(&req);
    do
    {
        status = rpcWaitMqClientMsg(1000);
    } while (status != -1);

    consolePrint(TAG, "Available devices:");
    uint8_t i;
    for (i = 0; i < nodeCount; i++)
    {
        char *devtype =
            (nodeList[i].Type == DEVICETYPE_ROUTER ? "ROUTER" : "COORDINATOR");

        consolePrint(TAG, "Type: %s", devtype);
        actReq.DstAddr = nodeList[i].NodeAddr;
        actReq.NwkAddrOfInterest = nodeList[i].NodeAddr;
        zdoActiveEpReq(&actReq);
        rpcGetMqClientMsg();
        do
        {
            status = rpcWaitMqClientMsg(1000);
        } while (status != -1);
        uint8_t cI;
        for (cI = 0; cI < nodeList[i].ChildCount; cI++)
        {
            uint8_t type = nodeList[i].childs[cI].Type;
            if (type == DEVICETYPE_ENDDEVICE)
            {
                consolePrint(TAG, "Type: END DEVICE");
                actReq.DstAddr = nodeList[i].childs[cI].ChildAddr;
                actReq.NwkAddrOfInterest = nodeList[i].childs[cI].ChildAddr;
                zdoActiveEpReq(&actReq);
                status = 0;
                rpcGetMqClientMsg();
                while (status != -1)
                {
                    status = rpcWaitMqClientMsg(1000);
                }
            }
        }
        consolePrint(TAG, "");
    }
}
/*********************************************************************
 * INTERFACE FUNCTIONS
 */
uint32_t znpInit(void)
{

    int32_t status = 0;
    uint32_t msgCnt = 0;

    // Flush all messages from the que
    while (status != -1)
    {
        status = rpcWaitMqClientMsg(10);
        if (status != -1)
        {
            msgCnt++;
        }
    }

    dbg_print(TAG, PRINT_LEVEL_INFO, "flushed %d message from msg queue", msgCnt);

    // Register Callbacks MT system callbacks
    sysRegisterCallbacks(mtSysCb);
    zdoRegisterCallbacks(mtZdoCb);
    afRegisterCallbacks(mtAfCb);

    return 0;
}

uint8_t initDone = 0;
void *znpMsgProcess(void *argument)
{
    if (initDone)
    {
        rpcWaitMqClientMsg(10000);
    }

    return 0;
}

void appProcess(void *argument)
{
    int32_t status;

    // Flush all messages from the que
    do
    {
        status = rpcWaitMqClientMsg(50);
    } while (status != -1);

    devState = DEV_HOLD;

    status = startNetwork();
    if (status != -1)
    {
        consolePrint(TAG, "Network up");
    }
    else
    {
        consolePrint(TAG, "Network Error");
    }

    OsalNvWriteFormat_t nvWrite;
    nvWrite.Id = ZCD_NV_ZDO_DIRECT_CB;
    nvWrite.Offset = 0;
    nvWrite.Len = 1;
    nvWrite.Value[0] = 1;
    status = sysOsalNvWrite(&nvWrite);

    displayDevices();

    sysGetExtAddr();

    return;
}
