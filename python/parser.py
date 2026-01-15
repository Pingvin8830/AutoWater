#!/usr/bin/python

import sys


def printHelp():
    text = f'''Bad run.
Usage:
{sys.argv[0]} -f <LOGFILE> -p <PROJECT>
    '''
    print(text)
    sys.exit()

if len(sys.argv) < 5: printHelp()
key = ''
log = ''
project = ''

for arg in sys.argv[1:]:
    match arg:
        case '-f': key = 'log'
        case '-p': key = 'project'
        case _:
            match key:
                case 'log': log = arg
                case 'project': project = arg

if not log or not project: printHelp()

print(f"Log: {log}; Project: {project}")
if project == "AutoWater": from autowater import *
tmp = log.split('/')
tmp = tmp[len(tmp)-1].split('\\')
destSplit = tmp[len(tmp)-1].split('.')
dest = f"{destSplit[0]}_TXT.{destSplit[1]}"

with open(log, 'r') as logFile:
    lastString = ''
    string = logFile.readline()
    while string:
        date = ''
        time = ''
        typeCode = None
        codeCode = None
        detailCode = None
        state = ''
        tmp = ''
        for char in string:
            if char != ' ' and char != '\n':
                tmp += char
            else:
                if not date: date = tmp
                elif not time: time = tmp
                elif typeCode == None: typeCode = int(tmp)
                elif codeCode == None: codeCode = int(tmp)
                elif detailCode == None: detailCode = int(tmp)
                elif not state: state = tmp
                tmp = ''
        if lastString != string:
            typeText = TYPES[typeCode]
            codeText = CODES[codeCode]
            detailText = DETAILS[detailCode]
            with open(dest, 'a') as destFile:
                destFile.write(f"Date: {date}; Time: {time}; Type: {typeText}; Code: {codeText}; Detail: {detailText}; State: {state}\n")
        lastString = string
        string = logFile.readline()
