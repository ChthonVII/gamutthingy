#ifndef PLANE_H
#define PLANE_H

#include "vec3.h"

class plane{
public:
    vec3 point;
    vec3 normal;
    
    // constructors
    plane();
    plane(vec3 A, vec3 B, vec3 C);
    
    void initialize(vec3 A, vec3 B, vec3 C);
};

// returns true if ray intersects plane at a single point, and puts that point in contactpoint
// contactpoint - stores output here
// rayOrigin - origin point of ray
// rayDirection - direction of ray
// planeNormal - normal of the plane
// plane Coord - an arbitrary point on the plane
bool linePlaneIntersection(vec3 &contactpoint, vec3 rayOrigin, vec3 rayDirection, vec3 planeNormal, vec3 planeCoord);

#endif
