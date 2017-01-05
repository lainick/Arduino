/*

*/

#include <EEPROM.h>

//#define __YUNGSHIN_TESTING__ // 讓測試用CODE有效
//#define __EEPROM_SAVE_STATUS__ // 使用EEPROM SAVE

// define const value
#define DI_NUMBER 16
#define DO_NUMBER 16
#define DI_START_PIN 22
#define DO_START_PIN 38
#define FEEDING_INTERVAL 3500 // 補料Timeout時間 (ms)
#define DISCHARGE_UP_INTERVAL 300 // 車胚刀上升時出料桿收起時間 (ms)
#define RETRY 3 // Retry次數定義
#define STATUS_COUNT 20
//
// define DI
#define DI_AUTO_MODE              (DI_START_PIN + 0)  // 雙程操作模式訊號
#define DI_SINGLE_MODE            (DI_START_PIN + 1)  // 單程操作模式訊號
#define DI_MANUAL_MODE            (DI_START_PIN + 2)  // 手動狀態訊號
#define DI_FEEDING_WORKING_BUTTON (DI_START_PIN + 3)  // 進料按鈕
#define DI_ENFORCE_STOP_BUTTON    (DI_START_PIN + 4)  // 緊急停止按鈕訊號
#define DI_STOP_BUTTON            (DI_START_PIN + 5)  // 停止按鈕訊號
#define DI_SIZE_FEED_LOW_LOC      (DI_START_PIN + 6)  // 進料輪下定位點 (正常位置)
#define DI_SIZE_FEED_HIGH_LOC     (DI_START_PIN + 7)  // 進料輪上定位點
#define DI_AXIS2_LOW_LOC          (DI_START_PIN + 8)  // 車胚刀2下定位點
#define DI_AXIS2_HIGH_LOC         (DI_START_PIN + 9)  // 車胚刀2上定位點
#define DI_AXIS1_LOW_LOC          (DI_START_PIN + 10) // 車胚刀1下定位點
#define DI_AXIS1_HIGH_LOC         (DI_START_PIN + 11) // 車胚刀1上定位點
#define DI_FEED_SENSER2           (DI_START_PIN + 12) // 進料Senser2
#define DI_FEED_SENSER1           (DI_START_PIN + 13) // 進料Senser1 (暫時可不管)
#define DI_FEEDING_CLIP_BUTTON    (DI_START_PIN + 14) // 補料輪閉合開關
#define DI_START_BUTTON           (DI_START_PIN + 15) // 自動啟動
//
// define DO
#define DO_FEEDING_CLIP           (DO_START_PIN + 0) // 補料輪 ON:夾 OFF:不夾
#define DO_SIZE_FEED_UP           (DO_START_PIN + 1) // 進料輪 ON:上升 OFF:下降
#define DO_FEEDING_WORKING        (DO_START_PIN + 2) // 補料輪 ON:轉動 OFF:不轉動
#define DO_SIZE_FEEDING           (DO_START_PIN + 3) // 進料輪 ON:進料 OFF:不動作
#define DO_DISCHARGE1             (DO_START_PIN + 4) // 出料桿1 ON:出料 OFF:縮入
//#define DO_AXIS1_DOWN             (DO_START_PIN + 5) // 車胚刀1 ON:下降 OFF:不動作
//#define DO_AXIS1_UP               (DO_START_PIN + 6) // 車胚刀1 ON:上升 OFF:不動作
#define DO_AXIS1_DOWN             (DO_START_PIN + 6) // 車胚刀1 ON:下降 OFF:不動作
#define DO_AXIS1_UP               (DO_START_PIN + 5) // 車胚刀1 ON:上升 OFF:不動作
#define DO_DISCHARGE2             (DO_START_PIN + 7) // 出料桿2 ON:出料 OFF:縮入
#define DO_AXIS2_DOWN             (DO_START_PIN + 8) // 車胚刀2 ON:下降 OFF:不動作
#define DO_AXIS2_UP               (DO_START_PIN + 9) // 車胚刀2 ON:上升 OFF:不動作
//
// define structure
typedef struct MachineStatus
{
	unsigned char bytWriteTimes;
	unsigned char bytLoc;
	unsigned char bytStatus;
}st_MachineStatus;
//
// define function
void setup(void);
void loop(void);
void LoopGetDIStatus(void);
void ProcessAutoMode(bool SkipFeeding = false);
void ProcessSingleMode(void);
void ProcessManualMode(void);
void ProcessFeeding(void); // 補料
bool ProcessSizeFeeding(void); // 進料
bool ProcessCut(int iAxisNum); // 車胚
bool ProcessAxisUp(int iAxisNum, bool bDisChargeUP = false);
bool ProcessAxisDown(int iAxisNum);
void TimeDelay(unsigned long mSec);
bool CheckTimeout(unsigned long *STicks, unsigned long TimeoutmSec);
void DO_Output(int iDO, bool bHigh);
int DIPinToArrayInt(int PinNum);
int DOPinToArrayInt(int PinNum);
void ShowMessage(char* msg);
void ReadStatusFromEEPROM(st_MachineStatus *Status);
void WriteStatusToEEPROM(st_MachineStatus *Status);
//
// define variable
bool DI_Status[DI_NUMBER];
bool DO_Status[DO_NUMBER];
bool bStart = false;
bool bEnforceStop = false;
unsigned long loopCnt = 0;
unsigned long G_SizeFeedingDownTick = 0;
st_MachineStatus myStatus;
//
void setup()
{
	//start serial connection
	Serial.begin(9600);
	//
	//configure pin2 as an input and enable the internal pull-up resistor
	int i = 0;
	char sTmp[200];
	
	memset(sTmp, 0, sizeof(sTmp));
	
	#if defined (__EEPROM_SAVE_STATUS__)
	ReadStatusFromEEPROM(&myStatus);
	bStart = (myStatus.bytStatus & 0x01 > 0);
	
	sprintf(sTmp, "Status-> WriteCnt:[%d], SaveStatus:[0x%X], Loc:[%d].", myStatus.bytWriteTimes, myStatus.bytStatus, myStatus.bytLoc);
	ShowMessage(sTmp);
	sprintf(sTmp, "bStart:[%d], (myStatus.bytStatus & 0x01 > 0) ? [%d]", bStart, (myStatus.bytStatus & 0x01 > 0));
	ShowMessage(sTmp);
	#endif
	
	for (i = 0; i < DI_NUMBER; i++)
		pinMode(DI_START_PIN + i, INPUT_PULLUP);
  
	for (i = 0; i < DO_NUMBER; i++)
	{
		pinMode(DO_START_PIN + i, OUTPUT);
		digitalWrite(DO_START_PIN + i, HIGH);
	}
	
	ShowMessage("Setup End");
	//
}

