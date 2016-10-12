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

#define BASEPORT 0x378 // Data register of parallel port
#define STATUSPORT 0x379 // Status register of parallel port

using namespace std;

char statusfile_path[100];
char commandfile_path[100];
string last_state;
bool status_changed=false;
int currentstate;
bool LEDs[3] = {false};

void GetCurrentButtState()
{
	string line;
	int currentstate;
	ifstream statusfile_handle(statusfile_path);
	if(statusfile_handle.is_open())
	{
		getline(statusfile_handle,line);
		statusfile_handle.close();
		
		if(line != last_state) // state changed
		{
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
		}
		
	} else {
		cout << "Error opening statusfile" << endl;
	}
}

void setLED(int led,bool status)
{
	LEDs[led-1,status];
	int led_value=0;
	if(LEDs[0]) led_value=led_value+1;
	if(LEDs[1]) led_value=led_value+2;
	if(LEDs[2]) led_value=led_value+4;
	outb(led_value, BASEPORT);
}

int main()
{
	int las_state;
	bool first=true; // Used as flapper for alarm
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 200000000L; // 200 ms
	
	char c;
        int n, tem;
	
	//set permissions to access port
        if (ioperm(BASEPORT, 3, 1)) {perror("ioperm"); exit(1);}
        if (ioperm(STATUSPORT, 3, 1)) {perror("ioperm"); exit(1);}
        
        tem = fcntl(0, F_GETFL, 0);
        fcntl (0, F_SETFL, (tem | O_NDELAY));
	
	cout << "butt IO master V 0.1 (C) by Daniel Litzbach starting up." << endl;
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
		GetCurrentButtState();
		if(status_changed) // do we have a net status?
		{
			if(currentstate==0) // connected
			{
				setLED(3,true); // Enable "onAir"-LED
				setLED(1,false); // Disable "offAir"-LED
			}
			if(currentstate==1) // disconnected
			{
				setLED(1,true); // Enable "offAir"-LED
				setLED(3,false); // Disable "onAir"-LED
			}
		} else { // if we don't have a new status...
			if(currentstate==2) // we have to continue alerting anyway, if we have a connection error!
			{
				if(first)
				{
					setLED(3,true);
					first=false;
				} else {
					setLED(3,false);
					first=true;
				}
			}
		}
		
		nanosleep(&ts,NULL);
	}
	return 0;
}