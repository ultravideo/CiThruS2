/*
 *  ADAPTED FROM https://github.com/madwyn/libcube2cyl
 *  AND LICENSED UNDER THE MIT LICENSE
 * 
 *  Cube2Cyl v1.0.2 - 2012-05-29
 *
 *  Cube2Cyl is a cubic projection to cylindrical projection conversion lib.
 *
 *  Homepage: http://www.wenyanan.com/cube2cyl/
 *  Please check the web page for further information, upgrades and bug fixes.
 *
 *  Copyright (c) 2012-2014 Yanan Wen (madwyn+src@gmail.com)
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 ******************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI        3.14159265358979323846
#define M_PI_2      1.57079632679489661923
#define M_PI_4      0.78539816339744830962
#endif

// sphere radius
#define S_RADIUS    1.0

// defines the faces of a cube
enum CUBE_FACES
{
    CUBE_TOP = 0,
    CUBE_LEFT,
    CUBE_FRONT,
    CUBE_RIGHT,
    CUBE_BACK,
    CUBE_DOWN,
    CUBE_FACE_NUM
};

// the cubic image coordinates
typedef struct
{
    CUBE_FACES face;    // the face of the cube
    double     x;       // the x coordinate
    double     y;       // the y coordinate
} CUBE_COORD;

class HERVANTAUE4_API Cube2Equirectangular
{
public:
    //-------- input information
    unsigned int pxCamV;    /**< The vertical pixel of a camera */
    unsigned int pxCamH;    /**< The horizontal pixel of a camera */

    double radius;          /**< The radius of the sphere */

    double rdPanoV;         /**< The vertical view portion */
    double rdPanoH;         /**< The horizontal view portion */

    //-------- output information
    unsigned int pxPanoSizeV;   /**< The vertical pixels of the panorama */
    unsigned int pxPanoSizeH;   /**< The horizontal pixels of the panorama */

    //-------- to access the pixel
    CUBE_FACES    cubeFaceId;    /**< The cube face to be read */
    double        mappedX;       /**< The x coordinate mapped on the cube face */
    double        mappedY;       /**< The y coordinate mapped on the cube face */

    void init(unsigned int pxInW, double rdInV, double rdInH);
    void init(unsigned int pxPanoH, unsigned int pxPanoV, unsigned int pxInW, double rdInV, double rdInH);

    void genMap();

    const CUBE_COORD* getCoord(const unsigned int& x, const unsigned int& y) const;

    Cube2Equirectangular();
    ~Cube2Equirectangular();

private:
    inline void calXY(const int& i, const int& j);

    inline void calXYZ(const int& i, const int& j, double& x, double& y, double& z);

    inline void calNormXY(const int& i, const int& j, double& x, double& y);
    inline void calThetaAndPhi(const double& x, const double& y, double& theta, double& phi);
    inline void calXyzFromThetaPhi(const double& theta, const double& phi, double& x, double& y, double& z);

    inline void calCubeFace(const double& theta, const double& phi);

    inline void locate(const double& axis, const double& px, const double& py, const double& rad);

    // the helper functions
    inline bool cmpDoubleEqual(const double& a, const double& b, const double& epsilon);
    inline bool cmpDoubleSmaller(const double& a, const double& b, const double& epsilon);
    inline bool cmpDoubleEqualSmaller(const double& a, const double& b, const double& epsilon);
    inline bool cmpDoubleLarger(const double& a, const double& b, const double& epsilon);
    inline bool cmpDoubleEqualLarger(const double& a, const double& b, const double& epsilon);
    inline bool isDoubleInRange(const double& value, const double& small, const double& large, const double& epsilon);

    inline void rotRad(double rad, double& x, double& y, double& temp);
    inline void transDis(double dis, double& x, double& y);

    //-------- The temp variables
    double normTheta;   /**< The normalised theta */
    double resCal;      /**< The resolution used for calculation */
    double normFactorX; /**< The normalisation factor for x */
    double normFactorY; /**< The normalisation factor for y */

    double sizeRatio;   /**< The size ratio of the mapped x and the actual radius */

    double tX;          /**< x coordinate in 3D space */
    double tY;          /**< y coordinate in 3D space */
    double tZ;          /**< z coordinate in 3D space */

    double tTheta;      /**< The radian horizontally */
    double tPhi;        /**< The radian vertically */

    double phiThreshold;    /**< The threshold of phi, it separates the top, middle and down
     of the cube, which are the edges of the top and down surface */

    CUBE_COORD* map;    /**< The map from panorama coordinates to cubic coordinates */
};