void loop()
{
	int iMode = 0;
	
	#if defined (__YUNGSHIN_TESTING__)
	bStart = true;
	#endif
	
	LoopGetDIStatus();
	
	if (bStart == false)
	{
		if (DI_Status[DIPinToArrayInt(DI_FEEDING_WORKING_BUTTON)] && DI_Status[DIPinToArrayInt(DI_FEEDING_CLIP_BUTTON)])
			DO_Output(DO_FEEDING_WORKING, true);
		else
		{
			DO_Output(DO_FEEDING_WORKING, false);
			
			if (DI_Status[DIPinToArrayInt(DI_FEEDING_WORKING_BUTTON)])
			{
				bStart = true;
				ProcessAutoMode(true);
				bStart = false;
			}
		}
		
		return;
	}
	
	loopCnt++;
	loopCnt %= 0xFFFFFFFE;
	ProcessAutoMode();
}

void LoopGetDIStatus()
{
	int i = 0;
	bool PreDIStatus;
	char sTmp[100];

	memset(sTmp, 0, sizeof(sTmp));
	
	// return 1 is LOW, 0 is HIGH, so use not to reserve
	for (i = 0; i < DI_NUMBER; i++)
	{
		PreDIStatus = DI_Status[i];
		DI_Status[i] = !digitalRead(DI_START_PIN + i);
		
		if (PreDIStatus != DI_Status[i])
		{
			sprintf(sTmp, "DI:[%d] change to [%d].", i, DI_Status[i]);
			//ShowMessage(sTmp);
		}
		
		delay(1);
	}
	
	if (DI_Status[DIPinToArrayInt(DI_START_BUTTON)])
	{
		if (!bStart)
		{
			myStatus.bytStatus |= 0x01;
			WriteStatusToEEPROM(&myStatus);
		}
		
		bStart = true;
	}
	
	if (DI_Status[DIPinToArrayInt(DI_STOP_BUTTON)] || DI_Status[DIPinToArrayInt(DI_ENFORCE_STOP_BUTTON)])
	{
		if (bStart)
		{
			myStatus.bytStatus &= 0xFE;
			WriteStatusToEEPROM(&myStatus);
		}
		
		bEnforceStop = DI_Status[DIPinToArrayInt(DI_ENFORCE_STOP_BUTTON)];
		bStart = false;
	}
}

