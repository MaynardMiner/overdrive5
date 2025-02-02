#if defined (LINUX)
#	include <dlfcn.h>	//dyopen, dlsym, dlclose
#	include <unistd.h>	//sleep
#	include <memory.h>
#	include <getopt.h>
#	define ADL_DLL_NAME	"libatiadlxx.so"
#else
#	include <windows.h>
#	include "wgetopt.h"
#	define ADL_DLL_NAME	"atiadlxx.dll"
#endif
#include <cmath>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "ADL/include/adl_sdk.h"
#include "cadl.hpp"
#include <iostream>
#include <fstream>
#include <vector>

#define ODPLVL_SIZE	20

using namespace std;

std::vector<std::string> v;

class CADLOverdrive5: public CADL
{
	static const char YesNoStr[2][4];
	int Verbose;
	
	const char* YesNo(int boolean)
	{
		return YesNoStr[boolean > 0];
	}

	int FindAdapter(void)
	{
		int iAdapter = 0;

		for (int i = 0; i < mNumberAdapters; i++)
		{
			if (mpAdapterInfo[i].iPresent)
			{
				int index = mpAdapterInfo[i].iAdapterIndex;
				int adapterID;
				//PrintAdapterInfo(mpAdapterInfo+i);
				if (ADL_OK == ADL_Adapter_ID_Get(index, &adapterID))
				{
					//printf("AdapterID = %x\n", AdapterID);
					// if it is an ATI/AMD adapter
					if (adapterID)
					{
						int active;
						if (ADL_OK == ADL_Adapter_Active_Get(index, &active))
						{
							if (active)
							{
								iAdapter = index;
								break;
							}
						}
					}
				}
			}
		}

		return iAdapter;
	}

	int FindTController(const int iAdapter)
	{
		for (int i = 0; ; i++)
		{
			ADLThermalControllerInfo tcinfo;

			tcinfo.iSize = sizeof(tcinfo);

			if (ADL_OK == ADL_Overdrive5_ThermalDevices_Enum(iAdapter, i, &tcinfo))
			{
				if (tcinfo.iThermalDomain) // if GPU domain
					return i;
				//tcinfo.iDomainIndex;
				//tcinfo.iFlags;
			}
		}
		
		return 0;
	}

	void PrintAdapterInfo(const LPAdapterInfo pAdapterInfo)
	{
		printf("\nAdapter %d:\n"
			"\tBus Number      = %d\n"
			"\tDevice Number   = %d\n"
			"\tFunction Number = %d\n"
			"\tVendor ID       = %d\n"
			"\tAdapter name    = %s\n"
			"\tPresent         = %s\n"
			,
			pAdapterInfo->iAdapterIndex,
			pAdapterInfo->iBusNumber,
			pAdapterInfo->iDeviceNumber,
			pAdapterInfo->iFunctionNumber,
			pAdapterInfo->iVendorID,
			pAdapterInfo->strAdapterName,
			YesNo(pAdapterInfo->iPresent));

#		if !defined (LINUX)
		printf("\tOS DisplayIndex = %d\n",
			pAdapterInfo->iOSDisplayIndex);
#		endif
	}

	void PrintCurrentActivity(const int iAdapter)
	{
		ADLPMActivity   act;

		act.iSize = sizeof(act);

		Sleep(500); // sleep to calm down the H/W

		if (ADL_OK == ADL_Overdrive5_CurrentActivity_Get(iAdapter, &act))
		{
			printf("Current Activity:\n"
				"\tEngine Clock       = %.2f MHz\n"
				"\tMemory Clock       = %.2f MHz\n"
				"\tVddc               = %.3f V\n"
				"\tActivity           = %d%%\n"
				"\tPerformance Level  = %d\n"
				"\tBus Speed          = %.2f GT/s\n"
				"\tBus Lanes          = %d/%d\n"
				,
				act.iEngineClock/100.f,
				act.iMemoryClock/100.f,
				act.iVddc/1000.f,
				act.iActivityPercent,
				act.iCurrentPerformanceLevel,
				act.iCurrentBusSpeed/1000.f,
				act.iCurrentBusLanes,
				act.iMaximumBusLanes);
		}
		else fprintf(stderr, "Error: cannot read Current Activity.\n");
	}

