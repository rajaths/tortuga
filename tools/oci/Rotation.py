# Copyright (C) 2007 Maryland Robotics Club
# Copyright (C) 2007 Jon Speiser <jspeiser@umd.edu>
# All rights reserved.
#
# Author: Jon Speiser <jspeiser@umd.edu>
import wx
from math import radians,sqrt,pi

import ext.core as core
import ext.math as math

class RotationCtrl(wx.Panel):
    def __init__(self, parent,label="[Default]"):
        wx.Panel.__init__(self, parent, -1)
        width,height = self.GetSize()
        self.label = label
        
        
        self.rotVal = 0
        
        self.Bind(wx.EVT_PAINT, self.OnPaint)
        self.Bind(wx.EVT_ERASE_BACKGROUND, self.OnEraseBackground)

    """ Take a value and set the GUI's rotation"""
    def setOrientation(self,quat):
        if self.label == "Roll":
            self.rotVal = (quat.getRoll(False)).valueRadians()
        elif self.label == "Pitch":
            self.rotVal = (quat.getPitch(False)).valueRadians()
        elif self.label == "Yaw":
            self.rotVal = (quat.getYaw(False)).valueRadians()
        self.Refresh()
        
    def rotate(self,degrees=15):
        self.rotVal+=degrees
        self.Refresh()
    
    def OnEraseBackground(self, event):
        pass
    
    def OnPaint(self, evt):
        dc = wx.PaintDC(self)
        gc = wx.GraphicsContext.Create(dc)
        self.draw(gc)        
           
    def draw(self,gc):
        #TODO: change pen color to correspond to degrees rotated?
        #TODO: clean up the code!
        width,height = self.GetSize()
        gc.SetPen(wx.Pen("black", 3))
        gc.SetBrush(wx.Brush(wx.Colour(0, 0, 0, 98)))
        """ Keep track of window width/height for centering """
        originalWidth, originalHeight = None, None   
        """ Ensure shapes are drawn appropriately
        
        If we find the width/height or height/width ratio is skewed,
        make the algorithm believe that we are dealing with smaller
        constraints so the shapes are not cut off
        
        """
        if (height/float(width) < .58):
            originalWidth = width
            width = height + float(height * .58)
        if (width/float(height) < .58):
            originalHeight = height
            height = width + float(width * .58)  
        xCenter = width / 4
        yCenter = height / 4     
        """ Use the distance formula to get the length of the sides """
        a = sqrt( (-xCenter-xCenter)**2 + (-yCenter-yCenter)**2 )
        b = sqrt( (xCenter-0)**2 + (-yCenter-yCenter)**2 )
        """ Radius Calculation
        
        Calculate the radius for an Isoceles Triangle: ((a^2)*b)/4*K
        where a and b are two sides and K is the area
        
        """
        radius = (a**2 *b) / (4 * (.5 * a * b)) 
        #Note that for simplicity this triangle has its origin at 0,0 
        #Rounding
        xCenter = int(xCenter+.5)
        yCenter = int(yCenter+.5)
        radius = int(radius+.5)
        
        trianglePath = gc.CreatePath()
        trianglePath.MoveToPoint(-xCenter, -yCenter)
        trianglePath.AddLineToPoint(xCenter, -yCenter)
        #trianglePath.MoveToPoint(xCenter,-yCenter)
        trianglePath.AddLineToPoint(0, radius)
        #trianglePath.MoveToPoint(0, radius)
        trianglePath.AddLineToPoint(-xCenter, -yCenter)
        trianglePath.CloseSubpath()
        
        directionCircle = gc.CreatePath()
        directionCircle.AddCircle(0,radius,3)
        directionCircle.CloseSubpath()

        """ Move to the middle """
        #if originalWidth <> None:    
        #    gc.Translate(originalWidth/2,yCenter*2)
        #elif originalHeight <> None:
        #    gc.Translate(xCenter*2, originalHeight/2)
        #else:
        #    gc.Translate(xCenter*2,yCenter*2)
        if originalWidth <> None: 
            gc.Translate(originalWidth/2,radius+5)
        else:
            gc.Translate(xCenter*2,radius+5)
        """ Rotate on origin triangle """
        gc.Rotate(self.rotVal) #Assumes radians!
        gc.DrawPath(trianglePath)
        gc.DrawPath(directionCircle)

        """ Draw the circle that circumscribes the triangle """
        circle = gc.CreatePath()
        circle.AddCircle(0,0,radius)
        gc.StrokePath(circle)
        
        """
        Create a copy of the triangle path to display desired orientation
        work on rotating independently
        """
        desiredPath = gc.CreatePath()
        #linepts = [(-xCenter,-yCenter),(xCenter,-yCenter),(0, radius+20),(-xCenter, -yCenter)]
        #print self.label, linepts
        desiredPath.AddPath(trianglePath)
        desiredPath.AddPath(directionCircle)
        desiredPath.CloseSubpath()
        xform = gc.CreateMatrix()
        xform.Rotate(pi/6)
        desiredPath.Transform(xform)
        
        pen = wx.Pen(wx.Colour(0, 0, 0, 128),3)
        
        #pen.SetWidth(4)
        #pen.SetStyle(wx.DOT)
        gc.SetPen(pen)
        brush = wx.Brush(wx.Colour(0, 255, 0, 90))
        gc.SetBrush(brush)
        gc.DrawPath(desiredPath)
        #gc.StrokePath(desiredPath)
        #gc.FillPath(desiredPath)