void ProcessFeeding(void)
{
	unsigned long STicks = 0;
	
	ShowMessage("Feeding Working Start!.");
	
	DO_Output(DO_FEEDING_CLIP, true);
	DO_Output(DO_SIZE_FEED_UP, true);
	STicks = millis();
	
	while (DI_Status[DIPinToArrayInt(DI_FEEDING_CLIP_BUTTON)] == false)
	{
		LoopGetDIStatus();
		
		if (CheckTimeout(&STicks, 1000) || bEnforceStop)
			break;
		
		delay(1);
	}
	
	STicks = millis();
	LoopGetDIStatus();
	DO_Output(DO_FEEDING_WORKING, true);
	
	while (DI_Status[DIPinToArrayInt(DI_FEED_SENSER2)] == false)
	{
		LoopGetDIStatus();
		
		if (CheckTimeout(&STicks, FEEDING_INTERVAL) || bEnforceStop || !bStart)
			break;
		
		delay(1);
	}
	
	ShowMessage("Feeding Working End!.");
	
	DO_Output(DO_FEEDING_WORKING, false);
	DO_Output(DO_SIZE_FEED_UP, false);
	DO_Output(DO_FEEDING_CLIP, false);
}

bool ProcessSizeFeeding(void)
{
	unsigned long STicks = 0;
	bool bRtn = false;
	
	ShowMessage("ProcessSizeFeeding Start!");
	
	LoopGetDIStatus();
	DO_Output(DO_SIZE_FEEDING, true);
	STicks = millis();
	// 
	while(DI_Status[DIPinToArrayInt(DI_SIZE_FEED_HIGH_LOC)] == false)
	{
		LoopGetDIStatus();
		
		//if (CheckTimeout(&STicks, 5000) || bEnforceStop)
		if (bEnforceStop)
		{
			bRtn = false;
			goto FUNCEXIT;
		}
		
		delay(1);
	}
	
	//DO_Output(DO_SIZE_FEEDING, false);
	G_SizeFeedingDownTick = millis();
	//
	//LoopGetDIStatus();
	//STicks = millis();
	//
	//while(DI_Status[DIPinToArrayInt(DI_SIZE_FEED_LOW_LOC)] == false)
	//{
	//	LoopGetDIStatus();
	//	
	//	//if (CheckTimeout(&STicks, 2000) || bEnforceStop)
	//	if (bEnforceStop)
	//	{
	//		bRtn = false;
	//		goto FUNCEXIT;
	//	}
	//	
	//	delay(1);
	//}
	
	bRtn = true;
	
FUNCEXIT:
	
	#if defined (__YUNGSHIN_TESTING__)
	bRtn = true; // 測試用，正式時要MARK
	#endif
	
	//DO_Output(DO_SIZE_FEEDING, false);
	ShowMessage("ProcessSizeFeeding End!");
	return (bRtn);
}

