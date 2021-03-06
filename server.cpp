/*
 * server.cpp
 *
 *  Created on: 2015-11-2
 *      Author: Max.Chiu
 *      Email: Kingsleyyau@gmail.com
 */

#include "AdmirerSender.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <string>
using namespace std;

string sConf = "";  // 配置文件

bool Parse(int argc, char *argv[]);

int main(int argc, char *argv[]) {
	printf("############## Admirer Sender ############## \n");
	printf("# Version : %s \n", VERSION_STRING);
	printf("# Build date : %s %s \n", __DATE__, __TIME__ );

	srand(time(0));

	/* Ignore SIGPIPE */
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGPIPE, &sa, 0);

	Parse(argc, argv);

	bool bFlag = false;
	AdmirerSender server;
	if( sConf.length() > 0 ) {
		bFlag = server.Run(sConf);
	} else {
		printf("# Usage : ./admirersender [ -f <config file> ] \n");
		bFlag = server.Run("/etc/admirersender.config");
	}

	while( bFlag && server.IsRunning() ) {
		/* do nothing here */
		sleep(5);
	}

	return EXIT_SUCCESS;
}

bool Parse(int argc, char *argv[]) {
	string key, value;
	for( int i = 1; (i + 1) < argc; i+=2 ) {
		key = argv[i];
		value = argv[i+1];

		if( key.compare("-f") == 0 ) {
			sConf = value;
		}
	}

	return true;
}
