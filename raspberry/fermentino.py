#!/usr/bin/python
# -*- coding: utf-8 -*-

import MySQLdb as mdb
import sys
import time
from datetime import datetime
import shutil
import urllib
import serial
import socket
import os
from configobj import ConfigObj

config = ConfigObj('/home/fermentino/settings/config.cfg')

con = None

print "fermentino raspberry script started ..."
print "by Joerg Holzapfel"

try:

    con = mdb.connect(config['dbServer'], config['dbUser'], 
        config['dbPass'], config['dbName']);

    cur = con.cursor()
    cur.execute("SELECT VERSION()")

    data = cur.fetchone()
    
    print "Database version : %s " % data
    
except mdb.Error, e:
  
    print "Error %d: %s" % (e.args[0],e.args[1])
    sys.exit(1)
    
try:

	ser = serial.Serial(config['port'], 9600, timeout=1)

except serial.SerialException, e:
	print e
	sys.exit(1)

#create a listening socket to communicate with PHP
if os.path.exists(config['scriptPath'] + '/FERMENTSOCKET'):
	# if socket already exists, remove it
	os.remove(config['scriptPath'] + '/FERMENTSOCKET')
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(config['scriptPath'] + '/FERMENTSOCKET')  # Bind FERMENTSOCKET
# set all permissions for socket
os.chmod(config['scriptPath'] + '/FERMENTSOCKET', 0777)
s.setblocking(1)  # set socket functions to be blocking
s.listen(5)  # Create a backlog queue for up to 5 connections
# blocking socket functions wait 'serialCheckInterval' seconds
s.settimeout(float(config['serialCheckInterval']))

ser.flush()
#set Arduino time
ser.write("ST%10.0f" % time.time())
print "Time sync message: ST%10.0f" % time.time()
time.sleep(1)

prevDataTime = 0.0  # keep track of time between new data requests
prevTimeOut = time.time()

run = 1

lcdText = ""

while(run):

	try:
		conn, addr = s.accept()
		message = conn.recv(1024)
		
		if "=" in message:
			messageType, value = message.split("=", 1)
		else:
			messageType = message
			
		if messageType == "stopScript":
			run = 0
			dontrunfile = open(config['wwwPath'] + 'do_not_run_fermentino', "w")
			dontrunfile.write("1")
			dontrunfile.close()
			continue
			
		elif messageType == "ack":  # acknowledge request
			conn.send('ack')
			
		elif messageType == "lcd":  # lcd contents requested
			conn.send(lcdText)
		
		if (time.time() - prevTimeOut) < config['serialCheckInterval']:
			continue
		else:
			# raise exception to check serial for data immediately
			raise socket.timeout
	
	except socket.timeout:
		# Do serial communication and update settings every SerialCheckInterval
		prevTimeOut = time.time()
		
		#request LCD text
		ser.write('l')
		time.sleep(1)
		
		# if no new data has been received for serialRequestInteval seconds
		if((time.time() - prevDataTime) >= float(config['interval'])):
			ser.write("t")  # request new from arduino

		elif((time.time() - prevDataTime) > float(config['interval']) +
										2 * float(config['interval'])):
			print "Error: Arduino is not responding to new data requests"
			#something is wrong: arduino is not responding to data requests
			#print >> sys.stderr, (time.strftime("%b %d %Y %H:%M:%S   ")
			#					+ "Error: Arduino is not responding to new data requests")
			
			
		while(1):
	
			line = ser.readline()
	
			if (line):
		
				try:
			
					if (line[0] == 'T'): #process temperature line
			
						print line[0:]
						splitline = line[1:].split(',')
						sql = "INSERT INTO `fermentino`.`fm_TempProtocol` (`tp_TimeStamp`, `tp_ArduinoTimestamp`, `tp_TargetTemp`, `tp_MeasuredTemp`, `tp_HeatingStatus`, `tp_CoolerStatus`, `tp_RunningProgram_ID`) VALUES (CURRENT_TIMESTAMP, FROM_UNIXTIME(" + splitline[0] + "), '" + splitline[2] + "', '" + splitline[1] + "', '" + splitline[3] + "', '0', '" + splitline[4] + "');"
						cur = con.cursor()
						cur.execute(sql)
						con.commit()
						
					elif(line[0] == 'L'):
						# lcd content received
						lcdText = line[1:]
						print lcdText
		
				except mdb.Error, e:
		
					print "Error %d: %s" % (e.args[0],e.args[1])
					sys.exit(1)
    		
			else:
				break
	
if ser:
	ser.close()   
        
if con:    
    con.close()