bool ProcessAxisUp(int iAxisNum, bool bDisChargeUP)
{
	bool bRtn = false;
	bool bGetNewTime1 = false;
	bool bGetNewTime2 = false;
	unsigned long l_DisCharge1Tick = millis();
	unsigned long l_DisCharge2Tick = millis();
	char sTmp[200];
	
	memset(sTmp, 0, sizeof(sTmp));
	sprintf(sTmp, "ProcessAxisUp Start!, bDisChargeUP:[%d]", bDisChargeUP);
	ShowMessage(sTmp);
	
	while (true)
	{
		LoopGetDIStatus();
		
		if (DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == false)
		{
			if (DO_Status[DOPinToArrayInt(DO_AXIS1_UP)] == false)
			{
				DO_Output(DO_AXIS1_UP, true);
				
				if (bDisChargeUP && bGetNewTime1 == false)
				{
					l_DisCharge1Tick = millis();
					bGetNewTime1 = true;
				}
			}
		}
		
		if (iAxisNum == 2)
		{
			if (DI_Status[DIPinToArrayInt(DI_AXIS2_HIGH_LOC)] == false)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_UP)] == false)
				{
					DO_Output(DO_AXIS2_UP, true);
					
					if (bDisChargeUP && bGetNewTime2 == false)
					{
						l_DisCharge2Tick = millis();
						bGetNewTime2 = true;
					}
				}
			}
		}
		
		if (bGetNewTime1)
		{
			if (CheckTimeout(&l_DisCharge1Tick, DISCHARGE_UP_INTERVAL) == true)
				DO_Output(DO_DISCHARGE1, false);
		}
		
		if (iAxisNum == 1)
		{
			if (DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_UP)] == true)
					DO_Output(DO_AXIS1_UP, false);
				
				bRtn = true;
				break;
			}
		}
		else if (iAxisNum == 2)
		{
			if (bGetNewTime2)
			{
				if (CheckTimeout(&l_DisCharge2Tick, DISCHARGE_UP_INTERVAL) == true)
					DO_Output(DO_DISCHARGE2, false);
			}
			
			if (DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == true && DI_Status[DIPinToArrayInt(DI_AXIS2_HIGH_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_UP)] == true)
					DO_Output(DO_AXIS1_UP, false);
				
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_UP)] == true)
					DO_Output(DO_AXIS2_UP, false);
				
				bRtn = true;
				break;
			}
			else if (DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_UP)] == true)
					DO_Output(DO_AXIS1_UP, false);
			}
			else if (DI_Status[DIPinToArrayInt(DI_AXIS2_HIGH_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_UP)] == true)
					DO_Output(DO_AXIS2_UP, false);
			}
		}
		
		if (bEnforceStop)
			break;
		
		#if defined (__YUNGSHIN_TESTING__)
		if (CheckTimeout(&l_DisCharge2Tick, 3000) == true)
		{
			bRtn = true;
			break;
		}
		#endif
		
		delay(1);
	}
	
	DO_Output(DO_AXIS1_UP, false);
	DO_Output(DO_AXIS2_UP, false);
	
	if (bDisChargeUP)
	{
		if (DO_Status[DOPinToArrayInt(DO_DISCHARGE1)] == true || DO_Status[DOPinToArrayInt(DO_DISCHARGE2)] == true)
		{
			DO_Output(DO_DISCHARGE1, false);
			DO_Output(DO_DISCHARGE2, false);
			delay(50);
		}
	}
	
	ShowMessage("ProcessAxisUp End!");
	return(bRtn);
}

