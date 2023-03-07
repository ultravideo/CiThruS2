#include "Cube2Equirectangular.h"

/** \brief The initialise function of Cube2Equirectangular. This one takes fewer parameters, and will
 * generate the width and height of the panorama automatically based on the input
 *
 * \param pxInW const unsigned int      The width of the input image
 * \param rdInV const          double   The radian of the view portion vertically, range [0.01, PI]
 * \param rdInH const          double   The radian of the view portion horizontally, range [0.01, 2*PI]
 * \return void
 *
 */
void Cube2Equirectangular::init(const unsigned int pxInW, const double rdInV, const double rdInH)
{
    unsigned int pxPanoH = (unsigned int)((rdInH / M_PI_2) * (double)pxInW);
    unsigned int pxPanoV = (unsigned int)((rdInV / M_PI_2) * (double)pxInW);
    init(pxPanoH, pxPanoV, pxInW, rdInV, rdInH);
}

/** \brief The initialise function of Cube2Equirectangular.
 *
 * \param pxPanoH const unsigned int  The desired panorama width
 * \param pxPanoV const unsigned int  The desired panorama height
 * \param pxInW   const unsigned int  The width of the input image
 * \param rdInV   const double        The radian of the view portion vertically, range [0.01, PI]
 * \param rdInH   const double        The radian of the view portion horizontally, range [0.01, 2*PI]
 * \return void
 *
 */
void Cube2Equirectangular::init(const unsigned int pxPanoH, const unsigned int pxPanoV, const unsigned int pxInW, const double rdInV, const double rdInH)
{
    // check parameters
    if ((pxInW == 0)
        || (pxPanoH == 0)
        || (pxPanoV == 0)) {
        return;
    }

    // check the view portion
    if (((rdInV < 0.01) || (rdInV > M_PI))
        || ((rdInH < 0.01) || (rdInH > (M_PI * 2.0)))) {
        return;
    }

    pxCamV = pxInW;
    pxCamH = pxInW;

    rdPanoV = rdInV;
    rdPanoH = rdInH;

    pxPanoSizeH = pxPanoH;
    pxPanoSizeV = pxPanoV;

    radius = ((double)pxInW) / 2.0;

    // the actual calculation resolution is 10 times bigger than the texture resolution
    resCal = M_PI_4 / (double)pxCamH / 10.0;

    // the normalisation factors
    normFactorX = rdPanoH / (M_PI * 2);
    normFactorY = rdPanoV / M_PI;
}

/** \brief Generate the map
 * To access the cubic pixel coordinates: map[x*y]
 *
 * \return void
 *
 */
void Cube2Equirectangular::genMap()
{
    if (NULL != map) {
        free(map);
    }

    map = (CUBE_COORD*)malloc(pxPanoSizeV * pxPanoSizeH * sizeof(CUBE_COORD));

    unsigned int pos = 0;

    for (unsigned int x = 0; x < pxPanoSizeH; ++x) {
        for (unsigned int y = 0; y < pxPanoSizeV; ++y) {
            calXY(x, y);

            map[pos].face = cubeFaceId;
            map[pos].x = mappedX;
            map[pos++].y = mappedY;
        }
    }
}

/** \brief Get the cubic coordinates
 *
 * \param x const unsigned int  The panorama x coordinate
 * \param y const unsigned int  The panorama y coordinate
 * \return CUBE_COORD
 */
const CUBE_COORD* Cube2Equirectangular::getCoord(const unsigned int& x, const unsigned int& y) const {
    return &map[x * pxPanoSizeV + y];
}
 

 /** \brief Rotate the point for a given radian
  *
  * \param rad double
  * \param x double&
  * \param y double&
  * \param temp double&
  * \return void
  *
  */
inline void Cube2Equirectangular::rotRad(double rad, double& x, double& y, double& temp)
{
    temp = x;
    x = x * cos(rad) - y * sin(rad);
    y = temp * sin(rad) + y * cos(rad);
}

/** \brief Translate the point for a given distance in both x and y coordinates
 *
 * \param dis double
 * \param x double&
 * \param y double&
 * \return void
 *
 */