	void PrintPerformanceLevels(const int iAdapter, const int def)
	{
		ADLODParameters odp;

		odp.iSize = sizeof(odp);

		if (ADL_OK == ADL_Overdrive5_ODParameters_Get(iAdapter, &odp))
		{
			ADLODPerformanceLevels odlvl[ODPLVL_SIZE];

			printf("Overdrive parameters:\n"
				"\tNumber of Performance Levels = %d\n"
				"\tActivity Reporting Supported = %s\n"
				"\tDiscrete Performance Levels  = %s\n"
				"\tEngine Clock = %7.2f - %7.2f step %6.2f MHz\n"
				"\tMemory Clock = %7.2f - %7.2f step %6.2f MHz\n"
				"\tVddc         = %7.3f - %7.3f step %7.3f V\n"
				,
				odp.iNumberOfPerformanceLevels,
				YesNo(odp.iActivityReportingSupported),
				YesNo(odp.iDiscretePerformanceLevels),
				odp.sEngineClock.iMin/100.f,
				odp.sEngineClock.iMax/100.f,
				odp.sEngineClock.iStep/100.f,
				odp.sMemoryClock.iMin/100.f,
				odp.sMemoryClock.iMax/100.f,
				odp.sMemoryClock.iStep/100.f,
				odp.sVddc.iMin/1000.f,
				odp.sVddc.iMax/1000.f,
				odp.sVddc.iStep/1000.f);

			odlvl[0].iSize = sizeof(odlvl[0]) + 
				sizeof(ADLODPerformanceLevel)*(odp.iNumberOfPerformanceLevels-1);

			if (ADL_OK == ADL_Overdrive5_ODPerformanceLevels_Get(iAdapter, def, odlvl))
			{
				for (int i = 0; i < odp.iNumberOfPerformanceLevels; i++)
				{
					printf("Performance level %d:\n"
						"\tEngine Clock = %.2f MHz\n"
						"\tMemory Clock = %.2f MHz\n"
						"\tVddc         = %.3f V\n"
						,
						i,
						odlvl[0].aLevels[i].iEngineClock/100.f,
						odlvl[0].aLevels[i].iMemoryClock/100.f,
						odlvl[0].aLevels[i].iVddc/1000.f);
				}
			}
			else fprintf(stderr, "Error: cannot read Performance Levels.\n");

		}
		else fprintf(stderr, "Error: cannot read Overdrive Parameters.\n");
	}

	void AddPerformanceLevel(ADLODPerformanceLevels *odlvl, const char* str)
	{
		float engclk;
		float memclk;
		float vddc;

		if (sscanf(str, "%f/%f/%f", &engclk, &memclk, &vddc) == 3)
		{
			int i;

			if (odlvl->iSize)
			{
				odlvl->iSize += sizeof(ADLODPerformanceLevel);
				i = (odlvl->iSize - sizeof(*odlvl))/sizeof(ADLODPerformanceLevel);
			}
			else
			{
				odlvl->iSize = sizeof(*odlvl);
				i = 0;
			}

			odlvl->aLevels[i].iEngineClock = (int) (engclk * 100);
			odlvl->aLevels[i].iMemoryClock = (int) (memclk * 100);
			odlvl->aLevels[i].iVddc = (int) (vddc * 1000);
		}
		else fprintf(stderr, "Error: cannot parse Performance Level %s\n", str);
	}

	void SetPerformanceLevels(const int iAdapter, ADLODPerformanceLevels *odplvl)
	{
		ADLODParameters odp;

		odp.iSize = sizeof(odp);

		if (ADL_OK == ADL_Overdrive5_ODParameters_Get(iAdapter, &odp))
		{
			if (ADL_OK != ADL_Overdrive5_ODPerformanceLevels_Set(iAdapter, odplvl))
				fprintf(stderr, "Error: cannot set Performance Levels.\n");
		}
		else fprintf(stderr, "Error: cannot read Overdrive Parameters.\n");
	}

