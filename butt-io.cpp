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
string last_state;
//bool status_changed=false;
bool in_changed=false;
int currentstate;
bool LEDs[3] = {false};
bool Buttons[3] = {false};
unsigned long last_tick[5] = {0}; // 0: Buttons - 1: butt-state - 2: call-button-blocker - 3: call-button-blink - 4: confirm-call
unsigned long CurrentTimeMS;
bool action_triggered=false;
int last_in_buffer=0;
bool call_enabled=false;
bool call_led_enabled=false;
bool confirm_call=false;

unsigned long GetCurrentTimeMS (void)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	return ms;
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

/*void GetCurrentButtState()
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
			status_changed=true;
			if(line == "conn")
			{
				currentstate=0;
			}
			else if(line == "disconn")
				{
				currentstate=1;
			}
			else if(line == "error")
				{
				currentstate=2;
			}
			else
			{
				currentstate=255;
			}
			last_state=line;
		}*else {
			cout << "unchanged" << endl;
			cout << "-" << line << "-" << endl;
			cout << "-" << last_state << "-" << endl;
			cout << "EOM" << endl;
		}
		
	} else {
		cout << "Error opening statusfile" << endl;
	}
}*/

void SetButtCommand(string ButtCommand)
{
	ofstream commandfile_handle;
	commandfile_handle.open(commandfile_path);
	commandfile_handle << ButtCommand;
	commandfile_handle.close();
}

void setLED(int led,bool status)
{
	LEDs[led-1]=status;
	int led_value=0;
	if(LEDs[0]) led_value=led_value+1; // offAir	(1)
	if(LEDs[1]) led_value=led_value+2; // Call	(2)
	if(LEDs[2]) led_value=led_value+4; // onAir	(3)
	outb(led_value, BASEPORT);
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
	
	cout << "Using IO files:" << endl;
	cout << "statusfile: " << statusfile_path << endl;
	cout << "commandfile: " << commandfile_path << endl;
	
	while(true)
	{
		//GetCurrentButtState();
		//cout << currentstate << endl;
		bool WorkedHard=false;
		
		CurrentTimeMS=GetCurrentTimeMS();
		
		if(CurrentTimeMS>=last_tick[0]+100) // Check buttons every 100 ms
		{
			//cout << "checking Buttons" << endl;
			GetCurrentButtons();
			if(in_changed)
			{
				if(Buttons[0]) // offAir
				{
					SetButtCommand("disconn");
					// TODO: Reset command ?
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
							setLED(2,true);
							last_tick[4]=CurrentTimeMS;
						}
						last_tick[2]=CurrentTimeMS;
						//cout << "Toggle Call" << endl;
					}
				}
				if(Buttons[2]) // onAir
				{
					SetButtCommand("conn");
					// TODO: Reset command ?
				}
			}
			
			last_tick[0]=CurrentTimeMS;
			WorkedHard=true;
		}
		
		if(confirm_call && CurrentTimeMS>=last_tick[4]+3000) // Do we have to confirm a call and have 3 seconds passed?
		{
			setLED(2,false);
			confirm_call=false;
		}
		
		if(call_enabled) // If the call is set, we need to toggle the LED
		{
			if(CurrentTimeMS>=last_tick[3]+800) // Toggle every 800 ms
			{
				if(call_led_enabled)
				{
					setLED(2,false);
					call_led_enabled=false;
				} else {
					setLED(2,true);
					call_led_enabled=true;
				}
				last_tick[3]=CurrentTimeMS;
			}
			WorkedHard=true;
		}
		
		if(!WorkedHard) // Have we been as busy as a bee?
		{
			//cout << "Nothing to do" << endl;
			//cout << CurrentTimeMS << endl;
			//nanosleep(&ts,NULL);
			usleep(20000); // Chill for 20 ms, don't wanna block the hosts resources!
			//cout << "waking up..." << endl;
		}
	}
	return 0;
}