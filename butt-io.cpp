// butt IO master v 0.1
// by Daniel Litzbach

#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/types.h>
#include <fcntl.h>
//#include <inttypes.h>
//#include <math.h>
#include <unistd.h>

#define BASEPORT 0x378 // Data register of parallel port
#define STATUSPORT 0x379 // Status register of parallel port

using namespace std;

char statusfile_path[100];
char commandfile_path[100];
char tsfile_path[100];
string last_state;
//bool status_changed=false;
bool in_changed=false;
int currentstate;
bool POut[4] = {false};
bool Buttons[3] = {false};
unsigned long last_tick[8] = {0}; // 0: Buttons - 1: butt-state - 2: call-button-debouncer - 3: call-button-blink - 4: confirm-call - 5: command-reset-request - 6: on/offAir-buttons-debouncer - 7: on/offAir-blink
unsigned long CurrentTimeMS;
bool action_triggered=false;
int last_in_buffer=0;
bool call_enabled=false;
bool call_led_enabled=false;
bool confirm_call=false;
bool command_reset_request=false;
bool error_signal=false;
bool dial_signal=false;
bool error_led_enabled=false;
bool dial_led_enabled=false;
unsigned long startup_ts=0;
bool pre_startup_notified=false;
bool startup_notified=false;
bool startup_call_enabled=false;
bool startup_call_led_enabled=false;

unsigned long GetCurrentTimeMS (void)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	return ms;
}

unsigned long MStoSec(unsigned long MS)
{
	return MS/1000;
}


void GetCurrentButtons()
{
	int buffer = inb(STATUSPORT);
	switch(buffer)
	{
		case 248: // Button 1 / Pin 10 (offAir)
			Buttons[0]=true;
			Buttons[1]=false;
			Buttons[2]=false;
			break;
		case 88: // Button 2 / Pin 11 (Call)
			Buttons[0]=false;
			Buttons[1]=true;
			Buttons[2]=false;
			break;
		case 104: // Button 3 / Pin 12 (onAir)
			Buttons[0]=false;
			Buttons[1]=false;
			Buttons[2]=true;
			break;
		default:
			Buttons[0]=false;
			Buttons[1]=false;
			Buttons[2]=false;
			//Buttons={false,false,false};
			break;
	}
	if(buffer!=last_in_buffer)
	{
		in_changed=true;
	} else {
		in_changed=false;
	}
	last_in_buffer=buffer;
	
}

void SetButtCommand(string ButtCommand)
{
	ofstream commandfile_handle;
	commandfile_handle.open(commandfile_path);
	commandfile_handle << ButtCommand;
	commandfile_handle.close();
}

void ScheduleCommandReset()
{
	command_reset_request=true;
	last_tick[5]=CurrentTimeMS;
	//cout << "SCH" << endl;
}

void setOut(int led,bool status)
{
	POut[led-1]=status;
	int out_value=0;
	if(POut[0]) out_value=out_value+1; // LED offAir	(1)
	if(POut[1]) out_value=out_value+2; // LED Call		(2)
	if(POut[2]) out_value=out_value+4; // LED onAir		(3)
	if(POut[3]) out_value=out_value+8; // Relais BRadio	(4)
	outb(out_value, BASEPORT);
}