	void SetHighestPerformanceLevel(const int iAdapter, const char* str)
	{
		float engclk;
		float memclk;
		float vddc;

		if (sscanf(str, "%f/%f/%f", &engclk, &memclk, &vddc) == 3)
		{
			ADLODParameters odp;

			odp.iSize = sizeof(odp);

			if (ADL_OK == ADL_Overdrive5_ODParameters_Get(iAdapter, &odp))
			{
				ADLODPerformanceLevels odplvl[ODPLVL_SIZE];

				odplvl[0].iSize = sizeof(odplvl[0]) + sizeof(ADLODPerformanceLevel)*(odp.iNumberOfPerformanceLevels-1);

				if (ADL_OK == ADL_Overdrive5_ODPerformanceLevels_Get(iAdapter, 0, odplvl))
				{
					int i = odp.iNumberOfPerformanceLevels-1;

					odplvl->aLevels[i].iEngineClock = (int) (engclk * 100);
					odplvl->aLevels[i].iMemoryClock = (int) (memclk * 100);
					odplvl->aLevels[i].iVddc = (int) (vddc * 1000);

					if (odplvl->aLevels[i].iEngineClock < odp.sEngineClock.iMin
					||  odplvl->aLevels[i].iEngineClock > odp.sEngineClock.iMax)
						fprintf(stderr, "Warning: Engine Clock %.2f is out of range.\n", engclk);

					if (odplvl->aLevels[i].iMemoryClock < odp.sMemoryClock.iMin
					||  odplvl->aLevels[i].iMemoryClock > odp.sMemoryClock.iMax)
						fprintf(stderr, "Warning: Memory Clock %.2f is out of range.\n", memclk);

					if (odplvl->aLevels[i].iVddc < odp.sVddc.iMin
					||  odplvl->aLevels[i].iVddc > odp.sVddc.iMax)
						fprintf(stderr, "Warning: Vddc %.3f is out of range.\n", vddc);

					if (ADL_OK != ADL_Overdrive5_ODPerformanceLevels_Set(iAdapter, odplvl))
						fprintf(stderr, "Error: cannot set Performance Levels.\n");
				}
				else fprintf(stderr, "Error: cannot read Performance Levels.\n");
			}
			else fprintf(stderr, "Error: cannot read Overdrive Parameters.\n");
		}
		else fprintf(stderr, "Error: cannot parse Performance Level %s\n", str);
	}

	void SetDefaultPerformanceLevels(const int iAdapter)
	{
		ADLODParameters odp;

		odp.iSize = sizeof(odp);

		if (ADL_OK == ADL_Overdrive5_ODParameters_Get(iAdapter, &odp))
		{
			ADLODPerformanceLevels odplvl[ODPLVL_SIZE];

			odplvl[0].iSize = sizeof(odplvl[0]) + 
				sizeof(ADLODPerformanceLevel)*(odp.iNumberOfPerformanceLevels-1);

			if (ADL_OK == ADL_Overdrive5_ODPerformanceLevels_Get(iAdapter, 1, odplvl))
			{
				if (ADL_OK != ADL_Overdrive5_ODPerformanceLevels_Set(iAdapter, odplvl))
					fprintf(stderr, "Error: cannot set Performance Levels.\n");
			}
			else fprintf(stderr, "Error: cannot read Performance Levels.\n");
		}
		else fprintf(stderr, "Error: cannot read Overdrive Parameters.\n");
	}

	void PrintPowerControl(const int iAdapter)
	{
		int supported;

		printf("PowerControl:\n");
		if (ADL_OK == ADL_Overdrive5_PowerControl_Caps(iAdapter, &supported))
		{
			printf("\tSupported         = %s\n", YesNo(supported));

			if (supported)
			{
				ADLPowerControlInfo pcinfo;
				int                 val;
				int                 def;
				
				if (ADL_OK == ADL_Overdrive5_PowerControlInfo_Get(iAdapter, &pcinfo))
				{
					printf("\tRange         = %d - %d step %d\n",
						pcinfo.iMinValue, pcinfo.iMaxValue, pcinfo.iStepValue);
				}
				if (ADL_OK == ADL_Overdrive5_PowerControl_Get(iAdapter, &val, &def))
				{
					printf("\tValue         = %d\n"
						   "\tDefault       = %d\n", val, def);
				}
			}
		}
		else fprintf(stderr, "Error: cannot get Power Control capabilities.\n");
	}

