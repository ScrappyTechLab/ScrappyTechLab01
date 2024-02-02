'''
py script that we execute on the computer, which publishes the colour data per frame into serial, so pico can consume the data
'''

import io, os
import time
import serial
import random
from PIL import Image
from image_processing import processImageFromFile, initializeVideo, processOneFrameFromVideo
import datetime
import threading
from queue import Queue
import getpass

import cv2
import numpy as np

# queue for communicating between the publisher (gathers colour info) and consumer (publishes to the light strip)
queue = Queue(maxsize = 10)


def readLine():
    '''
    this is to read back serial data from the pico
    '''
    return ser.readline()


def clearOutColours(colours):
    '''
    clear out the previously published pattern
    '''
    for j in range (0, 60):
        for i in range (0, 3):
            colours[j][i] = 0


def pattern(colours):
    '''
    to show a pattern in the light strip
    this can be used to test the strip and the communication
    '''
    for j in range (0, 30):
        colours[j][0] = 0
        colours[j][1] = 0
        colours[j][2] = 0
    for j in range (30, 60):
        colours[j][0] = 0
        colours[j][1] = 0
        colours[j][2] = 0


'''
retrive the colour information, mostly from a video stream
'''
def getColourInformation():

    while (True):
        # colours = [[0 for x in range(3)] for y in range(60)]
        startTime = datetime.datetime.now()
        colours = processOneFrameFromVideo()
        if colours is None:
            break
        # if queue.qsize() > 0:
            # time.sleep(0.01)
        queue.put(colours, block = True, timeout = 3)
        endTime = datetime.datetime.now()
        getColourInformation.frameExecutionTime += (endTime - startTime).microseconds
        getColourInformation.iterationCountGetColourInformation += 1
        print(f"count: {getColourInformation.iterationCountGetColourInformation} frameExecutionTime: {getColourInformation.frameExecutionTime / getColourInformation.iterationCountGetColourInformation}")

# below are static variables being initialized
getColourInformation.iterationCountGetColourInformation = 0
getColourInformation.frameExecutionTime = 0


def processColourInformation():
    '''
    process the colour information and light up the LED strip
    '''
    try:
        while (True):
            startTime = datetime.datetime.now()
            # colours = queue.get(block = True)

            if queue.qsize() > 1:
                for i in range (0, queue.qsize()):
                    colours = queue.get(block = True)
            else:
                colours = queue.get(block = True)

            if colours is None:
                break
            for j in range (0, 60):
                for i in range (0, 3):
                    # break out into digits
                    ones = str( int( (colours[j][i] % 10) / 1 ) )
                    tens = str( int( (colours[j][i] % 100) / 10 ) )
                    hundreds = str( int( (colours[j][i] % 1000) / 100 ) )

                    ser.write(str.encode(hundreds))
                    ser.write(str.encode(tens))
                    ser.write(str.encode(ones))

                    pass

                    # uncomment the below when we try to match the printed values from the pico with the value we sent to it
                    '''

                    # print(f"loop: {str(j)} {str(i)} . writing: {hundreds}{tens}{ones} . wrote: {str(written)}")
                    # read_back = int(readLine())
                    # if colours[j][i] == read_back:
                        # print("matched")
                        # pass
                    # else:
                        # print(f"unmatched. read: {read_back}")
                        # pass
                    '''


            endTime = datetime.datetime.now()

            processColourInformation.lightExecutionTime += (endTime - startTime).microseconds
            processColourInformation.iterationCountProcessColourInformation += 1
            print(f"count: {processColourInformation.iterationCountProcessColourInformation} lightExecutionTime: {processColourInformation.lightExecutionTime / processColourInformation.iterationCountProcessColourInformation}")
    except e:
        print (e)
        return

# below are static variables being initialized
processColourInformation.iterationCountProcessColourInformation = 0
processColourInformation.lightExecutionTime = 0




def main():
    # initialize video stream
    initializeVideo(f"/home/{getpass.getuser()}/Downloads/colourfulVideo.mp4")

    global ser
    ser = serial.Serial('/dev/ttyACM0', baudrate=9600, write_timeout=1)

    # spin up the light strip handling in a separate thread
    consumerThread = threading.Thread(target = processColourInformation, args = ())

    consumerThread.start()

    # the opencv processing seems to work on the primary thread. processing this on a separate thread, does not work
    getColourInformation()

    consumerThread.join()

    ser.close()


if __name__ == "__main__":
    main()