inline void Cube2Equirectangular::transDis(double dis, double& x, double& y)
{
    x += dis;
    y += dis;
}

/** \brief Calculate the x and y coordinates of given i and j
 *
 * \param i const int&  The coordinate along the width axis
 * \param j const int&  The coordinate along the height axis
 * \return void
 *
 */
inline void Cube2Equirectangular::calXY(const int& i, const int& j)
{
    calXYZ(i, j, tX, tY, tZ);

    switch (cubeFaceId) {
    case CUBE_TOP: {
        locate(tY, tZ, tX, M_PI);
        break;
    }
    case CUBE_DOWN: {
        locate(tY, tX, tZ, -M_PI_2);
        break;
    }
    case CUBE_LEFT: {
        locate(tZ, tX, tY, M_PI);
        break;
    }
    case CUBE_RIGHT: {
        locate(tZ, tY, tX, M_PI_2);
        break;
    }
    case CUBE_FRONT: {
        locate(tX, tZ, tY, 0.0);
        break;
    }
    case CUBE_BACK: {
        locate(tX, tY, tZ, -M_PI_2);
        break;
    }
    default: {
        break;
    }
    }
}

/** \brief Locate the point in the cubic image
 *
 * \param axis const double& The sphere coordinate along the axis
 * \param px   const double&
 * \param py   const double&
 * \param rad  const double&
 * \return void
 *
 */
inline void Cube2Equirectangular::locate(const double& axis, const double& px, const double& py, const double& rad)
{
    sizeRatio = radius / axis;

    mappedX = sizeRatio * px;
    mappedY = sizeRatio * py;

    // rotate
    rotRad(rad, mappedX, mappedY, sizeRatio);

    // translate
    transDis(radius, mappedX, mappedY);
}

/** \brief Calculate the face which the point is on
 *
 * \param theta const double&
 * \param phi const double&
 * \return void
 *
 */
inline void Cube2Equirectangular::calCubeFace(const double& theta, const double& phi)
{
    // Looking at the cube from top down
    // FRONT zone
    if (isDoubleInRange(theta, -M_PI_4, M_PI_4, resCal)) {
        cubeFaceId = CUBE_FRONT;
        normTheta = theta;
    }
    // LEFT zone
    else if (isDoubleInRange(theta, -(M_PI_2 + M_PI_4), -M_PI_4, resCal)) {
        cubeFaceId = CUBE_LEFT;
        normTheta = theta + M_PI_2;
    }
    // RIGHT zone
    else if (isDoubleInRange(theta, M_PI_4, M_PI_2 + M_PI_4, resCal)) {
        cubeFaceId = CUBE_RIGHT;
        normTheta = theta - M_PI_2;
    }
    else {
        cubeFaceId = CUBE_BACK;

        if (theta > 0.0) {
            normTheta = theta - M_PI;
        }
        else {
            normTheta = theta + M_PI;
        }
    }

    // find out which segment the line strikes to
    phiThreshold = atan2(S_RADIUS, S_RADIUS / cos(normTheta));
    //phiThreshold = atan2(S_RADIUS / cos(normTheta), S_RADIUS);

    // in the top segment
    if (phi > phiThreshold) {
        cubeFaceId = CUBE_DOWN;
    }
    // in the bottom segment
    else if (phi < -phiThreshold) {
        cubeFaceId = CUBE_TOP;
    }
    // in the middle segment
    else {
        ;
    }
}

/** \brief Calculate the 3D space coordinates of given 2D coordinates
 *
 * \param i const int&  The coordinate along the x-axis
 * \param j const int&  The coordinate along the y-axis
 * \param x double&
 * \param y double&
 * \param z double&
 * \return void
 *
 */
inline void Cube2Equirectangular::calXYZ(const int& i, const int& j, double& x, double& y, double& z) {
    calNormXY(i, j, x, y);
    calThetaAndPhi(x, y, tTheta, tPhi);
    calXyzFromThetaPhi(tTheta, tPhi, x, y, z);

    calCubeFace(tTheta, tPhi);
}