	void SetPowerControl(const int iAdapter, int iPower)
	{
		int supported;

		if (ADL_OK == ADL_Overdrive5_PowerControl_Caps(iAdapter, &supported))
		{
			if (supported)
			{
				ADLPowerControlInfo pcinfo;

				if (ADL_OK == ADL_Overdrive5_PowerControlInfo_Get(iAdapter, &pcinfo))
				{
					if (iPower < pcinfo.iMinValue || iPower > pcinfo.iMaxValue)
					{
						fprintf(stderr, "Error: Power Control %d is out of range.\n", iPower);
						return;
					}

					if (ADL_OK != ADL_Overdrive5_PowerControl_Set(iAdapter, iPower))
						fprintf(stderr, "Error: cannot set Power Control to %d.\n", iPower);
				}
				else fprintf(stderr, "Error: cannot get Power Control info.\n");
			}
			else fprintf(stderr, "Error: Power Control is not supported.\n");
		}
	}

	void PrintInfo(void)
	{
		for (int i = 0; i < mNumberAdapters; i++)
		{
			ADLThermalControllerInfo tcinfo;
			tcinfo.iSize = sizeof(tcinfo);
			int iAdapter = mpAdapterInfo[i].iAdapterIndex;
			int active;
			int adapterID;
			if (ADL_OK == ADL_Overdrive5_ThermalDevices_Enum(iAdapter, i, &tcinfo))
			{
			PrintAdapterInfo(mpAdapterInfo+i);
			if (ADL_OK == ADL_Adapter_Active_Get(iAdapter, &active))
				printf("\tActive          = %s\n", YesNo(active));
			if (ADL_OK == ADL_Adapter_ID_Get(iAdapter, &adapterID))
				printf("\tAdapter ID      = %08X\n", adapterID);

			PrintCurrentActivity(iAdapter);
			PrintPerformanceLevels(iAdapter, 0);
			}
		}
	}

	void PrintFanSpeed(const int iAdapter, const int iTController, int seconds)
	{
		ADLFanSpeedInfo  fan;
		ADLFanSpeedValue speed[2];

		fan.iSize    = sizeof(fan);

		speed[0].iSize  = sizeof(speed[0]);
		speed[0].iSpeedType = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;
		speed[0].iFanSpeed = 0;
		speed[0].iFlags = 0;

		if (ADL_OK != ADL_Overdrive5_FanSpeed_Get(iAdapter, iTController, speed))
			fprintf(stderr, "Error: cannot read Fan Speed.\n");

		printf("GPU %d Fan=%d\n", iAdapter,speed[0].iFanSpeed);
		
		while (seconds-->0)
		{
			Sleep(1000);

			if (ADL_OK != ADL_Overdrive5_FanSpeed_Get(iAdapter, iTController, speed))
			{
				fprintf(stderr, "Error: cannot read Fan Speed.\n");
				break;
			}

			printf("GPU %d Fan=%d\n", iAdapter,speed[0].iFanSpeed);
		}
	}

