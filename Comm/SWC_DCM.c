/******************************************************************************
* Device       : CCP(Central Console Panel with FPM)
* Creator      : JM.SEO
* Description  : Diagnostic DTC State Management functions
* Creation Date : 2024-08-21
******************************************************************************/
/*============================================================================*/
// INCLUDE(Application)
/*============================================================================*/
/* Include Header File, Autosar Platform */
#include "Dcm_PCTypes.h"
#include "Dcm_DsdInternal.h"

#include "prj_typedefs.h"

#if 0
/* Include Header File, Application */
#include "user_typedefs.h"
#include "user_defs.h"
#include "user_api.h"
#include "user_ccp_api.h"
#include "ccp_funcprototype.h"


/*============================================================================*/
// Declaration of Global Functions
/*============================================================================*/


/*============================================================================*/
// Declaration of Static Functions
/*============================================================================*/


/*============================================================================*/
// Declaration of External Variables
/*============================================================================*/
/* Declaration Used MM-CAN Communication State Value */
extern u8 LCAN_PDU_Ste;
/* Declaration Used Request When a NvMemory Write */
extern u8 Req_WRNvM_DTCInfo;
extern u8 Req_WRNvM_ACCCnt;

/*============================================================================*/
// Declaration of Global Variables
/*============================================================================*/
/* Declaration Diagnostic Control Variable, State of OK/Error */
DiagCCPSteCtrl DiagCCPSte;
/* Declaration Detect Start & End When a CAN Bus Off Check */
u8 b_LCan_BusOff_Start = Off;
u8 b_LCan_BusOff_End = Off;

/*============================================================================*/
// Declaration of Constant and Macro
/*============================================================================*/
/* Declaration Parameter Type Enum */
typedef enum
{
	Ste_BusOff_Off = 0u,
	Ste_BusOff_Detect,
	Ste_BusOff_On
} e_BusOffDTCCtrlState;


/*============================================================================*/
// Declaration of Static Variables
/*============================================================================*/


/*============================================================================*/
//	Application Functions - Initialize Diagnostic CCP Process
/*============================================================================*/
void fc_DiagCCPInit(void)
{
  /* DIAG Control Variable Initialize */
  DiagCCPSte.SwitchSte = kLCAN_DIAGSte_OK;
  DiagCCPSte.CANCommSte = kLCAN_DIAGSte_OK;
  DiagCCPSte.TouchSte = kLCAN_DIAGSte_NOTSUPPT;
  DiagCCPSte.HapticSte = kLCAN_DIAGSte_NOTSUPPT;

  /* CAN BUS Off Check Variable Initialzie */
  b_LCan_BusOff_Start = Off;
  b_LCan_BusOff_End = Off;
}


/*============================================================================*/
// Application Functions - Diagnostic DTC Setting Each DTC States
/*============================================================================*/
void Set_DiagCCPState(u8 dtc_idx,u8 dtc_ste)
{
	/* DTC Setting when Battery is Normal voltage(9~16V) */
	if(GetX(b_DTCInhibit)==Off)
	{
		switch(dtc_idx)
		{
		case kDIAGChkType_SWITCH:
			if(DiagCCPSte.SwitchSte!=dtc_ste) {
				DiagCCPSte.SwitchSte = dtc_ste;
			}
			break;
		case kDIAGChkType_CANSTE:
			if(DiagCCPSte.CANCommSte!=dtc_ste) {
				DiagCCPSte.CANCommSte = dtc_ste;
			}
			break;
		case kDIAGChkType_TOUCH:
			if(DiagCCPSte.TouchSte!=dtc_ste) {
				DiagCCPSte.TouchSte = dtc_ste;
			}
			break;
		case kDIAGChkType_HAPTIC:
			if(DiagCCPSte.HapticSte!=dtc_ste) {
				DiagCCPSte.HapticSte = dtc_ste;
			}
			break;
		default : /* No action required */
			break;
		}
	}
}
#endif

/*============================================================================*/
// Platform DCM Application Functions - Diagnostic Read DTC Information
/*============================================================================*/
Std_ReturnType Dcm_ReadDTCInformation(Dcm_OpStatusType OpStatus,Dcm_MsgContextType *pMsgContext)
{
	Std_ReturnType LddRetVal = RTE_E_OK;

	Dcm_ExternalProcessingDone(pMsgContext); /* Response */

	return LddRetVal;
}

/*============================================================================*/
// Platform DCM Application Functions - Diagnostic Clear DTC Information
/*============================================================================*/
Std_ReturnType Dcm_ClearDiagnosticInformation(Dcm_OpStatusType OpStatus,Dcm_MsgContextType *pMsgContext)
{
	Std_ReturnType LddRetVal = RTE_E_OK;

	Dcm_ExternalProcessingDone(pMsgContext); /* Response */

	return LddRetVal;
}