int main()
{
	//struct timespec ts;
	//ts.tv_sec = 1;
	//ts.tv_nsec = 200000000L; // 200 ms
	//ts.tv_nsec = 2000000000L; // 20 ms
	
	char c;
        int n, tem;
	
	//set permissions to access port
        if (ioperm(BASEPORT, 3, 1)) {perror("ioperm"); exit(1);}
        if (ioperm(STATUSPORT, 3, 1)) {perror("ioperm"); exit(1);}
        
        tem = fcntl(0, F_GETFL, 0);
        fcntl (0, F_SETFL, (tem | O_NDELAY));
	
	cout << "Fanradio butt IO dispatcher V 0.1 (C) by Daniel Litzbach starting up." << endl;
	cout << "Contact: daniel.litzbach@fufa-sv98.de or dl@litzinetz.de" << endl;
	
        strcpy(statusfile_path,getenv("HOME"));
        strcat(statusfile_path,"/.butt_status.dat");
	
        strcpy(commandfile_path,getenv("HOME"));
        strcat(commandfile_path,"/.butt_command.dat");
	
	strcpy(tsfile_path,getenv("HOME"));
        strcat(tsfile_path,"/.butt_ts.dat");
	
	cout << "Using IO files:" << endl;
	cout << "status file: " << statusfile_path << endl;
	cout << "command file: " << commandfile_path << endl;
	cout << "timestamp file" << tsfile_path << endl;
	
	string line;
	ifstream tsfile_handle(tsfile_path);
	if(tsfile_handle.is_open())
	{
		getline(tsfile_handle,line);
		tsfile_handle.close();
		startup_ts=stoi(line);
		//cout << startup_ts << endl;
		//cout << time(0) << endl;
	}
	
	while(true)
	{
		//GetCurrentButtState();
		//cout << currentstate << endl;
		bool WorkedHard=false;
		
		CurrentTimeMS=GetCurrentTimeMS();
		unsigned long CurrentTimeSec=time(0);
		if(CurrentTimeSec>=startup_ts-300 && CurrentTimeSec<startup_ts && !pre_startup_notified) // Is it within five minutes before the beginning and we haven't enabled the pre-notification yet?
		{
			call_enabled=true; // enable Call
			pre_startup_notified=true;
			WorkedHard=true;
		}
		
		if(CurrentTimeSec>=startup_ts-10 && CurrentTimeSec<startup_ts+300 && !startup_notified) // Is it within 10 seconds before (or less than five minutes after) the beginning and we haven't enabled the notification yet?
		{
			startup_call_enabled=true;
			startup_notified=true;
			WorkedHard=true;
		}
		
		if(startup_call_enabled) // Is a startup call currently active?
		{
			if(CurrentTimeMS>=last_tick[7]+150) // Toggle every 150 ms
			{
				if(startup_call_led_enabled)
				{
					setOut(3,false);
					startup_call_led_enabled=false;
				} else {
					setOut(3,true);
					startup_call_led_enabled=true;
				}
				last_tick[7]=CurrentTimeMS;
				WorkedHard=true;
			}
		}
		
		
		if(CurrentTimeMS>=last_tick[0]+100) // Check buttons every 100 ms
		{
			//cout << "checking Buttons" << endl;
			GetCurrentButtons();
			if(in_changed)
			{
				if(Buttons[0] && CurrentTimeMS>=last_tick[6]+1000) // offAir (debounce 1000ms)
				{
					SetButtCommand("disconn");
					//ScheduleCommandReset();
					last_tick[6]=CurrentTimeMS;
				}
				if(Buttons[1]) // Call
				{
					if(last_tick[2]==0 || CurrentTimeMS>=last_tick[2]+1000) // Process call button only if it hasn't been pushed before or if it's been pushed more than 1 second ago
					{
						// Toggle call
						if(!call_enabled)
						{
							call_enabled=true;
							confirm_call=false; // Disable call confirmation, if any
						} else {
							call_enabled=false;
							confirm_call=true; // If disabled, the call must be confirmed by lighting up the LED for 3 Seconds
							setOut(2,true);
							last_tick[4]=CurrentTimeMS;
						}
						last_tick[2]=CurrentTimeMS;
						//cout << "Toggle Call" << endl;
					}
				}
				if(Buttons[2] && CurrentTimeMS>=last_tick[6]+1000) // onAir
				{
					SetButtCommand("conn");
					//ScheduleCommandReset();
					last_tick[6]=CurrentTimeMS;
				}
			}
			
			last_tick[0]=CurrentTimeMS;
			WorkedHard=true;
		} // End of button check
		
		if(CurrentTimeMS>=last_tick[1]+300) // Check butt state every 300 ms
		{
			string line;
			ifstream statusfile_handle(statusfile_path);
			if(statusfile_handle.is_open())
			{
				getline(statusfile_handle,line);
				statusfile_handle.close();
				
				if(line != last_state) // state changed
				{
					//cout << "CHANGE" << endl;
					//status_changed=true;
					if(line == "conn")
					{
						error_signal=false; // diable error signal
						dial_signal=false; // disable dial signal
						setOut(1,false); // disable offAir
						setOut(3,true); // enable onAir
						setOut(4,true); // enable BRadio-Relais
						
						if(startup_call_enabled) // Disable startup call (if enabled) and reset tick counter
						{
							startup_call_enabled=false;
							startup_call_led_enabled=false;
							last_tick[7]=0;
						}
					}
					else if(line == "disconn")
					{
						error_signal=false; // diable error signal
						dial_signal=false; // disable dial signal
						setOut(1,true); // enable offAir
						setOut(3,false); // disable onAir
						setOut(4,false); // disable BRadio-Relais
					}
					else if(line == "error")
					{
						error_signal=true; // enable error signal
						dial_signal=false; // disable dial signal
						setOut(1,false); // disable offAir
						setOut(3,false); // disable onAir
						//cout << "err" << endl;
					}
					else if(line == "dial")
					{
						dial_signal=true;
						setOut(1,true); // ensable offAir
						setOut(3,false); // disable onAir
					}
					last_state=line;
				}
				
			}
			last_tick[1]=CurrentTimeMS;
			WorkedHard=true;
		}
		
		if(command_reset_request && CurrentTimeMS>=last_tick[5]+1000) // If there's a reset request, we must execute if after 1 second!
		{
			SetButtCommand("none");
			command_reset_request=false;
			//cout << "RST" << endl;
			WorkedHard=true;
		}
		
		if(confirm_call && CurrentTimeMS>=last_tick[4]+3000) // Do we have to confirm a call and have 3 seconds passed?
		{
			setOut(2,false);
			confirm_call=false;
			WorkedHard=true;
		}
		
		if(call_enabled) // If the call is set, we need to toggle the LED
		{
			if(CurrentTimeMS>=last_tick[3]+800) // Toggle every 800 ms
			{
				if(call_led_enabled)
				{
					setOut(2,false);
					call_led_enabled=false;
				} else {
					setOut(2,true);
					call_led_enabled=true;
				}
				last_tick[3]=CurrentTimeMS;
			}
			WorkedHard=true;
		}
		
		if(dial_signal) // Is butt on dial state?
		{
			if(CurrentTimeMS>=last_tick[7]+200) // Toggle on/offAir every 200 ms
			{
				if(dial_led_enabled)
				{
					setOut(3,false);
					setOut(1,true);
					dial_led_enabled=false;
				} else {
					setOut(3,true);
					setOut(1,false);
					dial_led_enabled=true;
				}
				last_tick[7]=CurrentTimeMS;
			}
			WorkedHard=true;
		}
		
		if(error_signal) // is butt on error state?
		{
			if(CurrentTimeMS>=last_tick[7]+200) // Toggle offAir every 200 ms
			{
				if(error_led_enabled)
				{
					setOut(1,false);
					error_led_enabled=false;
					//cout << "e0" << endl;
				} else {
					setOut(1,true);
					error_led_enabled=true;
					//cout << "e1" << endl;
				}
				last_tick[7]=CurrentTimeMS;
			}
			WorkedHard=true;
		}
		
		if(!WorkedHard) // Have we been as busy as a bee?
		{
			//cout << "Nothing to do" << endl;
			//cout << CurrentTimeMS << endl;
			//cout << "LT " <<last_tick[5] << endl;
			//nanosleep(&ts,NULL);
			usleep(20000); // Chill for 20 ms, don't wanna block the hosts resources!
			//cout << "waking up..." << endl;
		}
	}
	return 0;
}