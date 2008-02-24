/*
 * Copyright (C) 2008 Robotics at Maryland
 * Copyright (C) 2008 Joseph Lisee <jlisee@umd.edu>
 * All rights reserved.
 *
 * Author: Joseph Lisee <jlisee@umd.edu>
 * File:  packages/vision/test/src/CvCameraTest.cpp
 */


// STD Includes
#include <stdlib.h>
#include <stdio.h>

// Library Includes
#include "cv.h"
#include "highgui.h"

void usageError()
{
    fprintf(stderr, "Usage: CvCameraTest [camera number]\n");
    getchar();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    int camNum = CV_CAP_ANY;
    CvCapture* capture = 0;
    char* tailPtr = (char*)1;
        
    // Parse the camera num from the arguments
    if (argc == 2)
    {
        camNum = (int)strtol(argv[1], &tailPtr, 10);
        
        if ((*tailPtr) != '\0')
            usageError();
    }
    else if (argc != 1)
    {
        usageError();
    }

    // Open Up Cemera
    capture = cvCaptureFromCAM( CV_CAP_ANY );
    if (!capture)
    {
        fprintf(stderr, "ERROR: capture is NULL, no camera with num '%d'\n",
                camNum);
        getchar();
        return -1;
    }
    
    printf("Camera Capture Properties:\n");
    printf("\tWidth:  %f\n", cvGetCaptureProperty(capture,
                                                  CV_CAP_PROP_FRAME_WIDTH));
    printf("\tHeight: %f\n", cvGetCaptureProperty(capture,
                                                  CV_CAP_PROP_FRAME_HEIGHT));
    printf("\tFPS:    %f\n", cvGetCaptureProperty(capture,
                                                  CV_CAP_PROP_FPS));
    
    // Create a window in which the captured images will be presented
    cvNamedWindow("Raw Camera Image", CV_WINDOW_AUTOSIZE);
    
    // Show the image captured from the camera in the window and repeat
    while(1) {
        // Get one frame
        IplImage* frame = cvQueryFrame(capture);
        
        if (!frame) 
        {
            fprintf(stderr,
                    "ERROR: frame is null, could not get frame from camera\n");
            getchar();
            break;
        }
        
        cvShowImage("Raw Camera Image", frame);

        //Do not release the frame!
        //If ESC key pressed, Key=0x10001B under OpenCV 0.9.7(linux version),
        //remove higher bits using AND operator
        if ((cvWaitKey(10) & 255) == 27)
            break;
    }
    
    // Release the capture device housekeeping
    cvReleaseCapture(&capture);
    cvDestroyWindow("Raw Camera Image");
    
    return EXIT_SUCCESS;
}
