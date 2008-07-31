# Copyright (C) 2008 Maryland Robotics Club
# Copyright (C) 2008 Joseph Lisee <jlisee@umd.edu>
# All rights reserved.
#
# Author: Joseph Lisee <jlisee@umd.edu>
# File:  packages/python/ram/ai/sonar.py
  
# STD Imports
import math
  
# Project Imports
import ext.core as core
import ext.vehicle as vehicle
import ext.vehicle.device
import ext.math

import ram.ai.state as state
import ram.motion as motion
#import ram.motion.search
import ram.motion.pipe

COMPLETE = core.declareEventType('COMPLETE')

class PingerState(state.State):
    @staticmethod
    def transitions(myState, trans = None):
        if trans is None:
            trans = {}
        trans.update({vehicle.device.ISonar.UPDATE : myState})
        return trans
    
    def _isNewPing(self, event):
        if self._lastTime != event.pingTimeUSec:
            self._lastTime = event.pingTimeUSec
            return True
        return False
    
    def _pingerAngle(self, event):
        pingerOrientation = ext.math.Vector3.UNIT_X.getRotationTo(
            event.direction)
        return pingerOrientation.getYaw(True)
    
    def _loadSettings(self):
        self._closeZ = self._config.get('closeZ', 0.85)
        self._speedGain = self._config.get('speedGain', 5)
        self._yawGain = self._config.get('yawGain', 1)
        self._maxSpeed = self._config.get('maxSpeed', 1)
        self._maxSidewaysSpeed = 0
        self._sidewaysSpeedGain = 0
    
    def enter(self):
        self._pipe = ram.motion.pipe.Pipe(0, 0, 0)
        self._lastTime = 0

        self._loadSettings()

        motion = ram.motion.pipe.Hover(pipe = self._pipe, 
                                       maxSpeed = self._maxSpeed, 
                                       maxSidewaysSpeed = self._maxSidewaysSpeed, 
                                       sidewaysSpeedGain = self._sidewaysSpeedGain, 
                                       speedGain = self._speedGain, 
                                       yawGain = self._yawGain)
        self.motionManager.setMotion(motion)

    def exit(self):
        self.motionManager.stopCurrentMotion()
        

class Searching(state.State):
    CHANGE = core.declareEventType("CHANGE")

    @staticmethod
    def transitions():
        return { vehicle.device.ISonar.UPDATE : Searching,
	         Searching.CHANGE : FarSeeking }
        
    def UPDATE(self, event):
        if self._first:
            pingerOrientation = ext.math.Vector3.UNIT_X.getRotationTo(
                event.direction)
            self.controller.yawVehicle(pingerOrientation.getYaw(True).valueDegrees())
	   
	    self.timer = self.timerManager.newTimer(Searching.CHANGE, 4)
	    self.timer.start()
	    self._first = False

    def enter(self):
        self._first = True
        self.timer = None
        
    def exit(self):
        if self.timer is not None:
            self.timer.stop()

class TranslationSeeking(PingerState):
    CLOSE = core.declareEventType('CLOSE')    
    
    def _loadSettings(self):
        PingerState._loadSettings(self)
        self._maxSidewaysSpeed = self._config.get('maxSidewaysSpeed', 2)
        self._sidewaysSpeedGain = self._config.get('sidewaysSpeedGain', 2)

    def UPDATE(self, event):
        if self._isNewPing(event):
            # Converting from the vehicle reference frame, to the image space
            # reference frame used by the pipe motion
            self._pipe.setState(-event.direction.y, event.direction.x, 
                                ext.math.Degree(0))
            
            if math.fabs(event.direction.z) > math.fabs(self._closeZ):
                 self.publish(TranslationSeeking.CLOSE, core.Event())

# 0.65
class FarSeeking(TranslationSeeking):
    """
    Approaches the pinger from a far away difference
    """
    @staticmethod
    def transitions():
        return TranslationSeeking.transitions(FarSeeking, 
            { TranslationSeeking.CLOSE : CloseSeeking } ) 
                 
    def _loadSettings(self):
        TranslationSeeking._loadSettings(self)
        self._closeZ = self._config.get('midRangeZ', 0.65)
                 
class CloseSeeking(TranslationSeeking):
    """
    For when we are close to the pinger
    """
    @staticmethod
    def transitions():
        return TranslationSeeking.transitions(CloseSeeking, 
            { TranslationSeeking.CLOSE : End } ) 

    def _loadSettings(self):
        PingerState._loadSettings(self)
        self._closeZ = self._config.get('closeZ', 0.8)
    
class Hovering(TranslationSeeking):
    """
    Just hovers over the pinger
    """
    @staticmethod
    def transitions():
        return TranslationSeeking.transitions(Hovering)

class End(state.State):
    def enter(self):
        self.publish(COMPLETE, core.Event())