	void SetFanSpeed(const int iAdapter, const int iTController, int iSpeedType, int iSpeed)
	{
		ADLFanSpeedInfo  fan;
		ADLFanSpeedValue speed;

		fan.iSize    = sizeof(fan);
		speed.iSize  = sizeof(speed);

		if (ADL_OK == ADL_Overdrive5_FanSpeedInfo_Get(iAdapter, iTController, &fan))
		{
			if (iSpeedType == ADL_DL_FANCTRL_SPEED_TYPE_PERCENT)
			{
				if (iSpeed < fan.iMinPercent || iSpeed > fan.iMaxPercent)
					fprintf(stderr, "Warning: speed %d%% is out of range.\n", iSpeed);
			}
			else
			{
				if (iSpeed < fan.iMinRPM || iSpeed > fan.iMaxRPM)
					fprintf(stderr, "Warning: speed %d RPM is out of range.\n", iSpeed);
			}

			speed.iSpeedType = iSpeedType;
			speed.iFanSpeed = iSpeed;
			speed.iFlags = 0;//ADL_DL_FANCTRL_FLAG_USER_DEFINED_SPEED;

			if (ADL_OK != ADL_Overdrive5_FanSpeed_Set(iAdapter, iTController, &speed))
				fprintf(stderr, "Error: cannot set Fan Speed to %d.\n", iSpeed);
		}
		else fprintf(stderr, "Error: cannot read Fan Speed Info.\n");
	}

	void SetDefaultFanSpeed(const int iAdapter, const int iTController)
	{
		if (ADL_OK != ADL_Overdrive5_FanSpeedToDefault_Set(iAdapter, iTController))
			fprintf(stderr, "Error: cannot set Fan Speed to default.\n");

		PrintFanSpeed(iAdapter, iTController, 0);
	}

	void PrintTemperature(const int iAdapter, const int iTController, int seconds)
	{
		ADLTemperature temp;

		temp.iSize = sizeof(temp);

		if (ADL_OK == ADL_Overdrive5_Temperature_Get(iAdapter, iTController, &temp))
		{
			printf("GPU %d Temp=%.1f\n", iAdapter,temp.iTemperature/1000.f);

			while (seconds-->0)
			{
				Sleep(1000);

				if (ADL_OK == ADL_Overdrive5_Temperature_Get(iAdapter, iTController, &temp))
					printf("GPU %d Temp=%.1f\n", iAdapter,temp.iTemperature/1000.f);
				else break;
			}
		}
	}

		void* __stdcall ADL_Main_Memory_Alloc ( int iSize )
		{
    	 void* lpBuffer = malloc ( iSize );
    	 return lpBuffer;
		}
		void __stdcall ADL_Main_Memory_Free ( void* lpBuffer )
		{
    	if ( NULL != lpBuffer )
    	{
        free ( lpBuffer );
        lpBuffer = NULL;
    	}
		}

	void GetCurrenPower(const int iAdapter, int seconds)
	{
		
        ADL_CONTEXT_HANDLE context = NULL;
		int ADLWatts = 0;

    	if (ADL_OK == ADL_Adapter_CurrentPower_Get(context, iAdapter, 0, &ADLWatts))
	     {
		  printf("GPU %d Watts=%.1f\n", iAdapter,ADLWatts/256.f);
			while (seconds-->0)
			{
				Sleep(1000);

				if (ADL_OK == ADL_Adapter_CurrentPower_Get(context, iAdapter, 0, &ADLWatts))
					printf("GPU %d Watts=%.1f\n", iAdapter,ADLWatts/256.f);
				else break;
			}
		}
		else printf("Failed to get power\n", iAdapter,ADLWatts/256.f);
	}

#	if defined (LINUX)
	void Sleep(const int ms)
	{
		usleep(ms*1000);
	}
#	endif

public:
	void PrintUsage(const char* name)
	{
		printf("Overdrive5 v1.1 - ATI/AMD ADL OverDrive5 Tool\n");
		printf("Usage: %s [options]\n", name);
		printf("Options:\n"
			"	-h			- help\n"
			"	-l			- list adapters and thermal controllers\n"
			"	-a <n>			- use adapter <n>\n"
			"	-c <n>			- use thermal controller <n>\n"
			"	-f			- get fan speed\n"
			"	-F <n>			- set fan speed to <n> %%\n"
			"	-J <n>			- set fan speed to <n> RPM\n"
			"	-g <n>			- monitor fan speed for <n> seconds\n"
			"	-G			- set default fan speed\n"
			"	-q			- get performance levels\n"
			"	-Q			- get default performance levels\n"
			"	-A			- get current activity\n"
			"	-p <clk/mem/vddc>	- add a performance level\n"
			"	-P			- set the performance levels\n"
			"	-S <clk/mem/vddc>	- set the highest performance level\n"
			"	-D			- set default performance levels\n"
			"	-t			- get the temperature\n"
			"	-T <n>			- monitor the temperature for <n> seconds\n"
			"	-w			- get power control info.\n"
			"	-W <n>			- set power control value\n"
			"	-v <n>			- set verbosity level to <n>\n"
		);
	}