bool ProcessAxisDown(int iAxisNum)
{
	bool bRtn = false;
	#if defined (__YUNGSHIN_TESTING__)
	unsigned long l_ExitLoopTick = millis();
	#endif
	
	//DO_Output(DO_AXIS1_UP, false);
	//DO_Output(DO_AXIS2_UP, false);
	
	ShowMessage("ProcessAxisDown Start!");
	
	while (true)
	{
		LoopGetDIStatus();
		
		// 車胚軸承下降
		if (DI_Status[DIPinToArrayInt(DI_AXIS1_LOW_LOC)] == false)
		{
			if (DO_Status[DOPinToArrayInt(DO_AXIS1_DOWN)] == false)
				DO_Output(DO_AXIS1_DOWN, true);
		}
		
		if (iAxisNum == 2)
		{
			if (DI_Status[DIPinToArrayInt(DI_AXIS2_LOW_LOC)] == false)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_DOWN)] == false)
					DO_Output(DO_AXIS2_DOWN, true);
			}
		}
		//
		
		if (iAxisNum == 1)
		{
			if (DI_Status[DIPinToArrayInt(DI_AXIS1_LOW_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_DOWN)] == true)
					DO_Output(DO_AXIS1_DOWN, false);
				
				bRtn = true;
				break;
			}
		}
		else if (iAxisNum == 2)
		{
			if (DI_Status[DIPinToArrayInt(DI_AXIS1_LOW_LOC)] == true && DI_Status[DIPinToArrayInt(DI_AXIS2_LOW_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_DOWN)] == true)
					DO_Output(DO_AXIS1_DOWN, false);
				
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_DOWN)] == true)
					DO_Output(DO_AXIS2_DOWN, false);
				
				bRtn = true;
				break;
			}
			else if (DI_Status[DIPinToArrayInt(DI_AXIS1_LOW_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS1_DOWN)] == true)
					DO_Output(DO_AXIS1_DOWN, false);
			}
			else if (DI_Status[DIPinToArrayInt(DI_AXIS2_LOW_LOC)] == true)
			{
				if (DO_Status[DOPinToArrayInt(DO_AXIS2_DOWN)] == true)
					DO_Output(DO_AXIS2_DOWN, false);
			}
		}
		
		if (CheckTimeout(&G_SizeFeedingDownTick, 150))
			DO_Output(DO_SIZE_FEEDING, false);
		
		if (bEnforceStop)
			break;
		
		#if defined (__YUNGSHIN_TESTING__)
		if (CheckTimeout(&l_ExitLoopTick, 3000) == true)
		{
			bRtn = true;
			break;
		}
		#endif
		
		delay(1);
	}
	
	DO_Output(DO_AXIS1_DOWN, false);
	DO_Output(DO_AXIS2_DOWN, false);
	DO_Output(DO_SIZE_FEEDING, false);
	
	ShowMessage("ProcessAxisDown End!");
	return(bRtn);
}

bool ProcessCut(int iAxisNum)
{
	LoopGetDIStatus();
	
	// Axis UP
	ProcessAxisUp(iAxisNum);
	//
	
	if (bEnforceStop)
		goto FUNCEXIT;
	
	if (CheckTimeout(&G_SizeFeedingDownTick, 200))
		DO_Output(DO_SIZE_FEEDING, false);
	
	LoopGetDIStatus();
	
	// 出料
	if (DO_Status[DOPinToArrayInt(DO_DISCHARGE1)] == false)
		DO_Output(DO_DISCHARGE1, true);
	
	if (iAxisNum == 2)
	{
		if (DO_Status[DOPinToArrayInt(DO_DISCHARGE2)] == false)
			DO_Output(DO_DISCHARGE2, true);
	}
	//
	
	LoopGetDIStatus();
	
	if (bEnforceStop)
		goto FUNCEXIT;
	
	// Axis Down
	ProcessAxisDown(iAxisNum);
	//
	
	LoopGetDIStatus();
	
	if (bEnforceStop)
		goto FUNCEXIT;
	
	// Axis UP
	ProcessAxisUp(iAxisNum, true);
	//
	
	LoopGetDIStatus();
	return (true);
	
FUNCEXIT:
	
	LoopGetDIStatus();
	return (false);
}

