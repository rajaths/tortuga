/*
 * Copyright (C) 2008 Robotics at Maryland
 * Copyright (C) 2008 Joseph Lisee <jlisee@umd.edu>
 * All rights reserved.
 *
 * Author: Joseph Lisee <jlisee@umd.edu>
 * File:  packages/vision/test/src/TestImage.cxx
 */


// Library Includes
#include <UnitTest++/UnitTest++.h>
#include <boost/filesystem.hpp>

// Project Includes
#include "vision/include/Image.h"
#include "vision/include/OpenCVImage.h"

#include "vision/test/include/UnitTestChecks.h"
#include "vision/test/include/Utility.h"

#ifndef RAM_WINDOWS

static boost::filesystem::path getImagesDir()
{
    boost::filesystem::path root(getenv("RAM_SVN_DIR"));
    return root / "packages" / "vision" / "test" / "data" / "images";
}

using namespace ram;

SUITE(Image) {

TEST(transform)
{
    vision::Image* reference = vision::Image::loadFromFile(
        (getImagesDir() / "testrotate" / "upright.png").string());
    vision::Image* expected30 = vision::Image::loadFromFile(
        (getImagesDir() / "testrotate" / "30right.png").string());
    vision::Image* expected90 = vision::Image::loadFromFile(
        (getImagesDir() / "testrotate" / "90right.png").string());
    vision::Image* expectedNeg25Scale = vision::Image::loadFromFile(
        (getImagesDir() / "testrotate" / "25left_20scale.png").string());
    vision::Image* result = new vision::OpenCVImage(640, 480);

    // Rotate Image right 30 degrees
    vision::Image::transform(reference, result, math::Degree(30),
                             1.0, 0, 0, 255, 255, 255);
    CHECK_CLOSE(*expected30, *result, 1.5);

    // Rotate Image right 90 degrees
    vision::Image::transform(reference, result, math::Degree(90),
                             1.0, 0, 0, 255, 255, 255);
    CHECK_CLOSE(*expected90, *result, 1);

    // Rotate left 25 degrees and scale 20%
    vision::Image::transform(reference, result, math::Degree(-25), 1.2,
                             0, 0, 255, 255, 255);
    CHECK_CLOSE(*expectedNeg25Scale, *result, 3.0);
    
    delete reference;
    delete expected30;
    delete expected90;
    delete result;
}

TEST(blitImage)
{
    vision::Image* expected = vision::Image::loadFromFile(
        (getImagesDir() / "testblit" / "complete.png").string());
    vision::Image* source = vision::Image::loadFromFile(
        (getImagesDir() / "testblit" / "start.png").string());
    vision::Image* toBlit = vision::Image::loadFromFile(
        (getImagesDir() / "testblit" / "toblit.png").string());
    vision::Image* blankBlit = new vision::OpenCVImage(640, 480);
    vision::Image* result = new vision::OpenCVImage(640, 480);

    // Completely blank image should result in no change
    makeColor(blankBlit, 255, 255, 255);
    vision::Image::blitImage(blankBlit, source, result);
    CHECK_CLOSE(*source, *result, 0);

    // Now a normal blit
    vision::Image::blitImage(toBlit, source, result);
    CHECK_CLOSE(*result, *expected, 0);

    // Do a blit with a transform in it
    int transX = 200;
    int transY = 50;
    
    makeColor(source, 255, 255, 255);

    makeColor(toBlit, 255, 255, 255);
    drawSquare(toBlit, 200, 200, 50, 100, 0.0, CV_RGB(0, 0, 0));
    
    makeColor(expected, 255, 255, 255);
    drawSquare(expected, transX + 200, transY + 200, 50, 100, 0,
               CV_RGB(0, 0, 0));

    vision::Image::blitImage(toBlit, source, result, 255, 255, 255,
                             transX, transY);
    
    CHECK_CLOSE(*result, *expected, 0);
}

void drawPattern(vision::Image* image, int x, int y)
{
    vision::drawSquare(image, x, y, 128, 128, 0.0, CV_RGB(0,0,0));
    vision::drawSquare(image, x + 35, y + 40, 25, 70, 0.0, CV_RGB(0,255,0));
}

TEST(drawImage)
{
    vision::OpenCVImage expected(512, 512);
    vision::OpenCVImage src(512, 512);
    vision::OpenCVImage result(512, 512);
    vision::OpenCVImage toDraw(128, 128);

    // Prepare src
    vision::makeColor(&src, 255, 255, 255);
    
    // Prepare expected result
    vision::makeColor(&expected, 255, 255, 255);
    drawPattern(&expected, 345, 120);
//    vision::Image::showImage(&expected, "Expected");

    // Prepare image to draw
    drawPattern(&toDraw, 0, 0);
//    vision::Image::showImage(&toDraw, "To Draw");
    
    vision::Image::drawImage(&toDraw, 345, 120, &src, &result);
//    vision::Image::showImage(&result, "Result");
//    CHECK_CLOSE(((Image*)&result), *((Image*)&expected), 0);
}

}
#endif // RAM_WINDOWS