
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xadcps.h"
#include "xstatus.h"
#include "stdio.h"
#include <unistd.h>
#include "bram.h"


//User input function struct
struct Battery_Info
{
	int norm_volt; //nominal voltage in mV
	int capacity;  //capacity in mA
};

struct Battery_Info prompt_user();

#define XADC_DEVICE_ID 		XPAR_XADCPS_0_DEVICE_ID

//stuff from ADC

#define printf xil_printf /* Small foot-print printf function */

#define XAdcPs_ExternalRawToVoltage(AdcData)\
	((((float)(AdcData))* (1.0f))/65536.0f)

static XAdcPs XAdcInst;      /* XADC driver instance */

//define addresses
const int ADDR_ENABLE = 0;
const int ADDR_NOW_CURRENT = 1;
const int ADDR_SET_CURRENT = 2;
const int ADDR_PID = 3;

int Status;
XAdcPs *XAdcInstPtr = &XAdcInst;
XAdcPs_Config *AdcConfigPtr;


//ADC init function
void setup_ADC()
{
	/*
	 * Initialize the XAdc driver.
	 */
	AdcConfigPtr = XAdcPs_LookupConfig(XADC_DEVICE_ID);
	if (AdcConfigPtr == NULL) {
		Status = XST_FAILURE;
		return;
	}
	XAdcPs_CfgInitialize(XAdcInstPtr, AdcConfigPtr,
			AdcConfigPtr->BaseAddress);

	/*
	 * Self Test the XADC/ADC device
	 */
	Status = XAdcPs_SelfTest(XAdcInstPtr);
}

//write enable state to BRAM
void set_enable(int value)
{
	MYMEM_u(ADDR_ENABLE)=value;
}

//write 'set current' to BRAM
void set_set_current(int value)
{
	MYMEM_u(ADDR_SET_CURRENT)=value;
}

//write 'now current' to BRAM
void set_now_current(int value)
{
	MYMEM_u(ADDR_NOW_CURRENT)=value;
}

//read the PID
int PID_read()
{
	return (int)(MYMEM_u(ADDR_PID));
}

//read the value of one ADC
int read_ADC(int pin)
{
	int channel = 0;
	if (pin == 0)
		channel = 1;
	else if (pin == 1)
		channel = 9;
	else if (pin == 2)
		channel = 6;

	if (channel == 0)
		return -1;

	u32 VccPdroRawData = XAdcPs_GetAdcData(XAdcInstPtr, XADCPS_CH_AUX_MIN+channel);
	float VccPintData = XAdcPs_ExternalRawToVoltage(VccPdroRawData);

	return (int)(VccPintData * 1000 * 3.3f * 2);
}


int measure_battery_voltage()
{
	int a1 = read_ADC(1);
	int a2 = read_ADC(2);
	return a1 - a2;
}

int measure_current()
{
	int a0 = read_ADC(0);
	int a1 = read_ADC(1);

	return a0 - a1; // resistor is 1 ohm so no division with current setup
}

struct Battery_Info get_def_battery()
{
	int norm_volt = 1200;
	int capacity = 2000;

	struct Battery_Info bi;
	bi.norm_volt = norm_volt;
	bi.capacity = capacity;

	return bi;
}


int main()
{
    init_platform();

    print("SmartBat charger 3000 engaged \n\r");

	initMemory();

	xil_printf("BRAM initialization complete.\r\n");

	setup_ADC();


	//write 0 to addresses
	set_enable(0);
	set_now_current(0);
	set_set_current(0);


	//call prompt_user function
	//struct Battery_Info info = prompt_user();
	struct Battery_Info info = get_def_battery();
	xil_printf("Nominal voltage: %d mV\n", info.norm_volt);
	xil_printf("Battery capacity: %d mA\n", info.capacity);

	// measure
	int a0 = read_ADC(0);
	int a1 = read_ADC(1);
	int a2 = read_ADC(2);
	int actual_current = a0-a1;
	int battery_act_v = a1-a2;
	xil_printf("Battery actual voltage: %d mV\n", battery_act_v);

	xil_printf("%d \t %d \t %d \t %d mV \t %d mA\r\n", a0,a1,a2, battery_act_v, actual_current);

	if(battery_act_v >= info.norm_volt)
	{
		// already charged
		xil_printf("Battery fully charged, shutting down\n");
	    cleanup_platform();
	    return 0;
	}

	// wait 5 sec
	usleep(5000000);
	xil_printf("Starting charging\n");

	actual_current = measure_current();
	set_now_current(actual_current);
	//set_set_current(info.capacity);
	set_set_current(500);
	set_enable(1);



	for(int i = 0; i < 10000000; i++)
	{
		a0 = read_ADC(0);
		a1 = read_ADC(1);
		a2 = read_ADC(2);
		actual_current = a0-a1;
		set_now_current(actual_current);

		if(i % 100000 == 0)
		{
			int PID_value = PID_read();
			//xil_printf("PID outputs %d \r\n", PID_value);
			// print stuff once in a while
			battery_act_v = a1-a2;
			xil_printf("%d \t %d \t %d \t %d mV \t %d mA\r\n", a0,a1,a2, battery_act_v, actual_current);
		}



	} //end while loop

	//write 0 to addresses
	set_enable(0);
	set_now_current(0);
	set_set_current(0);
    cleanup_platform();
    return 0;
}

//user input function
struct Battery_Info prompt_user()
{
	int norm_volt = 0.0;
	int capacity = 0;
	char accept = 'N';

	while(accept != 'Y' && accept != 'y')
	{
		printf("Enter nominal voltage in mV: ");
		scanf( "%d", &norm_volt );

		printf("\nEnter battery capacity in mA: ");
		scanf( "%d", &capacity );

		printf("\n\n");
		printf("You entered:\n");
		printf("Voltage: %d mV \n", norm_volt);
		printf("Capacity: %d mA \n", capacity);
		printf("Press 'Y' to accept, or 'N' to re-enter values \n");

		scanf( "%c", &accept); //reads a "carriage return" char
		scanf( "%c", &accept); //then read the real user input
	}

	struct Battery_Info bi;
	bi.norm_volt = norm_volt;
	bi.capacity = capacity;

	return bi;
}