void ProcessAutoMode(bool SkipFeeding)
{
	unsigned long STicks = 0;
	int iTimeCnt = 0;
	int iAxisNum = 0;
	char sTmp[100];
	
	memset(sTmp, 0, sizeof(sTmp));
	LoopGetDIStatus();
	
	if (bStart == false)
		goto FUNCEXIT;
	
	// 判斷單程或雙程
	if (DI_Status[DIPinToArrayInt(DI_MANUAL_MODE)]) // 手動模式
	{
		bStart = false;
		goto FUNCEXIT;
	}
	else if (DI_Status[DIPinToArrayInt(DI_SINGLE_MODE)]) // 單程模式
		iAxisNum = 1;
	else // 自動模式
		iAxisNum = 2;
	//
	
	sprintf(sTmp, "Curcle Start! Count:[%ld].", loopCnt);
	ShowMessage(sTmp);
	LoopGetDIStatus();
	
	if (SkipFeeding == false)
	{
		#if not (defined (__YUNGSHIN_TESTING__))
		// 檢查所有東西是否都在定位
		if (DI_Status[DIPinToArrayInt(DI_SIZE_FEED_LOW_LOC)] == false || DI_Status[DIPinToArrayInt(DI_AXIS2_HIGH_LOC)] == false || DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == false)
		{
			#if defined (__EEPROM_SAVE_STATUS__)
			ProcessAxisUp(iAxisNum, true);
			delay(50);
			#endif
			
			if (DI_Status[DIPinToArrayInt(DI_SIZE_FEED_LOW_LOC)] == false || DI_Status[DIPinToArrayInt(DI_AXIS2_HIGH_LOC)] == false || DI_Status[DIPinToArrayInt(DI_AXIS1_HIGH_LOC)] == false)
			{
				// 原來寫法，有狀態不正確時暫停啟動 //
				bStart = false;
				ShowMessage("Something doesn't on set!");
				goto FUNCEXIT;
			}
		}
		//
		#endif
		// 補料
		for (iTimeCnt = 0; iTimeCnt < RETRY; iTimeCnt++)
		{
			#if not (defined (__YUNGSHIN_TESTING__))
			if (DI_Status[DIPinToArrayInt(DI_FEED_SENSER2)] || bStart == false)
				break;
			#endif
			
			ProcessFeeding();
			TimeDelay(1000);
			
			#if (defined (__YUNGSHIN_TESTING__))
			break;
			#endif
		}
		
		#if not (defined (__YUNGSHIN_TESTING__))
		if (DI_Status[DIPinToArrayInt(DI_FEED_SENSER2)] == false)
		{
			ShowMessage("No DI_FEED_SENSER2 Signal!");
			bStart = false;
			goto FUNCEXIT;
		}
		#endif
	}
	//
	
	LoopGetDIStatus();
	
	// 進料
	if (ProcessSizeFeeding() == false)
	{
		ShowMessage("Error in ProcessSizeFeeding()!");
		goto FUNCEXIT;
	}
	//
	
	LoopGetDIStatus();
	
	// 車胚
	if (ProcessCut(iAxisNum) == false)
	{
		ShowMessage("Error in ProcessCut()!");
		goto FUNCEXIT;
	}
	//
	
	sprintf(sTmp, "Curcle End! Count:[%ld].", loopCnt);
	ShowMessage(sTmp);
	//delay(100);
	
FUNCEXIT:
	
	if (bEnforceStop)
	{
		for (int i = 0; i < DO_NUMBER; i++)
			DO_Output(DO_START_PIN + i, false);
		
		bStart = false;
	}
	
	while (DI_Status[DIPinToArrayInt(DI_SIZE_FEED_LOW_LOC)] == false)
	{
		DO_Output(DO_SIZE_FEEDING, false);
		LoopGetDIStatus();
		
		if (bEnforceStop)
			break;
		
		delay(1);
	}
	
	bEnforceStop = false;
}