	int Run(int argc, char* argv[])
	{
		Verbose = 1;

		Init(ADL_DLL_NAME);

		if (!IsOK())
		{
#			if !defined (LINUX)
			// check the 32 bit calling application on 64 bit Windows case
			Init("atiadlxy.dll");
#			endif

			if (!IsOK())
			{
				fprintf(stderr, "Cannot initialize ADL! (%d)\n", LastError());
				return -1;
			}
			else fprintf(stderr, "The 32bit application on 64bit OS\n");
		}

		int iAdapter = FindAdapter();
		int iTController = FindTController(iAdapter);
		ADLODPerformanceLevels odplvl[ODPLVL_SIZE];
		char opt;

		odplvl[0].iSize = 0;

		// read command line
		while ((opt = getopt(argc, argv, ":hla:c:yY:fF:J:Gg:qQAp:PS:DtT:wW:v:")) != -1)
		{
			switch(opt) {
			case 'h':
				PrintUsage(argv[0]);
				return 0;
			case 'l':
				PrintInfo();
				break;
			case 'a':
				iAdapter = atoi(optarg);
				iTController = FindTController(iAdapter);
				break;
			case 'c':
				iTController = atoi(optarg);
				break;
			case 'f':
				PrintFanSpeed(iAdapter, iTController, 0);
				break;
			case 'F':
				SetFanSpeed(iAdapter, iTController, ADL_DL_FANCTRL_SPEED_TYPE_PERCENT, atoi(optarg));
				break;
			case 'J':
				SetFanSpeed(iAdapter, iTController, ADL_DL_FANCTRL_SPEED_TYPE_RPM, atoi(optarg));
				break;
			case 'g':
				PrintFanSpeed(iAdapter, iTController, atoi(optarg));
				break;
			case 'G':
				SetDefaultFanSpeed(iAdapter, iTController);
				break;
			case 'q':
				PrintPerformanceLevels(iAdapter, 0);
				break;
			case 'Q':
				PrintPerformanceLevels(iAdapter, 1);
				break;
			case 'A':
				PrintCurrentActivity(iAdapter);
				break;
			case 'p':
				AddPerformanceLevel(odplvl, optarg);
				break;
			case 'P':
				SetPerformanceLevels(iAdapter, odplvl);
				break;
			case 'S':
				SetHighestPerformanceLevel(iAdapter, optarg);
				break;
			case 'D':
				SetDefaultPerformanceLevels(iAdapter);
				break;
			case 't':
				PrintTemperature(iAdapter, iTController, 0);
				break;
			case 'T':
				PrintTemperature(iAdapter, iTController, atoi(optarg));
				break;
			case 'w':
				PrintPowerControl(iAdapter);
				break;
			case 'W':
				SetPowerControl(iAdapter, atoi(optarg));
				break;
			case 'y':
				GetCurrenPower(iAdapter, 0);
				break;
			case 'Y':
				GetCurrenPower(iAdapter, atoi(optarg));
				break;
			case 'v':
				Verbose = atoi(optarg);
				break;
			case ':':
				fprintf(stderr, "Option -%c requires an operand\n", optopt);
				PrintUsage(argv[0]);
				return -1;
			case '?':
				fprintf(stderr, "Unrecognized option: -%c\n", optopt);
				PrintUsage(argv[0]);
				return -1;
			}
		}

		return 0;
	}
};

const char CADLOverdrive5::YesNoStr[2][4] = { "No", "Yes" };

int main (int argc, char* argv[])
{
	CADLOverdrive5 od5;

	return od5.Run(argc, argv);
	return(0);
}