inline void Cube2Equirectangular::calNormXY(const int& i, const int& j, double& x, double& y) {
    x = ((2.0 * i) / pxPanoSizeH - 1.0) * normFactorX;
    y = ((2.0 * j) / pxPanoSizeV - 1.0) * normFactorY;
    // y = 1.0 - (2.0*j)/pxPanoSizeV;
}

inline void Cube2Equirectangular::calThetaAndPhi(const double& x, const double& y, double& theta, double& phi) {
    theta = x * M_PI;
    phi = y * M_PI_2;
    // for spherical vertical distortion
    // phi = asin(y);
}

inline void Cube2Equirectangular::calXyzFromThetaPhi(const double& theta, const double& phi, double& x, double& y, double& z) {
    x = cos(phi) * cos(theta);
    y = sin(phi);
    z = cos(phi) * sin(theta);
}


/** \brief Test if a == b
 *
 * \param a const double&
 * \param b const double&
 * \param epsilon const double& The precision
 * \return bool
 *
 */
inline bool Cube2Equirectangular::cmpDoubleEqual(const double& a, const double& b, const double& epsilon) {
    return (fabs(a - b) < epsilon);
}

/** \brief Test if a < b
 *
 * \param a const double&
 * \param b const double&
 * \param epsilon const double&
 * \return bool
 *
 */
inline bool Cube2Equirectangular::cmpDoubleSmaller(const double& a, const double& b, const double& epsilon) {
    return ((a - b) < 0) && (!cmpDoubleEqual(a, b, epsilon));
}

/** \brief Test if a <= b
 *
 * \param a const double&
 * \param b const double&
 * \param epsilon const double&
 * \return bool
 *
 */
inline bool Cube2Equirectangular::cmpDoubleEqualSmaller(const double& a, const double& b, const double& epsilon) {
    return ((a - b) < 0) || Cube2Equirectangular::cmpDoubleEqual(a, b, epsilon);
}


/** \brief Test if a > b
 *
 * \param a const double&
 * \param b const double&
 * \param epsilon const double&
 * \return bool
 *
 */
inline bool Cube2Equirectangular::cmpDoubleLarger(const double& a, const double& b, const double& epsilon) {
    return ((a - b) > 0) && (!cmpDoubleEqual(a, b, epsilon));
}

/** \brief Test if a >= b
 *
 * \param a const double&
 * \param b const double&
 * \param epsilon const double&
 * \return bool
 *
 */
inline bool Cube2Equirectangular::cmpDoubleEqualLarger(const double& a, const double& b, const double& epsilon) {
    return ((a - b) > 0) || Cube2Equirectangular::cmpDoubleEqual(a, b, epsilon);
}

/** \brief Test if value in in the range of [small, large)
 *
 * \param value   const double&
 * \param small   const double&
 * \param large   const double&
 * \param epsilon const double&
 * \return bool
 *
 */
inline bool Cube2Equirectangular::isDoubleInRange(const double& value, const double& small, const double& large, const double& epsilon) {
    return    cmpDoubleEqualLarger(value, small, epsilon)
        && cmpDoubleSmaller(value, large, epsilon);
}


/** \brief The default constructor
 *
 * \param void
 *
 */
Cube2Equirectangular::Cube2Equirectangular(void)
    : pxCamV(0),
    pxCamH(0),
    radius(0.0),
    rdPanoV(0.0),
    rdPanoH(0.0),
    pxPanoSizeV(0),
    pxPanoSizeH(0),
    cubeFaceId(CUBE_FRONT),
    mappedX(0.0),
    mappedY(0.0),
    normTheta(0.0),
    resCal(0.001),
    normFactorX(1.0),
    normFactorY(1.0),
    sizeRatio(1.0),
    tX(0.0),
    tY(0.0),
    tZ(0.0),
    tTheta(0.0),
    tPhi(0.0),
    phiThreshold(0.0),
    map(NULL) {
    ;
}

/** \brief The default destructor
 *
 * \param void
 *
 */
Cube2Equirectangular::~Cube2Equirectangular(void) {
    if (NULL != map) {
        free(map);
    }
}