void TimeDelay(unsigned long mSec)
{
	unsigned long StartTick = millis();
	
	while (CheckTimeout(&StartTick, mSec) == false)
	{
		LoopGetDIStatus();
		
		if (DI_Status[DIPinToArrayInt(DI_STOP_BUTTON)] == true)
			bStart = false;
		
		DI_Status[DIPinToArrayInt(DI_ENFORCE_STOP_BUTTON)] = !digitalRead(DI_ENFORCE_STOP_BUTTON);
		
		if (DI_Status[DIPinToArrayInt(DI_ENFORCE_STOP_BUTTON)])
		{
			bEnforceStop = true;
			break;
		}
		
		delay(1);
	}
}

bool CheckTimeout(unsigned long *STicks, unsigned long TimeoutmSec)
{
	bool bRtn = false;
	unsigned long NowTicks = millis();
	
	if (NowTicks < *STicks)
		*STicks = NowTicks;
	else if (NowTicks - *STicks >= TimeoutmSec)
		bRtn = true;
	
	return (bRtn);
}

void DO_Output(int iDO, bool bHigh)
{
	char sTmp[100];
	
	memset(sTmp, 0, sizeof(sTmp));
	
	if (DO_Status[DOPinToArrayInt(iDO)] != bHigh)
	{
		if (bHigh)
			digitalWrite(iDO, LOW);
		else
			digitalWrite(iDO, HIGH);
		
		DO_Status[DOPinToArrayInt(iDO)] = bHigh;
		sprintf(sTmp, "Send [%d] status [%d]", iDO, bHigh);
		//ShowMessage(sTmp);
		delay(1);
	}
}

int DIPinToArrayInt(int PinNum)
{
	return (PinNum - DI_START_PIN);
}

int DOPinToArrayInt(int PinNum)
{
	return (PinNum - DO_START_PIN);
}

void ShowMessage(char* msg)
{
	Serial.print(msg);
	Serial.println();
}

void ReadStatusFromEEPROM(st_MachineStatus *Status)
{
	int i = 0;
	int iEEPROM_Address = 0;
	
	#if defined (__EEPROM_SAVE_STATUS__)
	for (i  = 0; i < STATUS_COUNT; i++)
	{
		EEPROM.get(iEEPROM_Address + (sizeof(st_MachineStatus) * i), *Status);
		
		if (Status->bytWriteTimes < 10)
			break;
	}
	#endif
}

void WriteStatusToEEPROM(st_MachineStatus *Status)
{
	int i = 0;
	int iEEPROM_Address = sizeof(st_MachineStatus) * Status->bytLoc;
	unsigned char bytNewLoc = 0;
	char sTmp[200];
	
	bool bResetNewLoc = (Status->bytWriteTimes >= 9);
	
	#if defined (__EEPROM_SAVE_STATUS__)
	Status->bytWriteTimes++;
	EEPROM.put(iEEPROM_Address, *Status);
	
	sprintf(sTmp, "myStatus -> WriteCnt:[%d], LastStatus:[%d], Loc:[%d]", Status->bytWriteTimes, Status->bytStatus, Status->bytLoc);
	ShowMessage(sTmp);
	
	if (bResetNewLoc)
	{
		bytNewLoc = (Status->bytLoc + 1) % STATUS_COUNT;
		Status->bytWriteTimes = 0;
		Status->bytLoc = bytNewLoc;
		EEPROM.put(sizeof(st_MachineStatus) * bytNewLoc, Status);
	}
	#endif
}